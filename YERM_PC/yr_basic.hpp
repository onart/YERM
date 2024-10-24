#ifndef __YR_BASIC_HPP__
#define __YR_BASIC_HPP__

#include <cstdint>

namespace onart
{
    union variant8 {
        uint8_t bytedata1[8];
        uint16_t bytedata2[4];
        uint32_t bytedata4[2];
#define TYPE_N_CAST(type, varname) type varname; inline variant8(type t):varname(t){} inline type& operator=(type t){return varname = t;}
        TYPE_N_CAST(int8_t, i8)
        TYPE_N_CAST(uint8_t, u8)
        TYPE_N_CAST(int16_t, i16)
        TYPE_N_CAST(uint16_t, u16)
        TYPE_N_CAST(int32_t, i32)
        TYPE_N_CAST(uint32_t, u32)
        TYPE_N_CAST(int64_t, i64)
        TYPE_N_CAST(uint64_t, u64)
        TYPE_N_CAST(float, f)
        TYPE_N_CAST(double, db)
        TYPE_N_CAST(void*, vp)
#undef TYPE_N_CAST
        inline variant8(const variant8& other):u64(other.u64) {}
        inline variant8():u64(0) {}
    };

    template<class T>
    struct shp_t : public T {
        template<typename... Args>
        shp_t(Args&&... args) : T(std::forward<Args>(args)...) {}
    };
}

#endif