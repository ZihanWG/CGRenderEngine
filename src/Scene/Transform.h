#pragma once

#include <glm/glm.hpp>

struct Transform
{
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};

    glm::mat4 GetMatrix() const;
};
