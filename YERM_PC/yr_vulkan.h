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
#define VMA_VULKAN_VERSION 1000000
#include "../externals/vulkan/vk_mem_alloc.h"
#include <vector>

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
            /// @brief 그래픽스/전송 명령 버퍼를 풀로부터 할당합니다.
            /// @param count 할당할 수
            /// @param isPrimary true면 주 버퍼, false면 보조 버퍼입니다.
            /// @param buffers 버퍼가 들어갑니다. count 길이 이상의 배열이어야 하며, 할당 실패 시 첫 번째에 nullptr가 들어갑니다.
            void allocateCommandBuffers(int count, bool isPrimary, VkCommandBuffer* buffers);
            /// @brief 창 표면 초기화 이후 호출되어 특성을 파악합니다.
            void checkSurfaceHandle();
            /// @brief 기존 스왑체인을 제거하고 다시 생성하며, 스왑체인에 대한 이미지뷰도 가져옵니다.
            void createSwapchain(uint32_t width, uint32_t height, uint32_t gq, uint32_t pq);
            ~VkMachine();
            /// @brief 이 클래스 객체는 Game, Game2 밖에서는 생성, 소멸 호출이 불가능합니다.
            void operator delete(void*){}
        private:
            static VkMachine* singleton;
            VkInstance instance = nullptr;
            struct{
                VkSurfaceKHR handle;
                VkSurfaceCapabilitiesKHR caps;
                VkSurfaceFormatKHR format;
            } surface;
            VkPhysicalDevice card = nullptr;
            VkDevice device = nullptr;
            VkQueue graphicsQueue = nullptr;
            VkQueue presentQueue = nullptr;
            VkCommandPool gCommandPool = 0;
            VkCommandBuffer baseBuffer[1]={};
            struct{
                VkSwapchainKHR handle = 0;
                VkExtent2D extent;
                std::vector<VkImageView> imageView;
            }swapchain;
            
            VmaAllocator allocator = nullptr;
    };
}


#endif