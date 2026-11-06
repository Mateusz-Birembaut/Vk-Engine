#include "VkEngine/App.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>
#if defined(__linux__)
#include <SDL3/SDL_main.h>
#endif

#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>

#include "VkEngine/CameraController.h"
#include "VkEngine/Renderer.h"
#include "VkEngine/VkEngineInfo.h"
#include "VkEngine/VulkanCtx.h"
#include "VkEngine/Window.h"

App::App() = default;

App::~App()
{
    SDL_Quit();
}

void App::init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        throw std::runtime_error(std::string("[SDL] Couldn't init  : ") + SDL_GetError());

    if (!SDL_SetAppMetadata(app::NAME.data(), app::VERSION.data(), "vkeID"))
        SDL_Log("%s", "Could not set app meta data");

    m_window = std::make_unique<Window>();

    m_ctx = std::make_unique<VulkanCtx>();
    m_ctx->init(m_window.get());

    const char* exeDir = SDL_GetBasePath();
    std::filesystem::path shadePath{exeDir};
    shadePath += "Shaders";

    m_renderer = std::make_unique<Renderer>();
    m_renderer->init(m_ctx.get(), shadePath);

    m_cameraController = std::make_unique<CameraController>();
}

void App::run()
{
    init();

    uint64_t previousTime = SDL_GetTicksNS();
    VkExtent2D previousExtent{};

    while (m_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                m_running = false;
            }

            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                    m_window->setMouseCapture(false);
                } else if (e.key.scancode == SDL_SCANCODE_RETURN) {
                    m_window->setMouseCapture(true);
                }
            }

            if (e.type == SDL_EVENT_MOUSE_MOTION && m_window->isCapturingMouse()) {
                m_cameraController->handleMouseMotion(e.motion.xrel, e.motion.yrel);
            }
        }

        uint64_t currentTime = SDL_GetTicksNS();
        // delta time in s, dt < 0.1s to limit range of motion after alt tabing
        float dt = std::min(static_cast<float>(currentTime - previousTime) / 1e9f, 0.1f);
        previousTime = currentTime;

        m_cameraController->handleKeyboard(dt);

        if (!m_running)
            break;

        // update game state, draw the current frame
        m_renderer->drawFrame(m_cameraController->renderView());

        // update after swapchain recreation
        VkExtent2D currentExtent = m_ctx->swapchainExtent();
        if (currentExtent.width != 0 && currentExtent.height != 0 &&
            (currentExtent.height != previousExtent.height || currentExtent.width != previousExtent.width)) {
            previousExtent = currentExtent;
            m_cameraController->updateAspectRatio(
                static_cast<float>(currentExtent.width) / static_cast<float>(currentExtent.height)
            );
        }
    }
}