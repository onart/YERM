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
#ifndef YR_USE_WEBGPU
#error "This project is not configured for using WebGPU. Please re-generate project with CMake or remove yr_webgpu.cpp from project"
#endif
#ifndef __YR_WEBGPU_H__
#define __YR_WEBGPU_H__

#include "yr_string.hpp"
#include "yr_tuple.hpp"

#include "../externals/wasm_webgpu/lib_webgpu.h"

#include "yr_math.hpp"
#include "yr_threadpool.hpp"

#include <type_traits>
#include <vector>
#include <set>
#include <queue>
#include <memory>
#include <map>

#define VERTEX_FLOAT_TYPES float, vec2, vec3, vec4, float[1], float[2], float[3], float[4]
#define VERTEX_DOUBLE_TYPES double, double[1], double[2], double[3], double[4]
#define VERTEX_INT8_TYPES int8_t, int8_t[1], int8_t[2], int8_t[3], int8_t[4]
#define VERTEX_UINT8_TYPES uint8_t, uint8_t[1], uint8_t[2], uint8_t[3], uint8_t[4]
#define VERTEX_INT16_TYPES int16_t, int16_t[1], int16_t[2], int16_t[3], int16_t[4]
#define VERTEX_UINT16_TYPES uint16_t, uint16_t[1], uint16_t[2], uint16_t[3], uint16_t[4]
#define VERTEX_INT32_TYPES int32_t, ivec2, ivec3, ivec4, int32_t[1], int32_t[2], int32_t[3], int32_t[4]
#define VERTEX_UINT32_TYPES uint32_t, uvec2, uvec3, uvec4, uint32_t[1], uint32_t[2], uint32_t[3], uint32_t[4]

#define VERTEX_ATTR_TYPES VERTEX_FLOAT_TYPES, \
                VERTEX_DOUBLE_TYPES, \
                VERTEX_INT8_TYPES, \
                VERTEX_UINT8_TYPES,\
                VERTEX_INT16_TYPES,\
                VERTEX_UINT16_TYPES,\
                VERTEX_INT32_TYPES,\
                VERTEX_UINT32_TYPES

namespace onart {

    class Window;

    class WGMachine{
        friend class Game;
        public:
            constexpr static bool VULKAN_GRAPHICS = false, D3D12_GRAPHICS = false, D3D11_GRAPHICS = false, OPENGL_GRAPHICS = false, OPENGLES_GRAPHICS = false, METAL_GRAPHICS = false, WEBGPU_GRAPHICS = true;
        private:
            WGMachine(Window*);
            static void onGetWebGpuAdapter(WGpuAdapter, void*);
            static void onGetWebGpuDevice(WGpuDevice, void*);
        private:
            WGpuAdapter adapter;
            WGpuDevice device;
            WGpuSupportedLimits limits;
            WGpuCanvasContext canvas;
            WGpuQueue queue;
        public:
            static WGMachine* singleton;
    };
}


#endif // __YR_WEBGPU_H__