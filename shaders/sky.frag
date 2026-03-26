#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;
layout (location = 2) out vec4 AlbedoColor;
layout (location = 3) out vec4 NormalColor;
layout (location = 4) out vec4 MaterialColor;

in vec2 vTexCoord;

uniform mat4 uInverseViewProjection;
uniform vec3 uCameraPosition;
uniform sampler2D uEnvironmentMap;
uniform float uEnvironmentIntensity;

const float PI = 3.14159265359;

vec2 DirectionToLatLong(vec3 direction)
{
    vec3 dir = normalize(direction);
    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, theta / PI);
}

void main()
{
    vec2 ndc = vTexCoord * 2.0 - 1.0;
    vec4 clipPosition = vec4(ndc, 1.0, 1.0);
    vec4 worldPosition = uInverseViewProjection * clipPosition;
    vec3 worldDirection = normalize(worldPosition.xyz / worldPosition.w - uCameraPosition);

    vec3 color = texture(uEnvironmentMap, DirectionToLatLong(worldDirection)).rgb * uEnvironmentIntensity;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    FragColor = vec4(color, 1.0);
    BrightColor = vec4(brightness > 1.0 ? color : vec3(0.0), 1.0);
    AlbedoColor = vec4(0.0, 0.0, 0.0, 1.0);
    NormalColor = vec4(worldDirection * 0.5 + 0.5, 1.0);
    MaterialColor = vec4(0.0, 1.0, 1.0, 1.0);
}
