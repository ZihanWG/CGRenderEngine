// Renders a single directional-light shadow map used by the forward PBR pass.
#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Engine/RHI/Framebuffer.h"
#include "Engine/RHI/ShaderBufferManager.h"
#include "Engine/RHI/Shader.h"
#include "Engine/RHI/Texture2D.h"

class ResourceManager;
struct RenderWorld;
struct RenderSubmission;

class ShadowPass
{
public:
    void Initialize(ResourceManager& resourceManager, ShaderBufferManager& bufferManager);
    void Execute(const RenderWorld& renderWorld, const RenderSubmission& submission, bool enabled);

    const Texture2D& GetShadowTexture() const { return m_ShadowTexture; }

private:
    static constexpr int kResolution = 2048;

    bool m_Initialized = false;
    ShaderBufferManager* m_BufferManager = nullptr;
    std::shared_ptr<Shader> m_Shader;
    Framebuffer m_Framebuffer;
    Texture2D m_ShadowTexture;
};
