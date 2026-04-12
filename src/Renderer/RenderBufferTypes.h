#pragma once

#include <glm/glm.hpp>

enum class BufferBindingSlot : unsigned int
{
    Frame = 0,
    Lighting = 1,
    Material = 2
};

struct alignas(16) FrameUniformData
{
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 lightSpaceMatrix{1.0f};
    glm::mat4 inverseViewProjection{1.0f};
    glm::vec4 cameraPosition{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 environmentData{1.0f, 0.0f, 0.0f, 0.0f};
};

struct alignas(16) LightingUniformData
{
    glm::vec4 directionalDirection{0.0f, -1.0f, 0.0f, 0.0f};
    glm::vec4 directionalColorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 pointPositionRange{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 pointColorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
};

struct alignas(16) MaterialUniformData
{
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissive{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 params0{0.0f, 0.5f, 1.0f, 1.0f};
    glm::vec4 params1{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 textureFlags0{0.0f};
    glm::vec4 textureFlags1{0.0f};
};
