#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1000000
#define MA_IMPLEMENTATION
#define MA_NO_ENCODING


struct __imgspace{
    unsigned char* buf = nullptr;
    unsigned char* pool[9]{};
    inline unsigned char* alloc(unsigned long long s){
        if(!buf) {
            buf = new unsigned char[1024*1024 + 4096*4096*4]; // 1M + 64M
            for(int i=0;i<9;i++){
                pool[i] = buf + 131072 * i;
            }
        }
        if(s <= 131072) {
            for(int i=0;i<8;i++){
                if(pool[i]) {
                    auto ret = pool[i];
                    pool[i]=nullptr;
                    return ret;
                }
            }
            return nullptr;
        }
        auto ret = pool[8];
        pool[8] = nullptr;
        return ret;
    }

    inline unsigned char* realloc(void* p, unsigned long long s){
        if(p == nullptr) return alloc(s);
        if(!buf) {
            buf = new unsigned char[1024*1024 + 4096*4096*4]; // 1M + 64M
            for(int i=0;i<9;i++){
                pool[i] = buf + 131072 * i;
            }
        }
        if(s <= 131072) {
            return (unsigned char*)p;
        }
        auto ret = pool[8];
        pool[8] = nullptr;
        return ret;
    }

    inline void free(void* p){
        int i = ((unsigned char*)p - buf) / 131072;
        pool[i] = (unsigned char*)p;
    }
    ~__imgspace(){ delete buf; }
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