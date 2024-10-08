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
    using shader_t = ID3D11DeviceChild*;
}
#elif defined(YR_USE_OPENGL)
#include "yr_opengl.h"
namespace onart{
    using YRGraphics = GLMachine;
    using shader_t = unsigned;
}
#elif defined(YR_USE_GLES)
#error "OpenGL ES not ready"
//
#elif defined(YR_USE_METAL)
#error "Metal not ready"
//
#elif defined(YR_USE_WEBGPU)
#include "../YERM_web/yr_webgpu.h"
namespace onart{
    using YRGraphics = WGMachine;
    using shader_t = VkShaderModule;
}
#elif defined(YR_USE_VULKAN)
#include "yr_vulkan.h"
namespace onart{
    using YRGraphics = VkMachine;
    using shader_t = VkShaderModule;
}
#elif defined(YR_USE_WEBGL)
#include "../YERM_web/yr_webgl.h"
namespace onart{
    using YRGraphics = WGLMachine;
    using shader_t = unsigned;
}
#else
static_assert(0, "No Graphics library selected to be linked");
#endif

#endif