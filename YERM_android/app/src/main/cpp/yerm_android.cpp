#include <game-activity/native_app_glue/android_native_app_glue.h>

#include <game-activity/GameActivity.cpp>
#include <game-text-input/gametextinput.cpp>
extern "C" {
#include <game-activity/native_app_glue/android_native_app_glue.c>
}

extern "C" {
void android_main(struct android_app* state);
};
#include "YERM_PC/logger.hpp"
#include "YERM_PC/yr_sys.h"
#include "YERM_PC/yr_game.h"

void android_main(struct android_app* app) {
    onart::Game::start(app);
}