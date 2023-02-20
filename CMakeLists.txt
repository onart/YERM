cmake_minimum_required(VERSION 3.13)
set (CMAKE_CXX_STANDARD 17)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;")

if(ANDROID)
    message(STATUS "Build for Android platform is currently up to Android Studio. Please open YERM_android project with it.")
else()
    message(STATUS "Build system: ${CMAKE_GENERATOR}")
    project("yerm")

    option(YERM_VULKAN "Use Vulkan as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" ON)
    option(YERM_OPENGL "Use OpenGL as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    option(YERM_OPENGLES "Use OpenGL ES as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    if(CMAKE_SYSTEM_NAME MATCHES "Windows")
        option(YERM_D3D12 "Use Direct3D 12 as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
        option(YERM_D3D11 "Use Direct3D 11 as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    endif()
    if(${YERM_VULKAN})
        set(YERM_GRAPHICS YERM_PC/yr_vulkan.h YERM_PC/yr_vulkan.cpp)
    elseif(${YERM_OPENGL})
        set(YERM_GRAPHICS YERM_PC/yr_opengl.h YERM_PC/yr_opengl.cpp)
    elseif(${YERM_D3D12})
        set(YERM_GRAPHICS YERM_PC/yr_d3d12.h YERM_PC/yr_d3d12.cpp)
    elseif(${YERM_D3D11})
        set(YERM_GRAPHICS YERM_PC/yr_d3d11.h YERM_PC/yr_d3d11.cpp)
    elseif(${YERM_OPENGLES})
        set(YERM_GRAPHICS YERM_PC/yr_opengles.h YERM_PC/yr_opengles.cpp)
    else()
        set(YERM_GRAPHICS YERM_PC/yr_vulkan.h YERM_PC/yr_vulkan.cpp)
    endif()

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
        YERM_PC/yr_constants.hpp
        YERM_PC/yr_bits.hpp
        YERM_PC/yr_threadpool.hpp
        YERM_PC/yr_graphics.h

        ${YERM_GRAPHICS}
        YERM_PC/yr_sys.h
        YERM_PC/yr_sys.cpp
        YERM_PC/yr_game.h
        YERM_PC/yr_game.cpp
        YERM_PC/yr_input.h
        YERM_PC/yr_input.cpp
        YERM_PC/yr_singleheader.cpp
        YERM_PC/yr_audio.h
        YERM_PC/yr_audio.cpp
        YERM_PC/yr_constants.cpp
    )
    set_target_properties(yerm PROPERTIES BUILD_RPATH ".")
    target_include_directories(yerm PUBLIC externals)
    target_link_directories(yerm PUBLIC externals/glfw externals/vulkan externals/ktx externals/shaderc)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        add_compile_options("/utf-8")
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT yerm) # 재생버튼 눌러서 바로 실행해볼 수 있게 됨
        target_link_libraries(yerm glfw3_mt vulkan-1 ktx)
    else()
        target_link_libraries(yerm glfw3 vulkan ktx dl m pthread X11) # TODO: X11 부분을 타겟에 잘 맞게 분류
    endif()
    
endif()
