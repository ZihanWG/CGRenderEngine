#pragma once

#include <memory>

#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"

class Application
{
public:
    Application(int width, int height, const char* title);
    ~Application();

    void Run();

private:
    Scene BuildDemoScene() const;
    void HandleInput();

    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<Camera> m_Camera;
    Scene m_Scene;
};
