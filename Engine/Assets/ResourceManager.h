// Centralized entry point for cached shaders, builtin meshes, and environment maps.
#pragma once

#include <future>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>

#include "Engine/Assets/AssetCache.h"

class Mesh;
class Shader;
struct DecodedSceneModel;
struct EnvironmentImage;

class ResourceManager
{
public:
    std::shared_ptr<Shader> LoadShader(const std::string& vertexPath, const std::string& fragmentPath);
    std::shared_ptr<Mesh> GetFullscreenQuad();
    std::shared_ptr<Mesh> GetCube(float size = 1.0f);
    std::shared_ptr<Mesh> GetPlane(float size = 1.0f, float uvScale = 1.0f);
    std::shared_ptr<Mesh> GetSphere(float radius = 1.0f, int xSegments = 24, int ySegments = 16);
    std::shared_ptr<EnvironmentImage> LoadEnvironment(
        const std::string& path,
        std::string* errorMessage = nullptr
    );
    std::shared_future<std::shared_ptr<EnvironmentImage>> LoadEnvironmentAsync(const std::string& path);
    std::shared_ptr<DecodedSceneModel> LoadDecodedModel(
        const std::string& path,
        std::string* errorMessage = nullptr
    );
    std::shared_future<std::shared_ptr<DecodedSceneModel>> LoadDecodedModelAsync(const std::string& path);

private:
    AssetCache<Shader> m_ShaderCache;
    AssetCache<Mesh> m_MeshCache;
    AssetCache<EnvironmentImage> m_EnvironmentCache;
    AssetCache<DecodedSceneModel> m_DecodedModelCache;
    std::unordered_map<std::string, std::shared_future<std::shared_ptr<EnvironmentImage>>> m_EnvironmentLoadFutures;
    std::unordered_map<std::string, std::shared_future<std::shared_ptr<DecodedSceneModel>>> m_DecodedModelLoadFutures;
    mutable std::mutex m_EnvironmentMutex;
    mutable std::mutex m_DecodedModelMutex;
};
