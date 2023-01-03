#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include "../externals/single_header/stb_image.h"
#include "../externals/single_header/stb_image_resize.h"
#define KHRONOS_STATIC
#include "../externals/ktx/ktx.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <cstdint>
#include <filesystem>

inline bool isPOT(int n){ return (n & (n-1)) == 0; }
inline int gtePOT(int v){
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

bool convert(const char* fileName, int mip, bool uastc = true) {
    if(mip <= 0) return false;
    uint8_t* mipPtrs[16]{};
    int x,y,ch;
    mipPtrs[0] = stbi_load(fileName, &x, &y, &ch, 0);
    if(!mipPtrs[0]) return false;
    const uint32_t BASE_SIZE = sizeof(uint8_t) * ch * gtePOT(x) * gtePOT(y);
    if(!isPOT(x) || !isPOT(y)){
        uint8_t* newpix = (uint8_t*)std::malloc(BASE_SIZE);
        stbir_resize_uint8(mipPtrs[0], x, y, 0, newpix, gtePOT(x), gtePOT(y), 0, ch);
        x = gtePOT(x);
        y = gtePOT(y);
        stbi_image_free(mipPtrs[0]);
        mipPtrs[0] = newpix;
    }
    for(int i=1;i < mip;i++){
        if((x>>i) == 0 || (y>>i) == 0) break;
        mipPtrs[i] = (uint8_t*)std::malloc(BASE_SIZE >> (i*2));
        stbir_resize_uint8(mipPtrs[0], x, y, 0, mipPtrs[i], x>>i, y>>i, 0, ch);
    }

    ktxTexture2* texture;
    ktxTextureCreateInfo info{};
    constexpr uint32_t FORMAT[] = {~0, 9, 16, 23, 37}; // VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM
    info.vkFormat = FORMAT[ch];
    info.baseWidth = x;
    info.baseHeight = y;
    info.baseDepth = 1;
    info.numDimensions = 2; // 이미지 차원수(대부분 2)
    info.numFaces = 1; // 큐브맵일 때만 6, 나머지 1
    info.numLayers = 1; // 이미지 배열일 때 구성 이미지 수
    info.numLevels = mip; // 원하는 밉 수준 수
    info.isArray = KTX_FALSE;
    info.generateMipmaps = KTX_FALSE;
    ktx_error_code_e result = ktxTexture2_Create(&info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
    if (result != KTX_SUCCESS) {
        printf("KTX create failed: %d\n", result);
        stbi_image_free(mipPtrs[0]);
        return false;
    }
    for(int i = 0; i < mip; i++){
        result = ktxTexture_SetImageFromMemory(ktxTexture(texture), i, 0, 0, mipPtrs[i], sizeof(uint8_t) * ch * x * y);
        if(result != KTX_SUCCESS) {
            printf("KTX Image memory setting failed: %d\n", result);
            ktxTexture_Destroy(ktxTexture(texture));
            for(int i=0;i<mip;i++){ std::free(mipPtrs[i]); }
            return false;
        }
    }
    ktxBasisParams params{};
    params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
    params.uastc = uastc ? KTX_TRUE : KTX_FALSE;
    params.verbose = KTX_TRUE;
    params.structSize = sizeof(params);
    
    if((result = ktxTexture2_CompressBasisEx(texture, &params)) != KTX_SUCCESS){
        printf("KTX compress failed: %d\n",result);
        return false;
    }
    std::filesystem::path path(fileName);
    path.replace_extension("");
    path.append("_texture.ktx");
    result = ktxTexture_WriteToNamedFile(ktxTexture(texture), path.string().c_str());
    if (result != KTX_SUCCESS) {
        printf("KTX write failed: %d\n", result);
        return false;
    }
    else{
        printf("KTX convert complete\n");
    }

    ktxTexture_Destroy(ktxTexture(texture));
    for(int i = 0; i < mip; i++){ std::free(mipPtrs[i]); }
    return true;
}

int main(int argc, char* argv[]){
    if(argc < 2) {
        printf("Usage: %s file_name [mip_level] [uastc?]\n", argv[0]);
        return 0;
    }
    convert(argv[1], argc >= 3 ? std::atoi(argv[2]) : 1, argc >= 4);
}