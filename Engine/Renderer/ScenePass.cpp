// Builds the realtime scene color and debug outputs, including environment setup and UBO uploads.
//
// Responsibilities of this pass:
// - upload frame/light uniform blocks
// - ensure the environment texture and BRDF LUT are ready
// - render the sky background
// - render all submitted draw items with the forward PBR shader
// - write extra MRT targets for debugging and post-processing
//
// It is effectively the "main scene shading" stage of the engine.
#include "Engine/Renderer/ScenePass.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <glad/glad.h>

#include <glm/gtc/constants.hpp>

#include "Engine/Assets/EnvironmentLighting.h"
#include "Engine/Assets/ResourceManager.h"
#include "Engine/RHI/Mesh.h"
#include "Engine/RHI/RenderBufferTypes.h"
#include "Engine/Renderer/RenderSettings.h"
#include "Engine/Renderer/RenderSubmission.h"
#include "Engine/Renderer/RenderWorld.h"
#include "Engine/Scene/Material.h"

namespace
{
    constexpr int kEnvironmentWidth = 512;
    constexpr int kEnvironmentHeight = 256;
    constexpr int kBrdfLutSize = 128;
    constexpr unsigned int kBrdfSampleCount = 64u;
    constexpr float kPi = 3.14159265359f;

    void ApplyCullMode(MaterialCullMode cullMode)
    {
        switch (cullMode)
        {
        case MaterialCullMode::None:
            glDisable(GL_CULL_FACE);
            break;
        case MaterialCullMode::Front:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            break;
        case MaterialCullMode::Back:
        default:
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            break;
        }
    }

    void ApplyBlendMode(MaterialBlendMode blendMode)
    {
        switch (blendMode)
        {
        case MaterialBlendMode::AlphaBlend:
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            break;
        case MaterialBlendMode::Additive:
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glDepthMask(GL_FALSE);
            break;
        case MaterialBlendMode::Opaque:
        default:
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            break;
        }
    }

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
        // Importance sample the GGX distribution for the BRDF split-sum integration.
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
        // Offline integrate the specular BRDF response into a small lookup texture.
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

void ScenePass::Initialize(
    ResourceManager& resourceManager,
    ShaderBufferManager& bufferManager,
    int width,
    int height
)
{
    if (m_Initialized)
    {
        Resize(width, height);
        return;
    }

    m_BufferManager = &bufferManager;
    // Shader setup is centralized here so Execute can stay focused on per-frame work.
    m_Shader = resourceManager.LoadShader("Shaders/pbr.vert", "Shaders/pbr.frag");
    m_Shader->Use();
    m_Shader->SetInt("uShadowMap", 0);
    m_Shader->SetInt("uBaseColorTexture", 1);
    m_Shader->SetInt("uMetallicRoughnessTexture", 2);
    m_Shader->SetInt("uNormalTexture", 3);
    m_Shader->SetInt("uOcclusionTexture", 4);
    m_Shader->SetInt("uEmissiveTexture", 5);
    m_Shader->SetInt("uEnvironmentMap", 6);
    m_Shader->SetInt("uBrdfLut", 7);
    m_Shader->SetUniformBlockBinding("FrameData", bufferManager.GetBindingPoint(BufferBindingSlot::Frame));
    m_Shader->SetUniformBlockBinding("LightingData", bufferManager.GetBindingPoint(BufferBindingSlot::Lighting));
    m_Shader->SetUniformBlockBinding("MaterialData", bufferManager.GetBindingPoint(BufferBindingSlot::Material));
    m_Shader->SetUniformBlockBinding("ObjectData", bufferManager.GetBindingPoint(BufferBindingSlot::Object));
    m_BufferManager->InitializeUniformRingBuffer(BufferBindingSlot::Object, sizeof(ObjectUniformData), 1024);

    m_SkyShader = resourceManager.LoadShader("Shaders/sky.vert", "Shaders/sky.frag");
    m_SkyShader->Use();
    m_SkyShader->SetInt("uEnvironmentMap", 0);
    m_SkyShader->SetUniformBlockBinding("FrameData", bufferManager.GetBindingPoint(BufferBindingSlot::Frame));
    m_FullscreenQuad = resourceManager.GetFullscreenQuad();
    bufferManager.InitializeUniformBuffer(BufferBindingSlot::Frame, sizeof(FrameUniformData));
    bufferManager.InitializeUniformBuffer(BufferBindingSlot::Lighting, sizeof(LightingUniformData));
    m_MaterialBinder.Initialize(bufferManager);
    // The BRDF LUT and default environment state are viewport-independent.
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
    const RenderWorld& renderWorld,
    const RenderSubmission& submission,
    const Texture2D& shadowTexture,
    const RenderSettings& settings
)
{
    // Environment generation is cached internally and only refreshes when the scene source changes.
    GenerateEnvironmentMap(renderWorld);

    // Frame UBO = camera, matrix, and environment parameters shared by multiple shaders.
    // Lighting UBO = analytic lights shared by the scene shader.
    const SceneEnvironment* environment = renderWorld.renderScene.environment;
    const float environmentRotation = environment ? environment->rotationDegrees : 0.0f;
    const PointLight pointLight = !renderWorld.visibleLights.pointLights.empty()
        ? renderWorld.visibleLights.pointLights.front()
        : PointLight{};
    const FrameUniformData frameData{
        renderWorld.viewInfo.viewMatrix,
        renderWorld.viewInfo.projectionMatrix,
        renderWorld.perViewData.lightSpaceMatrix,
        renderWorld.viewInfo.inverseViewProjection,
        glm::vec4(renderWorld.viewInfo.position, 1.0f),
        glm::vec4(settings.environmentIntensity, m_EnvironmentMaxLod, environmentRotation, 0.0f)
    };
    const LightingUniformData lightingData{
        glm::vec4(renderWorld.visibleLights.directionalLight.direction, 0.0f),
        glm::vec4(
            renderWorld.visibleLights.directionalLight.color,
            renderWorld.visibleLights.directionalLight.intensity
        ),
        glm::vec4(
            pointLight.position,
            pointLight.range
        ),
        glm::vec4(
            pointLight.color,
            pointLight.intensity
        )
    };

    m_BufferManager->UploadUniform(BufferBindingSlot::Frame, frameData);
    m_BufferManager->UploadUniform(BufferBindingSlot::Lighting, lightingData);
    m_BufferManager->Bind(BufferBindingSlot::Frame);
    m_BufferManager->Bind(BufferBindingSlot::Lighting);

    glViewport(0, 0, m_Width, m_Height);
    m_Framebuffer.Bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Pass layout:
    // 1. background sky writes into the same MRT set as scene geometry
    // 2. opaque geometry is then shaded on top using depth testing
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    // Render the sky first so the forward pass can simply depth-test on top of it.
    m_SkyShader->Use();
    m_EnvironmentTexture.Bind(0);
    m_FullscreenQuad->Draw();

    glEnable(GL_DEPTH_TEST);
    ApplyCullMode(MaterialCullMode::Back);
    ApplyBlendMode(MaterialBlendMode::Opaque);

    m_Shader->Use();
    m_Shader->SetInt("uEnableShadows", settings.enableShadows ? 1 : 0);
    m_Shader->SetInt("uEnableIBL", settings.enableIBL ? 1 : 0);
    shadowTexture.Bind(0);
    m_EnvironmentTexture.Bind(6);
    m_BrdfLutTexture.Bind(7);

    const auto submitBatches = [&](const std::vector<InstancedDrawBatch>& batches) {
        for (const InstancedDrawBatch& batch : batches)
        {
            if (!batch.mesh || !batch.material)
            {
                continue;
            }

            for (std::size_t startIndex = 0; startIndex < batch.perObjectDataIndices.size(); startIndex += kMaxObjectMatricesPerDraw)
            {
                ObjectUniformData objectData{};
                const std::size_t remainingCount = batch.perObjectDataIndices.size() - startIndex;
                const std::size_t instanceCount = std::min<std::size_t>(remainingCount, kMaxObjectMatricesPerDraw);
                for (std::size_t localIndex = 0; localIndex < instanceCount; ++localIndex)
                {
                    const std::size_t perObjectDataIndex = batch.perObjectDataIndices[startIndex + localIndex];
                    if (perObjectDataIndex >= renderWorld.perObjectData.size())
                    {
                        continue;
                    }

                    objectData.modelMatrices[localIndex] = renderWorld.perObjectData[perObjectDataIndex].modelMatrix;
                }

                const BufferSlice objectSlice = m_BufferManager->UploadUniformRing(BufferBindingSlot::Object, objectData);
                m_BufferManager->BindRange(BufferBindingSlot::Object, objectSlice.offset, objectSlice.size);

                ApplyCullMode(batch.material->cullMode);
                ApplyBlendMode(batch.material->blendMode);
                // MaterialBinder owns the uniform block upload and texture fallback policy.
                m_MaterialBinder.Bind(*batch.material);
                batch.mesh->DrawInstanced(instanceCount);
            }
        }
    };

    submitBatches(submission.renderQueue.opaqueBatches);

    submitBatches(submission.renderQueue.transparentBatches);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    ApplyCullMode(MaterialCullMode::Back);
    Framebuffer::Unbind();
}

void ScenePass::GenerateEnvironmentMap(const RenderWorld& renderWorld)
{
    const SceneEnvironment* environment = renderWorld.renderScene.environment;
    const SceneEnvironment emptyEnvironment;
    const SceneEnvironment& environmentState = environment ? *environment : emptyEnvironment;
    const EnvironmentImage* hdrImage = environmentState.hdrImage.get();
    const DirectionalLight& directionalLight = renderWorld.visibleLights.directionalLight;
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
        // Avoid re-uploading or regenerating the environment unless the source really changed.
        return;
    }

    if (usingHdr)
    {
        // HDR equirect textures are uploaded directly and sampled as a lat-long map in shaders.
        // We rely on the generated mip chain as a cheap specular prefilter approximation.
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
    // Procedural fallback is baked into a 2D lat-long texture so the shader path stays identical.
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
    // The LUT is generated on the CPU once to avoid a startup dependency on compute shaders.
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
    // Keep explicit textures for debug views instead of hiding data in transient attachments.
    // This costs more VRAM than the absolute minimum, but makes the project much easier to
    // inspect and extend because every intermediate the user cares about has a named texture.
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
