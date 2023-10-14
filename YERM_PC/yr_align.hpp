#ifndef __YR_ALIGN_HPP__
#define __YR_ALIGN_HPP__

#include "yr_compiler_specific.hpp"
namespace onart
{
    class alignas(16) align16 {
        private:
            inline static bool isNotAligned(void* p){return ((size_t)p) & 0xf;}
        public:
            inline static void* operator new(size_t size){ return aligned_malloc(16, size); }
            inline static void* operator new[](size_t size){ return aligned_malloc(16, size); }
            inline static void operator delete(void* p){ aligned_free(p); }
            inline static void operator delete[](void* p){ aligned_free(p); }
            inline align16(){
#ifdef ALIGN_MEMBER_CHECK
                if(isNotAligned(this)){
                    assert(false && "the class including this type of object does not seem to have alignment of 16.");
                }
#endif
            }
    };
} // namespace onart


#endif