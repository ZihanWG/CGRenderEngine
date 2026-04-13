// Thin GLFW/GLAD wrapper that owns the native window and resize state.
#pragma once

struct GLFWwindow;

class Window
{
public:
    Window(int width, int height, const char* title);
    ~Window();

    bool ShouldClose() const;
    void PollEvents();
    void SwapBuffers();
    void RequestClose();

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    float GetAspectRatio() const;
    bool ConsumeResizeFlag();
    GLFWwindow* GetNativeHandle() const { return m_Window; }

    static double GetTimeSeconds();

private:
    // Keeps the cached size in sync with GLFW and marks the swapchain as dirty.
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    GLFWwindow* m_Window = nullptr;
    int m_Width = 0;
    int m_Height = 0;
    bool m_WasResized = false;
};
