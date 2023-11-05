#ifndef __YR_COMPILER_SPECIFIC_HPP__
#define __YR_COMPILER_SPECIFIC_HPP__
#include "../externals/boost/predef/compiler.h"

// aligned alloc
#include <cstdlib>

namespace onart {
#ifdef _MSC_VER
    inline void* aligned_malloc(size_t alignment, size_t size) { return _aligned_malloc(size, alignment); }
    constexpr auto aligned_free = _aligned_free;
#else
    constexpr auto aligned_malloc = aligned_alloc;
    constexpr auto aligned_free = std::free;
#endif
}

#endif