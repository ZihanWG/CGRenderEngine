#pragma once

#include <string>

#include <glm/glm.hpp>

class Scene;

class GLTFLoader
{
public:
    bool LoadModelIntoScene(
        const std::string& path,
        Scene& scene,
        const glm::mat4& rootTransform = glm::mat4(1.0f),
        std::string* errorMessage = nullptr
    ) const;
};
