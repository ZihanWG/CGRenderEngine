// Legacy EBO helper kept for simple examples outside the main Mesh abstraction.
#pragma once

class IndexBuffer
{
public:
    IndexBuffer(const unsigned int* data, unsigned int count);
    ~IndexBuffer();

    // Owns a raw GL name that the destructor deletes, so a copy would delete it twice.
    // Matches Texture2D/Framebuffer/ShaderBuffer/Mesh, which are non-copyable for the
    // same reason.
    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;

    void Bind() const;
    void Unbind() const;

    unsigned int GetCount() const { return m_Count; }

private:
    unsigned int m_ID = 0;
    unsigned int m_Count = 0;
};
