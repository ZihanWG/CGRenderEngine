#version 330 core
// Depth-only shadow pass with glTF alpha-mask support.

in vec2 vTexCoord;
uniform sampler2D uBaseColorTexture;
uniform float uBaseColorFactorAlpha;
uniform float uAlphaCutoff;
uniform int uHasBaseColorTexture;

void main()
{
    if (uAlphaCutoff > 0.0)
    {
        float alpha = uBaseColorFactorAlpha;
        if (uHasBaseColorTexture == 1)
        {
            alpha *= texture(uBaseColorTexture, vTexCoord).a;
        }
        if (alpha < uAlphaCutoff)
        {
            discard;
        }
    }
}
