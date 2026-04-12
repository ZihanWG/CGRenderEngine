#version 330 core

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;
layout (location = 2) out vec4 AlbedoColor;
layout (location = 3) out vec4 NormalColor;
layout (location = 4) out vec4 MaterialColor;

in vec2 vTexCoord;

layout (std140) uniform FrameData
{
    mat4 uView;
    mat4 uProjection;
    mat4 uLightSpaceMatrix;
    mat4 uInverseViewProjection;
    vec4 uCameraPosition;
    vec4 uEnvironmentData;
};

uniform sampler2D uEnvironmentMap;

const float PI = 3.14159265359;

vec2 DirectionToLatLong(vec3 direction)
{
    float rotationRadians = radians(uEnvironmentData.z);
    float cosRotation = cos(rotationRadians);
    float sinRotation = sin(rotationRadians);
    vec3 dir = normalize(vec3(
        direction.x * cosRotation - direction.z * sinRotation,
        direction.y,
        direction.x * sinRotation + direction.z * cosRotation
    ));
    float phi = atan(dir.z, dir.x);
    float theta = acos(clamp(dir.y, -1.0, 1.0));
    return vec2(phi / (2.0 * PI) + 0.5, theta / PI);
}

void main()
{
    vec2 ndc = vTexCoord * 2.0 - 1.0;
    vec4 clipPosition = vec4(ndc, 1.0, 1.0);
    vec4 worldPosition = uInverseViewProjection * clipPosition;
    vec3 worldDirection = normalize(worldPosition.xyz / worldPosition.w - uCameraPosition.xyz);

    vec3 color = texture(uEnvironmentMap, DirectionToLatLong(worldDirection)).rgb * uEnvironmentData.x;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    FragColor = vec4(color, 1.0);
    BrightColor = vec4(brightness > 1.0 ? color : vec3(0.0), 1.0);
    AlbedoColor = vec4(0.0, 0.0, 0.0, 1.0);
    NormalColor = vec4(worldDirection * 0.5 + 0.5, 1.0);
    MaterialColor = vec4(0.0, 1.0, 1.0, 1.0);
}
