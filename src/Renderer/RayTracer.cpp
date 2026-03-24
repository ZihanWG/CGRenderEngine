#include "Renderer/RayTracer.h"

#include <cmath>
#include <limits>

#include <glm/gtc/constants.hpp>

#include "Renderer/Mesh.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

namespace
{
    constexpr float kPi = 3.14159265359f;
    constexpr float kEpsilon = 0.0005f;

    struct Ray
    {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
    };

    struct HitInfo
    {
        float distance = std::numeric_limits<float>::max();
        glm::vec3 position{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        const Material* material = nullptr;
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

    bool IntersectScene(const Scene& scene, const Ray& ray, HitInfo& hitInfo)
    {
        bool hitAnything = false;

        for (const RenderObject& object : scene.GetObjects())
        {
            if (!object.mesh)
            {
                continue;
            }

            const glm::mat4 model = object.transform.GetMatrix();
            const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
            const auto& vertices = object.mesh->GetVertices();
            const auto& indices = object.mesh->GetIndices();

            for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
            {
                const Vertex& v0 = vertices[indices[i + 0]];
                const Vertex& v1 = vertices[indices[i + 1]];
                const Vertex& v2 = vertices[indices[i + 2]];

                const glm::vec3 p0 = glm::vec3(model * glm::vec4(v0.position, 1.0f));
                const glm::vec3 p1 = glm::vec3(model * glm::vec4(v1.position, 1.0f));
                const glm::vec3 p2 = glm::vec3(model * glm::vec4(v2.position, 1.0f));

                float t = 0.0f;
                float u = 0.0f;
                float v = 0.0f;
                if (!IntersectTriangle(ray, p0, p1, p2, t, u, v) || t >= hitInfo.distance)
                {
                    continue;
                }

                const float w = 1.0f - u - v;
                const glm::vec3 n0 = glm::normalize(normalMatrix * v0.normal);
                const glm::vec3 n1 = glm::normalize(normalMatrix * v1.normal);
                const glm::vec3 n2 = glm::normalize(normalMatrix * v2.normal);

                hitAnything = true;
                hitInfo.distance = t;
                hitInfo.position = ray.origin + ray.direction * t;
                hitInfo.normal = glm::normalize(w * n0 + u * n1 + v * n2);
                hitInfo.material = &object.material;
            }
        }

        return hitAnything;
    }

    bool IsOccluded(const Scene& scene, const Ray& ray, float maxDistance)
    {
        HitInfo hitInfo;
        if (!IntersectScene(scene, ray, hitInfo))
        {
            return false;
        }

        return hitInfo.distance < maxDistance;
    }

    glm::vec3 SampleSky(const glm::vec3& direction)
    {
        const float t = glm::clamp(direction.y * 0.5f + 0.5f, 0.0f, 1.0f);
        return glm::mix(glm::vec3(0.18f, 0.21f, 0.28f), glm::vec3(0.65f, 0.76f, 0.92f), t);
    }

    glm::vec3 EvaluateBRDF(
        const Material& material,
        const glm::vec3& normal,
        const glm::vec3& viewDirection,
        const glm::vec3& lightDirection,
        const glm::vec3& radiance
    )
    {
        const float roughness = glm::clamp(material.roughness, 0.05f, 1.0f);
        const glm::vec3 halfway = glm::normalize(viewDirection + lightDirection);
        const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), material.albedo, material.metallic);

        const float ndf = DistributionGGX(normal, halfway, roughness);
        const float geometry = GeometrySmith(normal, viewDirection, lightDirection, roughness);
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

    glm::vec3 TraceRay(const Scene& scene, const Ray& ray, int depth, const RayTraceSettings& settings)
    {
        HitInfo hitInfo;
        if (!IntersectScene(scene, ray, hitInfo) || !hitInfo.material)
        {
            return SampleSky(ray.direction);
        }

        const Material& material = *hitInfo.material;
        const glm::vec3 normal = glm::normalize(hitInfo.normal);
        const glm::vec3 viewDirection = glm::normalize(-ray.direction);
        const glm::vec3 shadowOrigin = hitInfo.position + normal * kEpsilon;
        glm::vec3 color = material.emissive;

        const DirectionalLight& directionalLight = scene.GetDirectionalLight();
        const glm::vec3 lightDirection = glm::normalize(-directionalLight.direction);
        if (!IsOccluded(scene, Ray{shadowOrigin, lightDirection}, 40.0f))
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
            if (!IsOccluded(scene, Ray{shadowOrigin, pointLightDirection}, distanceToPointLight - kEpsilon))
            {
                const float normalizedDistance = distanceToPointLight / pointLight.range;
                const float falloff = glm::clamp(1.0f - normalizedDistance * normalizedDistance, 0.0f, 1.0f);
                const float attenuation = (falloff * falloff) / (1.0f + distanceToPointLight * distanceToPointLight);
                const glm::vec3 radiance = pointLight.color * pointLight.intensity * attenuation;
                color += EvaluateBRDF(material, normal, viewDirection, pointLightDirection, radiance);
            }
        }

        color += material.albedo * 0.03f * material.ao * (1.0f - material.metallic);

        if (depth < settings.maxBounces)
        {
            const glm::vec3 f0 = glm::mix(glm::vec3(0.04f), material.albedo, material.metallic);
            const glm::vec3 fresnel = FresnelSchlick(glm::max(glm::dot(normal, viewDirection), 0.0f), f0);
            glm::vec3 reflectionDirection = glm::reflect(ray.direction, normal);
            reflectionDirection = glm::normalize(glm::mix(
                reflectionDirection,
                normal,
                material.roughness * material.roughness * 0.35f
            ));

            const glm::vec3 reflection = TraceRay(scene, Ray{shadowOrigin, reflectionDirection}, depth + 1, settings);
            const float reflectivity = glm::mix(0.05f, 1.0f, material.metallic);
            color += reflection * fresnel * reflectivity * (1.0f - material.roughness * 0.3f);
        }

        return glm::max(color, glm::vec3(0.0f));
    }
}

std::vector<glm::vec3> RayTracer::Render(
    const Scene& scene,
    const Camera& camera,
    const RayTraceSettings& settings
) const
{
    std::vector<glm::vec3> pixels(static_cast<std::size_t>(settings.width * settings.height));

    for (int y = 0; y < settings.height; ++y)
    {
        for (int x = 0; x < settings.width; ++x)
        {
            glm::vec3 accumulatedColor(0.0f);

            for (int sample = 0; sample < settings.samplesPerPixel; ++sample)
            {
                const glm::vec2 jitter = Hammersley(
                    static_cast<unsigned int>(sample),
                    static_cast<unsigned int>(settings.samplesPerPixel)
                );

                const float u = (static_cast<float>(x) + jitter.x) / static_cast<float>(settings.width);
                const float v = (static_cast<float>(y) + jitter.y) / static_cast<float>(settings.height);
                accumulatedColor += TraceRay(
                    scene,
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
