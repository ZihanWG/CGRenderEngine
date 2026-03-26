#include "Renderer/CompositePass.h"

#include <glad/glad.h>

#include "Renderer/Framebuffer.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture2D.h"

void CompositePass::Initialize()
{
    if (m_Initialized)
    {
        return;
    }

    m_Shader = std::make_unique<Shader>("shaders/fullscreen.vert", "shaders/composite.frag");
    m_Shader->Use();
    m_Shader->SetInt("uSceneColor", 0);
    m_Shader->SetInt("uBloomColor", 1);
    m_Shader->SetInt("uReferenceColor", 2);
    m_Shader->SetInt("uAlbedoColor", 3);
    m_Shader->SetInt("uNormalColor", 4);
    m_Shader->SetInt("uMaterialColor", 5);
    m_Shader->SetInt("uDepthTexture", 6);
    m_Shader->SetInt("uShadowMap", 7);
    m_Shader->SetInt("uHasReference", 0);
    m_Initialized = true;
}

void CompositePass::Execute(
    const Texture2D& sceneColor,
    const Texture2D& bloomColor,
    const Texture2D* referenceColor,
    const Texture2D& albedoColor,
    const Texture2D& normalColor,
    const Texture2D& materialColor,
    const Texture2D& depthTexture,
    const Texture2D& shadowTexture,
    const RenderSettings& settings,
    const Mesh& fullscreenQuad
)
{
    glDisable(GL_DEPTH_TEST);
    Framebuffer::Unbind();
    glClear(GL_COLOR_BUFFER_BIT);

    m_Shader->Use();
    m_Shader->SetFloat("uExposure", settings.exposure);
    m_Shader->SetFloat("uSplitPosition", settings.splitPosition);
    m_Shader->SetInt("uHasReference", referenceColor && settings.enableReferenceComparison ? 1 : 0);
    m_Shader->SetInt("uDebugView", static_cast<int>(settings.debugView));

    sceneColor.Bind(0);
    bloomColor.Bind(1);
    if (referenceColor && settings.enableReferenceComparison)
    {
        referenceColor->Bind(2);
    }
    else
    {
        sceneColor.Bind(2);
    }
    albedoColor.Bind(3);
    normalColor.Bind(4);
    materialColor.Bind(5);
    depthTexture.Bind(6);
    shadowTexture.Bind(7);

    fullscreenQuad.Draw();
}
