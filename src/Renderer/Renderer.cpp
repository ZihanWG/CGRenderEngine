#include "Renderer/Renderer.h"

#include <glad/glad.h>
#include <vector>

Renderer::~Renderer() = default;

void Renderer::Init()
{
    std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    std::vector<unsigned int> indices = {
        0, 1, 2
    };

    m_Mesh = std::make_unique<Mesh>(vertices, indices);
    m_Shader = std::make_unique<Shader>("shaders/basic.vert", "shaders/basic.frag");
}

void Renderer::BeginFrame()
{
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::Render()
{
    m_Shader->Use();
    m_Mesh->Draw();
}

void Renderer::EndFrame()
{
}