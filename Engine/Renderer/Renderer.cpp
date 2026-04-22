// Orchestrates the frame graph and bridges realtime rendering with the async reference path.
//
// High-level frame flow:
// 1. BeginFrame updates viewport-sized renderer state
// 2. BuildRenderWorld extracts Scene into RenderWorld and draw commands
// 3. BuildRenderGraph declares pass order and dependencies
// 4. ExecuteRenderGraph runs the scheduled passes
// 5. EndFrame clears transient frame state
//
// This file is the best place to understand how the engine pieces fit together.
#include "Engine/Renderer/Renderer.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Assets/ResourceManager.h"
#include "Engine/Core/JobSystem.h"
#include "Engine/Scene/Camera.h"
#include "Engine/Scene/Scene.h"

Renderer::Renderer(ResourceManager& resourceManager)
    : m_ResourceManager(resourceManager)
{
    // Keep the startup reference workload intentionally small so the window stays responsive.
    m_RayTraceSettings.samplesPerPixel = 2;
    m_RayTraceSettings.maxBounces = 1;
}

Renderer::~Renderer() = default;

void Renderer::Initialize(int viewportWidth, int viewportHeight)
{
    if (m_Initialized)
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_ViewportWidth = std::max(viewportWidth, 1);
    m_ViewportHeight = std::max(viewportHeight, 1);

    m_FullscreenQuad = m_ResourceManager.GetFullscreenQuad();
    // Pass objects own their own GL resources, but the renderer owns their lifetime and order.
    m_ShadowPass.Initialize(m_ResourceManager, m_ShaderBufferManager);
    m_ScenePass.Initialize(m_ResourceManager, m_ShaderBufferManager, m_ViewportWidth, m_ViewportHeight);
    m_BloomPass.Initialize(m_ResourceManager, m_ViewportWidth, m_ViewportHeight);
    m_CompositePass.Initialize(m_ResourceManager);
    m_Initialized = true;
}

void Renderer::BeginFrame(int viewportWidth, int viewportHeight, float timeSeconds, float deltaTime)
{
    if (!m_Initialized)
    {
        Initialize(viewportWidth, viewportHeight);
    }

    ResetFrameState();
    m_FrameActive = true;
    ++m_FrameIndex;
    m_FrameTimeSeconds = timeSeconds;
    m_FrameDeltaTime = deltaTime;
    m_ShaderBufferManager.BeginFrame();
    EnsureRenderTargets(viewportWidth, viewportHeight);
}

void Renderer::BuildRenderWorld(const Scene& scene, const Camera& camera)
{
    if (!m_FrameActive)
    {
        throw std::runtime_error("Renderer::BuildRenderWorld requires BeginFrame to run first.");
    }

    if (scene.GetContentVersion() != m_LastSceneVersion)
    {
        // Any scene mutation invalidates extracted render-world data, draw commands,
        // and the offline reference frame.
        m_LastSceneVersion = scene.GetContentVersion();
        m_RenderWorldCache.Invalidate();
        m_SubmissionCache.Invalidate();
        InvalidateReference();
    }

    m_FrameScene = &scene;
    m_FrameCamera = &camera;
    m_FrameRenderWorld = &m_RenderWorldCache.Build(
        scene,
        camera,
        m_ViewportWidth,
        m_ViewportHeight,
        m_FrameIndex,
        m_FrameTimeSeconds,
        m_FrameDeltaTime
    );
    m_FrameSubmission = &m_SubmissionCache.Build(*m_FrameRenderWorld);
}

void Renderer::BuildRenderGraph()
{
    if (!m_FrameActive)
    {
        throw std::runtime_error("Renderer::BuildRenderGraph requires BeginFrame to run first.");
    }
    if (!m_FrameScene || !m_FrameRenderWorld || !m_FrameSubmission || !m_FrameCamera)
    {
        throw std::runtime_error("Renderer::BuildRenderGraph requires BuildRenderWorld to run first.");
    }

    m_RenderGraph.Clear();
    // BuildRenderGraph is intentionally pure scheduling work: it declares pass order
    // and data dependencies, but does not execute the GPU work yet.
    //
    // Pass order is now data-driven instead of being buried in ad-hoc call order.
    //
    // "reference" intentionally has no dependency on the realtime passes because it does
    // not consume GPU outputs. It only shares the Scene + Camera inputs and can therefore
    // overlap conceptually with the realtime frame, even though its result is only visible
    // once the async job completes in a later frame.
    const RenderGraphResourceHandle renderWorldResource =
        m_RenderGraph.ImportResource("render_world", RenderGraphResourceType::CPUData);
    const RenderGraphResourceHandle renderSubmissionResource =
        m_RenderGraph.ImportResource("render_submission", RenderGraphResourceType::CPUData);
    const RenderGraphResourceHandle authoringSceneResource =
        m_RenderGraph.ImportResource("scene_authoring", RenderGraphResourceType::CPUData);
    const RenderGraphResourceHandle cameraResource =
        m_RenderGraph.ImportResource("camera", RenderGraphResourceType::CPUData);
    const RenderGraphResourceHandle swapchainBackbufferResource =
        m_RenderGraph.ImportResource("swapchain_backbuffer", RenderGraphResourceType::Backbuffer);

    const RenderGraphResourceHandle shadowFramebufferResource =
        m_RenderGraph.CreateResource("shadow_framebuffer", RenderGraphResourceType::Framebuffer);
    const RenderGraphResourceHandle sceneFramebufferResource =
        m_RenderGraph.CreateResource("scene_framebuffer", RenderGraphResourceType::Framebuffer);
    const RenderGraphResourceHandle bloomPingPongResource =
        m_RenderGraph.CreateResource("bloom_pingpong", RenderGraphResourceType::Framebuffer);
    const RenderGraphResourceHandle referenceTextureResource =
        m_RenderGraph.CreateResource("reference_texture", RenderGraphResourceType::Texture);

    const RenderGraphResourceHandle referenceFrameResource =
        m_RenderGraph.CreateResource("reference_frame", RenderGraphResourceType::CPUData);
    const RenderGraphResourceHandle shadowMapResource =
        m_RenderGraph.CreateResource("shadow_map", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle sceneColorResource =
        m_RenderGraph.CreateResource("scene_color", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle sceneBrightResource =
        m_RenderGraph.CreateResource("scene_bright", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle sceneAlbedoResource =
        m_RenderGraph.CreateResource("scene_albedo", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle sceneNormalResource =
        m_RenderGraph.CreateResource("scene_normal", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle sceneMaterialResource =
        m_RenderGraph.CreateResource("scene_material", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle sceneDepthResource =
        m_RenderGraph.CreateResource("scene_depth", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle bloomOutputResource =
        m_RenderGraph.CreateResource("bloom_output", RenderGraphResourceType::Texture);
    const RenderGraphResourceHandle referenceColorResource =
        m_RenderGraph.CreateResource("reference_color", RenderGraphResourceType::Texture);

    m_RenderGraph.AddPass("shadow")
        .Read({renderWorldResource, renderSubmissionResource})
        .Write(shadowMapResource)
        .Target(shadowFramebufferResource)
        .Execute([&]() {
            m_ShadowPass.Execute(*m_FrameRenderWorld, *m_FrameSubmission, m_Settings.enableShadows);
        });

    m_RenderGraph.AddPass("scene")
        .Read({renderWorldResource, renderSubmissionResource, shadowMapResource})
        .Write({
            sceneColorResource,
            sceneBrightResource,
            sceneAlbedoResource,
            sceneNormalResource,
            sceneMaterialResource,
            sceneDepthResource
        })
        .Target(sceneFramebufferResource)
        .Execute([&]() {
            m_ScenePass.Execute(
                *m_FrameRenderWorld,
                *m_FrameSubmission,
                m_ShadowPass.GetShadowTexture(),
                m_Settings
            );
        });

    m_RenderGraph.AddPass("bloom")
        .Read(sceneBrightResource)
        .Write(bloomOutputResource)
        .Target(bloomPingPongResource)
        .Execute([&]() {
            m_BloomPass.Execute(m_ScenePass.GetBrightTexture(), *m_FullscreenQuad, m_Settings);
        });

    m_RenderGraph.AddPass("reference_task")
        .Type(RenderGraphPassType::CPU)
        .Read({authoringSceneResource, cameraResource})
        .Write(referenceFrameResource)
        .Execute([&]() {
            UpdateReferenceTask(*m_FrameScene, *m_FrameCamera);
        });

    m_RenderGraph.AddPass("reference_upload")
        .Read(referenceFrameResource)
        .Write(referenceColorResource)
        .Target(referenceTextureResource)
        .Execute([&]() {
            UploadReferenceFrame();
        });

    m_RenderGraph.AddPass("composite")
        .Read({
            sceneColorResource,
            bloomOutputResource,
            referenceColorResource,
            sceneAlbedoResource,
            sceneNormalResource,
            sceneMaterialResource,
            sceneDepthResource,
            shadowMapResource
        })
        .Write(swapchainBackbufferResource)
        .Target(swapchainBackbufferResource)
        .Execute([&]() {
            glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
            m_CompositePass.Execute(
                m_ScenePass.GetSceneColorTexture(),
                m_BloomPass.ResolveOutputTexture(m_ScenePass.GetBrightTexture()),
                m_HasReference ? &m_ReferenceTexture : nullptr,
                m_ScenePass.GetAlbedoTexture(),
                m_ScenePass.GetNormalTexture(),
                m_ScenePass.GetMaterialTexture(),
                m_ScenePass.GetDepthTexture(),
                m_ShadowPass.GetShadowTexture(),
                m_Settings,
                *m_FullscreenQuad
            );
            glEnable(GL_DEPTH_TEST);
        });

    m_RenderGraph.Compile();
}

void Renderer::ExecuteRenderGraph()
{
    if (!m_FrameActive)
    {
        throw std::runtime_error("Renderer::ExecuteRenderGraph requires BeginFrame to run first.");
    }
    if (!m_FrameScene || !m_FrameCamera || !m_FrameRenderWorld || !m_FrameSubmission)
    {
        throw std::runtime_error(
            "Renderer::ExecuteRenderGraph requires BuildRenderWorld and BuildRenderGraph to run first."
        );
    }

    m_RenderGraph.Execute();
}

void Renderer::EndFrame()
{
    if (!m_FrameActive)
    {
        return;
    }

    ResetFrameState();
    m_FrameActive = false;
}

void Renderer::InvalidateReference()
{
    // Revision numbers let stale async jobs finish safely without overwriting newer work.
    ++m_ReferenceRevision;
    m_ReferenceDirty = true;
    m_HasReference = false;
    m_PendingReferenceFrame.reset();
}

void Renderer::SetSettings(const RenderSettings& settings)
{
    m_Settings = settings;
    m_Settings.exposure = std::clamp(m_Settings.exposure, 0.1f, 4.0f);
    m_Settings.environmentIntensity = std::clamp(m_Settings.environmentIntensity, 0.0f, 8.0f);
    m_Settings.splitPosition = std::clamp(m_Settings.splitPosition, 0.1f, 0.9f);
    m_Settings.bloomPasses = std::clamp(m_Settings.bloomPasses, 0, 12);
}

void Renderer::EnsureRenderTargets(int width, int height)
{
    const int safeWidth = std::max(width, 1);
    const int safeHeight = std::max(height, 1);

    if (safeWidth == m_ViewportWidth && safeHeight == m_ViewportHeight)
    {
        return;
    }

    m_ViewportWidth = safeWidth;
    m_ViewportHeight = safeHeight;
    // ShadowPass uses a fixed map size, so only scene/bloom/reference depend on the viewport.
    m_ScenePass.Resize(m_ViewportWidth, m_ViewportHeight);
    m_BloomPass.Resize(m_ViewportWidth, m_ViewportHeight);
    InvalidateReference();
}

void Renderer::UpdateReferenceTask(const Scene& scene, const Camera& camera)
{
    using namespace std::chrono_literals;

    if (m_RayTraceInFlight &&
        m_RayTraceFuture.valid() &&
        m_RayTraceFuture.wait_for(0ms) == std::future_status::ready)
    {
        // Consume the completed job without blocking the frame loop.
        // The revision guard matters because the user may have moved the camera or resized
        // the window while the worker thread was still tracing an older frame.
        ReferenceFrame referenceFrame = m_RayTraceFuture.get();
        m_RayTraceInFlight = false;

        if (referenceFrame.revision == m_ReferenceRevision)
        {
            m_PendingReferenceFrame = std::move(referenceFrame);
        }
    }

    if (m_RayTraceInFlight || !m_ReferenceDirty)
    {
        return;
    }

    const float aspectRatio = static_cast<float>(m_ViewportWidth) / static_cast<float>(std::max(m_ViewportHeight, 1));
    RayTraceSettings activeSettings = m_RayTraceSettings;
    // Downsample the reference image so the CPU path remains a background comparison tool.
    activeSettings.width = std::clamp(m_ViewportWidth / 3, 200, 320);
    activeSettings.height = std::max(
        120,
        static_cast<int>(static_cast<float>(activeSettings.width) / aspectRatio)
    );

    const Scene sceneCopy = scene;
    const Camera cameraCopy = camera;
    const std::size_t revision = m_ReferenceRevision;

    // Launch on a worker thread so the first presented frame does not stall on tracing.
    // Scene and Camera are copied on purpose: the worker must not read mutable engine state.
    m_ReferenceDirty = false;
    m_RayTraceInFlight = true;
    m_RayTraceFuture = JobSystem::Get().Submit([sceneCopy, cameraCopy, activeSettings, revision]() {
        RayTracer rayTracer;
        return ReferenceFrame{
            rayTracer.Render(sceneCopy, cameraCopy, activeSettings),
            activeSettings,
            revision
        };
    });
}

void Renderer::UploadReferenceFrame()
{
    if (!m_PendingReferenceFrame)
    {
        return;
    }

    ReferenceFrame& referenceFrame = *m_PendingReferenceFrame;
    m_ReferenceTexture.Allocate(
        referenceFrame.settings.width,
        referenceFrame.settings.height,
        GL_RGB16F,
        GL_RGB,
        GL_FLOAT,
        referenceFrame.pixels.data()
    );
    m_HasReference = true;
    m_ReferenceDirty = false;
    m_PendingReferenceFrame.reset();
}

void Renderer::ResetFrameState()
{
    m_RenderGraph.Clear();
    m_FrameScene = nullptr;
    m_FrameCamera = nullptr;
    m_FrameRenderWorld = nullptr;
    m_FrameSubmission = nullptr;
}
