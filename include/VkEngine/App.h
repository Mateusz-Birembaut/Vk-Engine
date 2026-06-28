#pragma once

#include <memory>

class Window;
class VulkanCtx;

class App {
public:
    App();
    ~App();

    void init();
    void run();

private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<VulkanCtx> m_ctx;
    bool m_running = true;
};
