cmake_minimum_required(VERSION 3.13)

project("yerm")

if (ANDROID)
    message(STATUS "Build for Android platform is currently up to Android Studio. Please open YERM_android project with it.")
elseif (EMSCRIPTEN)
    # Compiler/Tools
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;")
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

    # Build Settings
    option(YERM_BUILD_WEBGPU "Build WebGPU based graphics." OFF)
    option(YERM_BUILD_WEBGL "Build WebGL based graphics." ON)
    option(YERM_LINK_WEBGPU "Link WebGPU." OFF)
    option(YERM_LINK_WEBGL "Link WebGL." ON)

    set(YERM_BUILD_WEBGPU OFF)
    set(YERM_LINK_WEBGPU OFF)
    set(YERM_LINK_WEBGL ON)

    set(YERM_WEBGPU_SOURCE externals/wasm_webgpu/lib_webgpu.cpp externals/wasm_webgpu/lib_webgpu_cpp11.cpp YERM_PC/yr_singleheader.cpp)
    set(YERM_WEBGL_SOURCE yerm_web/yr_webgl.h yerm_web/yr_webgl.cpp YERM_PC/yr_singleheader.cpp)

    if(${YERM_BUILD_WEBGPU} OR ${YERM_LINK_WEBGPU})
        message(STATUS "Building for WebGPU..")
        set(YERM_BUILD_WEBGPU ON)
        add_library(YRGraphics_webgpu STATIC ${YERM_WEBGPU_SOURCE})
        target_include_directories(YRGraphics_webgpu PUBLIC externals)
    endif()
    if(${YERM_BUILD_WEBGL} OR ${YERM_LINK_WEBGL})
        message(STATUS "Building for WebGL..")
        set(YERM_BUILD_WEBGL ON)
        add_library(YRGraphics_webgl STATIC ${YERM_WEBGL_SOURCE})
        target_include_directories(YRGraphics_webgl PUBLIC externals)
    elseif(NOT ${YERM_BUILD_WEBGPU})
        message(STATUS "No web graphics base configured to be built. Building for WebGL")
        set(YERM_BUILD_WEBGL ON)
        add_library(YRGraphics_webgl STATIC ${${YERM_WEBGL_SOURCE}})
        target_include_directories(YRGraphics_webgl PUBLIC externals)
    endif()

    if(${YERM_LINK_WEBGPU})
        add_compile_definitions(YR_USE_WEBGPU)
        set(YERM_GRAPHICS YRGraphics_webgpu)
    elseif(${YERM_LINK_WEBGL})
        add_compile_definitions(YR_USE_WEBGL)
        set(YERM_GRAPHICS YRGraphics_webgl)
    endif()

    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    
    # Flags for Emscripten
	append_compiler_flags("-Wall -Wno-deprecated-declarations -Wno-nullability-completeness -Wno-unused-variable -fno-exceptions")
	append_compiler_flags_debug("-g3")

	append_linker_flags("-Wall")
	append_linker_flags_debug("-g3")

    if(${YERM_BUILD_WEBGPU})
        append_linker_flags_debug("-sASSERTIONS=1 -jsDWEBGPU_DEBUG=1")
	    append_linker_flags("--js-library ${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/lib_webgpu.js")
        append_linker_flags("--js-library ${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/lib_demo.js")
        append_linker_flags("--js-library ${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/library_miniprintf.js")
        append_linker_flags("--shell-file \"${CMAKE_CURRENT_SOURCE_DIR}/externals/wasm_webgpu/shell.html\"")
	    append_linker_flags("--closure-args=--externs=externals/wasm_webgpu//webgpu-closure-externs.js")
    endif()
    if(${YERM_BUILD_WEBGL})
        append_linker_flags("-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 --bind -sUSE_GLFW=3 -sWASM=1")
    endif()
    
	append_linker_flags("-sINITIAL_MEMORY=16MB -sMALLOC=emmalloc ")
    append_linker_flags("-sTEXTDECODER=2 -sABORTING_MALLOC=0 -sSUPPORT_ERRNO=0")
	append_linker_flags("-sENVIRONMENT=web -sMIN_FIREFOX_VERSION=87 -sMIN_SAFARI_VERSION=140000 -sMIN_CHROME_VERSION=90")
	append_linker_flags("-sTOTAL_STACK=16KB")
	append_linker_flags("-sALLOW_MEMORY_GROWTH=1")
	append_linker_flags("-sUSE_SDL=0 -sFILESYSTEM=0 -sAUTO_JS_LIBRARIES=0 -sDISABLE_EXCEPTION_THROWING=1")
    append_linker_flags("-lmath.js -lhtml5.js -lint53.js")

	append_linker_flags("-sMAXIMUM_MEMORY=4GB")

	# Manual debugging:
	#append_linker_flags("-g2 -sSINGLE_FILE=1")

	append_linker_flags_opts("--closure 1")

	append_linker_flags_minsizerel("-Oz")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY externals/wasm_webgpu)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

    add_executable(yerm
        YERM_web/wmain.cpp
        YERM_PC/yr_graphics.h
        YERM_PC/yr_sys.h
        YERM_PC/yr_sys.cpp
        YERM_PC/yr_game.h
        YERM_PC/yr_game.cpp
        YERM_PC/yr_audio.h
        YERM_PC/yr_audio.cpp
        YERM_PC/yr_constants.cpp
        YERM_PC/yr_input.h
        YERM_PC/yr_input.cpp
    )
    
    # Additional library
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    add_subdirectory(externals/ktx)
    target_link_directories(yerm PUBLIC externals/ktx)
    target_include_directories(yerm PUBLIC externals)
    target_link_libraries(yerm PUBLIC ${YERM_GRAPHICS} ktx glfw ${OPENGL_LIBRARIES})
else()

function(get_all_targets var dir)
    set(targets)
    get_all_targets_recursive(targets ${dir})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()

macro(get_all_targets_recursive targets dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()

macro(add_build_slib libname)
    target_include_directories(${libname} PUBLIC externals)
    set_target_properties(${libname} PROPERTIES FOLDER "YRGraphics")
endmacro()

    set (CMAKE_CXX_STANDARD 17)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;")
    message(STATUS "Build system: ${CMAKE_GENERATOR}")
    add_subdirectory(externals/glfw)
    add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    add_subdirectory(externals/ktx)

    get_all_targets(ktx_targets externals/ktx)
    foreach(ktx_target ${ktx_targets})
        set_target_properties(${ktx_target} PROPERTIES FOLDER "LIBKTX")
    endforeach()

    if(NOT CMAKE_VERSION VERSION_LESS "3.26")
        add_compile_definitions(_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)
    endif()

    option(YERM_BUILD_VULKAN "Build Vulkan based graphics." ON)
    option(YERM_BUILD_OPENGL "Build OpenGL 4.0 based graphics." ON)
    option(YERM_BUILD_OPENGLES "Build OpenGL ES 3.0 based graphics." OFF)

    option(YERM_LINK_VULKAN "Link with Vulkan based graphics. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" ON)
    option(YERM_LINK_OPENGL "Link with OpenGL 4.0 based graphics. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    option(YERM_LINK_OPENGLES "Build OpenGL ES 3.0 based graphics. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    if(CMAKE_SYSTEM_NAME MATCHES "Windows")
        option(YERM_BUILD_D3D12 "Build Direct3D 12 based graphics." OFF)
        option(YERM_BUILD_D3D11 "Build Direct3D 11 based graphics." ON)
        option(YERM_LINK_D3D11 "Link with Direct3D 11 based graphics. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
        option(YERM_LINK_D3D12 "Link with Direct3D 12 based graphics. If multiple or no API selected, the priority will be vulkan > opengl > direct3d 12 > direct3d 11 > opengl es" OFF)
    endif()

    if(${YERM_BUILD_VULKAN} OR ${YERM_LINK_VULKAN})
        add_library(YRGraphics_vk STATIC YERM_PC/yr_vulkan.h YERM_PC/yr_vulkan.cpp)
        set(YERM_BUILD_VULKAN ON)
        add_build_slib(YRGraphics_vk)
    endif()
    if(${YERM_BUILD_OPENGL} OR ${YERM_LINK_OPENGL})
        add_library(YRGraphics_gl STATIC YERM_PC/yr_opengl.cpp YERM_PC/yr_opengl.h externals/glad/glad.c)
        set(YERM_BUILD_OPENGL ON)
        add_build_slib(YRGraphics_gl)
    endif()
    if(${YERM_BUILD_D3D12} OR ${YERM_LINK_D3D12})
        add_library(YRGraphics_d3d12 STATIC YERM_PC/yr_d3d12.cpp YERM_PC/yr_d3d12.h)
        set(YERM_BUILD_D3D12 ON)
        add_build_slib(YRGraphics_d3d12)
    endif()
    if(${YERM_BUILD_D3D11} OR ${YERM_LINK_D3D11})
        add_library(YRGraphics_d3d11 STATIC YERM_PC/yr_d3d11.cpp YERM_PC/yr_d3d11.h)
        set(YERM_BUILD_D3D11 ON)
        add_build_slib(YRGraphics_d3d11)
    endif()
    if(${YERM_BUILD_OPENGLES} OR ${YERM_LINK_OPENGLES})
        add_library(YRGraphics_gles YERM_PC/yr_opengles.h YERM_PC/yr_opengles.cpp externals/glad/glad.c)
        set(YERM_BUILD_OPENGLES ON)
        add_build_slib(YRGraphics_gles)
    endif()

    if(${YERM_LINK_VULKAN})
        add_compile_definitions(YR_USE_VULKAN)
        set(YERM_GRAPHICS_LIB YRGraphics_vk)
    elseif(${YERM_LINK_OPENGL})
        add_compile_definitions(YR_USE_OPENGL)
        set(YERM_GRAPHICS_LIB YRGraphics_gl)
    elseif(${YERM_LINK_D3D12})
        add_compile_definitions(YR_USE_D3D12)
        set(YERM_GRAPHICS_LIB YRGraphics_d3d12)
    elseif(${YERM_LINK_D3D11})
        add_compile_definitions(YR_USE_D3D11)
        set(YERM_GRAPHICS_LIB YRGraphics_d3d11)
    elseif(${YERM_LINK_OPENGLES})
        add_compile_definitions(YR_USE_GLES)
        set(YERM_GRAPHICS_LIB YRGraphics_gles)
    else()
        add_compile_definitions(YR_USE_VULKAN)
        set(YERM_GRAPHICS_LIB YRGraphics_vk)
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
        YERM_PC/yr_align.hpp
        YERM_PC/yr_bits.hpp
        YERM_PC/yr_threadpool.hpp
        YERM_PC/yr_graphics.h

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
        target_link_libraries(yerm glfw vulkan-1 ktx ${YERM_GRAPHICS_LIB})
    else()
        target_link_libraries(yerm glfw vulkan ktx dl m pthread X11 ${YERM_GRAPHICS_LIB}) # TODO: X11 부분을 타겟에 잘 맞게 분류
    endif()
    
endif()
