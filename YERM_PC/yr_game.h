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
#ifndef __YR_GAME_H__
#define __YR_GAME_H__

#include "yr_constants.hpp"
#include "yr_sys.h"

#include <cstdint>
#include <chrono>
#include <string>

namespace onart{
#if defined(YR_USE_VULKAN)
#define YRGraphics VkMachine
#elif defined(YR_USE_OPENGL)
#define YRGraphics GLMachine
#elif defined(YR_USE_WEBGPU)
#define YRGraphics WGMachine
#elif defined(YR_USE_D3D11)
#define YRGraphics D3D11Machine
#endif
    class YRGraphics;

    /// @brief 프레임워크의 진입점입니다. main 함수가 리턴해야 재생성되는 안드로이드 NDK 화면회전 특성상 모든 멤버는 static입니다.
    class Game{
        public:
            /// @brief 창을 닫고 자원을 정상 해제한 후 게임을 종료합니다. PC 게임에서는 창을 닫는 행위와 동일하지만, 모바일에서는 게임을 정상적으로 종료하는 유일한 방법입니다.
            static void exit();
            /// @brief 게임을 시작합니다. 이 안에서 게임 루프가 계속해서 이루어집니다.
            /// @param hd android 환경에서는 android_app, 그 외에서는 nullptr입니다.
            /// @param opt 기본 옵션 외의 것을 주려면 @ref Window::CreationOptions 를 참고하세요.
            /// @return 종료 코드입니다. 0은 정상 종료, 1은 창 시스템 초기화 실패, 2는 이미 시작 중
            static int start(void* hd = nullptr, Window::CreationOptions* opt = nullptr);
            /// @brief 게임 프레임 번호입니다.
            static const int32_t& frame;
            /// @brief 게임을 시작하고 난 시간(나노초)입니다.
            static const uint64_t& tp;
            /// @brief 이전 프레임과 현 프레임 간의 간격(초)입니다.
            static const float& dt;
            /// @brief 이전 프레임과 현 프레임 간의 간격(초)의 역수입니다.
            static const float& idt;
            /// @brief 파일의 내용을 모두 읽어 옵니다. 이 함수는 화면 회전 시 음원 파일 포인터를 유지하기 어려운 경우를 위해 임시로 만들어졌습니다. 읽기에 실패하면 주어진 버퍼는 빈 상태가 됩니다.
            /// @param fileName 파일 이름
            /// @param buffer 데이터가 들어갈 위치. 이전에 내용이 있었더라도 무시됩니다.
            static void readFile(const char* fileName, std::basic_string<uint8_t>* buffer);
        private:
            static Window* window;
            static YRGraphics* vk;
            static int32_t _frame;
            static float _dt, _idt; // float인 이유: 이 엔진 내에서는 SIMD에서 double보다 효율적인 float 자료형이 주로 사용되는데 이게 타임과 연산될 일이 잦은 편이기 때문
            static uint64_t _tp;
            static void* hd;
        private:
            static void windowResized(int x, int y);
            static void pollEvents();
            static void finalize();
            static bool init();
            static void update();
            static void render();
    };
}
#undef YRGraphics

#endif