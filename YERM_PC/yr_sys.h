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
#ifndef __YR_SYS_H__
#define __YR_SYS_H__

#include "../externals/vulkan/vulkan.h"
#include "yr_string.hpp"

#include <functional>
#include <vector>

struct GLFWwindow;

namespace onart{
    /// @brief 창 시스템에 대한 불투명 타입입니다. GLFW와 AGDK의 vulkan 대상 인터페이스를 비슷하게 맞추기 위해 만들어졌습니다.
    class Window{
        public:
            friend class Game;
            /// @brief 창 생성 시 부여할 옵션입니다. 일부 옵션은 유효한 환경이 제한됩니다. (현재 버전에서는 PC 옵션만 존재합니다.)
            struct CreationOptions{
                /// @brief 창 폭(픽셀). 기본값은 640이며 PC에서만 사용됩니다.
                int width = 640;
                /// @brief 창 높이(픽셀). 기본값은 480이며 PC에서만 사용됩니다.
                int height = 480;
                /// @brief 창 크기 조절 가능 여부. 기본값은 false이며 PC에서만 사용됩니다.
                bool resizable = true;
                /// @brief 초기 전체화면 여부. 기본값은 false이며 PC에서만 사용됩니다.
                bool fullScreen = false;
                /// @brief 창에 닫기 버튼 등을 기본으로 둡니다. 기본값은 true이며 PC에서만 사용됩니다.
                bool decorated = true;
                /// @brief utf-8로 된 창 제목입니다. 파일 인코딩에 무관하게 utf-8을 사용하려면 u8"" 리터럴을 이용합니다. 기본값은 "YERM"이며 PC에서만 사용됩니다.
                char title[128] = u8"YERM";
            };
            /// @brief 창 객체를 생성합니다. 잘못된 객체를 전달하여 프로그램이 잘못되는 경우는 고려하지 않습니다.
            /// @param hd 대상 환경이 android인 경우 android_app 객체를 전달합니다. PC 대상인 경우 이 매개변수는 무시됩니다.
            /// @param options 생성 옵션을 줍니다. 비워 두거나 nullptr를 전달하면 기본 옵션으로 사용됩니다. 자세한 내용은 @ref CreationOptions 구조체를 참고하세요.
            Window(void *hd = nullptr, const CreationOptions *options = nullptr);
            Window(const Window&)=delete;
            ~Window();
            /// @brief 창 이벤트가 발생할 때까지 기다렸다가 발생하면 처리를 수행합니다. (아직 처리되지 않은 이벤트가 남아 있으면 즉시 그것들을 처리하고 리턴합니다.) 이 함수는 메인 스레드에서만 호출할 수 있습니다.
            void waitEvents();
            /// @brief 발생한 창 이벤트에 대한 처리를 수행합니다. 등록한 콜백 함수들도 호출됩니다. 이 함수는 메인 스레드에서만 호출할 수 있습니다.
            void pollEvents();
            /// @brief 현 프레임에 창이 닫히는 경우 true를 리턴합니다. 안드로이드 대상의 경우 화면 회전 시에도 true가 되므로, 반드시 프로그램 종료를 의미하는 것이 아니니 주의하세요.
            bool windowShouldClose();
            /// @brief (PC) 이 창에 대하여 화면의 DPI와 플랫폼의 기본 DPI 비율을 알려줍니다. (Android) DPI를 알려줍니다.
            /// @param x x 스케일을 받을 위치입니다. nullptr를 주면 넘어가지 않습니다.
            /// @param y y 스케일을 받을 위치입니다. nullptr를 주면 넘어가지 않습니다.
            void getContentScale(float* x, float* y);
            /// @brief (PC) 화면 표시 영역의 화면 좌표 상 크기를 알려줍니다. 창 관리 단에서 사용하기 위한 것입니다. (Android) @ref getFramebufferSize와 동일합니다.
            /// @param x 가로 길이를 받을 위치입니다. nullptr를 주면 넘어가지 않습니다.
            /// @param y 세로 길이를 받을 위치입니다. nullptr를 주면 넘어가지 않습니다.
            void getSize(int* x, int* y);
            /// @brief 화면 표시 영역의 픽셀 크기를 알려줍니다. 그래픽스 단에서 사용하기 위한 것입니다.
            /// @param x 가로 길이를 받을 위치입니다. nullptr를 주면 넘어가지 않습니다.
            /// @param y 세로 길이를 받을 위치입니다. nullptr를 주면 넘어가지 않습니다.
            void getFramebufferSize(int* x, int* y);
            /// @brief 창 크기를 설정합니다.
            void setSize(unsigned x, unsigned y);
            /// @brief 전체화면인 창에 대하여 창 모드로 바꾸거나, 창 위치와 크기를 바꿉니다. PC 플랫폼 이외에서는 동작하지 않습니다.
            /// @param xpos 창 좌측의 위치(화면 좌표)입니다. 비워 두거나 음수를 주는 경우 주 모니터의 가운데로 맞춥니다.
            /// @param ypos 창 상단의 위치(화면 좌표)입니다. 비워 두거나 음수를 주는 경우 주 모니터의 가운데로 맞춥니다.
            /// @param width 창 가로 길이(화면 좌표)입니다. 비워 두거나 음수를 주는 경우 주 모니터의 반으로 맞춥니다.
            /// @param height 창 세로 길이(화면 좌표)입니다. 비워 두거나 음수를 주는 경우 주 모니터의 반으로 맞춥니다.
            void setWindowed(int xpos = -1, int ypos = -1, int width=-1, int height = -1);
            /// @brief 주어진 모니터에 대하여 전체 화면으로 바꿉니다. PC 플랫폼 이외에서는 동작하지 않습니다.
            /// @param monitor 모니터 번호
            void setFullScreen(int monitor = 0);
            /// @brief 가로로 화면을 고정합니다. (180도 회전은 허용합니다.) PC 플랫폼에선 아무 동작도 하지 않습니다.
            void setHorizontal();
            /// @brief 세로로 화면을 고정합니다. (180도 회전은 장치에서 가능하다면 허용합니다.) PC 플랫폼에선 아무 동작도 하지 않습니다.
            void setVertical();
            /// @brief 화면 고정을 해제합니다. PC 플랫폼에선 아무 동작도 하지 않습니다.
            void setLiberal();
            /// @brief 창 닫기를 요청합니다.
            void close();
            /// @brief 그래픽스 API 컨텍스트를 가지는 스레드를 현재 스레드로 결정합니다. Vulkan을 사용하는 경우 아무 동작도 하지 않습니다.
            void setMainThread();
            /// @brief 객체가 정상적으로 생성되었는지 확인합니다.
            inline bool isNormal(){ return isOn; }
            /// @brief Vulkan instance에서 활성화해야 할 확장 이름들을 얻어옵니다. 그 외의 플랫폼에서는 빈 벡터를 리턴합니다.
            std::vector<const char*> requiredInstanceExentsions();
            /// @brief PC 환경에서는 주어진 값을 그대로 리턴하고, 안드로이드에서는 internal data path에 상대적인 경로를 리턴합니다.
            /// @param p 255자를 넘을 수 없습니다.
            string255 rwPath(const string255& p);
            /// @brief 창 표면을 생성합니다.
            /// @param instance 인스턴스입니다.
            /// @param surface 생성 성공 시 창 표면 핸들을 이 위치로 리턴합니다.
            /// @return 결과 코드입니다.
            VkResult createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
            /// @brief 창이 최소화되거나 비활성화될 때 호출될 함수입니다.
            std::function<void(int, int)> loseFocusCallback;
            /// @brief 창 크기가 변경될 때 호출될 함수입니다. 시그니처: void(int [가로 길이(픽셀)], int [세로 길이(픽셀)])
            std::function<void(int, int)> windowSizeCallback;
            /// @brief 키 입력, 모바일 (가상)버튼 등에 대한 콜백을 등록합니다. 시그니처: void(int [키코드], int [스캔코드], int [동작코드], int [shift 등 추가])
            std::function<void(int, int, int, int)> keyCallback;
            /// @brief 마우스 클릭에 대한 콜백을 등록합니다. 시그니처: void(int [키코드], int [동작코드], int [shift 등 추가])
            std::function<void(int, int, int)> clickCallback;
            /// @brief 마우스 이동 시 호출되는 함수입니다. 시그니처: void(double [x좌표 (왼쪽 끝이 0, 오른쪽 끝이 1)], double [y좌표 (위쪽 끝이 0, 아래쪽 끝이 1)])
            std::function<void(double, double)> posCallback;
            /// @brief 마우스 휠, 트랙볼 등에 의한 스크롤 콜백을 등록합니다. 시그니처: void(double [x 오프셋, 주로 0 아니면 1], double [y 오프셋, 주로 0 아니면 1])
            std::function<void(double, double)> scrollCallback;
            /// @brief 터치 동작 중 터치 다운/업/슬라이드에 대한 콜백을 등록합니다. 현재 PC에서는 지원되지 않습니다. 시그니처: void(int [터치 id], int [동작코드], float [현재 x좌표], float[현재 y좌표])
            std::function<void(int, int, float, float)> touchCallback;
            /// @brief window surface가 처음 활성화될 때까지 기다리는 플래그로, 그 이후에는 사용하지 않습니다.
            bool surfaceAvailable = false;
            /// @brief 현재 연결되어 있는 모니터의 수를 리턴합니다. 안드로이드 대상에서는 반드시 1이 리턴됩니다.
            static int getMonitorCount();
            /// @brief 모니터 주사율을 리턴합니다.
            /// @param monitor  모니터 번호. 0번은 반드시 주 모니터입니다. 안드로이드 대상에서는 이 값이 무시됩니다.
            /// @return 기본적으로 모니터 주사율을 리턴하며, 유효하지 않은 입력이었던 경우 -1을 리턴합니다.
            int getMonitorRefreshRate(int monitor = 0);
            /// @brief 종료하기 전에 이것을 호출해야 합니다.
            static void terminate();
        private:
            static void _sizeCallback(GLFWwindow* window, int x, int y);
            static void _keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
            static void _mouseButtonCallback(GLFWwindow* window, int key, int action, int mods);
            static void _mousePosCallback(GLFWwindow* window, double x, double y);
            static void _scrollCallback(GLFWwindow* window, double x, double y);
            /// @brief 라이브러리 초기 세팅을 수행합니다.
            /// @return 초기화에 성공하면 true를 리턴합니다.
            static bool init();
            /// @brief 내부적으로 플랫폼별 중심 객체를 사용합니다. PC 플랫폼은 GLFWwindow*, 안드로이드는 android_app*입니다.
            void *window = nullptr;
            bool isOn = false;
    };
}

#endif