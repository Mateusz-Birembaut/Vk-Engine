#include "VkEngine/Window.h"

#include <SDL3/SDL_vulkan.h>

#include <stdexcept>

#include "VkEngine/VkEngineInfo.h"

Window::Window()
{
    m_window = SDL_CreateWindow(app::NAME.data(), 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    setMouseCapture(m_capturingMouse);
}

Window::~Window()
{
    SDL_DestroyWindow(m_window);
}

bool Window::isCapturingMouse() const
{
    return m_capturingMouse;
}

void Window::setMouseCapture(bool isCapturingMouse)
{
    m_capturingMouse = isCapturingMouse;
    SDL_SetWindowRelativeMouseMode(m_window, m_capturingMouse);
}

VkExtent2D Window::getSizeInPixels() const
{
    int w{};
    int h{};

    if (!SDL_GetWindowSizeInPixels(m_window, &w, &h))
        SDL_Log("%s", "[SDL] : could not get window size");

    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

VkSurfaceKHR Window::createSurface(VkInstance instance, const struct VkAllocationCallbacks* pallocator) const
{
    VkSurfaceKHR vkSurface{};
    if (!SDL_Vulkan_CreateSurface(m_window, instance, pallocator, &vkSurface))
        throw std::runtime_error(std::string("[SDL] Could not create vulkan  : ") + SDL_GetError());

    return vkSurface;
}
