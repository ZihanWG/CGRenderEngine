// Main realtime scene pass. Produces HDR color plus debug G-buffer style outputs.
#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Engine/RHI/Framebuffer.h"
#include "Engine/Renderer/MaterialBinder.h"
#include "Engine/RHI/ShaderBufferManager.h"
#include "Engine/RHI/Shader.h"
#include "Engine/RHI/Texture2D.h"

class ResourceManager;
class Texture2D;
struct EnvironmentImage;
struct RenderWorld;
struct RenderSubmission;

struct RenderSettings;

class ScenePass
{
public:
    void Initialize(ResourceManager& resourceManager, ShaderBufferManager& bufferManager, int width, int height);
    void Resize(int width, int height);
    void Execute(
        const RenderWorld& renderWorld,
        const RenderSubmission& submission,
        const Texture2D& shadowTexture,
        const RenderSettings& settings
    );

    const Texture2D& GetSceneColorTexture() const { return m_SceneColorTexture; }
    const Texture2D& GetBrightTexture() const { return m_BrightTexture; }
    const Texture2D& GetAlbedoTexture() const { return m_AlbedoTexture; }
    const Texture2D& GetNormalTexture() const { return m_NormalTexture; }
    const Texture2D& GetMaterialTexture() const { return m_MaterialTexture; }
    const Texture2D& GetDepthTexture() const { return m_DepthTexture; }

private:
    // Allocate the MRT framebuffer used by the scene pass and debug views.
    void AllocateTargets();
    // Upload either the HDR environment texture or a generated procedural sky map.
    void GenerateEnvironmentMap(const RenderWorld& renderWorld);
    // Precompute the split-sum BRDF approximation once for specular IBL.
    void GenerateBrdfLut();

    bool m_Initialized = false;
    int m_Width = 0;
    int m_Height = 0;
    float m_EnvironmentMaxLod = 0.0f;
    bool m_EnvironmentReady = false;
    const EnvironmentImage* m_LastEnvironmentImage = nullptr;
    ShaderBufferManager* m_BufferManager = nullptr;
    std::shared_ptr<Shader> m_Shader;
    std::shared_ptr<Shader> m_SkyShader;
    std::shared_ptr<class Mesh> m_FullscreenQuad;
    MaterialBinder m_MaterialBinder;
    Framebuffer m_Framebuffer;
    Texture2D m_SceneColorTexture;
    Texture2D m_BrightTexture;
    Texture2D m_AlbedoTexture;
    Texture2D m_NormalTexture;
    Texture2D m_MaterialTexture;
    Texture2D m_DepthTexture;
    Texture2D m_EnvironmentTexture;
    Texture2D m_BrdfLutTexture;
    glm::vec3 m_LastEnvironmentLightDirection{0.0f};
    glm::vec3 m_LastEnvironmentLightColor{0.0f};
    float m_LastEnvironmentLightIntensity = -1.0f;
};
