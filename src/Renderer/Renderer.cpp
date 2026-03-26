#include "Renderer/Renderer.h"

#include <algorithm>
#include <chrono>

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Renderer/Mesh.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

Renderer::Renderer()
{
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

    m_FullscreenQuad = Mesh::CreateFullscreenQuad();
    m_ShadowPass.Initialize();
    m_ScenePass.Initialize(m_ViewportWidth, m_ViewportHeight);
    m_BloomPass.Initialize(m_ViewportWidth, m_ViewportHeight);
    m_CompositePass.Initialize();
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
        m_LastSceneVersion = scene.GetContentVersion();
        InvalidateReference();
    }

    EnsureRenderTargets(viewportWidth, viewportHeight);
    m_LightSpaceMatrix = CalculateLightSpaceMatrix(scene);

    m_ShadowPass.Execute(scene, m_LightSpaceMatrix, m_Settings.enableShadows);
    m_ScenePass.Execute(
        scene,
        camera,
        m_LightSpaceMatrix,
        m_ShadowPass.GetShadowTexture(),
        m_Settings
    );
    m_BloomPass.Execute(m_ScenePass.GetBrightTexture(), *m_FullscreenQuad, m_Settings);
    UpdateReference(scene, camera);

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
}

void Renderer::InvalidateReference()
{
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
    activeSettings.width = std::clamp(m_ViewportWidth / 3, 200, 320);
    activeSettings.height = std::max(
        120,
        static_cast<int>(static_cast<float>(activeSettings.width) / aspectRatio)
    );

    const Scene sceneCopy = scene;
    const Camera cameraCopy = camera;
    const std::size_t revision = m_ReferenceRevision;

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

glm::mat4 Renderer::CalculateLightSpaceMatrix(const Scene& scene) const
{
    const glm::vec3 target(0.0f, 1.0f, 0.0f);
    const glm::vec3 lightDirection = glm::normalize(scene.GetDirectionalLight().direction);
    const glm::vec3 lightPosition = target - lightDirection * 12.0f;
    const glm::mat4 lightView = glm::lookAt(lightPosition, target, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 lightProjection = glm::ortho(-12.0f, 12.0f, -12.0f, 12.0f, 1.0f, 30.0f);
    return lightProjection * lightView;
}
