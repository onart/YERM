cmake_minimum_required(VERSION 3.10)
set (CMAKE_CXX_STANDARD 17)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;")

if(ANDROID)
    message(STATUS "Build for Android platform is currently up to Android Studio. Please open YERM_android project with it.")
else()
    project("yerm")
    add_executable(yerm
        YERM_PC/main.cpp
        YERM_PC/logger.hpp
    )
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    target_include_directories(yerm PUBLIC externals)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()
    
endif()
