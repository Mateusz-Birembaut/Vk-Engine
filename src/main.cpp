#include <iostream>
#include <stdexcept>
#include <stdlib.h>

#include "VkEngine/VkEngine.h"

int main(int argc, char* argv[])
{
    VkEngine engine{};

    try {
        engine.init();
    } catch (const std::runtime_error& error) {
        std::cout << error.what() << '\n';
    }

    engine.run();

    return EXIT_SUCCESS;
}