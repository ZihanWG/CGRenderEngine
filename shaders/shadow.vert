#version 330 core
// Shadow-map vertex shader. Only position and transforms matter for the depth pass.

layout (location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;

void main()
{
    gl_Position = uLightSpaceMatrix * uModel * vec4(aPosition, 1.0);
}
