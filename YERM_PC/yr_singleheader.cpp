#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1000000
#define MA_IMPLEMENTATION
#define MA_NO_ENCODING

#include <cstdlib>
#include <cstring>

struct __imgspace{
    constexpr static unsigned long long BUFFER_UNIT = 1<<22; // 4M
    constexpr static unsigned long long BUFFER_COUNT = 4;
    constexpr static unsigned long long BUFFER_ALLOC = BUFFER_UNIT * BUFFER_COUNT + 4096 * 4096 * 4; // 16M + 64M
    unsigned char* buf = nullptr;
    unsigned char* pool[BUFFER_COUNT + 1]{};
    inline unsigned char* alloc(unsigned long long s){
        init();
        unsigned char* ret = nullptr;
        if(s <= BUFFER_UNIT) {
            for (int i = 0; i < BUFFER_COUNT; i++) {
                if(pool[i]) {
                    ret = pool[i];
                    pool[i] = nullptr;
                    break;
                }
            }
        }
        else {
            ret = pool[BUFFER_COUNT];
            pool[BUFFER_COUNT] = nullptr;
        }
        return ret ? ret : (unsigned char*)std::malloc(s);
    }

    inline unsigned char* realloc(void* p, unsigned long long s){
        if(p == nullptr) return alloc(s);
        if(!buf) {
            init();
        }
        if(!isFromPool(p)) {
            return (unsigned char*)std::realloc(p,s);
        }
        if(s <= BUFFER_UNIT) {
            return (unsigned char*)p;
        }
        this->free(p);
        auto ret = pool[BUFFER_COUNT];
        pool[BUFFER_COUNT] = nullptr;
        if (!ret) { ret = (unsigned char*)std::malloc(s); }
        std::memcpy(ret, p, BUFFER_UNIT);
        return ret;
    }

    inline void free(void* p){
        if(!isFromPool(p)){
            std::free(p);
            return;
        }
        int i = ((unsigned char*)p - buf) / BUFFER_UNIT;
        pool[i] = (unsigned char*)p;
    }
    ~__imgspace(){ delete[] buf; }
private:
    inline void init() {
        if (!buf) {
            buf = new unsigned char[BUFFER_ALLOC];
            for (int i = 0; i < BUFFER_COUNT + 1; i++) {
                pool[i] = buf + BUFFER_UNIT * i;
            }
        }
    }
    inline bool isFromPool(void* p){
        unsigned long long offset = (unsigned long long)((unsigned char*)p - buf);
        return offset <= BUFFER_ALLOC; // GIGO. 할당에서 머리로 돌려준 부분 주면 당연히 안 됨
    }
};
static thread_local __imgspace buffer;

static void* __yrmalloc(unsigned long long s){ return buffer.alloc(s); }

static void* __yrrealloc(void* p,unsigned long long s){ return buffer.realloc(p, s); }

static void __yrfree(void* p){ buffer.free(p); }

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC __yrmalloc
#define STBI_REALLOC __yrrealloc
#define STBI_FREE __yrfree

#include "../externals/single_header/stb_image.h"

#include "../externals/vulkan/vk_mem_alloc.h"
#include "../externals/single_header/miniaudio.h"
#include "../externals/single_header/stb_vorbis.c"