#pragma once

#include <glm/glm.hpp>

struct DirectionalLight;

glm::vec3 SampleProceduralEnvironment(
    const DirectionalLight& directionalLight,
    const glm::vec3& direction
);
