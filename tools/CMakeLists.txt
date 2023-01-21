cmake_minimum_required(VERSION 3.10)
set (CMAKE_CXX_STANDARD 17)

set(CMAKE_CONFIGURATION_TYPES "Release;")

message(STATUS "Build system: ${CMAKE_GENERATOR}")
project("yerm_tools")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
endif()

#img2ktx
add_executable(img2ktx yrt_ktx.cpp)
target_include_directories(img2ktx PUBLIC ../externals)
target_link_libraries(img2ktx ktx)
target_link_directories(img2ktx PUBLIC ../externals/ktx)

#xxd
add_executable(yrtxxd yrt_xxd.cpp)