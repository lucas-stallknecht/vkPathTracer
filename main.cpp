#include <iostream>
#include "Engine.h"

int main()
{
    pt::Engine engine{};

    try
    {
        engine.init();
        engine.run();
        engine.cleanup();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
