// Render-visible world state built from Scene + Camera before draw command generation.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Scene/Environment.h"
#include "Engine/Scene/Light.h"

class Camera;
class Mesh;
class Scene;
struct Material;
class MaterialInstance;

enum class PassMask : std::uint32_t
{
    None = 0u,
    Shadow = 1u << 0u,
    Opaque = 1u << 1u,
    Transparent = 1u << 2u
};

constexpr PassMask operator|(PassMask left, PassMask right)
{
    return static_cast<PassMask>(
        static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right)
    );
}

constexpr PassMask operator&(PassMask left, PassMask right)
{
    return static_cast<PassMask>(
        static_cast<std::uint32_t>(left) & static_cast<std::uint32_t>(right)
    );
}

constexpr PassMask& operator|=(PassMask& left, PassMask right)
{
    left = left | right;
    return left;
}

constexpr bool HasPass(PassMask mask, PassMask pass)
{
    return static_cast<std::uint32_t>(mask & pass) != 0u;
}

struct RenderSceneObject
{
    std::size_t objectIndex = 0;
    std::string name;
    std::shared_ptr<Mesh> mesh;
    const Material* material = nullptr;
    const MaterialInstance* materialInstance = nullptr;
    std::uint32_t renderStateId = 0;
    std::uint32_t materialStateId = 0;
    std::uint32_t materialAssetStateId = 0;
    std::uint32_t meshStateId = 0;
    PassMask passMask = PassMask::None;
};

struct RenderScene
{
    std::vector<RenderSceneObject> objects;
    const SceneEnvironment* environment = nullptr;
    std::size_t sceneVersion = 0;
};

struct ViewInfo
{
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    glm::mat4 inverseViewProjection{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    int viewportWidth = 1;
    int viewportHeight = 1;
};

struct VisibleLightList
{
    DirectionalLight directionalLight;
    std::vector<PointLight> pointLights;
};

struct PerFrameData
{
    std::uint64_t frameIndex = 0;
    float timeSeconds = 0.0f;
    float deltaTime = 0.0f;
};

struct PerViewData
{
    glm::mat4 lightSpaceMatrix{1.0f};
    glm::vec4 viewportData{1.0f, 1.0f, 1.0f, 1.0f};
};

struct PerObjectData
{
    std::size_t renderSceneObjectIndex = 0;
    glm::mat4 modelMatrix{1.0f};
    glm::vec3 worldPosition{0.0f};
    glm::vec3 worldBoundsCenter{0.0f};
    float worldBoundsRadius = 0.0f;
};

struct VisibleObject
{
    std::size_t renderSceneObjectIndex = 0;
    std::size_t perObjectDataIndex = 0;
    float viewDepth = 0.0f;
    PassMask passMask = PassMask::None;
};

struct VisibleSet
{
    std::vector<VisibleObject> objects;
};

struct RenderWorld
{
    RenderScene renderScene;
    ViewInfo viewInfo;
    VisibleLightList visibleLights;
    PerFrameData perFrameData;
    PerViewData perViewData;
    std::vector<PerObjectData> perObjectData;
    VisibleSet visibleSet;
};

class RenderWorldCache
{
public:
    const RenderWorld& Build(
        const Scene& scene,
        const Camera& camera,
        int viewportWidth,
        int viewportHeight,
        std::uint64_t frameIndex,
        float timeSeconds,
        float deltaTime
    );
    void Invalidate();

private:
    void ExtractRenderScene(const Scene& scene);
    void BuildViewInfo(const Camera& camera, int viewportWidth, int viewportHeight);
    void BuildVisibleLightList(const Scene& scene);
    void BuildVisibleSet();
    void UpdatePerFrameData(std::uint64_t frameIndex, float timeSeconds, float deltaTime);
    void UpdatePerViewData();
    glm::mat4 CalculateLightSpaceMatrix() const;

    std::size_t m_BuiltSceneVersion = 0;
    bool m_HasCachedView = false;
    glm::vec3 m_LastCameraPosition{0.0f};
    glm::vec3 m_LastCameraForward{0.0f};
    int m_LastViewportWidth = 0;
    int m_LastViewportHeight = 0;
    RenderWorld m_RenderWorld;
};
