// OpenGL texture allocation and binding helpers for both assets and render targets.
#include "Engine/RHI/Texture2D.h"

#include <utility>

namespace
{
    // Binds a texture on the active unit for setup or upload and restores the caller's
    // binding on scope exit. Optionally forces a byte-aligned unpack for tightly packed
    // CPU data and restores the previous alignment as well.
    class ScopedTextureSetup
    {
    public:
        ScopedTextureSetup(unsigned int texture, bool tightUnpack)
            : m_RestoreUnpack(tightUnpack)
        {
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_PreviousTexture);
            if (m_RestoreUnpack)
            {
                glGetIntegerv(GL_UNPACK_ALIGNMENT, &m_PreviousUnpackAlignment);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            }

            glBindTexture(GL_TEXTURE_2D, texture);
        }

        ~ScopedTextureSetup()
        {
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_PreviousTexture));
            if (m_RestoreUnpack)
            {
                glPixelStorei(GL_UNPACK_ALIGNMENT, m_PreviousUnpackAlignment);
            }
        }

        ScopedTextureSetup(const ScopedTextureSetup&) = delete;
        ScopedTextureSetup& operator=(const ScopedTextureSetup&) = delete;

    private:
        bool m_RestoreUnpack = false;
        GLint m_PreviousTexture = 0;
        GLint m_PreviousUnpackAlignment = 4;
    };
}

Texture2D::~Texture2D()
{
    if (m_ID)
    {
        glDeleteTextures(1, &m_ID);
    }
}

Texture2D::Texture2D(Texture2D&& other) noexcept
    : m_ID(std::exchange(other.m_ID, 0u))
    , m_Width(std::exchange(other.m_Width, 0))
    , m_Height(std::exchange(other.m_Height, 0))
{
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept
{
    if (this != &other)
    {
        if (m_ID)
        {
            glDeleteTextures(1, &m_ID);
        }

        m_ID = std::exchange(other.m_ID, 0u);
        m_Width = std::exchange(other.m_Width, 0);
        m_Height = std::exchange(other.m_Height, 0);
    }

    return *this;
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
    if (!m_ID)
    {
        glGenTextures(1, &m_ID);
    }

    m_Width = width;
    m_Height = height;

    // The wrapper intentionally keeps state setup in one place so callers do not leak GL state.
    const ScopedTextureSetup setup(m_ID, true);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width, height, 0, format, type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapT));
}

void Texture2D::SetData(GLenum format, GLenum type, const void* data) const
{
    // Update the full image. Partial updates are not needed in this project yet.
    const ScopedTextureSetup setup(m_ID, true);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, format, type, data);
}

void Texture2D::GenerateMipmaps() const
{
    const ScopedTextureSetup setup(m_ID, false);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void Texture2D::SetBorderColor(float r, float g, float b, float a) const
{
    const float borderColor[] = {r, g, b, a};
    const ScopedTextureSetup setup(m_ID, false);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
}

void Texture2D::Bind(unsigned int slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_ID);
}
