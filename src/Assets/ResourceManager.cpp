#include "Assets/ResourceManager.h"

#include <sstream>
#include <stdexcept>

#include "Renderer/EnvironmentLighting.h"
#include "Renderer/Mesh.h"
#include "Renderer/Shader.h"

std::shared_ptr<Shader> ResourceManager::LoadShader(
    const std::string& vertexPath,
    const std::string& fragmentPath
)
{
    const std::string key = vertexPath + "|" + fragmentPath;
    return m_ShaderCache.GetOrCreate(key, [&]() {
        return std::make_shared<Shader>(vertexPath, fragmentPath);
    });
}

std::shared_ptr<Mesh> ResourceManager::GetFullscreenQuad()
{
    return m_MeshCache.GetOrCreate("builtin:fullscreen-quad", []() {
        return Mesh::CreateFullscreenQuad();
    });
}

std::shared_ptr<Mesh> ResourceManager::GetCube(float size)
{
    std::ostringstream key;
    key << "builtin:cube:" << size;
    return m_MeshCache.GetOrCreate(key.str(), [size]() {
        return Mesh::CreateCube(size);
    });
}

std::shared_ptr<Mesh> ResourceManager::GetPlane(float size, float uvScale)
{
    std::ostringstream key;
    key << "builtin:plane:" << size << ":" << uvScale;
    return m_MeshCache.GetOrCreate(key.str(), [size, uvScale]() {
        return Mesh::CreatePlane(size, uvScale);
    });
}

std::shared_ptr<Mesh> ResourceManager::GetSphere(float radius, int xSegments, int ySegments)
{
    std::ostringstream key;
    key << "builtin:sphere:" << radius << ":" << xSegments << ":" << ySegments;
    return m_MeshCache.GetOrCreate(key.str(), [radius, xSegments, ySegments]() {
        return Mesh::CreateSphere(radius, xSegments, ySegments);
    });
}

std::shared_ptr<EnvironmentImage> ResourceManager::LoadEnvironment(
    const std::string& path,
    std::string* errorMessage
)
{
    return m_EnvironmentCache.GetOrCreate(path, [&]() {
        std::string localError;
        std::shared_ptr<EnvironmentImage> environment = LoadHdrEnvironment(path, &localError);
        if (!environment)
        {
            if (errorMessage)
            {
                *errorMessage = localError;
            }
            throw std::runtime_error(localError);
        }

        return environment;
    });
}
