#pragma once

#include <numbers>

#include "VkEngine/Camera.h"
#include "VkEngine/Types.h"

class CameraController {
public:
    vke::RenderView renderView() const;

    void updateAspectRatio(float newAspectRatio);
    void handleMouseMotion(float xRel, float yRel);
    void handleKeyboard(float dt);

private:
    Camera m_camera{};

    float m_speed{5.0f};
    float m_sensitivity{2 * static_cast<float>(std::numbers::pi) / 5000.0f};
};
