#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>

int main(int argc, char* argv[]){
    union {
        uint32_t i;
        uint8_t c[4];
    };
    i=1;
    bool IS_LE = c[0];
    fprintf(stderr, "%s endian system\n", IS_LE ? "little" : "big");
    if (!IS_LE) return 0;
    if(argc < 2) {
        printf("usage: %s fileName [variable size(default 8)]\n", argv[0]);
        return 0;
    }
    FILE* fp = fopen(argv[1], "rb");
    if(!fp) {
        perror("fopen");
        return 0;
    }
    int mode = argc >= 3 ? atoi(argv[2]) : 8;
    switch (mode)
    {
    case 8:
    case 16:
    case 32:
    case 64:
        mode /= 8;
        break;
    default:
        mode = 1;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if(sz % mode != 0){
        fprintf(stderr, "Invalid file size. %d byte encoding requested but the file size is not the multiple of that", mode);
        fclose(fp);
        return 0;
    }
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> content(sz);
    fread(content.data(), 1, sz, fp);
    fclose(fp);
    printf("const uint%d_t argv[%d]={",mode*8,sz/mode);
    size_t p=0;
    while(1) {
        uint64_t i = 0;
        memcpy(&i, &content[p], mode);
        printf("%lld",i);
        p += mode;
        sz -= mode;
        if (sz == 0) break;
        printf(",");
    }
    printf("};");
}