cmake_minimum_required(VERSION 3.10)
set (CMAKE_CXX_STANDARD 17)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;")

if(ANDROID)
    message(STATUS "Build for Android platform is currently up to Android Studio. Please open YERM_android project with it.")
else()
    message(STATUS "Build system: ${CMAKE_GENERATOR}")
    project("yerm")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_executable(yerm
        YERM_PC/main.cpp
        YERM_PC/logger.hpp
        YERM_PC/yr_simd.hpp
    )
    set_target_properties(yerm PROPERTIES BUILD_RPATH ".")
    target_include_directories(yerm PUBLIC externals)
    target_link_directories(yerm PUBLIC externals/glfw externals/vulkan externals/ktx externals/shaderc)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        target_link_libraries(yerm glfw3_mt vulkan-1 ktx shaderc_shared)
        add_custom_command(TARGET yerm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/externals/shaderc/libshaderc_shared.dll
                ${CMAKE_BINARY_DIR}/bin/libshaderc_shared.dll)
    else()
        target_link_libraries(yerm glfw3 vulkan shaderc_shared)
        add_custom_command(TARGET yerm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/externals/shaderc/libshaderc_shared.so
                ${CMAKE_BINARY_DIR}/bin/libshaderc_shared.so.1)
    endif()
    
endif()
