// Shared uniform buffer layouts used by multiple shaders and the material binder.
//
// These structs are the CPU/GPU contract for the engine's UBOs. If a shader reads one of
// these blocks, its field order and packing must stay aligned with the definitions below.
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
    // uEnvironmentData = (intensity, maxMipLod, rotationDegrees, unused).
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::mat4 lightSpaceMatrix{1.0f};
    glm::mat4 inverseViewProjection{1.0f};
    glm::vec4 cameraPosition{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 environmentData{1.0f, 0.0f, 0.0f, 0.0f};
};

struct alignas(16) LightingUniformData
{
    // vec4 packing keeps std140 layout predictable across drivers.
    glm::vec4 directionalDirection{0.0f, -1.0f, 0.0f, 0.0f};
    glm::vec4 directionalColorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 pointPositionRange{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 pointColorIntensity{1.0f, 1.0f, 1.0f, 1.0f};
};

struct alignas(16) MaterialUniformData
{
    // params0 = (metallic, roughness, ao, normalScale).
    // params1 = (occlusionStrength, reserved...).
    glm::vec4 albedo{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissive{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 params0{0.0f, 0.5f, 1.0f, 1.0f};
    glm::vec4 params1{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec4 textureFlags0{0.0f};
    glm::vec4 textureFlags1{0.0f};
};
