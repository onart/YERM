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
        /// @brief �����忡�� �ֱٿ� ȣ��� �Լ��� ���� ������ �Ϻ� Ȯ���� �� �ֽ��ϴ�. Vulkan ȣ�⿡ ���� ���а� �ƴ� ��� MAX_ENUM ���� ���ϴ�.
        static thread_local unsigned reason;
        constexpr static bool VULKAN_GRAPHICS = false, D3D12_GRAPHICS = false, D3D11_GRAPHICS = true, OPENGL_GRAPHICS = false, OPENGLES_GRAPHICS = false, METAL_GRAPHICS = false;
        /// @brief OpenGL ���� �ݹ��� ����Ϸ��� �̰��� Ȱ��ȭ�� �ּ���.
        constexpr static bool USE_D3D11_DEBUG = true;
        /// @brief �׸��� ����Դϴ�. �ؽ�ó�� ����ϰų� �޸� ������ �����Ϳ� ������ �� �ֽ��ϴ�. 
        class RenderTarget;
        /// @brief ������ũ���� ���� �н��Դϴ�.
        class RenderPass;
        /// @brief ȭ�鿡 �׸��� ���� ���� �н��Դϴ�. ���� �� ���� ���� ���� ������ ���ÿ� ���� ���� ����� ���� �����ϴ�.
        using RenderPass2Screen = RenderPass;
        /// @brief ť��ʿ� �׸��� ���� ���� �н��Դϴ�.
        class RenderPass2Cube;
        /// @brief ���� �ҷ����� �ؽ�ó�Դϴ�.
        class Texture;
        using pTexture = std::shared_ptr<Texture>;
        /// @brief �޸� ������ ������ �ؽ�ó�Դϴ�. �ǽð����� CPU�ܿ��� �����͸� ������ �� �ֽ��ϴ�. �������̳� �ٸ� â�� ȭ�� ���� �ؽ�ó�� ����� �� �����մϴ�.
        class StreamTexture;
        using pStreamTexture = std::shared_ptr<StreamTexture>;
        /// @brief �Ӽ��� ���� �����ϴ� ���� ��ü�Դϴ�.
        template<class, class...>
        struct Vertex;
        /// @brief ���� ���ۿ� �ε��� ���۸� ��ģ ���Դϴ�. ����� ���ø� �����ڸ� ���� ���� �� �ֽ��ϴ�.
        class Mesh;
        using pMesh = std::shared_ptr<Mesh>;
        /// @brief ���̴� �ڿ��� ��Ÿ���ϴ�. ���ÿ� �������� �ʴ´ٸ� ���� �����н� ���� ������ �� �ֽ��ϴ�.
        class UniformBuffer;
        /// @brief ���̴� �����Դϴ�.
        enum class ShaderType {
            VERTEX = 0,
            FRAGMENT = 1,
            GEOMETRY = 2,
            TESS_CTRL = 3,
            TESS_EVAL = 4
        };
        /// @brief ���� Ÿ���� �����Դϴ�.
        enum class RenderTargetType {
            /// @brief �� ���� 1���� �����մϴ�.
            COLOR1 = 0b1,
            /// @brief �� ���� 2���� �����մϴ�.
            COLOR2 = 0b11,
            /// @brief �� ���� 3���� �����մϴ�.
            COLOR3 = 0b111,
            /// @brief ����/���ٽ� ���۸��� �����մϴ�.
            DEPTH = 0b1000,
            /// @brief �� ���� 1���� ����/���ٽ� ���۸� �����մϴ�.
            COLOR1DEPTH = 0b1001,
            /// @brief �� ���� 2���� ����/���ٽ� ���۸� �����մϴ�.
            COLOR2DEPTH = 0b1011,
            /// @brief �� ���� 3���� ����/���ٽ� ���۸� �����մϴ�.
            COLOR3DEPTH = 0b1111
        };
        /// @brief ����Ÿ���� �ؽ�ó���� ���ø��� ����� �����մϴ�.
        enum class RenderTargetInputOption {
            INPUT_ATTACHMENT = 1, // GL ��ݿ��� �� �ɼ��� ������ ������ SAMPLED_LINEAR�� �Ѿ�ϴ�.
            SAMPLED_LINEAR = 1,
            SAMPLED_NEAREST = 2
        };
        /// @brief ���������� ���� �� �� �� �ִ� �ɼ��Դϴ�. ������ ���е��� ��Ʈ or�Ͽ� ���� �Լ��� �����մϴ�.
        enum PipelineOptions : uint32_t {
            USE_DEPTH = 0b1,
            USE_STENCIL = 0b10,
            CULL_BACK = 0b100,
        };
        /// @brief �̹��� ���Ϸκ��� �ؽ�ó�� ������ �� �� �� �ִ� �ɼ��Դϴ�.
        enum ImageTextureFormatOptions {
            /// @brief �̹��� ���� ������ ����մϴ�.
            IT_USE_ORIGINAL = 0,
            /// @brief �����ϸ� ǰ���� �ִ��� �����ϴ� ���� �ؽ�ó�� ����մϴ�.
            IT_USE_HQCOMPRESS = 1,
            /// @brief �����ϸ� ���� �ؽ�ó�� ����մϴ�.
            IT_USE_COMPRESS = 2
        };
        /// @brief ��û�� �񵿱� ���� �� �Ϸ�� ���� ������ ó���մϴ�.
        static void handle();
        /// @brief ��������� ���ϵ˴ϴ�.
        static mat4 preTransform();
        /// @brief ���� �̹��� ������ �ҷ��� �ؽ�ó�� �����մϴ�. �� ������ �ݵ�� 1�̸� �� �̻��� ���ϴ� ��� ktx2 ������ �̿��� �ּ���.
        /// @param fileName ���� �̸�
        /// @param key ���α׷� ���ο��� ����� �̸�����, �̰��� ������ �Ͱ� ��ġ�� ���ϰ� ���� ���� ������ �ҷ��Դ� ��ü�� �����մϴ�.
        /// @param srgb true�� �ؽ�ó ������ ���� srgb ������ �ִ� ������ ����մϴ�.
        /// @param option �̹����� ���� �ؽ�ó �������� �ٲ��� ������ �� �ֽ��ϴ�.
        static pTexture createTextureFromImage(const char* fileName, int32_t key, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief createTextureFromImage�� �񵿱������� �����մϴ�. �ڵ鷯�� �־����� �Ű������� ���� 32��Ʈ key, ���� 32��Ʈ VkResult�Դϴ�(key�� ����Ű�� �����Ͱ� �ƴ� �׳� key). �Ű����� ������ createTextureFromImage�� �����ϼ���.
        static void asyncCreateTextureFromImage(const char* fileName, int32_t key, std::function<void(void*)> handler, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief ���� �̹��� �����͸� �޸𸮿��� �ҷ��� �ؽ�ó�� �����մϴ�. �� ������ �ݵ�� 1�̸� �� �̻��� ���ϴ� ��� ktx2 ������ �̿��� �ּ���.
        /// @param mem �̹��� ���� �ּ�
        /// @param size mem �迭�� ����(����Ʈ)
        /// @param key ���α׷� ���ο��� ����� �̸�����, �̰��� ������ �Ͱ� ��ġ�� ���ϰ� ���� ���� ������ �ҷ��Դ� ��ü�� �����մϴ�.
        /// @param srgb true�� �ؽ�ó ������ ���� srgb ������ �ִ� ������ ����մϴ�.
        /// @param option �̹����� ���� �ؽ�ó �������� �ٲ��� ������ �� �ֽ��ϴ�.
        static pTexture createTextureFromImage(const uint8_t* mem, size_t size, int32_t key, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief createTextureFromImage�� �񵿱������� �����մϴ�. �ڵ鷯�� �־����� �Ű������� ���� 32��Ʈ key, ���� 32��Ʈ VkResult�Դϴ�(key�� ����Ű�� �����Ͱ� �ƴ� �׳� key). �Ű����� ������ createTextureFromImage�� �����ϼ���.
        static void asyncCreateTextureFromImage(const uint8_t* mem, size_t size, int32_t key, std::function<void(void*)> handler, bool srgb = true, ImageTextureFormatOptions option = IT_USE_ORIGINAL, bool linearSampler = true);
        /// @brief ktx2, BasisU ������ �ҷ��� �ؽ�ó�� �����մϴ�. (KTX2 �����̶� BasisU�� �ƴϸ� ������ ���ɼ��� �ֽ��ϴ�.) ���⿡�� libktx�� �� ������ ����� ������ ������ ������ �ʿ��ϸ� ����� �� �ֽ��ϴ�.
        /// @param fileName ���� �̸�
        /// @param key ���α׷� ���ο��� ����� �̸�����, �̰��� ������ �Ͱ� ��ġ�� ���ϰ� ���� ���� ������ �ҷ��Դ� ��ü�� �����մϴ�.
        /// @param nChannels ä�� ��(����)
        /// @param srgb true�� �ؽ�ó ������ ���� srgb ������ �ִ� ������ ����մϴ�.
        /// @param hq ������ �ִ��� �����ϰ� ���� �־�� �Ѵٸ� true�� �ݴϴ�. false�� �ָ� �޸𸮸� ũ�� ������ ���� ������ ǰ���� ������ �� �ֽ��ϴ�.
        static pTexture createTexture(const char* fileName, int32_t key, uint32_t nChannels, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief createTexture�� �񵿱������� �����մϴ�. �ڵ鷯�� �־����� �Ű������� ���� 32��Ʈ key, ���� 32��Ʈ VkResult�Դϴ�(key�� ����Ű�� �����Ͱ� �ƴ϶� �׳� key). �Ű����� ������ createTexture�� �����ϼ���.
        static void asyncCreateTexture(const char* fileName, int32_t key, uint32_t nChannels, std::function<void(void*)> handler, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief �޸� ���� ktx2 ������ ���� �ؽ�ó�� �����մϴ�. (KTX2 �����̶� BasisU�� �ƴϸ� ������ ���ɼ��� �ֽ��ϴ�.) ���⿡�� libktx�� �� ������ ����� ������ ������ ������ �ʿ��ϸ� ����� �� �ֽ��ϴ�.
        /// @param mem �̹��� ���� �ּ�
        /// @param size mem �迭�� ����(����Ʈ)
        /// @param nChannels ä�� ��(����)
        /// @param key ���α׷� ���ο��� ����� �̸��Դϴ�. �̰��� ������ �Ͱ� ��ġ�� ���ϰ� ���� ���� ������ �ҷ��Դ� ��ü�� �����մϴ�.
        /// @param srgb true�� �ؽ�ó ������ ���� srgb ������ �ִ� ������ ����մϴ�.
        /// @param hq ������ �ִ��� �����ϰ� ���� �־�� �Ѵٸ� true�� �ݴϴ�. false�� �ָ� �޸𸮸� ũ�� ������ ���� ������ ǰ���� ������ �� �ֽ��ϴ�.
        static pTexture createTexture(const uint8_t* mem, size_t size, uint32_t nChannels, int32_t key, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief createTexture�� �񵿱������� �����մϴ�. �ڵ鷯�� �־����� �Ű������� ���� 32��Ʈ key, ���� 32��Ʈ VkResult�Դϴ�(key�� ����Ű�� �����Ͱ� �ƴ϶� �׳� key). �Ű����� ������ createTexture�� �����ϼ���.
        static void asyncCreateTexture(const uint8_t* mem, size_t size, uint32_t nChannels, std::function<void(void*)> handler, int32_t key, bool srgb = true, bool hq = true, bool linearSampler = true);
        /// @brief �� �ؽ�ó�� ����ϴ�. �޸� ������ �����͸� �ø� �� �ֽ��ϴ�. �ø��� �������� �⺻ ���´� BGRA �����̸�, �ʿ��� ��� ���̴����� ���� �������Ͽ� ����մϴ�.
        static pStreamTexture createStreamTexture(uint32_t width, uint32_t height, int32_t key, bool linearSampler = true);
        /// @brief 2D ���� Ÿ���� �����ϰ� �ڵ��� �����մϴ�. �̰��� �����ϴ� ������ ������, ���α׷� ���� �� �ڵ����� �����˴ϴ�.
        /// @param width ���� ����(px).
        /// @param height ���� ����(px).
        /// @param key ���� ������ ������ �� �ִ� �̸��� �����մϴ�. ��, �ߺ��� �̸��� �Է��ϴ� ��� ���� �������� �ʰ� ������ ���� ���ϵ˴ϴ�. INT32_MIN, �� -2147483648 ���� ����Ǿ� �־� ����� �Ұ����մϴ�.
        /// @param type @ref RenderTargetType �����ϼ���.
        /// @param sampled ������ �ʽ��ϴ�.
        /// @param useDepthInput ���� ���۸� �������۰� �ƴ� �ؽ�ó�� �����մϴ�.
        /// @param useStencil true�� ���� �̹����� ���Ҿ� ���ٽ� ���۸� ����մϴ�.
        /// @param mmap �̰��� true��� �ȼ� �����Ϳ� ������ ������ �� �ֽ��ϴ�. ��, ������ ������ ������ ���ɼ��� �ſ� �����ϴ�.
        /// @return �̰��� ���� ���� �н��� ������ �� �ֽ��ϴ�.
        static RenderTarget* createRenderTarget2D(int width, int height, int32_t key, RenderTargetType type = RenderTargetType::COLOR1DEPTH, RenderTargetInputOption sampled = RenderTargetInputOption::SAMPLED_LINEAR, bool useDepthInput = false, bool useStencil = false, bool mmap = false);
        /// @brief HLSL ���̴� ���̳ʸ��� ���� ������Ʈ�� �����ϰ� �����ɴϴ�.
        /// @param code �����ϵ� HLSL ���̳ʸ�
        /// @param size code�� ����
        /// @param key ���� ������ ������ �� �ִ� �̸��� �����մϴ�. �ߺ��� �̸��� �Է��ϴ� ��� ���� �������� �ʰ� ������ ���� ���ϵ˴ϴ�.
        static ID3D11DeviceChild* createShader(const char* code, size_t size, int32_t key, ShaderType type = ShaderType::VERTEX);
        /// @brief ���̴����� ����� �� �ִ� uniform ���۸� �����Ͽ� �����մϴ�. �̰��� �����ϴ� ����� ������, ���α׷� ���� �� �ڵ����� �����˴ϴ�.
        /// @param length OpenGL�� ���ÿ� ���� �� ������ ����� ����� �� �����Ƿ� ������ �ʽ��ϴ�.
        /// @param size ������ ũ���Դϴ�.
        /// @param stages ������ �ʽ��ϴ�.
        /// @param key ���α׷� ������ ����� �̸��Դϴ�. �ߺ��� �̸��� �Էµ� ��� �־��� ������ �μ��� �����ϰ� �� �̸��� ���� ���۸� �����մϴ�. Ű INT32_MIN + 1�� ��� push��� �������̽��� ���� ���ǹǷ� �̿��� �� �����ϴ�.
        /// @param binding ���ε� ��ȣ�Դϴ�. 11�� ���ε��� ����Ϸ� �ϴ� ��� �����н��� push() �������̽��� ����Ͻ� �� �����ϴ�.
        static UniformBuffer* createUniformBuffer(uint32_t length, uint32_t size, size_t stages, int32_t key, uint32_t binding = 0);
        /// @brief �־��� ���� Ÿ�ٵ��� ������� �ϴ� �����н��� �����մϴ�. OpenGL API�� ��� �����н��� ������ ������� �ʰ� ������ �������������� �����˴ϴ�.
        /// @param targets ���� Ÿ�� �������� �迭�Դϴ�.
        /// @param subpassCount targets �迭�� ũ���Դϴ�.
        /// @param key �̸��Դϴ�. �̹� �ִ� �̸��� �Է��ϸ� ������ �μ��� ���� ���� ������ ���� �����մϴ�. INT32_MIN, �� -2147483648�� ����� ���̱� ������ ����� �� �����ϴ�.
        static RenderPass* createRenderPass(RenderTarget** targets, uint32_t subpassCount, int32_t key);
        /// @brief ť��� ����� �����н��� �����մϴ�.
        /// @param width Ÿ������ �����Ǵ� �� �̹����� ���� �����Դϴ�.
        /// @param height Ÿ������ �����Ǵ� �� �̹����� ���� �����Դϴ�.
        /// @param key �̸��Դϴ�.
        /// @param useColor true�� ��� �� ���� 1���� �̹����� ����մϴ�.
        /// @param useDepth true�� ��� ���� ���۸� �̹����� ����մϴ�. useDepth�� useColor�� ��� true�� ��� ���ø��� �� ���ۿ� ���ؼ��� �����մϴ�.
        static RenderPass2Cube* createRenderPass2Cube(uint32_t width, uint32_t height, int32_t key, bool useColor, bool useDepth);
        /// @brief ȭ������ �̾����� �����н��� �����մϴ�. �� �н��� Ÿ�ٵ��� ���� â�� �ػ󵵿� �����ϰ� �������ϴ�.
        /// @param targets ������ ���� Ÿ�ٵ��� Ÿ�� �迭�Դϴ�. �����н��� �������� �̰��� ������ ����ü�� �̹����Դϴ�.
        /// @param subpassCount ���� �����н��� ���Դϴ�. �� targets �迭 ���� + 1�� �Է��ؾ� �մϴ�.
        /// @param key �̸��Դϴ�. (RenderPass ��ü�� ���� ������ �������� ����) �̹� �ִ� �̸��� �Է��ϸ� ������ �μ��� ���� ���� ������ ���� �����մϴ�. INT32_MIN, �� -2147483648�� ����� ���̱� ������ ����� �� �����ϴ�.
        /// @param useDepth subpassCount�� 1�̰� �� ���� true�� ��� ���� �н����� ����/���ٽ� �̹����� ����ϰ� �˴ϴ�. subpassCount�� 1�� �ƴϸ� ���õ˴ϴ�.
        /// @param useDepthAsInput targets�� �ϴ��� �����ϸ�, �����ϴ� ������ ���� ������ ���� �����н��� �Է����� ����Ϸ��� true�� �ݴϴ�. nullptr�� �ִ� ��� �ϰ� false�� ��޵˴ϴ�. �� nullptr�� �ƴ϶�� �ݵ�� subpassCount - 1 ������ �迭�� �־����� �մϴ�.
        static RenderPass2Screen* createRenderPass2Screen(RenderTargetType* targets, uint32_t subpassCount, int32_t key, bool useDepth = true, bool* useDepthAsInput = nullptr);
        /// @brief �ƹ� ���۵� ���� �ʽ��ϴ�.
        static unsigned createPipelineLayout(...);
        /// @brief ������������ �����մϴ�. ������ ������������ ���Ŀ� �̸����� ����� ���� �ְ�, �־��� �����н��� �ش� �����н� ��ġ�� ���ϴ�.
        /// @param vs ���� ���̴� ����Դϴ�.
        /// @param fs ���� ���̴� ����Դϴ�.
        /// @param name �̸��Դϴ�. �ִ� 15����Ʈ������ �����մϴ�. �̹� �ִ� �̸��� �Է��ϸ� ������ �μ��� ���� ���� ������ ���� �����մϴ�.
        /// @param tc �׼����̼� ��Ʈ�� ���̴� ����Դϴ�. ������� �������� 0�� �ָ� �˴ϴ�.
        /// @param te �׼����̼� ��� ���̴� ����Դϴ�. ������� �������� 0�� �ָ� �˴ϴ�.
        /// @param gs ������Ʈ�� ���̴� ����Դϴ�. ������� �������� 0�� �ָ� �˴ϴ�.
        static unsigned createPipeline(unsigned vs, unsigned fs, int32_t name, unsigned tc = 0, unsigned te = 0, unsigned gs = 0);
        /// @brief ���� ����(��) ��ü�� �����մϴ�.
        /// @param vdata ���� ������
        /// @param vsize ���� �ϳ��� ũ��(����Ʈ)
        /// @param vcount ������ ��
        /// @param idata �ε��� �������Դϴ�. �� ���� vcount�� 65536 �̻��� ��� 32��Ʈ ������, �� �ܿ��� 16��Ʈ ������ ��޵˴ϴ�.
        /// @param isize �ε��� �ϳ��� ũ���Դϴ�. �� ���� �ݵ�� 2 �Ǵ� 4���� �մϴ�.
        /// @param icount �ε����� ���Դϴ�. �� ���� 0�̸� �ε��� ���۸� ������� �ʽ��ϴ�.
        /// @param key ���α׷� ������ ����� �̸��Դϴ�.
        /// @param stage �޸� ���� ���� ���������� �����͸� �ٲپ� ������ �����մϴ�. stage�� false�� �޸� ���� ����ϰ� �Ǵ� ��� vdata, idata�� nullptr�� �־ �˴ϴ�.
        static pMesh createMesh(void* vdata, size_t vsize, size_t vcount, void* idata, size_t isize, size_t icount, int32_t key, bool stage = true);
        /// @brief ������ �� ������ �����ϴ� �޽� ��ü�� �����մϴ�. �̰��� ���������� ��ü���� ���� �����Ͱ� ���ǵǾ� �ִ� ��츦 ���� ���Դϴ�.
        /// @param vcount ������ ��
        /// @param name ���α׷� ������ ����� �̸��Դϴ�.
        static pMesh createNullMesh(size_t vcount, int32_t key);
        /// @brief ����� �� �����н��� �����մϴ�. ������ nullptr�� �����մϴ�.
        static RenderPass2Screen* getRenderPass2Screen(int32_t key);
        /// @brief ����� �� �����н��� �����մϴ�. ������ nullptr�� �����մϴ�.
        static RenderPass* getRenderPass(int32_t key);
        /// @brief ����� �� �����н��� �����մϴ�. ������ nullptr�� �����մϴ�.
        static RenderPass2Cube* getRenderPass2Cube(int32_t key);
        /// @brief ����� �� ������������ �����մϴ�. ������ nullptr�� �����մϴ�.
        static unsigned getPipeline(int32_t key);
        /// @brief �ƹ� ���۵� ���� �ʽ��ϴ�.
        static unsigned getPipelineLayout(int32_t key);
        /// @brief ����� �� ���� Ÿ���� �����մϴ�. ������ nullptr�� �����մϴ�.
        static RenderTarget* getRenderTarget(int32_t key);
        /// @brief ����� �� ���� ���۸� �����մϴ�. ������ nullptr�� �����մϴ�.
        static UniformBuffer* getUniformBuffer(int32_t key);
        /// @brief ����� �� ���̴� ����� �����մϴ�. ������ 0�� �����մϴ�.
        static ID3D11DeviceChild* getShader(int32_t key);
        /// @brief �÷� �� �ؽ�ó ��ü�� �����մϴ�. ������ �� �����͸� �����մϴ�.
        static pTexture getTexture(int32_t key, bool lock = false);
        /// @brief ����� �� �޽� ��ü�� �����մϴ�. ������ �� �����͸� �����մϴ�.
        static pMesh getMesh(int32_t key);
        /// @brief â ǥ���� SRGB ������ ����ϴ��� �����մϴ�. 
        inline static bool isSurfaceSRGB() { return false; }
        /// @brief �ƹ� �͵� ���� �ʽ��ϴ�.
        inline static int getTextureLayout(uint32_t binding) { return 0; }
        /// @brief �ƹ� �͵� ���� �ʽ��ϴ�.
        inline static int getInputAttachmentLayout(uint32_t binding) { return 0; }
    private:
        /// @brief �⺻ OpenGL ���ؽ�Ʈ�� �����մϴ�.
        D3D11Machine(Window*);
        /// @brief ȭ������ �׸��� ���� �ʿ��� ũ�⸦ �����մϴ�.
        void createSwapchain(uint32_t width, uint32_t height, Window* window = nullptr);
        /// @brief ktxTexture2 ��ü�� �ؽ�ó�� �����մϴ�.
        pTexture createTexture(void* ktxObj, int32_t key, uint32_t nChannels, bool srgb, bool hq, bool linearSampler = true);
        /// @brief vulkan ��ü�� ���۴ϴ�.
        void free();
        ~D3D11Machine();
        /// @brief �� Ŭ���� ��ü�� Game �ۿ����� ����, �Ҹ� ȣ���� �Ұ����մϴ�.
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
        /// @brief ���� �Ӽ� ���ε��� �޾ƿɴϴ�.
        /// @param vattrs ��� ��ġ
        /// @param binding ���ε� ��ȣ
        /// @param locationPlus ���̴� ���� location�� ������ ��ȣ. �� ���� 0�� �ƴ� ��� per instance ���۷� ��޵˴ϴ�.
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
        /// @brief �־��� ��ȣ�� ������ �����մϴ�. �ε��� �ʰ� �� �����ϵ��� �ʽ��ϴ�.
        template<unsigned POS, std::enable_if_t<POS <= sizeof...(ATTR), bool> = false>
        constexpr inline auto& get() { return member.template get<POS>(); }
        /* template Ű���� ����
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