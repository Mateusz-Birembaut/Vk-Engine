#pragma once

#include <vulkan/vulkan_core.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>

class Window {
public:
    Window();

    ~Window();

    bool isCapturingMouse() const;
    bool isMinimized() const;
    void setMouseCapture(bool enable);
    void setMinimized(bool minimized);

    VkExtent2D getSizeInPixels() const;

    VkSurfaceKHR createSurface(VkInstance instance, const struct VkAllocationCallbacks* pallocator = nullptr) const;

private:
    SDL_Window* m_window{nullptr};
    bool m_capturingMouse{true};
    bool m_minimized{false};
};
