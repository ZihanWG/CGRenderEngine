#version 330 core

out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uImage;
uniform int uHorizontal;

void main()
{
    const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec2 texelSize = 1.0 / vec2(textureSize(uImage, 0));

    vec3 result = texture(uImage, vTexCoord).rgb * weights[0];
    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = uHorizontal == 1
            ? vec2(texelSize.x * float(i), 0.0)
            : vec2(0.0, texelSize.y * float(i));

        result += texture(uImage, vTexCoord + offset).rgb * weights[i];
        result += texture(uImage, vTexCoord - offset).rgb * weights[i];
    }

    FragColor = vec4(result, 1.0);
}
