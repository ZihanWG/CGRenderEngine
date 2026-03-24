#include "Renderer/Mesh.h"
#include "Renderer/VertexArray.h"
#include "Renderer/VertexBuffer.h"
#include "Renderer/IndexBuffer.h"
#include <glad/glad.h>

Mesh::Mesh(const std::vector<float>& vertices,
           const std::vector<unsigned int>& indices)
{
    m_IndexCount = static_cast<unsigned int>(indices.size());

    m_VAO = std::make_unique<VertexArray>();
    m_VBO = std::make_unique<VertexBuffer>(
        vertices.data(),
        static_cast<unsigned int>(vertices.size() * sizeof(float))
    );
    m_IBO = std::make_unique<IndexBuffer>(
        indices.data(),
        static_cast<unsigned int>(indices.size())
    );

    m_VAO->AddVertexBuffer_Position3f(*m_VBO);
    m_VAO->SetIndexBuffer(*m_IBO);
}

void Mesh::Draw() const
{
    m_VAO->Bind();
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
}

Mesh::~Mesh() = default;
