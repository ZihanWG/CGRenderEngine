#pragma once

#include <memory>
#include "Core/Window.h"
#include "Renderer/Renderer.h"

class Application
{
public:
    Application(int width, int height, const char* title);
    ~Application();

    void Run();

private:
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
};