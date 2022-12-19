#include "logger.hpp"
#include "yr_math.hpp"
#include "yr_game.h"

#include <filesystem>

using namespace onart;

int main(int argc, char* argv[]){
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    Game game;
    game.start();
}