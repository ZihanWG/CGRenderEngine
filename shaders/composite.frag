#version 330 core

out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uSceneColor;
uniform sampler2D uBloomColor;
uniform sampler2D uReferenceColor;
uniform float uExposure;
uniform float uSplitPosition;

vec3 ToneMap(vec3 color)
{
    color *= uExposure;
    color = (color * (2.51 * color + 0.03)) / (color * (2.43 * color + 0.59) + 0.14);
    color = clamp(color, 0.0, 1.0);
    return pow(color, vec3(1.0 / 2.2));
}

float Vignette(vec2 uv)
{
    vec2 centered = uv * (1.0 - uv);
    float amount = centered.x * centered.y * 18.0;
    return clamp(pow(amount, 0.18), 0.0, 1.0);
}

void main()
{
    vec3 realtimeColor = texture(uSceneColor, vTexCoord).rgb + texture(uBloomColor, vTexCoord).rgb;
    vec3 referenceColor = texture(uReferenceColor, vTexCoord).rgb;

    realtimeColor = ToneMap(realtimeColor);
    referenceColor = ToneMap(referenceColor);

    float vignette = Vignette(vTexCoord);
    realtimeColor *= vignette;
    referenceColor *= vignette;

    vec3 color = vTexCoord.x < uSplitPosition ? realtimeColor : referenceColor;

    if (abs(vTexCoord.x - uSplitPosition) < 0.0015)
    {
        color = mix(color, vec3(1.0, 0.9, 0.25), 0.9);
    }

    FragColor = vec4(color, 1.0);
}
