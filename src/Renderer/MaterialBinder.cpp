// Encodes material state into the shared material UBO and stable texture bindings.
//
// The binder exists so passes do not need to know:
// - which texture units are reserved for which material maps
// - what the fallback textures are when a slot is missing
// - how scalar material parameters are packed into the uniform block
//
// In a larger engine this would usually expand into descriptor/state caching.
#include "Renderer/MaterialBinder.h"

#include "Scene/Material.h"

void MaterialBinder::Initialize(ShaderBufferManager& bufferManager)
{
    m_BufferManager = &bufferManager;
    m_BufferManager->InitializeUniformBuffer(BufferBindingSlot::Material, sizeof(MaterialUniformData));

    // Small fallback textures let shaders stay branch-light even when a material slot is absent.
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
}

void MaterialBinder::Bind(const Material& material) const
{
    // Keep texture presence as flags in the UBO so shaders know whether to sample each slot.
    // Binding policy is intentionally stable:
    // slot 1 = baseColor
    // slot 2 = metallicRoughness
    // slot 3 = normal
    // slot 4 = occlusion
    // slot 5 = emissive
    //
    // Because those bindings never change, the shader setup path is simple and the
    // ScenePass can bind a material with one call instead of many scattered calls.
    MaterialUniformData materialData;
    materialData.albedo = glm::vec4(material.albedo, 1.0f);
    materialData.emissive = glm::vec4(material.emissive, 1.0f);
    materialData.params0 = glm::vec4(
        material.metallic,
        material.roughness,
        material.ao,
        material.normalScale
    );
    materialData.params1 = glm::vec4(material.occlusionStrength, 0.0f, 0.0f, 0.0f);
    materialData.textureFlags0 = glm::vec4(
        material.baseColorTexture ? 1.0f : 0.0f,
        material.metallicRoughnessTexture ? 1.0f : 0.0f,
        material.normalTexture ? 1.0f : 0.0f,
        material.occlusionTexture ? 1.0f : 0.0f
    );
    materialData.textureFlags1 = glm::vec4(
        material.emissiveTexture ? 1.0f : 0.0f,
        0.0f,
        0.0f,
        0.0f
    );

    m_BufferManager->UploadUniform(BufferBindingSlot::Material, materialData);
    m_BufferManager->Bind(BufferBindingSlot::Material);

    if (material.baseColorTexture)
    {
        material.baseColorTexture->Bind(1);
    }
    else
    {
        m_DefaultBaseColorTexture.Bind(1);
    }

    if (material.metallicRoughnessTexture)
    {
        material.metallicRoughnessTexture->Bind(2);
    }
    else
    {
        m_DefaultMetallicRoughnessTexture.Bind(2);
    }

    if (material.normalTexture)
    {
        material.normalTexture->Bind(3);
    }
    else
    {
        m_DefaultNormalTexture.Bind(3);
    }

    if (material.occlusionTexture)
    {
        material.occlusionTexture->Bind(4);
    }
    else
    {
        m_DefaultOcclusionTexture.Bind(4);
    }

    if (material.emissiveTexture)
    {
        material.emissiveTexture->Bind(5);
    }
    else
    {
        m_DefaultEmissiveTexture.Bind(5);
    }
}
