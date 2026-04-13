// Legacy VAO helper kept for simple experiments alongside the Mesh wrapper.
#pragma once

class VertexBuffer;
class IndexBuffer;

class VertexArray
{
public:
    VertexArray();
    ~VertexArray();

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer_Position3f(const VertexBuffer& vbo);
    void SetIndexBuffer(const IndexBuffer& ibo);

private:
    unsigned int m_ID = 0;
};
