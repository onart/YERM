
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.18.1)

# Declares and names the project.

project("yerm_android")
find_package(game-activity REQUIRED CONFIG)
find_package(games-controller REQUIRED CONFIG)
find_package(games-frame-pacing REQUIRED CONFIG)
find_package(games-performance-tuner REQUIRED CONFIG)
#find_package(oboe REQUIRED CONFIG)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

# ON: USE VULKAN, OFF: USE GL ES
set(YERM_VULKAN ON)
if(${YERM_VULKAN})
    set(YERM_GRAPHICS ../../../../../YERM_PC/yr_vulkan.h ../../../../../YERM_PC/yr_vulkan.cpp)
    add_compile_definitions(YR_USE_VULKAN)
else()
    set(YERM_GRAPHICS ../../../../../YERM_PC/yr_opengles.h ../../../../../YERM_PC/yr_opengles.cpp)
    add_compile_definitions(YR_USE_GLES)
endif()
add_compile_options("-Wno-nullability-completeness")
add_library( # Sets the name of the library.
             yerm_android

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             yerm_android.cpp
             ../../../../../YERM_PC/logger.hpp
             ../../../../../YERM_PC/yr_simd.hpp
             ../../../../../YERM_PC/yr_math.hpp
             ../../../../../YERM_PC/yr_string.hpp
             ../../../../../YERM_PC/yr_pool.hpp
             ../../../../../YERM_PC/yr_tuple.hpp
             ../../../../../YERM_PC/yr_bits.hpp
             ../../../../../YERM_PC/yr_constants.hpp
             ../../../../../YERM_PC/yr_compiler_specific.hpp
             ../../../../../YERM_PC/yr_threadpool.hpp
             ../../../../../YERM_PC/yr_graphics.h

             ../../../../../YERM_PC/yr_sys.h
             ../../../../../YERM_PC/yr_sys.cpp
             ../../../../../YERM_PC/yr_game.h
             ../../../../../YERM_PC/yr_game.cpp
             ${YERM_GRAPHICS}
             ../../../../../YERM_PC/yr_input.h
             ../../../../../YERM_PC/yr_input.cpp
             ../../../../../YERM_PC/yr_audio.h
             ../../../../../YERM_PC/yr_audio.cpp
             ../../../../../YERM_PC/yr_singleheader.cpp
             ../../../../../YERM_PC/yr_constants.cpp
             ../../../../../YERM_PC/yr_model.h
             ../../../../../YERM_PC/yr_model.cpp
        )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.
target_include_directories(yerm_android
        PUBLIC
        ../../../../..
        ../../../../../externals
        )

target_link_directories(yerm_android PUBLIC ../jniLibs/${ANDROID_ABI})

target_link_libraries( # Specifies the target library.
        yerm_android

        android
        vulkan
        game-activity::game-activity
        #games-frame-pacing::swappy_static
        #games-performance-tuner::tuningfork_static
        #oboe::oboe
        ktx
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib} )
