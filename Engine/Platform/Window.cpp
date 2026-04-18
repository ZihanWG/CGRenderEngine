// Initializes the OpenGL context and exposes the small window API used by the app.
#include "Engine/Platform/Window.h"

#include <iostream>
#include <stdexcept>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

Window::Window(int width, int height, const char* title)
    : m_Width(width), m_Height(height)
{
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW.");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_Window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window.");
    }

    glfwMakeContextCurrent(m_Window);
    // V-sync keeps the demo stable and avoids burning a core on uncapped presentation.
    glfwSwapInterval(1);
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwDestroyWindow(m_Window);
        glfwTerminate();
        throw std::runtime_error("Failed to initialize GLAD.");
    }

    glViewport(0, 0, width, height);

    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
}

Window::~Window()
{
    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
    }
    glfwTerminate();
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

void Window::RequestClose()
{
    glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
}

void Window::PollEvents()
{
    glfwPollEvents();
}

void Window::SwapBuffers()
{
    glfwSwapBuffers(m_Window);
}

float Window::GetAspectRatio() const
{
    return m_Height > 0 ? static_cast<float>(m_Width) / static_cast<float>(m_Height) : 1.0f;
}

bool Window::ConsumeResizeFlag()
{
    const bool wasResized = m_WasResized;
    m_WasResized = false;
    return wasResized;
}

double Window::GetTimeSeconds()
{
    return glfwGetTime();
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* instance = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!instance)
    {
        return;
    }

    instance->m_Width = width;
    instance->m_Height = height;
    instance->m_WasResized = true;
    glViewport(0, 0, width, height);
}
