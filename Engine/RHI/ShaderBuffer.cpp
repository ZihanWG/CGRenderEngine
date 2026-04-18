// RAII wrapper around OpenGL uniform/storage buffer objects.
#include "Engine/RHI/ShaderBuffer.h"

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

    // Updates use sub-data so persistent buffer handles can stay bound across frames.
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

void ShaderBuffer::BindRange(unsigned int bindingIndex, std::size_t offset, std::size_t size) const
{
    if (!m_ID || offset + size > m_Size)
    {
        throw std::runtime_error("ShaderBuffer bind range is invalid.");
    }

    glBindBufferRange(
        ResolveTarget(),
        bindingIndex,
        m_ID,
        static_cast<GLintptr>(offset),
        static_cast<GLsizeiptr>(size)
    );
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
