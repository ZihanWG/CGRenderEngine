// Small RAII wrapper around OpenGL framebuffers and their depth renderbuffer, when used.
#include "Engine/RHI/Framebuffer.h"

#include <utility>
#include <vector>

#include <glad/glad.h>

#include "Engine/RHI/Texture2D.h"

namespace
{
    // Binds a framebuffer for configuration and restores the caller's draw/read bindings
    // on scope exit, so setup helpers do not leave a different target bound behind them.
    class ScopedFramebufferBinding
    {
    public:
        explicit ScopedFramebufferBinding(unsigned int framebuffer)
        {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_PreviousDraw);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_PreviousRead);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        }

        ~ScopedFramebufferBinding()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_PreviousDraw));
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_PreviousRead));
        }

        ScopedFramebufferBinding(const ScopedFramebufferBinding&) = delete;
        ScopedFramebufferBinding& operator=(const ScopedFramebufferBinding&) = delete;

    private:
        GLint m_PreviousDraw = 0;
        GLint m_PreviousRead = 0;
    };
}

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
    const ScopedFramebufferBinding binding(EnsureCreated());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_TEXTURE_2D, texture.GetID(), 0);
}

void Framebuffer::AttachDepthTexture(const Texture2D& texture) const
{
    const ScopedFramebufferBinding binding(EnsureCreated());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture.GetID(), 0);
}

void Framebuffer::CreateDepthRenderbuffer(int width, int height)
{
    const ScopedFramebufferBinding binding(EnsureCreated());

    if (!m_DepthRenderbuffer)
    {
        glGenRenderbuffers(1, &m_DepthRenderbuffer);
    }

    GLint previousRenderbuffer = 0;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_DepthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_DepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previousRenderbuffer));
}

void Framebuffer::SetDrawBuffers(unsigned int count) const
{
    // Draw-buffer state belongs to the framebuffer object, so it survives the rebind below.
    const ScopedFramebufferBinding binding(EnsureCreated());

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
    const ScopedFramebufferBinding binding(EnsureCreated());
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}
