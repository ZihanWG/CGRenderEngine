// CPU texture sampling helpers for offline material evaluation.
#include "Engine/Scene/Material.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace
{
    std::uint32_t AcquireMaterialStateId()
    {
        static std::atomic<std::uint32_t> nextId{1u};
        return nextId.fetch_add(1u, std::memory_order_relaxed);
    }

    // Texture sampling wraps in UV space so the CPU path matches OpenGL repeat mode.
    int WrapIndex(int value, int size)
    {
        const int wrapped = value % size;
        return wrapped < 0 ? wrapped + size : wrapped;
    }

    float SrgbToLinear(float value)
    {
        if (value <= 0.04045f)
        {
            return value / 12.92f;
        }

        return std::pow((value + 0.055f) / 1.055f, 2.4f);
    }

    glm::vec4 LoadTexel(const ImageTexture& texture, int x, int y)
    {
        const int wrappedX = WrapIndex(x, texture.width);
        const int wrappedY = WrapIndex(y, texture.height);
        const std::size_t texelIndex = static_cast<std::size_t>((wrappedY * texture.width + wrappedX) * texture.channels);

        glm::vec4 texel(0.0f, 0.0f, 0.0f, 1.0f);
        for (int channel = 0; channel < texture.channels && channel < 4; ++channel)
        {
            float value = static_cast<float>(texture.pixels[texelIndex + channel]) / 255.0f;
            if (texture.srgb && channel < 3)
            {
                value = SrgbToLinear(value);
            }

            texel[channel] = value;
        }

        return texel;
    }
}

glm::vec4 ImageTexture::Sample(const glm::vec2& uv) const
{
    // Match GPU bilinear filtering closely enough for realtime/reference comparisons.
    if (width <= 0 || height <= 0 || channels <= 0 || pixels.empty())
    {
        return glm::vec4(1.0f);
    }

    const float wrappedU = uv.x - std::floor(uv.x);
    const float wrappedV = uv.y - std::floor(uv.y);

    const float x = wrappedU * static_cast<float>(width) - 0.5f;
    const float y = wrappedV * static_cast<float>(height) - 0.5f;

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = x - std::floor(x);
    const float ty = y - std::floor(y);

    const glm::vec4 c00 = LoadTexel(*this, x0, y0);
    const glm::vec4 c10 = LoadTexel(*this, x1, y0);
    const glm::vec4 c01 = LoadTexel(*this, x0, y1);
    const glm::vec4 c11 = LoadTexel(*this, x1, y1);

    const glm::vec4 cx0 = glm::mix(c00, c10, tx);
    const glm::vec4 cx1 = glm::mix(c01, c11, tx);
    return glm::mix(cx0, cx1, ty);
}

std::shared_ptr<MaterialAsset> MaterialAsset::Create(Material material)
{
    return std::make_shared<MaterialAsset>(std::move(material));
}

MaterialAsset::MaterialAsset(Material material)
    : m_Material(std::move(material)),
      m_StateId(AcquireMaterialStateId())
{
}

std::shared_ptr<MaterialInstance> MaterialInstance::Create(const std::shared_ptr<MaterialAsset>& asset)
{
    if (!asset)
    {
        return CreateLocal();
    }

    return std::shared_ptr<MaterialInstance>(
        new MaterialInstance(asset, asset->GetMaterial())
    );
}

std::shared_ptr<MaterialInstance> MaterialInstance::CreateLocal(Material material)
{
    return std::shared_ptr<MaterialInstance>(
        new MaterialInstance(nullptr, std::move(material))
    );
}

std::uint32_t MaterialInstance::GetAssetStateId() const
{
    return m_Asset ? m_Asset->GetStateId() : m_StateId;
}

MaterialInstance::MaterialInstance(std::shared_ptr<MaterialAsset> asset, Material material)
    : m_Asset(std::move(asset)),
      m_Material(std::move(material)),
      m_StateId(AcquireMaterialStateId())
{
}
