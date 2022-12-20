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
#ifndef __YR_VULKAN_H__
#define __YR_VULKAN_H__

#include "../externals/vulkan/vulkan.h"

namespace onart {

    class Window;

    /// @brief Vulkan 그래픽스 컨텍스트를 초기화하며, 모델, 텍스처, 셰이더 등 각종 객체는 이 VkMachine으로부터 생성할 수 있습니다.
    class VkMachine{
        friend class Game;
        friend class Game2;
        public:
            /// @brief Vulkan 확인 계층을 사용하려면 이것을 활성화해 주세요. 사용하려면 Vulkan "SDK"가 컴퓨터에 깔려 있어야 합니다.
            constexpr static bool USE_VALIDATION_LAYER = false;
        private:
            /// @brief 기본 Vulkan 컨텍스트를 생성합니다. 이 객체를 생성하면 기본적으로 인스턴스, 물리 장치, 가상 장치, 창 표면이 생성됩니다.
            VkMachine(Window*);
            /// @brief 2D에 최적화된 Vulkan 컨텍스트를 생성합니다.
            VkMachine(Window*, int);
            /// @brief 이 클래스 객체는 Game, Game2 밖에서는 생성, 소멸 호출이 불가능합니다.
            void operator delete(void*){}
        private:
            static VkMachine* singleton;
            VkInstance instance = nullptr;
            VkSurfaceKHR surface = 0;
            VkPhysicalDevice card = nullptr;
            VkDevice device = nullptr;
            VkQueue graphicsQueue = nullptr;
            VkQueue presentQueue = nullptr;
    };
}


#endif