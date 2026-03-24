#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out VS_OUT
{
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoord;
    vec4 FragPosLightSpace;
} vs_out;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpaceMatrix;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));

    vs_out.WorldPos = worldPosition.xyz;
    vs_out.Normal = normalize(normalMatrix * aNormal);
    vs_out.TexCoord = aTexCoord;
    vs_out.FragPosLightSpace = uLightSpaceMatrix * worldPosition;

    gl_Position = uProjection * uView * worldPosition;
}
