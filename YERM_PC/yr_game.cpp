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
#include "../externals/glad/glad.h"
#include "yr_game.h"
#include "yr_sys.h"
#include "yr_graphics.h"
#include "yr_input.h"
#include "yr_audio.h"

#include "logger.hpp"
#include "yr_math.hpp"
#include "yr_bits.hpp"

#include "yr_constants.hpp"

#include "../externals/boost/predef/platform.h"
#include "../externals/boost/predef/compiler.h"

#include <thread>

#if BOOST_PLAT_ANDROID
#include <game-activity/native_app_glue/android_native_app_glue.h>
#elif defined(EMSCRIPTEN)
#include <emscripten.h>
#define YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD 1
#endif

namespace onart{

    const std::chrono::steady_clock::time_point longTp = std::chrono::steady_clock::now();
    float Game::_dt = 0.016f, Game::_idt = 60.0f;
    uint64_t Game::_tp = (longTp - std::chrono::steady_clock::time_point()).count();
    const float &Game::dt(Game::_dt), &Game::idt(Game::_idt);
    const uint64_t& Game::tp(Game::_tp);
    int32_t Game::_frame = 1;
    const int32_t& Game::frame(Game::_frame);
    YRGraphics* Game::vk = nullptr;
    Window* Game::window = nullptr;
    void* Game::hd = nullptr;
    int32_t Game::loopFlag = 0;
    std::function<void()> Game::perFrameProc;
    std::function<void()> Game::onInit;

#ifndef YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
#define YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD (BOOST_PLAT_ANDROID)
#else
#undef YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
#define YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD 1
#endif

    static std::mutex eventQMutex;

    enum class WindowEvent{ WE_SIZE = 0, WE_KEYBOARD = 1, WE_CLICK = 2, WE_CURSOR = 3, WE_SCROLL = 4 };
    struct EV{
        WindowEvent sType;
        union{
            struct{int sizeX, sizeY;};
            struct{int key, scancode, action, mods;};
            struct{int mouseKey, mouseAction, mouseMods;};
            struct{double mouseX, mouseY;};
            struct{double scrollX, scrollY;};
        };
    };

    static std::vector<EV> eventQ;
    static std::vector<EV> tempQ;

    static void recordSizeEvent(int x, int y) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_SIZE;
        back.sizeX = x;
        back.sizeY = y;
    }

    static void recordKeyEvent(int key, int scancode, int action, int mods) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_KEYBOARD;
        back.key = key;
        back.scancode = scancode;
        back.action = action;
        back.mods = mods;
    }

    static void recordClickEvent(int key, int action, int mods) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_CLICK;
        back.mouseKey = key;
        back.mouseAction = action;
        back.mouseMods = mods;
    }

    static void recordCursorEvent(double x, double y) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_CURSOR;
        back.mouseX = x;
        back.mouseY = y;
    }

    static void recordScrollEvent(double x, double y) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_SCROLL;
        back.scrollX = x;
        back.scrollY = y;
    }

    bool loaded = false; // async 테스트용
    static float thr = 1.f;

    int Game::start(void* hd, Window::CreationOptions* opt){
        if(window) {
            LOGWITH("Warning: already started");
            return 2;
        }

        Game::hd = hd;

        delete window;
        window = new Window(hd, opt);
        if(!window->isNormal()) {
            delete window;
            LOGWITH("Window creation failed");
            Window::terminate();
            return 1;
        }

#if !YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
        std::thread gamethread([]() {
#endif
            window->setMainThread();
            vk = new YRGraphics();
            vk->addWindow(0, window);
            if (!init()) {
                delete window;
                Window::terminate();
                return 1;
            }
            loopFlag = 1;
            if (onInit) onInit();
#ifdef EMSCRIPTEN
            window->waitEvents();
            Audio::init(false);
            emscripten_set_main_loop(mainLoop, 0, 0);
#else            
            for (;loopFlag; _frame++) {
                mainLoop();
            }
#endif

            return 0;
#if !YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
            });
        while(!window->windowShouldClose()) { window->waitEvents(); }
        gamethread.join();
#endif

        finalize();
        return 0;
    }

    void Game::mainLoop(){
        Input::startFrame();
        pollEvents();
        if (window->windowShouldClose()) {
#ifdef EMSCRIPTEN
            emscripten_cancel_main_loop();
#else
            loopFlag = 0;
#endif
        }
        std::chrono::duration<uint64_t, std::nano> longDt = std::chrono::steady_clock::now() - longTp;
        // [0, 2^52 - 1] 범위 정수를 균등 간격의 [1.0, 2.0) 범위 double로 대응시키는 함수에 십억을 적용한 후 1을 뺀 결과에 곱하여 1로 만들 수 있는 수
        constexpr double ONE_SECOND = 1.0 / 0.0000002220446049250313080847263336181640625;

        uint64_t ndt = longDt.count() - _tp;
        double ddt = (fixedPointConversion64i(ndt) - 1.0) * ONE_SECOND; // 함수를 사용할 수 없는 ndt의 상한선: 4,503,599,627,370,496 > 52일
        double iddt = 1.0 / ddt;
        _tp = longDt.count();
        _dt = static_cast<float>(ddt);
        _idt = static_cast<float>(iddt);
        YRGraphics::handle();
        if (perFrameProc) perFrameProc();
    }

    void Game::pollEvents(){
#if YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
        window->pollEvents();
#else
        eventQMutex.lock();
        tempQ.swap(eventQ);
        eventQMutex.unlock();
        int sx = -1, sy = -1;
        double cx = -1, cy = -1;
        for(EV& ev: tempQ) {
            switch(ev.sType){
            case WindowEvent::WE_SIZE:
                sx = ev.sizeX;
                sy = ev.sizeY;
                break;
            case WindowEvent::WE_KEYBOARD:
                Input::keyboard(ev.key, ev.scancode, ev.action, ev.mods);
                break;
            case WindowEvent::WE_CLICK:
                Input::click(ev.mouseKey, ev.mouseAction, ev.mouseMods);
                break;
            case WindowEvent::WE_CURSOR:
                cx = ev.mouseX;
                cy = ev.mouseY;
                break;
            case WindowEvent::WE_SCROLL:
                // TODO. ev.scrollX, ev.scrollY
                break;
            }
        }
        tempQ.clear();
        if(sx != -1) {
            windowResized(sx, sy);
        }
        if(cx != -1) {
            Input::moveCursor(cx, cy);
        }
#endif
    }

    void Game::finalize(){
#ifndef EMSCRIPTEN
        Audio::finalize();
#endif
        delete vk;
        delete window; // Window보다 스왑체인을 먼저 없애야 함 (안 그러면 X11에서 막혀서 프로그램이 안 끝남)
        window = nullptr;
        vk = nullptr;
        Window::terminate();
    }

    bool Game::init() {
#ifndef EMSCRIPTEN
        Audio::init(); // 임시조치
#endif
#if YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
        window->clickCallback = Input::click;
        window->keyCallback = Input::keyboard;
        window->posCallback = Input::moveCursor;
        window->touchCallback = Input::touch;
        window->windowSizeCallback = windowResized;
#else
        window->clickCallback = recordClickEvent;
        window->keyCallback = recordKeyEvent;
        window->posCallback = recordCursorEvent;
        window->windowSizeCallback = recordSizeEvent;
        window->scrollCallback = recordScrollEvent;
#endif
        return true;
    }

    void Game::windowResized(int x, int y){
#if BOOST_PLAT_ANDROID
        vk->resetWindow(0, true);
#else
        vk->resetWindow(0);
#endif
    }

    void Game::exit(){
        window->close();
    }

    void Game::readFile(const char* fileName, std::basic_string<uint8_t>* buffer){
        buffer->clear();
#if BOOST_PLAT_ANDROID
        AAsset* asset = AAssetManager_open(reinterpret_cast<android_app*>(hd)->activity->assetManager, fileName, AASSET_MODE_BUFFER);
        if(asset) {
            int len = AAsset_getLength(asset);
            buffer->resize(len);
            AAsset_read(asset, buffer->data(), len);
            AAsset_close(asset);
        }
#else
        FILE* fp = fopen(fileName, "rb");
        if(fp) {
            fseek(fp, 0, SEEK_END);
            int len = ftell(fp);
            buffer->resize(len);
            fseek(fp, 0, SEEK_SET);
            fread(buffer->data(), 1, len, fp);
            fclose(fp);
        }
#endif
    }

    void Game::setUpdate(std::function<void()> proc) {
        perFrameProc = proc;
    }

    void Game::setInit(std::function<void()> proc) {
        onInit = proc;
    }
}