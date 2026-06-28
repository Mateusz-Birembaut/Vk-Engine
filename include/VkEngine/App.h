#pragma once

#include <memory>

class Window;
class VulkanCtx;
class Renderer;

class App {
public:
    App();
    ~App();

    void init();
    void run();

private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<VulkanCtx> m_ctx;
    std::unique_ptr<Renderer> m_renderer;
    bool m_running = true;
};
