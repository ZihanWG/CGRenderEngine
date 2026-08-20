#version 330 core
// Shadow-map vertex shader. Only position and transforms matter for the depth pass.

layout (location = 0) in vec3 aPosition;
layout (location = 2) in vec2 aTexCoord;
uniform mat4 uLightSpaceMatrix;

out vec2 vTexCoord;

layout (std140) uniform ObjectData
{
    mat4 uModelMatrices[128];
};

void main()
{
    mat4 modelMatrix = uModelMatrices[gl_InstanceID];
    vTexCoord = aTexCoord;
    gl_Position = uLightSpaceMatrix * modelMatrix * vec4(aPosition, 1.0);
}
