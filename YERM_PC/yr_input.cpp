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
        return pressedKey[(int)key] == Game::frameNumber();
    }

    bool Input::isKeyUpNow(KeyCode key){
        return pressedKey[(int)key] == -Game::frameNumber();
    }

    bool Input::isKeyDown(MouseKeyCode key){
        return pressedMouseKey[(int)key] > 0;
    }

    bool Input::isKeyDownNow(MouseKeyCode key){
        return pressedMouseKey[(int)key] == Game::frameNumber();
    }

    bool Input::isKeyUpNow(MouseKeyCode key){
        return pressedMouseKey[(int)key] == -Game::frameNumber();
    }

    void Input::keyboard(int keycode, int scancode, int action, int mod){
        if(action == KEY_DOWN) pressedKey[keycode] = Game::frameNumber();
        else if(action == KEY_UP) pressedKey[keycode] = -Game::frameNumber();
        LOGWITH(keycode, action);
    }

    void Input::click(int key, int action, int mods){
        LOGWITH(key, action);
        if(action == KEY_DOWN) pressedMouseKey[key] = Game::frameNumber();
        else if(action == KEY_UP) pressedMouseKey[key] = -Game::frameNumber();
    }

    void Input::moveCursor(double x, double y){
        LOGWITH(x, y);
        mousePos.x = x;
        mousePos.y = y;
    }

    void Input::touch(int id, int action, float x, float y){
        // TODO: 터치 ID가 항상 0부터 가능한 작은 값을 사용한다고 보장되어 있는가?를 확실히 알았으면 좋겠음
        static int64_t serial = 0;
        if(id > sizeof(_touches)/sizeof(_touches[0])) return;
        if(action == KEY_DOWN) {
            LOGWITH("DOWN",id,x,y);
            _touches[id].frame = Game::frameNumber();
            _touches[id].id = serial++;
            _touches[id].pos.x = x;
            _touches[id].pos.y = y;
        }
        else if(action == KEY_UP){
            LOGWITH("UP",id,x,y);
            _touches[id].frame = -Game::frameNumber();
            _touches[id].pos.x = x;
            _touches[id].pos.y = y;
        }
        else{ // move
            LOGWITH("MOVE",id,x,y);
            _touches[id].pos.x = x;
            _touches[id].pos.y = y;
        }
    }

    bool Input::TouchInfo::isPressedNow() const{
        return frame == Game::frameNumber();
    }

    bool Input::TouchInfo::isPressed() const{
        return frame > 0;
    }

    bool Input::TouchInfo::isUpNow() const{
        return frame == -Game::frameNumber();
    }
}