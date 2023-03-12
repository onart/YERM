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
    int Input::pressedKey[512];
    int Input::pressedMouseKey[8];
    dvec2 Input::mousePos;
    Input::TouchInfo Input::_touches[4]{};
    const decltype(Input::_touches)& Input::touches(_touches);
    const dvec2& Input::mousePosition(mousePos);

    bool Input::isKeyDown(KeyCode key){
        return pressedKey[(int)key] > 0;
    }

    bool Input::isKeyDownNow(KeyCode key){
        return pressedKey[(int)key] == Game::frame;
    }

    bool Input::isKeyUpNow(KeyCode key){
        return pressedKey[(int)key] == -Game::frame;
    }

    bool Input::isKeyDown(MouseKeyCode key){
        return pressedMouseKey[(int)key] > 0;
    }

    bool Input::isKeyDownNow(MouseKeyCode key){
        return pressedMouseKey[(int)key] == Game::frame;
    }

    bool Input::isKeyUpNow(MouseKeyCode key){
        return pressedMouseKey[(int)key] == -Game::frame;
    }

    void Input::keyboard(int keycode, int scancode, int action, int mod){
        if(action == KEY_DOWN) pressedKey[keycode] = Game::frame;
        else if(action == KEY_UP) pressedKey[keycode] = -Game::frame;
        //LOGWITH(keycode, action);
    }

    void Input::click(int key, int action, int mods){
        //LOGWITH(key, action);
        if(action == KEY_DOWN) pressedMouseKey[key] = Game::frame;
        else if(action == KEY_UP) pressedMouseKey[key] = -Game::frame;
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