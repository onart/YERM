#include "logger.hpp"
#include "yr_math.hpp"
#include <filesystem>

using namespace onart;

int main(int argc, char* argv[]){
    dvec3 a{1.0, 2.0,3.0, 4.0};
    LOGWITH(a.zzxz());
}