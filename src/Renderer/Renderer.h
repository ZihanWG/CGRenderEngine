#pragma once

#include <array>
#include <memory>

#include <glm/glm.hpp>

#include "Renderer/Framebuffer.h"
#include "Renderer/RayTracer.h"
#include "Renderer/Texture2D.h"

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void Initialize(int viewportWidth, int viewportHeight);
    void RenderFrame(const class Scene& scene, const class Camera& camera, int viewportWidth, int viewportHeight);

private:
    void CreateShaders();
    void CreateRenderTargets(int width, int height);
    void EnsureRenderTargets(int width, int height);
    void RenderShadowPass(const class Scene& scene);
    void RenderScenePass(const class Scene& scene, const class Camera& camera);
    void RenderBloomPass();
    void RenderCompositePass();
    void BakeReference(const class Scene& scene, const class Camera& camera);
    glm::mat4 CalculateLightSpaceMatrix(const class Scene& scene) const;

    static constexpr int kShadowMapSize = 2048;

    int m_ViewportWidth = 0;
    int m_ViewportHeight = 0;
    int m_LastBlurTextureIndex = 0;
    bool m_Initialized = false;
    bool m_ReferenceDirty = true;
    float m_Exposure = 1.05f;
    float m_SplitPosition = 0.5f;
    int m_BloomPasses = 6;
    glm::mat4 m_LightSpaceMatrix{1.0f};

    std::unique_ptr<class Shader> m_ShadowShader;
    std::unique_ptr<class Shader> m_PBRShader;
    std::unique_ptr<class Shader> m_BlurShader;
    std::unique_ptr<class Shader> m_CompositeShader;
    std::shared_ptr<class Mesh> m_FullscreenQuad;

    Framebuffer m_ShadowFramebuffer;
    Framebuffer m_HDRFramebuffer;
    std::array<Framebuffer, 2> m_BlurFramebuffers;

    Texture2D m_ShadowTexture;
    Texture2D m_HDRColorTexture;
    Texture2D m_HDRBrightTexture;
    std::array<Texture2D, 2> m_BlurTextures;
    Texture2D m_ReferenceTexture;

    RayTracer m_RayTracer;
    RayTraceSettings m_RayTraceSettings;
};
