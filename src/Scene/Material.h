// Material parameters shared by the realtime pass and the CPU reference tracer.
#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

class Texture2D;

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
