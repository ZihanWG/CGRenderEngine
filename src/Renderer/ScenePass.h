#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "Renderer/Framebuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture2D.h"

class Camera;
class Scene;
class Texture2D;
struct EnvironmentImage;

struct RenderSettings;

class ScenePass
{
public:
    void Initialize(int width, int height);
    void Resize(int width, int height);
    void Execute(
        const Scene& scene,
        const Camera& camera,
        const glm::mat4& lightSpaceMatrix,
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
    void AllocateTargets();
    void GenerateEnvironmentMap(const Scene& scene);
    void GenerateBrdfLut();

    bool m_Initialized = false;
    int m_Width = 0;
    int m_Height = 0;
    float m_EnvironmentMaxLod = 0.0f;
    bool m_EnvironmentReady = false;
    const EnvironmentImage* m_LastEnvironmentImage = nullptr;
    std::unique_ptr<Shader> m_Shader;
    std::unique_ptr<Shader> m_SkyShader;
    std::shared_ptr<class Mesh> m_FullscreenQuad;
    Framebuffer m_Framebuffer;
    Texture2D m_SceneColorTexture;
    Texture2D m_BrightTexture;
    Texture2D m_AlbedoTexture;
    Texture2D m_NormalTexture;
    Texture2D m_MaterialTexture;
    Texture2D m_DepthTexture;
    Texture2D m_DefaultBaseColorTexture;
    Texture2D m_DefaultMetallicRoughnessTexture;
    Texture2D m_DefaultNormalTexture;
    Texture2D m_DefaultOcclusionTexture;
    Texture2D m_DefaultEmissiveTexture;
    Texture2D m_EnvironmentTexture;
    Texture2D m_BrdfLutTexture;
    glm::vec3 m_LastEnvironmentLightDirection{0.0f};
    glm::vec3 m_LastEnvironmentLightColor{0.0f};
    float m_LastEnvironmentLightIntensity = -1.0f;
};
