#pragma once

#include <cstddef>
#include <future>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Renderer/BloomPass.h"
#include "Renderer/CompositePass.h"
#include "Renderer/RayTracer.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/ScenePass.h"
#include "Renderer/ShadowPass.h"
#include "Renderer/Texture2D.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Initialize(int viewportWidth, int viewportHeight);
    void RenderFrame(const class Scene& scene, const class Camera& camera, int viewportWidth, int viewportHeight);
    void InvalidateReference();

    const RenderSettings& GetSettings() const { return m_Settings; }
    void SetSettings(const RenderSettings& settings);

private:
    struct ReferenceFrame
    {
        std::vector<glm::vec3> pixels;
        RayTraceSettings settings;
        std::size_t revision = 0;
    };

    void EnsureRenderTargets(int width, int height);
    void UpdateReference(const class Scene& scene, const class Camera& camera);
    glm::mat4 CalculateLightSpaceMatrix(const class Scene& scene) const;

    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;
    bool m_Initialized = false;
    bool m_ReferenceDirty = true;
    bool m_RayTraceInFlight = false;
    bool m_HasReference = false;
    std::size_t m_ReferenceRevision = 1;
    std::size_t m_LastSceneVersion = 0;
    glm::mat4 m_LightSpaceMatrix{1.0f};

    std::shared_ptr<class Mesh> m_FullscreenQuad;

    RenderSettings m_Settings;
    ShadowPass m_ShadowPass;
    ScenePass m_ScenePass;
    BloomPass m_BloomPass;
    CompositePass m_CompositePass;
    Texture2D m_ReferenceTexture;

    RayTracer m_RayTracer;
    RayTraceSettings m_RayTraceSettings;
    std::future<ReferenceFrame> m_RayTraceFuture;
};
