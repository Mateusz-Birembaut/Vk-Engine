#include <cstdlib>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#include <SDL3/SDL_main.h>
#endif

#include "VkEngine/App.h"

int main(int, char*[])
{
    vke::App app{};

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal : " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}