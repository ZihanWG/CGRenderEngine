// ResourceManager turns path or parameter tuples into shared cached engine assets.
#include "Engine/Assets/ResourceManager.h"

#include <future>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Engine/Assets/EnvironmentLighting.h"
#include "Engine/Assets/GLTFLoader.h"
#include "Engine/Core/JobSystem.h"
#include "Engine/RHI/Mesh.h"
#include "Engine/RHI/Shader.h"

namespace
{
    std::shared_future<std::shared_ptr<EnvironmentImage>> MakeReadyEnvironmentFuture(
        const std::shared_ptr<EnvironmentImage>& environment
    )
    {
        std::promise<std::shared_ptr<EnvironmentImage>> promise;
        promise.set_value(environment);
        return promise.get_future().share();
    }

    std::shared_future<std::shared_ptr<DecodedSceneModel>> MakeReadyDecodedModelFuture(
        const std::shared_ptr<DecodedSceneModel>& decodedModel
    )
    {
        std::promise<std::shared_ptr<DecodedSceneModel>> promise;
        promise.set_value(decodedModel);
        return promise.get_future().share();
    }
}

ResourceManager::~ResourceManager()
{
    WaitForPendingLoads();
}

void ResourceManager::WaitForPendingLoads()
{
    // Jobs submitted by LoadEnvironmentAsync/LoadDecodedModelAsync capture `this` and
    // touch the caches and mutexes below. JobSystem is a static singleton that outlives
    // this object, so without this drain a shutdown mid-decode leaves a worker thread
    // writing into freed memory.
    //
    // The futures are copied out before waiting: the job bodies take the same mutexes,
    // so holding one here would deadlock.
    std::vector<std::shared_future<std::shared_ptr<EnvironmentImage>>> environmentLoads;
    {
        std::lock_guard<std::mutex> lock(m_EnvironmentMutex);
        environmentLoads.reserve(m_EnvironmentLoadFutures.size());
        for (const auto& entry : m_EnvironmentLoadFutures)
        {
            environmentLoads.push_back(entry.second);
        }
    }

    std::vector<std::shared_future<std::shared_ptr<DecodedSceneModel>>> decodedModelLoads;
    {
        std::lock_guard<std::mutex> lock(m_DecodedModelMutex);
        decodedModelLoads.reserve(m_DecodedModelLoadFutures.size());
        for (const auto& entry : m_DecodedModelLoadFutures)
        {
            decodedModelLoads.push_back(entry.second);
        }
    }

    // wait() rather than get(): a failed load stores an exception that nobody is
    // obliged to consume here, and rethrowing out of a destructor would terminate.
    for (const auto& load : environmentLoads)
    {
        if (load.valid())
        {
            load.wait();
        }
    }

    for (const auto& load : decodedModelLoads)
    {
        if (load.valid())
        {
            load.wait();
        }
    }
}

std::shared_ptr<Shader> ResourceManager::LoadShader(
    const std::string& vertexPath,
    const std::string& fragmentPath
)
{
    // Vertex/fragment pairs identify a unique program in this project.
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
    {
        std::lock_guard<std::mutex> lock(m_EnvironmentMutex);
        if (std::shared_ptr<EnvironmentImage> cached = m_EnvironmentCache.Find(path))
        {
            return cached;
        }
    }

    // Surface the loader error so callers can fall back to the procedural sky.
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

    {
        std::lock_guard<std::mutex> lock(m_EnvironmentMutex);
        m_EnvironmentCache.Store(path, environment);
    }

    return environment;
}

std::shared_future<std::shared_ptr<EnvironmentImage>> ResourceManager::LoadEnvironmentAsync(const std::string& path)
{
    {
        std::lock_guard<std::mutex> lock(m_EnvironmentMutex);
        if (std::shared_ptr<EnvironmentImage> cached = m_EnvironmentCache.Find(path))
        {
            return MakeReadyEnvironmentFuture(cached);
        }

        const auto existingLoad = m_EnvironmentLoadFutures.find(path);
        if (existingLoad != m_EnvironmentLoadFutures.end())
        {
            return existingLoad->second;
        }
    }

    std::shared_future<std::shared_ptr<EnvironmentImage>> future =
        JobSystem::Get().SubmitShared([this, path]() {
            std::string errorMessage;
            std::shared_ptr<EnvironmentImage> environment = LoadHdrEnvironment(path, &errorMessage);
            if (!environment)
            {
                throw std::runtime_error(
                    errorMessage.empty() ? "Failed to load HDR environment." : errorMessage
                );
            }

            std::lock_guard<std::mutex> lock(m_EnvironmentMutex);
            m_EnvironmentCache.Store(path, environment);
            return environment;
        });

    {
        std::lock_guard<std::mutex> lock(m_EnvironmentMutex);
        m_EnvironmentLoadFutures.insert_or_assign(path, future);
    }

    return future;
}

std::shared_ptr<DecodedSceneModel> ResourceManager::LoadDecodedModel(
    const std::string& path,
    std::string* errorMessage
)
{
    {
        std::lock_guard<std::mutex> lock(m_DecodedModelMutex);
        if (std::shared_ptr<DecodedSceneModel> cached = m_DecodedModelCache.Find(path))
        {
            return cached;
        }
    }

    GLTFLoader loader;
    std::shared_ptr<DecodedSceneModel> decodedModel = loader.DecodeModel(path, errorMessage);
    if (!decodedModel)
    {
        return nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(m_DecodedModelMutex);
        m_DecodedModelCache.Store(path, decodedModel);
    }

    return decodedModel;
}

std::shared_future<std::shared_ptr<DecodedSceneModel>> ResourceManager::LoadDecodedModelAsync(const std::string& path)
{
    {
        std::lock_guard<std::mutex> lock(m_DecodedModelMutex);
        if (std::shared_ptr<DecodedSceneModel> cached = m_DecodedModelCache.Find(path))
        {
            return MakeReadyDecodedModelFuture(cached);
        }

        const auto existingLoad = m_DecodedModelLoadFutures.find(path);
        if (existingLoad != m_DecodedModelLoadFutures.end())
        {
            return existingLoad->second;
        }
    }

    std::shared_future<std::shared_ptr<DecodedSceneModel>> future =
        JobSystem::Get().SubmitShared([this, path]() {
            GLTFLoader loader;
            std::string errorMessage;
            std::shared_ptr<DecodedSceneModel> decodedModel = loader.DecodeModel(path, &errorMessage);
            if (!decodedModel)
            {
                throw std::runtime_error(
                    errorMessage.empty() ? "Failed to decode glTF model." : errorMessage
                );
            }

            std::lock_guard<std::mutex> lock(m_DecodedModelMutex);
            m_DecodedModelCache.Store(path, decodedModel);
            return decodedModel;
        });

    {
        std::lock_guard<std::mutex> lock(m_DecodedModelMutex);
        m_DecodedModelLoadFutures.insert_or_assign(path, future);
    }

    return future;
}
