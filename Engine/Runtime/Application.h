// Owns the main engine loop, demo scene setup, and runtime input bindings.
#pragma once

#include <array>
#include <future>
#include <memory>

#include "Engine/Assets/ResourceManager.h"
#include "Engine/Platform/Window.h"
#include "Engine/Renderer/Renderer.h"
#include "Engine/Scene/Camera.h"
#include "Engine/Scene/Scene.h"

struct DecodedSceneModel;

class Application
{
public:
    Application(int width, int height, const char* title);
    ~Application();

    void Run();

private:
    // Poll OS/window events, react to resize, and advance frame timing state.
    void BeginFrame();
    // Advance non-render systems before building the render-facing frame state.
    void UpdateSystems();
    // Convert gameplay/scene state into the renderer's flattened render world.
    void BuildRenderWorld();
    // Declare pass order and dependencies for the current frame.
    void BuildRenderGraph();
    // Run the passes that were scheduled during BuildRenderGraph.
    void ExecuteRenderGraph();
    // Finalize presentation and reset per-frame state.
    void EndFrame();
    // Builds the sample scene used to exercise the renderer pipeline.
    Scene BuildDemoScene();
    // Finalize any CPU-side async assets that became ready on this frame.
    void PumpAsyncLoads();
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
    std::shared_future<std::shared_ptr<EnvironmentImage>> m_PendingEnvironmentLoad;
    std::string m_PendingEnvironmentPath;
    std::shared_future<std::shared_ptr<DecodedSceneModel>> m_PendingModelLoad;
    std::string m_PendingModelPath;
    glm::mat4 m_PendingModelRootTransform{1.0f};
    bool m_CaptureMouse = false;
    bool m_FirstMouseSample = true;
    double m_LastCursorX = 0.0;
    double m_LastCursorY = 0.0;
    double m_LastFrameTime = 0.0;
    float m_FrameDeltaTime = 0.0f;
    bool m_BloomToggleLatch = false;
    bool m_ShadowToggleLatch = false;
    bool m_IBLToggleLatch = false;
    bool m_ReferenceToggleLatch = false;
    bool m_RebakeLatch = false;
    std::array<bool, 8> m_DebugViewLatches{};
};
