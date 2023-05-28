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
#ifndef __YR_MODEL_H__
#define __YR_MODEL_H__

#include "yr_graphics.h"
#include <vector>

namespace onart{
    /// @brief 도형, 텍스처, 재질, 뼈 등의 데이터를 가지는 객체입니다. 트랜스폼, 포즈(애니메이션) 등은 모델 데이터에 해당하지 않습니다.
    class Model{
        public:
            /// @brief 이 열거형의 비트를 조합하여 모델 파일에서 불러올 정점 속성 또는 텍스처를 명시합니다. 단, 위치는 반드시 불러오게 됩니다.
            enum VAttr: uint64_t{
                VA_POSITION = 0,  // 위치, 값은 0으로 이것을 플래그에 포함하든 말든 반드시 위치 정보는 불러옵니다.
                VA_NORMAL = 1ull << 0,    // 법선
                VA_TEXCOORD = 1ull << 1, // 텍스처 좌표
                VA_TANGENT = 1ull << 2, // 접선과 부접선
                VA_BONE = 1ull << 3,   // 뼈의 가중치와 번호
                VA_PNT = VA_POSITION | VA_NORMAL | VA_TEXCOORD, // 위치, 법선, 텍스처 좌표
                VA_PNTB = VA_PNT | VA_BONE, // 위치, 법선, 텍스처 좌표, 뼈 가중치, 뼈 번호
                VA_PNTTB = VA_PNT | VA_TANGENT | VA_BONE, // 위치, 법선, 텍스처 좌표, 접선, 부접선, 뼈 가중치, 뼈 번호
                
                TX_ALBEDO = 0,          // albedo
                TX_NORMAL = 1ull << 63, // 법선 맵
                VA_ALL = ~0ull  // 파일에 존재하는 것 중 이 라이브러리에서 지원하는 모든 속성 및 텍스처
            };
            /// @brief gltf 2.0 파일로부터 모델을 불러옵니다.
            /// @param fileName 파일 이름
            /// @param name 프로그램 내에서 사용할 이름입니다.
            /// @param flags 불러올 속성을 거르는 플래그입니다. @ref VAttr
            static std::shared_ptr<Model> load(const char* fileName, int32_t name, uint64_t flags = VA_ALL);
            /// @brief 프로그램 상의 gltf 2.0 파일로부터 모델을 불러옵니다.
            /// @param name 프로그램 내에서 사용할 이름입니다.
            /// @param flags 불러올 속성을 거르는 플래그입니다. @ref VAttr
            static std::shared_ptr<Model> load(const uint8_t* mem, size_t size, int32_t name, uint64_t flags = VA_ALL);
            /// @brief 비동기로 모델을 불러옵니다. YRGrahpics의 스레드 풀을 활용합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아닌 그냥 key).
            static void asyncLoad(const char* fileName, int32_t name, std::function<void(void*)> handler, uint64_t flags = VA_ALL);
            /// @brief 비동기로 모델을 불러옵니다. YRGrahpics의 스레드 풀을 활용합니다. 핸들러에 주어지는 매개변수는 하위 32비트 key, 상위 32비트 VkResult입니다(key를 가리키는 포인터가 아닌 그냥 key).
            static void asyncLoad(const uint8_t* mem, size_t size, int32_t name, std::function<void(void*)> handler, uint64_t flags = VA_ALL);
            /// @brief 이미 프로그램 내에 생성한 메시, 텍스처를 가지고 모델 객체를 생성합니다.
            static std::shared_ptr<Model> assemble(const YRGraphics::pMesh& mesh, const YRGraphics::pTexture& albedoTexture, const YRGraphics::pTexture& normalTexture, int32_t name); 
            /// @brief 주어진 이름을 가진 모델 객체를 획득합니다.
            static std::shared_ptr<Model> getModel(int32_t name);
            /// @brief 주어진 렌더패스에 현재 상태의 모델을 그립니다.
            static void draw(YRGraphics::RenderPass* rp);
            /// @brief 주어진 렌더패스에 현재 상태의 모델을 그립니다.
            static void draw(YRGraphics::RenderPass2Cube* rp);
            /// @brief 주어진 렌더패스에 현재 상태의 모델을 그립니다.
#ifdef YR_USE_VULKAN
            static void draw(YRGraphics::RenderPass2Screen* rp);
#endif

            /// @brief 라이브러리에서 필요한 초기 세팅을 수행합니다. 안드로이드 환경인 경우 android_app 포인터를 여기로 전달해야 합니다.
            static void init(void* v = nullptr);
            inline const YRGraphics::pMesh& getMesh(){ return mesh; }
            inline const YRGraphics::pTexture& getAlbedo() { return albedo; }
            inline const YRGraphics::pTexture& getNormal() { return normal; }
            inline void clear(){ models.clear(); }
        private:
            struct material{
                vec4 ambient;
                vec4 diffuse;
                vec4 specular;
                float refractiveIndex;
                float shininess;
            };
            struct bone{

            };
            YRGraphics::pMesh mesh;
            YRGraphics::pTexture albedo, normal;
            static std::map<int32_t, std::shared_ptr<Model>> models;
    };
    using pModel = std::shared_ptr<Model>;
}

#endif