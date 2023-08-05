// Copyright 2022 onart@github. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef __YR_SIMD_HPP__
#define __YR_SIMD_HPP__

#ifdef YR_USE_WEBGPU
#define YR_NOSIMD // wasm용 simd 사용이 완전히 되기 전까지 봉인함.
#endif

#include "../externals/boost/predef/hardware.h"
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cassert>

namespace onart{

    /// \~korean
    /// @brief 배열의 앞 4개를 주어진 값으로 초기화합니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 초기화 값
    template <class T>
    inline void set4(T* vec, T val){
        vec[0]=val; vec[1]=val; vec[2]=val; vec[3]=val;
    }
    
    /// @brief 배열의 앞 4개를 주어진 값으로 초기화합니다. memcpy를 사용하는 것과 같습니다.
    /// @tparam T 
    /// @param dst 초기화할 배열
    /// @param src 값을 제공할 배열
    template <class T>
    inline void set4(T* dst, const T* src){
        memcpy(dst,src,4*sizeof(T));
    }

    /// @brief 배열을 앞에서부터 원하는 수만큼 주어진 값으로 초기화합니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 초기화 값
    /// @param size 초기화할 원소 수
    template <class T>
    inline void setAll(T* vec, T val, size_t size){
        size_t i = 4;
        T val4[4];
        set4<T>(val4, val); // 컴파일러의 XMM 최적화를 더 쉽게 하기 위해서
        for (; i <= size; i += 4) {
            set4<T>(vec + (i - 4), val4);
        }
        for (i -= 4; i < size; i++) {
            vec[i] = val;
        }
    }

    /// @brief 배열을 앞에서부터 원하는 만큼 주어진 값으로 초기화합니다. memcpy를 사용하는 것과 같습니다.
    /// @tparam T 
    /// @param dst 초기화할 배열
    /// @param src 값을 제공할 배열
    /// @param size 초기화할 원소 수
    template <class T>
    inline void setAll(T* dst, const T* src, size_t size){
        memcpy(dst, src, sizeof(T)*size);
    }

    /// @brief 배열의 앞 4개에 주어진 값을 누적합니다.
    template<class T>
    inline void add4(T* vec, T val){
        vec[0]+=val; vec[1]+=val; vec[2]+=val; vec[3]+=val;
    }

    /// @brief 배열의 앞 4개에 주어진 값을 누적합니다.
    template <class T>
    inline void add4(T* vec, const T* val){
        vec[0]+=val[0]; vec[1]+=val[1]; vec[2]+=val[2]; vec[3]+=val[3];
    }

    /// @brief 배열 앞에서부터 주어진 값을 누적합니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 더할 값
    /// @param size 더할 원소 수
    template<class T>
    inline void addAll(T* vec, T val, size_t size){
        size_t i = 4;
        T val4[4];
        set4<T>(val4, val); // 컴파일러의 XMM 최적화를 더 쉽게 하기 위해서
        for (; i <= size; i += 4) {
            add4<T>(vec + (i - 4), val4);
        }
        for (i -= 4; i < size; i++) {
            vec[i] += val;
        }
    }

    /// @brief 배열 앞에서부터 주어진 값을 누적합니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 더할 배열
    /// @param size 더할 원소 수
    template<class T>
    inline void addAll(T* vec, const T* val, size_t size){
        size_t i = 4;
        for (; i <= size; i += 4) {
            add4<T>(vec + (i - 4), val + (i - 4));
        }
        for (i -= 4; i < size; i++) {
            vec[i] += val[i];
        }
    }

    /// @brief 배열 앞 4개에서 주어진 값을 뺍니다. 
    template<class T>
    inline void sub4(T* vec, T val){
        vec[0]-=val; vec[1]-=val; vec[2]-=val; vec[3]-=val;
    }

    /// @brief 배열 앞 4개에서 주어진 값을 뺍니다. 
    template <class T>
    inline void sub4(T* vec, const T* val){
        vec[0]-=val[0]; vec[1]-=val[1]; vec[2]-=val[2]; vec[3]-=val[3];
    }

    /// @brief 배열 앞에서부터 주어진 값을 뺍니다. 
    /// @tparam T 
    /// @param vec 초기화할 배열 Array to be accumulated
    /// @param val 더할 값 Value to subtract
    /// @param size 더할 원소 수 Operation element count
    template<class T>
    inline void subAll(T* vec, T val, size_t size){
        size_t i = 4;
        T val4[4];
        set4<T>(val4, val); // 컴파일러의 XMM 최적화를 더 쉽게 하기 위해서
        for (; i <= size; i += 4) {
            sub4<T>(vec + (i - 4), val4);
        }
        for (i -= 4; i < size; i++) {
            vec[i] -= val;
        }
    }

    /// @brief 배열 앞에서부터 주어진 값을 뺍니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 더할 배열
    /// @param size 더할 원소 수
    template<class T>
    inline void subAll(T* vec, const T* val, size_t size){
        size_t i = 4;
        for (; i <= size; i += 4) {
            sub4<T>(vec + (i - 4), val + (i - 4));
        }
        for (i -= 4; i < size; i++) {
            vec[i] -= val[i];
        }
    }

    /// @brief 배열 앞 4개에 주어진 값을 곱합니다.
    template<class T>
    inline void mul4(T* vec, T val){
        vec[0]*=val; vec[1]*=val; vec[2]*=val; vec[3]*=val;
    }

    /// @brief 배열 앞 4개에 주어진 값을 곱합니다.
    template <class T>
    inline void mul4(T* vec, const T* val){
        vec[0]*=val[0]; vec[1]*=val[1]; vec[2]*=val[2]; vec[3]*=val[3];
    }

    /// @brief 배열 앞에서부터 주어진 값을 곱합니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 곱할 값
    /// @param size 곱할 원소 수
    template<class T>
    inline void mulAll(T* vec, T val, size_t size){
        size_t i = 4;
        T val4[4];
        set4<T>(val4, val); // 컴파일러의 XMM 최적화를 더 쉽게 하기 위해서
        for (; i <= size; i += 4) {
            mul4<T>(vec + (i - 4), val4);
        }
        for (i -= 4; i < size; i++) {
            vec[i] *= val;
        }
    }

    /// @brief 배열 앞에서부터 주어진 값을 곱합니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 곱할 배열
    /// @param size 곱할 원소 수
    template<class T>
    inline void mulAll(T* vec, const T* val, size_t size){
        size_t i = 4;
        for (; i <= size; i += 4) {
            mul4<T>(vec + (i - 4), val + (i - 4));
        }
        for (i -= 4; i < size; i++) {
            vec[i] *= val[i];
        }
    }

    /// @brief 배열 앞 4개를 주어진 값으로 나눕니다. 
    template<class T>
    inline void div4(T* vec, T val){
        vec[0]/=val; vec[1]/=val; vec[2]/=val; vec[3]/=val;
    }

    /// @brief 배열 앞 4개를 주어진 값으로 나눕니다. 
    template <class T>
    inline void div4(T* vec, const T* val){
        vec[0]/=val[0]; vec[1]/=val[1]; vec[2]/=val[2]; vec[3]/=val[3];
    }

    /// @brief 배열 앞에서부터 주어진 값으로 나눕니다. 
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 나눌 값
    /// @param size 나눌 원소 수
    template<class T>
    inline void divAll(T* vec, T val, size_t size){
        size_t i = 4;
        T val4[4];
        set4<T>(val4, val); // 컴파일러의 XMM 최적화를 더 쉽게 하기 위해서
        for (; i <= size; i += 4) {
            div4<T>(vec + (i - 4), val4);
        }
        for (i -= 4; i < size; i++) {
            vec[i] /= val;
        }
    }

    /// @brief 배열 앞에서부터 주어진 값으로 나눕니다.
    /// @tparam T 
    /// @param vec 초기화할 배열
    /// @param val 나눌 배열
    /// @param size 나눌 원소 수
    template<class T>
    inline void divAll(T* vec, const T* val, size_t size){
        size_t i = 4;
        for (; i <= size; i += 4) {
            div4<T>(vec + (i - 4), val + (i - 4));
        }
        for (i -= 4; i < size; i++) {
            vec[i] /= val[i];
        }
    }

    /// @brief 배열 앞에서부터 4개를 절댓값으로 바꿉니다.
    /// @tparam T 
    template<class T>
    inline void abs4(T* vec){
        vec[0] = std::abs(vec[0]);
        vec[1] = std::abs(vec[1]);
        vec[2] = std::abs(vec[2]);
        vec[3] = std::abs(vec[3]);
    }

    /// @brief 배열 앞에서부터 4개를 절댓값이 같은 음수로 바꿉니다.
    /// @tparam T 
    template<class T>
    inline void mabs4(T* vec){
        vec[0] = -std::abs(vec[0]);
        vec[1] = -std::abs(vec[1]);
        vec[2] = -std::abs(vec[2]);
        vec[3] = -std::abs(vec[3]);
    }

    /// @brief 배열 앞에서부터 4개의 부호를 반전합니다. -1을 곱하는 것과 속도가 같거나 빠르게 되어 있습니다.
    /// @tparam T 
    template<class T>
    inline void neg4(T* vec){
        mul4(vec, -1);
    }

    /// @brief 벡터 성분 섞기를 위한 인수입니다.
    enum class SWIZZLE_SYMBOL { X=0, Y=1, Z=2, W=3, R=0, G=1, B=2, A=3, S=0, T=1, P=2, Q=3 };
    constexpr SWIZZLE_SYMBOL SWIZZLE_X = SWIZZLE_SYMBOL::X;
    constexpr SWIZZLE_SYMBOL SWIZZLE_Y = SWIZZLE_SYMBOL::Y;
    constexpr SWIZZLE_SYMBOL SWIZZLE_Z = SWIZZLE_SYMBOL::Z;
    constexpr SWIZZLE_SYMBOL SWIZZLE_W = SWIZZLE_SYMBOL::W;
    constexpr SWIZZLE_SYMBOL SWIZZLE_R = SWIZZLE_SYMBOL::R;
    constexpr SWIZZLE_SYMBOL SWIZZLE_G = SWIZZLE_SYMBOL::G;
    constexpr SWIZZLE_SYMBOL SWIZZLE_B = SWIZZLE_SYMBOL::B;
    constexpr SWIZZLE_SYMBOL SWIZZLE_A = SWIZZLE_SYMBOL::A;
    constexpr SWIZZLE_SYMBOL SWIZZLE_S = SWIZZLE_SYMBOL::S;
    constexpr SWIZZLE_SYMBOL SWIZZLE_T = SWIZZLE_SYMBOL::T;
    constexpr SWIZZLE_SYMBOL SWIZZLE_P = SWIZZLE_SYMBOL::P;
    constexpr SWIZZLE_SYMBOL SWIZZLE_Q = SWIZZLE_SYMBOL::Q;

    /// @brief 벡터 성분 섞기를 위한 인수입니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline constexpr int SWIZZLE_IMM = ((int)P3 << 6) | ((int)P2 << 4) | ((int)P1 << 2) | (int)P0;
    
    /// @brief 배열의 앞 4개 성분을 주어진 대로 섞습니다.
    /// ARM Neon에는 직접적으로 swizzle/shuffle을 명시한 게 없음. 참고 https://documentation-service.arm.com/static/616d26aae4f35d248467d648?token=
    template<class T, SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(T* vec){
        T temp[4];
        temp[0] = vec[(int)P0];
        temp[1] = vec[(int)P1];
        temp[2] = vec[(int)P2];
        temp[3] = vec[(int)P3];
        memcpy(vec, temp, sizeof(temp));
    }

    /// @brief 역제곱근을 리턴합니다.
    inline double rsqrt(double d) { return 1.0 / std::sqrt(d); }

#ifdef YR_USING_SIMD
#undef YR_USING_SIMD
#endif

#ifndef YR_NOSIMD
#if BOOST_HW_SIMD_X86 >= BOOST_HW_SIMD_X86_SSE2_VERSION
#define YR_USING_SIMD
#include <emmintrin.h>
#elif BOOST_HW_SIMD_ARM >= BOOST_HW_SIMD_ARM_NEON_VERSION
#define YR_USING_SIMD
#include "../externals/single_header/sse2neon.h"
#endif
#endif

#ifdef YR_USING_SIMD

    using float128 = __m128;
    using double128 = __m128d;
    using int128 = __m128i;
    using uint128 = __m128i;

    inline float128 loadu(const float* vec) { return _mm_loadu_ps(vec); }
    inline float128 load(const float* vec) { return _mm_load_ps(vec); }
    inline float128 load(float f) { return _mm_set_ps1(f); }
    inline float128 load(float _1, float _2, float _3, float _4) { return _mm_set_ps(_4, _3, _2, _1); }
    inline float128 zerof128() { return _mm_setzero_ps(); }
    inline double128 loadu(const double* vec) { return _mm_loadu_pd(vec); }
    inline double128 load(const double* vec) { return _mm_loadu_pd(vec); }
    inline double128 load(double f) { return _mm_set1_pd(f); }
    inline double128 load(double _1, double _2) { return _mm_set_pd(_2, _1); }
    inline double128 zerod128() { return _mm_setzero_pd(); }
    inline int128 loadu(const int32_t* vec) { return _mm_loadu_si128((__m128i*)vec); }
    inline int128 load(const int32_t* vec) { return _mm_load_si128((__m128i*)vec); }
    inline int128 load(int32_t f) { return _mm_set1_epi32(f); }
    inline int128 load(int32_t _1, int32_t _2, int32_t _3, int32_t _4) { return _mm_set_epi32(_4, _3, _2, _1); }
    inline int128 zeroi128() { return _mm_setzero_si128(); }
    inline uint128 loadu(const uint32_t* vec) { return _mm_loadu_si128((__m128i*)vec); }
    inline uint128 load(const uint32_t* vec) { return _mm_load_si128((__m128i*)vec); }
    inline uint128 load(uint32_t f) { return _mm_set1_epi32(f); }
    inline uint128 load(uint32_t _1, uint32_t _2, uint32_t _3, uint32_t _4) { return _mm_set_epi32(_4, _3, _2, _1); }
    inline uint128 zerou128() { return _mm_setzero_si128(); }

    inline void storeu(float128 vec, float* output) { _mm_storeu_ps(output, vec); }
    inline void store(float128 vec, float* output) { _mm_store_ps(output, vec); }
    inline void storeu(double128 vec, double* output) { _mm_storeu_pd(output, vec); }
    inline void store(double128 vec, double* output) { _mm_store_pd(output, vec); }
    inline void storeu(int128 vec, int32_t* output) { _mm_storeu_si128((__m128i*)output, vec); }
    inline void store(int128 vec, int32_t* output) { _mm_store_si128((__m128i*)output, vec); }
    inline void storeu(uint128 vec, uint32_t* output) { _mm_storeu_si128((__m128i*)output, vec); }
    inline void store(uint128 vec, uint32_t* output) { _mm_store_si128((__m128i*)output, vec); }

    inline float128 add(float128 a, float128 b) { return _mm_add_ps(a,b); }
    inline float128 sub(float128 a, float128 b) { return _mm_sub_ps(a,b); }
    inline float128 mul(float128 a, float128 b) { return _mm_mul_ps(a,b); }
    inline float128 div(float128 a, float128 b) { return _mm_div_ps(a,b); }
    inline float128 b_and(float128 a, float128 b) { return _mm_and_ps(a,b); }
    inline float128 b_or(float128 a, float128 b) { return _mm_or_ps(a,b); }
    inline float128 b_xor(float128 a, float128 b) { return _mm_xor_ps(a,b); }

    inline float128 mabs(float128 a) { return b_or(_mm_set_ps1(-0.0f), a); }
    inline float128 abs(float128 a) { return b_xor(_mm_set_ps1(-0.0f), mabs(a)); }
    inline float128 sqrt(float128 a) { return _mm_sqrt_ps(a); }
    inline float128 rsqrt(float128 a) { return _mm_rsqrt_ps(a); }
    inline float128 rcp(float128 a) { return _mm_rcp_ps(a); }

    inline double128 add(double128 a, double128 b) { return _mm_add_pd(a,b); }
    inline double128 sub(double128 a, double128 b) { return _mm_sub_pd(a,b); }
    inline double128 mul(double128 a, double128 b) { return _mm_mul_pd(a,b); }
    inline double128 div(double128 a, double128 b) { return _mm_div_pd(a,b); }
    inline double128 b_and(double128 a, double128 b) { return _mm_and_pd(a,b); }
    inline double128 b_or(double128 a, double128 b) { return _mm_or_pd(a,b); }
    inline double128 b_xor(double128 a, double128 b) { return _mm_xor_pd(a,b); }

    inline double128 mabs(double128 a) { return b_or(_mm_set_pd1(-0.0), a); }
    inline double128 abs(double128 a) { return b_xor(_mm_set_pd1(-0.0), mabs(a)); }
    inline double128 sqrt(double128 a) { return _mm_sqrt_pd(a); }

    inline int128 add(int128 a, int128 b) { return _mm_add_epi32(a,b); }
    inline int128 sub(int128 a, int128 b) { return _mm_sub_epi32(a,b); }
    inline int128 mul(int128 a, int128 b) { return _mm_mullo_epi16(a,b); }
    inline int128 b_and(int128 a, int128 b) { return _mm_and_si128(a,b); }
    inline int128 b_or(int128 a, int128 b) { return _mm_or_si128(a,b); }
    inline int128 b_xor(int128 a, int128 b) { return _mm_xor_si128(a,b); }
    template<uint8_t A> inline int128 shiftLeft(int128 a) { return _mm_slli_epi32(a,A); }
    template<uint8_t A> inline int128 shiftRight(int128 a) { return _mm_srai_epi32(a,A); }
    inline int128 neg(int128 a){ return sub(zeroi128(), a); }

    template<bool a, bool b, bool c, bool d>
    inline float128 toggleSigns(float128 x) { 
        constexpr float SA = a ? -0.0f : 0.0f, SB = b ? -0.0f : 0.0f, SC = c ? -0.0f : 0.0f, SD = d ? -0.0f : 0.0f;
        return b_xor(_mm_set_ps(SA,SB,SC,SD), x);
    }

    template<bool a, bool b>
    inline double128 toggleSigns(double128 x) { 
        constexpr double SA = a ? -0.0 : 0.0, SB = b ? -0.0 : 0.0;
        return b_xor(_mm_set_pd(SA,SB), x);
    }

    inline float128 neg(float128 a) { return toggleSigns<true,true,true,true>(a); }
    inline double128 neg(double128 a) { return toggleSigns<true,true>(a); }

    /// @brief float 배열 앞 4개를 원하는 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline float128 swizzle(float128 a){ return _mm_shuffle_ps(a, a, (SWIZZLE_IMM<P0,P1,P2,P3>)); }

    /// @brief float 배열 앞 4개를 원하는 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline int128 swizzle(int128 a){ return _mm_shuffle_epi32(a, (SWIZZLE_IMM<P0,P1,P2,P3>)); }

    /// @brief float 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<float>(float* vec, float val){
        __m128 b = _mm_set_ps1(val);
        _mm_storeu_ps(vec, b);
    }

    /// @brief double 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<double>(double* vec, double val){
        __m128d b = _mm_set1_pd(val);
        _mm_storeu_pd(vec, b);
        _mm_storeu_pd(vec+2, b);
    }

    /// @brief int32_t 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<int32_t>(int32_t* vec, int32_t val){
        __m128i b = _mm_set1_epi32(val);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief uint32_t 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<uint32_t>(uint32_t* vec, uint32_t val){
        __m128i b = _mm_set1_epi32(val);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<float>(float* vec, float val){
        __m128 b = _mm_add_ps(_mm_loadu_ps(vec),_mm_set_ps1(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 누적합니다.
    /// @param vec 배열
    /// @param val 누적할 값들
    template<>
    inline void add4<float>(float* vec, const float* val){
        __m128 b = _mm_add_ps(_mm_loadu_ps(vec), _mm_loadu_ps(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<double>(double* vec, double val){
        __m128d v = _mm_set1_pd(val);
        __m128d b = _mm_add_pd(_mm_loadu_pd(vec),v); _mm_storeu_pd(vec, b);
        b = _mm_add_pd(_mm_loadu_pd(vec + 2),v); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 누적합니다.
    /// @param vec 배열
    /// @param val 누적할 값들
    template<>
    inline void add4<double>(double* vec, const double* val){
        __m128d b = _mm_add_pd(_mm_loadu_pd(vec), _mm_loadu_pd(val)); _mm_storeu_pd(vec, b);
        b = _mm_add_pd(_mm_loadu_pd(vec + 2), _mm_loadu_pd(val + 2)); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief int32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<int32_t>(int32_t* vec, int32_t val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_set1_epi32(val);
        b = _mm_add_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief int32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    /// @param vec 배열
    /// @param val 누적할 값들
    template<>
    inline void add4<int32_t>(int32_t* vec, const int32_t* val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_loadu_si128((__m128i*)val);
        b = _mm_add_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }
    
     /// @brief uint32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<uint32_t>(uint32_t* vec, uint32_t val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_set1_epi32(val);
        b = _mm_add_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    /// @param vec 배열
    /// @param val 누적할 값들
    template<>
    inline void add4<uint32_t>(uint32_t* vec, const uint32_t* val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_loadu_si128((__m128i*)val);
        b = _mm_add_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief float 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<float>(float* vec, float val){
        __m128 b = _mm_sub_ps(_mm_loadu_ps(vec),_mm_set_ps1(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief float 배열의 앞 4개에서 주어진 값을 뺍니다.
    /// @param vec 배열
    /// @param val 뺄 값들
    template<>
    inline void sub4<float>(float* vec, const float* val){
        __m128 b = _mm_sub_ps(_mm_loadu_ps(vec), _mm_loadu_ps(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief double 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<double>(double* vec, double val){
        __m128d v = _mm_set1_pd(val);
        __m128d b = _mm_sub_pd(_mm_loadu_pd(vec),v); _mm_storeu_pd(vec, b);
        b = _mm_sub_pd(_mm_loadu_pd(vec + 2),v); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief double 배열의 앞 4개에서 주어진 값을 뺍니다.
    /// @param vec 배열
    /// @param val 누적할 값들
    template<>
    inline void sub4<double>(double* vec, const double* val){
        __m128d b = _mm_sub_pd(_mm_loadu_pd(vec), _mm_loadu_pd(val)); _mm_storeu_pd(vec, b);
        b = _mm_sub_pd(_mm_loadu_pd(vec + 2), _mm_loadu_pd(val + 2)); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief int32_t 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<int32_t>(int32_t* vec, int32_t val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_set1_epi32(val);
        b = _mm_sub_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief int32_t 배열의 앞 4개에서 주어진 값을 뺍니다.
    /// @param vec 배열
    /// @param val 뺄 값들
    template<>
    inline void sub4<int32_t>(int32_t* vec, const int32_t* val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_loadu_si128((__m128i*)val);
        b = _mm_sub_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief uint32_t 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<uint32_t>(uint32_t* vec, uint32_t val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_set1_epi32(val);
        b = _mm_sub_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief uint32_t 배열의 앞 4개에서 주어진 값을 뺍니다.
    /// @param vec 배열
    /// @param val 뺄 값들
    template<>
    inline void sub4<uint32_t>(uint32_t* vec, const uint32_t* val){
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_loadu_si128((__m128i*)val);
        b = _mm_sub_epi32(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<float>(float* vec, float val){
        __m128 b = _mm_mul_ps(_mm_loadu_ps(vec),_mm_set_ps1(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 곱합니다.
    /// @param vec 배열
    /// @param val 뺄 값들
    template<>
    inline void mul4<float>(float* vec, const float* val){
        __m128 b = _mm_mul_ps(_mm_loadu_ps(vec), _mm_loadu_ps(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<double>(double* vec, double val){
        __m128d v = _mm_set1_pd(val);
        __m128d b = _mm_mul_pd(_mm_loadu_pd(vec),v); _mm_storeu_pd(vec, b);
        b = _mm_mul_pd(_mm_loadu_pd(vec + 2),v); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 곱합니다.
    /// @param vec 배열
    /// @param val 누적할 값들
    template<>
    inline void mul4<double>(double* vec, const double* val){
        __m128d b = _mm_mul_pd(_mm_loadu_pd(vec), _mm_loadu_pd(val)); _mm_storeu_pd(vec, b);
        b = _mm_mul_pd(_mm_loadu_pd(vec + 2), _mm_loadu_pd(val + 2)); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief int32_t 배열의 앞 4개에 주어진 값을 곱합니다. 각 int의 하위 16비트끼리만 올바르게 계산됨에 유의하세요.
    template<>
    inline void mul4<int32_t>(int32_t* vec, int32_t val){
        assert(val <= 0xffff && vec[0] <= 0xffff && vec[1] <= 0xffff && vec[2] <= 0xffff && vec[3] <= 0xffff && "이 용도로 이 함수(클래스)를 사용하기엔 부적합할 수 있습니다.");
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_set1_epi32(val);
        b = _mm_mullo_epi16(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief int32_t 배열의 앞 4개에 주어진 값을 곱합니다. 각 int의 하위 16비트끼리만 올바르게 계산됨에 유의하세요.
    /// @param vec 배열
    /// @param val 곱할 값들
    template<>
    inline void mul4<int32_t>(int32_t* vec, const int32_t* val){
        assert(val[0] <= 0xffff && val[1] <= 0xffff && val[2] <= 0xffff && val[3] <= 0xffff && vec[0] <= 0xffff && vec[1] <= 0xffff && vec[2] <= 0xffff && vec[3] <= 0xffff && "이 용도로 이 함수(클래스)를 사용하기엔 부적합할 수 있습니다.");
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_loadu_si128((__m128i*)val);
        b = _mm_mullo_epi16(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 곱합니다. 각 int의 하위 16비트끼리만 올바르게 계산됨에 유의하세요.
    template<>
    inline void mul4<uint32_t>(uint32_t* vec, uint32_t val){
        assert(val <= 0xffff && vec[0] <= 0xffff && vec[1] <= 0xffff && vec[2] <= 0xffff && vec[3] <= 0xffff && "이 용도로 이 함수(클래스)를 사용하기엔 부적합할 수 있습니다.");
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_set1_epi32(val);
        b = _mm_mullo_epi16(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 곱합니다. 각 int의 하위 16비트끼리만 올바르게 계산됨에 유의하세요.
    /// @param vec 배열
    /// @param val 곱할 값들
    template<>
    inline void mul4<uint32_t>(uint32_t* vec, const uint32_t* val){
        assert(val[0] <= 0xffff && val[1] <= 0xffff && val[2] <= 0xffff && val[3] <= 0xffff && vec[0] <= 0xffff && vec[1] <= 0xffff && vec[2] <= 0xffff && vec[3] <= 0xffff && "이 용도로 이 함수(클래스)를 사용하기엔 부적합할 수 있습니다.");
        __m128i b = _mm_loadu_si128((__m128i*)vec);
        __m128i v = _mm_loadu_si128((__m128i*)val);
        b = _mm_mullo_epi16(b, v);
        _mm_storeu_si128((__m128i*)vec, b);
    }


    /// @brief float 배열의 앞 4개를 주어진 값으로 나눕니다.
    template<>
    inline void div4<float>(float* vec, float val){
        __m128 b = _mm_div_ps(_mm_loadu_ps(vec),_mm_set_ps1(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief float 배열의 앞 4개를 주어진 값으로 나눕니다.
    /// @param vec 배열
    /// @param val 뺄 값들
    template<>
    inline void div4<float>(float* vec, const float* val){
        __m128 b = _mm_div_ps(_mm_loadu_ps(vec), _mm_loadu_ps(val));
        _mm_storeu_ps(vec, b);
    }

    /// @brief double 배열의 앞 4개를 주어진 값으로 나눕니다.
    template<>
    inline void div4<double>(double* vec, double val){
        __m128d v = _mm_set1_pd(val);
        __m128d b = _mm_div_pd(_mm_loadu_pd(vec),v); _mm_storeu_pd(vec, b);
        b = _mm_div_pd(_mm_loadu_pd(vec + 2),v); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief double 배열의 앞 4개를 주어진 값으로 나눕니다.
    /// @param vec 배열
    /// @param val 누적할 값들
    template<>
    inline void div4<double>(double* vec, const double* val){
        __m128d b = _mm_div_pd(_mm_loadu_pd(vec), _mm_loadu_pd(val)); _mm_storeu_pd(vec, b);
        b = _mm_div_pd(_mm_loadu_pd(vec + 2), _mm_loadu_pd(val + 2)); _mm_storeu_pd(vec + 2, b);
    }

    /// @brief float 배열 앞 4개를 절댓값으로 바꿉니다.
    template<>
    inline void abs4<float>(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        constexpr int32_t imask = 0x7fffffff;
        m = _mm_and_ps(m, _mm_set_ps1(*(float*)&imask));
        _mm_storeu_ps(vec, m);
    }

    /// @brief float 배열 앞 4개를 절댓값이 같은 음수로 바꿉니다.
    template<>
    inline void mabs4<float>(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        m = _mm_or_ps(m, _mm_set_ps1(-0.0f));
        _mm_storeu_ps(vec, m);
    }

    /// @brief float 배열 앞 4개의 부호를 반전시킵니다.
    template<>
    inline void neg4<float>(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        m = _mm_xor_ps(m, _mm_set_ps1(-0.0f));
        _mm_storeu_ps(vec, m);
    }
 
    /// @brief double 배열 앞 4개를 절댓값으로 바꿉니다.
    template<>
    inline void abs4<double>(double* vec){
        __m128d m;
        constexpr int64_t imask = 0x7fffffffffffffff;
        __m128d ands = _mm_set1_pd(*(double*)&imask);
        m = _mm_and_pd(_mm_loadu_pd(vec), ands); _mm_storeu_pd(vec, m);
        m = _mm_and_pd(_mm_loadu_pd(vec + 2), ands); _mm_storeu_pd(vec + 2, m);
    }

    /// @brief double 배열 앞 4개를 절댓값이 같은 음수로 바꿉니다.
    template<>
    inline void mabs4<double>(double* vec){
        __m128d m;
        __m128d mzero = _mm_set1_pd(-0.0);
        m = _mm_or_pd(_mm_loadu_pd(vec), mzero); _mm_storeu_pd(vec, m);
        m = _mm_or_pd(_mm_loadu_pd(vec+2), mzero); _mm_storeu_pd(vec+2, m);
    }

    /// @brief double 배열 앞 4개의 부호를 반전시킵니다.
    template<>
    inline void neg4<double>(double* vec){
        __m128d m;
        __m128d mzero = _mm_set1_pd(-0.0);
        m = _mm_xor_pd(_mm_loadu_pd(vec), mzero); _mm_storeu_pd(vec, m);
        m = _mm_xor_pd(_mm_loadu_pd(vec+2), mzero); _mm_storeu_pd(vec+2, m);
    }

    /// @brief float 배열 앞 4개를 원하는 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        m = _mm_shuffle_ps(m, m, (SWIZZLE_IMM<P0,P1,P2,P3>));
        _mm_storeu_ps(vec, m);
    }

    /// @brief double 배열 앞 4개를 원하는 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(double* vec){
        __m128d m1 = _mm_loadu_pd(vec);
        __m128d m2 = _mm_loadu_pd(vec + 2);
        if constexpr (P0 >= SWIZZLE_Z) {
            if constexpr (P1 >= SWIZZLE_Z) {
                constexpr int IMM0 = P0 == SWIZZLE_W;
                constexpr int IMM1 = P1 == SWIZZLE_W;
                __m128d m3 = _mm_shuffle_pd(m2, m2, (IMM1 << 1) | IMM0);
                _mm_storeu_pd(vec, m3);
            }
            else {
                constexpr int IMM0 = P0 == SWIZZLE_W;
                constexpr int IMM1 = P1 == SWIZZLE_Y;
                __m128d m3 = _mm_shuffle_pd(m2, m1, (IMM1 << 1) | IMM0);
                _mm_storeu_pd(vec, m3);
            }
        }
        else if constexpr (P1 >= SWIZZLE_Z) {
            constexpr int IMM0 = P0 == SWIZZLE_Y;
            constexpr int IMM1 = P1 == SWIZZLE_W;
            __m128d m3 = _mm_shuffle_pd(m1, m2, (IMM1 << 1) | IMM0);
            _mm_storeu_pd(vec, m3);
        }
        else {
            constexpr int IMM0 = P0 == SWIZZLE_Y;
            constexpr int IMM1 = P1 == SWIZZLE_Y;
            __m128d m3 = _mm_shuffle_pd(m1, m1, (IMM1 << 1) | IMM0);
            _mm_storeu_pd(vec, m3);
        }

        if constexpr (P2 >= SWIZZLE_Z) {
            if constexpr (P3 >= SWIZZLE_Z) {
                constexpr int IMM0 = P2 == SWIZZLE_W;
                constexpr int IMM1 = P3 == SWIZZLE_W;
                __m128d m3 = _mm_shuffle_pd(m2, m2, (IMM1 << 1) | IMM0);
                _mm_storeu_pd(vec + 2, m3);
            }
            else {
                constexpr int IMM0 = P2 == SWIZZLE_W;
                constexpr int IMM1 = P3 == SWIZZLE_Y;
                __m128d m3 = _mm_shuffle_pd(m2, m1, (IMM1 << 1) | IMM0);
                _mm_storeu_pd(vec + 2, m3);
            }
        }
        else if constexpr (P3 >= SWIZZLE_Z) {
            constexpr int IMM0 = P2 == SWIZZLE_Y;
            constexpr int IMM1 = P3 == SWIZZLE_W;
            __m128d m3 = _mm_shuffle_pd(m1, m2, (IMM1 << 1) | IMM0);
            _mm_storeu_pd(vec + 2, m3);
        }
        else {
            constexpr int IMM0 = P2 == SWIZZLE_Y;
            constexpr int IMM1 = P3 == SWIZZLE_Y;
            __m128d m3 = _mm_shuffle_pd(m1, m1, (IMM1 << 1) | IMM0);
            _mm_storeu_pd(vec + 2, m3);
        }
    }

    /// @brief int32_t 배열 앞 4개를 원하는 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(int32_t* vec){
        __m128i m = _mm_loadu_si128((__m128i*)vec);
        m = _mm_shuffle_epi32(m, (SWIZZLE_IMM<P0, P1, P2, P3>));
        _mm_storeu_si128((__m128i*)vec, m);
    }

    /// @brief uint32_t 배열 앞 4개를 원하는 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(uint32_t* vec){
        __m128i m = _mm_loadu_si128((__m128i*)vec);
        m = _mm_shuffle_epi32(m, (SWIZZLE_IMM<P0, P1, P2, P3>));
        _mm_storeu_si128((__m128i*)vec, m);
    }

    /// @brief float 배열에서 앞 4개를 양의 제곱근을 적용한 값으로 바꿉니다. 음수 검사는 하지 않고 nan으로 변합니다.
    inline void sqrt4(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        m = _mm_sqrt_ps(m);
        _mm_storeu_ps(vec, m);
    }

    /// @brief float 배열에서 앞 4개에 양의 제곱근의 역수를 적용한 값으로 바꿉니다. 음수 및 0 검사는 하지 않고 nan 혹은 inf로 변합니다. 
    inline void rsqrt4(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        m = _mm_rsqrt_ps(m);
        _mm_storeu_ps(vec, m);
    }

    /// @brief double 배열에서 앞 4개를 양의 제곱근을 적용한 값으로 바꿉니다. 음수 검사는 하지 않고 nan으로 변합니다.
    inline void sqrt4(double* vec){
        __m128d m = _mm_loadu_pd(vec); m = _mm_sqrt_pd(m); _mm_storeu_pd(vec, m);
        m = _mm_loadu_pd(vec + 2); m = _mm_sqrt_pd(m); _mm_storeu_pd(vec + 2, m);
    }

    /// @brief 실수 배열에서 앞 4개에 양의 제곱근을 적용한 값으로 조금 더 빠르지만 정밀도는 더 낮게 바꿉니다. 음수 및 0 검사는 하지 않고 nan 혹은 inf로 변합니다.
    /// @param vec 
    inline void fastSqrt4(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        __m128 rsq = _mm_rsqrt_ps(m);
        rsq = _mm_mul_ps(rsq, m);
        _mm_storeu_ps(vec, rsq);
    }

    /// @brief 주어진 float의 역제곱근을 리턴합니다. 제곱근보다 빠르지만 오차가 있을 수 있습니다.
    inline float rsqrt(float f){
        __m128 m = _mm_set_ss(f);
        m = _mm_rsqrt_ss(m);
        _mm_store_ss(&f,m);
        return f;
    }

    /// @brief 주어진 float의 역수의 근삿값을 리턴합니다. 0 검사는 하지 않습니다. 1/f보다 빠르지만 그보다 오차가 클 수 있습니다.
    inline float fastReciprocal(float f){
        __m128 m = _mm_set_ss(f);
        m = _mm_rcp_ss(m);
        _mm_store_ss(&f, m);
        return f;
    }

    /// @brief float 배열에서 앞 4개를 역수로 바꿉니다. 1에서 나누는 것보다 빠르지만 오차가 더 클 수 있으며, 0 검사는 하지 않습니다.
    /// @param vec 
    inline void fastReciprocal4(float* vec){
        __m128 m = _mm_loadu_ps(vec);
        m = _mm_rcp_ps(m);
        _mm_storeu_ps(vec, m);
    }

    /// @brief float 4개를 int 4개로 바꿉니다. (버림 적용)
    inline void float2int32(const float* val, int32_t* vec){
        __m128 v = _mm_loadu_ps(val);
        __m128i i = _mm_cvttps_epi32(v);
        _mm_storeu_si128((__m128i*)vec, i);
    }

    /// @brief 2바이트 정수 배열에 다른 배열의 대응 상분을 더합니다. 오버플로는 발생하지 않으며 값을 넘어갈 경우 최대/최솟값으로 고정됩니다.
    /// @param vec 누적 대상
    /// @param val 누적할 값
    /// @param size 누적할 개수
    inline void addsAll(int16_t* vec, const int16_t* val, size_t size){
        size_t i;
        for (i = 8; i < size; i += 8) {
            __m128i ves = _mm_loadu_si128((__m128i*)(vec + i - 8));
            __m128i ver = _mm_loadu_si128((__m128i*)(val + i - 8));
            ves = _mm_adds_epi16(ves, ver);
            _mm_storeu_si128((__m128i*)(vec + i - 8), ves);
        }
        for (i -= 8; i < size; i++) {
            vec[i] += val[i];
        }
    }

    /// @brief 2바이트 정수 배열의 각각의 값에 실수를 곱합니다. 곱하는 실수가 [0,1] 범위일 경우에만 정상적으로 계산해 줍니다. 최대 2의 오차가 발생할 수 있습니다.
    /// @param vec 
    /// @param val 
    /// @param size 
    inline void mulAll(int16_t* vec, float val, size_t size) {
        assert(val <= 1.0f && val >= 0.0f && "이 함수에서 곱해지는 실수의 값은 [0,1] 범위만 허용됩니다.");
        int16_t v2 = (int16_t)((float)val * 32768.0f);
        __m128i v = _mm_set1_epi16(v2);
        size_t i;
        for (i = 8; i < size; i += 8) {
            __m128i ves = _mm_loadu_si128((__m128i*)(vec + i - 8));
            ves = _mm_mulhi_epi16(ves, v);
            ves = _mm_slli_epi16(ves, 1);
            _mm_storeu_si128((__m128i*)(vec + i - 8), ves);
        }
        for (i -= 8; i < size; i++) {
            vec[i] = (int16_t)((float)vec[i] * val);
        }
    }
#else

    inline void addsAll(int16_t* vec, const int16_t* val, size_t size) {
        for (size_t i = 0; i < size; i++) {
            int32_t sum = (int32_t)vec[i] + (int32_t)val[i];
            vec[i] = (sum < INT16_MIN) 
                ? INT16_MIN 
                : (
                    (sum > INT16_MAX) 
                    ? INT16_MAX 
                    : sum);
        }
    }

    inline void mulAll(int16_t* vec, float val, size_t size) {
        assert(val <= 1.0f && val >= 0.0f && "이 함수에서 곱해지는 실수의 값은 [0,1] 범위만 허용됩니다.");
        int16_t v2 = (int16_t)((float)val * 32768.0f);
        for (size_t i = 0; i < size; i++) {
            vec[i] *= val;
        }
    }

    struct float128 { float _[4]; };
    struct double128 { double _[2]; };
    struct int128 { int32_t _[4]; };
    struct uint128 { uint32_t _[4]; };

    inline float128 loadu(const float* vec) { float128 ret; std::memcpy(&ret, vec, sizeof(ret)); return ret; }
    inline float128 load(const float* vec) { return loadu(vec); }
    inline float128 load(float f) { return { f,f,f,f }; }
    inline float128 load(float _1, float _2, float _3, float _4) { return { _1,_2,_3,_4 }; }
    inline float128 zerof128() { return { 0,0,0,0 }; }
    inline double128 loadu(const double* vec) { double128 ret; std::memcpy(&ret, vec, sizeof(ret)); return ret; }
    inline double128 load(const double* vec) { return loadu(vec); }
    inline double128 load(double f) { return { f,f }; }
    inline double128 load(double _1, double _2) { return { _1,_2 }; }
    inline double128 zerod128() { return { 0,0 }; }
    inline int128 loadu(const int32_t* vec) { int128 ret; std::memcpy(&ret, vec, sizeof(ret)); return ret; }
    inline int128 load(const int32_t* vec) { return loadu(vec); }
    inline int128 load(int32_t f) { return { f,f,f,f }; }
    inline int128 load(int32_t _1, int32_t _2, int32_t _3, int32_t _4) { return { _1,_2,_3,_4 }; }
    inline int128 zeroi128() { return { 0,0,0,0 }; }
    inline uint128 loadu(const uint32_t* vec) { uint128 ret; std::memcpy(&ret, vec, sizeof(ret)); return ret; }
    inline uint128 load(const uint32_t* vec) { return loadu(vec); }
    inline uint128 load(uint32_t f) { return { f,f,f,f }; }
    inline uint128 load(uint32_t _1, uint32_t _2, uint32_t _3, uint32_t _4) { return { _1,_2,_3,_4 }; }
    inline uint128 zerou128() { return { 0,0,0,0 }; }

    inline void storeu(float128 vec, float* output) { std::memcpy(output, &vec, sizeof(vec)); }
    inline void store(float128 vec, float* output) { storeu(vec, output); }
    inline void storeu(double128 vec, double* output) { std::memcpy(output, &vec, sizeof(vec)); }
    inline void store(double128 vec, double* output) { storeu(vec, output); }
    inline void storeu(int128 vec, int32_t* output) { std::memcpy(output, &vec, sizeof(vec)); }
    inline void store(int128 vec, int32_t* output) { storeu(vec, output); }
    inline void storeu(uint128 vec, uint32_t* output) { std::memcpy(output, &vec, sizeof(vec)); }
    inline void store(uint128 vec, uint32_t* output) { storeu(vec, output); }

    inline float128 add(float128 a, float128 b) { return { a._[0] + b._[0], a._[1] + b._[1], a._[2] + b._[2], a._[3] + b._[3] }; }
    inline float128 sub(float128 a, float128 b) { return { a._[0] - b._[0], a._[1] - b._[1], a._[2] - b._[2], a._[3] - b._[3] }; }
    inline float128 mul(float128 a, float128 b) { return { a._[0] * b._[0], a._[1] * b._[1], a._[2] * b._[2], a._[3] * b._[3] }; }
    inline float128 div(float128 a, float128 b) { return { a._[0] / b._[0], a._[1] / b._[1], a._[2] / b._[2], a._[3] / b._[3] }; }
#define CAST_AND_OP(op) uint64_t* ua = reinterpret_cast<uint64_t*>(&a); uint64_t* ub = reinterpret_cast<uint64_t*>(&b); union{decltype(a) ret; uint64_t pads[2];}; pads[0] = ua[0] op ub[0]; pads[1] = ua[1] op ub[1]
    inline float128 b_and(float128 a, float128 b) { CAST_AND_OP(&); return ret; }
    inline float128 b_or(float128 a, float128 b) { CAST_AND_OP(|); return ret; }
    inline float128 b_xor(float128 a, float128 b) { CAST_AND_OP(^); return ret; }

    inline double128 add(double128 a, double128 b) { return { a._[0] + b._[0], a._[1] + b._[1] }; }
    inline double128 sub(double128 a, double128 b) { return { a._[0] - b._[0], a._[1] - b._[1] }; }
    inline double128 mul(double128 a, double128 b) { return { a._[0] * b._[0], a._[1] * b._[1] }; }
    inline double128 div(double128 a, double128 b) { return { a._[0] / b._[0], a._[1] / b._[1] }; }
    inline double128 b_and(double128 a, double128 b) { CAST_AND_OP(&); return ret; }
    inline double128 b_or(double128 a, double128 b) { CAST_AND_OP(|); return ret; }
    inline double128 b_xor(double128 a, double128 b) { CAST_AND_OP(^); return ret; }

    inline int128 add(int128 a, int128 b) { return { a._[0] + b._[0], a._[1] + b._[1], a._[2] + b._[2], a._[3] + b._[3] }; }
    inline int128 sub(int128 a, int128 b) { return { a._[0] - b._[0], a._[1] - b._[1], a._[2] - b._[2], a._[3] - b._[3] }; }
    inline int128 mul(int128 a, int128 b) { return { a._[0] * b._[0], a._[1] * b._[1], a._[2] * b._[2], a._[3] * b._[3] }; }
    inline int128 b_and(int128 a, int128 b) { CAST_AND_OP(&); return ret; }
    inline int128 b_or(int128 a, int128 b) { CAST_AND_OP(|); return ret; }
    inline int128 b_xor(int128 a, int128 b) { CAST_AND_OP(^); return ret; }
    template<uint8_t A> inline int128 shiftLeft(int128 a) { return { a._[0] << A, a._[1] << A, a._[2] << A, a._[3] << A }; }
    template<uint8_t A> inline int128 shiftRight(int128 a) { return { a._[0] >> A, a._[1] >> A, a._[2] >> A, a._[3] >> A }; }
    inline int128 neg(int128 a) { return { -a._[0], -a._[1], -a._[2], -a._[3] }; }

    inline uint128 add(uint128 a, uint128 b) { return { a._[0] + b._[0], a._[1] + b._[1], a._[2] + b._[2], a._[3] + b._[3] }; }
    inline uint128 sub(uint128 a, uint128 b) { return { a._[0] - b._[0], a._[1] - b._[1], a._[2] - b._[2], a._[3] - b._[3] }; }
    inline uint128 mul(uint128 a, uint128 b) { return { a._[0] * b._[0], a._[1] * b._[1], a._[2] * b._[2], a._[3] * b._[3] }; }
    inline uint128 b_and(uint128 a, uint128 b) { CAST_AND_OP(&); return ret; }
    inline uint128 b_or(uint128 a, uint128 b) { CAST_AND_OP(|); return ret; }
    inline uint128 b_xor(uint128 a, uint128 b) { CAST_AND_OP(^); return ret; }
    template<uint8_t A> inline uint128 shiftLeft(uint128 a) { return { a._[0] << A, a._[1] << A, a._[2] << A, a._[3] << A }; }
    template<uint8_t A> inline uint128 shiftRight(uint128 a) { return { a._[0] >> A, a._[1] >> A, a._[2] >> A, a._[3] >> A }; }
    inline uint128 neg(uint128 a) { return { -a._[0], -a._[1], -a._[2], -a._[3] }; }

    template<bool a, bool b, bool c, bool d>
    inline float128 toggleSigns(float128 x) {
        return {a ? -x._[0] : x._[0], b ? -x._[1] : x._[1] , c ? -x._[2] : x._[2], d ? -x._[3] : x._[3] };
    }

    template<bool a, bool b>
    inline double128 toggleSigns(double128 x) {
        return { a ? -x._[0] : x._[0], b ? -x._[1] : x._[1] };
    }

    inline float128 mabs(float128 a) { return {-std::abs(a._[0]),-std::abs(a._[1]),-std::abs(a._[2]),-std::abs(a._[3])}; }
    inline float128 abs(float128 a) { return {std::abs(a._[0]),std::abs(a._[1]),std::abs(a._[2]),std::abs(a._[3])}; }
    inline float128 sqrt(float128 a) { return {std::sqrt(a._[0]),std::sqrt(a._[1]),std::sqrt(a._[2]),std::sqrt(a._[3])}; }
    inline float128 rsqrt(float128 a) { return { 1.0f / std::sqrt(a._[0]), 1.0f / std::sqrt(a._[1]), 1.0f / std::sqrt(a._[2]), 1.0f / std::sqrt(a._[3]) }; }
    inline float128 rcp(float128 a) { return { 1/a._[0], 1/a._[1], 1/a._[2], 1/a._[3]}; }
    inline double128 mabs(double128 a) { return {-std::abs(a._[0]), -std::abs(a._[1])}; }
    inline double128 abs(double128 a) { return {std::abs(a._[0]), std::abs(a._[1])}; }
    inline double128 sqrt(double128 a) { return {std::sqrt(a._[0]), std::sqrt(a._[1])}; }

    inline float128 neg(float128 a) { return toggleSigns<true, true, true, true>(a); }
    inline double128 neg(double128 a) { return toggleSigns<true, true>(a); }

    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline float128 swizzle(float128 a) { return { a._[(int)P0], a._[(int)P1], a._[(int)P2], a._[(int)P3] }; }

    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline int128 swizzle(int128 a) { return { a._[(int)P0], a._[(int)P1], a._[(int)P2], a._[(int)P3] }; }

    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline uint128 swizzle(uint128 a) { return { a._[(int)P0], a._[(int)P1], a._[(int)P2], a._[(int)P3] }; }

#undef CAST_AND_OP
    /// @brief 배열의 앞 4개를 제곱근값으로 바꿉니다.
    inline void sqrt4(float* vec){
        vec[0]=std::sqrt(vec[0]);
        vec[1]=std::sqrt(vec[1]);
        vec[2]=std::sqrt(vec[2]);
        vec[3]=std::sqrt(vec[3]);
    }
    /// @brief 배열의 앞 4개를 제곱근값으로 바꿉니다.
    inline void sqrt4(double* vec){
        vec[0]=std::sqrt(vec[0]);
        vec[1]=std::sqrt(vec[1]);
        vec[2]=std::sqrt(vec[2]);
        vec[3]=std::sqrt(vec[3]);
    }
    /// @brief sqrt4와 동일한 함수입니다.
    inline void fastSqrt4(float* vec){ sqrt4(vec); }
    /// @brief 주어진 float의 역제곱근을 리턴합니다. 0 혹은 음수 검사는 하지 않습니다.
    inline float rsqrt(float f){return std::sqrt(1.0f/f); }
    /// @brief 주어진 float의 역수의 근삿값을 리턴합니다. 0 검사는 하지 않습니다.
    inline float fastReciprocal(float f){ return 1.0f/f; }
    /// @brief 배열의 앞 4개를 역수로 바꿉니다.
    inline void fastReciprocal4(float* vec){ vec[0]=1/vec[0]; vec[1]=1/vec[1]; vec[2]=1/vec[2]; vec[3]=1/vec[3]; }
    /// @brief float 4개를 int 4개로 바꿉니다. (버림 적용)
    inline void float2int32(const float* val, int32_t* vec){
        vec[0]=(int32_t)(val[0]);
        vec[1]=(int32_t)(val[1]);
        vec[2]=(int32_t)(val[2]);
        vec[3]=(int32_t)(val[3]);
    }

    /// @brief 배열의 앞 4개 성분을 템플릿 인수로 주어진 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(float* vec){ swizzle4<float, P0, P1, P2, P3>(vec); }

    /// @brief 배열의 앞 4개 성분을 템플릿 인수로 주어진 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(double* vec){ swizzle4<double, P0, P1, P2, P3>(vec); }

    /// @brief 배열의 앞 4개 성분을 템플릿 인수로 주어진 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(int32_t* vec){ swizzle4<int32_t, P0, P1, P2, P3>(vec); }

    /// @brief 배열의 앞 4개 성분을 템플릿 인수로 주어진 대로 섞습니다.
    template<SWIZZLE_SYMBOL P0, SWIZZLE_SYMBOL P1, SWIZZLE_SYMBOL P2, SWIZZLE_SYMBOL P3>
    inline void swizzle4(uint32_t* vec){ swizzle4<uint32_t, P0, P1, P2, P3>(vec); }
#endif

}

#endif