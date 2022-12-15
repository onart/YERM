#include "logger.hpp"
#include "yr_math.hpp"
#include <filesystem>

int main(int argc, char* argv[]){
    onart::nvec<float, 3> v(2,3);
    onart::nvec<int, 3> w;
    w[0] = 1;
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    LOGWITH(v.normal(), v.length());
}