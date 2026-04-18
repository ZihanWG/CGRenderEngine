// Legacy VAO helper retained for small standalone experiments.
#include "Engine/RHI/VertexArray.h"
#include "Engine/RHI/VertexBuffer.h"
#include "Engine/RHI/IndexBuffer.h"
#include <glad/glad.h>

VertexArray::VertexArray()
{
    glGenVertexArrays(1, &m_ID);
}

VertexArray::~VertexArray()
{
    if (m_ID)
        glDeleteVertexArrays(1, &m_ID);
}

void VertexArray::Bind() const
{
    glBindVertexArray(m_ID);
}

void VertexArray::Unbind() const
{
    glBindVertexArray(0);
}

void VertexArray::AddVertexBuffer_Position3f(const VertexBuffer& vbo)
{
    Bind();
    vbo.Bind();

    // This helper only exposes a position-only layout because that is all the legacy samples need.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

void VertexArray::SetIndexBuffer(const IndexBuffer& ibo)
{
    Bind();
    ibo.Bind();
}
