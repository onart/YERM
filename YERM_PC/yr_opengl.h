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
#ifndef __YR_OPENGL_H__
#define __YR_OPENGL_H__

#include "yr_string.hpp"
#include "yr_tuple.hpp"

#include "yr_graphics_param.h"
#include "yr_math.hpp"
#include "yr_threadpool.hpp"

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

    /// @brief OpenGL 그래픽스 컨텍스트를 초기화하며, 모델, 텍스처, 셰이더 등 각종 객체는 이 VkMachine으로부터 생성할 수 있습니다.
    class GLMachine{
        friend class Game;
        public:
            /// @brief 저수준 인터페이스를 이용할 수 있습니다.
            class LowLevel;
            static int32_t currentWindowContext;
            /// @brief 스레드에서 최근에 호출된 함수의 실패 요인을 일부 확인할 수 있습니다. Vulkan 호출에 의한 실패가 아닌 경우 MAX_ENUM 값이 들어갑니다.
            static thread_local unsigned reason;
            constexpr static bool VULKAN_GRAPHICS = false, D3D12_GRAPHICS = false, D3D11_GRAPHICS = false, OPENGL_GRAPHICS = true, OPENGLES_GRAPHICS = false, METAL_GRAPHICS = false;
            /// @brief OpenGL 오류 콜백을 사용하려면 이것을 활성화해 주세요.
            constexpr static bool USE_OPENGL_DEBUG = true;
            struct WindowSystem;
            /// @brief 그리기 대상입니다. 텍스처로 사용하거나 메모리 맵으로 데이터에 접근할 수 있습니다. 
            class RenderTarget;
            /// @brief 오프스크린용 렌더 패스입니다.
            class RenderPass;
            /// @brief 화면에 그리기 위한 렌더 패스입니다. 여러 개 갖고 있을 수는 있지만 동시에 여러 개를 사용할 수는 없습니다.
            using RenderPass2Screen = RenderPass;
            /// @brief 큐브맵에 그리기 위한 렌더 패스입니다.
            class RenderPass2Cube;
            class Pipeline;
            struct PipelineInputVertexSpec;
            /// @brief 직접 불러오는 텍스처입니다.
            class Texture;
            using pTexture = std::shared_ptr<Texture>;
            class TextureSet;
            using pTextureSet = std::shared_ptr<TextureSet>;
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
                unsigned vertexShader;
                unsigned fragmentShader;
                unsigned geometryShader = 0;
                unsigned tessellationControlShader = 0;
                unsigned tessellationEvaluationShader = 0;
                PipelineLayoutOptions shaderResources;
                DepthStencilTesting depthStencil;
                AlphaBlend alphaBlend[3];
                float blendConstant[4]{};
                __devnull vsByteCode;
                __devnull vsByteCodeSize;
            };

            /// @brief 요청한 비동기 동작 중 완료된 것이 있으면 처리합니다.
            static void handle();
            /// @brief 원하는 비동기 동작을 요청합니다.
            /// @param work 다른 스레드에서 실행할 함수
            /// @param handler 호출되는 것
            /// @param strand 이 값이 값은 것들끼리는(0 제외) 동시에 다른 스레드에서 실행되지 않습니다. 그래픽스에 관련된 내용을 수행할 경우 1을 입력해 주세요.
            static void post(std::function<variant8(void)> work, std::function<void(variant8)> handler, uint8_t strand = 0);
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
            /// @param fileName 파일 이름
            /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
            /// @param opts @ref TextureCreationOptions
            /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
            static pTexture createTextureFromImage(int32_t key, const char* fileName, const TextureCreationOptions& opts = {});
            /// @brief @ref createTextureFromImage를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아닌 그냥 key). 매개변수 설명은 createTextureFromImage를 참고하세요.
            static void asyncCreateTextureFromImage(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts = {});
            /// @param mem 데이터 위치
            /// @param size 데이터 길이
            /// @param key 프로그램 내부에서 사용할 이름으로, 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
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
            /// @brief 메모리 상의 ktx2 파일을 통해 텍스처를 생성합니다.
            /// @param key 프로그램 내부에서 사용할 이름입니다. 이것이 기존의 것과 겹치면 파일과 관계 없이 기존에 불러왔던 객체를 리턴합니다.
            /// @param mem 이미지 시작 주소
            /// @param size mem 배열의 길이(바이트)
            /// @param opts @ref TextureCreationOptions
            /// @return 만들어진 텍스처 혹은 이미 있던 해당 key의 텍스처
            static pTexture createTexture(int32_t key, const uint8_t* mem, size_t size, const TextureCreationOptions& opts = {});
            /// @brief createTexture를 비동기적으로 실행합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아니라 그냥 key). 매개변수 설명은 createTexture를 참고하세요.
            static void asyncCreateTexture(int32_t key, const uint8_t* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts = {});
            /// @brief 여러 개의 텍스처를 연속된 넘버으로 바인드하는 집합을 생성합니다.
            static pTextureSet createTextureSet(int32_t key, const pTexture& binding0, const pTexture& binding1, const pTexture& binding2 = {}, const pTexture& binding3 = {});
            /// @brief 빈 텍스처를 만듭니다. 메모리 맵으로 데이터를 올릴 수 있습니다. 올리는 데이터의 기본 형태는 BGRA 순서이며, 필요한 경우 셰이더에서 직접 스위즐링하여 사용합니다.
            static pStreamTexture createStreamTexture(int32_t key, uint32_t width, uint32_t height, bool linearSampler = true);
            /// @brief GLSL 셰이더를 컴파일하여 보관하고 가져옵니다.
            /// @brief SPIR-V 컴파일된 셰이더를 VkShaderModule 형태로 저장하고 가져옵니다.
            /// @param key 이후 별도로 접근할 수 있는 이름을 지정합니다. 중복된 이름을 입력하는 경우 새로 생성되지 않고 기존의 것이 리턴됩니다.
            /// @param opts @ref ShaderModuleCreationOptions
            static unsigned createShader(int32_t key, const ShaderModuleCreationOptions& opts);
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
            static unsigned getShader(int32_t key);
            /// @brief 올려 둔 텍스처 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
            static pTexture getTexture(int32_t key);
            /// @brief 만들어 둔 메시 객체를 리턴합니다. 없으면 빈 포인터를 리턴합니다.
            static pMesh getMesh(int32_t key);
            /// @brief 해제되어야 할 자원을 안전하게 해제합니다. 각 자원의 collect와 이것의 호출 주기는 적절하게 설정할 필요가 있습니다.
            static void reap();
            /// @brief 모든 창의 수직 동기화 여부를 설정합니다.
            static void setVsync(bool vsyncOn);
        private:
            template<class T>
            inline static int32_t issueKey(const std::map<int32_t, T>& map) {
                if (map.empty()) return 0;
                int32_t fk = map.crbegin()->first;
                if (fk < INT32_MAX) { return fk + 1; }
                int32_t prev = INT32_MIN;
                for (auto it = map.cbegin(); it != map.cend(); ++it) {
                    if (it->first != prev + 1) return prev + 1;
                    prev = it->first;
                }
                return prev + 1;
            }
        public:
            static int32_t issueRenderPassKey() { return issueKey(singleton->renderPasses); }
            static int32_t issueRenderPass2ScreenKey() { return issueKey(singleton->finalPasses); }
            static int32_t issueRenderPass2CubeKey() { return issueKey(singleton->cubePasses); }
            static int32_t issueShaderKey() { return issueKey(singleton->shaders); }
            static int32_t issueUniformBufferKey() { return issueKey(singleton->uniformBuffers); }
            static int32_t issuePipelineKey() { return issueKey(singleton->pipelines); }
            static int32_t issueMeshKey() { return issueKey(singleton->meshes); }
            static int32_t issueTextureKey() { return issueKey(singleton->textures); }
            static int32_t issueStreamTextureKey() { return issueKey(singleton->streamTextures); }
            static int32_t issueTextureSetKey() { return issueKey(singleton->textureSets); }
            static int32_t issueWindowSystemsKey() { return issueKey(singleton->windowSystems); }
        private:
            /// @brief 기본 OpenGL 컨텍스트를 생성합니다.
            GLMachine();
            /// @brief 주어진 창에 렌더링할 수 있도록 정보를 추가합니다.
            /// @return 정상적으로 추가되었는지 여부
            bool addWindow(int32_t key, Window* window);
            /// @brief 창을 제거합니다. 해당 창에 연결되었던 모든 렌더패스는 제거됩니다.
            void removeWindow(int32_t key);
            /// @brief 창 크기 또는 스타일이 변경되거나 최소화/복원할 때 호출하여 뷰포트를 다시 세팅합니다.
            void resetWindow(int32_t key, bool = false);
            /// @brief ktxTexture2 객체로 텍스처를 생성합니다.
            pTexture createTexture(void* ktxObj, int32_t key, const TextureCreationOptions& opts);
            static RenderTarget* createRenderTarget2D(int width, int height, RenderTargetType type, bool useDepthInput, bool linear);
            /// @brief vulkan 객체를 없앱니다.
            void free();
            ~GLMachine();
            /// @brief 이 클래스 객체는 Game 밖에서는 생성, 소멸 호출이 불가능합니다.
            inline void operator delete(void* p){::operator delete(p);}
        private:
            static GLMachine* singleton;
            ThreadPool loadThread;
            std::map<int32_t, RenderPass*> renderPasses;
            std::map<int32_t, RenderPass2Screen*> finalPasses;
            std::map<int32_t, RenderPass2Cube*> cubePasses;
            std::map<int32_t, unsigned> shaders;
            std::map<int32_t, UniformBuffer*> uniformBuffers;
            std::map<int32_t, Pipeline*> pipelines;
            std::map<int32_t, pMesh> meshes;
            std::map<int32_t, pTexture> textures;
            std::map<int32_t, pStreamTexture> streamTextures;
            std::map<int32_t, pTextureSet> textureSets;
            std::map<int32_t, WindowSystem*> windowSystems;
            bool vsync = true;

            std::mutex textureGuard;

            std::vector<std::shared_ptr<RenderPass>> passes;
            enum vkm_strand { 
                NONE = 0,
                GENERAL = 1,
            };
    };

    struct GLMachine::WindowSystem {
        Window* window;
        uint32_t width, height;
        WindowSystem(Window*);
    };

    class GLMachine::RenderTarget{
        friend class GLMachine;
        friend class RenderPass;
        public:
            RenderTarget& operator=(const RenderTarget&) = delete;
        private:
            unsigned color1, color2, color3, depthStencil;
            unsigned framebuffer;
            unsigned width, height;
            const bool dsTexture;
            const RenderTargetType type;
            RenderTarget(RenderTargetType type, unsigned width, unsigned height, unsigned c1, unsigned c2, unsigned c3, unsigned ds, bool depthAsTexture, unsigned framebuffer);
            ~RenderTarget();
    };

    class GLMachine::RenderPass{
        friend class GLMachine;
        public:
            struct ReadBackBuffer {
                uint8_t* data;
                int32_t key;
            };
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
            /// @brief 주어진 텍스처를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
            /// @param pos 바인드할 set 번호
            /// @param tx 바인드할 텍스처
            void bind(uint32_t pos, const pTextureSet& tx);
            /// @brief 주어진 렌더 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다. 이 패스의 프레임버퍼에서 사용 중인 렌더타겟은 사용할 수 없습니다.
            /// @param pos 바인드할 set 번호
            /// @param target 바인드할 타겟
            /// @param index 렌더 타겟 내의 인덱스입니다. (0~2는 색 버퍼, 3은 깊이 버퍼)
            void bind(uint32_t pos, RenderPass* target);
            /// @brief 주어진 렌더 타겟을 텍스처의 형태로 바인드합니다. 서브패스 진행 중이 아니면 실패합니다. 이 패스의 프레임버퍼에서 사용 중인 렌더타겟은 사용할 수 없습니다.
            /// @param pos 바인드할 set 번호
            /// @param target 바인드할 타겟
            void bind(uint32_t pos, RenderPass2Cube* target);
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
            void start(uint32_t pos = 0, bool = false);
            /// @brief 기록된 명령을 모두 수행합니다. 동작이 완료되지 않아도 즉시 리턴합니다.
            /// @param other 이 패스가 시작하기 전에 기다릴 다른 렌더패스입니다. 전후 의존성이 존재할 경우 사용하는 것이 좋습니다. (Vk세마포어 동기화를 사용) 현재 버전에서 기다리는 단계는 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 하나로 고정입니다.
            void execute(...);
            /// @brief true를 리턴합니다.
            bool wait(uint64_t timeout = UINT64_MAX);
            /// @brief 렌더타겟의 크기를 일괄 변경합니다.
            void resize(int width, int height, bool linearSampled = true);
            /// @brief 렌더타겟에 직전 execute 이후 그려진 내용을 별도의 텍스처로 복사합니다.
            /// @param key 텍스처 키입니다.
            pTexture copy2Texture(int32_t key, const RenderTarget2TextureOptions& opts = {});
            /// @brief OpenGL API에서는 렌더타겟의 비동기 텍스처 복사를 지원하지 않습니다. 실제로는 호출 즉시 동기적으로 수행되며, 응용단 호환을 위해 함수가 존재합니다. 핸들러에 전달되는 값은 bytedata[0] key, bytedata[1] 성공 여부(0이 성공, 그 외 실패)
            void asyncCopy2Texture(int32_t key, std::function<void(variant8)> handler, const RenderTarget2TextureOptions& opts = {});
            /// @brief 렌더타겟에 직전 execute 이후 그려진 내용을 CPU 메모리에 작성합니다. 포맷은 렌더타겟과 동일합니다. OpenGL의 경우 주어진 영역의 좌측 하단 픽셀부터 가로로 배열됩니다. 현재 depth/stencil 버퍼는 항상 24/8 포맷임에 유의해 주세요.
            std::unique_ptr<uint8_t[]> readBack(uint32_t index, const TextureArea2D& area = {});
            /// @brief OpenGL API에서는 렌더타겟의 비동기 readback을 지원하지 않습니다. 실제로는 호출 즉시 동기적으로 수행되며, 응용단 호환을 위해 함수가 존재합니다. 핸들러에 전달되는 값은 bytedata[0] key, bytedata[1] 성공 여부(0이 성공, 그 외 실패)
            /// asyncReadBack 호출 시점보다 뒤에 그려진 내용이 캡처될 수 있으며 이 사양은 추후 변할 수 있습니다. 포맷은 렌더타겟과 동일합니다.
            /// @param key 핸들러에 전달될 키입니다.
            /// @param handler 비동기 핸들러입니다. @ref ReadBackBuffer의 포인터가 전달되며 해당 메모리는 자동으로 해제되므로 핸들러에서는 읽기만 가능합니다.
            void asyncReadBack(int32_t key, uint32_t index, std::function<void(variant8)> handler, const TextureArea2D& area = {});
        private:
            RenderPass(uint16_t stageCount, bool canBeRead, float* autoclear); // 이후 다수의 서브패스를 쓸 수 있도록 변경
            ~RenderPass();
            const uint16_t stageCount;
            std::vector<Pipeline*> pipelines;
            std::vector<RenderTarget*> targets;
            int currentPass = -1;
            int32_t windowIdx;
            bool is4Screen = false;
            struct {
                float    x;
                float    y;
                float    width;
                float    height;
                float    minDepth;
                float    maxDepth;
            }viewport;
            struct {
                int x, y, width, height;
            }scissor;
            const bool canBeRead;
            bool autoclear;
            float clearColor[4];
    };

    /// @brief 큐브맵 대상의 렌더패스입니다.
    /// 현 버전에서는 지오메트리 셰이더를 통한 레이어드 렌더링을 제공하지 않으며 고정적으로 6개의 서브패스를 활용합니다.
    /// execute 한 번에 6개를 모두 실행합니다.
    /// 각 그리기 명령 간의 시야 차이를 위하여 유니폼버퍼 바인드 시에만 몇 번째 패스에 주어진 바인드를 적용할지 명시하도록 만들어 두었습니다.
    /// 현 버전에서는 기본적으로 RenderPass2Cube의 큐브맵 텍스처 바인딩은 1번입니다.
    class GLMachine::RenderPass2Cube{
        friend class GLMachine;
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
            /// @brief 주어진 렌더 타겟의 결과를 텍스처로 바인드합니다.
            /// @param pos 바인드할 set 번호
            /// @param target 바인드할 타겟
            void bind(uint32_t pos, RenderPass* target);
            /// @brief 주어진 텍스처를 바인드합니다. 서브패스 진행중이 아니면 실패합니다.
            /// @param pos 바인드할 set 번호
            /// @param tx 바인드할 텍스처
            void bind(uint32_t pos, const pStreamTexture& tx);
            /// @brief 주어진 파이프라인을 사용합니다. 지오메트리 셰이더를 사용하도록 만들어진 패스인지 아닌지를 잘 보고 바인드해 주세요.
            void usePipeline(unsigned pipeline);
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
            inline bool isAvailable(){ return fbo; }
            /// @brief 큐브맵 타겟 크기를 바꿉니다. 이 함수를 호출한 경우, bind에서 면마다 따로 바인드했던 유니폼 버퍼는 모두 리셋되므로 필요하면 꼭 다시 기록해야 합니다.
            /// @return 실패한 경우 false를 리턴하며, 이 때 내부 데이터는 모두 해제되어 있습니다.
            bool resconstructFB(uint32_t width, uint32_t height);
            /// @brief 기록된 명령을 모두 수행합니다.
            /// @param other 이 패스가 시작하기 전에 기다릴 다른 렌더패스입니다.
            void execute(RenderPass* other = nullptr);
        private:
            inline RenderPass2Cube(){}
            ~RenderPass2Cube();

            unsigned fbo;
            unsigned targetCubeC, targetCubeD;
            unsigned pipeline;

            struct {
                UniformBuffer* ub;
                uint32_t ubPos;
                uint32_t setPos;
            }facewise[6];

            struct {
                float    x;
                float    y;
                float    width;
                float    height;
                float    minDepth;
                float    maxDepth;
            }viewport;
            struct {
                int x, y, width, height;
            }scissor;

            uint32_t width, height;
            bool recording = false;
    };

    class GLMachine::Texture{
        friend class GLMachine;
        friend class RenderPass;
        friend class TextureSet;
        public:
            /// @brief 사용하지 않는 텍스처 데이터를 정리합니다.
            /// @param removeUsing 사용하는 텍스처 데이터도 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
            static void collect(bool removeUsing = false);
            /// @brief 주어진 이름의 텍스처 데이터를 내립니다. 사용하고 있는 텍스처 데이터는 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
            static void drop(int32_t name);
            const uint16_t width, height;
        protected:
            Texture(uint32_t txo, uint16_t width, uint16_t height);
            ~Texture();
        private:
            unsigned txo;
    };

    class GLMachine::TextureSet {
        friend class GLMachine;
        friend class RenderPass;
        friend class RenderPass2Cube;
    private:
        pTexture textures[4]{};
        int textureCount;
    };

    class GLMachine::StreamTexture {
        friend class GLMachine;
        friend class RenderPass;
        public:
            /// @brief 사용하지 않는 텍스처 데이터를 정리합니다.
            /// @param removeUsing 사용하는 텍스처 데이터도 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
            static void collect(bool removeUsing = false);
            /// @brief 주어진 이름의 텍스처 데이터를 내립니다. 사용하고 있는 텍스처 데이터는 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getTexture로 찾을 수 없습니다.)
            static void drop(int32_t name);
            /// @brief 이미지 데이터를 다시 설정합니다.
            void update(void* img);
            /// @brief 이미지 데이터를 다시 설정합니다. 매개변수는 데이터 목적지 / 행 피치입니다.
            void updateBy(std::function<void(void*, uint32_t)> function);
            const uint16_t width, height;
        protected:
            StreamTexture(uint32_t txo, uint32_t pbo, uint16_t width, uint16_t height);
            ~StreamTexture();
        private:
            unsigned txo;
            unsigned pbo;
    };

    struct GLMachine::PipelineInputVertexSpec {
        int index;
        int offset;
        int dim;
        enum class t { F32 = 0, F64 = 1, I8 = 2, I16 = 3, I32 = 4, U8 = 5, U16 = 6, U32 = 7 } type;
    };

    class GLMachine::Pipeline: public align16 {
        friend class GLMachine;
        private:
            Pipeline(unsigned program, vec4 clearColor, unsigned vstr, unsigned istr);
            ~Pipeline();
            unsigned program;
            std::vector<PipelineInputVertexSpec> vspec;
            std::vector<PipelineInputVertexSpec> ispec;
            const unsigned vertexSize, instanceAttrStride;
            vec4 clearColor;
            DepthStencilTesting depthStencilOperation;
            AlphaBlend blendOperation[3];
            float blendConstant[4];
    };

    class GLMachine::Mesh{
        friend class GLMachine;
        public:
            /// @brief 사용하지 않는 메시 데이터를 정리합니다.
            /// @param removeUsing 사용하는 메시 데이터도 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getMesh로 찾을 수 없습니다.)
            static void collect(bool removeUsing = false);
            /// @brief 주어진 이름의 메시 데이터를 내립니다. 사용하고 있는 메시 데이터는 사용이 끝나는 즉시 해제되게 합니다. (이 호출 이후로는 getMesh로 찾을 수 없습니다.)
            static void drop(int32_t name);
            /// @brief 정점 버퍼를 수정합니다.
            /// @param input 입력 데이터
            /// @param offset 기존 데이터에서 수정할 시작점(바이트)입니다. (입력 데이터에서의 오프셋이 아닙니다. 0이 정점 버퍼의 시작점입니다.)
            /// @param size 기존 데이터에서 수정할 길이입니다.
            void update(const void* input, uint32_t offset, uint32_t size);
            /// @brief 인덱스 버퍼를 수정합니다.
            /// @param input 입력 데이터
            /// @param offset 기존 데이터에서 수정할 시작점(바이트)입니다. (입력 데이터에서의 오프셋이 아닙니다. 0이 인덱스 버퍼의 시작점입니다.)
            /// @param size 기존 데이터에서 수정할 길이입니다.
            void updateIndex(const void* input, uint32_t offset, uint32_t size);
        private:
            Mesh(unsigned vb, unsigned ib, size_t vcount, size_t icount, bool use32);
            ~Mesh();
            unsigned vb, ib, vao{};
            size_t vcount, icount;
            unsigned idxType;
    };

    class GLMachine::UniformBuffer{
        friend class GLMachine;
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
            uint16_t getIndex();
            /// @brief 0을 리턴합니다.
            inline int getLayout() { return 0; }
            /// @brief 128바이트 크기의 고정 유니폼버퍼를 업데이트합니다. 이것은 모든 파이프라인이 공유하며, 셰이더의 바인딩 11번으로 접근할 수 있습니다.
            static void updatePush(const void* input, uint32_t offset, uint32_t size);
        private:
            UniformBuffer(uint32_t length, unsigned ubo);
            ~UniformBuffer();
            unsigned ubo;
            bool shouldSync = false;
            uint32_t length;
    };

    template<class FATTR, class... ATTR>
    struct GLMachine::Vertex{
        friend class GLMachine;
        inline static constexpr bool CHECK_TYPE() {
            if constexpr (sizeof...(ATTR) == 0) return is_one_of<FATTR, VERTEX_ATTR_TYPES>;
            else return is_one_of<FATTR, VERTEX_ATTR_TYPES> || Vertex<ATTR...>::CHECK_TYPE();
        }
    private:
        struct _vattr {
            int dim;
            enum class t { F32 = 0, F64 = 1, I8 = 2, I16 = 3, I32 = 4, U8 = 5, U16 = 6, U32 = 7 } type;
        };

        ftuple<FATTR, ATTR...> member;
        template<class F>
        inline static constexpr _vattr getFormat(){
            if constexpr(is_one_of<F, VERTEX_FLOAT_TYPES>) {
                if constexpr(sizeof(F) / sizeof(float) == 1) return {1, _vattr::t::F32};
                if constexpr(std::is_same_v<F, vec2> || sizeof(F) / sizeof(float) == 2) return {2, _vattr::t::F32};
                if constexpr(std::is_same_v<F, vec3> || sizeof(F) / sizeof(float) == 3) return {3, _vattr::t::F32};
                return {4, _vattr::t::F32};
            }
            else if constexpr(is_one_of<F, VERTEX_DOUBLE_TYPES>) {
                if constexpr(sizeof(F) / sizeof(double) == 1) return {1, _vattr::t::F64};
                if constexpr(std::is_same_v<F, dvec2> || sizeof(F) / sizeof(double) == 2) return {2, _vattr::t::F64};
                if constexpr(std::is_same_v<F, dvec3> || sizeof(F) / sizeof(double) == 3) return {3, _vattr::t::F64};
                return {4, _vattr::t::F64};
            }
            else if constexpr(is_one_of<F, VERTEX_INT8_TYPES>) {
                if constexpr(sizeof(F) == 1) return {1, _vattr::t::I8};
                if constexpr(sizeof(F) == 2) return {2, _vattr::t::I8};
                if constexpr(sizeof(F) == 3) return {3, _vattr::t::I8};
                return {4, _vattr::t::I8};
            }
            else if constexpr(is_one_of<F, VERTEX_UINT8_TYPES>){
                if constexpr(sizeof(F) == 1) return {1, _vattr::t::U8};
                if constexpr(sizeof(F) == 2) return {2, _vattr::t::U8};
                if constexpr(sizeof(F) == 3) return {3, _vattr::t::U8};
                return {4, _vattr::t::U8};
            }
            else if constexpr(is_one_of<F, VERTEX_INT16_TYPES>){
                if constexpr(sizeof(F) == 2) return {1, _vattr::t::I16};
                if constexpr(sizeof(F) == 4) return {2, _vattr::t::I16};
                if constexpr(sizeof(F) == 6) return {3, _vattr::t::I16};;
                return {4, _vattr::t::I16};;
            }
            else if constexpr(is_one_of<F, VERTEX_UINT16_TYPES>){
                if constexpr(sizeof(F) == 2) return {1, _vattr::t::U16};
                if constexpr(sizeof(F) == 4) return {2, _vattr::t::U16};
                if constexpr(sizeof(F) == 6) return {3, _vattr::t::U16};;
                return {4, _vattr::t::U16};;
            }
            else if constexpr(is_one_of<F, VERTEX_INT32_TYPES>) {
                if constexpr(sizeof(F) / sizeof(int32_t) == 1) return {1, _vattr::t::I32};
                if constexpr(std::is_same_v<F, ivec2> || sizeof(F) / sizeof(int32_t) == 2) return {2, _vattr::t::I32};
                if constexpr(std::is_same_v<F, ivec3> || sizeof(F) / sizeof(int32_t) == 3) return {3, _vattr::t::I32};
                return {4, _vattr::t::I32};
            }
            else if constexpr(is_one_of<F, VERTEX_UINT32_TYPES>) {
                if constexpr(sizeof(F) / sizeof(uint32_t) == 1) return {1, _vattr::t::U32};
                if constexpr(std::is_same_v<F, uvec2> || sizeof(F) / sizeof(uint32_t) == 2) return {2, _vattr::t::U32};
                if constexpr(std::is_same_v<F, uvec3> || sizeof(F) / sizeof(uint32_t) == 3) return {3, _vattr::t::U32};
                return {4, _vattr::t::U32};
            }
            return {}; // UNREACHABLE
        }
    public:
        /// @brief 정점 속성 바인딩을 받아옵니다.
        /// @param vattrs 출력 위치
        /// @param binding 바인딩 번호
        /// @param locationPlus 셰이더 내의 location이 시작할 번호
        template<unsigned LOCATION = 0>
        inline static constexpr void info(PipelineInputVertexSpec* vattrs, uint32_t binding = 0, uint32_t locationPlus = 0){
            using A_TYPE = std::remove_reference_t<decltype(Vertex().get<LOCATION>())>;
            size_t offset = ftuple<FATTR, ATTR...>::template offset<LOCATION>();
            _vattr att = getFormat<A_TYPE>();
            vattrs->dim = att.dim;
            vattrs->index = LOCATION + locationPlus;
            vattrs->offset = offset;
            vattrs->type = (PipelineInputVertexSpec::t)att.type;
            if constexpr (LOCATION < sizeof...(ATTR)) info<LOCATION + 1>(vattrs + 1, 0, locationPlus);
        }
        inline Vertex() {static_assert(CHECK_TYPE(), "One or more of attribute types are inavailable"); }
        inline Vertex(const FATTR& first, const ATTR&... rest):member(first, rest...) { static_assert(CHECK_TYPE(), "One or more of attribute types are inavailable"); }
        
        /// @brief 주어진 번호의 참조를 리턴합니다. 인덱스 초과 시 컴파일되지 않습니다.
        template<unsigned POS, std::enable_if_t<POS <= sizeof...(ATTR), bool> = false>
        constexpr inline auto& get() { return member.template get<POS>(); }
    };

    class GLMachine::LowLevel {
    public:
        // pipeline
        inline static unsigned getPipeline(Pipeline* pp) { return pp->program; }

        // renderpass
        inline static unsigned getTexture(RenderPass* rp, int sub, int targ){
            auto target = rp->targets[sub];
            switch (targ) {
            case 0: return target->color1;
            case 1: return target->color2;
            case 2: return target->color3;
            case 3: return target->depthStencil;
            default: return {};
            }
        }

        inline static unsigned getFramebuffer(RenderPass* rp, int sub){ return rp->targets[sub]->framebuffer; }

        // texture
        inline static unsigned getHandle(Texture* tx) { return tx->txo; } 
        inline static unsigned getHandle(StreamTexture* tx) { return tx->txo; }
        inline static unsigned getBuffer(StreamTexture* tx) { return tx->pbo; }

        // mesh
        inline static unsigned getVertexBuffer(Mesh* ms) { return ms->vb; }
        inline static unsigned getIndexBuffer(Mesh* ms) { return ms->ib; }

        // uniform buffer
        inline static unsigned getHandle(UniformBuffer* ub) { return ub->ubo; }
    private:
        LowLevel() = delete;
        ~LowLevel() = delete;
    };
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


#endif // __YR_OPENGL_H__