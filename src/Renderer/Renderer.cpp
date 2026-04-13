// Orchestrates the frame graph and bridges realtime rendering with the async reference path.
//
// High-level frame flow:
// 1. react to scene/viewport invalidation
// 2. build or reuse RenderSubmission
// 3. schedule passes through RenderGraph
// 4. let the reference tracer progress opportunistically in the background
// 5. composite the final image to the default framebuffer
//
// This file is the best place to understand how the engine pieces fit together.
#include "Renderer/Renderer.h"

#include <algorithm>
#include <chrono>

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Assets/ResourceManager.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

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
    m_ShadowPass.Initialize(m_ResourceManager);
    m_ScenePass.Initialize(m_ResourceManager, m_ShaderBufferManager, m_ViewportWidth, m_ViewportHeight);
    m_BloomPass.Initialize(m_ResourceManager, m_ViewportWidth, m_ViewportHeight);
    m_CompositePass.Initialize(m_ResourceManager);
    m_Initialized = true;
}

void Renderer::RenderFrame(const Scene& scene, const Camera& camera, int viewportWidth, int viewportHeight)
{
    if (!m_Initialized)
    {
        Initialize(viewportWidth, viewportHeight);
    }

    if (scene.GetContentVersion() != m_LastSceneVersion)
    {
        // Any scene mutation invalidates both flattened submission data and the reference frame.
        m_LastSceneVersion = scene.GetContentVersion();
        m_SubmissionCache.Invalidate();
        InvalidateReference();
    }

    EnsureRenderTargets(viewportWidth, viewportHeight);
    const RenderSubmission& submission = m_SubmissionCache.Build(scene);
    m_LightSpaceMatrix = CalculateLightSpaceMatrix(submission);

    m_RenderGraph.Clear();
    // Pass order is now data-driven instead of being buried in ad-hoc call order.
    //
    // "reference" intentionally has no dependency on the realtime passes because it does
    // not consume GPU outputs. It only shares the Scene + Camera inputs and can therefore
    // overlap conceptually with the realtime frame, even though its result is only visible
    // once the async job completes in a later frame.
    m_RenderGraph.AddPass("shadow", {}, [&]() {
        m_ShadowPass.Execute(submission, m_LightSpaceMatrix, m_Settings.enableShadows);
    });
    m_RenderGraph.AddPass("scene", {"shadow"}, [&]() {
        m_ScenePass.Execute(
            submission,
            camera,
            m_LightSpaceMatrix,
            m_ShadowPass.GetShadowTexture(),
            m_Settings
        );
    });
    m_RenderGraph.AddPass("bloom", {"scene"}, [&]() {
        m_BloomPass.Execute(m_ScenePass.GetBrightTexture(), *m_FullscreenQuad, m_Settings);
    });
    m_RenderGraph.AddPass("reference", {}, [&]() {
        UpdateReference(scene, camera);
    });
    m_RenderGraph.AddPass("composite", {"scene", "bloom", "reference"}, [&]() {
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
    m_RenderGraph.Execute();
}

void Renderer::InvalidateReference()
{
    // Revision numbers let stale async jobs finish safely without overwriting newer work.
    ++m_ReferenceRevision;
    m_ReferenceDirty = true;
    m_HasReference = false;
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

void Renderer::UpdateReference(const Scene& scene, const Camera& camera)
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
    m_RayTraceFuture = std::async(std::launch::async, [this, sceneCopy, cameraCopy, activeSettings, revision]() {
        return ReferenceFrame{
            m_RayTracer.Render(sceneCopy, cameraCopy, activeSettings),
            activeSettings,
            revision
        };
    });
}

glm::mat4 Renderer::CalculateLightSpaceMatrix(const RenderSubmission& submission) const
{
    const glm::vec3 target(0.0f, 1.0f, 0.0f);
    const glm::vec3 lightDirection = glm::normalize(submission.sceneState.directionalLight.direction);
    const glm::vec3 lightPosition = target - lightDirection * 12.0f;
    const glm::mat4 lightView = glm::lookAt(lightPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 lightProjection = glm::ortho(-12.0f, 12.0f, -12.0f, 12.0f, 1.0f, 30.0f);
    return lightProjection * lightView;
}
