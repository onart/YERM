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
#ifndef YR_USE_D3D11
#error "This project is not configured for using d3d 11. Please re-generate project with CMake or remove yr_d3d11.cpp from project"
#endif
#ifndef __YR_D3D11_H__
#define __YR_D3D11_H__
#include "yr_string.hpp"
#include "yr_tuple.hpp"

#include "yr_math.hpp"
#include "yr_threadpool.hpp"
#include <d3d11.h>

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
                VERTEX_INT8_TYPES, \
                VERTEX_UINT8_TYPES,\
                VERTEX_INT16_TYPES,\
                VERTEX_UINT16_TYPES,\
                VERTEX_INT32_TYPES,\
                VERTEX_UINT32_TYPES

template<class T, class... _VAC>
constexpr inline int __VATI = -1;

template<class T, class _VAC1, class... _VAC>
constexpr inline int __VATI<T, _VAC1, _VAC...> = 
    onart::is_one_of<T, _VAC1, _VAC...> ? 
        std::is_same_v<T, _VAC1> ? 0 :
        1 + __VATI<T, _VAC...>
    :
    -1
    ;

namespace onart {

    template<class T>
    constexpr inline int VERTEX_ATTR_TYPE_INDEX = __VATI<T, VERTEX_ATTR_TYPES>;

    class Window;

    class D3D11Machine {
        friend class Game;
    public:
        /// @brief 스레드에서 최근에 호출된 함수의 실패 요인을 일부 확인할 수 있습니다. D3D11 호출에 의한 실패가 아닌 경우 MAX_ENUM 값이 들어갑니다.
        static thread_local HRESULT reason;
        constexpr static bool VULKAN_GRAPHICS = false, D3D12_GRAPHICS = false, D3D11_GRAPHICS = true, OPENGL_GRAPHICS = false, OPENGLES_GRAPHICS = false, METAL_GRAPHICS = false;
        /// @brief D3D11 디버그 레이어를 사용하려면 이것을 활성화해 주세요.
        constexpr static bool USE_D3D11_DEBUG = true;
        /// @brief 그리기 대상입니다. 텍스처로 사용하거나 메모리 맵으로 데이터에 접근할 수 있습니다. 
        class RenderTarget;
        /// @brief 오프스크린용 렌더 패스입니다.
        class RenderPass;
        /// @brief 화면에 그리기 위한 렌더 패스입니다. 여러 개 갖고 있을 수는 있지만 동시에 여러 개를 사용할 수는 없습니다.
        using RenderPass2Screen = RenderPass;
        /// @brief 큐브맵에 그리기 위한 렌더 패스입니다.
        class RenderPass2Cube;
        /// @brief 직접 불러오는 텍스처입니다.
        class Texture;
        using pTexture = std::shared_ptr<Texture>;
        /// @brief 메모리 맵으로 고정된 텍스처입니다. 실시간으로 CPU단에서 데이터를 수정할 수 있습니다. 동영상이나 다른 창의 화면 등을 텍스처로 사용할 때 적합합니다.
        class StreamTexture;
        using pStreamTexture = std::shared_ptr<StreamTexture>;
        class Pipeline;
        /// @brief 속성을 직접 정의하는 정점 객체입니다.
        template<class, class...>
        struct Vertex;
        /// @brief 정점 버퍼와 인덱스 버퍼를 합친 것입니다. 사양은 템플릿 생성자를 통해 정할 수 있습니다.
        class Mesh;
        using pMesh = std::shared_ptr<Mesh>;
        /// @brief 셰이더 자원을 나타냅니다. 동시에 사용되지만 않는다면 여러 렌더패스 간에 공유될 수 있습니다.
        class UniformBuffer;
        using PipelineInputVertexSpec = D3D11_INPUT_ELEMENT_DESC;
        
        /// @brief 렌더 타겟의 유형입니다.
        enum RenderTargetType {
            /// @brief 색 버퍼 1개를 보유합니다.
            RTT_COLOR1 = 0b1,
            /// @brief 색 버퍼 2개를 보유합니다.
            RTT_COLOR2 = 0b11,
            /// @brief 색 버퍼 3개를 보유합니다.
            RTT_COLOR3 = 0b111,
            /// @brief 깊이 버퍼를 보유합니다.
            RTT_DEPTH = 0b1000,
            /// @brief 스텐실 버퍼를 보유합니다.
            RTT_STENCIL = 0b10000,
        };

        /// @brief 파이프라인 생성 시 줄 수 있는 옵션입니다. 여기의 성분들을 비트 or하여 생성 함수에 전달합니다.
        enum PipelineOptions : uint32_t {
            USE_DEPTH = 0b1,
            USE_STENCIL = 0b10,
            CULL_BACK = 0b100,
        };
        /// @brief 요청한 비동기 동작 중 완료된 것이 있으면 처리합니다.
        static void handle();
        /// @brief 원하는 비동기 동작을 요청합니다.
        /// @param work 다른 스레드에서 실행할 함수
        /// @param handler 호출되는 것
        /// @param strand 이 값이 값은 것들끼리는(0 제외) 동시에 다른 스레드에서 실행되지 않습니다. 그래픽스에 관련된 내용을 수행할 경우 1을 입력해 주세요.
        static void post(std::function<variant8(void)> work, std::function<void(variant8)> handler, uint8_t strand = 0);
        /// @brief 단위행렬이 리턴됩니다.
        static mat4 preTransform();

        struct RenderPassCreationOptions {
            /// @brief 타겟의 공통 크기입니다. 기본값 없음
            int width, height;
            /// @brief 서브패스 수입니다. Cube 대상의 렌더패스 생성 시에는 무시됩니다. 기본값 1
            uint32_t subpassCount = 1;
            /// @brief 타겟의 유형입니다. @ref RenderTargetType nullptr를 주면 모두 COLOR1로 취급되지만, nullptr를 주지 않는 경우에는 모든 것이 주어져야 합니다.
            /// Screen 대상의 RenderPass에서는 스왑체인인 마지막을 제외한 만큼 주어져야 합니다. 기본값 nullptr
            RenderTargetType* targets = nullptr;
            /// @brief 각 패스의 중간에 깊이 버퍼를 사용할 경우 그것을 input attachment로 사용할지 여부입니다. nullptr를 주면 일괄 false로 취급되며 그 외에는 모든 것이 주어져야 합니다. 기본값 nullptr
            bool* depthInput = nullptr;
            /// @brief true를 주면 최종 타겟을 텍스처로 사용할 때 linear 필터를 사용합니다. 기본값 true
            bool linearSampled = true;
            /// @brief screen 대상의 렌더패스의 최종 타겟에 depth 또는 stencil을 포함할지 결정합니다. 즉, RenderTargetType::DEPTH, RenderTargetType::STENCIL 이외에는 무시됩니다. 기본값 COLOR1
            RenderTargetType screenDepthStencil = RenderTargetType::RTT_COLOR1;
            /// @brief true일 경우 내용을 CPU 메모리로 읽어오거나 텍스처로 추출할 수 있습니다. RenderPass2Screen 및 RenderPass2Cube 생성 시에는 무시됩니다. 기본값 false
            bool canCopy = false;
            /// @brief 렌더패스 시작 시 모든 서브패스 타겟(색/깊이/스텐실)을 주어진 색으로 클리어합니다. 깊이/스텐실은 항상 1, 0으로 클리어합니다. vulkan API의 경우 autoclear를 사용하는 것이 더 성능이 높을 수 있습니다.
            struct {
                bool use = true;
                float color[4]{};
            }autoclear;
        };

        /// @brief 이미지 파일로부터 텍스처를 생성할 때 줄 수 있는 옵션입니다. 비트 OR로 여러 조건을 조합할 수 있습니다.
        enum TextureFormatOptions {
            /// @brief 원본이 BasisU인 경우: 품질을 우선으로 트랜스코드합니다. 그 외: 그대로 사용합니다.
            IT_PREFER_QUALITY = 0,
            /// @brief 원본이 BasisU인 경우: 작은 용량을 우선으로 트랜스코드합니다. 원본이 비압축 형식인 경우: 하드웨어에서 가능한 경우 압축하여 사용니다. 그 외: 그대로 사용합니다.
            IT_PREFER_COMPRESS = 1,
        };
        /// @brief 텍스처 생성에 사용하는 옵션입니다.
        struct TextureCreationOptions {
            /// @brief @ref ImageTextureFormatOptions 기본값 IT_PREFER_QUALITY
            TextureFormatOptions opts = IT_PREFER_QUALITY;
            /// @brief 확대 또는 축소 샘플링 시 true면 bilinear 필터를 사용합니다. false면 nearest neighbor 필터를 사용합니다. 기본값 true
            bool linearSampled = true;
            /// @brief 원본 텍스처가 srgb 공간에 있는지 여부입니다. 기본값 false
            bool srgb = false;
            /// @brief 이미지의 채널 수를 지정합니다. 이 값은 BasisU 텍스처에 대하여 사용되며 그 외에는 이 값을 무시하고 원본 이미지의 채널 수를 사용합니다. 기본값 4
            int nChannels = 4;
        };

        enum ShaderStage {
            VERTEX = 0b1,
            FRAGMENT = 0b10,
            GEOMETRY = 0b100,
            TESS_CTRL = 0b1000,
            TESS_EVAL = 0b10000,
            GRAPHICS_ALL = VERTEX | FRAGMENT | GEOMETRY | TESS_CTRL | TESS_EVAL
        };

        struct MeshCreationOptions {
            /// @brief 정점 데이터입니다. 기본값 없음
            void* vertices;
            /// @brief 정점 수입니다. 기본값 없음
            size_t vertexCount;
            /// @brief 개별 정점의 크기입니다. 기본값 없음
            size_t singleVertexSize;
            /// @brief 인덱스 데이터입니다. 기본값 nullptr
            void* indices = nullptr;
            /// @brief 인덱스 수입니다. 기본값 0
            size_t indexCount = 0;
            /// @brief 개별 인덱스의 크기입니다. 2 또는 4여야 합니다.
            size_t singleIndexSize = 0;
            /// @brief false인 경우 데이터를 수정할 수 있고 그러기 유리한 위치에 저장합니다. 기본값 true
            bool fixed = true;
        };

        struct UniformBufferCreationOptions {
            /// @brief 유니폼 버퍼의 크기입니다. 기본값 없음
            size_t size;
            /// @brief 유니폼 버퍼에 접근할 수 있는 셰이더 단계입니다. @ref ShaderStage 기본값 GRAPHICS_ALL
            uint32_t accessibleStages = ShaderStage::GRAPHICS_ALL;
            /// @brief 동적 유니폼 버퍼의 항목 수입니다. 1을 주면 동적 유니폼 버퍼로 만들어지지 않습니다. 기본값 1
            uint32_t count = 1;
        };

        struct ShaderModuleCreationOptions {
            /// @brief 컴파일된 바이너리입니다. 기본값 없음
            const void* source;
            /// @brief source의 크기(바이트)입니다. 기본값 없음
            size_t size;
            /// @brief 대상 셰이더 단계입니다. Vulkan에서는 사용하지 않습니다. 기본값 없음
            ShaderStage stage;
        };

        enum class ShaderResourceType : uint8_t {
            NONE = 0,
            UNIFORM_BUFFER_1 = 1,
            DYNAMIC_UNIFORM_BUFFER_1 = 2,
            TEXTURE_1 = 3,
            TEXTURE_2 = 4,
            TEXTURE_3 = 5,
            TEXTURE_4 = 6,
            INPUT_ATTACHMENT_1 = 7,
            INPUT_ATTACHMENT_2 = 8,
            INPUT_ATTACHMENT_3 = 9,
            INPUT_ATTACHMENT_4 = 10,
        };

        struct PipelineLayoutOptions {
            ShaderResourceType pos0 = ShaderResourceType::NONE;
            ShaderResourceType pos1 = ShaderResourceType::NONE;
            ShaderResourceType pos2 = ShaderResourceType::NONE;
            ShaderResourceType pos3 = ShaderResourceType::NONE;
            bool usePush = false;
        };

        enum class CompareOp {
            NEVER = D3D11_COMPARISON_NEVER,
            LESS = D3D11_COMPARISON_LESS,
            EQUAL = D3D11_COMPARISON_EQUAL,
            LTE = D3D11_COMPARISON_LESS_EQUAL,
            GREATER = D3D11_COMPARISON_GREATER,
            NE = D3D11_COMPARISON_NOT_EQUAL,
            GTE = D3D11_COMPARISON_GREATER_EQUAL,
            ALWAYS = D3D11_COMPARISON_ALWAYS,
        };

        enum class StencilOp {
            KEEP = D3D11_STENCIL_OP_KEEP,
            ZERO = D3D11_STENCIL_OP_ZERO,
            REPLACE = D3D11_STENCIL_OP_REPLACE,
            PLUS1_CLAMP = D3D11_STENCIL_OP_INCR_SAT,
            MINUSE1_CLAMP = D3D11_STENCIL_OP_DECR_SAT,
            INVERT = D3D11_STENCIL_OP_INVERT,
            PLUS1_WRAP = D3D11_STENCIL_OP_INCR,
            MINUS1_WRAP = D3D11_STENCIL_OP_DECR,
        };

        struct DepthStencilTesting {
            CompareOp comparison = CompareOp::LESS;
            bool depthTest = false;
            bool depthWrite = false;
            bool stencilTest = false;
            struct StencilWorks {
                StencilOp onFail = StencilOp::KEEP;
                StencilOp onDepthFail = StencilOp::KEEP;
                StencilOp onPass = StencilOp::KEEP;
                CompareOp compare = CompareOp::ALWAYS;
                uint32_t reference = 0;
                uint32_t writeMask = 0xff;
                uint32_t compareMask = 0xff;
            }stencilFront, stencilBack;
        };

        enum class BlendOperator {
            ADD = D3D11_BLEND_OP_ADD,
            SUBTRACT = D3D11_BLEND_OP_SUBTRACT,
            REVERSE_SUBTRACT = D3D11_BLEND_OP_REV_SUBTRACT,
            MINIMUM = D3D11_BLEND_OP_MIN,
            MAXIMUM = D3D11_BLEND_OP_MAX,
        };

        enum class BlendFactor {
            ZERO = D3D11_BLEND_ZERO,
            ONE = D3D11_BLEND_ONE,
            SRC_COLOR = D3D11_BLEND_SRC_COLOR,
            ONE_MINUS_SRC_COLOR = D3D11_BLEND_INV_SRC_COLOR,
            DST_COLOR = D3D11_BLEND_DEST_COLOR,
            ONE_MINUS_DST_COLOR = D3D11_BLEND_INV_DEST_COLOR,
            SRC_ALPHA = D3D11_BLEND_SRC_ALPHA,
            ONE_MINUS_SRC_ALPHA = D3D11_BLEND_INV_SRC_ALPHA,
            DST_ALPHA = D3D11_BLEND_DEST_ALPHA,
            ONE_MINUS_DST_ALPHA = D3D11_BLEND_INV_DEST_ALPHA,
            CONSTANT_COLOR = D3D11_BLEND_BLEND_FACTOR,
            ONE_MINUS_CONSTANT_COLOR = D3D11_BLEND_INV_BLEND_FACTOR,
            //CONSTANT_ALPHA = D3D11_BLEND_BLEND_FACTOR,
            //ONE_MINUS_CONSTANT_ALPHA = D3D11_BLEND_INV_BLEND_FACTOR,
            SRC_ALPHA_SATURATE = D3D11_BLEND_SRC_ALPHA_SAT,
            SRC1_COLOR = D3D11_BLEND_SRC1_COLOR,
            ONE_MINUS_SRC1_COLOR = D3D11_BLEND_INV_SRC1_COLOR,
            SRC1_ALPHA = D3D11_BLEND_SRC1_ALPHA,
            ONE_MINUS_SRC1_ALPHA = D3D11_BLEND_INV_SRC1_ALPHA,
        };

        struct AlphaBlend {
            BlendOperator colorOp = BlendOperator::ADD;
            BlendOperator alphaOp = BlendOperator::ADD;
            BlendFactor srcColorFactor = BlendFactor::ONE;
            BlendFactor dstColorFactor = BlendFactor::ZERO;
            BlendFactor srcAlphaFactor = BlendFactor::ONE;
            BlendFactor dstAlphaFactor = BlendFactor::ZERO;
            inline constexpr bool operator== (const AlphaBlend& other) const {
#define COMP_ATTR(name) (name == other.name)
                return COMP_ATTR(colorOp) && COMP_ATTR(alphaOp) && COMP_ATTR(srcColorFactor) && COMP_ATTR(dstColorFactor) && COMP_ATTR(srcAlphaFactor) && COMP_ATTR(dstAlphaFactor);
#undef COMP_ATTR
            }
            inline constexpr bool operator!=(const AlphaBlend& other) const { return !operator==(other); }
            inline static constexpr AlphaBlend overwrite() { return AlphaBlend{}; }
            inline static constexpr AlphaBlend normal() { return AlphaBlend{ BlendOperator::ADD, BlendOperator::ADD, BlendFactor::SRC_ALPHA, BlendFactor::ONE_MINUS_SRC_ALPHA, BlendFactor::ONE, BlendFactor::ONE_MINUS_SRC_ALPHA }; }
            inline static constexpr AlphaBlend pma() { return AlphaBlend{ BlendOperator::ADD, BlendOperator::ADD, BlendFactor::ONE, BlendFactor::ONE_MINUS_SRC_ALPHA, BlendFactor::ONE, BlendFactor::ONE_MINUS_SRC_ALPHA }; }
        };

        struct PipelineCreationOptions {
            PipelineInputVertexSpec* vertexSpec = nullptr;
            uint32_t vertexSize = 0;
            uint32_t vertexAttributeCount = 0;
            PipelineInputVertexSpec* instanceSpec = nullptr;
            uint32_t instanceDataStride = 0;
            uint32_t instanceAttributeCount = 0;
            RenderPass* pass = nullptr;
            RenderPass2Screen* pass2screen = nullptr;
            uint32_t subpassIndex = 0;
            ID3D11DeviceChild* vertexShader;
            ID3D11DeviceChild* fragmentShader;
            ID3D11DeviceChild* geometryShader = nullptr;
            ID3D11DeviceChild* tessellationControlShader = nullptr;
            ID3D11DeviceChild* tessellationEvaluationShader = nullptr;
            PipelineLayoutOptions shaderResources;
            DepthStencilTesting depthStencil;
            AlphaBlend alphaBlend[3];
            float blendConstant[4]{};
            /// @brief 정점 input layout을 생성하기 위한 정점 셰이더 바이트코드입니다. 단, vsByteCodeSize 값이 0으로 주어지는 경우 이 포인터는 "다른 파이프라인"으로 취급되며 해당 파이프라인과 동일한 레이아웃을 사용합니다.
            void* vsByteCode = nullptr;
            size_t vsByteCodeSize = 0;
        };

        /// @brief 복사 영역을 지정합니다.
        struct TextureArea2D {
            /// @brief 복사 영역의 x좌표(px)를 설정합니다. 왼쪽이 0입니다. 기본값 0
            uint32_t x = 0;
            /// @brief 복사 영역의 y좌표(px)를 설정합니다. 위쪽이 0입니다. 기본값 0
            uint32_t y = 0;
            /// @brief 복사 영역의 가로 길이(px)를 설정합니다. 0이면 x, y, height에 무관하게 전체가 복사됩니다. 기본값 0
            uint32_t width = 0;
            /// @brief 복사 영역의 세로 길이(px)를 설정합니다. 0이면 x, y, width에 무관하게 전체가 복사됩니다. 기본값 0
            uint32_t height = 0;
        };

        struct RenderTarget2TextureOptions {
            /// @brief 0~2: 타겟의 해당 번호의 색 버퍼를 복사합니다. 3~: 현재 지원하지 않습니다. 기본값 0
            uint32_t index = 0;
            /// @brief true인 경우 결과 텍스처의 샘플링 방식이 linear로 수행됩니다. 기본값 false
            bool linearSampled = false;
            /// @brief 복사 영역을 지정합니다. @ref TextureArea2D
            TextureArea2D area;
        };

        /// @brief 픽셀 데이터를 통해 텍스처 객체를 생성합니다. 밉 수준은 반드시 1입니다.
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 입력과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param color 픽셀 데이터입니다.
        /// @param width 가로 길이(px)이며, 패딩은 없음으로 가정합니다.
        /// @param height 세로 길이(px)
        /// @param opts @ref TextureCreationOptions
        /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
        static pTexture createTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, const TextureCreationOptions& opts = {});
        /// @brief 픽셀 데이터를 통해 텍스처 객체를 생성합니다. 밉 수준은 반드시 1입니다.
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 입력과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param color 픽셀 데이터입니다.
        /// @param width 가로 길이(px)이며, 패딩은 없음으로 가정합니다.
        /// @param height 세로 길이(px)
        /// @param opts @ref TextureCreationOptions
        /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
        static void aysncCreateTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, std::function<void(variant8)> handler, const TextureCreationOptions& opts = {});
        /// @brief 보통 이미지 파일을 불러와 텍스처를 생성합니다. 밉 수준은 반드시 1이며 그 이상을 원하는 경우 ktx2 형식을 이용해 주세요.
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param fileName 파일 이름
        /// @param opts @ref TextureCreationOptions
        /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
        static pTexture createTextureFromImage(int32_t key, const char* fileName, const TextureCreationOptions& opts = {});
        /// @brief @ref createTextureFromImage를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아닌 그냥 key). 매개변수 설명은 createTextureFromImage를 참고하세요.
        static void asyncCreateTextureFromImage(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts = {});
        /// @brief 메모리 내의 이미지 파일을 통해 텍스처를 생성합니다. 밉 수준은 반드시 1이며 그 이상을 원하는 경우 ktx2 형식을 이용해 주세요.
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param mem 데이터 위치
        /// @param size 데이터 길이
        /// @param opts @ref TextureCreationOptions
        /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
        static pTexture createTextureFromImage(int32_t key, const void* mem, size_t size, const TextureCreationOptions& opts = {});
        /// @brief @ref createTextureFromImage를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아닌 그냥 key). 매개변수 설명은 createTextureFromImage를 참고하세요.
        static void asyncCreateTextureFromImage(int32_t key, const void* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts = {});
        /// @brief ktx2 파일을 불러와 텍스처를 생성합니다. @ref 
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param fileName 파일 이름
        /// @param opts @ref TextureCreationOptions
        /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
        static pTexture createTexture(int32_t key, const char* fileName, const TextureCreationOptions& opts = {});
        /// @brief createTexture를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아니라 그냥 key). 매개변수 설명은 createTexture를 참고하세요.
        static void asyncCreateTexture(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts = {});
        /// @brief 메모리 상의 ktx2 파일을 통해 텍스처를 생성합니다. (KTX2 파일이라도 BasisU가 아니면 실패할 가능성이 있습니다.) 여기에도 libktx로 그 형식을 만드는 별도의 도구가 있으니 필요하면 사용할 수 있습니다.
        /// @param key 프로그램 내부에서 사용할 이름입니다. 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param mem 이미지 시작 주소
        /// @param size mem 배열의 길이(바이트)
        /// @param opts @ref TextureCreationOptions
        /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
        static pTexture createTexture(int32_t key, const uint8_t* mem, size_t size, const TextureCreationOptions& opts = {});
        /// @brief createTexture를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아니라 그냥 key). 매개변수 설명은 createTexture를 참고하세요.
        static void asyncCreateTexture(int32_t key, const uint8_t* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts = {});
        /// @brief 빈 텍스처를 만듭니다. 메모리 맵으로 데이터를 올릴 수 있습니다. 올리는 데이터의 기본 형태는 BGRA 순서이며, 필요한 경우 셰이더에서 직접 스위즐링하여 사용합니다.
        static pStreamTexture createStreamTexture(int32_t key, uint32_t width, uint32_t height, bool linearSampler = true);
        /// @brief SPIR-V 컴파일된 셰이더를 VkShaderModule 형태로 저장하고 가져옵니다.
        /// @param key 이후 별도로 접근할 수 있는 이름을 지정합니다. 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다.
        /// @param opts @ref ShaderModuleCreationOptions
        static ID3D11DeviceChild* createShader(int32_t key, const ShaderModuleCreationOptions& opts);
        /// @brief 셰이더에서 사용할 수 있는 uniform 버퍼를 생성하여 리턴합니다. 이것을 해제하는 방법은 없으며, 프로그램 종료 시 자동으로 해제됩니다.
        /// @param key 프로그램 내에서 사용할 이름입니다. 중복된 이름이 입력된 경우 주어진 나머지 인수를 무시하고 그 이름을 가진 버퍼를 리턴합니다.
        /// @param opts @ref UniformBufferCreationOptions
        static UniformBuffer* createUniformBuffer(int32_t key, const UniformBufferCreationOptions& opts);
        /// @brief 렌더 패스를 생성합니다. 렌더 패스는 렌더 타겟과 유의어로 보아도 되나, 여러 개의 서브패스로 구성됩니다.
            /// @param key 프로그램 내에서 사용할 이름입니다. 중복된 이름이 입력된 경우 주어진 나머지 인수를 무시하고 그 이름을 가진 버퍼를 리턴합니다.
        static RenderPass* createRenderPass(int32_t key, const RenderPassCreationOptions& opts);
        /// @brief 큐브맵 대상의 렌더패스를 생성합니다.
        /// @param width 타겟으로 생성되는 각 이미지의 가로 길이입니다.
        /// @param height 타겟으로 생성되는 각 이미지의 세로 길이입니다.
        /// @param key 이름입니다.
        /// @param useColor true인 경우 색 버퍼 1개를 이미지에 사용합니다.
        /// @param useDepth true인 경우 깊이 버퍼를 이미지에 사용합니다. useDepth와 useColor가 모두 true인 경우 샘플링은 색 버퍼에 대해서만 가능합니다.
        static RenderPass2Cube* createRenderPass2Cube(int32_t key, uint32_t width, uint32_t height, bool useColor, bool useDepth);
        /// @brief 화면으로 이어지는 렌더패스를 생성합니다. 각 패스의 타겟들은 현재 창의 해상도와 동일하게 맞춰집니다.
        /// @param key 이름입니다. (RenderPass 객체와 같은 집합을 공유하지 않음) 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다. INT32_MIN, 즉 -2147483648은 예약된 값이기 때문에 사용할 수 없습니다.
        static RenderPass2Screen* createRenderPass2Screen(int32_t key, int32_t windowIdx, const RenderPassCreationOptions& opts);
        /// @brief 파이프라인을 생성합니다. 생성된 파이프라인은 이후에 이름으로 불러올 수도 있고, 주어진 렌더패스의 해당 서브패스 위치로 들어갑니다.
        static Pipeline* createPipeline(int32_t key, const PipelineCreationOptions& opts);
        /// @brief 정점 버퍼를 생성합니다.
        /// @param key 사용할 이름입니다. 중복된 이름을 입력하는 경우 기존의 Mesh를 리턴합니다.
        /// @param opts @ref MeshCreationOptions
        static pMesh createMesh(int32_t key, const MeshCreationOptions& opts);
        /// @brief 정점의 수 정보만 저장하는 메시 객체를 생성합니다. 이것은 파이프라인 자체에서 정점 데이터가 정의되어 있는 경우를 위한 것입니다.
        /// @param vcount 정점의 수
        /// @param name 프로그램 내에서 사용할 이름입니다.
        static pMesh createNullMesh(int32_t key, size_t vcount);
        /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static RenderPass2Screen* getRenderPass2Screen(int32_t key);
        /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static RenderPass* getRenderPass(int32_t key);
        /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static RenderPass2Cube* getRenderPass2Cube(int32_t key);
        /// @brief 만들어 둔 파이프라인을 리턴합니다. 없으면 nullptr를 리턴합니다.
        static Pipeline* getPipeline(int32_t key);
        /// @brief 만들어 둔 공유 버퍼를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static UniformBuffer* getUniformBuffer(int32_t key);
        /// @brief 만들어 둔 셰이더 모듈을 리턴합니다. 없으면 0을 리턴합니다.
        static ID3D11DeviceChild* getShader(int32_t key);
        /// @brief 올려 둔 텍스처 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
        static pTexture getTexture(int32_t key);
        /// @brief 만들어 둔 메시 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
        static pMesh getMesh(int32_t key);
        /// @brief 해제되어야 할 자원을 안전하게 해제합니다. 각 자원의 collect와 이것의 호출 주기는 적절하게 설정할 필요가 있습니다.
        static void reap();
        /// @brief 모든 창의 수직 동기화 여부를 설정합니다.
        static void setVsync(bool vsyncOn);
        /// @brief 창 표면이 SRGB 공간을 사용하는지 리턴합니다. 
        inline static bool isSurfaceSRGB() { return false; }
        /// @brief 아무 것도 하지 않습니다.
        inline static int getTextureLayout(uint32_t binding) { return 0; }
        /// @brief 아무 것도 하지 않습니다.
        inline static int getInputAttachmentLayout(uint32_t binding) { return 0; }
    private:
        /// @brief 기본 OpenGL 컨텍스트를 생성합니다.
        D3D11Machine();
        /// @brief 주어진 창에 렌더링할 수 있도록 정보를 추가합니다.
            /// @return 정상적으로 추가되었는지 여부
        bool addWindow(int32_t key, Window* window);
        /// @brief 창을 제거합니다. 해당 창에 연결되었던 모든 렌더패스는 제거됩니다.
        void removeWindow(int32_t key);
        /// @brief 창 크기 또는 스타일이 변경되거나 최소화/복원할 때 호출하여 스왑체인을 재생성합니다.
        void resetWindow(int32_t key, bool = false);
        /// @brief ktxTexture2 객체로 텍스처를 생성합니다.
        pTexture createTexture(void* ktxObj, int32_t key, const TextureCreationOptions& opts);
        /// @brief 렌더패스에서 사용할 2D 렌더타겟을 생성합니다.
        static RenderTarget* createRenderTarget2D(int width, int height, RenderTargetType type, bool useDepthInput, bool sampled, bool linear);
        /// @brief vulkan 객체를 없앱니다.
        void free();
        ~D3D11Machine();
        /// @brief 이 클래스 객체는 Game 밖에서는 생성, 소멸 호출이 불가능합니다.
        inline void operator delete(void* p) { ::operator delete(p); }
    private:
        static D3D11Machine* singleton;
        static uint64_t currentRenderPass;
        ID3D11Device* device{};
        ID3D11DeviceContext* context{};
        class WindowSystem;
        std::map<int32_t, WindowSystem*> windowSystems;
        int vsync = 1;

        bool canUseBC7 = false;

        ThreadPool loadThread;
        std::map<int32_t, RenderPass*> renderPasses;
        std::map<int32_t, RenderPass2Screen*> finalPasses;
        std::map<int32_t, RenderPass2Cube*> cubePasses;
        std::map<int32_t, RenderTarget*> renderTargets;
        std::map<int32_t, ID3D11DeviceChild*> shaders;
        std::map<int32_t, UniformBuffer*> uniformBuffers;
        std::map<int32_t, Pipeline*> pipelines;
        std::map<int32_t, pMesh> meshes;
        std::map<int32_t, pTexture> textures;
        std::map<int32_t, pStreamTexture> streamTextures;

        ID3D11SamplerState* linearBorderSampler;
        ID3D11SamplerState* nearestBorderSampler;
        ID3D11RasterizerState* basicRasterizer;

        struct ImageSet {
            ID3D11Resource* tex{};
            ID3D11ShaderResourceView* srv{};
            void free();
            ~ImageSet();
        };

        std::mutex textureGuard;

        std::vector<std::shared_ptr<RenderPass>> passes;
        enum vkm_strand {
            NONE = 0,
            GENERAL = 1,
        };
    };

    class D3D11Machine::WindowSystem {
        friend class D3D11Machine;
        struct {
            IDXGISwapChain* handle{};
            int width;
            int height;
        }swapchain;
        /// @brief 스왑체인 대상의 유효한 렌더 타겟을 얻습니다.
        ID3D11RenderTargetView* getSwapchainTarget();
        ID3D11DepthStencilView* screenDSView{};
        Window* window = nullptr;
    private:
        std::map<ID3D11Texture2D*, ID3D11RenderTargetView*> screenTargets;
        void resizeSwapchain();
        WindowSystem(Window*);
        ~WindowSystem();
    };

    class D3D11Machine::Texture {
        friend class D3D11Machine;
        friend class RenderPass;
    public:
        /// @brief 사용하지 않는 텍스처 데이터를 정리합니다.
        /// @param removeUsing 사용하는 텍스처 데이터도 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
        static void collect(bool removeUsing = false);
        /// @brief 주어진 이름의 텍스처 데이터를 내립니다. 사용하고 있는 텍스처 데이터는 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
        static void drop(int32_t name);
        /// @brief 원본 텍스처의 크기입니다.
        const uint16_t width, height;
    protected:
        Texture(ID3D11Resource* texture, ID3D11ShaderResourceView* dset, uint16_t width, uint16_t height, bool isCubemap, bool linearSampled);
        ~Texture();
    private:
        ID3D11Resource* texture;
        ID3D11ShaderResourceView* dset;
        const bool isCubemap;
        bool linearSampled;
    };

    class D3D11Machine::RenderTarget {
        friend class D3D11Machine;
        friend class RenderPass;
    public:
        RenderTarget& operator=(const RenderTarget&) = delete;
    private:
        ID3D11RenderTargetView* dset1{}, * dset2{}, * dset3{};
        ID3D11DepthStencilView* dsetDS{};
        ImageSet* color1{}, *color2{}, *color3{}, *ds{};
        unsigned width, height;
        const bool linearSampled;
        const bool depthInput;
        const RenderTargetType type;
        RenderTarget(RenderTargetType type, unsigned width, unsigned height, ImageSet**, ID3D11RenderTargetView**, ID3D11DepthStencilView*, bool, bool);
        ~RenderTarget();
    };

    class D3D11Machine::StreamTexture {
        friend class D3D11Machine;
        friend class RenderPass;
    public:
        /// @brief 사용하지 않는 텍스처 데이터를 정리합니다.
        /// @param removeUsing 사용하는 텍스처 데이터도 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
        static void collect(bool removeUsing = false);
        /// @brief 주어진 이름의 텍스처 데이터를 내립니다. 사용하고 있는 텍스처 데이터는 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
        static void drop(int32_t name);
        /// @brief 이미지 데이터를 다시 설정합니다.
        void update(void* img);
        const uint16_t width, height;
    protected:
        StreamTexture(ID3D11Texture2D* txo, ID3D11ShaderResourceView* srv, uint16_t width, uint16_t height, bool linearSampler, void* mmap, uint64_t rowPitch);
        ~StreamTexture();
    private:
        ID3D11Texture2D* txo;
        ID3D11ShaderResourceView* dset;
        bool linearSampled;
        void* mmap;
        uint64_t rowPitch;
        const bool copyFull;
    };

    class D3D11Machine::RenderPass {
        friend class D3D11Machine;
    public:
        struct ReadBackBuffer {
            uint8_t* data;
            int32_t key;
        };
    public:
        RenderPass& operator=(const RenderPass&) = delete;
        /// @brief 뷰포트를 설정합니다. 기본 상태는 프레임버퍼 생성 당시의 크기들입니다. (즉 @ref resize 를 사용 시 여기서 수동으로 정한 값은 리셋됩니다.)
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
        /// @brief 주어진 렌더패스의 최종 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다.
        void bind(uint32_t pos, RenderPass* prev);
        /// @brief 주어진 렌더패스의 최종 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다.
        void bind(uint32_t pos, RenderPass2Cube* prev);
        /// @brief 주어진 텍스처를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
        /// @param pos 바인드할 set 번호
        /// @param tx 바인드할 텍스처
        void bind(uint32_t pos, const pStreamTexture& tx);
        /// @brief 주어진 파이프라인을 사용하게 합니다.
        /// @param pipeline 파이프라인
        /// @param subpass 서브패스 번호
        void usePipeline(Pipeline* pipeline, unsigned subpass);
        /// @brief 푸시 상수를 세팅합니다. 서브패스 진행중이 아니면 실패합니다.
        void push(void* input, uint32_t start, uint32_t end);
        /// @brief 메시를 그립니다. 정점 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
        /// @param start 정점 시작 위치 (주어진 메시에 인덱스 버퍼가 있는 경우 그것을 기준으로 합니다.)
        /// @param count 정점 수. 0이 주어진 경우 주어진 start부터 끝까지 그립니다.
        void invoke(const pMesh&, uint32_t start = 0, uint32_t count = 0);
        /// @brief 메시를 그립니다. 정점 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
        /// @param mesh 기본 정점버퍼
        /// @param instanceInfo 인스턴스 속성 버퍼
        /// @param instanceCount 인스턴스 수
        /// @param istart 인스턴스 시작 위치
        /// @param start 정점 시작 위치 (주어진 메시에 인덱스 버퍼가 있는 경우 그것을 기준으로 합니다.)
        /// @param count 정점 수. 0이 주어진 경우 주어진 start부터 끝까지 그립니다.
        void invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart = 0, uint32_t start = 0, uint32_t count = 0);
        /// @brief 현재 서브패스의 타겟을 클리어합니다.
        /// @param toClear 실제로 클리어할 타겟을 명시합니다.
        /// @param colors 초기화할 색상을 앞에서부터 차례대로 (r, g, b, a) 명시합니다. depth/stencil 타겟은 각각 고정 1 / 0으로 클리어됩니다.
        void clear(RenderTargetType toClear, float* colors);
        /// @brief 서브패스를 시작합니다. 이미 서브패스가 시작된 상태라면 다음 서브패스를 시작하며, 다음 것이 없으면 아무 동작도 하지 않습니다. 주어진 파이프라인이 없으면 동작이 실패합니다.
        /// @param pos 이전 서브패스의 결과인 입력 첨부물을 바인드할 위치의 시작점입니다. 예를 들어, pos=0이고 이전 타겟이 색 첨부물 2개, 깊이 첨부물 1개였으면 0, 1, 2번에 바인드됩니다. 셰이더를 그에 맞게 만들어야 합니다.
        void start(uint32_t pos = 0);
        /// @brief 기록된 명령을 모두 수행합니다. 동작이 완료되지 않아도 즉시 리턴합니다.
        /// @param other 이 패스가 시작하기 전에 기다릴 다른 렌더패스입니다. 전후 의존성이 존재할 경우 사용하는 것이 좋습니다. (Vk세마포어 동기화를 사용) 현재 버전에서 기다리는 단계는 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 하나로 고정입니다.
        void execute(RenderPass* other = nullptr);
        /// @brief true를 리턴합니다.
        bool wait(uint64_t timeout = UINT64_MAX);
        /// @brief 렌더타겟의 크기를 일괄 변경합니다. RenderPass2Screen에 대해서는 사용할 수 없습니다.
        void resize(int width, int height, bool linearSampled = true);
        /// @brief 렌더타겟에 직전 execute 이후 그려진 내용을 별도의 텍스처로 복사합니다. 텍스처는 DEFAULT usage로 생성되며, IMMUTABLE을 원하는 경우 readBack의 결과를 이용하여 텍스처 생성 함수를 별도로 호출해 주세요.
        /// @param key 텍스처 키입니다.
        pTexture copy2Texture(int32_t key, const RenderTarget2TextureOptions& opts = {});
        /// @brief 렌더타겟에 직전 execute 이후 그려진 내용을 별도의 텍스처로 복사합니다. 동기화의 편의를 위해 DEFAULT usage로 생성되며, IMMUTABLE을 원하는 경우 readBack의 결과를 이용하여 텍스처 생성 함수를 별도로 호출해 주세요.
        void asyncCopy2Texture(int32_t key, std::function<void(variant8)> handler, const RenderTarget2TextureOptions& opts = {});
        /// @brief 렌더타겟에 직전 execute 이후 그려진 내용을 CPU 메모리에 작성합니다. 포맷은 렌더타겟과 동일합니다. 현재 depth/stencil 버퍼는 항상 24/8 포맷임에 유의해 주세요.
        std::unique_ptr<uint8_t[]> readBack(uint32_t index, const TextureArea2D& area = {});
        /// @brief 렌더타겟에 그려진 내용을 CPU 메모리에 비동기로 작성합니다. asyncReadBack 호출 시점보다 뒤에 그려진 내용이 캡처될 수 있으며 이 사양은 추후 변할 수 있습니다. 포맷은 렌더타겟과 동일합니다. 동기화 편의를 위하여 handle() 여러 번에 걸쳐 수행됩니다.
        /// @param key 핸들러에 전달될 키입니다.
        /// @param handler 비동기 핸들러입니다. @ref ReadBackBuffer의 포인터가 전달되며 해당 메모리는 자동으로 해제되므로 핸들러에서는 읽기만 가능합니다.
        void asyncReadBack(int32_t key, uint32_t index, std::function<void(variant8)> handler, const TextureArea2D& area = {});
    private:
        RenderPass(uint16_t stageCount, bool canBeRead, float* autoclear);
        ~RenderPass();
        const uint16_t stageCount;
        std::vector<Pipeline*> pipelines;
        std::vector<RenderTarget*> targets;
        int currentPass = -1;
        int32_t windowIdx;
        bool is4Screen = false;
        D3D11_VIEWPORT viewport;
        D3D11_RECT scissor;
        const static Mesh* bound;
        const bool canBeRead;
        bool autoclear;
        float clearColor[4];
    };

    /// @brief 큐브맵 대상의 렌더패스입니다.
    /// 현 버전에서는 지오메트리 셰이더를 통한 레이어드 렌더링을 제공하지 않으며 고정적으로 6개의 서브패스를 활용합니다.
    /// execute 한 번에 6개를 모두 실행합니다.
    /// 각 그리기 명령 간의 시야 차이를 위하여 유니폼버퍼 바인드 시에만 몇 번째 패스에 주어진 바인드를 적용할지 명시하도록 만들어 두었습니다.
    /// 현 버전에서는 기본적으로 RenderPass2Cube의 큐브맵 텍스처 바인딩은 1번입니다.
    class D3D11Machine::RenderPass2Cube {
        friend class D3D11Machine;
    public:
        RenderPass2Cube& operator=(const RenderPass2Cube&) = delete;
        /// @brief 주어진 유니폼버퍼를 바인드합니다.
        /// @param pos 바인드할 set 번호 (셰이더 내에서)
        /// @param ub 바인드할 유니폼 버퍼
        /// @param pass 이 버퍼를 사용할 면으로, 0~5만 가능합니다. (큐브맵 샘플 기준 방향은 0번부터 +x, -x, +y, -y, +z, -z입니다.) 그 외의 값을 주는 경우 패스 인수와 관계 없는 것으로 인식하고 공통으로 바인드합니다. (주의. 바인드할 수 있는 버퍼는 하나뿐입니다. 여러 번 호출되면 기존 명령을 덮어쓰며 한 번 기록한 것은 렌더패스 실행이 완료되어도 사라지지 않습니다.)
        /// @param ubPos 동적 유니폼 버퍼인 경우 그 중 몇 번째 셋을 바인드할지 정합니다. 동적 유니폼 버퍼가 아니면 무시됩니다.
        void bind(uint32_t pos, UniformBuffer* ub, uint32_t pass = 6, uint32_t ubPos = 0);
        /// @brief 주어진 텍스처를 바인드합니다.
        /// @param pos 바인드할 set 번호
        /// @param tx 바인드할 텍스처
        void bind(uint32_t pos, const pTexture& tx);
        /// @brief 주어진 렌더패스의 최종 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다.
        void bind(uint32_t pos, RenderPass* prev);
        /// @brief 주어진 렌더패스의 최종 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다.
        void bind(uint32_t pos, RenderPass2Cube* prev);
        /// @brief 주어진 텍스처를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
        /// @param pos 바인드할 set 번호
        /// @param tx 바인드할 텍스처
        void bind(uint32_t pos, const pStreamTexture& tx);
        /// @brief 주어진 파이프라인을 사용합니다. 지오메트리 셰이더를 사용하도록 만들어진 패스인지 아닌지를 잘 보고 바인드해 주세요.
        void usePipeline(Pipeline* pipeline);
        /// @brief 푸시 상수를 세팅합니다. 서브패스 진행중이 아니면 실패합니다.
        void push(void* input, uint32_t start, uint32_t end);
        /// @brief 메시를 그립니다. 정점 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
        void invoke(const pMesh&, uint32_t start = 0, uint32_t count = 0);
        /// @brief 메시를 그립니다. 정점/인스턴스 사양은 파이프라인과 맞아야 하며, 현재 바인드된 파이프라인이 그렇지 않은 경우 usePipeline으로 다른 파이프라인을 등록해야 합니다.
        void invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart = 0, uint32_t start = 0, uint32_t count = 0);
        /// @brief 서브패스를 시작합니다. 이미 시작된 상태인 경우 아무 동작도 하지 않습니다.
        void start();
        /// @brief true를 리턴합니다.
        bool wait(uint64_t timeout = UINT64_MAX);
        /// @brief 렌더패스가 실제로 사용 가능한지 확인합니다.
        inline bool isAvailable() { return srv; }
        /// @brief 큐브맵 타겟 크기를 바꿉니다. 이 함수를 호출한 경우, bind에서 면마다 따로 바인드했던 유니폼 버퍼는 모두 리셋되므로 필요하면 꼭 다시 기록해야 합니다.
        /// @return 실패한 경우 false를 리턴하며, 이 때 내부 데이터는 모두 해제되어 있습니다.
        bool resconstructFB(uint32_t width, uint32_t height);
        /// @brief 기록된 명령을 모두 수행합니다. 동작이 완료되지 않아도 즉시 리턴합니다.
        /// @param other 이 패스가 시작하기 전에 기다릴 다른 렌더패스입니다. 전후 의존성이 존재할 경우 사용하는 것이 좋습니다. (Vk세마포어 동기화를 사용) 현재 버전에서 기다리는 단계는 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 하나로 고정입니다.
        void execute(RenderPass* other = nullptr);
    private:
        inline RenderPass2Cube() {}
        ~RenderPass2Cube();

        ID3D11Texture2D* colorMap{}, * depthMap{};
        ID3D11RenderTargetView* rtv{};
        ID3D11DepthStencilView* dsv{};
        ID3D11ShaderResourceView* srv{};

        Pipeline* pipeline{};

        struct {
            UniformBuffer* ub;
            uint32_t ubPos;
            uint32_t setPos;
        }facewise[6];

        D3D11_VIEWPORT viewport;
        D3D11_RECT scissor;

        uint32_t width, height;
        bool recording = false;
    };

    /// @brief D3D11 셰이더 객체의 집합입니다. 어떤 멤버도 직접 사용할 수 없습니다.
    class D3D11Machine::Pipeline: public align16 {
        friend class D3D11Machine;
        friend class RenderPass;
        private:
            Pipeline(ID3D11InputLayout*, ID3D11VertexShader*, ID3D11HullShader*, ID3D11DomainShader*, ID3D11GeometryShader*, ID3D11PixelShader*, ID3D11DepthStencilState*, UINT stencilRef, ID3D11BlendState*);
            ~Pipeline();
            ID3D11InputLayout* layout;
            ID3D11VertexShader* vs;
            ID3D11HullShader* tcs;
            ID3D11DomainShader* tes;
            ID3D11GeometryShader* gs;
            ID3D11PixelShader* fs;
            ID3D11DepthStencilState* dsState;
            ID3D11BlendState* blendState;
            UINT stencilRef;
            vec4 blendRef;
    };

    class D3D11Machine::UniformBuffer {
        friend class D3D11Machine;
        friend class RenderPass;
    public:
        /// @brief 아무 동작도 하지 않습니다.
        void resize(uint32_t size);
        /// @brief 유니폼 버퍼의 내용을 갱신합니다. 넘치게 데이터를 줘도 추가 할당은 하지 않으므로 주의하세요.
        /// @param input 입력 데이터
        /// @param index 사용되지 않습니다.
        /// @param offset 데이터 내에서 몇 바이트째부터 수정할지
        /// @param size 덮어쓸 양
        void update(const void* input, uint32_t index, uint32_t offset, uint32_t size);
        /// @brief 0을 리턴합니다.
        inline static uint16_t getIndex() { return 0; }
        /// @brief 0을 리턴합니다.
        inline static int getLayout() { return 0; }
    private:
        UniformBuffer(uint32_t length, ID3D11Buffer* ubo);
        ~UniformBuffer();
        ID3D11Buffer* ubo;
        uint32_t length;
    };

    class D3D11Machine::Mesh {
        friend class D3D11Machine;
        public:
            void update(const void* input, uint32_t offset, uint32_t size);
            void updateIndex(const void* input, uint32_t offset, uint32_t size);
        private:
            Mesh(ID3D11Buffer* vb, ID3D11Buffer* ib, DXGI_FORMAT indexFormat, size_t vcount, size_t icount, UINT vStride);
            ~Mesh();
            ID3D11Buffer* vb;
            ID3D11Buffer* ib;
            const DXGI_FORMAT indexFormat;
            const size_t vcount, icount;
            const UINT vStride;
    };
    
    constexpr LPCSTR VS_SEMANTIC[] = { u8"_0_",u8"_1_",u8"_2_",u8"_3_",u8"_4_",u8"_5_",u8"_6_",u8"_7_",u8"_8_",u8"_9_",u8"_10_",u8"_11_",u8"_12_",u8"_13_",u8"_14_",u8"_15_" };

    template<class FATTR, class... ATTR>
    struct D3D11Machine::Vertex {
        friend class D3D11Machine;
        inline static constexpr bool CHECK_TYPE() {
            if constexpr (sizeof...(ATTR) == 0) return is_one_of<FATTR, VERTEX_ATTR_TYPES>;
            else return is_one_of<FATTR, VERTEX_ATTR_TYPES> || Vertex<ATTR...>::CHECK_TYPE();
        }
    private:
        ftuple<FATTR, ATTR...> member;
        template<class F>
        inline static constexpr DXGI_FORMAT getFormat() {
            if constexpr (is_one_of<F, VERTEX_FLOAT_TYPES>) {
                if constexpr (sizeof(F) / sizeof(float) == 1) return DXGI_FORMAT_R32_FLOAT;
                if constexpr (std::is_same_v<F, vec2> || sizeof(F) / sizeof(float) == 2) return DXGI_FORMAT_R32G32_FLOAT;
                if constexpr (std::is_same_v<F, vec3> || sizeof(F) / sizeof(float) == 3) return DXGI_FORMAT_R32G32B32_FLOAT;
                return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            else if constexpr (is_one_of<F, VERTEX_INT8_TYPES>) {
                if constexpr (sizeof(F) == 1) return DXGI_FORMAT_R8_SINT;
                if constexpr (sizeof(F) == 2) return DXGI_FORMAT_R8G8_SINT;
                if constexpr (sizeof(F) == 3) return DXGI_FORMAT_R8G8B8_SINT;
                return DXGI_FORMAT_R8G8B8A8_SINT;
            }
            else if constexpr (is_one_of<F, VERTEX_UINT8_TYPES>) {
                if constexpr (sizeof(F) == 1) return DXGI_FORMAT_R8_UINT;
                if constexpr (sizeof(F) == 2) return DXGI_FORMAT_R8G8_UINT;
                if constexpr (sizeof(F) == 3) return DXGI_FORMAT_R8G8B8_UINT;
                return DXGI_FORMAT_R8G8B8A8_UINT;
            }
            else if constexpr (is_one_of<F, VERTEX_INT16_TYPES>) {
                if constexpr (sizeof(F) == 2) return DXGI_FORMAT_R16_SINT;
                if constexpr (sizeof(F) == 4) return DXGI_FORMAT_R16G16_SINT;
                if constexpr (sizeof(F) == 6) return DXGI_FORMAT_R16G16B16_SINT;
                return DXGI_FORMAT_R16G16B16A16_SINT;
            }
            else if constexpr (is_one_of<F, VERTEX_UINT16_TYPES>) {
                if constexpr (sizeof(F) == 2) return DXGI_FORMAT_R16_UINT;
                if constexpr (sizeof(F) == 4) return DXGI_FORMAT_R16G16_UINT;
                if constexpr (sizeof(F) == 6) return DXGI_FORMAT_R16G16B16_UINT;
                return DXGI_FORMAT_R16G16B16A16_UINT;
            }
            else if constexpr (is_one_of<F, VERTEX_INT32_TYPES>) {
                if constexpr (sizeof(F) / sizeof(int32_t) == 1) return DXGI_FORMAT_R32_SINT;
                if constexpr (std::is_same_v<F, ivec2> || sizeof(F) / sizeof(int32_t) == 2) return DXGI_FORMAT_R32G32_SINT;
                if constexpr (std::is_same_v<F, ivec3> || sizeof(F) / sizeof(int32_t) == 3) return DXGI_FORMAT_R32G32B32_SINT;
                return DXGI_FORMAT_R32G32B32A32_SINT;
            }
            else if constexpr (is_one_of<F, VERTEX_UINT32_TYPES>) {
                if constexpr (sizeof(F) / sizeof(uint32_t) == 1) return DXGI_FORMAT_R32_UINT;
                if constexpr (std::is_same_v<F, uvec2> || sizeof(F) / sizeof(uint32_t) == 2) return DXGI_FORMAT_R32G32_UINT;
                if constexpr (std::is_same_v<F, uvec3> || sizeof(F) / sizeof(uint32_t) == 3) return DXGI_FORMAT_R32G32B32_UINT;
                return DXGI_FORMAT_R32G32B32A32_UINT;
            }
            return DXGI_FORMAT_FORCE_UINT; // UNREACHABLE
        }
    public:
        /// @brief 정점 속성 바인딩을 받아옵니다.
        /// @param vattrs 출력 위치
        /// @param binding 바인딩 번호
        /// @param locationPlus 셰이더 내의 location이 시작할 번호. 이 값이 0이 아닌 경우 per instance 버퍼로 취급됩니다.
        template<unsigned LOCATION = 0>
        inline static constexpr void info(D3D11_INPUT_ELEMENT_DESC* vattrs, uint32_t binding = 0, uint32_t locationPlus = 0) {
            using A_TYPE = std::remove_reference_t<decltype(Vertex().get<LOCATION>())>;
            vattrs->SemanticName = VS_SEMANTIC[LOCATION];
            vattrs->SemanticIndex = 0;
            vattrs->InputSlot = binding;
            vattrs->InputSlotClass = locationPlus ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
            vattrs->Format = getFormat<A_TYPE>();
            vattrs->AlignedByteOffset = (uint32_t)(ftuple<FATTR, ATTR...>::template offset<LOCATION>());
            vattrs->InstanceDataStepRate = locationPlus ? 1 : 0;
            if constexpr (LOCATION < sizeof...(ATTR)) info<LOCATION + 1>(vattrs + 1, binding, locationPlus);
        }
        inline Vertex() { static_assert(CHECK_TYPE(), "One or more of attribute types are inavailable"); }
        inline Vertex(const FATTR& first, const ATTR&... rest) :member(first, rest...) { static_assert(CHECK_TYPE(), "One or more of attribute types are inavailable"); }
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

    // 테스트를 위한 함수
    ID3DBlob* compileShader(const char* code, size_t size, D3D11Machine::ShaderStage type);
}

#undef VERTEX_FLOAT_TYPES
#undef VERTEX_DOUBLE_TYPES
#undef VERTEX_INT8_TYPES
#undef VERTEX_UINT8_TYPES
#undef VERTEX_INT16_TYPES
#undef VERTEX_UINT16_TYPES
#undef VERTEX_INT32_TYPES
#undef VERTEX_UINT32_TYPES

#undef VERTEX_ATTR_TYPES

#endif // __YR_D3D11_H__