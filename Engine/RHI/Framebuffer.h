// Small framebuffer wrapper used by the render passes to attach textures and validate setup.
#pragma once

class Texture2D;

class Framebuffer
{
public:
    // Construction makes no GL call; the framebuffer name is created on first use.
    // That lets a Framebuffer be a member of an object built before the GL context exists.
    Framebuffer() = default;
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    void Bind() const;
    static void Unbind();

    void AttachColorTexture(const Texture2D& texture, unsigned int index) const;
    void AttachDepthTexture(const Texture2D& texture) const;
    void CreateDepthRenderbuffer(int width, int height);
    // The draw buffer list must match the currently attached color targets.
    void SetDrawBuffers(unsigned int count) const;
    bool CheckComplete() const;

private:
    void Release() noexcept;
    unsigned int EnsureCreated() const;

    // Mutable so the const binding helpers can create the name lazily on first use.
    mutable unsigned int m_ID = 0;
    unsigned int m_DepthRenderbuffer = 0;
};
