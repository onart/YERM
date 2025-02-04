#version 450
// #extension GL_KHR_vulkan_glsl: enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTc;

layout(location = 0) out vec2 tc;

layout(std140, push_constant) uniform ui{
    mat4 aspect;
    float t;
};

void main() {
    gl_Position = vec4(inPosition, 1.0) * aspect;
    tc = inTc;
}