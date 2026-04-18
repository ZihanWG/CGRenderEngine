// Top-level render coordinator that owns pass objects and the offline reference job.
#pragma once

#include <cstddef>
#include <future>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Renderer/BloomPass.h"
#include "Engine/Renderer/CompositePass.h"
#include "Engine/Renderer/RayTracer.h"
#include "Engine/Renderer/RenderGraph.h"
#include "Engine/Renderer/RenderSettings.h"
#include "Engine/Renderer/RenderSubmission.h"
#include "Engine/Renderer/RenderWorld.h"
#include "Engine/Renderer/ScenePass.h"
#include "Engine/RHI/ShaderBufferManager.h"
#include "Engine/Renderer/ShadowPass.h"
#include "Engine/RHI/Texture2D.h"

class ResourceManager;

class Renderer
{
public:
    explicit Renderer(ResourceManager& resourceManager);
    ~Renderer();

    void Initialize(int viewportWidth, int viewportHeight);
    void BeginFrame(int viewportWidth, int viewportHeight, float timeSeconds, float deltaTime);
    void BuildRenderWorld(const class Scene& scene, const class Camera& camera);
    void BuildRenderGraph();
    void ExecuteRenderGraph();
    void EndFrame();
    void InvalidateReference();

    const RenderSettings& GetSettings() const { return m_Settings; }
    void SetSettings(const RenderSettings& settings);

private:
    // CPU ray-traced frame produced asynchronously for realtime/offline comparison.
    struct ReferenceFrame
    {
        std::vector<glm::vec3> pixels;
        RayTraceSettings settings;
        std::size_t revision = 0;
    };

    // Reallocates viewport-dependent render targets when the window changes size.
    void EnsureRenderTargets(int width, int height);
    // Starts or consumes the asynchronous ray tracing task when needed.
    void UpdateReference(const class Scene& scene, const class Camera& camera);
    // Clear pointers and graph state carried only for the current frame.
    void ResetFrameState();

    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;
    bool m_Initialized = false;
    bool m_FrameActive = false;
    bool m_ReferenceDirty = true;
    bool m_RayTraceInFlight = false;
    bool m_HasReference = false;
    std::size_t m_ReferenceRevision = 1;
    std::size_t m_LastSceneVersion = 0;
    std::uint64_t m_FrameIndex = 0;
    float m_FrameTimeSeconds = 0.0f;
    float m_FrameDeltaTime = 0.0f;
    ResourceManager& m_ResourceManager;
    const class Scene* m_FrameScene = nullptr;
    const class Camera* m_FrameCamera = nullptr;
    const RenderWorld* m_FrameRenderWorld = nullptr;
    const RenderSubmission* m_FrameSubmission = nullptr;

    std::shared_ptr<class Mesh> m_FullscreenQuad;

    RenderSettings m_Settings;
    RenderGraph m_RenderGraph;
    RenderWorldCache m_RenderWorldCache;
    RenderSubmissionCache m_SubmissionCache;
    ShaderBufferManager m_ShaderBufferManager;
    ShadowPass m_ShadowPass;
    ScenePass m_ScenePass;
    BloomPass m_BloomPass;
    CompositePass m_CompositePass;
    Texture2D m_ReferenceTexture;

    RayTraceSettings m_RayTraceSettings;
    std::future<ReferenceFrame> m_RayTraceFuture;
};
