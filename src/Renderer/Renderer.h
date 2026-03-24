#pragma once

#include <memory>
#include "Renderer/Shader.h"
#include "Renderer/Mesh.h"

class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    void Init();
    void BeginFrame();
    void Render();
    void EndFrame();

private:
    std::unique_ptr<Shader> m_Shader;
    std::unique_ptr<Mesh> m_Mesh;
};