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
#include "logger.hpp"

#include "yr_math.hpp"

namespace onart{

    static Game* singleton = nullptr;
    static std::chrono::steady_clock clock;

    Game::Game():longTp(clock.now()), _dt(0), _idt(0), _tp(0), frame(1), vk(nullptr), window(nullptr){ }

    int Game::start(void* hd, Window::CreationOptions* opt){
        if(singleton) return 2;
        window = new Window(hd, opt);
        if(!window->isNormal()) {
            delete window;
            LOGWITH("Window creation failed");
            Window::terminate();
            return 1;
        }
        if(!init()){
            delete window;
            Window::terminate();
            return 1;
        }

        singleton = this;
        
        // set window callbacks here...

        for(frame = 1; !window->windowShouldClose(); frame++) {
            window->pollEvents();
            std::chrono::duration<float, std::ratio<1>> longDt = clock.now() - longTp;
            float temp = longDt.count();
            _dt = temp - _tp;
            _tp = temp;
            _idt = 1.0f / _dt;
        }

        finalize();
        singleton = nullptr;
        return 0;
    }

    int Game2::start(void* hd, Window::CreationOptions* opt){
        if(singleton) return 2;
        window = new Window(hd, opt);
        if(!window->isNormal()) {
            delete window;
            Window::terminate();
            return 1;
        }
        if(!init()){
            delete window;
            Window::terminate();
            return 1;
        }

        singleton = this;
        
        // set window callbacks here...

        for(frame = 1; !window->windowShouldClose(); frame++) {
            window->pollEvents();
            std::chrono::duration<float, std::ratio<1>> longDt = clock.now() - longTp;
            float temp = longDt.count();
            _dt = temp - _tp;
            _tp = temp;
            _idt = 1.0f / _dt;
        }

        finalize();
        singleton = nullptr;
        return 0;
    }

    void Game::finalize(){
        delete window;
        delete vk;
        Window::terminate();
    }

    bool Game::init() {
        vk = new VkMachine(window);
        window->clickCallback = Input::click;
        window->keyCallback = Input::keyboard;
        window->posCallback = Input::moveCursor;
        window->touchCallback = Input::touch;
        return true;
    }

    bool Game2::init(){
        return true;
    }

    float Game::tp(){ return singleton->_tp; }
    float Game::dt(){ return singleton->_dt; }
    float Game::idt(){ return singleton->_idt; }
    int32_t Game::frameNumber(){ return singleton->frame; }
    //static int 
}