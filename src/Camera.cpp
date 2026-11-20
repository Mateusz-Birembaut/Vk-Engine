#include "VkEngine/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace vke
{

Camera::Camera()
{
    refreshView();
    refreshProj();
}

glm::mat4 Camera::view() const
{
    return m_view;
}

glm::mat4 Camera::proj() const
{
    return m_proj;
}

glm::vec3 Camera::pos() const
{
    return m_pos;
}

glm::vec3 Camera::fwd() const
{
    const float x = std::cos(m_pitch) * std::cos(m_yaw);
    const float y = std::sin(m_pitch);
    const float z = std::cos(m_pitch) * std::sin(m_yaw);

    return glm::vec3{x, y, z};
}

glm::vec3 Camera::right() const
{
    const glm::vec3 fwdVec = fwd();
    return right(fwdVec);
}

glm::vec3 Camera::right(const glm::vec3& fwd) const
{
    return glm::normalize(glm::cross(fwd, WORLD_UP));
}

void Camera::setAspectRatio(float newAspectRatio)
{
    if (!std::isfinite(newAspectRatio) || newAspectRatio <= 0.0f)
        return;

    m_aspectRatio = newAspectRatio;
    refreshProj();
}

void Camera::updatePosition(const glm::vec3& positionOffset)
{
    m_pos += positionOffset;
    refreshView();
}

void Camera::refreshView()
{
    const glm::vec3 fwdVec = fwd();
    const glm::vec3 rightVec = right(fwdVec);
    const glm::vec3 upVec = glm::normalize(glm::cross(rightVec, fwdVec));

    m_view = glm::lookAt(m_pos, m_pos + fwdVec, upVec);
}

void Camera::refreshProj()
{
    m_proj = glm::perspective(m_fov, m_aspectRatio, m_near, m_far);
}

void Camera::updateOrientation(float dYaw, float dPitch)
{
    static constexpr float twoPi = 2.0f * static_cast<float>(std::numbers::pi);
    // small offset to prevent cam fwd to be colinear with world up
    static constexpr float halfPi = (static_cast<float>(std::numbers::pi) / 2.0F) - 0.01f;

    // -pi/2 < pitch < pi/2
    m_pitch = std::clamp(m_pitch - dPitch, -halfPi, halfPi);

    // 0 < yaw < 2 pi
    m_yaw = std::fmod(m_yaw + dYaw, twoPi);
    if (m_yaw < 0.0f)
    {
        m_yaw += twoPi;
    }

    refreshView();
}

} // namespace vke