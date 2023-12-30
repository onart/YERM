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

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "../externals/vulkan/vulkan_android.h"

namespace onart{

#define _HAPP reinterpret_cast<android_app*>(window)

    static void initCmd(android_app* app, int32_t cmd){
        if(cmd == NativeAppGlueAppCmd::APP_CMD_INIT_WINDOW) {
            Window* w = (Window*)app->userData;
            w->surfaceAvailable = true;
        }
    }
    
    /// @brief 안드로이드 환경의 기본 이벤트에 대한 콜백으로 쓰입니다.
    static void handleCmd(android_app* app, int32_t cmd){
        switch (cmd) {
        case NativeAppGlueAppCmd::APP_CMD_CONFIG_CHANGED:
            {
                // 장치 구성 속성이 변경된 경우
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_CONTENT_RECT_CHANGED:
            {
                // 창의 표시 영역이 변경된 경우 (app->contentRect 확인)
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_DESTROY:
            {
                // 액티비티 종료됨.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_GAINED_FOCUS:
            {
                // 액티비티가 포커스를 받은 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_INIT_WINDOW:
            {
                // app->window를 통해 창 표면을 사용할 수 있게 됨.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_LOST_FOCUS:
            {
                // 액티비티가 포커스를 잃은 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_LOW_MEMORY:
            {
                // 메모리가 부족하다는 경고라고 합니다.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_PAUSE:
            {
                // 액티비티가 중지된 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_RESUME:
            {
                // 액티비티가 재개된 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_SAVE_STATE:
            {
                // 여기서는 여러분이 만든 임의의 컨텍스트를 저장해 두는 게 좋습니다.
                // app->savedState를 활용하고, app->savedStateSize를 세팅합니다.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_START:
            {
                // 액티비티가 시작된 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_STOP:
            {
                // 액티비티가 정지된 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_TERM_WINDOW:
            {
                // window가 종료되려는 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_WINDOW_INSETS_CHANGED:
            {
                // Window 인셋이 변경된 경우.
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_WINDOW_REDRAW_NEEDED:
            {
                // ANativeWindow를 깔끔하게 다시 그려야 합니다.
                Window* w = (Window*)app->userData;
                int x,y;
                w->getFramebufferSize(&x,&y);
                if(w->windowSizeCallback) w->windowSizeCallback(x,y);
                break;
            }
        case NativeAppGlueAppCmd::APP_CMD_WINDOW_RESIZED:
            {
                // ANativeWindow 크기가 변한 경우.
                break;
            }
        case NativeAppGlueAppCmd::UNUSED_APP_CMD_INPUT_CHANGED:
            {
                // Unused. Reserved for future use when usage of AInputQueue will be supported
                break;
            }
        default:
            break;
        }
    }

    static void setOrientation(android_app* app, int orientation){
        JNIEnv* jni;
        app->activity->vm->AttachCurrentThread(&jni, nullptr);
        jclass clazz = jni->GetObjectClass(app->activity->javaGameActivity);
        jmethodID methodID = jni->GetMethodID(clazz, "setRequestedOrientation", "(I)V");
        jni->CallVoidMethod(app->activity->javaGameActivity, methodID, orientation);
        app->activity->vm->DetachCurrentThread();
    }

    /// @brief 들어온 입력이 있으면 처리합니다.
    static void onInput(android_app* app){
        android_input_buffer* inputs=android_app_swap_input_buffers(app);
        Window* w = (Window*)app->userData;
        if(inputs){
            for(uint64_t i=0;i<inputs->motionEventsCount;i++){
                GameActivityMotionEvent& ev = inputs->motionEvents[i];
                switch (ev.action & AMOTION_EVENT_ACTION_MASK) {
                    case AMOTION_EVENT_ACTION_DOWN:
                    case AMOTION_EVENT_ACTION_UP:
                        for(uint32_t j=0;j<ev.pointerCount;j++){
                            float x = GameActivityPointerAxes_getX(&ev.pointers[j]);
                            float y = GameActivityPointerAxes_getY(&ev.pointers[j]);
                            int32_t id = ev.pointers[j].id;
                            if(ev.source == AINPUT_SOURCE_TOUCHSCREEN) if(w->touchCallback) w->touchCallback(id, ev.action, x, y);
                            //else if(ev.source == AINPUT_SOURCE_MOUSE) if(w->clickCallback) w->clickCallback(ev.action, x, y);
                        }
                        break;
                    case AMOTION_EVENT_ACTION_MOVE:
                        for(uint32_t j = 0;j < ev.pointerCount;j++){
                            float x = GameActivityPointerAxes_getX(&ev.pointers[j]);
                            float y = GameActivityPointerAxes_getY(&ev.pointers[j]);
                            int32_t id = ev.pointers[j].id;
                            if(ev.source == AINPUT_SOURCE_TOUCHSCREEN) if(w->touchCallback) w->touchCallback(id, ev.action, x, y);
                            //else if(ev.source == AINPUT_SOURCE_MOUSE) if(w->posCallback) w->posCallback(x, y);
                        }
                        break;
                    default:
                        // 참고: https://developer.android.com/reference/android/view/MotionEvent#constants_1
                        break;
                }
            }
            for(uint64_t i=0;i<inputs->keyEventsCount;i++){
                GameActivityKeyEvent& ev = inputs->keyEvents[i];
                switch (ev.action) {
                    case AKEY_EVENT_ACTION_DOWN:
                    case AKEY_EVENT_ACTION_UP:
                        // ev.unicodeChar
                        if(w->keyCallback) w->keyCallback(ev.keyCode, 0, ev.action, 0);
                        break;
                    case AKEY_EVENT_ACTION_MULTIPLE:
                        // Multiple duplicate key events have occurred in a row,
                        // or a complex string is being delivered.
                        // The repeat_count property of the key event contains the number of times the given key code should be executed.
                        break;
                    default:
                        // 참고: https://developer.android.com/reference/android/view/KeyEvent#constants_1
                        break;
                }
            }
            // 아래를 호출하지 않으면 이벤트 버퍼가 가득 찹니다.
            android_app_clear_key_events(inputs);
            android_app_clear_motion_events(inputs);
        }
}

    bool Window::init(){
        return true;
    }

    Window::Window(void* hd, const CreationOptions* options): window(hd) {
        _HAPP->onAppCmd = onart::initCmd;
        _HAPP->userData = this;
        while(!surfaceAvailable){ 
            int events;
            android_poll_source* source;
            while(ALooper_pollAll(1, nullptr, &events, (void**)&source) >= 0) {
                if(source != nullptr){
                    source->process(source->app, source);
                        android_input_buffer* inputs=android_app_swap_input_buffers(source->app);
                        if(inputs){
                            android_app_clear_key_events(inputs);
                            android_app_clear_motion_events(inputs);
                        }
                }
                if(_HAPP->destroyRequested){
                    break;
                }
            }
        }
        _HAPP->onAppCmd = onart::handleCmd;
        isOn = true;
    }

    Window::~Window(){ }

    void Window::pollEvents(){
        int events;
        android_poll_source* source;
        while(ALooper_pollAll(0, nullptr, &events, (void**)&source) >= 0) {
            if(source != nullptr){
                source->process(source->app, source);
            }
            if(_HAPP->destroyRequested){
                break;
            }
        }
        if(!_HAPP->destroyRequested) onart::onInput(_HAPP);
    }

    void Window::waitEvents() {
        int events;
        android_poll_source* source;
        int timeout = -1;
        while(ALooper_pollAll(timeout, nullptr, &events, (void**)&source) >= 0) {
            if(source != nullptr){
                source->process(source->app, source);
            }
            if(_HAPP->destroyRequested){
                break;
            }
            timeout = 0;
        }
        if(!_HAPP->destroyRequested) onart::onInput(_HAPP);
    }

    bool Window::windowShouldClose(){
        return _HAPP->destroyRequested;
    }

    void Window::getContentScale(float* x, float* y){
        AConfiguration* configuration = AConfiguration_new();
        AConfiguration_fromAssetManager(configuration, _HAPP->activity->assetManager);
        int32_t density = AConfiguration_getDensity(configuration);
        AConfiguration_delete(configuration);
        if(x) *x = density;
        if(y) *y = density;
    }

    void Window::getFramebufferSize(int* x, int* y){
        if(!_HAPP->window) return;
        if(x) *x = ANativeWindow_getWidth(_HAPP->window);
        if(y) *y = ANativeWindow_getHeight(_HAPP->window);
    }

    void Window::getSize(int* x,int* y){
        getFramebufferSize(x,y);
    }

    VkResult Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface){
        VkAndroidSurfaceCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        info.window = _HAPP->window;
        PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR = (PFN_vkCreateAndroidSurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR");
        if(vkCreateAndroidSurfaceKHR) { return vkCreateAndroidSurfaceKHR(instance, &info, nullptr, surface); }
        LOGWITH("Error: Failed to dispatch procedure vkCreateAndroidSurfaceKHR");
        return VkResult::VK_ERROR_UNKNOWN;
    }

    std::vector<const char*> Window::requiredInstanceExentsions(){
        return { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
    }

    int Window::getMonitorRefreshRate(int monitor){
        return 60; // TODO: Swappy 없이 실제 값 구하기가 가능한가?
    }

    int Window::getMonitorCount(){
        return 1;
    }

    void Window::close(){
        GameActivity_finish(_HAPP->activity); // TODO: W/GameActivity: Failed writing to work fd가 뜨는데 괜찮은가?
    }

    void Window::setHorizontal() {
        constexpr int32_t USER_HORIZONTAL = 0xb;
        setOrientation(_HAPP, USER_HORIZONTAL);
    }

    void Window::setVertical() {
        constexpr int32_t USER_VERTICAL = 0xc;
        setOrientation(_HAPP, USER_VERTICAL);
    }

    void Window::setLiberal() {
        constexpr int32_t USER_ORIENTATION = 2;
        setOrientation(_HAPP, USER_ORIENTATION);
    }

    string255 Window::rwPath(const string255& p){ 
        return string255(_HAPP->activity->internalDataPath) + p;
    }

    void Window::setMainThread() {
#if defined(YR_USE_GLES)
        // eglMakeCurrent
#endif
    }

    void Window::setSize(unsigned, unsigned){ }
    void Window::setWindowed(int,int,int,int){ }
    void Window::setFullScreen(int){ }
    void Window::terminate() { }

#undef _HAPP
}

#else

#if BOOST_OS_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include "../externals/glfw/include/GLFW/glfw3.h"
#include "../externals/glfw/include/GLFW/glfw3native.h"

namespace onart{

    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    void Window::_sizeCallback(GLFWwindow* window, int x, int y){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->windowSizeCallback) w->windowSizeCallback(x, y);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    void Window::_keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->keyCallback) w->keyCallback(key, scancode, action, mods);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    void Window::_mouseButtonCallback(GLFWwindow* window, int key, int action, int mods) {
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->clickCallback) w->clickCallback(key, action, mods);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    void Window::_mousePosCallback(GLFWwindow* window, double x, double y){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->posCallback) w->posCallback(x, y);
    }
    /// @brief GLFW 이벤트 콜백과 사용자 지정 콜백을 연결합니다.
    void Window::_scrollCallback(GLFWwindow* window, double x, double y){
        Window *w = (Window*)glfwGetWindowUserPointer(window);
        if(w->scrollCallback) w->scrollCallback(x, y);
    }

    Window::Window(void *hd, const CreationOptions *options){
        CreationOptions defaultOpts;
        if (!options) options = &defaultOpts;
        init();
        glfwWindowHint(GLFW_DECORATED, options->decorated);
        glfwWindowHint(GLFW_RESIZABLE, options->resizable);
        glfwWindowHint(GLFW_VISIBLE, false);
#if defined(YR_USE_VULKAN) || defined(YR_USE_D3D11)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#elif defined(YR_USE_OPENGL)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#elif defined(YR_USE_OPENGLES)

#endif
        window = glfwCreateWindow(options->width, options->height, options->title, options->fullScreen ? glfwGetPrimaryMonitor() : nullptr, nullptr);
        if(!window) {
            const char* err; glfwGetError(&err); // 오류 발생 시점에 GLFW가 할당해 갖고 있으며 해제는 또 오류가 나거나 terminate 시점에 되는 데이터이기 때문에 밖에서 free하면 안 됨
            LOGWITH("Error creating window:", err);
            return;
        }
        GLFWwindow *gw = (GLFWwindow *)window;
        glfwSetWindowUserPointer(gw, this);
        glfwSetFramebufferSizeCallback(gw, _sizeCallback);
        glfwSetKeyCallback(gw, _keyCallback);
        glfwSetMouseButtonCallback(gw, _mouseButtonCallback);
        glfwSetCursorPosCallback(gw, _mousePosCallback);
        glfwSetScrollCallback(gw, _scrollCallback);
        glfwShowWindow(gw);
        // TODO:
        // glfwSetWindowFocusCallback PC/Android 활성/비활성.
        //
        // glfwSetWindowCloseCallback PC 창 닫기
        // glfwSetWindowIconifyCallback PC/Android 최소화/복원(Android는 window destroy에 해당)
        isOn=true;
    }

    Window::~Window(){
        glfwDestroyWindow((GLFWwindow*)window);
    }

    int Window::getMonitorRefreshRate(int monitor){
        init();
        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if((unsigned)monitor < (unsigned)count){
            return glfwGetVideoMode(monitors[monitor])->refreshRate;
        }
        LOGWITH("Invalid monitor number");
        return -1;
    }

    bool Window::windowShouldClose(){
        return glfwWindowShouldClose((GLFWwindow*)window);
    }

    void Window::close(){
        glfwWindowShouldClose((GLFWwindow*)window);
    }

    bool Window::init(){
        static bool isGLFWOn = false;
        return isGLFWOn || (isGLFWOn = glfwInit());
    }

    void Window::pollEvents(){
        glfwPollEvents();
    }

    void Window::waitEvents() {
        glfwWaitEvents();
    }

    void Window::getContentScale(float* x, float* y){
        glfwGetWindowContentScale((GLFWwindow*)window, x, y);
    }

    void Window::setSize(unsigned x, unsigned y){
        glfwSetWindowSize((GLFWwindow*)window, x, y);
    }

    void Window::getSize(int* x, int* y){
        glfwGetWindowSize((GLFWwindow*)window, x, y);
    }

    void Window::getFramebufferSize(int* x, int* y){
        glfwGetFramebufferSize((GLFWwindow*)window, x, y);
    }

    void Window::setFullScreen(int monitor){
        int count;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        if((unsigned)monitor < (unsigned)count){
            glfwSetWindowMonitor((GLFWwindow*)window, monitors[monitor], 0, 0, 0, 0, GLFW_DONT_CARE);
        }
        LOGWITH("Invalid monitor number");
    }

    void Window::setWindowed(int xpos, int ypos, int width, int height){
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if(width <= 0){ width = mode->width; }
        if(height <= 0) { height = mode->height; }
        if(xpos < 0){
            glfwGetMonitorPos(monitor, &xpos, nullptr);
            xpos += (mode->width - width) / 2;
        }
        if(ypos < 0){
            glfwGetMonitorPos(monitor, nullptr, &ypos);
            ypos += (mode->height - height) / 2;
        }
        glfwSetWindowMonitor((GLFWwindow*)window, nullptr, xpos, ypos, width, height, GLFW_DONT_CARE);
    }

    int Window::getMonitorCount(){
        init();
        int count;
        glfwGetMonitors(&count);
        return count;
    }

    VkResult Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface){
        return glfwCreateWindowSurface(instance, (GLFWwindow*)window, nullptr, surface);
    }

    void* Window::getWin32Handle() {
#if BOOST_OS_WINDOWS
        return glfwGetWin32Window((GLFWwindow*)window);
#else
        return nullptr;
#endif
    }

    void Window::glRefreshInterval(int count) {
#ifdef YR_USE_OPENGL
        glfwSwapInterval(count);
#endif
    }

    void Window::glPresent() {
#ifdef YR_USE_OPENGL
        glfwSwapBuffers((GLFWwindow*)window);
#endif
    }

    std::vector<const char*> Window::requiredInstanceExentsions(){
        uint32_t count;
        const char** names = glfwGetRequiredInstanceExtensions(&count);
        if(!names){ LOGWITH("GLFW Error"); return {}; }
        std::vector<const char*> strs;
        strs.reserve(count);
        for(uint32_t i = 0; i < count; i++) {
            strs.push_back(names[i]);
        }
        return strs;
    }

    void Window::terminate(){
        glfwTerminate();
    }
    
    void Window::setMainThread() {
#if defined(YR_USE_OPENGL) || defined(YR_USE_GLES)
        glfwMakeContextCurrent((GLFWwindow*)window);
#endif
    }

    string255 Window::rwPath(const string255& p){ return p; }

    void Window::setHorizontal(){ }
    void Window::setVertical(){ }
    void Window::setLiberal(){ }
}

#endif