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
#ifndef __YR_GRAPHICS_H__
#define __YR_GRAPHICS_H__

#include "../externals/boost/predef/platform.h"

#ifdef YR_USE_D3D12
#error "D3D12 not ready"
// 
#elif defined(YR_USE_D3D11)
#include "yr_d3d11.h"
namespace onart {
    using YRGraphics = D3D11Machine;
    using pipeline_t = D3D11Machine::Pipeline*;
    using shader_t = ID3D11DeviceChild*;
}
#elif defined(YR_USE_OPENGL)
#include "yr_opengl.h"
namespace onart{
    using YRGraphics = GLMachine;
    using pipeline_t = GLMachine::Pipeline*;
    using shader_t = unsigned;
}
#elif defined(YR_USE_GLES)
#error "OpenGL ES not ready"
//
#elif defined(YR_USE_METAL)
#error "Metal not ready"
//
#elif defined(YR_USE_WEBGPU)
#include "../YERM_webgpu/yr_webgpu.h"
namespace onart{
    using YRGraphics = WGMachine;
    using pipeline_t = VkPipeline;
    using shader_t = VkShaderModule;
}
#else
#include "yr_vulkan.h"
namespace onart{
    using YRGraphics = VkMachine;
    using pipeline_t = VkMachine::Pipeline*;
    using shader_t = VkShaderModule;
}
#endif

#endif