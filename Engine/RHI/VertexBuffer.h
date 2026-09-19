// Legacy VBO helper kept for simple examples outside the main Mesh abstraction.
#pragma once

class VertexBuffer
{
public:
    VertexBuffer(const void* data, unsigned int size);
    ~VertexBuffer();

    // Owns a raw GL name that the destructor deletes, so a copy would delete it twice.
    // Matches Texture2D/Framebuffer/ShaderBuffer/Mesh, which are non-copyable for the
    // same reason.
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

    void Bind() const;
    void Unbind() const;

private:
    unsigned int m_ID = 0;
};
