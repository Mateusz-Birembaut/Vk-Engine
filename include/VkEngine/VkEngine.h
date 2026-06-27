#pragma once

#include <memory>

#include "Window.h"

class VkEngine {
public:
    VkEngine() = default;
    ~VkEngine();

    void init();
    void run();

private:
    std::unique_ptr<Window> m_window = nullptr;
    bool m_running = true;
};
