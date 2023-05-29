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
#ifndef __YR_INPUT_H__
#define __YR_INPUT_H__

#include "../externals/boost/predef/platform.h"
#include "yr_math.hpp"

#include <cstdint>

#if BOOST_PLAT_ANDROID
#include <game-activity/native_app_glue/android_native_app_glue.h>
#else
#include "../externals/glfw/include/GLFW/glfw3.h"
#endif

namespace onart{
    class Input{
        friend class Game;
        friend class Game2;
        public:
            enum class KeyCode{
#if BOOST_PLAT_ANDROID
                // 숫자열
				alpha0 = AKEYCODE_0,
				alpha1 = AKEYCODE_1,
				alpha2 = AKEYCODE_2,
				alpha3 = AKEYCODE_3,
				alpha4 = AKEYCODE_4,
				alpha5 = AKEYCODE_5,
				alpha6 = AKEYCODE_6,
				alpha7 = AKEYCODE_7,
				alpha8 = AKEYCODE_8,
				alpha9 = AKEYCODE_9,
				minus = AKEYCODE_MINUS,
				equal = AKEYCODE_EQUALS,
				prime = AKEYCODE_GRAVE,
				backspace = AKEYCODE_DEL,
				// 키패드
				pad0 = AKEYCODE_NUMPAD_0,
				pad1 = AKEYCODE_NUMPAD_1,
				pad2 = AKEYCODE_NUMPAD_2,
				pad3 = AKEYCODE_NUMPAD_3,
				pad4 = AKEYCODE_NUMPAD_4,
				pad5 = AKEYCODE_NUMPAD_5,
				pad6 = AKEYCODE_NUMPAD_6,
				pad7 = AKEYCODE_NUMPAD_7,
				pad8 = AKEYCODE_NUMPAD_8,
				pad9 = AKEYCODE_NUMPAD_9,
				pad_slash = AKEYCODE_NUMPAD_DIVIDE,
				asterisk = AKEYCODE_NUMPAD_MULTIPLY,
				pad_minus = AKEYCODE_NUMPAD_SUBTRACT,
				plus = AKEYCODE_NUMPAD_ADD,
				pad_enter = AKEYCODE_NUMPAD_ENTER,
				numlock = AKEYCODE_NUM_LOCK,
				// 알파벳
				A = AKEYCODE_A,
				B = AKEYCODE_B,
				C = AKEYCODE_C,
				D = AKEYCODE_D,
				E = AKEYCODE_E,
				F = AKEYCODE_F,
				G = AKEYCODE_G,
				H = AKEYCODE_H,
				I = AKEYCODE_I,
				J = AKEYCODE_J,
				K = AKEYCODE_K,
				L = AKEYCODE_L,
				M = AKEYCODE_M,
				N = AKEYCODE_N,
				O = AKEYCODE_O,
				P = AKEYCODE_P,
				Q = AKEYCODE_Q,
				R = AKEYCODE_R,
				S = AKEYCODE_S,
				T = AKEYCODE_T,
				U = AKEYCODE_U,
				V = AKEYCODE_V,
				W = AKEYCODE_W,
				X = AKEYCODE_X,
				Y = AKEYCODE_Y,
				Z = AKEYCODE_Z,
				// 최상단
				F1 = AKEYCODE_F1,
				F2 = AKEYCODE_F2,
				F3 = AKEYCODE_F3,
				F4 = AKEYCODE_F4,
				F5 = AKEYCODE_F5,
				F6 = AKEYCODE_F6,
				F7 = AKEYCODE_F7,
				F8 = AKEYCODE_F8,
				F9 = AKEYCODE_F9,
				F10 = AKEYCODE_F10,
				F11 = AKEYCODE_F11,
				F12 = AKEYCODE_F12,
				escape = AKEYCODE_ESCAPE,
				print = AKEYCODE_SYSRQ,
				scroll = AKEYCODE_SCROLL_LOCK,
				pause = AKEYCODE_BREAK,
				// 기능 패드
				insert = AKEYCODE_INSERT,
				home = AKEYCODE_MOVE_HOME,
				pageup = AKEYCODE_PAGE_UP,
				pagedown = AKEYCODE_PAGE_DOWN,
				end = AKEYCODE_MOVE_END,
				Delete = AKEYCODE_FORWARD_DEL,
				// 좌/하단
				tab = AKEYCODE_TAB,
				capslock = AKEYCODE_CAPS_LOCK,
				shift_L = AKEYCODE_SHIFT_LEFT,
				shift_R = AKEYCODE_SHIFT_RIGHT,
				ctrl_L = AKEYCODE_CTRL_LEFT,
				ctrl_R = AKEYCODE_CTRL_RIGHT,
				window_L = AKEYCODE_WINDOW,
				window_R = AKEYCODE_WINDOW,
				alt_L = AKEYCODE_ALT_LEFT,
				alt_R = AKEYCODE_ALT_RIGHT,
				// 방향키
				left = AKEYCODE_DPAD_LEFT,
				right = AKEYCODE_DPAD_RIGHT,
				up = AKEYCODE_DPAD_UP,
				down = AKEYCODE_DPAD_DOWN,
				// 나머지
				comma = AKEYCODE_COMMA,
				period = AKEYCODE_PERIOD,
				slash = AKEYCODE_SLASH,
				semicolon = AKEYCODE_SEMICOLON,
				apostrophe = AKEYCODE_APOSTROPHE,
				backslash = AKEYCODE_BACKSLASH,
				left_br = AKEYCODE_LEFT_BRACKET,
				right_br = AKEYCODE_RIGHT_BRACKET,
				space = AKEYCODE_SPACE,
				enter = AKEYCODE_ENTER,
                cancel = AKEYCODE_BACK, // 보통 우측 하단에 있는 뒤로 가기 키
#else
                // 숫자열
				alpha0 = GLFW_KEY_0,
				alpha1 = GLFW_KEY_1,
				alpha2 = GLFW_KEY_2,
				alpha3 = GLFW_KEY_3,
				alpha4 = GLFW_KEY_4,
				alpha5 = GLFW_KEY_5,
				alpha6 = GLFW_KEY_6,
				alpha7 = GLFW_KEY_7,
				alpha8 = GLFW_KEY_8,
				alpha9 = GLFW_KEY_9,
				minus = GLFW_KEY_MINUS,
				equal = GLFW_KEY_EQUAL,
				prime = GLFW_KEY_GRAVE_ACCENT,
				backspace = GLFW_KEY_BACKSPACE,
				// 키패드
				pad0 = GLFW_KEY_KP_0,
				pad1 = GLFW_KEY_KP_1,
				pad2 = GLFW_KEY_KP_2,
				pad3 = GLFW_KEY_KP_3,
				pad4 = GLFW_KEY_KP_4,
				pad5 = GLFW_KEY_KP_5,
				pad6 = GLFW_KEY_KP_6,
				pad7 = GLFW_KEY_KP_7,
				pad8 = GLFW_KEY_KP_8,
				pad9 = GLFW_KEY_KP_9,
				pad_slash = GLFW_KEY_KP_DIVIDE,
				asterisk = GLFW_KEY_KP_MULTIPLY,
				pad_minus = GLFW_KEY_KP_SUBTRACT,
				plus = GLFW_KEY_KP_ADD,
				pad_enter = GLFW_KEY_KP_ENTER,
				numlock = GLFW_KEY_NUM_LOCK,
				// 알파벳
				A = GLFW_KEY_A,
				B = GLFW_KEY_B,
				C = GLFW_KEY_C,
				D = GLFW_KEY_D,
				E = GLFW_KEY_E,
				F = GLFW_KEY_F,
				G = GLFW_KEY_G,
				H = GLFW_KEY_H,
				I = GLFW_KEY_I,
				J = GLFW_KEY_J,
				K = GLFW_KEY_K,
				L = GLFW_KEY_L,
				M = GLFW_KEY_M,
				N = GLFW_KEY_N,
				O = GLFW_KEY_O,
				P = GLFW_KEY_P,
				Q = GLFW_KEY_Q,
				R = GLFW_KEY_R,
				S = GLFW_KEY_S,
				T = GLFW_KEY_T,
				U = GLFW_KEY_U,
				V = GLFW_KEY_V,
				W = GLFW_KEY_W,
				X = GLFW_KEY_X,
				Y = GLFW_KEY_Y,
				Z = GLFW_KEY_Z,
				// 최상단
				F1 = GLFW_KEY_F1,
				F2 = GLFW_KEY_F2,
				F3 = GLFW_KEY_F3,
				F4 = GLFW_KEY_F4,
				F5 = GLFW_KEY_F5,
				F6 = GLFW_KEY_F6,
				F7 = GLFW_KEY_F7,
				F8 = GLFW_KEY_F8,
				F9 = GLFW_KEY_F9,
				F10 = GLFW_KEY_F10,
				F11 = GLFW_KEY_F11,
				F12 = GLFW_KEY_F12,
				escape = GLFW_KEY_ESCAPE,
				print = GLFW_KEY_PRINT_SCREEN,
				scroll = GLFW_KEY_SCROLL_LOCK,
				pause = GLFW_KEY_PAUSE,
				// 기능 패드
				insert = GLFW_KEY_INSERT,
				home = GLFW_KEY_HOME,
				pageup = GLFW_KEY_PAGE_UP,
				pagedown = GLFW_KEY_PAGE_DOWN,
				end = GLFW_KEY_END,
				Delete = GLFW_KEY_DELETE,
				// 좌/하단
				tab = GLFW_KEY_TAB,
				capslock = GLFW_KEY_CAPS_LOCK,
				shift_L = GLFW_KEY_LEFT_SHIFT,
				shift_R = GLFW_KEY_RIGHT_SHIFT,
				ctrl_L = GLFW_KEY_LEFT_CONTROL,
				ctrl_R = GLFW_KEY_RIGHT_CONTROL,
				window_L = GLFW_KEY_LEFT_SUPER,
				window_R = GLFW_KEY_RIGHT_SUPER,
				alt_L = GLFW_KEY_LEFT_ALT,
				alt_R = GLFW_KEY_RIGHT_ALT,
				// 방향키
				left = GLFW_KEY_LEFT,
				right = GLFW_KEY_RIGHT,
				up = GLFW_KEY_UP,
				down = GLFW_KEY_DOWN,
				// 나머지
				comma = GLFW_KEY_COMMA,
				period = GLFW_KEY_PERIOD,
				slash = GLFW_KEY_SLASH,
				semicolon = GLFW_KEY_SEMICOLON,
				apostrophe = GLFW_KEY_APOSTROPHE,
				backslash = GLFW_KEY_BACKSLASH,
				left_br = GLFW_KEY_LEFT_BRACKET,
				right_br = GLFW_KEY_RIGHT_BRACKET,
				space = GLFW_KEY_SPACE,
				enter = GLFW_KEY_ENTER,
                cancel = GLFW_KEY_BACKSPACE,
#endif
            };

            enum class MouseKeyCode {
#if BOOST_PLAT_ANDROID
				left = AMOTION_EVENT_BUTTON_PRIMARY,
				right = AMOTION_EVENT_BUTTON_SECONDARY,
				middle = AMOTION_EVENT_BUTTON_TERTIARY,
				//wheel_up = AMOTION_EVENT_BUTTON_FORWARD,
				//wheel_down = AMOTION_EVENT_BUTTON_BACK
#else
				left = GLFW_MOUSE_BUTTON_LEFT,
				right = GLFW_MOUSE_BUTTON_RIGHT,
				middle = GLFW_MOUSE_BUTTON_MIDDLE,
				wheel_up = GLFW_MOUSE_BUTTON_LAST + 1,
				wheel_down = GLFW_MOUSE_BUTTON_LAST + 2
#endif
			};

#if BOOST_PLAT_ANDROID
            constexpr static int KEY_DOWN = AKEY_EVENT_ACTION_DOWN;
            constexpr static int KEY_UP = AKEY_EVENT_ACTION_UP;
#else
            constexpr static int KEY_DOWN = GLFW_PRESS;
            constexpr static int KEY_UP = GLFW_RELEASE;
#endif
            /// @brief 주어진 키가 현재 눌려 있는지 확인합니다.
            static bool isKeyDown(KeyCode key);
            /// @brief 주어진 키가 이번 프레임에 눌렸는지 확인합니다.
            static bool isKeyDownNow(KeyCode key);
            /// @brief 주어진 키가 이번 프레임에 떼였는지 확인합니다.
            static bool isKeyUpNow(KeyCode key);

            /// @brief 주어진 키가 현재 눌려 있는지 확인합니다.
            static bool isKeyDown(MouseKeyCode key);
            /// @brief 주어진 키가 이번 프레임에 눌렸는지 확인합니다.
            static bool isKeyDownNow(MouseKeyCode key);
            /// @brief 주어진 키가 이번 프레임에 떼였는지 확인합니다.
            static bool isKeyUpNow(MouseKeyCode key);

            /// @brief 터치 입력에 관한 개별 정보입니다.
            struct TouchInfo{
                friend class Input;
				/// @brief 좌측 상단이 0,0이며 단위는 픽셀입니다.
                vec2 pos;
				/// @brief 터치가 시작되면서 부여된 ID입니다. 같은 ID인 객체는 항상 같은 위치에서 찾을 수 있습니다. 지나간 ID는 다시 부여되지 않습니다.
                int64_t id;
				/// @brief 이 터치가 시작된 프레임 번호이거나 터치가 끝난 프레임 번호의 부호 반전을 담고 있습니다. 직접 사용해도 되지만 멤버함수를 사용하는 것이 더 편합니다.
                int frame;
                /// @brief 지금 프레임에 이 터치가 발생한 경우 true를 리턴합니다.
                bool isPressedNow() const;
                /// @brief 이 터치가 현재 유효한 경우 true를 리턴합니다.
                bool isPressed() const;
                /// @brief 지금 프레임에 이 터치가 떼어진 경우라면 true를 리턴합니다.
                bool isUpNow() const;
            };

        private:
            static int32_t pressedKey[];
            static int32_t pressedMouseKey[];
            static dvec2 mousePos;
            static TouchInfo _touches[4]; // 일단 터치는 최대 4개까지만 지원하는 걸로
            
            /// @brief 현재 프레임에 들어온 키를 등록합니다.
            static void keyboard(int keycode, int scancode, int action, int mod);
            /// @brief 현재 프레임에 들어온 키를 등록합니다. (마우스)
            static void click(int key, int action, int mods);
            /// @brief 마우스 커서의 위치를 저장합니다.
            static void moveCursor(double x, double y);
            /// @brief 터치 위치를 저장합니다.
            static void touch(int id, int action, float x, float y);
        public:
            /// @brief 현재 프레임의 터치 상태를 확인할 수 있습니다. 현재 PC 버전에서는 사용할 수 없습니다.
            static const decltype(_touches)& touches;
			/// @brief 현재 프레임의 마우스 위치입니다.
			static const dvec2& mousePosition;
    };
}

#endif