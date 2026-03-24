#include "Core/Application.h"
#include <iostream>

int main()
{
    try
    {
        Application app(1280, 720, "Realtime Renderer");
        app.Run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}