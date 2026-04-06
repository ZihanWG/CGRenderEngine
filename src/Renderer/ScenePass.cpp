#include "Renderer/ScenePass.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

#include <glm/gtc/constants.hpp>

#include "Renderer/EnvironmentLighting.h"
#include "Renderer/Mesh.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/Shader.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

namespace
{
    constexpr int kEnvironmentWidth = 512;
    constexpr int kEnvironmentHeight = 256;
    constexpr int kBrdfLutSize = 128;
    constexpr unsigned int kBrdfSampleCount = 64u;
    constexpr float kPi = 3.14159265359f;

    float RadicalInverseVdC(unsigned int bits)
    {
        bits = (bits << 16u) | (bits >> 16u);
        bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
        bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
        bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
        bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
        return static_cast<float>(bits) * 2.3283064365386963e-10f;
    }

    glm::vec2 Hammersley(unsigned int i, unsigned int n)
    {
        return glm::vec2(
            static_cast<float>(i) / static_cast<float>(n),
            RadicalInverseVdC(i)
        );
    }

    glm::vec3 ImportanceSampleGGX(const glm::vec2& xi, const glm::vec3& normal, float roughness)
    {
        const float a = roughness * roughness;
        const float phi = 2.0f * kPi * xi.x;
        const float cosTheta = std::sqrt((1.0f - xi.y) / (1.0f + (a * a - 1.0f) * xi.y));
        const float sinTheta = std::sqrt(glm::max(1.0f - cosTheta * cosTheta, 0.0f));

        const glm::vec3 halfway(
            std::cos(phi) * sinTheta,
            std::sin(phi) * sinTheta,
            cosTheta
        );

        const glm::vec3 up = glm::abs(normal.z) < 0.999f
            ? glm::vec3(0.0f, 0.0f, 1.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(up, normal));
        const glm::vec3 bitangent = glm::cross(normal, tangent);

        return glm::normalize(
            tangent * halfway.x +
            bitangent * halfway.y +
            normal * halfway.z
        );
    }

    float GeometrySchlickGGX(float nDotV, float roughness)
    {
        const float a = roughness;
        const float k = (a * a) * 0.5f;
        return nDotV / glm::max(nDotV * (1.0f - k) + k, 1e-5f);
    }

    float GeometrySmith(float nDotV, float nDotL, float roughness)
    {
        return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
    }

    glm::vec2 IntegrateBRDF(float nDotV, float roughness)
    {
        const glm::vec3 view(
            std::sqrt(glm::max(1.0f - nDotV * nDotV, 0.0f)),
            0.0f,
            nDotV
        );
        const glm::vec3 normal(0.0f, 0.0f, 1.0f);

        float scale = 0.0f;
        float bias = 0.0f;
        for (unsigned int sampleIndex = 0; sampleIndex < kBrdfSampleCount; ++sampleIndex)
        {
            const glm::vec2 xi = Hammersley(sampleIndex, kBrdfSampleCount);
            const glm::vec3 halfway = ImportanceSampleGGX(xi, normal, roughness);
            const glm::vec3 light = glm::normalize(2.0f * glm::dot(view, halfway) * halfway - view);

            const float nDotL = glm::max(light.z, 0.0f);
            const float nDotH = glm::max(halfway.z, 0.0f);
            const float vDotH = glm::max(glm::dot(view, halfway), 0.0f);
            if (nDotL <= 0.0f)
            {
                continue;
            }

            const float geometry = GeometrySmith(nDotV, nDotL, roughness);
            const float visibility = (geometry * vDotH) / glm::max(nDotH * nDotV, 1e-5f);
            const float fresnel = std::pow(1.0f - vDotH, 5.0f);

            scale += (1.0f - fresnel) * visibility;
            bias += fresnel * visibility;
        }

        return glm::vec2(scale, bias) / static_cast<float>(kBrdfSampleCount);
    }
}

void ScenePass::Initialize(int width, int height)
{
    if (m_Initialized)
    {
        Resize(width, height);
        return;
    }

    m_Shader = std::make_unique<Shader>("shaders/pbr.vert", "shaders/pbr.frag");
    m_Shader->Use();
    m_Shader->SetInt("uShadowMap", 0);
    m_Shader->SetInt("uBaseColorTexture", 1);
    m_Shader->SetInt("uMetallicRoughnessTexture", 2);
    m_Shader->SetInt("uNormalTexture", 3);
    m_Shader->SetInt("uOcclusionTexture", 4);
    m_Shader->SetInt("uEmissiveTexture", 5);
    m_Shader->SetInt("uEnvironmentMap", 6);
    m_Shader->SetInt("uBrdfLut", 7);

    m_SkyShader = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
    m_SkyShader->Use();
    m_SkyShader->SetInt("uEnvironmentMap", 0);
    m_FullscreenQuad = Mesh::CreateFullscreenQuad();

    const unsigned char whitePixel[] = {255, 255, 255, 255};
    const unsigned char normalPixel[] = {128, 128, 255, 255};
    const unsigned char blackPixel[] = {0, 0, 0, 255};
    m_DefaultBaseColorTexture.Allocate(
        1,
        1,
        GL_SRGB8_ALPHA8,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        whitePixel
    );
    m_DefaultMetallicRoughnessTexture.Allocate(
        1,
        1,
        GL_RGBA8,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        whitePixel
    );
    m_DefaultNormalTexture.Allocate(
        1,
        1,
        GL_RGBA8,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        normalPixel
    );
    m_DefaultOcclusionTexture.Allocate(
        1,
        1,
        GL_RGBA8,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        whitePixel
    );
    m_DefaultEmissiveTexture.Allocate(
        1,
        1,
        GL_SRGB8_ALPHA8,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        blackPixel
    );
    GenerateBrdfLut();
    m_EnvironmentMaxLod = std::floor(std::log2(static_cast<float>(std::max(kEnvironmentWidth, kEnvironmentHeight))));

    m_Initialized = true;
    Resize(width, height);
}

void ScenePass::Resize(int width, int height)
{
    m_Width = std::max(width, 1);
    m_Height = std::max(height, 1);
    AllocateTargets();
}

void ScenePass::Execute(
    const Scene& scene,
    const Camera& camera,
    const glm::mat4& lightSpaceMatrix,
    const Texture2D& shadowTexture,
    const RenderSettings& settings
)
{
    GenerateEnvironmentMap(scene);

    glViewport(0, 0, m_Width, m_Height);
    m_Framebuffer.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    m_SkyShader->Use();
    m_SkyShader->SetMat4(
        "uInverseViewProjection",
        glm::inverse(camera.GetProjectionMatrix() * camera.GetViewMatrix())
    );
    m_SkyShader->SetVec3("uCameraPosition", camera.GetPosition());
    m_SkyShader->SetFloat("uEnvironmentIntensity", settings.environmentIntensity);
    m_SkyShader->SetFloat("uEnvironmentRotationDegrees", scene.GetEnvironment().rotationDegrees);
    m_EnvironmentTexture.Bind(0);
    m_FullscreenQuad->Draw();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    m_Shader->Use();
    m_Shader->SetMat4("uView", camera.GetViewMatrix());
    m_Shader->SetMat4("uProjection", camera.GetProjectionMatrix());
    m_Shader->SetMat4("uLightSpaceMatrix", lightSpaceMatrix);
    m_Shader->SetVec3("uCameraPosition", camera.GetPosition());
    m_Shader->SetVec3("uDirectionalLight.direction", scene.GetDirectionalLight().direction);
    m_Shader->SetVec3("uDirectionalLight.color", scene.GetDirectionalLight().color);
    m_Shader->SetFloat("uDirectionalLight.intensity", scene.GetDirectionalLight().intensity);
    m_Shader->SetVec3("uPointLight.position", scene.GetPointLight().position);
    m_Shader->SetVec3("uPointLight.color", scene.GetPointLight().color);
    m_Shader->SetFloat("uPointLight.intensity", scene.GetPointLight().intensity);
    m_Shader->SetFloat("uPointLight.range", scene.GetPointLight().range);
    m_Shader->SetInt("uEnableShadows", settings.enableShadows ? 1 : 0);
    m_Shader->SetInt("uEnableIBL", settings.enableIBL ? 1 : 0);
    m_Shader->SetFloat("uEnvironmentIntensity", settings.environmentIntensity);
    m_Shader->SetFloat("uEnvironmentMaxLod", m_EnvironmentMaxLod);
    m_Shader->SetFloat("uEnvironmentRotationDegrees", scene.GetEnvironment().rotationDegrees);
    shadowTexture.Bind(0);
    m_EnvironmentTexture.Bind(6);
    m_BrdfLutTexture.Bind(7);

    for (const RenderObject& object : scene.GetObjects())
    {
        if (!object.mesh)
        {
            continue;
        }

        m_Shader->SetMat4("uModel", object.transform.GetMatrix());
        m_Shader->SetVec3("uMaterial.albedo", object.material.albedo);
        m_Shader->SetFloat("uMaterial.metallic", object.material.metallic);
        m_Shader->SetFloat("uMaterial.roughness", object.material.roughness);
        m_Shader->SetFloat("uMaterial.ao", object.material.ao);
        m_Shader->SetVec3("uMaterial.emissive", object.material.emissive);
        m_Shader->SetFloat("uMaterial.normalScale", object.material.normalScale);
        m_Shader->SetFloat("uMaterial.occlusionStrength", object.material.occlusionStrength);

        m_Shader->SetInt("uHasBaseColorTexture", object.material.baseColorTexture ? 1 : 0);
        m_Shader->SetInt(
            "uHasMetallicRoughnessTexture",
            object.material.metallicRoughnessTexture ? 1 : 0
        );
        m_Shader->SetInt("uHasNormalTexture", object.material.normalTexture ? 1 : 0);
        m_Shader->SetInt("uHasOcclusionTexture", object.material.occlusionTexture ? 1 : 0);
        m_Shader->SetInt("uHasEmissiveTexture", object.material.emissiveTexture ? 1 : 0);

        if (object.material.baseColorTexture)
        {
            object.material.baseColorTexture->Bind(1);
        }
        else
        {
            m_DefaultBaseColorTexture.Bind(1);
        }

        if (object.material.metallicRoughnessTexture)
        {
            object.material.metallicRoughnessTexture->Bind(2);
        }
        else
        {
            m_DefaultMetallicRoughnessTexture.Bind(2);
        }

        if (object.material.normalTexture)
        {
            object.material.normalTexture->Bind(3);
        }
        else
        {
            m_DefaultNormalTexture.Bind(3);
        }

        if (object.material.occlusionTexture)
        {
            object.material.occlusionTexture->Bind(4);
        }
        else
        {
            m_DefaultOcclusionTexture.Bind(4);
        }

        if (object.material.emissiveTexture)
        {
            object.material.emissiveTexture->Bind(5);
        }
        else
        {
            m_DefaultEmissiveTexture.Bind(5);
        }

        object.mesh->Draw();
    }

    Framebuffer::Unbind();
}

void ScenePass::GenerateEnvironmentMap(const Scene& scene)
{
    const SceneEnvironment& environment = scene.GetEnvironment();
    const EnvironmentImage* hdrImage = environment.hdrImage.get();
    const DirectionalLight& directionalLight = scene.GetDirectionalLight();
    const glm::vec3 normalizedDirection = glm::normalize(directionalLight.direction);
    const bool usingHdr = hdrImage && hdrImage->IsValid();
    const bool hdrUnchanged =
        usingHdr &&
        m_EnvironmentReady &&
        hdrImage == m_LastEnvironmentImage &&
        hdrImage->width == m_EnvironmentTexture.GetWidth() &&
        hdrImage->height == m_EnvironmentTexture.GetHeight();
    const bool lightUnchanged =
        !usingHdr &&
        m_EnvironmentReady &&
        m_LastEnvironmentImage == nullptr &&
        glm::length(normalizedDirection - m_LastEnvironmentLightDirection) < 1e-5f &&
        glm::length(directionalLight.color - m_LastEnvironmentLightColor) < 1e-5f &&
        glm::abs(directionalLight.intensity - m_LastEnvironmentLightIntensity) < 1e-5f;

    if (hdrUnchanged || lightUnchanged)
    {
        return;
    }

    if (usingHdr)
    {
        m_EnvironmentTexture.Allocate(
            hdrImage->width,
            hdrImage->height,
            GL_RGB16F,
            GL_RGB,
            GL_FLOAT,
            hdrImage->pixels.data(),
            GL_LINEAR_MIPMAP_LINEAR,
            GL_LINEAR,
            GL_REPEAT,
            GL_CLAMP_TO_EDGE
        );
        m_EnvironmentTexture.GenerateMipmaps();
        m_EnvironmentMaxLod = std::floor(std::log2(static_cast<float>(std::max(hdrImage->width, hdrImage->height))));
        m_LastEnvironmentImage = hdrImage;
        m_EnvironmentReady = true;
        return;
    }

    std::vector<glm::vec3> pixels(static_cast<std::size_t>(kEnvironmentWidth * kEnvironmentHeight));
    for (int y = 0; y < kEnvironmentHeight; ++y)
    {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kEnvironmentHeight);
        const float theta = v * glm::pi<float>();
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);

        for (int x = 0; x < kEnvironmentWidth; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kEnvironmentWidth);
            const float phi = (u - 0.5f) * glm::two_pi<float>();
            const glm::vec3 direction(
                std::cos(phi) * sinTheta,
                cosTheta,
                std::sin(phi) * sinTheta
            );
            pixels[static_cast<std::size_t>(y * kEnvironmentWidth + x)] =
                SampleProceduralEnvironment(directionalLight, direction);
        }
    }

    m_EnvironmentTexture.Allocate(
        kEnvironmentWidth,
        kEnvironmentHeight,
        GL_RGB16F,
        GL_RGB,
        GL_FLOAT,
        pixels.data(),
        GL_LINEAR_MIPMAP_LINEAR,
        GL_LINEAR,
        GL_REPEAT,
        GL_CLAMP_TO_EDGE
    );
    m_EnvironmentTexture.GenerateMipmaps();
    m_EnvironmentMaxLod = std::floor(std::log2(static_cast<float>(std::max(kEnvironmentWidth, kEnvironmentHeight))));
    m_LastEnvironmentImage = nullptr;
    m_LastEnvironmentLightDirection = normalizedDirection;
    m_LastEnvironmentLightColor = directionalLight.color;
    m_LastEnvironmentLightIntensity = directionalLight.intensity;
    m_EnvironmentReady = true;
}

void ScenePass::GenerateBrdfLut()
{
    std::vector<glm::vec2> pixels(static_cast<std::size_t>(kBrdfLutSize * kBrdfLutSize));
    for (int y = 0; y < kBrdfLutSize; ++y)
    {
        const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(kBrdfLutSize);
        for (int x = 0; x < kBrdfLutSize; ++x)
        {
            const float nDotV = (static_cast<float>(x) + 0.5f) / static_cast<float>(kBrdfLutSize);
            pixels[static_cast<std::size_t>(y * kBrdfLutSize + x)] =
                IntegrateBRDF(glm::clamp(nDotV, 0.001f, 0.999f), roughness);
        }
    }

    m_BrdfLutTexture.Allocate(
        kBrdfLutSize,
        kBrdfLutSize,
        GL_RG16F,
        GL_RG,
        GL_FLOAT,
        pixels.data()
    );
}

void ScenePass::AllocateTargets()
{
    m_SceneColorTexture.Allocate(m_Width, m_Height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_BrightTexture.Allocate(m_Width, m_Height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_AlbedoTexture.Allocate(m_Width, m_Height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_NormalTexture.Allocate(m_Width, m_Height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_MaterialTexture.Allocate(m_Width, m_Height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_DepthTexture.Allocate(
        m_Width,
        m_Height,
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr,
        GL_NEAREST,
        GL_NEAREST
    );

    m_Framebuffer.Bind();
    m_Framebuffer.AttachColorTexture(m_SceneColorTexture, 0);
    m_Framebuffer.AttachColorTexture(m_BrightTexture, 1);
    m_Framebuffer.AttachColorTexture(m_AlbedoTexture, 2);
    m_Framebuffer.AttachColorTexture(m_NormalTexture, 3);
    m_Framebuffer.AttachColorTexture(m_MaterialTexture, 4);
    m_Framebuffer.AttachDepthTexture(m_DepthTexture);
    m_Framebuffer.SetDrawBuffers(5);
    if (!m_Framebuffer.CheckComplete())
    {
        throw std::runtime_error("Scene framebuffer is incomplete.");
    }

    Framebuffer::Unbind();
}
