// OpenGL texture allocation and binding helpers for both assets and render targets.
#include "Engine/RHI/Texture2D.h"

Texture2D::Texture2D()
{
    glGenTextures(1, &m_ID);
}

Texture2D::~Texture2D()
{
    if (m_ID)
    {
        glDeleteTextures(1, &m_ID);
    }
}

void Texture2D::Allocate(
    int width,
    int height,
    GLenum internalFormat,
    GLenum format,
    GLenum type,
    const void* data,
    GLenum minFilter,
    GLenum magFilter,
    GLenum wrapS,
    GLenum wrapT
)
{
    m_Width = width;
    m_Height = height;

    // The wrapper intentionally keeps state setup in one place so callers do not leak GL state.
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width, height, 0, format, type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapT));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::SetData(GLenum format, GLenum type, const void* data) const
{
    // Update the full image. Partial updates are not needed in this project yet.
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, format, type, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::GenerateMipmaps() const
{
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::SetBorderColor(float r, float g, float b, float a) const
{
    const float borderColor[] = {r, g, b, a};
    glBindTexture(GL_TEXTURE_2D, m_ID);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}
