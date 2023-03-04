# YERM
Your Engine Recycling Mine


## Overview
이것은 C++, Vulkan 기반 게임 엔진 프로젝트입니다.
* 대상 플랫폼: x86 / ARM 계열 프로세서(x86, ARM 모두 64비트만), Win32, X11, Android

## Feature
* 예정

## API Documentation
* [Github Wiki](https://github.com/onart/YERM/wiki)
* Doxygen을 이용하여 개별 클래스/함수를 설명하는 문서를 생성할 수 있습니다.

## Building
* 이 프로젝트의 빌드를 위해서는 MSVC, make 같은 기본적인 생성 도구와 [CMake](https://cmake.org/download/)가 필요합니다.

* PC
```bash
mkdir build
cd build
cmake ..
```
SIMD 파트 때문에 컴파일러는 [Clang](https://github.com/llvm/llvm-project/releases/tag/llvmorg-15.0.6)을 가장 권장합니다. (MSVC에서 사용하는 경우 해당 도구 안에서 LLVM을 받으면 됩니다.)

* Android\
안드로이드 스튜디오 NDK, AGDK를 보유한 상태에서 그대로 열어 작업할 수 있습니다.


## Related Source
Source | Description
---- | ----
[Boost.Predef](https://www.boost.org/doc/libs/1_73_0/libs/predef/doc/index.html) | 빌드 타임에 플랫폼을 판별하기 위한 헤더입니다. [Boost Software License](https://www.boost.org/LICENSE_1_0.txt)
[ASIO](https://think-async.com/Asio/index.html) | 여러 플랫폼에서 BSD 소켓을 사용할 수 있고 비동기적 동작, 스레드 풀 등을 포함한 헤더입니다. Boost와의 종속성이 없는 버전입니다. [Boost Software License](https://www.boost.org/LICENSE_1_0.txt)
[GLFW](https://www.glfw.org/) | PC 대상 크로스 플랫폼 창 관리자입니다. [zlib/libpng license](https://www.glfw.org/license)
[libktx](https://github.com/KhronosGroup/KTX-Software) | KTX 텍스처 컨테이너 파일을 읽고 씁니다. 이 프로젝트에서는 Basis Universal 형태를 사용하기 위해 이 라이브러리를 사용합니다. 자체 라이센스는 아파치 2.0이지만 그 안에서 포함된 외부 소스는 다양한 라이센스를 가지고 있으므로 참고하시기 바랍니다. 다음 링크 내의 라이센스는 모두 아파치 2.0과 공존할 수 있는 것으로 확인됩니다. [license](https://github.com/KhronosGroup/KTX-Software/tree/master/LICENSES)
[miniaudio](https://github.com/mackron/miniaudio) | 헤더 하나로 구성된 크로스 플랫폼 음원 재생 모듈입니다. [Public domain / MIT license](https://github.com/mackron/miniaudio/blob/master/LICENSE)
[sse2neon.h](https://github.com/DLTcollab/sse2neon) | x86 SSE intrinsic 명령을 ARM Neon intrinsic 명령으로 해석해 줍니다. [MIT license](https://github.com/DLTcollab/sse2neon/blob/master/LICENSE)
[stb](https://github.com/nothings/stb) | 이미지 압축 형식을 읽고 쓰는 `stb_image.h`, 이미지 크기를 재설정하는 `stb_image_resize.h`, Ogg Vorbis 형식을 읽고 쓰는 `stb_vorbis.c`를 사용합니다. resize는 tools에서 사용합니다.
[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | Vulkan의 GPU 메모리 자원인 `VkImage`, `VkBuffer` 할당 수를 줄이고 개별 할당 크기를 늘립니다. [MIT license](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/LICENSE.txt)
[Vulkan SDK](https://vulkan.lunarg.com/) | Vulkan을 C/C++로 사용하기 위한 도구입니다. 사용되는 모든 부분은 [Apache 2.0 / MIT license](https://vulkan.lunarg.com/license/)를 따릅니다. (헤더 파일 참고)
[Android Studio](https://developer.android.com/studio/) | 이 프로젝트는 안드로이드 대상으로 빌드하려면 Android Studio(NDK, AGDK)를 사용하도록 되어 있습니다. 스튜디오의 라이센스 상으로는, 스튜디오 자체를 안드로이드 외의 플랫폼 대상의 개발을 위해 사용하지만 않으면 단순 사용은 대부분 가능합니다. [license](https://developer.android.com/studio/terms)
[tinygltf](https://github.com/syoyo/tinygltf) | GLTF 파일을 불러오는 도구입니다. [MIT license](https://github.com/syoyo/tinygltf/blob/release/LICENSE)
