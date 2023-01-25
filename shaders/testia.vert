#version 450
// #extension GL_KHR_vulkan_glsl: enable

const vec2 pos[3] = {vec2(-1, -1), vec2(-1, 3), vec2(3, -1)};
const vec2 tc[3] = {vec2(0, 0), vec2(0, 2), vec2(2, 0)};

void main() {
    gl_Position = vec4(pos[gl_VertexIndex], 0.0f, 1.0f);
}
