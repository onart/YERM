#version 450

layout(location = 0) in vec2 tc;

layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D tex;

layout(std140, push_constant) uniform ui{
    mat4 aspect;
    float t;
};

void main() {
    outColor = texture(tex, tc);
    outColor.a *= t;
}