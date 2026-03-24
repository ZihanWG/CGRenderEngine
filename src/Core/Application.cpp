#include "Core/Application.h"

#include <GLFW/glfw3.h>

#include "Renderer/Mesh.h"

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

    m_Renderer = std::make_unique<Renderer>();
    m_Renderer->Initialize(m_Window->GetWidth(), m_Window->GetHeight());
}

Application::~Application() = default;

void Application::Run()
{
    double lastTime = Window::GetTimeSeconds();

    while (!m_Window->ShouldClose())
    {
        m_Window->PollEvents();
        HandleInput();

        if (m_Window->ConsumeResizeFlag())
        {
            m_Camera->SetAspectRatio(m_Window->GetAspectRatio());
        }

        const double currentTime = Window::GetTimeSeconds();
        const double deltaTime = currentTime - lastTime;
        (void)deltaTime;
        lastTime = currentTime;

        m_Renderer->RenderFrame(
            m_Scene,
            *m_Camera,
            m_Window->GetWidth(),
            m_Window->GetHeight()
        );

        m_Window->SwapBuffers();
    }
}

Scene Application::BuildDemoScene() const
{
    Scene scene;

    scene.GetDirectionalLight().direction = glm::vec3(-0.55f, -0.9f, -0.25f);
    scene.GetDirectionalLight().color = glm::vec3(1.0f, 0.97f, 0.92f);
    scene.GetDirectionalLight().intensity = 3.8f;

    scene.GetPointLight().position = glm::vec3(1.8f, 2.6f, 1.2f);
    scene.GetPointLight().color = glm::vec3(1.0f, 0.76f, 0.45f);
    scene.GetPointLight().intensity = 70.0f;
    scene.GetPointLight().range = 10.0f;

    auto ground = Mesh::CreatePlane(16.0f, 6.0f);
    auto sphere = Mesh::CreateSphere(1.0f, 24, 16);
    auto cube = Mesh::CreateCube(1.0f);

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

void Application::HandleInput()
{
    if (glfwGetKey(m_Window->GetNativeHandle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        m_Window->RequestClose();
    }
}
