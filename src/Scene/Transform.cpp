// Converts scene transform state into model matrices used by submission and ray tracing.
#include "Scene/Transform.h"

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Transform::GetMatrix() const
{
    if (useMatrixOverride)
    {
        // glTF imports can preserve the exact authored matrix instead of decomposing it.
        return matrixOverride;
    }

    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}
