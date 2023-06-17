cmake_minimum_required(VERSION 3.13)
set (CMAKE_CXX_STANDARD 17)

set(CMAKE_CONFIGURATION_TYPES "Debug;Release;")

if (ANDROID)
    message(STATUS "Build for Android platform is currently up to Android Studio. Please open YERM_android project with it.")
elseif (EMSCRIPTEN)
    macro(append_compiler_flags FLAGS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS}")
    endmacro()
    
    macro(append_compiler_flags_debug FLAGS)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${FLAGS}")
    endmacro()

    macro(append_linker_flags FLAGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${FLAGS}")
    endmacro()

    macro(append_linker_flags_debug FLAGS)
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${FLAGS}")
    endmacro()

    macro(append_linker_flags_opts FLAGS)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} ${FLAGS}")
    endmacro()

    macro(append_linker_flags_minsizerel FLAGS)
    set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} ${FLAGS}")
    endmacro()
    
    message(STATUS "Building for WebGPU..")
    project("yerm")
    add_compile_definitions(YR_USE_WEBGPU)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
	append_compiler_flags("-Wall")
	append_compiler_flags_debug("-g3")

	append_linker_flags("-Wall")
	append_linker_flags_debug("-g3")
	append_linker_flags_debug("-sASSERTIONS=1 -jsDWEBGPU_DEBUG=1")

	append_linker_flags("--js-library ${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/lib_webgpu.js")
    append_linker_flags("--js-library ${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/lib_demo.js")
    append_linker_flags("--js-library ${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/library_miniprintf.js")
    append_linker_flags("--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/shell.html\"")
	append_linker_flags("-sMINIMAL_RUNTIME=2 -sTEXTDECODER=2 -sABORTING_MALLOC=0 -sSUPPORT_ERRNO=0")
	append_linker_flags("-sENVIRONMENT=web -sMIN_FIREFOX_VERSION=87 -sMIN_SAFARI_VERSION=140000 -sMIN_EDGE_VERSION=90 -sMIN_CHROME_VERSION=90")
	append_linker_flags("-sTOTAL_STACK=16KB -sINITIAL_MEMORY=128KB")
	append_linker_flags("-sALLOW_MEMORY_GROWTH=1")
	append_linker_flags("-sUSE_SDL=0 -sFILESYSTEM=0 -sAUTO_JS_LIBRARIES=0 -sDISABLE_EXCEPTION_THROWING=1")
	append_linker_flags("-lmath.js -lhtml5.js -lint53.js")
	append_linker_flags("--closure-args=--externs=externals/wasm_webgpu//webgpu-closure-externs.js")
    
	append_linker_flags("-sINITIAL_MEMORY=16MB -sMALLOC=emmalloc --memory-init-file 0")

	append_linker_flags("-sMAXIMUM_MEMORY=4GB")

	# Manual debugging:
	#append_linker_flags("-g2 -sSINGLE_FILE=1")

	append_linker_flags_opts("--closure 1")

	append_linker_flags_minsizerel("-Oz")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY externals/wasm_webgpu)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_library(webgpu STATIC externals/wasm_webgpu/lib_webgpu.cpp externals/wasm_webgpu/lib_webgpu_cpp11.cpp)

    add_executable(yerm
        YERM_webgpu/hello_triangle_minimal.c
    )
    target_include_directories(yerm PUBLIC externals)
    target_link_libraries(yerm PUBLIC webgpu)
else()
    message(STATUS "Build system: ${CMAKE_GENERATOR}")
    add_subdirectory(externals/glfw externals/)
    project("yerm")

    option(YERM_VULKAN "Use Vulkan as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" ON)
    option(YERM_OPENGL "Use OpenGL 4.0 as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    option(YERM_OPENGLES "Use OpenGL ES 3.0 as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    if(CMAKE_SYSTEM_NAME MATCHES "Windows")
        option(YERM_D3D12 "Use Direct3D 12 as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
        option(YERM_D3D11 "Use Direct3D 11 as the graphics API. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    endif()
    if(${YERM_VULKAN})
        set(YERM_GRAPHICS YERM_PC/yr_vulkan.h YERM_PC/yr_vulkan.cpp)
        add_compile_definitions(YR_USE_VULKAN)
    elseif(${YERM_OPENGL})
        set(YERM_GRAPHICS YERM_PC/yr_opengl.h YERM_PC/yr_opengl.cpp externals/glad/glad.c)
        add_compile_definitions(YR_USE_OPENGL)
    elseif(${YERM_D3D12})
        set(YERM_GRAPHICS YERM_PC/yr_d3d12.h YERM_PC/yr_d3d12.cpp)
        add_compile_definitions(YR_USE_D3D12)
    elseif(${YERM_D3D11})
        set(YERM_GRAPHICS YERM_PC/yr_d3d11.h YERM_PC/yr_d3d11.cpp)
        add_compile_definitions(YR_USE_D3D11)
    elseif(${YERM_OPENGLES})
        set(YERM_GRAPHICS YERM_PC/yr_opengles.h YERM_PC/yr_opengles.cpp externals/glad.c)
        add_compile_definitions(YR_USE_GLES)
    else()
        set(YERM_GRAPHICS YERM_PC/yr_vulkan.h YERM_PC/yr_vulkan.cpp)
        add_compile_definitions(YR_USE_VULKAN)
    endif()

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        add_compile_options("-Wno-nullability-completeness")
    endif()
    add_executable(yerm
        YERM_PC/main.cpp
        YERM_PC/logger.hpp
        YERM_PC/yr_simd.hpp
        YERM_PC/yr_math.hpp
        YERM_PC/yr_string.hpp
        YERM_PC/yr_pool.hpp
        YERM_PC/yr_tuple.hpp
        YERM_PC/yr_constants.hpp
        YERM_PC/yr_compiler_specific.hpp
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
        YERM_PC/yr_model.h
        YERM_PC/yr_model.cpp
    )
    add_dependencies(yerm glfw)
    set_target_properties(yerm PROPERTIES BUILD_RPATH ".")
    target_include_directories(yerm PUBLIC externals)
    target_link_directories(yerm PUBLIC externals/vulkan externals/ktx externals/shaderc)
    if(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
        add_compile_options("/utf-8")
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT yerm) # 재생버튼 눌러서 바로 실행해볼 수 있게 됨
        target_link_libraries(yerm glfw vulkan-1 ktx)
    else()
        target_link_libraries(yerm glfw vulkan ktx dl m pthread X11) # TODO: X11 부분을 타겟에 잘 맞게 분류
    endif()
    
endif()
