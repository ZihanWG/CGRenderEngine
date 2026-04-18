// Builds the render-visible world from Scene + Camera before draw command generation.
#include "Engine/Renderer/RenderWorld.h"

#include <algorithm>
#include <array>
#include <cstring>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/RHI/Mesh.h"
#include "Engine/Scene/Camera.h"
#include "Engine/Scene/Scene.h"

namespace
{
    struct FrustumPlane
    {
        glm::vec3 normal{0.0f};
        float distance = 0.0f;
    };

    struct WorldFrustum
    {
        std::array<FrustumPlane, 6> planes{};
    };

    bool IsSameView(const glm::vec3& leftPosition, const glm::vec3& rightPosition,
                    const glm::vec3& leftForward, const glm::vec3& rightForward)
    {
        return glm::length(leftPosition - rightPosition) < 1e-5f &&
               glm::length(leftForward - rightForward) < 1e-5f;
    }

    std::uint32_t HashBytes(std::uint32_t seed, const void* data, std::size_t size)
    {
        constexpr std::uint32_t kFnvPrime = 16777619u;
        const auto* bytes = static_cast<const unsigned char*>(data);
        std::uint32_t hash = seed;
        for (std::size_t index = 0; index < size; ++index)
        {
            hash ^= static_cast<std::uint32_t>(bytes[index]);
            hash *= kFnvPrime;
        }

        return hash;
    }

    std::uint32_t HashPointer(std::uint32_t seed, const void* pointer)
    {
        const std::uintptr_t pointerValue = reinterpret_cast<std::uintptr_t>(pointer);
        return HashBytes(seed, &pointerValue, sizeof(pointerValue));
    }

    std::uint32_t HashUInt(std::uint32_t seed, std::uint32_t value)
    {
        return HashBytes(seed, &value, sizeof(value));
    }

    std::uint32_t HashFloat(std::uint32_t seed, float value)
    {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return HashUInt(seed, bits);
    }

    std::uint32_t HashVec3(std::uint32_t seed, const glm::vec3& value)
    {
        seed = HashFloat(seed, value.x);
        seed = HashFloat(seed, value.y);
        seed = HashFloat(seed, value.z);
        return seed;
    }

    std::uint32_t BuildRenderStateId(const Material& material)
    {
        std::uint32_t stateId = 2166136261u;
        stateId = HashUInt(stateId, static_cast<std::uint32_t>(material.blendMode));
        stateId = HashUInt(stateId, static_cast<std::uint32_t>(material.cullMode));
        stateId = HashUInt(stateId, material.castShadows ? 1u : 0u);
        return stateId;
    }

    std::uint32_t BuildMaterialStateId(const Material& material)
    {
        std::uint32_t stateId = BuildRenderStateId(material);
        stateId = HashVec3(stateId, material.albedo);
        stateId = HashFloat(stateId, material.metallic);
        stateId = HashFloat(stateId, material.roughness);
        stateId = HashFloat(stateId, material.ao);
        stateId = HashVec3(stateId, material.emissive);
        stateId = HashFloat(stateId, material.normalScale);
        stateId = HashFloat(stateId, material.occlusionStrength);
        stateId = HashFloat(stateId, material.opacity);
        stateId = HashPointer(stateId, material.baseColorTexture.get());
        stateId = HashPointer(stateId, material.metallicRoughnessTexture.get());
        stateId = HashPointer(stateId, material.normalTexture.get());
        stateId = HashPointer(stateId, material.occlusionTexture.get());
        stateId = HashPointer(stateId, material.emissiveTexture.get());
        return stateId;
    }

    PassMask BuildPassMask(const RenderObject& object)
    {
        if (!object.mesh)
        {
            return PassMask::None;
        }

        const Material& material = object.ResolveMaterial();
        PassMask passMask = UsesTransparentPass(material.blendMode)
            ? PassMask::Transparent
            : PassMask::Opaque;
        if (material.castShadows && !UsesTransparentPass(material.blendMode))
        {
            passMask |= PassMask::Shadow;
        }

        return passMask;
    }

    FrustumPlane NormalizePlane(const glm::vec4& coefficients)
    {
        const glm::vec3 normal(coefficients.x, coefficients.y, coefficients.z);
        const float normalLength = glm::length(normal);
        if (normalLength <= 1e-6f)
        {
            return {};
        }

        return FrustumPlane{
            normal / normalLength,
            coefficients.w / normalLength
        };
    }

    WorldFrustum BuildWorldFrustum(const glm::mat4& viewProjection)
    {
        const glm::mat4 matrix = glm::transpose(viewProjection);
        return WorldFrustum{{
            NormalizePlane(matrix[3] + matrix[0]),
            NormalizePlane(matrix[3] - matrix[0]),
            NormalizePlane(matrix[3] + matrix[1]),
            NormalizePlane(matrix[3] - matrix[1]),
            NormalizePlane(matrix[3] + matrix[2]),
            NormalizePlane(matrix[3] - matrix[2])
        }};
    }

    bool IntersectsFrustum(const WorldFrustum& frustum, const glm::vec3& center, float radius)
    {
        for (const FrustumPlane& plane : frustum.planes)
        {
            if (glm::dot(plane.normal, center) + plane.distance < -radius)
            {
                return false;
            }
        }

        return true;
    }

    glm::vec3 TransformPoint(const glm::mat4& matrix, const glm::vec3& point)
    {
        return glm::vec3(matrix * glm::vec4(point, 1.0f));
    }

    float ExtractMaxScale(const glm::mat4& matrix)
    {
        const float xScale = glm::length(glm::vec3(matrix[0]));
        const float yScale = glm::length(glm::vec3(matrix[1]));
        const float zScale = glm::length(glm::vec3(matrix[2]));
        return std::max({xScale, yScale, zScale, 1e-6f});
    }
}

const RenderWorld& RenderWorldCache::Build(
    const Scene& scene,
    const Camera& camera,
    int viewportWidth,
    int viewportHeight,
    std::uint64_t frameIndex,
    float timeSeconds,
    float deltaTime
)
{
    const glm::vec3 cameraPosition = camera.GetPosition();
    const glm::vec3 cameraForward = camera.GetForward();
    const int safeViewportWidth = std::max(viewportWidth, 1);
    const int safeViewportHeight = std::max(viewportHeight, 1);
    const bool sameView = m_HasCachedView &&
        IsSameView(cameraPosition, m_LastCameraPosition, cameraForward, m_LastCameraForward);
    const bool sameViewport =
        safeViewportWidth == m_LastViewportWidth &&
        safeViewportHeight == m_LastViewportHeight;

    if (m_BuiltSceneVersion != scene.GetContentVersion() || !sameView || !sameViewport)
    {
        ExtractRenderScene(scene);
        BuildViewInfo(camera, safeViewportWidth, safeViewportHeight);
        BuildVisibleLightList(scene);
        BuildVisibleSet();
        UpdatePerViewData();

        m_BuiltSceneVersion = scene.GetContentVersion();
        m_LastCameraPosition = cameraPosition;
        m_LastCameraForward = cameraForward;
        m_LastViewportWidth = safeViewportWidth;
        m_LastViewportHeight = safeViewportHeight;
        m_HasCachedView = true;
    }

    UpdatePerFrameData(frameIndex, timeSeconds, deltaTime);
    return m_RenderWorld;
}

void RenderWorldCache::Invalidate()
{
    m_BuiltSceneVersion = 0;
    m_HasCachedView = false;
    m_LastViewportWidth = 0;
    m_LastViewportHeight = 0;
    m_RenderWorld = {};
}

void RenderWorldCache::ExtractRenderScene(const Scene& scene)
{
    m_RenderWorld.renderScene.objects.clear();
    m_RenderWorld.perObjectData.clear();
    m_RenderWorld.renderScene.sceneVersion = scene.GetContentVersion();
    m_RenderWorld.renderScene.environment = &scene.GetEnvironment();

    const auto& objects = scene.GetObjects();
    m_RenderWorld.renderScene.objects.reserve(objects.size());
    m_RenderWorld.perObjectData.reserve(objects.size());

    for (std::size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex)
    {
        const RenderObject& object = objects[objectIndex];
        const PassMask passMask = BuildPassMask(object);
        if (passMask == PassMask::None)
        {
            continue;
        }

        const std::size_t renderSceneObjectIndex = m_RenderWorld.renderScene.objects.size();
        const glm::mat4 modelMatrix = object.transform.GetMatrix();
        const glm::vec3 worldPosition = glm::vec3(modelMatrix[3]);
        const MaterialInstance* materialInstance = object.GetMaterialInstance();
        const Material& material = object.ResolveMaterial();
        const BoundingSphere localBounds = object.mesh ? object.mesh->GetBounds() : BoundingSphere{};
        const glm::vec3 worldBoundsCenter = TransformPoint(modelMatrix, localBounds.center);
        const float worldBoundsRadius = localBounds.radius * ExtractMaxScale(modelMatrix);
        const std::uint32_t renderStateId = BuildRenderStateId(material);
        const std::uint32_t materialStateId = BuildMaterialStateId(material);
        const std::uint32_t materialAssetStateId = materialInstance && materialInstance->GetAsset()
            ? materialInstance->GetAssetStateId()
            : materialStateId;

        m_RenderWorld.renderScene.objects.push_back(RenderSceneObject{
            objectIndex,
            object.name,
            object.mesh,
            &material,
            materialInstance,
            renderStateId,
            materialStateId,
            materialAssetStateId,
            object.mesh ? object.mesh->GetStateId() : 0u,
            passMask
        });

        m_RenderWorld.perObjectData.push_back(PerObjectData{
            renderSceneObjectIndex,
            modelMatrix,
            worldPosition,
            worldBoundsCenter,
            worldBoundsRadius
        });
    }
}

void RenderWorldCache::BuildViewInfo(const Camera& camera, int viewportWidth, int viewportHeight)
{
    m_RenderWorld.viewInfo = ViewInfo{
        camera.GetViewMatrix(),
        camera.GetProjectionMatrix(),
        glm::inverse(camera.GetProjectionMatrix() * camera.GetViewMatrix()),
        camera.GetPosition(),
        camera.GetForward(),
        camera.GetRight(),
        camera.GetUp(),
        viewportWidth,
        viewportHeight
    };
}

void RenderWorldCache::BuildVisibleLightList(const Scene& scene)
{
    m_RenderWorld.visibleLights.directionalLight = scene.GetDirectionalLight();
    m_RenderWorld.visibleLights.pointLights.clear();

    const PointLight& pointLight = scene.GetPointLight();
    if (pointLight.intensity > 0.0f && pointLight.range > 0.0f)
    {
        m_RenderWorld.visibleLights.pointLights.push_back(pointLight);
    }
}

void RenderWorldCache::BuildVisibleSet()
{
    m_RenderWorld.visibleSet.objects.clear();
    m_RenderWorld.visibleSet.objects.reserve(m_RenderWorld.renderScene.objects.size());
    const WorldFrustum frustum = BuildWorldFrustum(
        m_RenderWorld.viewInfo.projectionMatrix * m_RenderWorld.viewInfo.viewMatrix
    );

    for (std::size_t renderSceneObjectIndex = 0;
         renderSceneObjectIndex < m_RenderWorld.renderScene.objects.size();
         ++renderSceneObjectIndex)
    {
        const RenderSceneObject& sceneObject = m_RenderWorld.renderScene.objects[renderSceneObjectIndex];
        const PerObjectData& perObjectData = m_RenderWorld.perObjectData[renderSceneObjectIndex];
        if (!IntersectsFrustum(
                frustum,
                perObjectData.worldBoundsCenter,
                perObjectData.worldBoundsRadius
            ))
        {
            continue;
        }

        const float viewDepth = glm::dot(
            m_RenderWorld.viewInfo.forward,
            perObjectData.worldPosition - m_RenderWorld.viewInfo.position
        );

        m_RenderWorld.visibleSet.objects.push_back(VisibleObject{
            renderSceneObjectIndex,
            renderSceneObjectIndex,
            viewDepth,
            sceneObject.passMask
        });
    }
}

void RenderWorldCache::UpdatePerFrameData(std::uint64_t frameIndex, float timeSeconds, float deltaTime)
{
    m_RenderWorld.perFrameData = PerFrameData{
        frameIndex,
        timeSeconds,
        deltaTime
    };
}

void RenderWorldCache::UpdatePerViewData()
{
    const float width = static_cast<float>(std::max(m_RenderWorld.viewInfo.viewportWidth, 1));
    const float height = static_cast<float>(std::max(m_RenderWorld.viewInfo.viewportHeight, 1));
    m_RenderWorld.perViewData = PerViewData{
        CalculateLightSpaceMatrix(),
        glm::vec4(width, height, 1.0f / width, 1.0f / height)
    };
}

glm::mat4 RenderWorldCache::CalculateLightSpaceMatrix() const
{
    const glm::vec3 target(0.0f, 1.0f, 0.0f);
    const glm::vec3 lightDirection = glm::normalize(m_RenderWorld.visibleLights.directionalLight.direction);
    const glm::vec3 lightPosition = target - lightDirection * 12.0f;
    const glm::mat4 lightView = glm::lookAt(lightPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 lightProjection = glm::ortho(-12.0f, 12.0f, -12.0f, 12.0f, 1.0f, 30.0f);
    return lightProjection * lightView;
}
