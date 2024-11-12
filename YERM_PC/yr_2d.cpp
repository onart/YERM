// Copyright 2022 onart@github. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "yr_2d.h"

namespace onart{
    YRGraphics::pPipeline get2DDefaultPipeline() {
        static int32_t _2dppid = INT32_MIN;
        if (_2dppid == INT32_MIN) {
            _2dppid = YRGraphics::issuePipelineKey();

            YRGraphics::PipelineCreationOptions opts;

            opts.alphaBlend[0] = AlphaBlend::normal();
            opts.depthStencil.depthTest = false;
            opts.depthStencil.depthWrite = false;
            opts.depthStencil.stencilTest = false;
            opts.depthStencil.stencilFront.writeMask = 0;
            opts.depthStencil.stencilBack.writeMask = 0;

            YRGraphics::pRenderPass temp = YRGraphics::createRenderPass(INT32_MIN, { 4,4 });
            opts.pass = temp.get();

            opts.shaderResources.usePush = true;
            opts.shaderResources.pos0 = ShaderResourceType::UNIFORM_BUFFER_1;
            opts.shaderResources.pos1 = ShaderResourceType::DYNAMIC_UNIFORM_BUFFER_1;
            opts.shaderResources.pos2 = ShaderResourceType::TEXTURE_1;

            opts.instanceAttributeCount = 0;
            using _2dvertex_t = YRGraphics::Vertex<float[2], float[2]>;
            opts.vertexAttributeCount = 2;
            opts.vertexSize = sizeof(_2dvertex_t);

            YRGraphics::PipelineInputVertexSpec vspec[2];
            _2dvertex_t::info(vspec, 0, 0);
            opts.vertexSpec = vspec;

            /*
            
            */
            if constexpr (YRGraphics::VULKAN_GRAPHICS) {
                /*
                #version 450
                // #extension GL_KHR_vulkan_glsl: enable

                layout(location = 0) in vec2 inPosition;
                layout(location = 1) in vec2 inTc;

                layout(location = 0) out vec2 tc;

                layout(std140, set = 0, binding = 0) uniform PerFrame {
                    mat4 viewProjection;
                };
                
                layout(std140, push_constant) uniform ui{
                    mat4 model;
                    vec4 texrect;
                    vec4 pad[3];
                };
                
                void main() {
                    gl_Position = vec4(inPosition, 0.0, 1.0) * model * viewProjection;
                    tc = inTc * texrect.xy + texrect.zw;
                }
                */
                const uint32_t _2DVS[354] = { 119734787,65536,851979,58,0,131089,1,393227,1,1280527431,1685353262,808793134,0,196622,0,1,589839,0,4,1852399981,0,13,18,45,46,327752,11,0,11,0,327752,11,1,11,1,327752,11,2,11,3,327752,11,3,11,4,196679,11,2,262215,18,30,0,262215,27,6,16,262216,28,0,5,327752,28,0,35,0,327752,28,0,7,16,327752,28,1,35,64,327752,28,2,35,80,196679,28,2,262216,35,0,5,327752,35,0,35,0,327752,35,0,7,16,196679,35,2,262215,37,34,0,262215,37,33,0,262215,45,30,0,262215,46,30,1,131091,2,196641,3,2,196630,6,32,262167,7,6,4,262165,8,32,0,262187,8,9,1,262172,10,6,9,393246,11,7,6,10,10,262176,12,3,11,262203,12,13,3,262165,14,32,1,262187,14,15,0,262167,16,6,2,262176,17,1,16,262203,17,18,1,262187,6,20,0,262187,6,21,1065353216,262168,25,7,4,262187,8,26,3,262172,27,7,26,327710,28,25,7,27,262176,29,9,28,262203,29,30,9,262176,31,9,25,196638,35,25,262176,36,2,35,262203,36,37,2,262176,38,2,25,262176,42,3,7,262176,44,3,16,262203,44,45,3,262203,17,46,1,262187,14,48,1,262176,49,9,7,327734,2,4,0,3,131320,5,262205,16,19,18,327761,6,22,19,0,327761,6,23,19,1,458832,7,24,22,23,20,21,327745,31,32,30,15,262205,25,33,32,327824,7,34,24,33,327745,38,39,37,15,262205,25,40,39,327824,7,41,34,40,327745,42,43,13,15,196670,43,41,262205,16,47,46,327745,49,50,30,48,262205,7,51,50,458831,16,52,51,51,0,1,458831,16,56,51,51,2,3,524300,16,57,1,50,47,52,56,196670,45,57,65789,65592 };
                /*
                #version 450
                
                layout(location = 0) in vec2 tc;

                layout(location = 0) out vec4 outColor;
                layout(set = 2, binding = 0) uniform sampler2D tex;

                layout(std140, set = 0, binding = 0) uniform PerFrame {
                    mat4 viewProjection;
                };
                
                layout(std140, push_constant) uniform ui{
                    vec4 pad[5];
                    vec4 color;
                    vec4 pad2[2];
                };
                
                void main() {
                    outColor = texture(tex, tc) * color;
                }
                */
                const uint32_t _2DFS[204] = { 119734787,65536,851979,34,0,131089,1,393227,1,1280527431,1685353262,808793134,0,196622,0,1,458767,4,4,1852399981,0,9,17,196624,4,7,262215,9,30,0,262215,13,34,2,262215,13,33,0,262215,17,30,0,262215,22,6,16,262215,24,6,16,327752,25,0,35,0,327752,25,1,35,80,327752,25,2,35,96,196679,25,2,131091,2,196641,3,2,196630,6,32,262167,7,6,4,262176,8,3,7,262203,8,9,3,589849,10,6,1,0,0,0,1,0,196635,11,10,262176,12,0,11,262203,12,13,0,262167,15,6,2,262176,16,1,15,262203,16,17,1,262165,20,32,0,262187,20,21,5,262172,22,7,21,262187,20,23,2,262172,24,7,23,327710,25,22,7,24,262176,26,9,25,262203,26,27,9,262165,28,32,1,262187,28,29,1,262176,30,9,7,327734,2,4,0,3,131320,5,262205,11,14,13,262205,15,18,17,327767,7,19,14,18,327745,30,31,27,29,262205,7,32,31,327813,7,33,19,32,196670,9,33,65789,65592 };
                int32_t vshk = YRGraphics::issueShaderKey();
                opts.vertexShader = YRGraphics::createShader(vshk, { _2DVS, sizeof(_2DVS), ShaderStage::VERTEX });
                int32_t fshk = YRGraphics::issueShaderKey();
                opts.fragmentShader = YRGraphics::createShader(fshk, { _2DFS, sizeof(_2DFS), ShaderStage::FRAGMENT });
            }
            else if constexpr (YRGraphics::OPENGL_GRAPHICS) {
                const char _2DVS[] = R"(
#version 450
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTc;
layout(location = 0) out vec2 tc;
layout(std140, binding = 0) uniform PerFrame {
    mat4 viewProjection;
};
layout(std140, binding=11) uniform ui{
    mat4 model;
    vec4 texrect;
    vec4 color;
    vec4 pad[2];
};
void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0) * model * viewProjection;
    tc = inTc * texrect.xy + texrect.zw;
}
)";
                const char _2DFS[] = R"(
#version 450
layout(location = 0) in vec2 tc;
out vec4 outColor;
layout(binding = 0) uniform sampler2D tex;
layout(std140, binding=11) uniform ui{
    mat4 model;
    vec4 texrect;
    vec4 color;
    vec4 pad[2];
};
void main() {
    outColor = texture(tex, tc) * color;
}
)";
                int32_t vshk = YRGraphics::issueShaderKey();
                opts.vertexShader = YRGraphics::createShader(vshk, { _2DVS, sizeof(_2DVS), ShaderStage::VERTEX });
                int32_t fshk = YRGraphics::issueShaderKey();
                opts.fragmentShader = YRGraphics::createShader(fshk, { _2DFS, sizeof(_2DFS), ShaderStage::FRAGMENT });
            }
            else if constexpr (YRGraphics::D3D11_GRAPHICS) {
                /*
                struct PS_INPUT{
                    float4 pos: SV_POSITION;
                    float2 tc: TEXCOORD0;
                };
                cbuffer _perFrame: register(b0){
                    float4x4 viewProjection;
                }
                cbuffer _0: register(b13){
                    float4x4 model;
                    float4 texrect;
                    float4 pad[3];
                }

                PS_INPUT main(float2 inPosition: _0_, float2 inTc: _1_) {
                    PS_INPUT ret = (PS_INPUT)0;
                    ret.pos = mul(float4(inPosition, 0.0, 1.0), model);
                    ret.pos = mul(ret.pos, viewProjection);
                    ret.tc = inTc * texrect.xy + texrect.zw;
                    return ret;
                }
                */
                const uint8_t _2DVS[1380] = { 68,88,66,67,211,186,67,37,146,36,38,10,226,47,202,255,252,156,82,7,1,0,0,0,100,5,0,0,5,0,0,0,52,0,0,0,96,2,0,0,168,2,0,0,0,3,0,0,200,4,0,0,82,68,69,70,36,2,0,0,2,0,0,0,140,0,0,0,2,0,0,0,60,0,0,0,0,5,254,255,0,1,0,0,252,1,0,0,82,68,49,49,60,0,0,0,24,0,0,0,32,0,0,0,40,0,0,0,36,0,0,0,12,0,0,0,0,0,0,0,124,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,134,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,13,0,0,0,1,0,0,0,1,0,0,0,95,112,101,114,70,114,97,109,101,0,95,48,0,171,171,171,124,0,0,0,1,0,0,0,188,0,0,0,64,0,0,0,0,0,0,0,0,0,0,0,134,0,0,0,3,0,0,0,32,1,0,0,128,0,0,0,0,0,0,0,0,0,0,0,228,0,0,0,0,0,0,0,64,0,0,0,2,0,0,0,252,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,118,105,101,119,80,114,111,106,101,99,116,105,111,110,0,102,108,111,97,116,52,120,52,0,3,0,3,0,4,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,243,0,0,0,152,1,0,0,0,0,0,0,64,0,0,0,2,0,0,0,252,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,158,1,0,0,64,0,0,0,16,0,0,0,2,0,0,0,176,1,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,212,1,0,0,80,0,0,0,48,0,0,0,0,0,0,0,216,1,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,109,111,100,101,108,0,116,101,120,114,101,99,116,0,102,108,111,97,116,52,0,171,171,171,1,0,3,0,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,1,0,0,112,97,100,0,1,0,3,0,1,0,4,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,166,1,0,0,77,105,99,114,111,115,111,102,116,32,40,82,41,32,72,76,83,76,32,83,104,97,100,101,114,32,67,111,109,112,105,108,101,114,32,49,48,46,49,0,73,83,71,78,64,0,0,0,2,0,0,0,8,0,0,0,56,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,3,3,0,0,60,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,1,0,0,0,3,3,0,0,95,48,95,0,95,49,95,0,79,83,71,78,80,0,0,0,2,0,0,0,8,0,0,0,56,0,0,0,0,0,0,0,1,0,0,0,3,0,0,0,0,0,0,0,15,0,0,0,68,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,1,0,0,0,3,12,0,0,83,86,95,80,79,83,73,84,73,79,78,0,84,69,88,67,79,79,82,68,0,171,171,171,83,72,69,88,192,1,0,0,80,0,1,0,112,0,0,0,106,8,0,1,89,0,0,4,70,142,32,0,0,0,0,0,4,0,0,0,89,0,0,4,70,142,32,0,13,0,0,0,5,0,0,0,95,0,0,3,50,16,16,0,0,0,0,0,95,0,0,3,50,16,16,0,1,0,0,0,103,0,0,4,242,32,16,0,0,0,0,0,1,0,0,0,101,0,0,3,50,32,16,0,1,0,0,0,104,0,0,2,2,0,0,0,54,0,0,5,50,0,16,0,0,0,0,0,70,16,16,0,0,0,0,0,54,0,0,5,66,0,16,0,0,0,0,0,1,64,0,0,0,0,128,63,16,0,0,8,18,0,16,0,1,0,0,0,70,2,16,0,0,0,0,0,70,131,32,0,13,0,0,0,0,0,0,0,16,0,0,8,34,0,16,0,1,0,0,0,70,2,16,0,0,0,0,0,70,131,32,0,13,0,0,0,1,0,0,0,16,0,0,8,66,0,16,0,1,0,0,0,70,2,16,0,0,0,0,0,70,131,32,0,13,0,0,0,2,0,0,0,16,0,0,8,130,0,16,0,1,0,0,0,70,2,16,0,0,0,0,0,70,131,32,0,13,0,0,0,3,0,0,0,17,0,0,8,18,32,16,0,0,0,0,0,70,14,16,0,1,0,0,0,70,142,32,0,0,0,0,0,0,0,0,0,17,0,0,8,34,32,16,0,0,0,0,0,70,14,16,0,1,0,0,0,70,142,32,0,0,0,0,0,1,0,0,0,17,0,0,8,66,32,16,0,0,0,0,0,70,14,16,0,1,0,0,0,70,142,32,0,0,0,0,0,2,0,0,0,17,0,0,8,130,32,16,0,0,0,0,0,70,14,16,0,1,0,0,0,70,142,32,0,0,0,0,0,3,0,0,0,50,0,0,11,50,32,16,0,1,0,0,0,70,16,16,0,1,0,0,0,70,128,32,0,13,0,0,0,4,0,0,0,230,138,32,0,13,0,0,0,4,0,0,0,62,0,0,1,83,84,65,84,148,0,0,0,12,0,0,0,2,0,0,0,0,0,0,0,4,0,0,0,9,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
                /*
                struct PS_INPUT{
                    float4 pos: SV_POSITION;
                    float2 tc: TEXCOORD0;
                };

                Texture2D tex: register(t0);
                SamplerState spr: register(s0);

                cbuffer _0: register(b13){
                    float4 pad[5];
                    float4 color;
                    float4 pad2[2];
                }

                float4 main(PS_INPUT input): SV_TARGET {
                    return tex.Sample(spr, input.tc) * color;
                }
                */
                const uint8_t _2DFS[1020] = { 68,88,66,67,13,109,139,117,65,189,41,209,179,191,96,24,203,213,6,161,1,0,0,0,252,3,0,0,5,0,0,0,52,0,0,0,36,2,0,0,124,2,0,0,176,2,0,0,96,3,0,0,82,68,69,70,232,1,0,0,1,0,0,0,168,0,0,0,3,0,0,0,60,0,0,0,0,5,255,255,0,129,0,0,192,1,0,0,82,68,49,49,60,0,0,0,24,0,0,0,32,0,0,0,40,0,0,0,36,0,0,0,12,0,0,0,0,0,0,0,156,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,160,0,0,0,2,0,0,0,5,0,0,0,4,0,0,0,255,255,255,255,0,0,0,0,1,0,0,0,13,0,0,0,164,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,13,0,0,0,1,0,0,0,1,0,0,0,115,112,114,0,116,101,120,0,95,48,0,171,164,0,0,0,3,0,0,0,192,0,0,0,128,0,0,0,0,0,0,0,0,0,0,0,56,1,0,0,0,0,0,0,80,0,0,0,0,0,0,0,68,1,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,104,1,0,0,80,0,0,0,16,0,0,0,2,0,0,0,112,1,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,148,1,0,0,96,0,0,0,32,0,0,0,0,0,0,0,156,1,0,0,0,0,0,0,255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0,112,97,100,0,102,108,111,97,116,52,0,171,1,0,3,0,1,0,4,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,1,0,0,99,111,108,111,114,0,171,171,1,0,3,0,1,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,1,0,0,112,97,100,50,0,171,171,171,1,0,3,0,1,0,4,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,60,1,0,0,77,105,99,114,111,115,111,102,116,32,40,82,41,32,72,76,83,76,32,83,104,97,100,101,114,32,67,111,109,112,105,108,101,114,32,49,48,46,49,0,73,83,71,78,80,0,0,0,2,0,0,0,8,0,0,0,56,0,0,0,0,0,0,0,1,0,0,0,3,0,0,0,0,0,0,0,15,0,0,0,68,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,1,0,0,0,3,3,0,0,83,86,95,80,79,83,73,84,73,79,78,0,84,69,88,67,79,79,82,68,0,171,171,171,79,83,71,78,44,0,0,0,1,0,0,0,8,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,15,0,0,0,83,86,95,84,65,82,71,69,84,0,171,171,83,72,69,88,168,0,0,0,80,0,0,0,42,0,0,0,106,8,0,1,89,0,0,4,70,142,32,0,13,0,0,0,6,0,0,0,90,0,0,3,0,96,16,0,0,0,0,0,88,24,0,4,0,112,16,0,0,0,0,0,85,85,0,0,98,16,0,3,50,16,16,0,1,0,0,0,101,0,0,3,242,32,16,0,0,0,0,0,104,0,0,2,1,0,0,0,69,0,0,139,194,0,0,128,67,85,21,0,242,0,16,0,0,0,0,0,70,16,16,0,1,0,0,0,70,126,16,0,0,0,0,0,0,96,16,0,0,0,0,0,56,0,0,8,242,32,16,0,0,0,0,0,70,14,16,0,0,0,0,0,70,142,32,0,13,0,0,0,5,0,0,0,62,0,0,1,83,84,65,84,148,0,0,0,3,0,0,0,1,0,0,0,0,0,0,0,2,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
                int32_t vshk = YRGraphics::issueShaderKey();
                opts.vertexShader = YRGraphics::createShader(vshk, { _2DVS, sizeof(_2DVS), ShaderStage::VERTEX });
                int32_t fshk = YRGraphics::issueShaderKey();
                opts.fragmentShader = YRGraphics::createShader(fshk, { _2DFS, sizeof(_2DFS), ShaderStage::FRAGMENT });
                opts.vsByteCode = _2DVS;
                opts.vsByteCodeSize = sizeof(_2DVS);
            }
            else if constexpr (YRGraphics::OPENGLES_GRAPHICS) {
                const char _2DVS[] = R"(
#version 300 es
precision mediump float;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTc;

out vec2 tc;
layout(std140) uniform PerFrame {
    mat4 viewProjection;
};
layout(std140) uniform push{
    mat4 model;
    vec4 texrect;
    vec4 color;
    vec4 pad[2];
};
void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0) * model * viewProjection;
    tc = inTc * texrect.xy + texrect.zw;
}
)";
                const char _2DFS[] = R"(
#version 300 es
precision mediump float;

in vec2 tc;
out vec4 outColor;
uniform sampler2D tex;
layout(std140) uniform push{
    mat4 model;
    vec4 texrect;
    vec4 color;
    vec4 pad[2];
};
void main() {
    outColor = texture(tex, tc) * color;
}
)";
                R"(#version 300 es
precision mediump float;

in vec2 tc;

out vec4 outColor;
uniform sampler2D tex;

uniform push{
    mat4 aspect;
    float t;
};

void main() {
    outColor = texture(tex, tc);
}
)";
                int32_t vshk = YRGraphics::issueShaderKey();
                opts.vertexShader = YRGraphics::createShader(vshk, { _2DVS, sizeof(_2DVS), ShaderStage::VERTEX });
                int32_t fshk = YRGraphics::issueShaderKey();
                opts.fragmentShader = YRGraphics::createShader(fshk, { _2DFS, sizeof(_2DFS), ShaderStage::FRAGMENT });
            }
            else {
                static_assert(YRGraphics::VULKAN_GRAPHICS || YRGraphics::D3D11_GRAPHICS || YRGraphics::OPENGL_GRAPHICS || YRGraphics::OPENGLES_GRAPHICS, "Not ready");
            }
            YRGraphics::createPipeline(_2dppid, opts);
        }
        return YRGraphics::getPipeline(_2dppid);
    }
}