#include "Core/Application.h"

Application::Application(int width, int height, const char* title)
{
    m_Window = std::make_unique<Window>(width, height, title);
    m_Renderer = std::make_unique<Renderer>();
    m_Renderer->Init();
}

Application::~Application() = default;

void Application::Run()
{
    while (!m_Window->ShouldClose())
    {
        m_Window->PollEvents();

        m_Renderer->BeginFrame();
        m_Renderer->Render();
        m_Renderer->EndFrame();

        m_Window->SwapBuffers();
    }
}