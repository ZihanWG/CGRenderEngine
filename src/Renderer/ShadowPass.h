#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture2D.h"

class Scene;

class ShadowPass
{
public:
    void Initialize();
    void Execute(const Scene& scene, const glm::mat4& lightSpaceMatrix, bool enabled);

    const Texture2D& GetShadowTexture() const { return m_ShadowTexture; }

private:
    static constexpr int kResolution = 2048;

    bool m_Initialized = false;
    std::unique_ptr<Shader> m_Shader;
    Framebuffer m_Framebuffer;
    Texture2D m_ShadowTexture;
};
