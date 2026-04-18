// Ping-pong Gaussian blur used to spread bright highlights into a bloom texture.
#include "Engine/Renderer/BloomPass.h"

#include <algorithm>
#include <stdexcept>

#include <glad/glad.h>

#include "Engine/Assets/ResourceManager.h"
#include "Engine/RHI/Mesh.h"
#include "Engine/Renderer/RenderSettings.h"

void BloomPass::Initialize(ResourceManager& resourceManager, int width, int height)
{
    if (m_Initialized)
    {
        Resize(width, height);
        return;
    }

    m_Shader = resourceManager.LoadShader("Shaders/fullscreen.vert", "Shaders/blur.frag");
    m_Shader->Use();
    m_Shader->SetInt("uImage", 0);

    m_Initialized = true;
    Resize(width, height);
}

void BloomPass::Resize(int width, int height)
{
    m_Width = std::max(width, 1);
    m_Height = std::max(height, 1);
    AllocateTargets();
}

void BloomPass::Execute(const Texture2D& sourceTexture, const Mesh& fullscreenQuad, const RenderSettings& settings)
{
    if (!settings.enableBloom || settings.bloomPasses <= 0)
    {
        // Fall back to the original bright-pass texture when bloom is disabled.
        m_UsesSourceTexture = true;
        return;
    }

    m_UsesSourceTexture = false;
    glDisable(GL_DEPTH_TEST);

    bool horizontal = true;
    bool firstIteration = true;
    m_Shader->Use();

    for (int i = 0; i < settings.bloomPasses; ++i)
    {
        // Alternate horizontal and vertical blur passes between the two intermediate targets.
        const int targetIndex = horizontal ? 0 : 1;
        const int sourceIndex = horizontal ? 1 : 0;

        m_Framebuffers[targetIndex].Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        m_Shader->SetInt("uHorizontal", horizontal ? 1 : 0);

        if (firstIteration)
        {
            sourceTexture.Bind(0);
        }
        else
        {
            m_BlurTextures[sourceIndex].Bind(0);
        }

        fullscreenQuad.Draw();
        horizontal = !horizontal;
        firstIteration = false;
    }

    m_LastBlurTextureIndex = horizontal ? 1 : 0;
    Framebuffer::Unbind();
}

const Texture2D& BloomPass::ResolveOutputTexture(const Texture2D& sourceTexture) const
{
    return m_UsesSourceTexture ? sourceTexture : m_BlurTextures[m_LastBlurTextureIndex];
}

void BloomPass::AllocateTargets()
{
    for (std::size_t i = 0; i < m_BlurTextures.size(); ++i)
    {
        m_BlurTextures[i].Allocate(m_Width, m_Height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
        m_Framebuffers[i].Bind();
        m_Framebuffers[i].AttachColorTexture(m_BlurTextures[i], 0);
        m_Framebuffers[i].SetDrawBuffers(1);
        if (!m_Framebuffers[i].CheckComplete())
        {
            throw std::runtime_error("Bloom framebuffer is incomplete.");
        }
    }

    Framebuffer::Unbind();
}
