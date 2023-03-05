#version 450

layout(location = 0) out vec4 outColor;

layout(input_attachment_index = 0, binding = 0) uniform subpassInput inColor;

void main() {
    vec4 inp = subpassLoad(inColor);
    outColor = vec4(sqrt(inp.xyz), inp.w);
}