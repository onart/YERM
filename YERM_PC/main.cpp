#include "logger.hpp"
#include <filesystem>

int main(int argc, char* argv[]){
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    LOGWITH(argv[0]);
}