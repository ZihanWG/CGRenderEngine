// CPU reference renderer with cached BVH acceleration and shared material semantics.
//
// This is not meant to be a film renderer. Its purpose is narrower:
// - provide a second rendering path with different failure modes
// - let the user compare realtime and offline-ish results side by side
// - reuse the same scene/material inputs as much as possible
//
// The implementation therefore favors clarity and reuse over physical completeness.
#include "Engine/Renderer/RayTracer.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include <glm/gtc/constants.hpp>

#include "Engine/Assets/EnvironmentLighting.h"
#include "Engine/RHI/Mesh.h"
#include "Engine/Scene/Camera.h"
#include "Engine/Scene/Scene.h"

namespace
{
    constexpr float kPi = 3.14159265359f;
    constexpr float kEpsilon = 0.0005f;
    constexpr int kLeafPrimitiveCount = 4;

    struct Ray
    {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
    };

    struct AABB
    {
        glm::vec3 min{std::numeric_limits<float>::max()};
        glm::vec3 max{-std::numeric_limits<float>::max()};

        void Expand(const glm::vec3& point)
        {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        void Expand(const AABB& bounds)
        {
            Expand(bounds.min);
            Expand(bounds.max);
        }

        glm::vec3 Extents() const
        {
            return max - min;
        }

        int LongestAxis() const
        {
            const glm::vec3 extents = Extents();
            if (extents.x >= extents.y && extents.x >= extents.z)
            {
                return 0;
            }
            if (extents.y >= extents.z)
            {
                return 1;
            }
            return 2;
        }
    };

    struct TrianglePrimitive
    {
        glm::vec3 positions[3];
        glm::vec3 normals[3];
        glm::vec2 texCoords[3];
        glm::vec4 tangents[3];
        glm::vec3 centroid{0.0f};
        AABB bounds;
        std::size_t objectIndex = 0;
    };

    struct BVHNode
    {
        AABB bounds;
        int leftChild = -1;
        int rightChild = -1;
        int primitiveOffset = 0;
        int primitiveCount = 0;

        bool IsLeaf() const
        {
            return primitiveCount > 0;
        }
    };

    struct SceneAcceleration
    {
        std::vector<TrianglePrimitive> primitives;
        std::vector<BVHNode> nodes;
    };

    struct HitInfo
    {
        float distance = std::numeric_limits<float>::max();
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec2 texCoord{0.0f};
        glm::vec4 tangent{0.0f};
        const TrianglePrimitive* primitive = nullptr;
        std::size_t objectIndex = std::numeric_limits<std::size_t>::max();
    };

    struct MaterialSample
    {
        glm::vec3 albedo{1.0f};
        float metallic = 0.0f;
        float roughness = 0.5f;
        float ao = 1.0f;
        glm::vec3 emissive{0.0f};
    };

    float DistributionGGX(const glm::vec3& n, const glm::vec3& h, float roughness)
    {
        const float a = roughness * roughness;
        const float a2 = a * a;
        const float nDotH = glm::max(glm::dot(n, h), 0.0f);
        const float nDotH2 = nDotH * nDotH;
        const float denominator = (nDotH2 * (a2 - 1.0f) + 1.0f);
        return a2 / glm::max(kPi * denominator * denominator, 0.0001f);
    }

    float GeometrySchlickGGX(float nDotV, float roughness)
    {
        const float r = roughness + 1.0f;
        const float k = (r * r) / 8.0f;
        return nDotV / glm::max(nDotV * (1.0f - k) + k, 0.0001f);
    }

    float GeometrySmith(const glm::vec3& n, const glm::vec3& v, const glm::vec3& l, float roughness)
    {
        const float nDotV = glm::max(glm::dot(n, v), 0.0f);
        const float nDotL = glm::max(glm::dot(n, l), 0.0f);
        return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
    }

    glm::vec3 FresnelSchlick(float cosTheta, const glm::vec3& f0)
    {
        const float factor = std::pow(glm::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
        return f0 + (glm::vec3(1.0f) - f0) * factor;
    }

    float RadicalInverseVdC(unsigned int bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    glm::vec2 Hammersley(unsigned int i, unsigned int n)
    {
        return glm::vec2(static_cast<float>(i) / static_cast<float>(n), RadicalInverseVdC(i));
    }

    bool IntersectTriangle(
        const Ray& ray,
        const glm::vec3& p0,
        const glm::vec3& p1,
        const glm::vec3& p2,
        float& outT,
        float& outU,
        float& outV
    )
    {
        // Standard Moller-Trumbore intersection with a small epsilon to reject self-hits.
        const glm::vec3 edge1 = p1 - p0;
        const glm::vec3 edge2 = p2 - p0;
        const glm::vec3 pVec = glm::cross(ray.direction, edge2);
        const float determinant = glm::dot(edge1, pVec);
        if (glm::abs(determinant) < 1e-7f)
        {
            return false;
        }

        const float inverseDeterminant = 1.0f / determinant;
        const glm::vec3 tVec = ray.origin - p0;
        outU = glm::dot(tVec, pVec) * inverseDeterminant;
        if (outU < 0.0f || outU > 1.0f)
        {
            return false;
        }

        const glm::vec3 qVec = glm::cross(tVec, edge1);
        outV = glm::dot(ray.direction, qVec) * inverseDeterminant;
        if (outV < 0.0f || outU + outV > 1.0f)
        {
            return false;
        }

        outT = glm::dot(edge2, qVec) * inverseDeterminant;
        return outT > kEpsilon;
    }

    bool IntersectAABB(const Ray& ray, const AABB& bounds, float tMin, float tMax, float& outNearT)
    {
        // Slab test used by BVH traversal. Returns the near entry distance for child ordering.
        float nearT = tMin;
        float farT = tMax;

        for (int axis = 0; axis < 3; ++axis)
        {
            const float origin = ray.origin[axis];
            const float direction = ray.direction[axis];

            if (glm::abs(direction) < 1e-8f)
            {
                if (origin < bounds.min[axis] || origin > bounds.max[axis])
                {
                    return false;
                }

                continue;
            }

            const float inverseDirection = 1.0f / direction;
            float t0 = (bounds.min[axis] - origin) * inverseDirection;
            float t1 = (bounds.max[axis] - origin) * inverseDirection;

            if (t0 > t1)
            {
                std::swap(t0, t1);
            }

            nearT = glm::max(nearT, t0);
            farT = glm::min(farT, t1);

            if (nearT > farT)
            {
                return false;
            }
        }

        outNearT = nearT;
        return true;
    }

    glm::vec3 SafeNormalize(const glm::vec3& value, const glm::vec3& fallback)
    {
        const float lengthSquared = glm::dot(value, value);
        if (lengthSquared <= 1e-12f)
        {
            return fallback;
        }

        return value / std::sqrt(lengthSquared);
    }

    glm::vec3 BuildFallbackTangent(const glm::vec3& normal)
    {
        const glm::vec3 axis = glm::abs(normal.y) < 0.999f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        return SafeNormalize(glm::cross(axis, normal), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    template <typename T>
    void HashCombine(std::size_t& seed, const T& value)
    {
        seed ^= std::hash<T>{}(value) + 0x9e3779b97f4a7c15ull + (seed << 6u) + (seed >> 2u);
    }

    void HashMatrix(std::size_t& seed, const glm::mat4& matrix)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                HashCombine(seed, matrix[column][row]);
            }
        }
    }

    std::size_t ComputeSceneGeometryHash(const Scene& scene)
    {
        // Cache invalidation is based on mesh identity plus object transforms.
        std::size_t seed = 0x6eed0e9da4d94a4full;
        const auto& objects = scene.GetObjects();
        HashCombine(seed, objects.size());

        for (const RenderObject& object : objects)
        {
            HashCombine(seed, reinterpret_cast<std::uintptr_t>(object.mesh.get()));
            if (object.mesh)
            {
                HashCombine(seed, object.mesh->GetVertices().size());
                HashCombine(seed, object.mesh->GetIndices().size());
            }

            HashMatrix(seed, object.transform.GetMatrix());
        }

        return seed;
    }

    float SampleAmbientOcclusion(const Material& material, const glm::vec2& texCoord)
    {
        float occlusion = material.ao;
        if (material.occlusionTextureData)
        {
            const float textureOcclusion = material.occlusionTextureData->Sample(texCoord).r;
            occlusion *= glm::mix(
                1.0f,
                textureOcclusion,
                glm::clamp(material.occlusionStrength, 0.0f, 1.0f)
            );
        }

        return glm::clamp(occlusion, 0.0f, 1.0f);
    }

    glm::vec3 SampleEmissive(const Material& material, const glm::vec2& texCoord)
    {
        glm::vec3 emissive = material.emissive;
        if (material.emissiveTextureData)
        {
            emissive *= glm::vec3(material.emissiveTextureData->Sample(texCoord));
        }

        return glm::max(emissive, glm::vec3(0.0f));
    }

    glm::vec3 SampleNormalMap(
        const Material& material,
        const TrianglePrimitive* primitive,
        const glm::vec2& texCoord,
        const glm::vec3& interpolatedNormal,
        const glm::vec4& interpolatedTangent
    )
    {
        // Prefer authored tangents; reconstruct them from UV gradients only as a fallback.
        const glm::vec3 fallbackNormal = SafeNormalize(interpolatedNormal, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 fallbackTangent = BuildFallbackTangent(fallbackNormal);
        if (!primitive || !material.normalTextureData)
        {
            return fallbackNormal;
        }

        glm::vec3 tangent = glm::vec3(interpolatedTangent);
        glm::vec3 bitangent(0.0f);

        if (glm::dot(tangent, tangent) > 1e-8f && glm::abs(interpolatedTangent.w) > 0.5f)
        {
            tangent = tangent - fallbackNormal * glm::dot(fallbackNormal, tangent);
            tangent = SafeNormalize(tangent, fallbackTangent);
            bitangent = SafeNormalize(
                glm::cross(fallbackNormal, tangent) * (interpolatedTangent.w < 0.0f ? -1.0f : 1.0f),
                glm::cross(fallbackNormal, tangent)
            );
        }
        else
        {
            const glm::vec2 deltaUv1 = primitive->texCoords[1] - primitive->texCoords[0];
            const glm::vec2 deltaUv2 = primitive->texCoords[2] - primitive->texCoords[0];
            const float determinant = deltaUv1.x * deltaUv2.y - deltaUv2.x * deltaUv1.y;
            if (glm::abs(determinant) < 1e-6f)
            {
                return fallbackNormal;
            }

            const glm::vec3 edge1 = primitive->positions[1] - primitive->positions[0];
            const glm::vec3 edge2 = primitive->positions[2] - primitive->positions[0];
            const float inverseDeterminant = 1.0f / determinant;

            tangent =
                (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * inverseDeterminant;
            bitangent =
                (edge2 * deltaUv1.x - edge1 * deltaUv2.x) * inverseDeterminant;

            tangent = tangent - fallbackNormal * glm::dot(fallbackNormal, tangent);
            bitangent = bitangent - fallbackNormal * glm::dot(fallbackNormal, bitangent);

            tangent = SafeNormalize(tangent, fallbackTangent);
            bitangent = SafeNormalize(bitangent, glm::cross(fallbackNormal, tangent));
            if (glm::dot(glm::cross(tangent, bitangent), fallbackNormal) < 0.0f)
            {
                bitangent = -bitangent;
            }
        }

        glm::vec3 tangentNormal = glm::vec3(material.normalTextureData->Sample(texCoord)) * 2.0f - 1.0f;
        tangentNormal.x *= material.normalScale;
        tangentNormal.y *= material.normalScale;
        tangentNormal = SafeNormalize(tangentNormal, glm::vec3(0.0f, 0.0f, 1.0f));

        return SafeNormalize(
            tangent * tangentNormal.x + bitangent * tangentNormal.y + fallbackNormal * tangentNormal.z,
            fallbackNormal
        );
    }

    MaterialSample SampleMaterial(const Material& material, const glm::vec2& texCoord)
    {
        // Mirror the realtime material conventions closely so comparisons stay meaningful.
        MaterialSample sample;
        sample.albedo = material.albedo;
        sample.metallic = material.metallic;
        sample.roughness = material.roughness;
        sample.ao = SampleAmbientOcclusion(material, texCoord);
        sample.emissive = SampleEmissive(material, texCoord);

        if (material.baseColorTextureData)
        {
            const glm::vec4 baseColor = material.baseColorTextureData->Sample(texCoord);
            sample.albedo *= glm::vec3(baseColor);
        }

        if (material.metallicRoughnessTextureData)
        {
            const glm::vec4 metallicRoughness = material.metallicRoughnessTextureData->Sample(texCoord);
            sample.roughness *= metallicRoughness.g;
            sample.metallic *= metallicRoughness.b;
        }

        sample.roughness = glm::clamp(sample.roughness, 0.05f, 1.0f);
        sample.metallic = glm::clamp(sample.metallic, 0.0f, 1.0f);
        sample.albedo = glm::max(sample.albedo, glm::vec3(0.0f));
        return sample;
    }

    TrianglePrimitive BuildPrimitive(
        std::size_t objectIndex,
        const glm::mat4& model,
        const glm::mat3& normalMatrix,
        const Vertex& v0,
        const Vertex& v1,
        const Vertex& v2
    )
    {
        // Flatten scene geometry into world-space triangles so the BVH can ignore object graphs.
        TrianglePrimitive primitive;
        const glm::mat3 modelMatrix = glm::mat3(model);
        primitive.positions[0] = glm::vec3(model * glm::vec4(v0.position, 1.0f));
        primitive.positions[1] = glm::vec3(model * glm::vec4(v1.position, 1.0f));
        primitive.positions[2] = glm::vec3(model * glm::vec4(v2.position, 1.0f));
        primitive.normals[0] = glm::normalize(normalMatrix * v0.normal);
        primitive.normals[1] = glm::normalize(normalMatrix * v1.normal);
        primitive.normals[2] = glm::normalize(normalMatrix * v2.normal);
        primitive.texCoords[0] = v0.texCoord;
        primitive.texCoords[1] = v1.texCoord;
        primitive.texCoords[2] = v2.texCoord;

        const auto transformTangent = [&](const Vertex& vertex, const glm::vec3& normal) {
            const glm::vec3 tangentDirection = modelMatrix * glm::vec3(vertex.tangent);
            if (glm::dot(tangentDirection, tangentDirection) <= 1e-8f || glm::abs(vertex.tangent.w) <= 0.5f)
            {
                return glm::vec4(0.0f);
            }

            const glm::vec3 tangent = SafeNormalize(
                tangentDirection - normal * glm::dot(normal, tangentDirection),
                BuildFallbackTangent(normal)
            );
            return glm::vec4(tangent, vertex.tangent.w);
        };

        primitive.tangents[0] = transformTangent(v0, primitive.normals[0]);
        primitive.tangents[1] = transformTangent(v1, primitive.normals[1]);
        primitive.tangents[2] = transformTangent(v2, primitive.normals[2]);
        primitive.bounds.Expand(primitive.positions[0]);
        primitive.bounds.Expand(primitive.positions[1]);
        primitive.bounds.Expand(primitive.positions[2]);
        primitive.centroid = (primitive.positions[0] + primitive.positions[1] + primitive.positions[2]) / 3.0f;
        primitive.objectIndex = objectIndex;
        return primitive;
    }

    int BuildBVHRecursive(SceneAcceleration& acceleration, int start, int end)
    {
        // Median split on the longest centroid axis is simple but performs well enough here.
        const int nodeIndex = static_cast<int>(acceleration.nodes.size());
        acceleration.nodes.push_back({});
        BVHNode& node = acceleration.nodes.back();

        AABB bounds;
        AABB centroidBounds;
        for (int primitiveIndex = start; primitiveIndex < end; ++primitiveIndex)
        {
            bounds.Expand(acceleration.primitives[primitiveIndex].bounds);
            centroidBounds.Expand(acceleration.primitives[primitiveIndex].centroid);
        }

        node.bounds = bounds;
        const int primitiveCount = end - start;
        if (primitiveCount <= kLeafPrimitiveCount)
        {
            node.primitiveOffset = start;
            node.primitiveCount = primitiveCount;
            return nodeIndex;
        }

        const int splitAxis = centroidBounds.LongestAxis();
        const glm::vec3 centroidExtents = centroidBounds.Extents();
        if (centroidExtents[splitAxis] < 1e-5f)
        {
            node.primitiveOffset = start;
            node.primitiveCount = primitiveCount;
            return nodeIndex;
        }

        const int middle = start + primitiveCount / 2;
        std::nth_element(
            acceleration.primitives.begin() + start,
            acceleration.primitives.begin() + middle,
            acceleration.primitives.begin() + end,
            [splitAxis](const TrianglePrimitive& lhs, const TrianglePrimitive& rhs) {
                return lhs.centroid[splitAxis] < rhs.centroid[splitAxis];
            }
        );

        node.leftChild = BuildBVHRecursive(acceleration, start, middle);
        node.rightChild = BuildBVHRecursive(acceleration, middle, end);
        return nodeIndex;
    }

    SceneAcceleration BuildSceneAcceleration(const Scene& scene)
    {
        // Build a flat list of world-space triangles once, then accelerate that representation.
        SceneAcceleration acceleration;
        const auto& objects = scene.GetObjects();

        for (std::size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex)
        {
            const RenderObject& object = objects[objectIndex];
            if (!object.mesh)
            {
                continue;
            }

            const glm::mat4 model = object.transform.GetMatrix();
            const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            const auto& vertices = object.mesh->GetVertices();
            const auto& indices = object.mesh->GetIndices();

            for (std::size_t index = 0; index + 2 < indices.size(); index += 3)
            {
                const Vertex& v0 = vertices[indices[index + 0]];
                const Vertex& v1 = vertices[indices[index + 1]];
                const Vertex& v2 = vertices[indices[index + 2]];

                acceleration.primitives.push_back(
                    BuildPrimitive(objectIndex, model, normalMatrix, v0, v1, v2)
                );
            }
        }

        if (!acceleration.primitives.empty())
        {
            acceleration.nodes.reserve(acceleration.primitives.size() * 2);
            BuildBVHRecursive(
                acceleration,
                0,
                static_cast<int>(acceleration.primitives.size())
            );
        }

        return acceleration;
    }

    bool IntersectScene(
        const SceneAcceleration& acceleration,
        const Ray& ray,
        HitInfo& hitInfo
    )
    {
        if (acceleration.nodes.empty())
        {
            return false;
        }

        // Iterative traversal avoids recursion and keeps traversal state explicit.
        bool hitAnything = false;
        std::vector<int> stack;
        stack.reserve(64);
        stack.push_back(0);

        while (!stack.empty())
        {
            const int nodeIndex = stack.back();
            stack.pop_back();

            const BVHNode& node = acceleration.nodes[nodeIndex];
            float nodeNearT = 0.0f;
            if (!IntersectAABB(ray, node.bounds, kEpsilon, hitInfo.distance, nodeNearT))
            {
                continue;
            }

            if (node.IsLeaf())
            {
                // Leaves store a contiguous primitive range, so no extra indirection is needed.
                for (int primitiveOffset = 0; primitiveOffset < node.primitiveCount; ++primitiveOffset)
                {
                    const TrianglePrimitive& primitive =
                        acceleration.primitives[node.primitiveOffset + primitiveOffset];

                    float t = 0.0f;
                    float u = 0.0f;
                    float v = 0.0f;
                    if (!IntersectTriangle(
                            ray,
                            primitive.positions[0],
                            primitive.positions[1],
                            primitive.positions[2],
                            t,
                            u,
                            v
                        ) || t >= hitInfo.distance)
                    {
                        continue;
                    }

                    const float w = 1.0f - u - v;
                    hitAnything = true;
                    hitInfo.distance = t;
                    hitInfo.position = ray.origin + ray.direction * t;
                    hitInfo.normal = glm::normalize(
                        w * primitive.normals[0] +
                        u * primitive.normals[1] +
                        v * primitive.normals[2]
                    );
                    hitInfo.texCoord =
                        w * primitive.texCoords[0] +
                        u * primitive.texCoords[1] +
                        v * primitive.texCoords[2];
                    hitInfo.tangent =
                        w * primitive.tangents[0] +
                        u * primitive.tangents[1] +
                        v * primitive.tangents[2];
                    hitInfo.primitive = &primitive;
                    hitInfo.objectIndex = primitive.objectIndex;
                }

                continue;
            }

            const BVHNode& leftNode = acceleration.nodes[node.leftChild];
            const BVHNode& rightNode = acceleration.nodes[node.rightChild];

            float leftNearT = 0.0f;
            float rightNearT = 0.0f;
            const bool hitLeft = IntersectAABB(ray, leftNode.bounds, kEpsilon, hitInfo.distance, leftNearT);
            const bool hitRight = IntersectAABB(ray, rightNode.bounds, kEpsilon, hitInfo.distance, rightNearT);

            if (hitLeft && hitRight)
            {
                if (leftNearT < rightNearT)
                {
                    stack.push_back(node.rightChild);
                    stack.push_back(node.leftChild);
                }
                else
                {
                    stack.push_back(node.leftChild);
                    stack.push_back(node.rightChild);
                }
            }
            else if (hitLeft)
            {
                stack.push_back(node.leftChild);
            }
            else if (hitRight)
            {
                stack.push_back(node.rightChild);
            }
        }

        return hitAnything;
    }

    bool IsOccluded(const SceneAcceleration& acceleration, const Ray& ray, float maxDistance)
    {
        HitInfo hitInfo;
        hitInfo.distance = maxDistance;
        return IntersectScene(acceleration, ray, hitInfo) && hitInfo.distance < maxDistance;
    }

    glm::vec3 EvaluateBRDF(
        const MaterialSample& material,
        const glm::vec3& normal,
        const glm::vec3& viewDirection,
        const glm::vec3& lightDirection,
        const glm::vec3& radiance
    )
    {
        const glm::vec3 halfway = glm::normalize(viewDirection + lightDirection);
        const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), material.albedo, material.metallic);

        const float ndf = DistributionGGX(normal, halfway, material.roughness);
        const float geometry = GeometrySmith(normal, viewDirection, lightDirection, material.roughness);
        const glm::vec3 fresnel = FresnelSchlick(glm::max(glm::dot(halfway, viewDirection), 0.0f), f0);

        const glm::vec3 numerator = ndf * geometry * fresnel;
        const float denominator = 4.0f *
                                  glm::max(glm::dot(normal, viewDirection), 0.0f) *
                                  glm::max(glm::dot(normal, lightDirection), 0.0f) + 0.0001f;
        const glm::vec3 specular = numerator / denominator;

        const glm::vec3 kS = fresnel;
        const glm::vec3 kD = (glm::vec3(1.0f) - kS) * (1.0f - material.metallic);
        const float nDotL = glm::max(glm::dot(normal, lightDirection), 0.0f);

        return (kD * material.albedo / kPi + specular) * radiance * nDotL;
    }

    glm::vec3 TraceRay(
        const Scene& scene,
        const SceneAcceleration& acceleration,
        const Ray& ray,
        int depth,
        const RayTraceSettings& settings
    )
    {
        // This is intentionally a small reference integrator, not a physically complete path tracer.
        //
        // Shading order:
        // 1. intersect scene or fall back to environment
        // 2. sample material textures on CPU
        // 3. evaluate direct directional + point light
        // 4. add a cheap environment ambient term
        // 5. optionally recurse once for glossy reflection
        HitInfo hitInfo;
        if (!IntersectScene(acceleration, ray, hitInfo))
        {
            return SampleSceneEnvironment(
                scene.GetEnvironment(),
                scene.GetDirectionalLight(),
                ray.direction
            );
        }

        const auto& objects = scene.GetObjects();
        if (hitInfo.objectIndex >= objects.size())
        {
            return SampleSceneEnvironment(
                scene.GetEnvironment(),
                scene.GetDirectionalLight(),
                ray.direction
            );
        }

        const DirectionalLight& directionalLight = scene.GetDirectionalLight();
        const Material& materialSource = objects[hitInfo.objectIndex].ResolveMaterial();
        const MaterialSample material = SampleMaterial(materialSource, hitInfo.texCoord);
        const glm::vec3 normal = SampleNormalMap(
            materialSource,
            hitInfo.primitive,
            hitInfo.texCoord,
            hitInfo.normal,
            hitInfo.tangent
        );
        const glm::vec3 viewDirection = glm::normalize(-ray.direction);
        const glm::vec3 shadowOrigin = hitInfo.position + normal * kEpsilon;
        glm::vec3 color = material.emissive;

        const glm::vec3 lightDirection = glm::normalize(-directionalLight.direction);
        if (!IsOccluded(acceleration, Ray{shadowOrigin, lightDirection}, 40.0f))
        {
            const glm::vec3 radiance = directionalLight.color * directionalLight.intensity;
            color += EvaluateBRDF(material, normal, viewDirection, lightDirection, radiance);
        }

        const PointLight& pointLight = scene.GetPointLight();
        const glm::vec3 toPointLight = pointLight.position - hitInfo.position;
        const float distanceToPointLight = glm::length(toPointLight);
        if (distanceToPointLight < pointLight.range)
        {
            const glm::vec3 pointLightDirection = glm::normalize(toPointLight);
            if (!IsOccluded(
                    acceleration,
                    Ray{shadowOrigin, pointLightDirection},
                    distanceToPointLight - kEpsilon
                ))
            {
                const float normalizedDistance = distanceToPointLight / pointLight.range;
                const float falloff = glm::clamp(1.0f - normalizedDistance * normalizedDistance, 0.0f, 1.0f);
                const float attenuation = (falloff * falloff) / (1.0f + distanceToPointLight * distanceToPointLight);
                const glm::vec3 radiance = pointLight.color * pointLight.intensity * attenuation;
                color += EvaluateBRDF(material, normal, viewDirection, pointLightDirection, radiance);
            }
        }

        color += SampleSceneEnvironment(scene.GetEnvironment(), directionalLight, normal) *
                 material.albedo *
                 material.ao *
                 (1.0f - material.metallic) *
                 0.35f;

        if (depth < settings.maxBounces)
        {
            // The secondary bounce is a rough glossy reflection used to expose major shading gaps.
            const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), material.albedo, material.metallic);
            const glm::vec3 fresnel = FresnelSchlick(glm::max(glm::dot(normal, viewDirection), 0.0f), f0);
            glm::vec3 reflectionDirection = glm::reflect(ray.direction, normal);
            reflectionDirection = glm::normalize(glm::mix(
                reflectionDirection,
                normal,
                material.roughness * material.roughness * 0.35f
            ));

            const glm::vec3 reflection = TraceRay(
                scene,
                acceleration,
                Ray{shadowOrigin, reflectionDirection},
                depth + 1,
                settings
            );
            const float reflectivity = glm::mix(0.05f, 1.0f, material.metallic);
            color += reflection * fresnel * reflectivity * (1.0f - material.roughness * 0.3f);
        }

        return glm::max(color, glm::vec3(0.0f));
    }
}

struct RayTracer::CachedAcceleration
{
    std::size_t geometryHash = 0;
    SceneAcceleration acceleration;
};

RayTracer::RayTracer() = default;
RayTracer::~RayTracer() = default;

const RayTracer::CachedAcceleration& RayTracer::GetCachedAcceleration(const Scene& scene)
{
    const std::size_t geometryHash = ComputeSceneGeometryHash(scene);
    if (!m_CachedAcceleration || m_CachedAcceleration->geometryHash != geometryHash)
    {
        // Geometry rebuilds are amortized across many reference bakes until the scene changes.
        // Material or light changes do not require rebuilding because the BVH only depends on geometry.
        m_CachedAcceleration = std::make_unique<CachedAcceleration>(CachedAcceleration{
            geometryHash,
            BuildSceneAcceleration(scene)
        });
    }

    return *m_CachedAcceleration;
}

std::vector<glm::vec3> RayTracer::Render(
    const Scene& scene,
    const Camera& camera,
    const RayTraceSettings& settings
)
{
    const CachedAcceleration& cachedAcceleration = GetCachedAcceleration(scene);
    const SceneAcceleration& acceleration = cachedAcceleration.acceleration;
    std::vector<glm::vec3> pixels(static_cast<std::size_t>(settings.width * settings.height));

    // This render is embarrassingly parallel, but kept single-threaded inside the worker task
    // for simplicity. The outer async job already moves the expensive work off the main thread.
    for (int y = 0; y < settings.height; ++y)
    {
        for (int x = 0; x < settings.width; ++x)
        {
            glm::vec3 accumulatedColor(0.0f);

            for (int sample = 0; sample < settings.samplesPerPixel; ++sample)
            {
                // Hammersley jitter gives deterministic stratified supersampling without RNG state.
                const glm::vec2 jitter = Hammersley(
                    static_cast<unsigned int>(sample),
                    static_cast<unsigned int>(settings.samplesPerPixel)
                );

                const float u = (static_cast<float>(x) + jitter.x) / static_cast<float>(settings.width);
                const float v = (static_cast<float>(y) + jitter.y) / static_cast<float>(settings.height);
                accumulatedColor += TraceRay(
                    scene,
                    acceleration,
                    Ray{camera.GetPosition(), camera.GenerateRayDirection(u, v)},
                    0,
                    settings
                );
            }

            pixels[static_cast<std::size_t>(y * settings.width + x)] =
                accumulatedColor / static_cast<float>(settings.samplesPerPixel);
        }
    }

    return pixels;
}
