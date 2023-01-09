cmake_minimum_required(VERSION 3.13)
set (CMAKE_CXX_STANDARD 17)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;")

if(ANDROID)
    message(STATUS "Build for Android platform is currently up to Android Studio. Please open YERM_android project with it.")
else()
    message(STATUS "Build system: ${CMAKE_GENERATOR}")
    project("yerm")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_executable(yerm
        YERM_PC/main.cpp
        YERM_PC/logger.hpp
        YERM_PC/yr_simd.hpp
        YERM_PC/yr_math.hpp
        YERM_PC/yr_string.hpp
        YERM_PC/yr_pool.hpp
        YERM_PC/yr_tuple.hpp
        YERM_PC/yr_sys.h
        YERM_PC/yr_sys.cpp
        YERM_PC/yr_game.h
        YERM_PC/yr_game.cpp
        YERM_PC/yr_vulkan.h
        YERM_PC/yr_vulkan.cpp
        YERM_PC/yr_input.h
        YERM_PC/yr_input.cpp
        YERM_PC/yr_singleheader.cpp
        YERM_PC/yr_audio.h
        YERM_PC/yr_audio.cpp
    )
    set_target_properties(yerm PROPERTIES BUILD_RPATH ".")
    target_include_directories(yerm PUBLIC externals)
    target_link_directories(yerm PUBLIC externals/glfw externals/vulkan externals/ktx externals/shaderc)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        add_compile_options("/utf-8")
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT yerm) # 재생버튼 눌러서 바로 실행해볼 수 있게 됨
        target_link_libraries(yerm glfw3_mt vulkan-1 ktx shaderc_shared)
    else()
        target_link_libraries(yerm glfw3 vulkan shaderc_shared ktx dl m pthread X11) # TODO: X11 부분을 타겟에 잘 맞게 분류
    endif()
    
endif()
