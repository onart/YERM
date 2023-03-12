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
#ifndef __YR_CONSTANTS_HPP__
#define __YR_CONSTANTS_HPP__

#include <cstdint>

extern const uint32_t TEST_FRAG[207];
extern const uint32_t TEST_VERT[252];
extern const uint32_t TEST_IA_FRAG[163];
extern const uint32_t TEST_IA_VERT[219];
extern const uint32_t TEST_TX_VERT[276];
extern const uint32_t TEST_TX_FRAG[119];
extern const uint8_t SCALEPX[4932];

extern const uint8_t TEX0[63239];

namespace onart{
    /// @brief 엔진의 그래픽스 타입을 빌드 전에 지정합니다.
    enum class GRAPHICS_TYPE {
        /// @brief 엔진이 2D 게임에 최적화됩니다.
        _2D,
        /// @brief 엔진이 2D 게임에 최적화되지 않습니다.
        _3D
    };
}

#endif