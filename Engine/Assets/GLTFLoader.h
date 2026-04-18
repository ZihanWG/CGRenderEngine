// Imports a subset of glTF into the engine's Scene/Material/Mesh structures.
#pragma once

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/RHI/Mesh.h"
#include "Engine/Scene/Material.h"

class Scene;

struct DecodedRenderObject
{
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    Material material;
    int materialAssetIndex = -1;
    glm::mat4 localTransform{1.0f};
};

struct DecodedSceneModel
{
    std::vector<DecodedRenderObject> objects;
};

class GLTFLoader
{
public:
    std::shared_ptr<DecodedSceneModel> DecodeModel(
        const std::string& path,
        std::string* errorMessage = nullptr
    ) const;
    std::shared_future<std::shared_ptr<DecodedSceneModel>> DecodeModelAsync(
        const std::string& path
    ) const;
    void AppendDecodedModelToScene(
        DecodedSceneModel& model,
        Scene& scene,
        const glm::mat4& rootTransform = glm::mat4(1.0f)
    ) const;

    // Appends the loaded model to an existing scene under an optional root transform.
    bool LoadModelIntoScene(
        const std::string& path,
        Scene& scene,
        const glm::mat4& rootTransform = glm::mat4(1.0f),
        std::string* errorMessage = nullptr
    ) const;
};
