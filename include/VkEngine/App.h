#pragma once

#include <memory>

class Window;
class VulkanCtx;
class Renderer;
class CameraController;

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
    std::unique_ptr<CameraController> m_cameraController;
    bool m_running = true;
};
