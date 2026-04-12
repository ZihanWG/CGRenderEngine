#include "Renderer/ShaderBuffer.h"

#include <stdexcept>

#ifndef GL_SHADER_STORAGE_BUFFER
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#endif

ShaderBuffer::~ShaderBuffer()
{
    if (m_ID)
    {
        glDeleteBuffers(1, &m_ID);
    }
}

void ShaderBuffer::Allocate(ShaderBufferKind kind, std::size_t size, GLenum usage)
{
    EnsureCreated();
    m_Kind = kind;
    m_Size = size;

    const GLenum target = ResolveTarget();
    glBindBuffer(target, m_ID);
    glBufferData(target, static_cast<GLsizeiptr>(size), nullptr, usage);
    glBindBuffer(target, 0);
}

void ShaderBuffer::SetData(const void* data, std::size_t size, std::size_t offset) const
{
    if (!m_ID || offset + size > m_Size)
    {
        throw std::runtime_error("ShaderBuffer update range is invalid.");
    }

    const GLenum target = ResolveTarget();
    glBindBuffer(target, m_ID);
    glBufferSubData(
        target,
        static_cast<GLintptr>(offset),
        static_cast<GLsizeiptr>(size),
        data
    );
    glBindBuffer(target, 0);
}

void ShaderBuffer::BindBase(unsigned int bindingIndex) const
{
    if (!m_ID)
    {
        return;
    }

    glBindBufferBase(ResolveTarget(), bindingIndex, m_ID);
}

GLenum ShaderBuffer::ResolveTarget() const
{
    switch (m_Kind)
    {
    case ShaderBufferKind::Uniform:
        return GL_UNIFORM_BUFFER;
    case ShaderBufferKind::Storage:
        return GL_SHADER_STORAGE_BUFFER;
    default:
        throw std::runtime_error("Unsupported shader buffer kind.");
    }
}

void ShaderBuffer::EnsureCreated()
{
    if (!m_ID)
    {
        glGenBuffers(1, &m_ID);
    }
}
