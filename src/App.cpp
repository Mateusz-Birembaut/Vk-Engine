#include "VkEngine/App.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#if defined(__linux__)
#include <SDL3/SDL_main.h>
#endif

#include <filesystem>
#include <iostream>
#include <stdexcept>

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

    if (!SDL_SetAppMetadata(app::NAME.data(), app::VERSION.data(), "VkEngineID"))
        SDL_Log("%s", "Could not set app meta data");

    m_window = std::make_unique<Window>();

    m_ctx = std::make_unique<VulkanCtx>();
    m_ctx->init(m_window.get());

    const char* exeDir = SDL_GetBasePath();
    std::filesystem::path shadePath{exeDir};
    shadePath += "Shaders";

    m_renderer = std::make_unique<Renderer>();
    m_renderer->init(m_ctx.get(), shadePath);
}

void App::run()
{
    init();

    while (m_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_running = false;
            }
        }

        if (!m_running)
            break;

        // update game state, draw the current frame
        m_renderer->drawFrame();
    }
}
