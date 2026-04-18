// Camera math for view/projection matrices, fly movement, and ray generation.
#include "Engine/Scene/Camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(
    const glm::vec3& position,
    const glm::vec3& target,
    float fovDegrees,
    float aspectRatio,
    float nearClip,
    float farClip
)
    : m_Position(position),
      m_Target(target),
      m_WorldUp(0.0f, 1.0f, 0.0f),
      m_FovDegrees(fovDegrees),
      m_AspectRatio(aspectRatio),
      m_NearClip(nearClip),
      m_FarClip(farClip)
{
    // Seed yaw/pitch from the initial look-at target so mouse look starts aligned.
    const glm::vec3 forward = glm::normalize(m_Target - m_Position);
    m_YawDegrees = glm::degrees(glm::atan(forward.z, forward.x));
    m_PitchDegrees = glm::degrees(glm::asin(glm::clamp(forward.y, -1.0f, 1.0f)));
    UpdateTarget();
}

void Camera::SetAspectRatio(float aspectRatio)
{
    m_AspectRatio = aspectRatio;
}

void Camera::MoveForward(float distance)
{
    const glm::vec3 offset = GetForward() * distance;
    m_Position += offset;
    m_Target += offset;
}

void Camera::MoveRight(float distance)
{
    const glm::vec3 offset = GetRight() * distance;
    m_Position += offset;
    m_Target += offset;
}

void Camera::MoveUp(float distance)
{
    const glm::vec3 offset = m_WorldUp * distance;
    m_Position += offset;
    m_Target += offset;
}

void Camera::Rotate(float yawOffsetDegrees, float pitchOffsetDegrees)
{
    m_YawDegrees += yawOffsetDegrees;
    m_PitchDegrees = glm::clamp(m_PitchDegrees + pitchOffsetDegrees, -89.0f, 89.0f);
    UpdateTarget();
}

glm::mat4 Camera::GetViewMatrix() const
{
    return glm::lookAt(m_Position, m_Target, m_WorldUp);
}

glm::mat4 Camera::GetProjectionMatrix() const
{
    return glm::perspective(
        glm::radians(m_FovDegrees),
        m_AspectRatio,
        m_NearClip,
        m_FarClip
    );
}

glm::vec3 Camera::GetForward() const
{
    return glm::normalize(m_Target - m_Position);
}

glm::vec3 Camera::GetRight() const
{
    return glm::normalize(glm::cross(GetForward(), m_WorldUp));
}

glm::vec3 Camera::GetUp() const
{
    return glm::normalize(glm::cross(GetRight(), GetForward()));
}

glm::vec3 Camera::GenerateRayDirection(float u, float v) const
{
    // Reconstruct the direction from the camera basis instead of inverting a matrix per ray.
    const float tanHalfFov = glm::tan(glm::radians(m_FovDegrees) * 0.5f);
    const float px = (2.0f * u - 1.0f) * m_AspectRatio * tanHalfFov;
    const float py = (2.0f * v - 1.0f) * tanHalfFov;
    return glm::normalize(GetForward() + px * GetRight() + py * GetUp());
}

void Camera::UpdateTarget()
{
    const float yawRadians = glm::radians(m_YawDegrees);
    const float pitchRadians = glm::radians(m_PitchDegrees);

    const glm::vec3 forward(
        glm::cos(yawRadians) * glm::cos(pitchRadians),
        glm::sin(pitchRadians),
        glm::sin(yawRadians) * glm::cos(pitchRadians)
    );

    m_Target = m_Position + glm::normalize(forward);
}
