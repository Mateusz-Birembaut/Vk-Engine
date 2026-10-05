#include <iostream>
#include <stdexcept>
#include <stdlib.h>

#ifdef _WIN32
#include <SDL3/SDL_main.h>
#endif

#include "VkEngine/App.h"

int main(int, char*[])
{
    App app{};

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal : " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}