#pragma once

class Texture2D;

class Framebuffer
{
public:
    Framebuffer();
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    void Bind() const;
    static void Unbind();

    void AttachColorTexture(const Texture2D& texture, unsigned int index) const;
    void AttachDepthTexture(const Texture2D& texture) const;
    void CreateDepthRenderbuffer(int width, int height);
    void SetDrawBuffers(unsigned int count) const;
    bool CheckComplete() const;

private:
    unsigned int m_ID = 0;
    unsigned int m_DepthRenderbuffer = 0;
};
