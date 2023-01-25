#version 450
// #extension GL_KHR_vulkan_glsl: enable

const vec2 pos[3] = {vec2(-1, -1), vec2(-1, 3), vec2(3, -1)};
const vec2 texc[3] = {vec2(0, 0), vec2(0, 2), vec2(2, 0)};

layout(location = 0) out vec2 tc;

void main() {
    gl_Position = vec4(pos[gl_VertexIndex], 0.0f, 1.0f);
    tc = texc[gl_VertexIndex];
}
