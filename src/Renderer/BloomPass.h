// Applies a separable Gaussian blur to the bright-pass texture for bloom.
#pragma once

#include <array>
#include <memory>

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture2D.h"

class Mesh;
class ResourceManager;
class Texture2D;

struct RenderSettings;

class BloomPass
{
public:
    void Initialize(ResourceManager& resourceManager, int width, int height);
    void Resize(int width, int height);
    void Execute(const Texture2D& sourceTexture, const Mesh& fullscreenQuad, const RenderSettings& settings);

    const Texture2D& ResolveOutputTexture(const Texture2D& sourceTexture) const;

private:
    // Ping-pong blur targets sized to the current viewport.
    void AllocateTargets();

    bool m_Initialized = false;
    bool m_UsesSourceTexture = true;
    int m_Width = 0;
    int m_Height = 0;
    int m_LastBlurTextureIndex = 0;
    std::shared_ptr<Shader> m_Shader;
    std::array<Framebuffer, 2> m_Framebuffers;
    std::array<Texture2D, 2> m_BlurTextures;
};
