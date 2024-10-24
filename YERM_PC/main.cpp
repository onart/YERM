#include "logger.hpp"
#include "yr_math.hpp"
#include "yr_game.h"
#include "yr_graphics.h"
#include "yr_gameapp.h"

#include <filesystem>
#include <cstdlib>

using namespace onart;
int main(int argc, char* argv[]){
#if BOOST_OS_WINDOWS
    system("chcp 65001");
#endif
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    Game game;
    //game.setUpdate();
    game.start();
}