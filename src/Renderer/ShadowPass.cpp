#include "Renderer/ShadowPass.h"

#include <stdexcept>

#include <glad/glad.h>

#include "Assets/ResourceManager.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderSubmission.h"

void ShadowPass::Initialize(ResourceManager& resourceManager)
{
    if (m_Initialized)
    {
        return;
    }

    m_Shader = resourceManager.LoadShader("shaders/shadow.vert", "shaders/shadow.frag");
    m_ShadowTexture.Allocate(
        kResolution,
        kResolution,
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

    m_Framebuffer.Bind();
    m_Framebuffer.AttachDepthTexture(m_ShadowTexture);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (!m_Framebuffer.CheckComplete())
    {
        throw std::runtime_error("Shadow framebuffer is incomplete.");
    }

    Framebuffer::Unbind();
    m_Initialized = true;
}

void ShadowPass::Execute(const RenderSubmission& submission, const glm::mat4& lightSpaceMatrix, bool enabled)
{
    if (!m_Initialized)
    {
        return;
    }

    glViewport(0, 0, kResolution, kResolution);
    m_Framebuffer.Bind();
    glClear(GL_DEPTH_BUFFER_BIT);

    if (!enabled)
    {
        Framebuffer::Unbind();
        return;
    }

    glCullFace(GL_FRONT);
    m_Shader->Use();
    m_Shader->SetMat4("uLightSpaceMatrix", lightSpaceMatrix);

    for (const RenderItem& item : submission.drawItems)
    {
        if (!item.mesh)
        {
            continue;
        }

        m_Shader->SetMat4("uModel", item.modelMatrix);
        item.mesh->Draw();
    }

    glCullFace(GL_BACK);
    Framebuffer::Unbind();
}
