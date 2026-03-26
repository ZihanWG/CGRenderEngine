#pragma once

#include <glad/glad.h>

class Texture2D
{
public:
    Texture2D();
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    void Allocate(
        int width,
        int height,
        GLenum internalFormat,
        GLenum format,
        GLenum type,
        const void* data = nullptr,
        GLenum minFilter = GL_LINEAR,
        GLenum magFilter = GL_LINEAR,
        GLenum wrapS = GL_CLAMP_TO_EDGE,
        GLenum wrapT = GL_CLAMP_TO_EDGE
    );

    void SetData(GLenum format, GLenum type, const void* data) const;
    void GenerateMipmaps() const;
    void SetBorderColor(float r, float g, float b, float a) const;
    void Bind(unsigned int slot) const;

    unsigned int GetID() const { return m_ID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

private:
    unsigned int m_ID = 0;
    int m_Width = 0;
    int m_Height = 0;
};
