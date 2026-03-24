#pragma once

#include <glm/glm.hpp>

struct DirectionalLight
{
    glm::vec3 direction{-0.55f, -0.9f, -0.25f};
    glm::vec3 color{1.0f};
    float intensity = 3.5f;
};

struct PointLight
{
    glm::vec3 position{1.8f, 2.6f, 1.2f};
    glm::vec3 color{1.0f, 0.78f, 0.46f};
    float intensity = 60.0f;
    float range = 10.0f;
};
