// Legacy VBO helper kept for simple examples outside the main Mesh abstraction.
#pragma once

class VertexBuffer
{
public:
    VertexBuffer(const void* data, unsigned int size);
    ~VertexBuffer();

    void Bind() const;
    void Unbind() const;

private:
    unsigned int m_ID = 0;
};
