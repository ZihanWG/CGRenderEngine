#include "Scene/Camera.h"

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
}

void Camera::SetAspectRatio(float aspectRatio)
{
    m_AspectRatio = aspectRatio;
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
    const float tanHalfFov = glm::tan(glm::radians(m_FovDegrees) * 0.5f);
    const float px = (2.0f * u - 1.0f) * m_AspectRatio * tanHalfFov;
    const float py = (2.0f * v - 1.0f) * tanHalfFov;
    return glm::normalize(GetForward() + px * GetRight() + py * GetUp());
}
