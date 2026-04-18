// Material parameters shared by the realtime pass and the CPU reference tracer.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

class Texture2D;

enum class MaterialBlendMode : std::uint8_t
{
    Opaque = 0,
    AlphaBlend = 1,
    Additive = 2
};

enum class MaterialCullMode : std::uint8_t
{
    Back = 0,
    None = 1,
    Front = 2
};

constexpr bool UsesTransparentPass(MaterialBlendMode mode)
{
    return mode == MaterialBlendMode::AlphaBlend || mode == MaterialBlendMode::Additive;
}

struct ImageTexture
{
    int width = 0;
    int height = 0;
    int channels = 0;
    bool srgb = false;
    std::vector<unsigned char> pixels;

    // CPU-side bilinear sampling used by the offline reference renderer.
    glm::vec4 Sample(const glm::vec2& uv) const;
};

struct Material
{
    glm::vec3 albedo{1.0f};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f};
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    float opacity = 1.0f;
    MaterialBlendMode blendMode = MaterialBlendMode::Opaque;
    MaterialCullMode cullMode = MaterialCullMode::Back;
    bool castShadows = true;
    std::shared_ptr<Texture2D> baseColorTexture;
    std::shared_ptr<Texture2D> metallicRoughnessTexture;
    std::shared_ptr<Texture2D> normalTexture;
    std::shared_ptr<Texture2D> occlusionTexture;
    std::shared_ptr<Texture2D> emissiveTexture;
    std::shared_ptr<ImageTexture> baseColorTextureData;
    std::shared_ptr<ImageTexture> metallicRoughnessTextureData;
    std::shared_ptr<ImageTexture> normalTextureData;
    std::shared_ptr<ImageTexture> occlusionTextureData;
    std::shared_ptr<ImageTexture> emissiveTextureData;
};

class MaterialAsset
{
public:
    static std::shared_ptr<MaterialAsset> Create(Material material = {});

    explicit MaterialAsset(Material material);

    const Material& GetMaterial() const { return m_Material; }
    std::uint32_t GetStateId() const { return m_StateId; }

private:
    Material m_Material;
    std::uint32_t m_StateId = 0;
};

class MaterialInstance
{
public:
    static std::shared_ptr<MaterialInstance> Create(const std::shared_ptr<MaterialAsset>& asset);
    static std::shared_ptr<MaterialInstance> CreateLocal(Material material = {});

    const Material& GetMaterial() const { return m_Material; }
    Material& GetMutableMaterial() { return m_Material; }
    const std::shared_ptr<MaterialAsset>& GetAsset() const { return m_Asset; }
    std::uint32_t GetStateId() const { return m_StateId; }
    std::uint32_t GetAssetStateId() const;

private:
    MaterialInstance(std::shared_ptr<MaterialAsset> asset, Material material);

    std::shared_ptr<MaterialAsset> m_Asset;
    Material m_Material;
    std::uint32_t m_StateId = 0;
};
