#version 330 core

out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uSceneColor;
uniform sampler2D uBloomColor;
uniform sampler2D uReferenceColor;
uniform sampler2D uAlbedoColor;
uniform sampler2D uNormalColor;
uniform sampler2D uMaterialColor;
uniform sampler2D uDepthTexture;
uniform sampler2D uShadowMap;
uniform float uExposure;
uniform float uSplitPosition;
uniform int uHasReference;
uniform int uDebugView;

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

float LinearizeDepth(float depth)
{
    const float nearPlane = 0.1;
    const float farPlane = 40.0;
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main()
{
    vec3 sceneColor = texture(uSceneColor, vTexCoord).rgb;
    vec3 bloomColor = texture(uBloomColor, vTexCoord).rgb;
    vec3 realtimeColor = sceneColor + bloomColor;
    vec3 referenceColor = uHasReference == 1
        ? texture(uReferenceColor, vTexCoord).rgb
        : realtimeColor;

    if (uDebugView == 1)
    {
        FragColor = vec4(ToneMap(sceneColor), 1.0);
        return;
    }

    if (uDebugView == 2)
    {
        FragColor = vec4(ToneMap(bloomColor), 1.0);
        return;
    }

    if (uDebugView == 3)
    {
        FragColor = vec4(texture(uAlbedoColor, vTexCoord).rgb, 1.0);
        return;
    }

    if (uDebugView == 4)
    {
        FragColor = vec4(texture(uNormalColor, vTexCoord).rgb, 1.0);
        return;
    }

    if (uDebugView == 5)
    {
        FragColor = vec4(texture(uMaterialColor, vTexCoord).rgb, 1.0);
        return;
    }

    if (uDebugView == 6)
    {
        float depth = texture(uDepthTexture, vTexCoord).r;
        float linearDepth = LinearizeDepth(depth) / 40.0;
        FragColor = vec4(vec3(clamp(linearDepth, 0.0, 1.0)), 1.0);
        return;
    }

    if (uDebugView == 7)
    {
        float shadowDepth = texture(uShadowMap, vTexCoord).r;
        FragColor = vec4(vec3(shadowDepth), 1.0);
        return;
    }

    realtimeColor = ToneMap(realtimeColor);
    referenceColor = ToneMap(referenceColor);

    float vignette = Vignette(vTexCoord);
    realtimeColor *= vignette;
    referenceColor *= vignette;

    vec3 color = vTexCoord.x < uSplitPosition ? realtimeColor : referenceColor;

    FragColor = vec4(color, 1.0);
}
