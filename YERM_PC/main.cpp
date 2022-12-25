#include "logger.hpp"
#include "yr_math.hpp"
#include "yr_game.h"

#include <filesystem>
#include <cstdlib>

using namespace onart;

int main(int argc, char* argv[]){
#if BOOST_OS_WINDOWS
    system("chcp 65001");
#endif
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    LOGWITH(u16string255(u"한글abcd"), string255(u8"한글abcd"), convert(string255(u8"한글abcd")));
    Game game;
    game.start();
}