#version 330 core
// Shadow-map vertex shader. Only position and transforms matter for the depth pass.

layout (location = 0) in vec3 aPosition;
uniform mat4 uLightSpaceMatrix;

layout (std140) uniform ObjectData
{
    mat4 uModelMatrices[128];
};

void main()
{
    mat4 modelMatrix = uModelMatrices[gl_InstanceID];
    gl_Position = uLightSpaceMatrix * modelMatrix * vec4(aPosition, 1.0);
}
