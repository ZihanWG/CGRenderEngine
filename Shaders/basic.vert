#version 330 core
// Minimal position-only test vertex shader kept for simple smoke tests.
layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos, 1.0);
}
