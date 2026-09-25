// Small RAII wrapper around OpenGL framebuffers and their depth renderbuffer, when used.
#include "Engine/RHI/Framebuffer.h"

#include <utility>
#include <vector>

#include <glad/glad.h>

#include "Engine/RHI/Texture2D.h"

Framebuffer::~Framebuffer()
{
    Release();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_ID(std::exchange(other.m_ID, 0u))
    , m_DepthRenderbuffer(std::exchange(other.m_DepthRenderbuffer, 0u))
{
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
    if (this != &other)
    {
        Release();
        m_ID = std::exchange(other.m_ID, 0u);
        m_DepthRenderbuffer = std::exchange(other.m_DepthRenderbuffer, 0u);
    }

    return *this;
}

void Framebuffer::Release() noexcept
{
    if (m_DepthRenderbuffer)
    {
        glDeleteRenderbuffers(1, &m_DepthRenderbuffer);
        m_DepthRenderbuffer = 0;
    }

    if (m_ID)
    {
        glDeleteFramebuffers(1, &m_ID);
        m_ID = 0;
    }
}

unsigned int Framebuffer::EnsureCreated() const
{
    if (!m_ID)
    {
        glGenFramebuffers(1, &m_ID);
    }

    return m_ID;
}

void Framebuffer::Bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, EnsureCreated());
}

void Framebuffer::Unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::AttachColorTexture(const Texture2D& texture, unsigned int index) const
{
    Bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, texture.GetID(), 0);
}

void Framebuffer::AttachDepthTexture(const Texture2D& texture) const
{
    Bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture.GetID(), 0);
}

void Framebuffer::CreateDepthRenderbuffer(int width, int height)
{
    Bind();

    if (!m_DepthRenderbuffer)
    {
        glGenRenderbuffers(1, &m_DepthRenderbuffer);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_DepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Framebuffer::SetDrawBuffers(unsigned int count) const
{
    Bind();

    // Build the attachment list dynamically so the caller only specifies the color count.
    std::vector<GLenum> attachments(count);
    for (unsigned int i = 0; i < count; ++i)
    {
        attachments[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());
}

bool Framebuffer::CheckComplete() const
{
    Bind();
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}
