#pragma once

class Window
{
public:
    Window(int width, int height, const char* title);
    ~Window();

    bool ShouldClose() const;
    void PollEvents();
    void SwapBuffers();

private:
    struct GLFWwindow* m_Window = nullptr;
};