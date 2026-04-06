#include "Renderer/EnvironmentLighting.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "stb_image.h"
#include "Scene/Environment.h"
#include "Scene/Light.h"

namespace
{
    constexpr float kPi = 3.14159265359f;

    float SmoothStep(float edge0, float edge1, float x)
    {
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    std::string ResolveAssetPath(const std::string& path)
    {
        const std::filesystem::path inputPath(path);
        if (inputPath.is_absolute())
        {
            return inputPath.string();
        }

        const std::filesystem::path root(CGENGINE_ASSET_ROOT);
        return (root / inputPath).string();
    }

    glm::vec2 DirectionToLatLong(const glm::vec3& direction, float rotationDegrees)
    {
        const float rotationRadians = glm::radians(rotationDegrees);
        const float cosRotation = std::cos(rotationRadians);
        const float sinRotation = std::sin(rotationRadians);
        const glm::vec3 dir = glm::normalize(glm::vec3(
            direction.x * cosRotation - direction.z * sinRotation,
            direction.y,
            direction.x * sinRotation + direction.z * cosRotation
        ));

        const float phi = std::atan2(dir.z, dir.x);
        const float theta = std::acos(glm::clamp(dir.y, -1.0f, 1.0f));
        return glm::vec2(phi / (2.0f * kPi) + 0.5f, theta / kPi);
    }

    glm::vec3 LoadHdrPixel(const EnvironmentImage& image, int x, int y)
    {
        const int wrappedX = ((x % image.width) + image.width) % image.width;
        const int clampedY = std::clamp(y, 0, image.height - 1);
        const std::size_t texelIndex =
            static_cast<std::size_t>((clampedY * image.width + wrappedX) * image.channels);

        glm::vec3 texel(0.0f);
        for (int channel = 0; channel < 3 && channel < image.channels; ++channel)
        {
            texel[channel] = image.pixels[texelIndex + channel];
        }

        return glm::max(texel, glm::vec3(0.0f));
    }
}

bool EnvironmentImage::IsValid() const
{
    return width > 0 && height > 0 && channels >= 3 && !pixels.empty();
}

glm::vec3 EnvironmentImage::Sample(const glm::vec3& direction, float rotationDegrees) const
{
    if (!IsValid())
    {
        return glm::vec3(0.0f);
    }

    const glm::vec2 uv = DirectionToLatLong(direction, rotationDegrees);
    const float x = uv.x * static_cast<float>(width) - 0.5f;
    const float y = uv.y * static_cast<float>(height) - 0.5f;

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = x - std::floor(x);
    const float ty = y - std::floor(y);

    const glm::vec3 c00 = LoadHdrPixel(*this, x0, y0);
    const glm::vec3 c10 = LoadHdrPixel(*this, x1, y0);
    const glm::vec3 c01 = LoadHdrPixel(*this, x0, y1);
    const glm::vec3 c11 = LoadHdrPixel(*this, x1, y1);

    return glm::mix(
        glm::mix(c00, c10, tx),
        glm::mix(c01, c11, tx),
        ty
    );
}

std::shared_ptr<EnvironmentImage> LoadHdrEnvironment(
    const std::string& path,
    std::string* errorMessage
)
{
    const std::string resolvedPath = ResolveAssetPath(path);
    int width = 0;
    int height = 0;
    int channels = 0;
    float* data = stbi_loadf(resolvedPath.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        if (errorMessage)
        {
            *errorMessage = "Failed to load HDR image: " + resolvedPath;
        }
        return nullptr;
    }

    auto image = std::make_shared<EnvironmentImage>();
    image->width = width;
    image->height = height;
    image->channels = channels;
    image->pixels.assign(
        data,
        data + static_cast<std::size_t>(width * height * channels)
    );
    stbi_image_free(data);
    return image;
}

glm::vec3 SampleProceduralEnvironment(
    const DirectionalLight& directionalLight,
    const glm::vec3& direction
)
{
    const glm::vec3 dir = glm::normalize(direction);
    const glm::vec3 skyZenith(0.11f, 0.22f, 0.46f);
    const glm::vec3 skyHorizon(0.72f, 0.81f, 0.94f);
    const glm::vec3 groundColor(0.05f, 0.055f, 0.065f);

    const float upFactor = SmoothStep(-0.18f, 0.85f, dir.y);
    const float zenithFactor = SmoothStep(0.05f, 1.0f, dir.y);

    glm::vec3 environment = dir.y >= 0.0f
        ? glm::mix(skyHorizon, skyZenith, zenithFactor)
        : glm::mix(groundColor, skyHorizon * 0.18f, SmoothStep(-0.4f, 0.0f, dir.y));

    const glm::vec3 sunDirection = glm::normalize(-directionalLight.direction);
    const float sunDot = glm::max(glm::dot(dir, sunDirection), 0.0f);
    const float lightScale = glm::max(directionalLight.intensity, 0.0f);
    const glm::vec3 sunRadiance = directionalLight.color * lightScale;

    const float sunHalo = std::pow(sunDot, 20.0f) * 0.035f;
    const float sunGlow = std::pow(sunDot, 96.0f) * 0.18f;
    const float sunDisk = std::pow(sunDot, 2048.0f) * 2.5f;

    environment += sunRadiance * (sunHalo + sunGlow + sunDisk);
    environment += sunRadiance * (0.025f * upFactor);
    return glm::max(environment, glm::vec3(0.0f));
}

glm::vec3 SampleSceneEnvironment(
    const SceneEnvironment& environment,
    const DirectionalLight& directionalLight,
    const glm::vec3& direction
)
{
    if (environment.hdrImage && environment.hdrImage->IsValid())
    {
        return environment.hdrImage->Sample(direction, environment.rotationDegrees);
    }

    return SampleProceduralEnvironment(directionalLight, direction);
}
