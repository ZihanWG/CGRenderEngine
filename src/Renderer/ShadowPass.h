// Renders a single directional-light shadow map used by the forward PBR pass.
#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture2D.h"

class ResourceManager;
struct RenderSubmission;

class ShadowPass
{
public:
    void Initialize(ResourceManager& resourceManager);
    void Execute(const RenderSubmission& submission, const glm::mat4& lightSpaceMatrix, bool enabled);

    const Texture2D& GetShadowTexture() const { return m_ShadowTexture; }

private:
    static constexpr int kResolution = 2048;

    bool m_Initialized = false;
    std::shared_ptr<Shader> m_Shader;
    Framebuffer m_Framebuffer;
    Texture2D m_ShadowTexture;
};
