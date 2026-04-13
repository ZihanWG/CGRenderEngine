// Simple fly camera shared by the realtime renderer and CPU ray tracer.
#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
    Camera(
        const glm::vec3& position,
        const glm::vec3& target,
        float fovDegrees,
        float aspectRatio,
        float nearClip,
        float farClip
    );

    void SetAspectRatio(float aspectRatio);
    void MoveForward(float distance);
    void MoveRight(float distance);
    void MoveUp(float distance);
    void Rotate(float yawOffsetDegrees, float pitchOffsetDegrees);

    const glm::vec3& GetPosition() const { return m_Position; }
    const glm::vec3& GetTarget() const { return m_Target; }

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;
    // Generate a world-space primary ray for normalized viewport coordinates.
    glm::vec3 GenerateRayDirection(float u, float v) const;

private:
    glm::vec3 m_Position;
    glm::vec3 m_Target;
    glm::vec3 m_WorldUp;
    float m_FovDegrees = 45.0f;
    float m_AspectRatio = 1.0f;
    float m_NearClip = 0.1f;
    float m_FarClip = 100.0f;
    float m_YawDegrees = -90.0f;
    float m_PitchDegrees = 0.0f;

    // Rebuilds the target vector from yaw/pitch after mouse look changes.
    void UpdateTarget();
};
