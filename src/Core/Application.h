// Owns the main engine loop, demo scene setup, and runtime input bindings.
#pragma once

#include <array>
#include <memory>

#include "Assets/ResourceManager.h"
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
    // Builds the sample scene used to exercise the renderer pipeline.
    Scene BuildDemoScene();
    // Applies camera motion and renderer hotkeys once per frame.
    void HandleInput(float deltaTime);
    // Prints the runtime controls once at startup.
    void PrintControls() const;
    // Converts "pressed this frame" behavior into a latched toggle key.
    bool ConsumeToggleKey(int key, bool& latch) const;

    ResourceManager m_ResourceManager;
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<Camera> m_Camera;
    Scene m_Scene;
    bool m_CaptureMouse = false;
    bool m_FirstMouseSample = true;
    double m_LastCursorX = 0.0;
    double m_LastCursorY = 0.0;
    bool m_BloomToggleLatch = false;
    bool m_ShadowToggleLatch = false;
    bool m_IBLToggleLatch = false;
    bool m_ReferenceToggleLatch = false;
    bool m_RebakeLatch = false;
    std::array<bool, 8> m_DebugViewLatches{};
};
