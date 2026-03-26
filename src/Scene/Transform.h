#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Transform
{
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    glm::vec3 scale{1.0f};
    bool useMatrixOverride = false;
    glm::mat4 matrixOverride{1.0f};

    glm::mat4 GetMatrix() const;
};
