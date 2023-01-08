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
#include "yr_game.h"
#include "yr_sys.h"
#include "yr_vulkan.h"
#include "yr_input.h"
#include "yr_audio.h"

#include "logger.hpp"
#include "yr_math.hpp"

#include "../externals/boost/predef/platform.h"
#include "../externals/boost/predef/compiler.h"

#if BOOST_PLAT_ANDROID
#include <game-activity/native_app_glue/android_native_app_glue.h>
#endif

namespace onart{

    const std::chrono::steady_clock::time_point Game::longTp = std::chrono::steady_clock::now();
    float Game::_dt = 0.016f, Game::_idt = 60.0f, Game::_tp = 0.0f;
    const float &Game::dt(Game::_dt), &Game::idt(Game::_idt), &Game::tp(Game::_tp);
    int32_t Game::_frame = 1;
    const int32_t& Game::frame(Game::_frame);
    VkMachine* Game::vk = nullptr;
    Window* Game::window = nullptr;
    void* Game::hd = nullptr;

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
        vk = new VkMachine(window);
        if(!init()){
            delete window;
            Window::terminate();
            return 1;
        }

        for(;; _frame++) {
            window->pollEvents();
            if(window->windowShouldClose()) break;
            std::chrono::duration<float, std::ratio<1>> longDt = std::chrono::steady_clock::now() - longTp;
            float temp = longDt.count();
            _dt = temp - _tp;
            _tp = temp;
            _idt = 1.0f / _dt;
        }

        finalize();
        return 0;
    }

    void Game::finalize(){
        Audio::finalize();
        delete window;
        delete vk;
        window = nullptr;
        vk = nullptr;
        Window::terminate();
    }

    bool Game::init() {
        Audio::init();
        window->clickCallback = Input::click;
        window->keyCallback = Input::keyboard;
        window->posCallback = Input::moveCursor;
        window->touchCallback = Input::touch;
        window->windowSizeCallback = windowResized;
        return true;
    }

    void Game::windowResized(int x, int y){
        vk->createSwapchain(x, y);
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
#elif BOOST_COMP_MSVC
        FILE* fp;
        fopen_s(&fp, fileName, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            int len = ftell(fp);
            buffer->resize(len);
            fseek(fp, 0, SEEK_SET);
            fread_s(buffer->data(), len, 1, len, fp);
            fclose(fp);
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
}