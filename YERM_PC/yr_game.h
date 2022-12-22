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

namespace onart{

    class VkMachine;

    /// @brief 프레임워크의 진입점입니다. Game, Game2 객체는 프로그램 상에서 둘이 합쳐 1개만 존재할 수 있습니다. 그 이후의 객체는 무효 객체가 되어 segfault 혹은 정의되지 않은 동작을 하게 됩니다.
    class Game{
        public:
            Game();
            /// @brief 창을 닫고 자원을 정상 해제한 후 게임을 종료합니다.
            void exit();
            /// @brief 게임을 시작합니다. 이 안에서 게임 루프가 계속해서 이루어집니다.
            /// @param hd android 환경에서는 android_app, 그 외에서는 nullptr입니다.
            /// @param opt 기본 옵션 외의 것을 주려면 @ref Window::CreationOptions 를 참고하세요.
            /// @return 종료 코드입니다. 0은 정상 종료, 1은 창 시스템 초기화 실패, 2는 이미 시작 중
            int start(void* hd = nullptr, Window::CreationOptions* opt = nullptr);
            static int32_t frameNumber();
            /// @brief 게임을 시작하고 난 시간(초)을 리턴합니다.
            static float tp();
            /// @brief 이전 프레임과 현 프레임 간의 간격(초)을 리턴합니다.
            static float dt();
            /// @brief 이전 프레임과 현 프레임 간의 간격(초)의 역수를 리턴합니다.
            static float idt();
        protected:
            Window* window;
            VkMachine* vk;
            int32_t frame;
            float _tp, _dt, _idt; // float인 이유: 이 엔진 내에서는 SIMD에서 double보다 효율적인 float 자료형이 주로 사용되는데 이게 타임과 연산될 일이 잦은 편이기 때문
            const std::chrono::steady_clock::time_point longTp;
            void finalize();
        private:
            bool init();
            void update();
            void render();
    };

    /// @brief 프레임워크의 진입점입니다. 2D 게임에 최적화되어 있습니다. Game, Game2 객체는 프로그램 상에서 둘이 합쳐 1개만 존재할 수 있습니다. 그 이후의 객체는 무효 객체가 되어 segfault 혹은 정의되지 않은 동작을 하게 됩니다.
    class Game2: public Game{
        public:
            /// @brief 게임을 시작합니다. 이 안에서 게임 루프가 계속해서 이루어집니다.
            /// @param hd android 환경에서는 android_app, 그 외에서는 nullptr입니다.
            /// @param opt 기본 옵션 외의 것을 주려면 @ref Window::CreationOptions 를 참고하세요.
            /// @return 종료 코드입니다. 0은 정상 종료, 1은 창 시스템 초기화 실패, 2는 이미 시작 중
            int start(void* hd = nullptr, Window::CreationOptions* opt = nullptr);
        private:
            bool init();
            
    };
}

#endif