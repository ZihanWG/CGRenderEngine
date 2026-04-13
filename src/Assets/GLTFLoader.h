// Imports a subset of glTF into the engine's Scene/Material/Mesh structures.
#pragma once

#include <string>

#include <glm/glm.hpp>

class Scene;

class GLTFLoader
{
public:
    // Appends the loaded model to an existing scene under an optional root transform.
    bool LoadModelIntoScene(
        const std::string& path,
        Scene& scene,
        const glm::mat4& rootTransform = glm::mat4(1.0f),
        std::string* errorMessage = nullptr
    ) const;
};
