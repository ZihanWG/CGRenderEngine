// Legacy EBO helper kept for simple examples outside the main Mesh abstraction.
#pragma once

class IndexBuffer
{
public:
    IndexBuffer(const unsigned int* data, unsigned int count);
    ~IndexBuffer();

    void Bind() const;
    void Unbind() const;

    unsigned int GetCount() const { return m_Count; }

private:
    unsigned int m_ID = 0;
    unsigned int m_Count = 0;
};
