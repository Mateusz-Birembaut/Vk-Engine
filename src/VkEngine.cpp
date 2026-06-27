#include "VkEngine/VkEngine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_main.h>

#include <stdexcept>

#include "VkEngine/VkEngineInfo.h"

VkEngine::~VkEngine()
{
    SDL_Quit();
}

void VkEngine::init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        throw std::runtime_error(std::string("Could not init sdl : ") + SDL_GetError());

    if (!SDL_SetAppMetadata(app::NAME.data(), app::VERSION.data(), "VkEngineID"))
        SDL_Log("%s", "Could not set app meta data");

    m_window = std::make_unique<Window>();
}

void VkEngine::run()
{
    while (m_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                m_running = false;
            }

            // update game state, draw the current frame
        }
    }
}
