// Lightweight transform that supports both authored TRS and a full matrix override.
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

    // Returns either the override matrix or the TRS-composed model matrix.
    glm::mat4 GetMatrix() const;
};
