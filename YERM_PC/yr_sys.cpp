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
#include "yr_sys.h"
#include "logger.hpp"

#include "../externals/boost/predef/platform.h"
#include "../externals/boost/predef/os.h"

#if BOOST_PLAT_ANDROID

#include 

namespace onart{
    bool Window::init(){
        return true;
    }
}

#else

#include "../externals/glfw/glfw3.h"

namespace onart{

    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    static void windowSizeCallback(GLFWwindow* window, int x, int y){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->windowSizeCallback) w->windowSizeCallback(x, y);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->keyCallback) w->keyCallback(key, scancode, action, mods);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    static void mouseButtonCallback(GLFWwindow* window, int key, int action, int mods) {
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->clickTouchCallback) w->clickTouchCallback(key, action, mods);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    static void mousePosCallback(GLFWwindow* window, double x, double y){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->posCallback) w->posCallback(x, y);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    static void scrollCallback(GLFWwindow* window, double x, double y){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->scrollCallback) w->scrollCallback(x, y);
    }

    Window::Window(void *hd, const CreationOptions *options){
        CreationOptions defaultOpts;
        if (!options) options = &defaultOpts;
        glfwWindowHint(GLFW_DECORATED, options->decorated);
        glfwWindowHint(GLFW_RESIZABLE, options->resizable);
        glfwWindowHint(GLFW_VISIBLE, false);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(options->width, options->height, options->title, options->fullScreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
        if(!window) {
            const char* err; glfwGetError(&err); // 오류 발생 시점에 GLFW가 할당해 갖고 있으며 해제는 또 오류가 나거나 terminate 시점에 되는 데이터이기 때문에 밖에서 free하면 안 됨
            LOGWITH("Error creating window:", err);
            return;
        }
        GLFWwindow *gw = (GLFWwindow *)window;
        glfwSetWindowUserPointer(gw, this);
        glfwSetWindowSizeCallback(gw, onart::windowSizeCallback);
        glfwSetKeyCallback(gw, onart::keyCallback);
        glfwSetMouseButtonCallback(gw, onart::mouseButtonCallback);
        glfwSetCursorPosCallback(gw, onart::mousePosCallback);
        glfwSetScrollCallback(gw, onart::scrollCallback);
    }

    Window::~Window(){

    }

    bool Window::windowShouldClose(){
        return glfwWindowShouldClose((GLFWwindow*)window);
    }

    bool Window::init(){
        static bool isOn = false;
        return isOn || (isOn = glfwInit());
    }

    void Window::pollEvents(){
        glfwPollEvents();
    }

    void Window::terminate(){
        glfwTerminate();
    }
}

#endif