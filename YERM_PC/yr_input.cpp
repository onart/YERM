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
#include "yr_input.h"
#include "yr_game.h"

namespace onart{
    Input::press_t Input::pressedKey[512];
    Input::press_t Input::pressedMouseKey[8];
    dvec2 Input::mousePos;
    Input::TouchInfo Input::_touches[4]{};
    const decltype(Input::_touches)& Input::touches(_touches);
    const dvec2& Input::mousePosition(mousePos);
    Input::KeyInput Input::rfk[32];
    int Input::rfkCount = 0;

    bool Input::isKeyDown(KeyCode key){
        return pressedKey[(int)key].frame > 0;
    }

    bool Input::isKeyDownNow(KeyCode key){
        const press_t& keystate = pressedKey[(int)key];
        return keystate.frame == Game::frame || (keystate.frame == -Game::frame && keystate.count);
    }

    bool Input::isKeyUpNow(KeyCode key){
        const press_t& keystate = pressedKey[(int)key];
        return keystate.frame == -Game::frame || (keystate.frame == Game::frame && keystate.count);
    }

    bool Input::isKeyDown(MouseKeyCode key){
        return pressedMouseKey[(int)key].frame > 0;
    }

    bool Input::isKeyDownNow(MouseKeyCode key){
        const press_t& keystate = pressedMouseKey[(int)key];
        return keystate.frame == Game::frame || (keystate.frame == -Game::frame && keystate.count);
    }

    bool Input::isKeyUpNow(MouseKeyCode key){
        const press_t& keystate = pressedMouseKey[(int)key];
        return keystate.frame == -Game::frame || (keystate.frame == Game::frame && keystate.count);
    }

    void Input::keyboard(int keycode, int scancode, int action, int mod){
        press_t& keystate = pressedKey[keycode];
        if (action == KEY_DOWN) { 
            if (keystate.frame == -Game::frame) { keystate.count++; }
            else { keystate.count = 0; }
            keystate.frame = Game::frame;
        }
        else if (action == KEY_UP) { 
            if (keystate.frame == Game::frame) { keystate.count++; }
            else { keystate.count = 0; }
            keystate.frame = -Game::frame;
        }
        else {
            return;
        }
        if (rfkCount < sizeof(rfk) / sizeof(rfk[0])) {
            rfk[rfkCount].keyCode = (KeyCode)keycode;
            rfk[rfkCount].down = action == KEY_DOWN;
            rfkCount++;
        }
        //LOGWITH(keycode, action);
    }

    void Input::click(int key, int action, int mods){
        //LOGWITH(key, action);
        press_t& keystate = pressedMouseKey[key];
        if (action == KEY_DOWN) { 
            if (keystate.frame == -Game::frame) { keystate.count++; }
            else { keystate.count = 0; }
            keystate.frame = Game::frame;
        }
        else if (action == KEY_UP) { 
            if (keystate.frame == Game::frame) { keystate.count++; }
            else { keystate.count = 0; }
            keystate.frame = -Game::frame;
        }
    }

    void Input::startFrame() {
        rfkCount = 0;
    }

    void Input::moveCursor(double x, double y){
        mousePos.x = x;
        mousePos.y = y;
    }

    void Input::touch(int id, int action, float x, float y){
        // TODO: 터치 ID가 항상 0부터 가능한 작은 값을 사용한다고 보장되어 있는가?를 확실히 알았으면 좋겠음. 일단 지금은 그럼
        static int64_t serial = 0;
        if(id >= sizeof(_touches)/sizeof(_touches[0])) return;
        if(action == KEY_DOWN) {
            //LOGWITH("DOWN",id,x,y);
            _touches[id].frame = Game::frame;
            _touches[id].id = serial++;
            _touches[id].pos.x = x;
            _touches[id].pos.y = y;
        }
        else if(action == KEY_UP){
            //LOGWITH("UP",id,x,y);
            _touches[id].frame = -Game::frame;
            _touches[id].pos.x = x;
            _touches[id].pos.y = y;
        }
        else{ // move
            //LOGWITH("MOVE",id,x,y);
            _touches[id].pos.x = x;
            _touches[id].pos.y = y;
        }
    }

    const Input::KeyInput* Input::recentFrameKeyInputBegin() { return rfk; }
    
    const Input::KeyInput* Input::recentFrameKeyInputEnd() { return rfk + rfkCount; }

    bool Input::TouchInfo::isPressedNow() const{
        return frame == Game::frame;
    }

    bool Input::TouchInfo::isPressed() const{
        return frame > 0;
    }

    bool Input::TouchInfo::isUpNow() const{
        return frame == -Game::frame;
    }
}