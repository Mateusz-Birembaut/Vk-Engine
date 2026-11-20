#include "VkEngine/CameraController.h"

#include <glm/vec3.hpp>
#include <SDL3/SDL_keyboard.h>

namespace vke
{

vke::RenderView CameraController::renderView() const
{
    return {m_camera.view(), m_camera.proj(), m_camera.pos()};
}

void CameraController::updateAspectRatio(float newAspectRatio)
{
    m_camera.setAspectRatio(newAspectRatio);
}

void CameraController::handleMouseMotion(float xRel, float yRel)
{
    float radX = m_sensitivity * xRel;
    float radY = m_sensitivity * yRel;

    m_camera.updateOrientation(radX, radY);
}

void CameraController::handleKeyboard(float dt)
{
    const bool* keyStates = SDL_GetKeyboardState(nullptr);

    float xDir = 0.0f;
    float zDir = 0.0f;

    if (keyStates[SDL_SCANCODE_W])
    {
        xDir += 1.0f;
    }
    if (keyStates[SDL_SCANCODE_S])
    {
        xDir -= 1.0f;
    }
    if (keyStates[SDL_SCANCODE_D])
    {
        zDir += 1.0f;
    }
    if (keyStates[SDL_SCANCODE_A])
    {
        zDir -= 1.0f;
    }

    if (xDir == 0 && zDir == 0)
        return;

    glm::vec3 xAxis = m_camera.fwrd() * xDir;
    glm::vec3 zAxis = m_camera.right() * zDir;

    m_camera.updatePosition(glm::normalize(xAxis + zAxis) * dt * m_speed);
}

} // namespace vke