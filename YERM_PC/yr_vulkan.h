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
#include "yr_tuple.hpp"

#include "../externals/vulkan/vulkan.h"
#define VMA_VULKAN_VERSION 1000000
#include "../externals/vulkan/vk_mem_alloc.h"

#include "yr_math.hpp"

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

    /// @brief Vulkan 그래픽스 컨텍스트를 초기화하며, 모델, 텍스처, 셰이더 등 각종 객체는 이 VkMachine으로부터 생성할 수 있습니다.
    class VkMachine{
        friend class Game;
        public:
            /// @brief Vulkan 확인 계층을 사용하려면 이것을 활성화해 주세요. 사용하려면 Vulkan "SDK"가 컴퓨터에 깔려 있어야 합니다.
            constexpr static bool USE_VALIDATION_LAYER = false;
            /// @brief 그리기 대상입니다. 텍스처로 사용하거나 메모리 맵으로 데이터에 접근할 수 있습니다. 
            class RenderTarget;
            /// @brief 오프스크린용 렌더 패스입니다.
            class RenderPass;
            /// @brief 화면에 그리기 위한 렌더 패스입니다. 여러 개 갖고 있을 수는 있지만 동시에 여러 개를 사용할 수는 없습니다.
            class RenderPass2Screen;
            /// @brief 직접 불러오는 텍스처입니다.
            class Texture;
            /// @brief 속성을 직접 정의하는 정점 객체입니다.
            template<class, class...>
            struct Vertex;
            using pTexture = std::shared_ptr<Texture>;
            /// @brief 정점 버퍼와 인덱스 버퍼를 합친 것입니다. 사양은 템플릿 생성자를 통해 정할 수 있습니다.
            class Mesh;
            using pMesh = std::shared_ptr<Mesh>;
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
            /// @brief 파이프라인 생성 시 줄 수 있는 옵션입니다. 여기의 성분들을 비트 or하여 생성 함수에 전달합니다.
            enum PipelineOptions: uint32_t {
                USE_DEPTH = 0b1,
                USE_STENCIL = 0b10,
                CULL_BACK = 0b100,
            };
            /// @brief 스왑체인 회전 변동 관련 최적의 처리를 위해 응용단에서 추가로 가해야 할 회전입니다. PC 버전에서는 반드시 단위행렬이 리턴됩니다.
            static mat4 preTransform();
            /// @brief ktx2, BasisU 파일을 불러와 텍스처를 생성합니다. (KTX2 파일이라도 BasisU가 아니면 실패할 가능성이 있습니다.) 여기에도 libktx로 그 형식을 만드는 별도의 도구가 있으니 필요하면 사용할 수 있습니다.
            /// @param fileName 파일 이름
            /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
            /// @param nChannels 채널 수(색상)
            /// @param srgb true면 텍스처 원본의 색을 srgb 공간에 있는 것으로 취급합니다.
            /// @param hq 원본이 최대한 섬세하게 남아 있어야 한다면 true를 줍니다. false를 주면 메모리를 크게 절약할 수도 있지만 품질이 낮아질 수 있습니다.
            static pTexture createTexture(const char* fileName, int32_t key, uint32_t nChannels, bool srgb = true, bool hq = true);
            /// @brief 메모리 상의 ktx2 파일을 통해 텍스처를 생성합니다. (KTX2 파일이라도 BasisU가 아니면 실패할 가능성이 있습니다.) 여기에도 libktx로 그 형식을 만드는 별도의 도구가 있으니 필요하면 사용할 수 있습니다.
            /// @param mem 이미지 시작 주소
            /// @param size mem 배열의 길이(바이트)
            /// @param nChannels 채널 수(색상)
            /// @param key 프로그램 내부에서 사용할 이름입니다. 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
            /// @param srgb true면 텍스처 원본의 색을 srgb 공간에 있는 것으로 취급합니다.
            /// @param hq 원본이 최대한 섬세하게 남아 있어야 한다면 true를 줍니다. false를 주면 메모리를 크게 절약할 수도 있지만 품질이 낮아질 수 있습니다.
            static pTexture createTexture(const uint8_t* mem, size_t size, uint32_t nChannels, int32_t key, bool srgb = true, bool hq = true);
            /// @brief 2D 렌더 타겟을 생성하고 핸들을 리턴합니다. 이것을 해제하는 수단은 없으며, 프로그램 종료 시 자동으로 해제됩니다.
            /// @param width 가로 길이(px).
            /// @param height 세로 길이(px).
            /// @param key 이후 별도로 접근할 수 있는 이름을 지정합니다. 단, 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다. INT32_MIN, 즉 -2147483648 값은 예약되어 있어 사용이 불가능합니다.
            /// @param type @ref RenderTargetType 참고하세요.
            /// @param sampled true면 샘플링 텍스처로 사용되고, false면 입력 첨부물로 사용됩니다. 일단은 화면을 최종 목적지로 간주하기 때문에 그 외의 용도는 없습니다.
            /// @param useDepthInput 이 타겟이 입력 첨부물 혹은 텍스처로 사용될 경우에 깊이 버퍼도 입력 첨부물로 사용할지 여부입니다. 깊이가 입력 첨부물 혹은 샘플링으로 사용될 경우, 스텐실 버퍼는 사용할 수 없습니다.
            /// @param useStencil true면 깊이 이미지에 더불어 스텐실 버퍼를 사용합니다.
            /// @param mmap 이것이 true라면 픽셀 데이터에 순차로 접근할 수 있습니다. 단, 렌더링 성능이 낮아질 가능성이 매우 높습니다.
            /// @return 이것을 통해 렌더 패스를 생성할 수 있습니다.
            static RenderTarget* createRenderTarget2D(int width, int height, int32_t key, RenderTargetType type = RenderTargetType::COLOR1DEPTH, bool sampled = true, bool useDepthInput = false, bool useStencil = false, bool mmap = false);
            /// @brief SPIR-V 컴파일된 셰이더를 VkShaderModule 형태로 저장하고 가져옵니다.
            /// @param spv SPIR-V 코드
            /// @param size 주어진 SPIR-V 코드의 길이(바이트)
            /// @param key 이후 별도로 접근할 수 있는 이름을 지정합니다. 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다.
            static VkShaderModule createShader(const uint32_t* spv, size_t size, int32_t key);
            /// @brief 셰이더에서 사용할 수 있는 uniform 버퍼를 생성하여 리턴합니다. 이것을 해제하는 방법은 없으며, 프로그램 종료 시 자동으로 해제됩니다.
            /// @param length 이 값이 1이면 버퍼 하나를 생성하며, 2 이상이면 동적 공유 버퍼를 생성합니다. 동적 공유 버퍼는 렌더 패스 진행 중 바인드할 영역을 바꿀 수 있습니다.
            /// @param size 각 버퍼의 크기입니다.
            /// @param stages 이 자원에 접근할 수 있는 셰이더 단계들입니다. (비트 플래그의 조합)
            /// @param key 프로그램 내에서 사용할 이름입니다. 중복된 이름이 입력된 경우 주어진 나머지 인수를 무시하고 그 이름을 가진 버퍼를 리턴합니다.
            /// @param binding 바인딩 번호입니다.
            static UniformBuffer* createUniformBuffer(uint32_t length, uint32_t size, VkShaderStageFlags stages, int32_t key, uint32_t binding = 0);
            /// @brief 주어진 렌더 타겟들을 대상으로 하는 렌더패스를 구성합니다.
            /// @param targets 렌더 타겟 포인터의 배열입니다. 마지막 것을 제외한 모든 타겟은 input attachment로 생성되었어야 하며 그렇지 않은 경우 실패합니다.
            /// @param subpassCount targets 배열의 크기입니다.
            /// @param key 이름입니다. 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다. INT32_MIN, 즉 -2147483648은 예약된 값이기 때문에 사용할 수 없습니다.
            static RenderPass* createRenderPass(RenderTarget** targets, uint32_t subpassCount, int32_t key);
            /// @brief 화면으로 이어지는 렌더패스를 생성합니다. 각 패스의 타겟들은 현재 창의 해상도와 동일하게 맞춰집니다.
            /// @param targets 생성할 렌더 타겟들의 타입 배열입니다. 서브패스의 마지막은 이것을 제외한 스왑체인 이미지입니다.
            /// @param subpassCount 최종 서브패스의 수입니다. 즉 targets 배열 길이 + 1을 입력해야 합니다.
            /// @param key 이름입니다. (RenderPass 객체와 같은 집합을 공유하지 않음) 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다. INT32_MIN, 즉 -2147483648은 예약된 값이기 때문에 사용할 수 없습니다.
            /// @param useDepth subpassCount가 1이고 이 값이 true인 경우 최종 패스에서 깊이/스텐실 이미지를 사용하게 됩니다. subpassCount가 1이 아니면 무시됩니다.
            /// @param useDepthAsInput targets와 일대일 대응하며, 대응하는 성분의 깊이 성분을 다음 서브패스의 입력으로 사용하려면 true를 줍니다. nullptr를 주는 경우 일괄 false로 취급됩니다. 즉 nullptr가 아니라면 반드시 subpassCount - 1 길이의 배열이 주어져야 합니다.
            static RenderPass2Screen* createRenderPass2Screen(RenderTargetType* targets, uint32_t subpassCount, int32_t key, bool useDepth = true, bool* useDepthAsInput = nullptr);
            /// @brief 파이프라인 레이아웃을 만듭니다. 파이파라인 레이아웃은 셰이더에서 접근할 수 있는 uniform 버퍼, 입력 첨부물, 텍스처 등의 사양을 말합니다.
            /// @param layouts 레이아웃 객체의 배열입니다. Texture, UniformBuffer, RenderTarget의 getLayout으로 얻을 수 있습니다.
            /// @param count layouts의 길이
            /// @param stages 푸시 상수에 접근할 수 있는 파이프라인 단계 플래그들입니다. 이 값에 0 외의 값이 들어가면 푸시 상수 공간은 항상 128바이트가 명시됩니다. 0이 들어가면 푸시 상수는 사용한다고 명시되지 않습니다.
            /// @param key 이름입니다. 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다.
            static VkPipelineLayout createPipelineLayout(VkDescriptorSetLayout* layouts, uint32_t count, VkShaderStageFlags stages, int32_t key);
            /// @brief 파이프라인을 생성합니다. 생성된 파이프라인은 이후에 이름으로 사용할 수도 있고, 주어진 렌더패스의 해당 서브패스 위치로 들어갑니다.
            /// @param vinfo 정점 속성 바인드를 위한 것입니다. Vertex 템플릿 클래스로부터 얻을 수 있습니다.
            /// @param vsize 개별 정점의 크기입니다. Vertex 템플릿 클래스에 sizeof를 사용하여 얻을 수 있습니다.
            /// @param vattr vinfo 배열의 길이, 즉 정점 속성의 수입니다.
            /// @param iinfo 인스턴스 속성 바인드를 위한 것입니다. Vertex 템플릿 클래스로부터 얻을 수 있습니다. 인스턴싱이 들어가지 않는 파이프라인인 경우 nullptr를 주면 됩니다.
            /// @param isize 개별 인스턴스 속성 집합의 크기입니다. Vertex 템플릿 클래스에 sizeof를 사용하여 얻을 수 있습니다.
            /// @param iattr iinfo 배열의 길이, 즉 인스턴스 속성의 수입니다. 인스턴싱이 들어가지 않는 파이프라인인 경우 반드시 0을 주어야 합니다.
            /// @param pass 이 파이프라인을 사용할 렌더패스입니다. 이걸 명시했다고 꼭 여기에만 사용할 수 있는 것은 아닙니다. 입/출력 첨부물의 사양이 맞기만 하면 됩니다.
            /// @param subpass 이 파이프라인을 사용할, 주어진 렌더패스의 서브패스 번호입니다. 이걸 명시했다고 꼭 여기에만 사용할 수 있는 것은 아닙니다. 입/출력 첨부물의 사양이 맞기만 하면 됩니다.
            /// @param flags 파이프라인 고정 옵션의 플래그입니다. @ref PipelineOptions
            /// @param vs 정점 셰이더 모듈입니다.
            /// @param fs 조각 셰이더 모듈입니다.
            /// @param name 이름입니다. 최대 15바이트까지만 가능합니다. 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다.
            /// @param front 앞면에 대한 스텐실 연산 인수입니다. 사용하지 않으려면 nullptr를 주면 됩니다.
            /// @param back 뒷면에 대한 스텐실 연산 인수입니다. 사용하지 않으려면 nullptr를 주면 됩니다.
            static VkPipeline createPipeline(VkVertexInputAttributeDescription* vinfo, uint32_t vsize, uint32_t vattr, VkVertexInputAttributeDescription* iinfo, uint32_t isize, uint32_t iattr, RenderPass* pass, uint32_t subpass, uint32_t flags, VkPipelineLayout layout, VkShaderModule vs, VkShaderModule fs, int32_t name, VkStencilOpState* front = nullptr, VkStencilOpState* back = nullptr);
            /// @brief 파이프라인을 생성합니다. 생성된 파이프라인은 이후에 이름으로 사용할 수도 있고, 주어진 렌더패스의 해당 서브패스 위치로 들어갑니다.
            /// @param vinfo 정점 속성 바인드를 위한 것입니다. Vertex 템플릿 클래스로부터 얻을 수 있습니다.
            /// @param vsize 개별 정점의 크기입니다. Vertex 템플릿 클래스에 sizeof를 사용하여 얻을 수 있습니다.
            /// @param vattr vinfo 배열의 길이, 즉 정점 속성의 수입니다.
            /// @param iinfo 인스턴스 속성 바인드를 위한 것입니다. Vertex 템플릿 클래스로부터 얻을 수 있습니다. 인스턴싱이 들어가지 않는 파이프라인인 경우 nullptr를 주면 됩니다.
            /// @param isize 개별 인스턴스 속성 집합의 크기입니다. Vertex 템플릿 클래스에 sizeof를 사용하여 얻을 수 있습니다.
            /// @param iattr iinfo 배열의 길이, 즉 인스턴스 속성의 수입니다. 인스턴싱이 들어가지 않는 파이프라인인 경우 반드시 0을 주어야 합니다.
            /// @param pass 이 파이프라인을 사용할 렌더패스입니다. 이걸 명시했다고 꼭 여기에만 사용할 수 있는 것은 아닙니다. 입/출력 첨부물의 사양이 맞기만 하면 됩니다.
            /// @param subpass 이 파이프라인을 사용할, 주어진 렌더패스의 서브패스 번호입니다. 이걸 명시했다고 꼭 여기에만 사용할 수 있는 것은 아닙니다. 입/출력 첨부물의 사양이 맞기만 하면 됩니다.
            /// @param flags 파이프라인 고정 옵션의 플래그입니다. @ref PipelineOptions
            /// @param vs 정점 셰이더 모듈입니다.
            /// @param fs 조각 셰이더 모듈입니다.
            /// @param name 이름입니다. 최대 15바이트까지만 가능합니다. 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다.
            /// @param front 앞면에 대한 스텐실 연산 인수입니다. 사용하지 않으려면 nullptr를 주면 됩니다.
            /// @param back 뒷면에 대한 스텐실 연산 인수입니다. 사용하지 않으려면 nullptr를 주면 됩니다.
            static VkPipeline createPipeline(VkVertexInputAttributeDescription* vinfo, uint32_t vsize, uint32_t vattr, VkVertexInputAttributeDescription* iinfo, uint32_t isize, uint32_t iattr, RenderPass2Screen* pass, uint32_t subpass, uint32_t flags, VkPipelineLayout layout, VkShaderModule vs, VkShaderModule fs, int32_t name, VkStencilOpState* front = nullptr, VkStencilOpState* back = nullptr);
            /// @brief 정점 버퍼(모델) 객체를 생성합니다.
            /// @param vdata 정점 데이터
            /// @param vsize 정점 하나의 크기(바이트)
            /// @param vcount 정점의 수
            /// @param idata 인덱스 데이터입니다. 이 값은 vcount가 65536 이상일 경우 32비트 정수로, 그 외에는 16비트 정수로 취급됩니다.
            /// @param isize 인덱스 하나의 크기입니다. 이 값은 반드시 2 또는 4여야 합니다.
            /// @param icount 인덱스의 수입니다. 이 값이 0이면 인덱스 버퍼를 사용하지 않습니다.
            /// @param key 프로그램 내에서 사용할 이름입니다.
            /// @param stage 메모리 맵을 통해 지속적으로 데이터를 바꾸어 보낼지 선택합니다. stage가 false라서 메모리 맵을 사용하게 되는 경우 vdata, idata를 nullptr로 주어도 됩니다.
            static pMesh createMesh(void* vdata, size_t vsize, size_t vcount, void* idata, size_t isize, size_t icount, int32_t key, bool stage = true);
            /// @brief 정점의 수 정보만 저장하는 메시 객체를 생성합니다. 이것은 파이프라인 자체에서 정점 데이터가 정의되어 있는 경우를 위한 것입니다.
            /// @param vcount 정점의 수
            /// @param name 프로그램 내에서 사용할 이름입니다.
            static pMesh createNullMesh(size_t vcount, int32_t key);
            /// @brief 큐브맵 렌더 타겟을 생성하고 핸들을 리턴합니다. 한 번 부여된 핸들 번호는 같은 프로세스에서 다시는 사용되지 않습니다.
            /// @param size 각 면의 가로/세로 길이(px)
            /// @return 핸들입니다. 이것을 통해 렌더 패스를 생성할 수 있습니다.
            static int createRenderTargetCube(int size, int32_t key);
            /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
            static RenderPass2Screen* getRenderPass2Screen(int32_t key);
            /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
            static RenderPass* getRenderPass(int32_t key);
            /// @brief 만들어 둔 파이프라인을 리턴합니다. 없으면 nullptr를 리턴합니다.
            static VkPipeline getPipeline(int32_t key);
            /// @brief 만들어 둔 파이프라인 레이아웃을 리턴합니다. 없으면 nullptr를 리턴합니다.
            static VkPipelineLayout getPipelineLayout(int32_t key);
            /// @brief 만들어 둔 렌더 타겟을 리턴합니다. 없으면 nullptr를 리턴합니다.
            static RenderTarget* getRenderTarget(int32_t key);
            /// @brief 만들어 둔 공유 버퍼를 리턴합니다. 없으면 nullptr를 리턴합니다.
            static UniformBuffer* getUniformBuffer(int32_t key);
            /// @brief 만들어 둔 셰이더 모듈을 리턴합니다. 없으면 nullptr를 리턴합니다.
            static VkShaderModule getShader(int32_t key);
            /// @brief 올려 둔 텍스처 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
            static pTexture getTexture(int32_t key);
            /// @brief 만들어 둔 메시 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
            static pMesh getMesh(int32_t key);
            /// @brief 창 표면이 SRGB 공간을 사용하는지 리턴합니다. 
            inline static bool isSurfaceSRGB(){ return singleton->surface.format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;}
            /// @brief Vulkan 인스턴스를 리턴합니다. 함께 사용하고 싶은 기능이 있는데 구현되어 있지 않은 경우에만 사용하면 됩니다.
            inline static VkInstance getInstance() { return singleton->instance; }
            /// @brief Vulkan 물리 장치를 리턴합니다. 함께 사용하고 싶은 기능이 있는데 구현되어 있지 않은 경우에만 사용하면 됩니다.
            inline static VkPhysicalDevice getPhysicalDevice() { return singleton->physicalDevice.card; }
            /// @brief Vulkan 논리 장치를 리턴합니다. 함께 사용하고 싶은 기능이 있는데 구현되어 있지 않은 경우에만 사용하면 됩니다.
            inline static VkDevice getDevice() { return singleton->device; }
            /// @brief Vma 할당기를 리턴합니다. 함께 사용하고 싶은 기능이 있는데 구현되어 있지 않은 경우에만 사용하면 됩니다.
            inline static VmaAllocator getAllocator() { return singleton->allocator; }
            /// @brief 주어진 바인딩 번호의 텍스처 기술자 집합(combined sampler) 레이아웃을 리턴합니다.
            inline static VkDescriptorSetLayout getTextureLayout(uint32_t binding) { assert(binding < 4); return singleton->textureLayout[binding]; }
            /// @brief 주어진 바인딩 번호의 입력 첨부물 기술자 집합 레이아웃을 리턴합니다.
            inline static VkDescriptorSetLayout getInputAttachmentLayout(uint32_t binding) { assert(binding < 4); return singleton->inputAttachmentLayout[binding]; }
        private:
            /// @brief 기본 Vulkan 컨텍스트를 생성합니다. 이 객체를 생성하면 기본적으로 인스턴스, 물리 장치, 가상 장치, 창 표면이 생성됩니다.
            VkMachine(Window*);
            /// @brief 그래픽스/전송 명령 버퍼를 풀로부터 할당합니다.
            /// @param count 할당할 수
            /// @param isPrimary true면 주 버퍼, false면 보조 버퍼입니다.
            /// @param buffers 버퍼가 들어갑니다. count 길이 이상의 배열이어야 하며, 할당 실패 시 첫 번째에 nullptr가 들어갑니다.
            void allocateCommandBuffers(int count, bool isPrimary, VkCommandBuffer* buffers);
            /// @brief 기술자 집합을 할당합니다.
            /// @param layouts 할당할 집합 레이아웃
            /// @param count 할당할 수
            /// @param output 리턴받을 곳. 실패하면 첫 번째 원소가 nullptr로 들어감이 보장됩니다.
            void allocateDescriptorSets(VkDescriptorSetLayout* layouts, uint32_t count, VkDescriptorSet* output);
            /// @brief 창 표면 초기화 이후 호출되어 특성을 파악합니다.
            void checkSurfaceHandle();
            /// @brief 기존 스왑체인을 제거하고 다시 생성하며, 스왑체인에 대한 이미지뷰도 가져옵니다.
            void createSwapchain(uint32_t width, uint32_t height, Window* window = nullptr);
            /// @brief 기존 스왑체인과 관련된 모든 것을 해제합니다.
            void destroySwapchain();
            /// @brief 텍스처와 입력 첨부물에 대한 기술자 집합 레이아웃을 미리 만들어 둡니다.
            bool createLayouts();
            /// @brief 샘플러들을 미리 만들어 둡니다.
            bool createSamplers();
            /// @brief 펜스를 생성합니다. (해제는 알아서)
            VkFence createFence(bool signaled = false);
            /// @brief 세마포어를 생성합니다. (해제는 알아서)
            VkSemaphore createSemaphore();
            /// @brief ktxTexture2 객체로 텍스처를 생성합니다.
            pTexture createTexture(void* ktxObj, int32_t key, uint32_t nChannels, bool srgb, bool hq);
            /// @brief vulkan 객체를 없앱니다.
            void free();
            ~VkMachine();
            /// @brief 이 클래스 객체는 Game 밖에서는 생성, 소멸 호출이 불가능합니다.
            inline void operator delete(void*){}
        private:
            static VkMachine* singleton;
            VkInstance instance = VK_NULL_HANDLE;
            struct{
                VkSurfaceKHR handle;
                VkSurfaceCapabilitiesKHR caps;
                VkSurfaceFormatKHR format;
            } surface;
            struct{
                VkPhysicalDevice card = VK_NULL_HANDLE;
                uint32_t gq, pq;
                uint64_t minUBOffsetAlignment;
            } physicalDevice;
            VkDevice device = VK_NULL_HANDLE;
            VkQueue graphicsQueue = VK_NULL_HANDLE;
            VkQueue presentQueue = VK_NULL_HANDLE;
            VkCommandPool gCommandPool = VK_NULL_HANDLE;
            VkCommandBuffer baseBuffer[1]={};
            VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
            VkDescriptorSetLayout textureLayout[4] = {}; // 바인딩 0~3 하나씩
            VkDescriptorSetLayout inputAttachmentLayout[4] = {}; // 바인딩 0~3 하나씩
            
            VkSampler textureSampler[16] = {}; // maxLod 1~17. TODO: 비등방성 샘플링 선택 제공
            struct{
                VkSwapchainKHR handle = VK_NULL_HANDLE;
                VkExtent2D extent;
                std::vector<VkImageView> imageView;
            }swapchain;
            std::map<int32_t, RenderPass*> renderPasses;
            std::map<int32_t, RenderPass2Screen*> finalPasses;
            std::map<int32_t, RenderTarget*> renderTargets;
            std::map<int32_t, VkShaderModule> shaders;
            std::map<int32_t, UniformBuffer*> uniformBuffers;
            std::map<int32_t, VkPipelineLayout> pipelineLayouts;
            std::map<int32_t, VkPipeline> pipelines;
            std::map<int32_t, pMesh> meshes;
            std::map<int32_t, pTexture> textures;
            struct ImageSet{
                VkImage img = VK_NULL_HANDLE;
                VkImageView view = VK_NULL_HANDLE;
                VmaAllocation alloc = nullptr;
                void free();
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
        friend class RenderPass;
        friend class RenderPass2Screen;
        public:
            RenderTarget& operator=(const RenderTarget&) = delete;
        private:
            /// @brief 이 타겟을 위한 첨부물을 기술합니다.
            /// @return 색 첨부물의 수(최대 3)
            uint32_t attachmentRefs(VkAttachmentDescription* descr, bool forSample);
            uint32_t getDescriptorSets(VkDescriptorSet* out);
            VkMachine::ImageSet* color1, *color2, *color3, *depthstencil;
            VkDescriptorSet dset1 = VK_NULL_HANDLE, dset2 = VK_NULL_HANDLE, dset3 = VK_NULL_HANDLE, dsetDS = VK_NULL_HANDLE;
            unsigned width, height;
            const bool mapped, sampled;
            const RenderTargetType type;
            RenderTarget(RenderTargetType type, unsigned width, unsigned height, VkMachine::ImageSet*, VkMachine::ImageSet*, VkMachine::ImageSet*, VkMachine::ImageSet*, bool, bool, VkDescriptorSet*);
            ~RenderTarget();
    };

    class VkMachine::RenderPass{
        friend class VkMachine;
        public:
            RenderPass& operator=(const RenderPass&) = delete;
            /// @brief 뷰포트를 설정합니다. 기본 상태는 프레임버퍼 생성 당시의 크기들입니다. (즉 @ref reconstructFB 를 사용 시 여기서 수동으로 정한 값은 리셋됩니다.)
            /// 이것은 패스 내의 모든 파이프라인이 공유합니다.
            /// @param width 뷰포트 가로 길이(px)
            /// @param height 뷰포트 세로 길이(px)
            /// @param x 뷰포트 좌측 좌표(px, 맨 왼쪽이 0)
            /// @param y 뷰포트 상단 좌표(px, 맨 위쪽이 0)
            /// @param applyNow 이 값이 참이면 변경된 값이 파이프라인에 즉시 반영됩니다.
            void setViewport(float width, float height, float x, float y, bool applyNow = false);
            /// @brief 시저를 설정합니다. 기본 상태는 자름 없음입니다.
            /// 이것은 패스 내의 모든 파이프라인이 공유합니다.
            /// @param width 살릴 직사각형의 가로 길이(px)
            /// @param height 살릴 직사각형의 세로 길이(px)
            /// @param x 살릴 직사각형의 좌측 좌표(px, 맨 왼쪽이 0)
            /// @param y 살릴 직사각형의 상단 좌표(px, 맨 위쪽이 0)
            /// @param applyNow 이 값이 참이면 변경된 값이 파이프라인에 즉시 반영됩니다.
            void setScissor(uint32_t width, uint32_t height, int32_t x, int32_t y, bool applyNow = false);
            /// @brief 주어진 유니폼버퍼를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
            /// @param pos 바인드할 set 번호
            /// @param ub 바인드할 버퍼
            /// @param ubPos 버퍼가 동적 공유 버퍼인 경우, 그것의 몇 번째 성분을 바인드할지 정합니다. 아닌 경우 이 값은 무시됩니다.
            void bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos = 0);
            /// @brief 주어진 텍스처를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
            /// @param pos 바인드할 set 번호
            /// @param tx 바인드할 텍스처
            void bind(uint32_t pos, const pTexture& tx);
            /// @brief 주어진 렌더 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다. 이 패스의 프레임버퍼에서 사용 중인 렌더타겟은 사용할 수 없습니다.
            /// @param pos 바인드할 set 번호
            /// @param target 바인드할 타겟
            /// @param index 렌더 타겟 내의 인덱스입니다. (0~2는 색 버퍼, 3은 깊이 버퍼)
            void bind(uint32_t pos, RenderTarget* target, uint32_t index);
            /// @brief 주어진 파이프라인을 주어진 서브패스에 사용하게 합니다. 이 함수는 매번 호출할 필요는 없지만, 해당 서브패스 진행 중에 사용하면 그때부터 바로 새로 주어진 파이프라인을 사용합니다.
            /// @param pipeline 파이프라인
            /// @param layout 파이프라인 레이아웃
            /// @param subpass 서브패스 번호
            void usePipeline(VkPipeline pipeline, VkPipelineLayout layout, uint32_t subpass);
            /// @brief 푸시 상수를 세팅합니다. 서브패스 진행중이 아니면 실패합니다.
            void push(void* input, uint32_t start, uint32_t end);
            /// @brief 메시를 그립니다. 정점 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
            void invoke(const pMesh&);
            /// @brief 메시를 그립니다. 정점 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
            /// @param mesh 기본 정점버퍼
            /// @param instanceInfo 인스턴스 속성 버퍼
            /// @param instanceCount 인스턴스 수
            void invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount);
            /// @brief 서브패스를 시작합니다. 이미 서브패스가 시작된 상태라면 다음 서브패스를 시작하며, 다음 것이 없으면 아무 동작도 하지 않습니다. 주어진 파이프라인이 없으면 동작이 실패합니다.
            /// @param pos 이전 서브패스의 결과인 입력 첨부물을 바인드할 위치의 시작점입니다. 예를 들어, pos=0이고 이전 타겟이 색 첨부물 2개, 깊이 첨부물 1개였으면 0, 1, 2번에 바인드됩니다. 셰이더를 그에 맞게 만들어야 합니다.
            void start(uint32_t pos = 0);
            /// @brief 기록된 명령을 모두 수행합니다. 동작이 완료되지 않아도 즉시 리턴합니다.
            /// @param other 이 패스가 시작하기 전에 기다릴 다른 렌더패스입니다. 전후 의존성이 존재할 경우 사용하는 것이 좋습니다. (Vk세마포어 동기화를 사용) 현재 버전에서 기다리는 단계는 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 하나로 고정입니다.
            void execute(RenderPass* other = nullptr);
            /// @brief draw 수행 이후에 호출되면 그리기가 끝나고 나서 리턴합니다. 그 외의 경우는 그냥 리턴합니다.
            /// @param timeout 기다릴 최대 시간(ns), UINT64_MAX (~0) 값이 입력되면 무한정 기다립니다.
            /// @return 렌더패스 동작이 실제로 끝나서 리턴했으면 true입니다.
            bool wait(uint64_t timeout = UINT64_MAX);
            /// @brief 프레임버퍼를 주어진 2D 타겟에 대하여 재구성합니다. 주어지는 타겟들은 렌더패스를 만들 때의 사양과 일치해야 합니다.
            /// @param targets RenderTarget 포인터 배열입니다.
            /// @param count targets의 길이로 이 값이 2 이상이면 모든 타겟의 크기가 동일해야 합니다.
            void reconstructFB(RenderTarget** targets, uint32_t count);
        private:
            RenderPass(VkRenderPass rp, VkFramebuffer fb, uint16_t stageCount); // 이후 다수의 서브패스를 쓸 수 있도록 변경
            ~RenderPass();
            const uint16_t stageCount;
            VkFramebuffer fb = VK_NULL_HANDLE;
            VkRenderPass rp = VK_NULL_HANDLE;
            std::vector<VkPipeline> pipelines;
            std::vector<VkPipelineLayout> pipelineLayouts;
            std::vector<RenderTarget*> targets;
            int currentPass = -1;
            VkViewport viewport;
            VkRect2D scissor;
            VkCommandBuffer cb = VK_NULL_HANDLE;
            const Mesh* bound = nullptr;
            
            
            VkFence fence = VK_NULL_HANDLE;
            VkSemaphore semaphore = VK_NULL_HANDLE;
    };

    class VkMachine::RenderPass2Screen{
        friend class VkMachine;
        public:
            RenderPass2Screen& operator=(const RenderPass2Screen&) = delete;
            /// @brief 뷰포트를 설정합니다. 기본 상태는 프레임버퍼 생성 당시의 크기들입니다. (즉 @ref reconstructFB 를 사용 시 여기서 수동으로 정한 값은 리셋됩니다.)
            /// 이것은 패스 내의 모든 파이프라인이 공유합니다.
            /// @param width 뷰포트 가로 길이(px)
            /// @param height 뷰포트 세로 길이(px)
            /// @param x 뷰포트 좌측 좌표(px, 맨 왼쪽이 0)
            /// @param y 뷰포트 상단 좌표(px, 맨 위쪽이 0)
            /// @param applyNow 이 값이 참이면 변경된 값이 파이프라인에 즉시 반영됩니다.
            void setViewport(float width, float height, float x, float y, bool applyNow = false);
            /// @brief 시저를 설정합니다. 기본 상태는 자름 없음입니다.
            /// 이것은 패스 내의 모든 파이프라인이 공유합니다.
            /// @param width 살릴 직사각형의 가로 길이(px)
            /// @param height 살릴 직사각형의 세로 길이(px)
            /// @param x 살릴 직사각형의 좌측 좌표(px, 맨 왼쪽이 0)
            /// @param y 살릴 직사각형의 상단 좌표(px, 맨 위쪽이 0)
            /// @param applyNow 이 값이 참이면 변경된 값이 파이프라인에 즉시 반영됩니다.
            void setScissor(uint32_t width, uint32_t height, int32_t x, int32_t y, bool applyNow = false);
            /// @brief 주어진 유니폼버퍼를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
            /// @param pos 바인드할 set 번호
            /// @param ub 바인드할 버퍼
            /// @param ubPos 버퍼가 동적 공유 버퍼인 경우, 그것의 몇 번째 성분을 바인드할지 정합니다. 아닌 경우 이 값은 무시됩니다.
            void bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos = 0);
            /// @brief 주어진 텍스처를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
            /// @param pos 바인드할 set 번호
            /// @param tx 바인드할 텍스처
            void bind(uint32_t pos, const pTexture& tx);
            /// @brief 주어진 렌더 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다. 이 패스의 프레임버퍼에서 사용 중인 렌더타겟은 (어차피 접근도 불가능하지만) 사용할 수 없습니다.
            /// @param pos 바인드할 set 번호
            /// @param target 바인드할 타겟
            /// @param index 렌더 타겟 내의 인덱스입니다. (0~2는 색 버퍼, 3은 깊이 버퍼)
            void bind(uint32_t pos, RenderTarget* target, uint32_t index);
            /// @brief 주어진 파이프라인을 주어진 서브패스에 사용하게 합니다. 이 함수는 매번 호출할 필요는 없지만, 해당 서브패스 진행 중에 사용하면 그때부터 바로 새로 주어진 파이프라인을 사용합니다.
            /// @param pipeline 파이프라인
            /// @param layout 파이프라인 레이아웃
            /// @param subpass 서브패스 번호
            void usePipeline(VkPipeline pipeline, VkPipelineLayout layout, uint32_t subpass);
            /// @brief 푸시 상수를 세팅합니다. 서브패스 진행중이 아니면 실패합니다.
            void push(void* input, uint32_t start, uint32_t end);
            /// @brief 메시를 그립니다. 정점 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
            void invoke(const pMesh&);
            /// @brief 메시를 그립니다. 정점/인스턴스 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
            /// @param mesh 
            /// @param instanceInfo 
            /// @param instanceCount 
            void invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount);
            /// @brief 서브패스를 시작합니다. 이미 서브패스가 시작된 상태라면 다음 서브패스를 시작하며, 다음 것이 없으면 아무 동작도 하지 않습니다. 주어진 파이프라인이 없으면 동작이 실패합니다.
            /// @param pos 이전 서브패스의 결과인 입력 첨부물을 바인드할 위치의 시작점입니다. 예를 들어, pos=0이고 이전 타겟이 색 첨부물 2개, 깊이 첨부물 1개였으면 0, 1, 2번에 바인드됩니다. 셰이더를 그에 맞게 만들어야 합니다.
            void start(uint32_t pos = 0);
            /// @brief 기록된 명령을 모두 수행합니다. 동작이 완료되지 않아도 즉시 리턴합니다.
            /// @param other 이 패스가 시작하기 전에 기다릴 다른 렌더패스입니다. 전후 의존성이 존재할 경우 사용하는 것이 좋습니다. (Vk세마포어 동기화를 사용) 현재 버전에서 기다리는 단계는 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 하나로 고정입니다.
            void execute(RenderPass* other = nullptr);
            /// @brief draw 수행 이후에 호출되면 가장 최근에 제출된 그리기 및 화면 표시 명령이 끝나고 나서 리턴합니다. 그 외의 경우는 그냥 리턴합니다.
            /// @param timeout 기다릴 최대 시간(ns), UINT64_MAX (~0) 값이 입력되면 무한정 기다립니다.
            /// @return 렌더패스 동작이 실제로 끝나서 리턴했으면 true입니다.
            bool wait(uint64_t timeout = UINT64_MAX);
            /// @brief 이 패스가 사용 가능한 상태인지 확인합니다. 사용 불가능하게 된 패스를 다시 사용할 수 있게 만들 방법은 없습니다.
            inline bool isAvailable() { return rp; }
        private:
            /// @brief 프레임버퍼 이미지를 처음부터 다시 만들어 프레임버퍼를 다시 생성합니다.
            /// @param width 가로 길이
            /// @param height 세로 길이
            /// @return 성공 여부 (실패 시 내부의 모든 데이터는 해제됨)
            bool reconstructFB(uint32_t width, uint32_t height);
            RenderPass2Screen(VkRenderPass rp, std::vector<RenderTarget*>&& targets, std::vector<VkFramebuffer>&& fbs, VkImage dsImage, VkImageView dsView, VmaAllocation dsAlloc);
            ~RenderPass2Screen();
            constexpr static uint32_t COMMANDBUFFER_COUNT = 4; // 트리플버퍼링 상정
            VkRenderPass rp = VK_NULL_HANDLE;

            std::vector<VkFramebuffer> fbs; // 스왑체인 이미지마다 하나씩
            std::vector<RenderTarget*> targets; // 마지막 단계 제외 서브패스마다 하나씩
            std::vector<VkPipeline> pipelines; // 서브패스마다 하나씩
            std::vector<VkPipelineLayout> pipelineLayouts; // 서브패스마다 하나씩

            VkImage dsImage;
            VkImageView dsView;
            VmaAllocation dsAlloc;

            VkCommandBuffer cbs[COMMANDBUFFER_COUNT] = {};
            VkFence fences[COMMANDBUFFER_COUNT] = {};
            VkSemaphore acquireSm[COMMANDBUFFER_COUNT] = {};
            VkSemaphore drawSm[COMMANDBUFFER_COUNT] = {}; // 하나로 같이 쓰면 낮은 확률로 화면 프레젠트가 먼저 실행될 수도 있음
            const Mesh* bound = nullptr;

            uint32_t currentCB = 0;
            uint32_t recently = 3;
            uint32_t width, height;
            
            int currentPass = -1;
            uint32_t imgIndex;
            VkViewport viewport;
            VkRect2D scissor;
    };

    class VkMachine::Texture{
        friend class VkMachine;
        friend class RenderPass;
        public:
            /// @brief 사용하지 않는 텍스처 데이터를 정리합니다.
            /// @param removeUsing 사용하는 텍스처 데이터도 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
            static void collect(bool removeUsing = false);
            /// @brief 주어진 이름의 텍스처 데이터를 내립니다. 사용하고 있는 텍스처 데이터는 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
            static void drop(int32_t name);
        protected:
            Texture(VkImage img, VkImageView imgView, VmaAllocation alloc, VkDescriptorSet dset, uint32_t binding);
            VkDescriptorSetLayout getLayout();
            ~Texture();
        private:
            VkImage img;
            VkImageView view;
            VmaAllocation alloc;
            VkDescriptorSet dset;
            uint32_t binding;
    };

    class VkMachine::Mesh{
        friend class VkMachine;
        public:
            /// @brief 사용하지 않는 메시 데이터를 정리합니다.
            /// @param removeUsing 사용하는 메시 데이터도 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getMesh로 찾을 수 없습니다.)
            static void collect(bool removeUsing = false);
            /// @brief 주어진 이름의 메시 데이터를 내립니다. 사용하고 있는 메시 데이터는 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getMesh로 찾을 수 없습니다.)
            static void drop(int32_t name);
            /// @brief 메모리 맵을 사용하도록 생성된 객체에 대하여 정점 속성 데이터를 업데이트합니다. 그 외의 경우 아무 동작도 하지 않습니다.
            /// @param input 입력 데이터
            /// @param offset 기존 데이터에서 수정할 시작점(바이트)입니다. (입력 데이터에서의 오프셋이 아닙니다. 0이 정점 버퍼의 시작점입니다.)
            /// @param size 기존 데이터에서 수정할 길이입니다.
            void update(const void* input, uint32_t offset, uint32_t size);
            /// @brief 메모리 맵을 사용하도록 생성된 객체에 대하여 정점 인덱스 데이터를 업데이트합니다. 그 외의 경우 아무 동작도 하지 않습니다.
            /// @param input 입력 데이터
            /// @param offset 기존 데이터에서 수정할 시작점(바이트)입니다. (입력 데이터에서의 오프셋이 아닙니다. 0이 인덱스 버퍼의 시작점입니다.)
            /// @param size 기존 데이터에서 수정할 길이입니다.
            void updateIndex(const void* input, uint32_t offset, uint32_t size);
        private:
            Mesh(VkBuffer vb, VmaAllocation vba, size_t vcount, size_t icount, size_t ioff, void *vmap, bool use32);
            ~Mesh();
            VkBuffer vb;
            VmaAllocation vba;
            size_t vcount, icount, ioff;
            VkIndexType idxType;
            void *vmap;
    };

    class VkMachine::UniformBuffer{
        friend class VkMachine;
        friend class RenderPass;
        friend class RenderPass2Screen;
        public:
            /// @brief 동적 공유 버퍼에 한하여 버퍼 원소 수를 주어진 만큼으로 조정합니다. 기존 데이터 중 주어진 크기 이내에 있는 것은 유지됩니다. 단, 어지간해서는 이 함수를 직/간접적으로 호출할 일이 없도록 첫 생성 시 크기(매개변수 length)를 적절히 마련합시다.
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
            inline uint32_t offset(uint32_t index) { return individual * index; }
            /// @brief 임시로 저장되어 있던 내용을 모두 GPU로 올립니다.
            void sync();
            UniformBuffer(uint32_t length, uint32_t individual, VkBuffer buffer, VkDescriptorSetLayout layout, VkDescriptorSet dset, VmaAllocation alloc, void* mmap, uint32_t binding);
            ~UniformBuffer();
            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            VkDescriptorSet dset = VK_NULL_HANDLE;
            VkBuffer buffer = VK_NULL_HANDLE;
            VmaAllocation alloc = nullptr;
            const bool isDynamic;
            bool shouldSync = false;
            uint32_t binding;
            const uint32_t individual; // 동적 공유 버퍼인 경우 (버퍼 업데이트를 위한) 개별 성분의 크기
            uint32_t length;
            std::vector<uint8_t> staged; // 순차접근을 효율적으로 활용하기 위해
            std::priority_queue<uint16_t, std::vector<uint16_t>, std::greater<uint16_t>> indices;
            void* mmap;
    };

    template<class FATTR, class... ATTR>
    struct VkMachine::Vertex{
        friend class VkMachine;
        inline static constexpr bool CHECK_TYPE() {
            if constexpr (sizeof...(ATTR) == 0) return is_one_of<FATTR, VERTEX_ATTR_TYPES>;
            else return is_one_of<FATTR, VERTEX_ATTR_TYPES> || Vertex<ATTR...>::CHECK_TYPE();
        }
    private:
        ftuple<FATTR, ATTR...> member;
        template<class F>
        inline static constexpr VkFormat getFormat(){
            if constexpr(is_one_of<F, VERTEX_FLOAT_TYPES>) {
                if constexpr(sizeof(F) / sizeof(float) == 1) return VK_FORMAT_R32_SFLOAT;
                if constexpr(std::is_same_v<F, vec2> || sizeof(F) / sizeof(float) == 2) return VK_FORMAT_R32G32_SFLOAT;
                if constexpr(std::is_same_v<F, vec3> || sizeof(F) / sizeof(float) == 3) return VK_FORMAT_R32G32B32_SFLOAT;
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            else if constexpr(is_one_of<F, VERTEX_DOUBLE_TYPES>) {
                if constexpr(sizeof(F) / sizeof(double) == 1) return VK_FORMAT_R64_SFLOAT;
                if constexpr(std::is_same_v<F, dvec2> || sizeof(F) / sizeof(double) == 2) return VK_FORMAT_R64G64_SFLOAT;
                if constexpr(std::is_same_v<F, dvec3> || sizeof(F) / sizeof(double) == 3) return VK_FORMAT_R64G64B64_SFLOAT;
                return VK_FORMAT_R64G64B64A64_SFLOAT;
            }
            else if constexpr(is_one_of<F, VERTEX_INT8_TYPES>) {
                if constexpr(sizeof(F) == 1) return VK_FORMAT_R8_SINT;
                if constexpr(sizeof(F) == 2) return VK_FORMAT_R8G8_SINT;
                if constexpr(sizeof(F) == 3) return VK_FORMAT_R8G8B8_SINT;
                return VK_FORMAT_R8G8B8A8_SINT;
            }
            else if constexpr(is_one_of<F, VERTEX_UINT8_TYPES>){
                if constexpr(sizeof(F) == 1) return VK_FORMAT_R8_UINT;
                if constexpr(sizeof(F) == 2) return VK_FORMAT_R8G8_UINT;
                if constexpr(sizeof(F) == 3) return VK_FORMAT_R8G8B8_UINT;
                return VK_FORMAT_R8G8B8A8_UINT;
            }
            else if constexpr(is_one_of<F, VERTEX_INT16_TYPES>){
                if constexpr(sizeof(F) == 2) return VK_FORMAT_R16_SINT;
                if constexpr(sizeof(F) == 4) return VK_FORMAT_R16G16_SINT;
                if constexpr(sizeof(F) == 6) return VK_FORMAT_R16G16B16_SINT;
                return VK_FORMAT_R16G16B16A16_SINT;
            }
            else if constexpr(is_one_of<F, VERTEX_UINT16_TYPES>){
                if constexpr(sizeof(F) == 2) return VK_FORMAT_R16_UINT;
                if constexpr(sizeof(F) == 4) return VK_FORMAT_R16G16_UINT;
                if constexpr(sizeof(F) == 6) return VK_FORMAT_R16G16B16_UINT;
                return VK_FORMAT_R16G16B16A16_UINT;
            }
            else if constexpr(is_one_of<F, VERTEX_INT32_TYPES>) {
                if constexpr(sizeof(F) / sizeof(int32_t) == 1) return VK_FORMAT_R32_SINT;
                if constexpr(std::is_same_v<F, ivec2> || sizeof(F) / sizeof(int32_t) == 2) return VK_FORMAT_R32G32_SINT;
                if constexpr(std::is_same_v<F, ivec3> || sizeof(F) / sizeof(int32_t) == 3) return VK_FORMAT_R32G32B32_SINT;
                return VK_FORMAT_R32G32B32A32_SINT;
            }
            else if constexpr(is_one_of<F, VERTEX_UINT32_TYPES>) {
                if constexpr(sizeof(F) / sizeof(uint32_t) == 1) return VK_FORMAT_R32_UINT;
                if constexpr(std::is_same_v<F, uvec2> || sizeof(F) / sizeof(uint32_t) == 2) return VK_FORMAT_R32G32_UINT;
                if constexpr(std::is_same_v<F, uvec3> || sizeof(F) / sizeof(uint32_t) == 3) return VK_FORMAT_R32G32B32_UINT;
                return VK_FORMAT_R32G32B32A32_UINT;
            }
            return VK_FORMAT_UNDEFINED; // UNREACHABLE
        }
    public:
        /// @brief 정점 속성 바인딩을 받아옵니다.
        /// @param vattrs 출력 위치
        /// @param binding 바인딩 번호
        /// @param locationPlus 셰이더 내의 location이 시작할 번호
        template<unsigned LOCATION = 0>
        inline static constexpr void info(VkVertexInputAttributeDescription* vattrs, uint32_t binding = 0, uint32_t locationPlus = 0){
            using A_TYPE = std::remove_reference_t<decltype(Vertex().get<LOCATION>())>;
            vattrs->binding = binding;
            vattrs->location = LOCATION + locationPlus;
            vattrs->format = getFormat<A_TYPE>();
            vattrs->offset = ftuple<FATTR, ATTR...>::template offset<LOCATION>();
            if constexpr(LOCATION < sizeof...(ATTR)) info<LOCATION + 1>(vattrs + 1, binding);
        }
        inline Vertex() {static_assert(CHECK_TYPE(), "One or more of attribute types are inavailable"); }
        inline Vertex(const FATTR& first, const ATTR&... rest):member(first, rest...) { static_assert(CHECK_TYPE(), "One or more of attribute types are inavailable"); }
        /// @brief 주어진 번호의 참조를 리턴합니다. 인덱스 초과 시 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS <= sizeof...(ATTR), bool> = false>
        constexpr inline auto& get() { return member.template get<POS>(); }
        /* template 키워드 설명
         * When the name of a member template specialization appears after . or -> in a postfix-expression,
         * or after nested-name-specifier in a qualified-id,
         * and the postfix-expression or qualified-id explicitly depends on a template-parameter (14.6.2),
         * the member template name must be prefixed by the keyword template.
         * Otherwise the name is assumed to name a non-template.
         * */
    };
}


#endif