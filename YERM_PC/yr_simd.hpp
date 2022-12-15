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

    /// @brief 역제곱근을 리턴합니다.
    inline double rsqrt(double d) { return 1.0 / sqrt(d); }

#ifndef YR_NOSIMD
#if BOOST_HW_SIMD_X86 >= BOOST_HW_SIMD_X86_SSE2_VERSION
#define YR_USING_SIMD
#include <emmintrin.h>
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

    /// @brief float 배열 앞 4개를 절댓값으로 바꿉니다.
    template<>
    inline void abs4<double>(double* vec){
        __m128d m = _mm_loadu_pd(vec);
		constexpr int64_t imask = 0x7fffffffffffffff;
		m = _mm_and_pd(m, _mm_set1_pd(*(double*)&imask));
		_mm_storeu_pd(vec, m);
    }

    /// @brief float 배열 앞 4개를 절댓값이 같은 음수로 바꿉니다.
    template<>
    inline void mabs4<double>(double* vec){
        __m128d m = _mm_loadu_pd(vec);
		m = _mm_or_pd(m, _mm_set1_pd(-0.0));
		_mm_storeu_pd(vec, m);
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


#elif BOOST_HW_SIMD_ARM >= BOOST_HW_SIMD_ARM_NEON_VERSION
#define YR_USING_SIMD
#include <arm_neon.h>

    /*
    AArch64에서는 unaligned load, store를 별도의 intrinsic으로 지원하지 않으며 정렬되지 않아도 작동 자체는 하는데, 성능 차이는 클 수도 크지 않을 수도 있음
    https://community.arm.com/support-forums/f/dev-platforms-forum/8806/loads-and-stores-for-unaligned-memory-addresses
    원문:
    You're right, there are only generic load and store instructions. Whether alignment is handled in hardware is a function of the underlying Memory Type of the addresses you're trying to access ("Normal" memory can handle unaligned accesses), and the current processor state (SCTLR_ELx.A might cause exceptions for unaligned accesses). In that sense you can just use unaligned addresses and things should, in general, work.
    You may notice a performance difference, however, since the microarchitectural implementation of an unaligned load or store is handled differently across processors, and it also depends on the capabilities of the fabric connecting your processor to memory. But, whether source and destination addresses for loads and stores are aligned or not, it will be the same instruction to access it. Only a few 'atomic' instructions (with memory model semantics that disallow being broken up or misaligned even on "Normal" memory) require aligned addresses.
    */

    /// @brief float 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<float32_t>(float32_t* vec, float32_t val){
        float32x4_t b = vdupq_n_f32(val);
        vst1q_f32(vec,b);
    }

    /// @brief double 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<float64_t>(float64_t* vec, float64_t val) {
        float64x2_t b = vdupq_n_f64(val);
        vst1q_f64(vec, b);
        vst1q_f64(vec + 2, b);
    }

    /// @brief int32_t 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<int32_t>(int32_t* vec, int32_t val){
        int32x4_t b = vdupq_n_s32(val);
        vst1q_s32(vec, b);
    }

    /// @brief uint32_t 배열의 앞 4개를 주어진 값으로 초기화합니다.
    template<>
    inline void set4<uint32_t>(uint32_t* vec, uint32_t val){
        uint32x4_t b = vdupq_n_u32(val);
        vst1q_u32(vec, b);
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<float32_t>(float32_t* vec, float32_t val){
        // ld1q에 _ex를 붙이면 align assert가 들어가는 것 같음
        // 더 intrinsic스러운 걸로 neon_faddq32가 있음. 심지어 이쪽은 매크로고 그쪽은 매크로가 아님
        vst1q_f32(vec, vaddq_n_f32(vld1q_f32(vec), val));
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<float32_t>(float32_t* vec, const float32_t* val){
        vst1q_f32(vec, vaddq_f32(vld1q_f32(val),vld1q_f32(vec)));
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<float64_t>(float64_t* vec, float64_t val){
        vst1q_f64(vec, vaddq_n_f64(vld1q_f64(vec), val));
        vst1q_f64(vec+2, vaddq_n_f64(vld1q_f64(vec+2), val));
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<float64_t>(float64_t* vec, const float64_t* val){
        vst1q_f64(vec, vaddq_f64(vld1q_f64(val),vld1q_f64(vec)));
        vst1q_f64(vec+2, vaddq_f64(vld1q_f64(val+2),vld1q_f64(vec+2)));
    }

    /// @brief int32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<int32_t>(int32_t* vec, int32_t val) {
        vst1q_s32(vec, vaddq_n_s32(vld1q_s32(vec), val));
    }

    /// @brief int32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<int32_t>(int32_t* vec, const int32_t* val) {
        vst1q_s32(vec, vaddq_s32(vld1q_s32(val), vld1q_s32(vec)));
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<uint32_t>(uint32_t* vec, uint32_t val) {
        vst1q_u32(vec, vaddq_n_u32(vld1q_u32(vec), val));
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void add4<uint32_t>(uint32_t* vec, const uint32_t* val) {
        vst1q_u32(vec, vaddq_u32(vld1q_u32(val), vld1q_u32(vec)));
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void sub4<float32_t>(float32_t* vec, float32_t val){
        vst1q_f32(vec, vsubq_n_f32(vld1q_f32(vec), val));
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void sub4<float32_t>(float32_t* vec, const float32_t* val){
        vst1q_f32(vec, vsubq_f32(vld1q_f32(vec),vld1q_f32(val)));
    }

    /// @brief double 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<float64_t>(float64_t* vec, float64_t val){
        vst1q_f64(vec, vsubq_n_f64(vld1q_f64(vec), val));
        vst1q_f64(vec+2, vsubq_n_f64(vld1q_f64(vec+2), val));
    }

    /// @brief double 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<float64_t>(float64_t* vec, const float64_t* val){
        vst1q_f64(vec, vsubq_f64(vld1q_f64(val),vld1q_f64(vec)));
        vst1q_f64(vec+2, vsubq_f64(vld1q_f64(val+2),vld1q_f64(vec+2)));
    }

    /// @brief int32_t 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<int32_t>(int32_t* vec, int32_t val) {
        vst1q_s32(vec, vsubq_n_s32(vld1q_s32(vec), val));
    }

    /// @brief int32_t 배열의 앞 4개에서 주어진 값을 뺍니다.
    template<>
    inline void sub4<int32_t>(int32_t* vec, const int32_t* val) {
        vst1q_s32(vec, vsubq_s32(vld1q_s32(vec), vld1q_s32(val)));
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void sub4<uint32_t>(uint32_t* vec, uint32_t val) {
        vst1q_u32(vec, vsubq_n_u32(vld1q_u32(vec), val));
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 누적합니다.
    template<>
    inline void sub4<uint32_t>(uint32_t* vec, const uint32_t* val) {
        vst1q_u32(vec, vsubq_u32(vld1q_u32(vec), vld1q_u32(val)));
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<float32_t>(float32_t* vec, float32_t val){
        vst1q_f32(vec, vmulq_n_f32(vld1q_f32(vec), val));
    }

    /// @brief float 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<float32_t>(float32_t* vec, const float32_t* val){
        vst1q_f32(vec, vmulq_f32(vld1q_f32(vec),vld1q_f32(val)));
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<float64_t>(float64_t* vec, float64_t val){
        vst1q_f64(vec, vmulq_n_f64(vld1q_f64(vec), val));
        vst1q_f64(vec+2, vmulq_n_f64(vld1q_f64(vec+2), val));
    }

    /// @brief double 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<float64_t>(float64_t* vec, const float64_t* val){
        vst1q_f64(vec, vmulq_f64(vld1q_f64(vec),vld1q_f64(val)));
        vst1q_f64(vec+2, vmulq_f64(vld1q_f64(vec+2),vld1q_f64(val+2)));
    }

        /// @brief int32_t 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<int32_t>(int32_t* vec, int32_t val) {
        vst1q_s32(vec, vmulq_n_s32(vld1q_s32(vec), val));
    }

    /// @brief int32_t 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<int32_t>(int32_t* vec, const int32_t* val) {
        vst1q_s32(vec, vmulq_s32(vld1q_s32(vec), vld1q_s32(val)));
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<uint32_t>(uint32_t* vec, uint32_t val) {
        vst1q_u32(vec, vmulq_n_u32(vld1q_u32(vec), val));
    }

    /// @brief uint32_t 배열의 앞 4개에 주어진 값을 곱합니다.
    template<>
    inline void mul4<uint32_t>(uint32_t* vec, const uint32_t* val) {
        vst1q_u32(vec, vmulq_u32(vld1q_u32(vec), vld1q_u32(val)));
    }

    /// @brief float 배열의 앞 4개를 주어진 값으로 나눕니다.
    template<>
    inline void div4<float32_t>(float32_t* vec, float32_t val){
        vst1q_f32(vec, vdivq_n_f32(vld1q_f32(vec), val));
    }

    /// @brief float 배열의 앞 4개를 주어진 값으로 나눕니다.
    template<>
    inline void div4<float32_t>(float32_t* vec, const float32_t* val){
        vst1q_f32(vec, vdivq_f32(vld1q_f32(vec),vld1q_f32(val)));
    }

    /// @brief double 배열의 앞 4개를 주어진 값으로 나눕니다.
    template<>
    inline void div4<float64_t>(float64_t* vec, float64_t val){
        vst1q_f64(vec, vdivq_n_f64(vld1q_f64(vec), val));
        vst1q_f64(vec+2, vdivq_n_f64(vld1q_f64(vec+2), val));
    }

    /// @brief double 배열의 앞 4개를 주어진 값으로 나눕니다.
    template<>
    inline void div4<float64_t>(float64_t* vec, const float64_t* val){
        vst1q_f64(vec, vdivq_f64(vld1q_f64(vec),vld1q_f64(val)));
        vst1q_f64(vec+2, vdivq_f64(vld1q_f64(vec+2),vld1q_f64(val+2)));
    }

    /// @brief float 배열 앞 4개를 절댓값으로 바꿉니다.
    template<>
    inline void abs4<float32_t>(float32_t* vec){
        vst1q_f32(vec, vabsq_f32(vld1q_f32(vec)));
    }

    /// @brief double 배열 앞 4개를 절댓값으로 바꿉니다.
    template<>
    inline void abs4<float64_t>(float64_t* vec){
        vst1q_f64(vec, vabsq_f64(vld1q_f64(vec)));
        vst1q_f64(vec+2, vabsq_f64(vld1q_f64(vec+2)));
    }

    /// @brief int32_t 배열 앞 4개를 절댓값으로 바꿉니다.
    template<>
    inline void abs4<int32_t>(int32_t* vec){
        vst1q_s32(vec, vabsq_s32(vld1q_s32(vec)));
    }

    /// @brief float 배열 앞 4개를 절댓값이 같은 음수로 바꿉니다.
    template<>
    inline void mabs4<float32_t>(float32_t* vec){
        vst1q_f32(vec, vmulq_n_f32(vabsq_f32(vld1q_f32(vec)), -1.0f));
    }

    /// @brief double 배열 앞 4개를 절댓값이 같은 음수로 바꿉니다.
    template<>
    inline void mabs4<float64_t>(float64_t* vec){
        vst1q_f64(vec, vmulq_n_f64(vabsq_f64(vld1q_f64(vec)), -1.0f));
        vst1q_f64(vec+2, vmulq_n_f64(vabsq_f64(vld1q_f64(vec+2)), -1.0f));
    }

    /// @brief int32_t 배열 앞 4개를 절댓값이 같은 음수로 바꿉니다.
    template<>
    inline void mabs4<int32_t>(int32_t* vec){
        vst1q_s32(vec, vmulq_n_s32(vabsq_s32(vld1q_s32(vec)),-1));
    }

        
    /// @brief float 배열에서 앞 4개를 양의 제곱근을 적용한 값으로 바꿉니다. 음수 검사는 하지 않고 nan으로 변합니다.
    inline void sqrt4(float32_t* vec){
        vst1q_f32(vec, vsqrtq_f32(vld1q_f32(vec)));
    }

    /// @brief float 배열에서 앞 4개를 양의 제곱근의 역수의 근삿값으로 바꿉니다. 0, 음수 검사는 하지 않고 nan이나 inf로 변합니다.
    inline void rsqrt4(float32_t* vec){
        vst1q_f32(vec, vrsqrteq_f32(vld1q_f32(vec))); // vrsqrtsq가 조금 더 많은 과정을 거쳐 조금 더 가까운 값을 얻는 모양임.
    }

    /// @brief double 배열에서 앞 4개를 양의 제곱근을 적용한 값으로 바꿉니다. 음수 검사는 하지 않고 nan으로 변합니다.
    inline void sqrt4(double* vec){
        vst1q_f64(vec, vsqrtq_f64(vld1q_f64(vec)));
        vst1q_f64(vec+2, vsqrtq_f64(vld1q_f64(vec+2)));
    }

    /// @brief 실수 배열에서 앞 4개에 양의 제곱근을 적용한 값으로 조금 더 빠르지만 정밀도는 더 낮게 바꿉니다. 음수 및 0 검사는 하지 않고 nan 혹은 inf로 변합니다.
    inline void fastSqrt4(float* vec){
        vst1q_f32(vec, vmulq_f32(vld1q_f32(vec), vrsqrteq_f32(vld1q_f32(vec))));
    }

    /// @brief 주어진 float의 역제곱근을 리턴합니다. 제곱근보다 빠르지만 오차가 있을 수 있습니다.
    inline float32_t rsqrt(float f){
        return vrsqrtes_f32(f); // TODO: 설명을 보면 제곱근의 근사라고 되어 있는데 이름은 역제곱근임. 둘 중 실제로 뭐가 맞는지 알아보기
    }

    /// @brief 주어진 float의 역수의 근삿값을 리턴합니다. 0 검사는 하지 않습니다. 1/f보다 빠르지만 그보다 오차가 클 수 있습니다.
    inline float32_t fastReciprocal(float32_t f){
        return vrecpes_f32(f);
    }

    /// @brief float 배열에서 앞 4개를 역수로 바꿉니다. 1에서 나누는 것보다 빠르지만 오차가 더 클 수 있으며, 0 검사는 하지 않습니다.
    inline void fastReciprocal4(float32_t* vec){
        vst1q_f32(vec, vrecpeq_f32(vld1q_f32(vec)));
    }

    /// @brief float 4개를 int 4개로 바꿉니다. (버림 적용)
    inline void float2int32(const float* val, int32_t* vec){
        vst1q_s32(vec, vcvtq_s32_f32(vld1q_f32(val)));
    }

    /// @brief 2바이트 정수 배열에 다른 배열의 대응 상분을 더합니다. 오버플로는 발생하지 않으며 값을 넘어갈 경우 최대/최솟값으로 고정됩니다.
    /// @param vec 누적 대상
    /// @param val 누적할 값
    /// @param size 누적할 개수
    inline void addsAll(int16_t* vec, const int16_t* val, size_t size){
        size_t i;
		for (i = 8; i < size; i += 8) {
            int16x8_t ves = vld1q_s16(vec + i - 8);
            int16x8_t ver = vld1q_s16(val + i - 8);
            ves = vqaddq_s16(ves, ver); // saturated 합
            vst1q_s16(vec + i - 8, ves);
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
		size_t i;
		for (i = 8; i < size; i += 8) {
            int16x8_t ves = vld1q_s16(vec + i - 8);
            ves = vqdmulhq_n_s16(ves, v2);
            ves = vshlq_n_s16(ves, 1);
            vst1q_s16(vec + i - 8, ves);
		}
		for (i -= 8; i < size; i++) {
			vec[i] = (int16_t)((float)vec[i] * val);
		}
	}

#endif
#endif

#ifndef YR_USING_SIMD
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
        vec[0]=const_cast<int32_t>(val[0]);
        vec[1]=const_cast<int32_t>(val[1]);
        vec[2]=const_cast<int32_t>(val[2]);
        vec[3]=const_cast<int32_t>(val[3]);
    }
#endif

}

#endif