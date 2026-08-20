#version 330 core
// Final post process: debug-view selection, tone mapping, vignette, and optional split compare.

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
uniform int uEnableFxaa;
uniform vec2 uInverseResolution;

vec3 ToneMap(vec3 color)
{
    // ACES-style fit keeps highlights controlled while staying inexpensive.
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

float Luma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 SampleRealtime(vec2 uv)
{
    return ToneMap(texture(uSceneColor, uv).rgb + texture(uBloomColor, uv).rgb);
}

vec3 ApplyFxaa(vec2 uv)
{
    vec3 rgbM = SampleRealtime(uv);
    vec3 rgbNW = SampleRealtime(uv + vec2(-1.0, -1.0) * uInverseResolution);
    vec3 rgbNE = SampleRealtime(uv + vec2( 1.0, -1.0) * uInverseResolution);
    vec3 rgbSW = SampleRealtime(uv + vec2(-1.0,  1.0) * uInverseResolution);
    vec3 rgbSE = SampleRealtime(uv + vec2( 1.0,  1.0) * uInverseResolution);

    float lumaM = Luma(rgbM);
    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    if (lumaMax - lumaMin < max(0.0312, lumaMax * 0.125))
    {
        return rgbM;
    }

    vec2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.03125, 0.0078125);
    float inverseDirectionMin = 1.0 / (min(abs(direction.x), abs(direction.y)) + directionReduce);
    direction = clamp(direction * inverseDirectionMin, vec2(-8.0), vec2(8.0)) * uInverseResolution;

    vec3 rgbA = 0.5 * (
        SampleRealtime(uv + direction * (1.0 / 3.0 - 0.5)) +
        SampleRealtime(uv + direction * (2.0 / 3.0 - 0.5))
    );
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        SampleRealtime(uv + direction * -0.5) +
        SampleRealtime(uv + direction * 0.5)
    );
    float lumaB = Luma(rgbB);
    return (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;
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
        // Depth is linearized so the debug output is visually meaningful.
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

    realtimeColor = uEnableFxaa == 1 ? ApplyFxaa(vTexCoord) : ToneMap(realtimeColor);
    referenceColor = ToneMap(referenceColor);

    float vignette = Vignette(vTexCoord);
    realtimeColor *= vignette;
    referenceColor *= vignette;

    vec3 color = vTexCoord.x < uSplitPosition ? realtimeColor : referenceColor;

    FragColor = vec4(color, 1.0);
}
