#include "Renderer/Renderer.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Renderer/Mesh.h"
#include "Renderer/Shader.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

Renderer::Renderer()
{
    m_RayTraceSettings.samplesPerPixel = 4;
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

    m_FullscreenQuad = Mesh::CreateFullscreenQuad();
    CreateShaders();
    CreateRenderTargets(viewportWidth, viewportHeight);
    m_Initialized = true;
}

void Renderer::RenderFrame(const Scene& scene, const Camera& camera, int viewportWidth, int viewportHeight)
{
    if (!m_Initialized)
    {
        Initialize(viewportWidth, viewportHeight);
    }

    EnsureRenderTargets(viewportWidth, viewportHeight);
    m_LightSpaceMatrix = CalculateLightSpaceMatrix(scene);

    RenderShadowPass(scene);
    RenderScenePass(scene, camera);
    RenderBloomPass();

    if (m_ReferenceDirty)
    {
        BakeReference(scene, camera);
    }

    RenderCompositePass();
}

void Renderer::CreateShaders()
{
    m_ShadowShader = std::make_unique<Shader>("shaders/shadow.vert", "shaders/shadow.frag");
    m_PBRShader = std::make_unique<Shader>("shaders/pbr.vert", "shaders/pbr.frag");
    m_BlurShader = std::make_unique<Shader>("shaders/fullscreen.vert", "shaders/blur.frag");
    m_CompositeShader = std::make_unique<Shader>("shaders/fullscreen.vert", "shaders/composite.frag");

    m_BlurShader->Use();
    m_BlurShader->SetInt("uImage", 0);

    m_CompositeShader->Use();
    m_CompositeShader->SetInt("uSceneColor", 0);
    m_CompositeShader->SetInt("uBloomColor", 1);
    m_CompositeShader->SetInt("uReferenceColor", 2);
}

void Renderer::CreateRenderTargets(int width, int height)
{
    m_ViewportWidth = std::max(width, 1);
    m_ViewportHeight = std::max(height, 1);

    m_ShadowTexture.Allocate(
        kShadowMapSize,
        kShadowMapSize,
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr,
        GL_NEAREST,
        GL_NEAREST,
        GL_CLAMP_TO_BORDER,
        GL_CLAMP_TO_BORDER
    );
    m_ShadowTexture.SetBorderColor(1.0f, 1.0f, 1.0f, 1.0f);

    m_ShadowFramebuffer.Bind();
    m_ShadowFramebuffer.AttachDepthTexture(m_ShadowTexture);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (!m_ShadowFramebuffer.CheckComplete())
    {
        throw std::runtime_error("Shadow framebuffer is incomplete.");
    }

    m_HDRColorTexture.Allocate(m_ViewportWidth, m_ViewportHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_HDRBrightTexture.Allocate(m_ViewportWidth, m_ViewportHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);

    m_HDRFramebuffer.Bind();
    m_HDRFramebuffer.AttachColorTexture(m_HDRColorTexture, 0);
    m_HDRFramebuffer.AttachColorTexture(m_HDRBrightTexture, 1);
    m_HDRFramebuffer.CreateDepthRenderbuffer(m_ViewportWidth, m_ViewportHeight);
    m_HDRFramebuffer.SetDrawBuffers(2);
    if (!m_HDRFramebuffer.CheckComplete())
    {
        throw std::runtime_error("HDR framebuffer is incomplete.");
    }

    for (std::size_t i = 0; i < m_BlurTextures.size(); ++i)
    {
        m_BlurTextures[i].Allocate(m_ViewportWidth, m_ViewportHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        m_BlurFramebuffers[i].Bind();
        m_BlurFramebuffers[i].AttachColorTexture(m_BlurTextures[i], 0);
        m_BlurFramebuffers[i].SetDrawBuffers(1);
        if (!m_BlurFramebuffers[i].CheckComplete())
        {
            throw std::runtime_error("Blur framebuffer is incomplete.");
        }
    }

    Framebuffer::Unbind();
    m_ReferenceDirty = true;
}

void Renderer::EnsureRenderTargets(int width, int height)
{
    if (width == m_ViewportWidth && height == m_ViewportHeight)
    {
        return;
    }

    CreateRenderTargets(width, height);
}

void Renderer::RenderShadowPass(const Scene& scene)
{
    glViewport(0, 0, kShadowMapSize, kShadowMapSize);
    m_ShadowFramebuffer.Bind();
    glClear(GL_DEPTH_BUFFER_BIT);

    glCullFace(GL_FRONT);
    m_ShadowShader->Use();
    m_ShadowShader->SetMat4("uLightSpaceMatrix", m_LightSpaceMatrix);

    for (const RenderObject& object : scene.GetObjects())
    {
        if (!object.mesh)
        {
            continue;
        }

        m_ShadowShader->SetMat4("uModel", object.transform.GetMatrix());
        object.mesh->Draw();
    }

    glCullFace(GL_BACK);
    Framebuffer::Unbind();
}

void Renderer::RenderScenePass(const Scene& scene, const Camera& camera)
{
    glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
    m_HDRFramebuffer.Bind();
    glClearColor(0.06f, 0.07f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_PBRShader->Use();
    m_PBRShader->SetMat4("uView", camera.GetViewMatrix());
    m_PBRShader->SetMat4("uProjection", camera.GetProjectionMatrix());
    m_PBRShader->SetMat4("uLightSpaceMatrix", m_LightSpaceMatrix);
    m_PBRShader->SetVec3("uCameraPosition", camera.GetPosition());
    m_PBRShader->SetVec3("uDirectionalLight.direction", scene.GetDirectionalLight().direction);
    m_PBRShader->SetVec3("uDirectionalLight.color", scene.GetDirectionalLight().color);
    m_PBRShader->SetFloat("uDirectionalLight.intensity", scene.GetDirectionalLight().intensity);
    m_PBRShader->SetVec3("uPointLight.position", scene.GetPointLight().position);
    m_PBRShader->SetVec3("uPointLight.color", scene.GetPointLight().color);
    m_PBRShader->SetFloat("uPointLight.intensity", scene.GetPointLight().intensity);
    m_PBRShader->SetFloat("uPointLight.range", scene.GetPointLight().range);
    m_PBRShader->SetInt("uShadowMap", 0);
    m_ShadowTexture.Bind(0);

    for (const RenderObject& object : scene.GetObjects())
    {
        if (!object.mesh)
        {
            continue;
        }

        m_PBRShader->SetMat4("uModel", object.transform.GetMatrix());
        m_PBRShader->SetVec3("uMaterial.albedo", object.material.albedo);
        m_PBRShader->SetFloat("uMaterial.metallic", object.material.metallic);
        m_PBRShader->SetFloat("uMaterial.roughness", object.material.roughness);
        m_PBRShader->SetFloat("uMaterial.ao", object.material.ao);
        m_PBRShader->SetVec3("uMaterial.emissive", object.material.emissive);
        object.mesh->Draw();
    }

    Framebuffer::Unbind();
}

void Renderer::RenderBloomPass()
{
    glDisable(GL_DEPTH_TEST);

    bool horizontal = true;
    bool firstIteration = true;

    m_BlurShader->Use();

    for (int i = 0; i < m_BloomPasses; ++i)
    {
        const int targetIndex = horizontal ? 0 : 1;
        const int sourceIndex = horizontal ? 1 : 0;

        m_BlurFramebuffers[targetIndex].Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        m_BlurShader->SetInt("uHorizontal", horizontal ? 1 : 0);

        if (firstIteration)
        {
            m_HDRBrightTexture.Bind(0);
        }
        else
        {
            m_BlurTextures[sourceIndex].Bind(0);
        }

        m_FullscreenQuad->Draw();
        horizontal = !horizontal;
        firstIteration = false;
    }

    m_LastBlurTextureIndex = horizontal ? 1 : 0;
    Framebuffer::Unbind();
}

void Renderer::RenderCompositePass()
{
    Framebuffer::Unbind();
    glViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    m_CompositeShader->Use();
    m_CompositeShader->SetFloat("uExposure", m_Exposure);
    m_CompositeShader->SetFloat("uSplitPosition", m_SplitPosition);

    m_HDRColorTexture.Bind(0);
    m_BlurTextures[m_LastBlurTextureIndex].Bind(1);
    m_ReferenceTexture.Bind(2);
    m_FullscreenQuad->Draw();

    glEnable(GL_DEPTH_TEST);
}

void Renderer::BakeReference(const Scene& scene, const Camera& camera)
{
    const float aspectRatio = static_cast<float>(m_ViewportWidth) / static_cast<float>(std::max(m_ViewportHeight, 1));
    m_RayTraceSettings.width = std::clamp(m_ViewportWidth / 2, 320, 480);
    m_RayTraceSettings.height = std::max(
        180,
        static_cast<int>(static_cast<float>(m_RayTraceSettings.width) / aspectRatio)
    );

    const std::vector<glm::vec3> pixels = m_RayTracer.Render(scene, camera, m_RayTraceSettings);
    m_ReferenceTexture.Allocate(
        m_RayTraceSettings.width,
        m_RayTraceSettings.height,
        GL_RGB16F,
        GL_RGB,
        GL_FLOAT,
        pixels.data()
    );

    m_ReferenceDirty = false;
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
