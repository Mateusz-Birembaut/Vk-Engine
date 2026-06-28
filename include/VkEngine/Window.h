#pragma once

#include <vulkan/vulkan.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

class Window {
public:
    Window();

    ~Window();

    VkExtent2D getSizeInPixels();

    VkSurfaceKHR createSurface(VkInstance instance, const struct VkAllocationCallbacks* pallocator = nullptr);

private:
    SDL_Window* m_window = nullptr;
};
