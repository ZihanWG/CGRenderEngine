// Legacy VAO helper kept for simple experiments alongside the Mesh wrapper.
#pragma once

class VertexBuffer;
class IndexBuffer;

class VertexArray
{
public:
    VertexArray();
    ~VertexArray();

    // Owns a raw GL name that the destructor deletes, so a copy would delete it twice.
    // Matches Texture2D/Framebuffer/ShaderBuffer/Mesh, which are non-copyable for the
    // same reason.
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;

    void Bind() const;
    void Unbind() const;

    void AddVertexBuffer_Position3f(const VertexBuffer& vbo);
    void SetIndexBuffer(const IndexBuffer& ibo);

private:
    unsigned int m_ID = 0;
};
