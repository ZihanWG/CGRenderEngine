#pragma once

#include <cstddef>

#include <glad/glad.h>

enum class ShaderBufferKind
{
    Uniform,
    Storage
};

class ShaderBuffer
{
public:
    ShaderBuffer() = default;
    ~ShaderBuffer();

    ShaderBuffer(const ShaderBuffer&) = delete;
    ShaderBuffer& operator=(const ShaderBuffer&) = delete;

    void Allocate(ShaderBufferKind kind, std::size_t size, GLenum usage = GL_DYNAMIC_DRAW);
    void SetData(const void* data, std::size_t size, std::size_t offset = 0) const;
    void BindBase(unsigned int bindingIndex) const;

    std::size_t GetSize() const { return m_Size; }
    ShaderBufferKind GetKind() const { return m_Kind; }

private:
    GLenum ResolveTarget() const;
    void EnsureCreated();

    unsigned int m_ID = 0;
    std::size_t m_Size = 0;
    ShaderBufferKind m_Kind = ShaderBufferKind::Uniform;
};
