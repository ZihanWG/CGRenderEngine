#pragma once

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

struct DirectionalLight;
struct SceneEnvironment;

struct EnvironmentImage
{
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<float> pixels;

    bool IsValid() const;
    glm::vec3 Sample(const glm::vec3& direction, float rotationDegrees = 0.0f) const;
};

std::shared_ptr<EnvironmentImage> LoadHdrEnvironment(
    const std::string& path,
    std::string* errorMessage = nullptr
);

glm::vec3 SampleProceduralEnvironment(
    const DirectionalLight& directionalLight,
    const glm::vec3& direction
);

glm::vec3 SampleSceneEnvironment(
    const SceneEnvironment& environment,
    const DirectionalLight& directionalLight,
    const glm::vec3& direction
);
