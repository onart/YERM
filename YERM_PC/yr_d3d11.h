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
                VERTEX_DOUBLE_TYPES, \
                VERTEX_INT8_TYPES, \
                VERTEX_UINT8_TYPES,\
                VERTEX_INT16_TYPES,\
                VERTEX_UINT16_TYPES,\
                VERTEX_INT32_TYPES,\
                VERTEX_UINT32_TYPES

namespace onart {

    class Window;

    class D3D11Machine {
        friend class Game;
    public:
        /// @brief 스레드에서 최근에 호출된 함수의 실패 요인을 일부 확인할 수 있습니다. Vulkan 호출에 의한 실패가 아닌 경우 MAX_ENUM 값이 들어갑니다.
        static thread_local unsigned reason;
        constexpr static bool VULKAN_GRAPHICS = false, D3D12_GRAPHICS = false, D3D11_GRAPHICS = true, OPENGL_GRAPHICS = false, OPENGLES_GRAPHICS = false, METAL_GRAPHICS = false;
        /// @brief OpenGL 오류 콜백을 사용하려면 이것을 활성화해 주세요.
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
        /// @brief 속성을 직접 정의하는 정점 객체입니다.
        template<class, class...>
        struct Vertex;
        /// @brief 정점 버퍼와 인덱스 버퍼를 합친 것입니다. 사양은 템플릿 생성자를 통해 정할 수 있습니다.
        class Mesh;
        using pMesh = std::shared_ptr<Mesh>;
        /// @brief 셰이더 자원을 나타냅니다. 동시에 사용되지만 않는다면 여러 렌더패스 간에 공유될 수 있습니다.
        class UniformBuffer;
        /// @brief 셰이더 유형입니다.
        enum class ShaderType {
            VERTEX = 0,
            FRAGMENT = 1,
            GEOMETRY = 2,
            TESS_CTRL = 3,
            TESS_EVAL = 4
        };
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
        /// @brief 렌더타겟인 텍스처에서 샘플링할 방식을 선택합니다.
        enum class RenderTargetInputOption {
            INPUT_ATTACHMENT = 1, // GL 기반에서 이 옵션은 사용되지 않으며 SAMPLED_LINEAR로 넘어갑니다.
            SAMPLED_LINEAR = 1,
            SAMPLED_NEAREST = 2
        };
        /// @brief 파이프라인 생성 시 줄 수 있는 옵션입니다. 여기의 성분들을 비트 or하여 생성 함수에 전달합니다.
        enum PipelineOptions : uint32_t {
            USE_DEPTH = 0b1,
            USE_STENCIL = 0b10,
            CULL_BACK = 0b100,
        };
        /// @brief 이미지 파일로부터 텍스처를 생성할 때 줄 수 있는 옵션입니다.
        enum ImageTextureFormatOptions {
            /// @brief 이미지 원본 형식을 사용합니다.
            IT_USE_ORIGINAL = 0,
            /// @brief 가능하면 품질을 최대한 유지하는 압축 텍스처를 사용합니다.
            IT_USE_HQCOMPRESS = 1,
            /// @brief 가능하면 압축 텍스처를 사용합니다.
            IT_USE_COMPRESS = 2
        };
        /// @brief 요청한 비동기 동작 중 완료된 것이 있으면 처리합니다.
        static void handle();
        /// @brief 단위행렬이 리턴됩니다.
        static mat4 preTransform();
        /// @brief 보통 이미지 파일을 불러와 텍스처를 생성합니다. 밉 수준은 반드시 1이며 그 이상을 원하는 경우 ktx2 형식을 이용해 주세요.
        /// @param fileName 파일 이름
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param srgb true면 텍스처 원본의 색을 srgb 공간에 있는 것으로 취급합니다.
        /// @param option 이미지를 압축 텍스처 형식으로 바꿀지 결정할 수 있습니다.
        static pTexture createTextureFromImage(const char* fileName, int32_t key, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief createTextureFromImage를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아닌 그냥 key). 매개변수 설명은 createTextureFromImage를 참고하세요.
        static void asyncCreateTextureFromImage(const char* fileName, int32_t key, std::function<void(void*)> handler, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief 보통 이미지 데이터를 메모리에서 불러와 텍스처를 생성합니다. 밉 수준은 반드시 1이며 그 이상을 원하는 경우 ktx2 형식을 이용해 주세요.
        /// @param mem 이미지 시작 주소
        /// @param size mem 배열의 길이(바이트)
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param srgb true면 텍스처 원본의 색을 srgb 공간에 있는 것으로 취급합니다.
        /// @param option 이미지를 압축 텍스처 형식으로 바꿀지 결정할 수 있습니다.
        static pTexture createTextureFromImage(const uint8_t* mem, size_t size, int32_t key, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief createTextureFromImage를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아닌 그냥 key). 매개변수 설명은 createTextureFromImage를 참고하세요.
        static void asyncCreateTextureFromImage(const uint8_t* mem, size_t size, int32_t key, std::function<void(void*)> handler, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief ktx2, BasisU 파일을 불러와 텍스처를 생성합니다. (KTX2 파일이라도 BasisU가 아니면 실패할 가능성이 있습니다.) 여기에도 libktx로 그 형식을 만드는 별도의 도구가 있으니 필요하면 사용할 수 있습니다.
        /// @param fileName 파일 이름
        /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param nChannels 채널 수(색상)
        /// @param srgb true면 텍스처 원본의 색을 srgb 공간에 있는 것으로 취급합니다.
        /// @param hq 원본이 최대한 섬세하게 남아 있어야 한다면 true를 줍니다. false를 주면 메모리를 크게 절약할 수도 있지만 품질이 낮아질 수 있습니다.
        static pTexture createTexture(const char* fileName, int32_t key, uint32_t nChannels, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief createTexture를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아니라 그냥 key). 매개변수 설명은 createTexture를 참고하세요.
        static void asyncCreateTexture(const char* fileName, int32_t key, uint32_t nChannels, std::function<void(void*)> handler, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief 메모리 상의 ktx2 파일을 통해 텍스처를 생성합니다. (KTX2 파일이라도 BasisU가 아니면 실패할 가능성이 있습니다.) 여기에도 libktx로 그 형식을 만드는 별도의 도구가 있으니 필요하면 사용할 수 있습니다.
        /// @param mem 이미지 시작 주소
        /// @param size mem 배열의 길이(바이트)
        /// @param nChannels 채널 수(색상)
        /// @param key 프로그램 내부에서 사용할 이름입니다. 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
        /// @param srgb true면 텍스처 원본의 색을 srgb 공간에 있는 것으로 취급합니다.
        /// @param hq 원본이 최대한 섬세하게 남아 있어야 한다면 true를 줍니다. false를 주면 메모리를 크게 절약할 수도 있지만 품질이 낮아질 수 있습니다.
        static pTexture createTexture(const uint8_t* mem, size_t size, uint32_t nChannels, int32_t key, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief createTexture를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아니라 그냥 key). 매개변수 설명은 createTexture를 참고하세요.
        static void asyncCreateTexture(const uint8_t* mem, size_t size, uint32_t nChannels, std::function<void(void*)> handler, int32_t key, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief 빈 텍스처를 만듭니다. 메모리 맵으로 데이터를 올릴 수 있습니다. 올리는 데이터의 기본 형태는 BGRA 순서이며, 필요한 경우 셰이더에서 직접 스위즐링하여 사용합니다.
        static pStreamTexture createStreamTexture(uint32_t width, uint32_t height, int32_t key, bool linearSampler = true);
        /// @brief 2D 렌더 타겟을 생성하고 핸들을 리턴합니다. 이것을 해제하는 수단은 없으며, 프로그램 종료 시 자동으로 해제됩니다.
        /// @param width 가로 길이(px).
        /// @param height 세로 길이(px).
        /// @param key 이후 별도로 접근할 수 있는 이름을 지정합니다. 단, 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다. INT32_MIN, 즉 -2147483648 값은 예약되어 있어 사용이 불가능합니다.
        /// @param type @ref RenderTargetType 참고하세요.
        /// @param sampled 사용되지 않습니다.
        /// @param useDepthInput 깊이 버퍼를 렌더버퍼가 아닌 텍스처로 생성합니다.
        /// @param useStencil true면 깊이 이미지에 더불어 스텐실 버퍼를 사용합니다.
        /// @param mmap 이것이 true라면 픽셀 데이터에 순차로 접근할 수 있습니다. 단, 렌더링 성능이 낮아질 가능성이 매우 높습니다.
        /// @return 이것을 통해 렌더 패스를 생성할 수 있습니다.
        static RenderTarget* createRenderTarget2D(int width, int height, int32_t key, RenderTargetType type = RenderTargetType::COLOR1DEPTH, RenderTargetInputOption sampled = RenderTargetInputOption::SAMPLED_LINEAR, bool useDepthInput = false, bool useStencil = false, bool mmap = false);
        /// @brief HLSL 셰이더 바이너리를 통해 오브젝트로 저장하고 가져옵니다.
        /// @param code 컴파일된 HLSL 바이너리
        /// @param size code의 길이
        /// @param key 이후 별도로 접근할 수 있는 이름을 지정합니다. 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다.
        static ID3D11DeviceChild* createShader(const char* code, size_t size, int32_t key, ShaderType type = ShaderType::VERTEX);
        /// @brief 셰이더에서 사용할 수 있는 uniform 버퍼를 생성하여 리턴합니다. 이것을 해제하는 방법은 없으며, 프로그램 종료 시 자동으로 해제됩니다.
        /// @param length OpenGL은 동시에 여러 개 렌더링 명령이 수행될 수 없으므로 사용되지 않습니다.
        /// @param size 버퍼의 크기입니다.
        /// @param stages 사용되지 않습니다.
        /// @param key 프로그램 내에서 사용할 이름입니다. 중복된 이름이 입력된 경우 주어진 나머지 인수를 무시하고 그 이름을 가진 버퍼를 리턴합니다. 키 INT32_MIN + 1의 경우 push라는 인터페이스를 위해 사용되므로 이용할 수 없습니다.
        /// @param binding 바인딩 번호입니다. 11번 바인딩을 사용하려 하는 경우 렌더패스의 push() 인터페이스는 사용하실 수 없습니다.
        static UniformBuffer* createUniformBuffer(uint32_t length, uint32_t size, size_t stages, int32_t key, uint32_t binding = 0);
        /// @brief 주어진 렌더 타겟들을 대상으로 하는 렌더패스를 구성합니다. OpenGL API의 경우 서브패스의 개념을 사용하지 않고 보통의 파이프라인으로 구성됩니다.
        /// @param targets 렌더 타겟 포인터의 배열입니다.
        /// @param subpassCount targets 배열의 크기입니다.
        /// @param key 이름입니다. 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다. INT32_MIN, 즉 -2147483648은 예약된 값이기 때문에 사용할 수 없습니다.
        static RenderPass* createRenderPass(RenderTarget** targets, uint32_t subpassCount, int32_t key);
        /// @brief 큐브맵 대상의 렌더패스를 생성합니다.
        /// @param width 타겟으로 생성되는 각 이미지의 가로 길이입니다.
        /// @param height 타겟으로 생성되는 각 이미지의 세로 길이입니다.
        /// @param key 이름입니다.
        /// @param useColor true인 경우 색 버퍼 1개를 이미지에 사용합니다.
        /// @param useDepth true인 경우 깊이 버퍼를 이미지에 사용합니다. useDepth와 useColor가 모두 true인 경우 샘플링은 색 버퍼에 대해서만 가능합니다.
        static RenderPass2Cube* createRenderPass2Cube(uint32_t width, uint32_t height, int32_t key, bool useColor, bool useDepth);
        /// @brief 화면으로 이어지는 렌더패스를 생성합니다. 각 패스의 타겟들은 현재 창의 해상도와 동일하게 맞춰집니다.
        /// @param targets 생성할 렌더 타겟들의 타입 배열입니다. 서브패스의 마지막은 이것을 제외한 스왑체인 이미지입니다.
        /// @param subpassCount 최종 서브패스의 수입니다. 즉 targets 배열 길이 + 1을 입력해야 합니다.
        /// @param key 이름입니다. (RenderPass 객체와 같은 집합을 공유하지 않음) 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다. INT32_MIN, 즉 -2147483648은 예약된 값이기 때문에 사용할 수 없습니다.
        /// @param useDepth subpassCount가 1이고 이 값이 true인 경우 최종 패스에서 깊이/스텐실 이미지를 사용하게 됩니다. subpassCount가 1이 아니면 무시됩니다.
        /// @param useDepthAsInput targets와 일대일 대응하며, 대응하는 성분의 깊이 성분을 다음 서브패스의 입력으로 사용하려면 true를 줍니다. nullptr를 주는 경우 일괄 false로 취급됩니다. 즉 nullptr가 아니라면 반드시 subpassCount - 1 길이의 배열이 주어져야 합니다.
        static RenderPass2Screen* createRenderPass2Screen(RenderTargetType* targets, uint32_t subpassCount, int32_t key, bool useDepth = true, bool* useDepthAsInput = nullptr);
        /// @brief 아무 동작도 하지 않습니다.
        static unsigned createPipelineLayout(...);
        /// @brief 파이프라인을 생성합니다. 생성된 파이프라인은 이후에 이름으로 사용할 수도 있고, 주어진 렌더패스의 해당 서브패스 위치로 들어갑니다.
        /// @param vs 정점 셰이더 모듈입니다.
        /// @param fs 조각 셰이더 모듈입니다.
        /// @param name 이름입니다. 최대 15바이트까지만 가능합니다. 이미 있는 이름을 입력하면 나머지 인수와 관계 없이 기존의 것을 리턴합니다.
        /// @param tc 테셀레이션 컨트롤 셰이더 모듈입니다. 사용하지 않으려면 0을 주면 됩니다.
        /// @param te 테셀레이션 계산 셰이더 모듈입니다. 사용하지 않으려면 0을 주면 됩니다.
        /// @param gs 지오메트리 셰이더 모듈입니다. 사용하지 않으려면 0을 주면 됩니다.
        static unsigned createPipeline(unsigned vs, unsigned fs, int32_t name, unsigned tc = 0, unsigned te = 0, unsigned gs = 0);
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
        /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static RenderPass2Screen* getRenderPass2Screen(int32_t key);
        /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static RenderPass* getRenderPass(int32_t key);
        /// @brief 만들어 둔 렌더패스를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static RenderPass2Cube* getRenderPass2Cube(int32_t key);
        /// @brief 만들어 둔 파이프라인을 리턴합니다. 없으면 nullptr를 리턴합니다.
        static unsigned getPipeline(int32_t key);
        /// @brief 아무 동작도 하지 않습니다.
        static unsigned getPipelineLayout(int32_t key);
        /// @brief 만들어 둔 렌더 타겟을 리턴합니다. 없으면 nullptr를 리턴합니다.
        static RenderTarget* getRenderTarget(int32_t key);
        /// @brief 만들어 둔 공유 버퍼를 리턴합니다. 없으면 nullptr를 리턴합니다.
        static UniformBuffer* getUniformBuffer(int32_t key);
        /// @brief 만들어 둔 셰이더 모듈을 리턴합니다. 없으면 0을 리턴합니다.
        static ID3D11DeviceChild* getShader(int32_t key);
        /// @brief 올려 둔 텍스처 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
        static pTexture getTexture(int32_t key, bool lock = false);
        /// @brief 만들어 둔 메시 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
        static pMesh getMesh(int32_t key);
        /// @brief 창 표면이 SRGB 공간을 사용하는지 리턴합니다. 
        inline static bool isSurfaceSRGB() { return false; }
        /// @brief 아무 것도 하지 않습니다.
        inline static int getTextureLayout(uint32_t binding) { return 0; }
        /// @brief 아무 것도 하지 않습니다.
        inline static int getInputAttachmentLayout(uint32_t binding) { return 0; }
    private:
        /// @brief 기본 OpenGL 컨텍스트를 생성합니다.
        D3D11Machine(Window*);
        /// @brief 화면으로 그리기 위해 필요한 크기를 전달합니다.
        void createSwapchain(uint32_t width, uint32_t height, Window* window = nullptr);
        /// @brief ktxTexture2 객체로 텍스처를 생성합니다.
        pTexture createTexture(void* ktxObj, int32_t key, uint32_t nChannels, bool srgb, bool hq, bool linearSampler = true);
        /// @brief vulkan 객체를 없앱니다.
        void free();
        ~D3D11Machine();
        /// @brief 이 클래스 객체는 Game 밖에서는 생성, 소멸 호출이 불가능합니다.
        inline void operator delete(void* p) { ::operator delete(p); }
    private:
        static D3D11Machine* singleton;
        ID3D11Device* device{};
        ID3D11DeviceContext* context{};
        IDXGISwapChain* swapchain{};

        ThreadPool loadThread;
        std::map<int32_t, RenderPass*> renderPasses;
        std::map<int32_t, RenderPass2Screen*> finalPasses;
        std::map<int32_t, RenderPass2Cube*> cubePasses;
        std::map<int32_t, RenderTarget*> renderTargets;
        std::map<int32_t, ID3D11DeviceChild*> shaders;
        std::map<int32_t, UniformBuffer*> uniformBuffers;
        std::map<int32_t, unsigned> pipelines;
        std::map<int32_t, pMesh> meshes;
        std::map<int32_t, pTexture> textures;
        std::map<int32_t, pStreamTexture> streamTextures;

        std::mutex textureGuard;
        uint32_t surfaceWidth, surfaceHeight;

        std::vector<std::shared_ptr<RenderPass>> passes;
        enum vkm_strand {
            NONE = 0,
            GENERAL = 1,
        };
    };

    class D3D11Machine::Mesh {
        friend class D3D11Machine;
        public:
            template<class... VATTR> void setInputLayout(void* vs, size_t size, unsigned locationPlus = 0, unsigned inputSlot = 0);
        private:
            Mesh(ID3D11Buffer* vb, ID3D11Buffer* ib);
            ~Mesh();
            ID3D11InputLayout* layout{};
            ID3D11Buffer* vb;
            ID3D11Buffer* ib;
    };

    constexpr LPCSTR VS_SEMANTIC[] = { u8"_0",u8"_1",u8"_2",u8"_3",u8"_4",u8"_5",u8"_6",u8"_7",u8"_8",u8"_9",u8"_10",u8"_11",u8"_12",u8"_13",u8"_14",u8"_15" };

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
                if constexpr (sizeof(F) / sizeof(float) == 1) return DXGI_FORMAT_R32_SFLOAT;
                if constexpr (std::is_same_v<F, vec2> || sizeof(F) / sizeof(float) == 2) return DXGI_FORMAT_R32G32_SFLOAT;
                if constexpr (std::is_same_v<F, vec3> || sizeof(F) / sizeof(float) == 3) return DXGI_FORMAT_R32G32B32_SFLOAT;
                return DXGI_FORMAT_R32G32B32A32_SFLOAT;
            }
            else if constexpr (is_one_of<F, VERTEX_DOUBLE_TYPES>) {
                if constexpr (sizeof(F) / sizeof(double) == 1) return DXGI_FORMAT_R64_SFLOAT;
                if constexpr (std::is_same_v<F, dvec2> || sizeof(F) / sizeof(double) == 2) return DXGI_FORMAT_R64G64_SFLOAT;
                if constexpr (std::is_same_v<F, dvec3> || sizeof(F) / sizeof(double) == 3) return DXGI_FORMAT_R64G64B64_SFLOAT;
                return DXGI_FORMAT_R64G64B64A64_SFLOAT;
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

    template<class... VATTR>
    void D3D11Machine::Mesh::setInputLayout(void* vs, size_t size, unsigned locationPlus, unsigned inputSlot) {
        D3D11_INPUT_ELEMENT_DESC attrs[sizeof...(VATTR)]{};
        D3D11Machine::Vertex<VATTR...>::info(attrs, inputSlot, locationPlus);
        HRESULT result = D3D11Machine::singleton->device->CreateInputLayout(attrs, sizeof...(VATTR), vs, size, &layout);
        
        if (result != S_OK) {
            LOGWITH("Failed to create input layout:", result);
        }
    }
}

#endif // __YR_D3D11_H__