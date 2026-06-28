#include <iostream>
#include <stdexcept>
#include <stdlib.h>

#include "VkEngine/App.h"

int main(int argc, char* argv[])
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