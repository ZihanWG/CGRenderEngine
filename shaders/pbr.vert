#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec4 aTangent;

out VS_OUT
{
    vec3 WorldPos;
    vec3 Normal;
    vec3 WorldTangent;
    float TangentSign;
    vec2 TexCoord;
    vec4 FragPosLightSpace;
} vs_out;

layout (std140) uniform FrameData
{
    mat4 uView;
    mat4 uProjection;
    mat4 uLightSpaceMatrix;
    mat4 uInverseViewProjection;
    vec4 uCameraPosition;
    vec4 uEnvironmentData;
};

uniform mat4 uModel;

void main()
{
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    mat3 normalMatrix = transpose(inverse(mat3(uModel)));
    mat3 modelMatrix = mat3(uModel);
    vec3 worldNormal = normalize(normalMatrix * aNormal);
    vec3 worldTangent = modelMatrix * aTangent.xyz;
    float tangentLength = length(worldTangent);

    if (tangentLength > 1e-6 && abs(aTangent.w) > 0.5)
    {
        worldTangent = normalize(worldTangent - worldNormal * dot(worldNormal, worldTangent));
        vs_out.WorldTangent = worldTangent;
        vs_out.TangentSign = aTangent.w;
    }
    else
    {
        vs_out.WorldTangent = vec3(0.0);
        vs_out.TangentSign = 0.0;
    }

    vs_out.WorldPos = worldPosition.xyz;
    vs_out.Normal = worldNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.FragPosLightSpace = uLightSpaceMatrix * worldPosition;

    gl_Position = uProjection * uView * worldPosition;
}
