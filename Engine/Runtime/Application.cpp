// Wires together the window, scene, camera, and renderer into a runnable app.
//
// Reading guide:
// 1. Start with Application::Run to see the lifetime of one frame.
// 2. Then read BuildDemoScene to understand what content is fed into the renderer.
// 3. Finally read HandleInput to see which runtime switches affect RenderSettings
//    and when the offline reference needs to be invalidated.
#include "Engine/Runtime/Application.h"

#include <exception>
#include <filesystem>
#include <iostream>

#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Assets/GLTFLoader.h"
#include "Engine/Core/AssetPaths.h"

namespace
{
    // Scans the bundled environment folder and returns the first HDR file, if any.
    std::string FindFirstHdrEnvironment()
    {
        const std::filesystem::path environmentRoot =
            CGEngine::Core::ResolveProjectPath("Assets/environments");
        if (!std::filesystem::exists(environmentRoot))
        {
            return {};
        }

        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(environmentRoot))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::string extension = entry.path().extension().string();
            if (extension == ".hdr" || extension == ".HDR")
            {
                return entry.path().string();
            }
        }

        return {};
    }
}

Application::Application(int width, int height, const char* title)
{
    m_Window = std::make_unique<Window>(width, height, title);
    m_Camera = std::make_unique<Camera>(
        glm::vec3(0.0f, 2.4f, 8.5f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        45.0f,
        m_Window->GetAspectRatio(),
        0.1f,
        40.0f
    );

    m_Scene = BuildDemoScene();

    m_Renderer = std::make_unique<Renderer>(m_ResourceManager);
    m_Renderer->Initialize(m_Window->GetWidth(), m_Window->GetHeight());
    m_LastFrameTime = Window::GetTimeSeconds();
    PrintControls();
}

Application::~Application() = default;

void Application::Run()
{
    while (!m_Window->ShouldClose())
    {
        BeginFrame();
        UpdateSystems();
        BuildRenderWorld();
        BuildRenderGraph();
        ExecuteRenderGraph();
        EndFrame();
    }
}

void Application::BeginFrame()
{
    // Keep OS events and resize handling isolated from scene/system updates.
    m_Window->PollEvents();

    if (m_Window->ConsumeResizeFlag())
    {
        m_Camera->SetAspectRatio(m_Window->GetAspectRatio());
    }

    const double currentTime = Window::GetTimeSeconds();
    m_FrameDeltaTime = static_cast<float>(currentTime - m_LastFrameTime);
    m_LastFrameTime = currentTime;

    m_Renderer->BeginFrame(
        m_Window->GetWidth(),
        m_Window->GetHeight(),
        static_cast<float>(currentTime),
        m_FrameDeltaTime
    );
}

void Application::UpdateSystems()
{
    PumpAsyncLoads();
    HandleInput(m_FrameDeltaTime);
}

void Application::BuildRenderWorld()
{
    m_Renderer->BuildRenderWorld(m_Scene, *m_Camera);
}

void Application::BuildRenderGraph()
{
    m_Renderer->BuildRenderGraph();
}

void Application::ExecuteRenderGraph()
{
    m_Renderer->ExecuteRenderGraph();
}

void Application::EndFrame()
{
    m_Renderer->EndFrame();
    m_Window->SwapBuffers();
}

Scene Application::BuildDemoScene()
{
    // The demo scene intentionally mixes analytic primitives, emissive geometry,
    // and a textured glTF asset so every major renderer path is exercised.
    //
    // This scene is also deliberately small enough that the CPU reference tracer
    // can keep up in the background. If you later replace it with a larger glTF
    // scene, the renderer architecture still works, but the async reference bake
    // will become the first subsystem that needs more optimization.
    Scene scene;

    scene.GetDirectionalLight().direction = glm::vec3(-0.55f, -0.9f, -0.25f);
    scene.GetDirectionalLight().color = glm::vec3(1.0f, 0.97f, 0.92f);
    scene.GetDirectionalLight().intensity = 3.8f;

    scene.GetPointLight().position = glm::vec3(1.8f, 2.6f, 1.2f);
    scene.GetPointLight().color = glm::vec3(1.0f, 0.76f, 0.45f);
    scene.GetPointLight().intensity = 70.0f;
    scene.GetPointLight().range = 10.0f;

    const std::string hdrPath = FindFirstHdrEnvironment();
    if (!hdrPath.empty())
    {
        scene.GetEnvironment().hdrPath = hdrPath;
        m_PendingEnvironmentPath = hdrPath;
        m_PendingEnvironmentLoad = m_ResourceManager.LoadEnvironmentAsync(hdrPath);
        std::cout << "[Env] Queued HDR environment load: " << hdrPath << std::endl;
    }
    else
    {
        std::cout << "[Env] No HDR environment found in Assets/environments, using procedural sky" << std::endl;
    }

    auto ground = m_ResourceManager.GetPlane(16.0f, 6.0f);
    auto sphere = m_ResourceManager.GetSphere(1.0f, 24, 16);
    auto cube = m_ResourceManager.GetCube(1.0f);

    {
        RenderObject object;
        object.name = "Ground";
        object.mesh = ground;
        object.material.albedo = glm::vec3(0.52f, 0.54f, 0.58f);
        object.material.metallic = 0.0f;
        object.material.roughness = 0.92f;
        object.material.ao = 1.0f;
        scene.AddObject(object);
    }

    {
        RenderObject object;
        object.name = "ClaySphere";
        object.mesh = sphere;
        object.transform.position = glm::vec3(-2.2f, 1.0f, -0.4f);
        object.material.albedo = glm::vec3(0.84f, 0.28f, 0.24f);
        object.material.metallic = 0.02f;
        object.material.roughness = 0.62f;
        scene.AddObject(object);
    }

    {
        RenderObject object;
        object.name = "GoldSphere";
        object.mesh = sphere;
        object.transform.position = glm::vec3(0.0f, 1.0f, 0.0f);
        object.material.albedo = glm::vec3(1.0f, 0.8f, 0.24f);
        object.material.metallic = 1.0f;
        object.material.roughness = 0.18f;
        scene.AddObject(object);
    }

    {
        RenderObject object;
        object.name = "TealSphere";
        object.mesh = sphere;
        object.transform.position = glm::vec3(2.2f, 1.0f, 0.45f);
        object.material.albedo = glm::vec3(0.15f, 0.72f, 0.64f);
        object.material.metallic = 0.65f;
        object.material.roughness = 0.36f;
        scene.AddObject(object);
    }

    {
        RenderObject object;
        object.name = "ShadowCaster";
        object.mesh = cube;
        object.transform.position = glm::vec3(-4.2f, 0.65f, 1.8f);
        object.transform.rotationDegrees = glm::vec3(0.0f, 26.0f, 0.0f);
        object.transform.scale = glm::vec3(1.1f, 1.3f, 1.1f);
        object.material.albedo = glm::vec3(0.18f, 0.28f, 0.84f);
        object.material.metallic = 0.0f;
        object.material.roughness = 0.48f;
        scene.AddObject(object);
    }

    {
        const glm::mat4 rootTransform =
            glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 0.75f, 2.2f)) *
            glm::scale(glm::mat4(1.0f), glm::vec3(1.6f));
        m_PendingModelPath = "Assets/models/BoxTextured.glb";
        m_PendingModelRootTransform = rootTransform;
        m_PendingModelLoad = m_ResourceManager.LoadDecodedModelAsync(m_PendingModelPath);
        std::cout << "[Assets] Queued BoxTextured.glb decode" << std::endl;
    }

    {
        RenderObject object;
        object.name = "Emitter";
        object.mesh = cube;
        object.transform.position = scene.GetPointLight().position;
        object.transform.scale = glm::vec3(0.22f);
        object.material.albedo = glm::vec3(1.0f);
        object.material.metallic = 0.0f;
        object.material.roughness = 0.08f;
        object.material.emissive = glm::vec3(8.0f, 5.6f, 3.2f);
        scene.AddObject(object);
    }

    return scene;
}

void Application::PumpAsyncLoads()
{
    if (m_PendingEnvironmentLoad.valid() &&
        m_PendingEnvironmentLoad.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try
        {
            m_Scene.GetEnvironment().hdrPath = m_PendingEnvironmentPath;
            m_Scene.GetEnvironment().hdrImage = m_PendingEnvironmentLoad.get();
            std::cout << "[Env] Loaded HDR environment asynchronously: "
                      << m_PendingEnvironmentPath
                      << std::endl;
        }
        catch (const std::exception& exception)
        {
            std::cout << "[Env] Failed async HDR environment load, using procedural sky: "
                      << exception.what()
                      << std::endl;
        }

        m_PendingEnvironmentLoad = {};
        m_PendingEnvironmentPath.clear();
    }

    if (m_PendingModelLoad.valid() &&
        m_PendingModelLoad.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        GLTFLoader loader;
        try
        {
            std::shared_ptr<DecodedSceneModel> decodedModel = m_PendingModelLoad.get();
            loader.AppendDecodedModelToScene(*decodedModel, m_Scene, m_PendingModelRootTransform);
            std::cout << "[Assets] Finalized glTF model asynchronously: "
                      << m_PendingModelPath
                      << std::endl;
        }
        catch (const std::exception& exception)
        {
            std::cerr << "[Assets] Failed async glTF load for "
                      << m_PendingModelPath
                      << ": "
                      << exception.what()
                      << std::endl;
        }

        m_PendingModelLoad = {};
        m_PendingModelPath.clear();
        m_PendingModelRootTransform = glm::mat4(1.0f);
    }
}

void Application::HandleInput(float deltaTime)
{
    // Camera movement invalidates the offline reference because the viewpoint changed.
    // Pure renderer toggles do not necessarily invalidate the scene itself, but they
    // may change which passes run or how the final composite is produced.
    GLFWwindow* nativeWindow = m_Window->GetNativeHandle();
    bool referenceDirty = false;

    if (glfwGetKey(nativeWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        m_Window->RequestClose();
    }

    const float moveSpeed = glfwGetKey(nativeWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 8.0f : 4.0f;
    const float moveDistance = moveSpeed * deltaTime;

    if (glfwGetKey(nativeWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        m_Camera->MoveForward(moveDistance);
        referenceDirty = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        m_Camera->MoveForward(-moveDistance);
        referenceDirty = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        m_Camera->MoveRight(-moveDistance);
        referenceDirty = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        m_Camera->MoveRight(moveDistance);
        referenceDirty = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_Q) == GLFW_PRESS)
    {
        m_Camera->MoveUp(-moveDistance);
        referenceDirty = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_E) == GLFW_PRESS)
    {
        m_Camera->MoveUp(moveDistance);
        referenceDirty = true;
    }

    const bool shouldCaptureMouse = glfwGetMouseButton(nativeWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (shouldCaptureMouse != m_CaptureMouse)
    {
        m_CaptureMouse = shouldCaptureMouse;
        m_FirstMouseSample = true;
        glfwSetInputMode(
            nativeWindow,
            GLFW_CURSOR,
            m_CaptureMouse ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL
        );
    }

    if (m_CaptureMouse)
    {
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(nativeWindow, &cursorX, &cursorY);

        if (m_FirstMouseSample)
        {
            m_LastCursorX = cursorX;
            m_LastCursorY = cursorY;
            m_FirstMouseSample = false;
        }
        else
        {
            const float deltaX = static_cast<float>(cursorX - m_LastCursorX);
            const float deltaY = static_cast<float>(m_LastCursorY - cursorY);
            m_LastCursorX = cursorX;
            m_LastCursorY = cursorY;

            if (deltaX != 0.0f || deltaY != 0.0f)
            {
                m_Camera->Rotate(deltaX * 0.12f, deltaY * 0.12f);
                referenceDirty = true;
            }
        }
    }

    RenderSettings settings = m_Renderer->GetSettings();
    bool settingsChanged = false;

    if (ConsumeToggleKey(GLFW_KEY_B, m_BloomToggleLatch))
    {
        settings.enableBloom = !settings.enableBloom;
        settingsChanged = true;
        std::cout << "[Render] Bloom " << (settings.enableBloom ? "On" : "Off") << std::endl;
    }

    if (ConsumeToggleKey(GLFW_KEY_N, m_ShadowToggleLatch))
    {
        settings.enableShadows = !settings.enableShadows;
        settingsChanged = true;
        std::cout << "[Render] Shadows " << (settings.enableShadows ? "On" : "Off") << std::endl;
    }

    if (ConsumeToggleKey(GLFW_KEY_M, m_IBLToggleLatch))
    {
        settings.enableIBL = !settings.enableIBL;
        settingsChanged = true;
        std::cout << "[Render] IBL " << (settings.enableIBL ? "On" : "Off") << std::endl;
    }

    if (ConsumeToggleKey(GLFW_KEY_C, m_ReferenceToggleLatch))
    {
        settings.enableReferenceComparison = !settings.enableReferenceComparison;
        settingsChanged = true;
        std::cout << "[Render] Reference Comparison "
                  << (settings.enableReferenceComparison ? "On" : "Off")
                  << std::endl;
    }

    if (ConsumeToggleKey(GLFW_KEY_R, m_RebakeLatch))
    {
        m_Renderer->InvalidateReference();
        std::cout << "[Render] Queued reference rebake" << std::endl;
    }

    const auto setDebugView = [&](int key, std::size_t index, DebugViewMode mode, const char* label) {
        if (ConsumeToggleKey(key, m_DebugViewLatches[index]))
        {
            settings.debugView = mode;
            settingsChanged = true;
            std::cout << "[Render] Debug View -> " << label << std::endl;
        }
    };

    setDebugView(GLFW_KEY_1, 0, DebugViewMode::Final, "Final");
    setDebugView(GLFW_KEY_2, 1, DebugViewMode::SceneColor, "SceneColor");
    setDebugView(GLFW_KEY_3, 2, DebugViewMode::BrightColor, "BrightColor");
    setDebugView(GLFW_KEY_4, 3, DebugViewMode::Albedo, "Albedo");
    setDebugView(GLFW_KEY_5, 4, DebugViewMode::Normal, "Normal");
    setDebugView(GLFW_KEY_6, 5, DebugViewMode::Material, "Material");
    setDebugView(GLFW_KEY_7, 6, DebugViewMode::Depth, "Depth");
    setDebugView(GLFW_KEY_8, 7, DebugViewMode::Shadow, "Shadow");

    const float exposureStep = deltaTime * 0.85f;
    const float splitStep = deltaTime * 0.45f;

    if (glfwGetKey(nativeWindow, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(nativeWindow, GLFW_KEY_KP_ADD) == GLFW_PRESS)
    {
        settings.exposure += exposureStep;
        settingsChanged = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(nativeWindow, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
    {
        settings.exposure -= exposureStep;
        settingsChanged = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
    {
        settings.splitPosition -= splitStep;
        settingsChanged = true;
    }
    if (glfwGetKey(nativeWindow, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
    {
        settings.splitPosition += splitStep;
        settingsChanged = true;
    }

    if (settingsChanged)
    {
        m_Renderer->SetSettings(settings);
    }

    if (referenceDirty)
    {
        m_Renderer->InvalidateReference();
    }
}

void Application::PrintControls() const
{
    std::cout
        << "Controls:\n"
        << "  WASD/QE: move camera\n"
        << "  Right Mouse: look around\n"
        << "  Shift: move faster\n"
        << "  B: toggle bloom\n"
        << "  N: toggle shadows\n"
        << "  M: toggle IBL\n"
        << "  C: toggle realtime/reference split compare\n"
        << "  R: rebake ray-traced reference\n"
        << "  1-8: switch final / scene / bright / albedo / normal / material / depth / shadow views\n"
        << "  +/-: adjust exposure\n"
        << "  [ / ]: adjust split position\n"
        << "  Esc: quit\n";
}

bool Application::ConsumeToggleKey(int key, bool& latch) const
{
    // Trigger once on the rising edge so held keys do not flip settings every frame.
    const bool isPressed = glfwGetKey(m_Window->GetNativeHandle(), key) == GLFW_PRESS;
    const bool triggered = isPressed && !latch;
    latch = isPressed;
    return triggered;
}
