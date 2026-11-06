#pragma once

#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

inline constexpr glm::vec3 WORLD_UP{0.0f, 1.0f, 0.0f};

class Camera {
public:
    Camera();

    glm::mat4 view() const;
    glm::mat4 proj() const;

    glm::vec3 fwrd() const;
    glm::vec3 right() const;

    void setAspectRatio(float newAspectRatio);

    void updatePosition(const glm::vec3& positionOffset);
    void updateOrientation(float dRadYaw, float dRadPitch);

private:
    void refreshView();
    void refreshProj();

    glm::vec3 right(const glm::vec3& fwrd) const;

    glm::mat4 m_view{1.0f};
    glm::mat4 m_proj{1.0f};

    glm::vec3 m_pos{0.0f, 0.0f, 10.0f};

    float m_pitch{0.0f};
    float m_yaw{glm::radians(270.0f)};

    float m_fov{glm::radians(45.0f)};
    float m_aspectRatio{16.0f / 9.0f};
    float m_near{1.0f};
    float m_far{100.0f};
};
