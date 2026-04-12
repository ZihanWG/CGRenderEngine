#pragma once

#include <memory>
#include <string>

#include "Assets/AssetCache.h"

class Mesh;
class Shader;
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

private:
    AssetCache<Shader> m_ShaderCache;
    AssetCache<Mesh> m_MeshCache;
    AssetCache<EnvironmentImage> m_EnvironmentCache;
};
