#include "Assets/GLTFLoader.h"

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define TINYGLTF_IMPLEMENTATION
#include "json.hpp"
#include "tiny_gltf.h"

#include "Renderer/Mesh.h"
#include "Renderer/Texture2D.h"
#include "Scene/Scene.h"

namespace
{
    std::string ResolveAssetPath(const std::string& path)
    {
        const std::filesystem::path inputPath(path);
        if (inputPath.is_absolute())
        {
            return inputPath.string();
        }

        const std::filesystem::path root(CGENGINE_ASSET_ROOT);
        return (root / inputPath).string();
    }

    std::size_t GetComponentCount(int type)
    {
        switch (type)
        {
        case TINYGLTF_TYPE_SCALAR:
            return 1;
        case TINYGLTF_TYPE_VEC2:
            return 2;
        case TINYGLTF_TYPE_VEC3:
            return 3;
        case TINYGLTF_TYPE_VEC4:
            return 4;
        default:
            throw std::runtime_error("Unsupported accessor type.");
        }
    }

    std::size_t GetComponentSize(int componentType)
    {
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            return 1;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            return 2;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        case TINYGLTF_COMPONENT_TYPE_INT:
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            return 4;
        default:
            throw std::runtime_error("Unsupported accessor component type.");
        }
    }

    const unsigned char* GetAccessorElementPointer(
        const tinygltf::Model& model,
        const tinygltf::Accessor& accessor,
        std::size_t index
    )
    {
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        const std::size_t stride = accessor.ByteStride(bufferView) != 0
            ? accessor.ByteStride(bufferView)
            : GetComponentCount(accessor.type) * GetComponentSize(accessor.componentType);

        return buffer.data.data() + bufferView.byteOffset + accessor.byteOffset + stride * index;
    }

    glm::vec2 ReadVec2Float(
        const tinygltf::Model& model,
        const tinygltf::Accessor& accessor,
        std::size_t index
    )
    {
        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC2)
        {
            throw std::runtime_error("Expected a float vec2 accessor.");
        }

        const float* data = reinterpret_cast<const float*>(GetAccessorElementPointer(model, accessor, index));
        return glm::vec2(data[0], data[1]);
    }

    glm::vec3 ReadVec3Float(
        const tinygltf::Model& model,
        const tinygltf::Accessor& accessor,
        std::size_t index
    )
    {
        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
        {
            throw std::runtime_error("Expected a float vec3 accessor.");
        }

        const float* data = reinterpret_cast<const float*>(GetAccessorElementPointer(model, accessor, index));
        return glm::vec3(data[0], data[1], data[2]);
    }

    glm::vec4 ReadVec4Float(
        const tinygltf::Model& model,
        const tinygltf::Accessor& accessor,
        std::size_t index
    )
    {
        if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC4)
        {
            throw std::runtime_error("Expected a float vec4 accessor.");
        }

        const float* data = reinterpret_cast<const float*>(GetAccessorElementPointer(model, accessor, index));
        return glm::vec4(data[0], data[1], data[2], data[3]);
    }

    std::uint32_t ReadIndex(
        const tinygltf::Model& model,
        const tinygltf::Accessor& accessor,
        std::size_t index
    )
    {
        const unsigned char* data = GetAccessorElementPointer(model, accessor, index);

        switch (accessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *reinterpret_cast<const std::uint8_t*>(data);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return *reinterpret_cast<const std::uint16_t*>(data);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return *reinterpret_cast<const std::uint32_t*>(data);
        default:
            throw std::runtime_error("Unsupported index component type.");
        }
    }

    void ComputeNormals(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices)
    {
        for (Vertex& vertex : vertices)
        {
            vertex.normal = glm::vec3(0.0f);
        }

        for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            Vertex& v0 = vertices[indices[i + 0]];
            Vertex& v1 = vertices[indices[i + 1]];
            Vertex& v2 = vertices[indices[i + 2]];

            const glm::vec3 edge1 = v1.position - v0.position;
            const glm::vec3 edge2 = v2.position - v0.position;
            const glm::vec3 faceNormal = glm::normalize(glm::cross(edge1, edge2));

            v0.normal += faceNormal;
            v1.normal += faceNormal;
            v2.normal += faceNormal;
        }

        for (Vertex& vertex : vertices)
        {
            if (glm::length(vertex.normal) > 0.0f)
            {
                vertex.normal = glm::normalize(vertex.normal);
            }
            else
            {
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }
    }

    glm::vec3 BuildFallbackTangent(const glm::vec3& normal)
    {
        const glm::vec3 axis = glm::abs(normal.y) < 0.999f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        return glm::normalize(glm::cross(axis, normal));
    }

    void ComputeTangents(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices)
    {
        std::vector<glm::vec3> tangentAccum(vertices.size(), glm::vec3(0.0f));
        std::vector<glm::vec3> bitangentAccum(vertices.size(), glm::vec3(0.0f));

        for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            const std::uint32_t i0 = indices[i + 0];
            const std::uint32_t i1 = indices[i + 1];
            const std::uint32_t i2 = indices[i + 2];

            const Vertex& v0 = vertices[i0];
            const Vertex& v1 = vertices[i1];
            const Vertex& v2 = vertices[i2];

            const glm::vec3 edge1 = v1.position - v0.position;
            const glm::vec3 edge2 = v2.position - v0.position;
            const glm::vec2 deltaUv1 = v1.texCoord - v0.texCoord;
            const glm::vec2 deltaUv2 = v2.texCoord - v0.texCoord;
            const float determinant = deltaUv1.x * deltaUv2.y - deltaUv1.y * deltaUv2.x;
            if (glm::abs(determinant) < 1e-8f)
            {
                continue;
            }

            const float inverseDeterminant = 1.0f / determinant;
            const glm::vec3 tangent = (edge1 * deltaUv2.y - edge2 * deltaUv1.y) * inverseDeterminant;
            const glm::vec3 bitangent = (edge2 * deltaUv1.x - edge1 * deltaUv2.x) * inverseDeterminant;

            tangentAccum[i0] += tangent;
            tangentAccum[i1] += tangent;
            tangentAccum[i2] += tangent;
            bitangentAccum[i0] += bitangent;
            bitangentAccum[i1] += bitangent;
            bitangentAccum[i2] += bitangent;
        }

        for (std::size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex)
        {
            const glm::vec3 normal = glm::normalize(vertices[vertexIndex].normal);
            glm::vec3 tangent = tangentAccum[vertexIndex];

            if (glm::length(tangent) <= 1e-8f)
            {
                vertices[vertexIndex].tangent = glm::vec4(BuildFallbackTangent(normal), 1.0f);
                continue;
            }

            tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
            const float handedness = glm::dot(glm::cross(normal, tangent), bitangentAccum[vertexIndex]) < 0.0f
                ? -1.0f
                : 1.0f;
            vertices[vertexIndex].tangent = glm::vec4(tangent, handedness);
        }
    }

    glm::mat4 BuildNodeLocalMatrix(const tinygltf::Node& node)
    {
        if (node.matrix.size() == 16)
        {
            glm::mat4 matrix(1.0f);
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    matrix[column][row] = static_cast<float>(node.matrix[column * 4 + row]);
                }
            }
            return matrix;
        }

        glm::mat4 matrix(1.0f);

        if (node.translation.size() == 3)
        {
            matrix = glm::translate(
                matrix,
                glm::vec3(
                    static_cast<float>(node.translation[0]),
                    static_cast<float>(node.translation[1]),
                    static_cast<float>(node.translation[2])
                )
            );
        }

        if (node.rotation.size() == 4)
        {
            const glm::quat rotation(
                static_cast<float>(node.rotation[3]),
                static_cast<float>(node.rotation[0]),
                static_cast<float>(node.rotation[1]),
                static_cast<float>(node.rotation[2])
            );
            matrix *= glm::mat4_cast(rotation);
        }

        if (node.scale.size() == 3)
        {
            matrix = glm::scale(
                matrix,
                glm::vec3(
                    static_cast<float>(node.scale[0]),
                    static_cast<float>(node.scale[1]),
                    static_cast<float>(node.scale[2])
                )
            );
        }

        return matrix;
    }

    std::shared_ptr<Texture2D> LoadTexture(
        int textureIndex,
        bool srgb,
        const tinygltf::Model& model,
        std::unordered_map<std::uint64_t, std::shared_ptr<Texture2D>>& cache
    )
    {
        if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
        {
            return nullptr;
        }

        const tinygltf::Texture& texture = model.textures[textureIndex];
        if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
        {
            return nullptr;
        }

        const std::uint64_t cacheKey = (static_cast<std::uint64_t>(texture.source) << 1u) | (srgb ? 1u : 0u);
        const auto cached = cache.find(cacheKey);
        if (cached != cache.end())
        {
            return cached->second;
        }

        const tinygltf::Image& image = model.images[texture.source];
        if (image.image.empty())
        {
            return nullptr;
        }

        GLenum format = GL_RGBA;
        GLenum internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

        switch (image.component)
        {
        case 1:
            format = GL_RED;
            internalFormat = GL_R8;
            break;
        case 2:
            format = GL_RG;
            internalFormat = GL_RG8;
            break;
        case 3:
            format = GL_RGB;
            internalFormat = srgb ? GL_SRGB8 : GL_RGB8;
            break;
        case 4:
            format = GL_RGBA;
            internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            break;
        default:
            throw std::runtime_error("Unsupported glTF image channel count.");
        }

        auto textureResource = std::make_shared<Texture2D>();
        textureResource->Allocate(
            image.width,
            image.height,
            internalFormat,
            format,
            GL_UNSIGNED_BYTE,
            image.image.data(),
            GL_LINEAR_MIPMAP_LINEAR,
            GL_LINEAR,
            GL_REPEAT,
            GL_REPEAT
        );
        textureResource->GenerateMipmaps();

        cache.emplace(cacheKey, textureResource);
        return textureResource;
    }

    std::shared_ptr<ImageTexture> LoadImageTexture(
        int textureIndex,
        bool srgb,
        const tinygltf::Model& model,
        std::unordered_map<std::uint64_t, std::shared_ptr<ImageTexture>>& cache
    )
    {
        if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
        {
            return nullptr;
        }

        const tinygltf::Texture& texture = model.textures[textureIndex];
        if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
        {
            return nullptr;
        }

        const std::uint64_t cacheKey = (static_cast<std::uint64_t>(texture.source) << 1u) | (srgb ? 1u : 0u);
        const auto cached = cache.find(cacheKey);
        if (cached != cache.end())
        {
            return cached->second;
        }

        const tinygltf::Image& image = model.images[texture.source];
        if (image.image.empty())
        {
            return nullptr;
        }

        auto textureResource = std::make_shared<ImageTexture>();
        textureResource->width = image.width;
        textureResource->height = image.height;
        textureResource->channels = image.component;
        textureResource->srgb = srgb;
        textureResource->pixels = image.image;

        cache.emplace(cacheKey, textureResource);
        return textureResource;
    }

    Material BuildMaterial(
        const tinygltf::Model& model,
        int materialIndex,
        std::unordered_map<std::uint64_t, std::shared_ptr<Texture2D>>& textureCache,
        std::unordered_map<std::uint64_t, std::shared_ptr<ImageTexture>>& imageTextureCache
    )
    {
        Material material;
        if (materialIndex < 0 || materialIndex >= static_cast<int>(model.materials.size()))
        {
            return material;
        }

        const tinygltf::Material& gltfMaterial = model.materials[materialIndex];
        const tinygltf::PbrMetallicRoughness& pbr = gltfMaterial.pbrMetallicRoughness;

        if (pbr.baseColorFactor.size() == 4)
        {
            material.albedo = glm::vec3(
                static_cast<float>(pbr.baseColorFactor[0]),
                static_cast<float>(pbr.baseColorFactor[1]),
                static_cast<float>(pbr.baseColorFactor[2])
            );
        }

        material.metallic = static_cast<float>(pbr.metallicFactor);
        material.roughness = static_cast<float>(pbr.roughnessFactor);

        if (gltfMaterial.emissiveFactor.size() == 3)
        {
            material.emissive = glm::vec3(
                static_cast<float>(gltfMaterial.emissiveFactor[0]),
                static_cast<float>(gltfMaterial.emissiveFactor[1]),
                static_cast<float>(gltfMaterial.emissiveFactor[2])
            );
        }

        if (pbr.baseColorTexture.index >= 0)
        {
            material.baseColorTexture = LoadTexture(pbr.baseColorTexture.index, true, model, textureCache);
            material.baseColorTextureData = LoadImageTexture(
                pbr.baseColorTexture.index,
                true,
                model,
                imageTextureCache
            );
        }

        if (pbr.metallicRoughnessTexture.index >= 0)
        {
            material.metallicRoughnessTexture = LoadTexture(pbr.metallicRoughnessTexture.index, false, model, textureCache);
            material.metallicRoughnessTextureData = LoadImageTexture(
                pbr.metallicRoughnessTexture.index,
                false,
                model,
                imageTextureCache
            );
        }

        if (gltfMaterial.normalTexture.index >= 0)
        {
            material.normalTexture = LoadTexture(gltfMaterial.normalTexture.index, false, model, textureCache);
            material.normalTextureData = LoadImageTexture(
                gltfMaterial.normalTexture.index,
                false,
                model,
                imageTextureCache
            );
            material.normalScale = static_cast<float>(gltfMaterial.normalTexture.scale);
        }

        if (gltfMaterial.occlusionTexture.index >= 0)
        {
            material.occlusionTexture = LoadTexture(gltfMaterial.occlusionTexture.index, false, model, textureCache);
            material.occlusionTextureData = LoadImageTexture(
                gltfMaterial.occlusionTexture.index,
                false,
                model,
                imageTextureCache
            );
            material.occlusionStrength = static_cast<float>(gltfMaterial.occlusionTexture.strength);
        }

        if (gltfMaterial.emissiveTexture.index >= 0)
        {
            material.emissiveTexture = LoadTexture(gltfMaterial.emissiveTexture.index, true, model, textureCache);
            material.emissiveTextureData = LoadImageTexture(
                gltfMaterial.emissiveTexture.index,
                true,
                model,
                imageTextureCache
            );
        }

        return material;
    }

    void AppendPrimitive(
        const tinygltf::Model& model,
        const tinygltf::Primitive& primitive,
        const glm::mat4& worldMatrix,
        const std::string& objectName,
        Scene& scene,
        std::unordered_map<std::uint64_t, std::shared_ptr<Texture2D>>& textureCache,
        std::unordered_map<std::uint64_t, std::shared_ptr<ImageTexture>>& imageTextureCache
    )
    {
        const auto positionIt = primitive.attributes.find("POSITION");
        if (positionIt == primitive.attributes.end())
        {
            throw std::runtime_error("glTF primitive does not contain POSITION data.");
        }

        const tinygltf::Accessor& positionAccessor = model.accessors[positionIt->second];
        const tinygltf::Accessor* normalAccessor = nullptr;
        const tinygltf::Accessor* texCoordAccessor = nullptr;
        const tinygltf::Accessor* tangentAccessor = nullptr;

        if (const auto normalIt = primitive.attributes.find("NORMAL"); normalIt != primitive.attributes.end())
        {
            normalAccessor = &model.accessors[normalIt->second];
        }

        if (const auto texCoordIt = primitive.attributes.find("TEXCOORD_0"); texCoordIt != primitive.attributes.end())
        {
            texCoordAccessor = &model.accessors[texCoordIt->second];
        }

        if (const auto tangentIt = primitive.attributes.find("TANGENT"); tangentIt != primitive.attributes.end())
        {
            tangentAccessor = &model.accessors[tangentIt->second];
        }

        std::vector<Vertex> vertices(positionAccessor.count);
        for (std::size_t vertexIndex = 0; vertexIndex < positionAccessor.count; ++vertexIndex)
        {
            vertices[vertexIndex].position = ReadVec3Float(model, positionAccessor, vertexIndex);

            if (normalAccessor)
            {
                vertices[vertexIndex].normal = ReadVec3Float(model, *normalAccessor, vertexIndex);
            }

            if (texCoordAccessor)
            {
                vertices[vertexIndex].texCoord = ReadVec2Float(model, *texCoordAccessor, vertexIndex);
            }

            if (tangentAccessor)
            {
                vertices[vertexIndex].tangent = ReadVec4Float(model, *tangentAccessor, vertexIndex);
            }
        }

        std::vector<std::uint32_t> indices;
        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            indices.resize(indexAccessor.count);
            for (std::size_t index = 0; index < indexAccessor.count; ++index)
            {
                indices[index] = ReadIndex(model, indexAccessor, index);
            }
        }
        else
        {
            indices.resize(vertices.size());
            for (std::size_t index = 0; index < vertices.size(); ++index)
            {
                indices[index] = static_cast<std::uint32_t>(index);
            }
        }

        if (!normalAccessor)
        {
            ComputeNormals(vertices, indices);
        }

        if (texCoordAccessor && !tangentAccessor)
        {
            ComputeTangents(vertices, indices);
        }

        RenderObject object;
        object.name = objectName;
        object.mesh = std::make_shared<Mesh>(std::move(vertices), std::move(indices));
        object.material = BuildMaterial(model, primitive.material, textureCache, imageTextureCache);
        object.transform.useMatrixOverride = true;
        object.transform.matrixOverride = worldMatrix;
        scene.AddObject(std::move(object));
    }

    void ProcessNode(
        const tinygltf::Model& model,
        int nodeIndex,
        const glm::mat4& parentTransform,
        Scene& scene,
        std::unordered_map<std::uint64_t, std::shared_ptr<Texture2D>>& textureCache,
        std::unordered_map<std::uint64_t, std::shared_ptr<ImageTexture>>& imageTextureCache
    )
    {
        const tinygltf::Node& node = model.nodes[nodeIndex];
        const glm::mat4 worldTransform = parentTransform * BuildNodeLocalMatrix(node);

        if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size()))
        {
            const tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for (std::size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
            {
                const std::string meshName = mesh.name.empty() ? "glTFMesh" : mesh.name;
                AppendPrimitive(
                    model,
                    mesh.primitives[primitiveIndex],
                    worldTransform,
                    meshName + "_" + std::to_string(primitiveIndex),
                    scene,
                    textureCache,
                    imageTextureCache
                );
            }
        }

        for (int childIndex : node.children)
        {
            ProcessNode(model, childIndex, worldTransform, scene, textureCache, imageTextureCache);
        }
    }
}

bool GLTFLoader::LoadModelIntoScene(
    const std::string& path,
    Scene& scene,
    const glm::mat4& rootTransform,
    std::string* errorMessage
) const
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warnings;
    std::string errors;

    const std::string resolvedPath = ResolveAssetPath(path);
    const std::string extension = std::filesystem::path(resolvedPath).extension().string();
    const bool loaded = extension == ".glb"
        ? loader.LoadBinaryFromFile(&model, &errors, &warnings, resolvedPath)
        : loader.LoadASCIIFromFile(&model, &errors, &warnings, resolvedPath);

    if (!warnings.empty())
    {
        std::ostringstream warningStream;
        warningStream << "glTF warnings: " << warnings;
        if (errorMessage)
        {
            *errorMessage = warningStream.str();
        }
    }

    if (!loaded)
    {
        if (errorMessage)
        {
            *errorMessage = errors.empty() ? "Failed to load glTF model." : errors;
        }
        return false;
    }

    std::unordered_map<std::uint64_t, std::shared_ptr<Texture2D>> textureCache;
    std::unordered_map<std::uint64_t, std::shared_ptr<ImageTexture>> imageTextureCache;

    const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex >= 0 && sceneIndex < static_cast<int>(model.scenes.size()))
    {
        const tinygltf::Scene& gltfScene = model.scenes[sceneIndex];
        for (int nodeIndex : gltfScene.nodes)
        {
            ProcessNode(model, nodeIndex, rootTransform, scene, textureCache, imageTextureCache);
        }
    }
    else
    {
        for (std::size_t nodeIndex = 0; nodeIndex < model.nodes.size(); ++nodeIndex)
        {
            ProcessNode(model, static_cast<int>(nodeIndex), rootTransform, scene, textureCache, imageTextureCache);
        }
    }

    return true;
}
