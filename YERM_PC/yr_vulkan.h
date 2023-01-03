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

#include "yr_string.hpp"

#include "../externals/vulkan/vulkan.h"
#define VMA_VULKAN_VERSION 1000000
#include "../externals/vulkan/vk_mem_alloc.h"
#include <vector>
#include <set>
#include <queue>
#include <memory>
#include <map>

namespace onart {

    class Window;

    /// @brief Vulkan 그래픽스 컨텍스트를 초기화하며, 모델, 텍스처, 셰이더 등 각종 객체는 이 VkMachine으로부터 생성할 수 있습니다.
    class VkMachine{
        friend class Game;
        public:
            /// @brief Vulkan 확인 계층을 사용하려면 이것을 활성화해 주세요. 사용하려면 Vulkan "SDK"가 컴퓨터에 깔려 있어야 합니다.
            constexpr static bool USE_VALIDATION_LAYER = false;
            /// @brief 그리기 대상입니다. 텍스처로 사용하거나 메모리 맵으로 데이터에 접근할 수 있습니다. 
            class RenderTarget;
            /// @brief 
            class RenderPass;
            /// @brief 
            class Texture;
            using pTexture = std::shared_ptr<Texture>;
            /// @brief 정점 버퍼와 인덱스 버퍼를 합친 것입니다. 사양은 템플릿 생성자를 통해 정할 수 있습니다.
            class Mesh;
            /// @brief 푸시 상수를 제외한 셰이더 자원을 나타냅니다. 동시에 사용되지만 않는다면 여러 렌더패스 간에 공유될 수 있습니다.
            class UniformBuffer;
            /// @brief 렌더 타겟의 유형입니다.
            enum class RenderTargetType { 
                /// @brief 색 버퍼 1개를 보유합니다.
                COLOR1 = 0b1,
                /// @brief 색 버퍼 2개를 보유합니다.
                COLOR2 = 0b11,
                /// @brief 색 버퍼 3개를 보유합니다.
                COLOR3 = 0b111,
                /// @brief 깊이/스텐실 버퍼만을 보유합니다.
                DEPTH = 0b1000,
                /// @brief 색 버퍼 1개와 깊이/스텐실 버퍼를 보유합니다.
                COLOR1DEPTH = 0b1001,
                /// @brief 색 버퍼 2개와 깊이/스텐실 버퍼를 보유합니다.
                COLOR2DEPTH = 0b1011,
                /// @brief 색 버퍼 3개와 깊이/스텐실 버퍼를 보유합니다.
                COLOR3DEPTH = 0b1111
            };
            /// @brief ktx2, BasisU 파일을 불러와 텍스처를 생성합니다. (KTX2 파일이라도 BasisU가 아니면 실패합니다.) 여기에도 libktx로 그 형식을 만드는 별도의 도구가 있으니 필요하면 사용할 수 있습니다.
            /// @param fileName 파일 이름
            /// @param name 프로그램 내부에서 사용할 이름으로, 비워 두면 파일 이름을 그대로 사용합니다. 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
            /// @param ubtcs1 품질이 낮아져도 관계 없는데 메모리를 아끼고 싶을 때 true를 줍니다. 원본 파일이 이미 KTX2 - BasisU 형식인 경우 무시됩니다.
            pTexture createTexture(const string128& fileName, string128 name = "", bool ubtcs1 = false);
            /// @brief 메모리 상의 ktx2 파일을 통해 텍스처를 생성합니다. (KTX2 파일이라도 BasisU가 아니면 실패합니다.) 여기에도 libktx로 그 형식을 만드는 별도의 도구가 있으니 필요하면 사용할 수 있습니다.
            /// @param mem 이미지 시작 주소
            /// @param size mem 배열의 길이(바이트)
            /// @param name 프로그램 내부에서 사용할 이름입니다. 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
            pTexture createTexture(const uint8_t* mem, size_t size, string128 name);
            /// @brief 2D 렌더 타겟을 생성하고 핸들을 리턴합니다.
            /// @param width 가로 길이(px).
            /// @param height 세로 길이(px).
            /// @param name 이후 별도로 접근할 수 있는 이름을 지정합니다. 단, 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다. "screen"이라는 이름은 예약되어 있어 사용이 불가능하며, 말 그대로 화면을 뜻합니다.
            /// @param type @ref RenderTargetType 참고하세요.
            /// @param sampled true면 샘플링 텍스처로 사용되고, false면 입력 첨부물로 사용됩니다. 일단은 화면을 최종 목적지로 간주하기 때문에 그 외의 용도는 없습니다.
            /// @param mmap 이것이 true라면 픽셀 데이터에 순차로 접근할 수 있습니다. 단, 렌더링 성능이 낮아질 가능성이 매우 높습니다.
            /// @return 이것을 통해 렌더 패스를 생성할 수 있습니다.
            RenderTarget* createRenderTarget2D(int width, int height, const string16& name, RenderTargetType type = RenderTargetType::COLOR1DEPTH, bool sampled = true, bool mmap = false);
            /// @brief SPIR-V 컴파일된 셰이더를 VkShaderModule 형태로 저장하고 가져옵니다.
            /// @param spv SPIR-V 코드
            /// @param size 주어진 SPIR-V 코드의 길이(바이트)
            /// @param name 이후 별도로 접근할 수 있는 이름을 지정합니다. 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다.
            VkShaderModule createShader(const uint32_t* spv, size_t size, const string16& name);
            /// @brief 큐브맵 렌더 타겟을 생성하고 핸들을 리턴합니다. 한 번 부여된 핸들 번호는 같은 프로세스에서 다시는 사용되지 않습니다.
            /// @param size 각 면의 가로/세로 길이(px)
            /// @return 핸들입니다. 이것을 통해 렌더 패스를 생성할 수 있습니다.
            int createRenderTargetCube(int size);
            /// @brief 렌더 타겟을 제거합니다. 이것을 대상으로 하는 렌더패스 역시 모두 사용이 막힙니다.
            /// @param handle 렌더 타겟 이름
            void destroyRenderTarget(const string16& name);
        private:
            /// @brief 기본 Vulkan 컨텍스트를 생성합니다. 이 객체를 생성하면 기본적으로 인스턴스, 물리 장치, 가상 장치, 창 표면이 생성됩니다.
            VkMachine(Window*);
            /// @brief 그래픽스/전송 명령 버퍼를 풀로부터 할당합니다.
            /// @param count 할당할 수
            /// @param isPrimary true면 주 버퍼, false면 보조 버퍼입니다.
            /// @param buffers 버퍼가 들어갑니다. count 길이 이상의 배열이어야 하며, 할당 실패 시 첫 번째에 nullptr가 들어갑니다.
            void allocateCommandBuffers(int count, bool isPrimary, VkCommandBuffer* buffers);
            /// @brief 창 표면을 재설정합니다. 이에 따라 스왑체인, 생성한 모든 말단 렌더패스들도 모두 재생성됩니다.
            void resetWindow(Window* window);
            /// @brief 창 표면 초기화 이후 호출되어 특성을 파악합니다.
            void checkSurfaceHandle();
            /// @brief 기존 스왑체인을 제거하고 다시 생성하며, 스왑체인에 대한 이미지뷰도 가져옵니다.
            void createSwapchain(uint32_t width, uint32_t height, uint32_t gq, uint32_t pq);
            /// @brief 기존 스왑체인과 관련된 모든 것을 해제합니다.
            void destroySwapchain();
            /// @brief vulkan 객체를 없앱니다.
            void free();
            ~VkMachine();
            /// @brief 이 클래스 객체는 Game 밖에서는 생성, 소멸 호출이 불가능합니다.
            inline void operator delete(void*){}
        private:
            static VkMachine* singleton;
            VkInstance instance = nullptr;
            struct{
                VkSurfaceKHR handle;
                VkSurfaceCapabilitiesKHR caps;
                VkSurfaceFormatKHR format;
            } surface;
            struct{
                VkPhysicalDevice card = nullptr;
                uint32_t gq, pq;
                uint64_t minUBOffsetAlignment;
            } physicalDevice;
            VkDevice device = nullptr;
            VkQueue graphicsQueue = nullptr;
            VkQueue presentQueue = nullptr;
            VkCommandPool gCommandPool = 0;
            VkCommandBuffer baseBuffer[1]={};
            VkDescriptorPool descriptorPool = nullptr;
            struct{
                VkSwapchainKHR handle = 0;
                VkExtent2D extent;
                std::vector<VkImageView> imageView;
            }swapchain;
            std::map<string16, RenderTarget*> renderTargets;
            std::map<string16, VkShaderModule> shaders;
            std::map<string128, pTexture> textures;
            struct ImageSet{
                VkImage img = nullptr;
                VkImageView view = nullptr;
                VmaAllocation alloc = nullptr;
                void free(VkDevice, VmaAllocator);
            };
            std::set<ImageSet*> images;
            std::vector<std::shared_ptr<RenderPass>> passes;
            VmaAllocator allocator = nullptr;
        private:
            /// @brief 렌더 타겟에 부여된 이미지 셋을 제거합니다.
            void removeImageSet(ImageSet*);
    };

    class VkMachine::RenderTarget{
        friend class VkMachine;
        public:
            RenderTarget& operator=(const RenderTarget&) = delete;
        private:
            std::vector<RenderPass*> passes; // 렌더타겟이 없어지면 그것을 타겟으로 하는 렌더패스도 모두 없애야 함. 크기가 바뀌거나 하면 렌더패스도 모두 그에 맞게 바꿔야 함(크기를 바꿀 일의 예: 유저 설정에 의해 성능을 올리기 위해 크기를 줄임)
            VkMachine::ImageSet* color1, *color2, *color3, *depthstencil;
            unsigned width, height;
            RenderTarget(unsigned width, unsigned height, VkMachine::ImageSet*, VkMachine::ImageSet*, VkMachine::ImageSet*, VkMachine::ImageSet*);
            ~RenderTarget();
    };

    class VkMachine::RenderPass{
        friend class VkMachine;
        public:
            RenderPass& operator=(const RenderPass&) = delete;
            void setViewport();
            void setScissor();
            /// @brief 푸시 상수를 세팅합니다.
            void push(void*, int start, int end);
            /// @brief 메시를 그립니다. (TODO: 인스턴싱을 위한 인터페이스 따로 추가)
            void invoke(Mesh*);
            /// @brief 명령 기록을 시작합니다.
            void start();
            /// @brief 기록된 명령을 모두 수행합니다. 동작이 완료되지 않아도 즉시 리턴합니다.
            void execute();
            /// @brief draw 수행 이후에 호출되면 그리기가 끝나고 나서 리턴합니다. 그 외의 경우는 그냥 리턴합니다.
            void wait();
            /// @brief 렌더타겟이 없어져 더 이상 사용할 수 없는 경우 true를 리턴합니다.
            bool isDangling();
        private:
            RenderPass(RenderTarget*, VkShaderModule vs, VkShaderModule fs); // 이후 다수의 서브패스를 쓸 수 있도록 변경
            void constructFB(RenderTarget*);
            void constructPipeline(VkShaderModule vs, VkShaderModule fs);
            void reconstruct(RenderTarget*);
            VkFramebuffer fb = nullptr;
            VkRenderPass rp = nullptr;
            VkPipeline pipeline = nullptr;
            VkPipelineLayout layout = nullptr;
            VkFence fence = nullptr;
            bool dangle = false;
    };

    class VkMachine::Texture{
        public:
        protected:
            Texture(VkImage img, VkImageView imgView, VmaAllocation alloc);
            ~Texture();
        private:
            VkImage img;
            VkImageView view;
            VmaAllocation alloc;
    };

    class VkMachine::Mesh{
        public:
        private:
    };

    class VkMachine::UniformBuffer{
        public:
            inline uint32_t offset(uint32_t index) { return individual * index; }
            /// @brief 동적 공유 버퍼에 한하여 버퍼 원소 수를 주어진 만큼으로 조정합니다. 기존 데이터 중 주어진 크기 이내에 있는 것은 유지됩니다.
            void resize(uint32_t size);
            /// @brief 유니폼 버퍼의 내용을 갱신합니다.
            /// @param input 입력 데이터
            /// @param index 몇 번째 내용을 수정할 것인지입니다. 동적 버퍼가 아닌 경우 값은 무시됩니다.
            /// @param offset 데이터 내에서 몇 바이트째부터 수정할지
            /// @param size 덮어쓸 양
            void update(const void* input, uint32_t index, uint32_t offset, uint32_t size);
            /// @brief 동적 유니폼 버퍼인 경우 사용할 수 있는 인덱스를 리턴합니다. 사용할 수 있는 공간이 없는 경우 공간을 늘립니다.
            uint16_t getIndex();
            /// @brief 파이프라인을 정의하기 위한 레이아웃을 가져옵니다.
            inline VkDescriptorSetLayout getLayout() { return layout; }
        private:
            /// @brief 임시로 저장되어 있던 내용을 모두 GPU로 올립니다.
            void sync();
            UniformBuffer(uint32_t length, uint32_t size, VkShaderStageFlags stages, uint32_t binding = 0);
            ~UniformBuffer();
            VkDescriptorSetLayout layout = nullptr;
            VkDescriptorSet dset = nullptr;
            VkBuffer buffer = nullptr;
            VmaAllocation alloc = nullptr;
            const bool isDynamic;
            uint32_t binding;
            uint32_t individual = 0; // 동적 공유 버퍼인 경우 (버퍼 업데이트를 위한) 개별 성분의 크기
            uint32_t length;
            std::vector<uint8_t> staged;
            std::priority_queue<uint16_t, std::vector<uint16_t>, std::greater<uint16_t>> indices;
            void* mmap;
    };
}


#endif