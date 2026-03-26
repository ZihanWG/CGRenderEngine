#include "Renderer/EnvironmentLighting.h"

#include <algorithm>
#include <cmath>

#include "Scene/Light.h"

namespace
{
    float SmoothStep(float edge0, float edge1, float x)
    {
        const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
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
