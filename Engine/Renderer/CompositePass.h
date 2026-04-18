// Final fullscreen pass that tone maps, debugs buffers, and optionally compares references.
#pragma once

#include <memory>

#include "Engine/RHI/Shader.h"

class Mesh;
class ResourceManager;
class Texture2D;

struct RenderSettings;

class CompositePass
{
public:
    void Initialize(ResourceManager& resourceManager);
    void Execute(
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
    );

private:
    bool m_Initialized = false;
    std::shared_ptr<Shader> m_Shader;
};
