#include "VkEngine/Window.h"

Window::Window()
{
    m_window = SDL_CreateWindow("VkEngine", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
}

Window::~Window()
{
    SDL_DestroyWindow(m_window);
}

VkExtent2D Window::getSizeInPixels()
{
    int w{};
    int h{};

    if (!SDL_GetWindowSizeInPixels(m_window, &w, &h))
        SDL_Log("%s", "SDL : could not get window size");

    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}
