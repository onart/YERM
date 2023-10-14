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
#ifndef __YR_MATH_HPP__
#define __YR_MATH_HPP__

#include "logger.hpp"
#include "yr_simd.hpp"
#include "yr_align.hpp"
#include "yr_compiler_specific.hpp"

#include <cstring>
#include <type_traits>
#include <cmath>
#include <limits>
#include <iostream>

namespace onart{

    struct Quaternion;
    template<class T> struct v128 { using RG_T = T[16 / sizeof(T)]; };
    template<> struct v128<float> { using RG_T = float128; };
    template<> struct v128<double> { using RG_T = double128; };
    template<> struct v128<int32_t> { using RG_T = int128; };
    template<> struct v128<uint32_t> { using RG_T = uint128; };

    template<class T, std::enable_if_t<std::is_floating_point_v<T>,bool> = true> constexpr inline T PI = (T)3.14159265358979323846;
    /// @brief 2~4차원 벡터입니다. 길이에 관계없이 상호 변환이 가능합니다. 타입은 float, int32_t, uint32_t만 사용할 수 있습니다. double은 준비중입니다.
    /// @tparam T 성분의 타입입니다. 사칙연산 및 부호 반전이 가능해야 합니다.
    /// @tparam D 벡터의 차원수입니다. 2~4차원만 사용할 수 있습니다.
    template<class T, unsigned D>
    struct alignas(16) nvec: public align16{
        using RG_T = typename v128<T>::RG_T;
        union{
            RG_T rg;
            T entry[4];
            struct { T x, y, z, w; };
            struct { T s, t, p, q; };
            struct { T r, g, b, a; };
        };
        /// @brief 영벡터를 초기화합니다.
        inline nvec() :rg(load(T{})) {}
        /// @brief SSE2 또는 그렇게 보이는 NEON 벡터를 이용해 초기화합니다.
        inline nvec(RG_T rg) : rg(rg) {}
        /// @brief 벡터의 모든 성분을 하나의 값으로 초기화합니다.
        explicit inline nvec(T a):rg(load(a)) { static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); }
        /// @brief 벡터의 값 중 앞 2~4개를 초기화합니다.
        inline nvec(T x, T y, T z = 0, T w = 0): rg(load(x,y,z,w)) { static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); }
        /// @brief 벡터를 복사합니다.
        inline nvec(const nvec &v): rg(v.rg) { }
        /// @brief 배열을 이용하여 벡터를 생성합니다.
        explicit inline nvec(const T* v): rg(load(v)) { static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); }
        /// @brief 한 차원 낮은 벡터와 나머지 한 성분을 이어붙여 벡터를 생성합니다.
        inline nvec(const nvec<T,D-1>& v, T a): rg(v.rg) { static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); entry[D-1] = a; }

        /// @brief 벡터의 모든 성분을 하나의 값으로 초기화합니다.
        inline void set(T a) { rg = load(a); }
        /// @brief 다른 벡터의 값을 복사해 옵니다. 차원수는 달라도 됩니다.
        template <unsigned E> inline void set(const nvec<T, E>& v) { rg = v.rg; }

        /// @brief 벡터의 모든 성분을 하나의 값으로 초기화합니다.
        inline nvec &operator=(T a) { set(a); return *this; }
        /// @brief 다른 벡터의 값을 복사해 옵니다. 차원수는 달라도 됩니다.
        template<unsigned E> inline nvec &operator=(const nvec<T, E>& v) { set(v); return *this; }

        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec& operator+=(const nvec& v) { rg = add(rg, v.rg); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec& operator-=(const nvec& v) { rg = sub(rg, v.rg); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec& operator*=(const nvec& v) { rg = mul(rg, v.rg); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec& operator/=(const nvec& v) { rg = div(rg, v.rg); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator+(const nvec &v) const { return nvec(*this) += v; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator-(const nvec &v) const { return nvec(*this) -= v; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator*(const nvec &v) const { return nvec(*this) *= v; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator/(const nvec &v) const { return nvec(*this) /= v; }

        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec& operator+=(T a) { rg = add(rg,load(a)); return *this; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec& operator-=(T a) { rg = sub(rg,load(a)); return *this; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec& operator*=(T a) { rg = mul(rg,load(a)); return *this; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec& operator/=(T a) { rg = div(rg,load(a)); return *this; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec operator+(T a) const { return nvec(*this) += a; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec operator-(T a) const { return nvec(*this) -= a; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec operator*(T a) const { return nvec(*this) *= a; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec operator/(T a) const { return nvec(*this) /= a; }

        /// @brief 벡터의 모든 성분이 동일한 경우 참을 리턴합니다.        
        inline bool operator==(const nvec &v) const {
            if constexpr(!std::is_floating_point_v<T>){
                return memcmp(entry, v.entry, sizeof(T) * D) == 0;
            }
            else {
                RG_T rtemp = mabs(sub(rg, v.rg));
                alignas(16) T atemp[4]; store(rtemp, atemp);
                constexpr T NEG_EPSILON = -std::numeric_limits<T>::epsilon();

                bool b2 = atemp[0] >= NEG_EPSILON && atemp[1] >= NEG_EPSILON;
                if constexpr(D > 2) b2 = b2 && atemp[2] >= NEG_EPSILON;
                if constexpr(D > 3) b2 = b2 && atemp[3] >= NEG_EPSILON;
                return b2;
            }
        }

        /// @brief 벡터의 모든 성분이 동일한 경우가 아니라면 참을 리턴합니다.
        inline bool operator!=(const nvec &v) const { return !operator==(v); }

        /// @brief 부호를 반전시켜 리턴합니다.
        inline nvec operator-() const { return nvec(neg(rg)); }
        /// @brief 배열과 같은 용도의 인덱스 연산자입니다.        
        inline T &operator[](unsigned i) { assert(i < D); return entry[i]; }
        /// @brief 배열과 같은 용도의 인덱스 연산자입니다.        
        inline const T &operator[](unsigned i) const { assert(i < D); return entry[i]; }

        /// @brief 다른 벡터와의 내적을 리턴합니다.
        inline T dot(const nvec& v) const {
            nvec nv(*this * v);
            if constexpr(D == 2) return nv[0] + nv[1];
            else if constexpr(D == 3) return nv[0] + nv[1] + nv[2];
            else if constexpr(D == 4) return (nv[0] + nv[1]) + (nv[2] + nv[3]);
            else return T();
        }

        /// @brief 벡터 간의 내적을 리턴합니다.
        friend inline T dot(const nvec& v1, const nvec& v2) { return v1.dot(v2); }

        /// @brief 벡터 길이의 제곱을 리턴합니다.
        inline T length2() const { return dot(*this); }

        /// @brief 벡터 길이를 리턴합니다.
        template <class U=T, std::enable_if_t<std::is_floating_point_v<U>,bool> = true> inline T length() const { return std::sqrt(length2()); }
        
        /// @brief 벡터의 방향을 유지하고 길이를 1로 맞춘 것을 리턴합니다.
        template <class U=T, std::enable_if_t<std::is_floating_point_v<U>,bool> = true> inline nvec normal() const { return (*this) * rsqrt(length2()); }

        /// @brief 벡터를 단위벡터로 바꿉니다.
        inline void normalize() { operator*=(rsqrt(length2())); }

        /// @brief 다른 벡터와의 차의 크기의 제곱을 리턴합니다.
        inline T distance2(const nvec &v) const { return (*this - v).length2(); }

        /// @brief 다른 벡터와의 차의 크기의 제곱을 리턴합니다.
        template <class U=T, std::enable_if_t<std::is_floating_point_v<U>,bool> = true> inline T distance(const nvec &v) const { T d2 = distance2(v); return d2 * rsqrt(d2); }

        /// @brief 디버그 출력을 위한 스트림 함수입니다.
        friend inline std::ostream& operator<<(std::ostream& os, const nvec& v) {
            if constexpr(D == 2) { return os << '[' << v[0] << ' ' << v[1] << ']'; }
            else if constexpr(D == 3) { return os <<'[' << v[0] << ' ' << v[1] << ' ' << v[2] << ']'; }
            else if constexpr(D == 4) { return os <<'[' << v[0] << ' ' << v[1] << ' ' <<  v[2] << ' ' << v[3] << ']'; }
            else return os;
        }
        /// @brief 벡터와 스칼라 간의 추가 연산 오버로딩입니다.
        friend inline nvec operator+(T f, const nvec &v) { return v + f; }
        /// @brief 벡터와 스칼라 간의 추가 연산 오버로딩입니다.
        friend inline nvec operator*(T f, const nvec &v) { return v * f; }

        /// @brief 2개 벡터의 성분별 선형 보간을 리턴합니다.
        /// @param a 보간 대상 1(t=0에 가까울수록 이 벡터에 가깝습니다.)
        /// @param b 보간 대상 2(t=1에 가까울수록 이 벡터에 가깝습니다.)
        /// @param t 선형 보간 값
        template <class U=T, std::enable_if_t<std::is_floating_point_v<U>,bool> = true> friend inline nvec lerp(const nvec &a, const nvec &b, const nvec &t) { return a * (1 - t) + b * t; }

        /// @brief 2개 벡터의 선형 보간을 리턴합니다.
        /// @param a 보간 대상 1(t=0에 가까울수록 이 벡터에 가깝습니다.)
        /// @param b 보간 대상 2(t=1에 가까울수록 이 벡터에 가깝습니다.)
        /// @param t 선형 보간 값
        template <class U=T, std::enable_if_t<std::is_floating_point_v<U>,bool> = true> friend inline nvec lerp(const nvec &a, const nvec &b, T t) { return a * (1 - t) + b * t; }

#define DIM_LOWER_BOUND(N) template <unsigned U=D, std::enable_if_t<U >= N,bool> = true>
        /// @brief GLSL식의 swizzle 인터페이스입니다.
        DIM_LOWER_BOUND(1) inline nvec<T, 2> xx() const { return nvec<T, 2>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 2> xy() const { return nvec<T, 2>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 2> xz() const { return nvec<T, 2>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 2> xw() const { return nvec<T, 2>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 2> yx() const { return nvec<T, 2>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 2> yy() const { return nvec<T, 2>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 2> yz() const { return nvec<T, 2>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 2> yw() const { return nvec<T, 2>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 2> zx() const { return nvec<T, 2>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 2> zy() const { return nvec<T, 2>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 2> zz() const { return nvec<T, 2>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 2> zw() const { return nvec<T, 2>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 2> wx() const { return nvec<T, 2>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 2> wy() const { return nvec<T, 2>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 2> wz() const { return nvec<T, 2>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 2> ww() const { return nvec<T, 2>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }

        DIM_LOWER_BOUND(1) inline nvec<T, 3> xxx() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 3> xxy() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> xxz() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> xxw() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 3> yxx() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 3> yxy() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> yxz() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> yxw() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zxx() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zxy() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zxz() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> zxw() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wxx() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wxy() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wxz() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wxw() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 3> xyx() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 3> xyy() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> xyz() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> xyw() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 3> yyx() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 3> yyy() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> yyz() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> yyw() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zyx() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zyy() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zyz() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> zyw() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wyx() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wyy() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wyz() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wyw() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> xzx() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> xzy() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> xzz() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> xzw() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> yzx() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> yzy() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> yzz() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> yzw() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zzx() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zzy() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 3> zzz() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> zzw() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wzx() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wzy() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wzz() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wzw() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> xwx() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> xwy() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> xwz() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> xww() const { return nvec<T, 3>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> ywx() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> ywy() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> ywz() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> yww() const { return nvec<T, 3>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> zwx() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> zwy() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> zwz() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> zww() const { return nvec<T, 3>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wwx() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wwy() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> wwz() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 3> www() const { return nvec<T, 3>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }

        DIM_LOWER_BOUND(1) inline nvec<T, 4> xxxx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> xxxy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xxxz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xxxw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> xyxx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> xyxy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xyxz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xyxw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzxx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzxy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzxz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xzxw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwxx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwxy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwxz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwxw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> xxyx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> xxyy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xxyz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xxyw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> xyyx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> xyyy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xyyz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xyyw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzyx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzyy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzyz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xzyw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwyx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwyy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwyz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwyw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xxzx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xxzy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xxzz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xxzw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xyzx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xyzy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xyzz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xyzw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzzx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzzy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> xzzz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xzzw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwzx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwzy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwzz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwzw() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xxwx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xxwy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xxwz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xxww() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_X, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xywx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xywy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xywz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xyww() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xzwx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xzwy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xzwz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xzww() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwwx() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwwy() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwwz() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> xwww() const { return nvec<T, 4>(swizzle<SWIZZLE_X, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W>(rg)); }

        DIM_LOWER_BOUND(2) inline nvec<T, 4> yxxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> yxxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yxxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yxxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> yyxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> yyxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yyxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yyxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yzxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> yxyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> yxyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yxyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yxyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> yyyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(2) inline nvec<T, 4> yyyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yyyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yyyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yzyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yxzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yxzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yxzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yxzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yyzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yyzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yyzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yyzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> yzzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yzzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yxwx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yxwy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yxwz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yxww() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yywx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yywy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yywz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yyww() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yzwx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yzwy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yzwz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> yzww() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywwx() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywwy() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywwz() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> ywww() const { return nvec<T, 4>(swizzle<SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W>(rg)); }

        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zxxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zyxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zzxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwxx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwxy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwxz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwxw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zxyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zyyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zzyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwyx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwyy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwyz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwyw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zxzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zxzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zyzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zyzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(3) inline nvec<T, 4> zzzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zzzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwzx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwzy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwzz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwzw() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zxwx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zxwy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zxwz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zxww() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zywx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zywy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zywz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zyww() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zzwx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zzwy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zzwz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zzww() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwwx() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwwy() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwwz() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> zwww() const { return nvec<T, 4>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W>(rg)); }

        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxxx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxxy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxxz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxxw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyxx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyxy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyxz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyxw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzxx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzxy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzxz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzxw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwxx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_X, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwxy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwxz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwxw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_X, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxyx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxyy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxyz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxyw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyyx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyyy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyyz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyyw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzyx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzyy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzyz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzyw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwyx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwyy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwyz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwyw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxzx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxzy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxzz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxzw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyzx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyzy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyzz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyzw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzzx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzzy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzzz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzzw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwzx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwzy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwzz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwzw() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxwx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxwy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxwz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wxww() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_X, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wywx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wywy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wywz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wyww() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Y, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzwx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzwy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzwz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wzww() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_W>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwwx() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_X>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwwy() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Y>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwwz() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_Z>(rg)); }
        DIM_LOWER_BOUND(4) inline nvec<T, 4> wwww() const { return nvec<T, 4>(swizzle<SWIZZLE_W, SWIZZLE_W, SWIZZLE_W, SWIZZLE_W>(rg)); }

#undef DIM_LOWER_BOUND
    };

    using vec2 = nvec<float, 2>; using vec3 = nvec<float, 3>; using vec4 = nvec<float, 4>;
    using ivec2 = nvec<int32_t, 2>; using ivec3 = nvec<int32_t, 3>; using ivec4 = nvec<int32_t, 4>;
    using uvec2 = nvec<uint32_t, 2>; using uvec3 = nvec<uint32_t, 3>; using uvec4 = nvec<uint32_t, 4>;
    using dvec2 = nvec<double, 2>; using dvec3 = nvec<double, 3>; using dvec4 = nvec<double, 4>;

    /// @brief float 벡터의 값을 버림하여 int 벡터로 전환하여 리턴합니다.
    template<unsigned D> nvec<int, D> f2i(const nvec<float, D>& iv) { nvec<int, D> ret; float2int32(iv.entry, ret.entry); return ret; }

    /// @brief 2개의 2차원 실수 벡터 외적의 z축 성분을 계산합니다.
    inline float cross2(const vec2& a, const vec2& b) { vec2 temp = a * b.yx(); return temp[0] - temp[1]; }
    /// @brief 2개의 2차원 실수 벡터 외적의 z축 성분을 계산합니다.
    inline double cross2(const dvec2 &a, const dvec2 &b) { return a[0] * b[1] - a[1] * b[0]; }

    /// @brief 2개의 3차원 실수 벡터의 외적을 계산합니다.
    inline vec3 cross(const vec3& a, const vec3& b){
        vec3 mul = a * b.yzx() - b * a.yzx();
        return mul.yzx();
    }

    /// @brief 2개의 3차원 실수 벡터의 외적을 계산합니다.
    //inline dvec3 cross(const dvec3& a, const dvec3& b){
        //dvec3 mul = a * b.yzx() - b * a.yzx();
        //return mul.yzx();
    //}

    /// @brief 2개 단위벡터의 구면선형보간을 리턴합니다.
    /// @param a 구면 선형 보간 대상 1(t=0에 가까울수록 이 벡터에 가깝습니다.)
    /// @param b 구면 선형 보간 대상 2(t=1에 가까울수록 이 벡터에 가깝습니다.)
    /// @param t 구면 선형 보간 값
    inline vec3 slerp(const vec3 a, const vec3& b, float t){
        float sinx = cross(a, b).length();
        float theta = std::asin(sinx);
        if (theta <= std::numeric_limits<float>::epsilon()) return a;
        return a * std::sin(theta * (1 - t)) + b * std::sin(theta * t);
    }

    /// @brief 2개 단위벡터의 구면선형보간을 리턴합니다.
    /// @param a 구면 선형 보간 대상 1(t=0에 가까울수록 이 벡터에 가깝습니다.)
    /// @param b 구면 선형 보간 대상 2(t=1에 가까울수록 이 벡터에 가깝습니다.)
    /// @param t 구면 선형 보간 값
    //inline dvec3 slerp(const dvec3 a, const dvec3& b, double t){
        //double sinx = cross(a, b).length();
        //double theta = std::asin(sinx);
        //if (theta <= std::numeric_limits<double>::epsilon()) return a;
        //return a * std::sin(theta * (1 - t)) + b * std::sin(theta * t);
    //}

    /// @brief 행 우선 순서로 구성된 2x2 행렬입니다.
    struct alignas(16) mat2: public align16 {
        union{
            float a[4];
            struct{ float _11,_12,_21,_22; };
        };
        /// @brief 행 우선 순서로 매개변수를 주어 행렬을 생성합니다.
        inline mat2(float _11,float _12,float _21, float _22): a{_11,_12,_21,_22}{ }
        /// @brief 복사생성자입니다.
        inline mat2(const mat2 &m) { memcpy(a, m.a, sizeof(a)); }

        /// @brief 인덱스 연산자는 행 우선 순서로 일자로 접근할 수 있습니다. 행과 열을 따로 주고자 하면 at() 함수를 이용해 주세요.
        inline float &operator[](unsigned i) { assert(i < 4); return a[i]; }
        /// @brief 인덱스 연산자는 행 우선 순서로 일자로 접근할 수 있습니다. 행과 열을 따로 주고자 하면 at() 함수를 이용해 주세요.
        inline const float &operator[](unsigned i) const { assert(i < 4); return a[i]; }
        /// @brief row행 col열 성분의 참조를 리턴합니다. (0 베이스)
        inline float &at(unsigned row, unsigned col) { assert(row < 2 && col < 2); return a[row * 2 + col]; }
        /// @brief row행 col열 성분의 참조를 리턴합니다. (0 베이스)
        inline const float& at(unsigned row, unsigned col) const { assert(row < 2 && col < 2); return a[row * 2 + col]; }
        /// @brief 단위행렬로 바꿉니다.
        inline void toI() { a[0] = a[3] = 1.0f; a[1] = a[2] = 0; }
        
        /// @brief 다른 행렬과 연산합니다.
        inline mat2 &operator+=(const mat2 &m) { add4(a, m.a); return *this; }
        /// @brief 다른 행렬과 연산합니다.
        inline mat2 &operator-=(const mat2 &m) { sub4(a, m.a); return *this; }
        /// @brief 다른 행렬과 연산합니다.
        inline mat2 operator+(const mat2& m) const { return mat2(*this) += m; }
        /// @brief 다른 행렬과 연산합니다.
        inline mat2 operator-(const mat2& m) const { return mat2(*this) -= m; }
        /// @brief 행렬끼리 곱합니다.
        inline mat2 operator*(const mat2& m) const {
            return mat2(
                a[0] * m[0] + a[1] * m[2],
                a[0] * m[1] + a[1] * m[3],

                a[2] * m[0] + a[3] * m[2],
                a[2] * m[1] + a[3] * m[3]
            );
        }
        /// @brief 행렬끼리 곱합니다.
        inline mat2 &operator*=(const mat2 &m) { return *this = operator*(m); }

        /// @brief 벡터에 선형변환을 적용한 것을 리턴합니다.
        inline vec2 operator*(const vec2& v) const { return vec2(a[0] * v[0] + a[1] * v[1], a[2] * v[0] + a[3] * v[1]); }

        /// @brief 행렬에 실수배를 합니다.
        inline mat2 &operator*=(float f) { mul4(a, f); return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat2 &operator/=(float f) { div4(a, f); return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat2 operator*(float f) const { return mat2(*this) *= f; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat2 operator/(float f) const { return mat2(*this) /= f; }

        /// @brief 행렬식을 리턴합니다.
        inline float det() const { return a[0] * a[3] - a[1] * a[2]; }

        /// @brief 수반 행렬을 리턴합니다.
        inline mat2 adjugate() const { return mat2(a[3], -a[1], -a[2], a[0]); }

        /// @brief 역행렬을 리턴합니다.
        inline mat2 inverse() const{
            float d = det();
            if (d == 0) LOGWITH(this,": no inverse?");
            return adjugate() / d;
        }
        /// @brief 전치 행렬을 리턴합니다. 
        inline mat2 transpose() const { return mat2(_11, _21, _12, _22); }

        /// @brief 디버그 출력을 위한 스트림 함수입니다.
        friend inline std::ostream& operator<<(std::ostream& os, const mat2& v) {
            return os << '[' << v._11 << ' ' << v._12 << "]\n"
                        << '[' << v._21 << ' ' << v._22 << ']';
        }
    };

    /// @brief 행 우선 순서의 3x3 행렬입니다. mat3은 9개의 float 변수를 갖지만 16 배수 정렬에 의해 실제로는 48바이트를 차지하니 주의하세요.
    struct alignas(16) mat3: public align16{
        union{
            float a[9];
            struct { float _11,_12,_13,_21,_22,_23,_31,_32,_33; };
        };
        /// @brief 단위행렬을 생성합니다.
        inline mat3() : a{} { _11 = _22 = _33 = 1.0f; }
        /// @brief 행 우선 순서로 매개변수를 주어 행렬을 생성합니다.
        inline mat3(float _11,float _12,float _13,float _21,float _22,float _23,float _31,float _32,float _33):a { _11,_12,_13,_21,_22,_23,_31,_32,_33 } { }
        /// @brief 복사 생성자입니다.
        inline mat3(const mat3 &m) { memcpy(a, m.a, sizeof(a)); }
        /// @brief 행 우선 순서로 성분에 접근합니다.
        inline float &operator[](unsigned i) { assert(i < 9); return a[i]; }
        /// @brief 행 우선 순서로 성분에 접근합니다.
        inline const float &operator[](unsigned i) const { assert(i < 9); return a[i]; }
        /// @brief row행 col열 원소의 참조를 리턴합니다. (0 베이스)
        inline float &at(unsigned row, unsigned col) { assert(row<3 && col<3); return a[row * 3 + col]; }
        /// @brief row행 col열 원소의 참조를 리턴합니다. (0 베이스)
        inline const float &at(unsigned row, unsigned col) const { assert(row<3 && col<3); return a[row * 3 + col]; }
        /// @brief 단위행렬로 바꿉니다.
        inline void toI() { memset(a, 0, sizeof(a)); _11=_22=_33=1.0f; }
        /// @brief 다른 행렬과 연산합니다.
        inline mat3 &operator+=(const mat3 &m) { add4(a, m.a); add4(a+4, m.a+4); a[8]+=m.a[8]; return *this; }
        /// @brief 다른 행렬과 연산합니다.
        inline mat3 &operator-=(const mat3 &m) { sub4(a, m.a); sub4(a+4, m.a+4); a[8]-=m.a[8]; return *this; }
        /// @brief 다른 행렬과 연산합니다.
        inline mat3 operator+(const mat3 &m) { return mat3(*this) += m; }
        /// @brief 다른 행렬과 연산합니다.
        inline mat3 operator-(const mat3 &m) { return mat3(*this) -= m; }
        /// @brief i행 벡터를 리턴합니다. 0~2만 입력 가능합니다. 
        inline vec3 row(size_t i) const { assert(i <= 2); return vec3(a + i * 3); }
        /// @brief i열 벡터를 리턴합니다. 0~2만 입력 가능합니다. 
        inline vec3 col(size_t i) const { assert(i <= 2); return vec3(a[i], a[i + 3], a[i + 6]); }
        
        /// @brief 행렬곱을 수행합니다.
        inline mat3 operator*(const mat3& m) const {
            mat3 ret;
            for (int i = 0, ent = 0; i < 3; i++){
                vec3 r = row(i);
                for (int j = 0; j < 3; j++,ent++){
                    vec3 c = m.col(j);
                    ret[ent] = r.dot(c);
                }
            }
            return ret;
        }
        /// @brief 행렬곱을 수행합니다.
        inline mat3 &operator*=(const mat3 &m) { return *this = operator*(m); }
        /// @brief 벡터에 선형변환을 적용하여 리턴합니다.
        inline vec3 operator*(const vec3& v) const { return vec3(row(0).dot(v), row(1).dot(v), row(2).dot(v)); }
        /// @brief 행렬에 실수배를 합니다.
        inline mat3 &operator*=(float f) { mul4(a, f); mul4(a + 4, f); a[8] *= f; return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat3 &operator/=(float f) { div4(a, f); div4(a + 4, f); a[8] /= f; return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat3 operator*(float f) const { return mat3(*this) *= f; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat3 operator/(float f) const { return mat3(*this) /= f; }

        /// @brief 행렬식을 리턴합니다.
        inline float det() const { 
            return _11 * (_22 * _33 - _23 * _32) + _12 * (_23 * _31 - _21 * _33) + _13 * (_21 * _32 - _22 * _31);
        }

        /// @brief 대각 성분의 합을 리턴합니다.
        inline float trace() const { return _11 + _22 + _33; }

        /// @brief 수반 행렬을 리턴합니다.
        inline mat3 adjugate() const {
            return mat3(
                (_22 * _33 - _32 * _23), (_13 * _32 - _12 * _33), (_12 * _23 - _13 * _22),
                (_23 * _31 - _21 * _33), (_11 * _33 - _13 * _31), (_21 * _13 - _11 * _23),
                (_21 * _32 - _31 * _22), (_31 * _12 - _11 * _32), (_11 * _22 - _21 * _12)
            );
        }

        /// @brief 역행렬을 리턴합니다.
        inline mat3 inverse() const {
            float d = det();
            if(d == 0) LOGWITH(this,": no inverse?");
            return adjugate() / d;
        }

        /// @brief 전치 행렬을 리턴합니다.
        inline mat3 transpose() const { return mat3(_11, _21, _31, _12, _22, _32, _13, _23, _33); }

        /// @brief 좌측 상단 2x2 행렬로 캐스트합니다.
        inline operator mat2() { return mat2(_11, _12, _21, _22); }

        /// @brief 2차원 병진 행렬을 계산합니다.
        inline static mat3 translate(const vec2 &t) { return mat3(1, 0, t.x, 0, 1, t.y, 0, 0, 1); }
        /// @brief 2차원 병진 행렬을 계산합니다.
        inline static mat3 translate(float x, float y) { return mat3(1, 0, x, 0, 1, y, 0, 0, 1); }
        /// @brief 2차원 크기 변환 행렬을 계산합니다.
        inline static mat3 scale(const vec2 &t) { return mat3(t.x, 0, 0, 0, t.y, 1, 0, 0, 1); }
        /// @brief 2차원 크기 변환 행렬을 계산합니다.
        inline static mat3 scale(float x, float y) { return mat3(x, 0, 0, 0, y, 1, 0, 0, 1); }
        /// @brief Z축 기준의 2차원 회전을 리턴합니다. 이와 다른 회전을 원하는 경우 3x3이 아닌 4x4 변환을 사용해야 합니다.
        inline static mat3 rotate(float z) { return mat3(std::cos(z),-std::sin(z), 0, std::sin(z), std::cos(z), 0, 0, 0, 1); }
        /// @brief 3차원 오일러 회전에 의한 행렬을 리턴합니다. 
        /// @param roll X축 방향 회전
        /// @param pitch Y축 방향 회전
        /// @param yaw Z축 방향 회전
        inline static mat3 rotate(float roll, float pitch, float yaw);

        /// @brief 디버그 출력을 위한 스트림 함수입니다.
        friend inline std::ostream& operator<<(std::ostream& os, const mat3& v) {
            return os << '[' << v._11 << ' ' << v._12 << ' ' << v._13 << "]\n"
                    << '[' << v._21 << ' ' << v._22 << ' ' << v._23 << "]\n"
                    << '[' << v._31 << ' ' << v._32 <<  ' ' << v._33 << ']';
        }
    };

    /// @brief 열 우선 순서의 3x3 행렬입니다. mat3은 9개의 float 변수를 갖지만 16 배수 정렬에 의해 실제로는 48바이트를 차지하니 주의하세요.
    struct alignas(16) cmat3: public align16{
        union{
            float a[9];
            struct { float _11,_21,_31,_12,_22,_32,_13,_23,_33; };
        };
        /// @brief 단위행렬을 생성합니다.
        inline cmat3() : a{} { _11 = _22 = _33 = 1.0f; }
        /// @brief 행 우선 순서로 매개변수를 주어 행렬을 생성합니다.
        inline cmat3(float _11,float _21,float _31,float _12,float _22,float _32,float _13,float _23,float _33):a { _11,_21,_31,_12,_22,_32,_13,_23,_33 } { }
        /// @brief 복사 생성자입니다.
        inline cmat3(const cmat3 &m) { memcpy(a, m.a, sizeof(a)); }
        /// @brief 열 우선 순서로 성분에 접근합니다.
        inline float &operator[](unsigned i) { assert(i < 9); return a[i]; }
        /// @brief 열 우선 순서로 성분에 접근합니다.
        inline const float &operator[](unsigned i) const { assert(i < 9); return a[i]; }
        /// @brief row행 col열 원소의 참조를 리턴합니다. (0 베이스)
        inline float &at(unsigned row, unsigned col) { assert(row<3 && col<3); return a[col * 3 + row]; }
        /// @brief row행 col열 원소의 참조를 리턴합니다. (0 베이스)
        inline const float &at(unsigned row, unsigned col) const { assert(row<3 && col<3); return a[col * 3 + row]; }
        /// @brief 단위행렬로 바꿉니다.
        inline void toI() { memset(a, 0, sizeof(a)); _11=_22=_33=1.0f; }
        /// @brief 다른 행렬과 연산합니다.
        inline cmat3 &operator+=(const cmat3 &m) { add4(a, m.a); add4(a+4, m.a+4); a[8]+=m.a[8]; return *this; }
        /// @brief 다른 행렬과 연산합니다.
        inline cmat3 &operator-=(const cmat3 &m) { sub4(a, m.a); sub4(a+4, m.a+4); a[8]-=m.a[8]; return *this; }
        /// @brief 다른 행렬과 연산합니다.
        inline cmat3 operator+(const cmat3 &m) { return cmat3(*this) += m; }
        /// @brief 다른 행렬과 연산합니다.
        inline cmat3 operator-(const cmat3 &m) { return cmat3(*this) -= m; }
        /// @brief i행 벡터를 리턴합니다. 0~2만 입력 가능합니다. 
        inline vec3 row(size_t i) const { assert(i <= 2); return vec3(a[i], a[i + 3], a[i + 6]); }
        /// @brief i열 벡터를 리턴합니다. 0~2만 입력 가능합니다. 
        inline vec3 col(size_t i) const { assert(i <= 2); return vec3(a + i * 3); }
        
        /// @brief 행렬곱을 수행합니다.
        inline cmat3 operator*(const cmat3& m) const {
            cmat3 ret;
            for (int i = 0, ent = 0; i < 3; i++){
                vec3 c = col(i);
                for (int j = 0; j < 3; j++,ent++){
                    vec3 r = m.row(j);
                    ret[ent] = c.dot(r);
                }
            }
            return ret;
        }
        /// @brief 행렬곱을 수행합니다.
        inline cmat3 &operator*=(const cmat3 &m) { return *this = operator*(m); }
        /// @brief 벡터에 선형변환을 적용하여 리턴합니다.
        inline vec3 operator*(const vec3& v) const { return vec3(row(0).dot(v), row(1).dot(v), row(2).dot(v)); }
        /// @brief 행렬에 실수배를 합니다.
        inline cmat3 &operator*=(float f) { mul4(a, f); mul4(a + 4, f); a[8] *= f; return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline cmat3 &operator/=(float f) { div4(a, f); div4(a + 4, f); a[8] /= f; return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline cmat3 operator*(float f) const { return cmat3(*this) *= f; }
        /// @brief 행렬에 실수배를 합니다.
        inline cmat3 operator/(float f) const { return cmat3(*this) /= f; }

        /// @brief 행렬식을 리턴합니다.
        inline float det() const { 
            return _11 * (_22 * _33 - _23 * _32) + _12 * (_23 * _31 - _21 * _33) + _13 * (_21 * _32 - _22 * _31);
        }

        /// @brief 대각 성분의 합을 리턴합니다.
        inline float trace() const { return _11 + _22 + _33; }

        /// @brief 수반 행렬을 리턴합니다.
        inline cmat3 adjugate() const {
            return cmat3(
                (_22 * _33 - _32 * _23), (_23 * _31 - _21 * _33), (_21 * _32 - _31 * _22),
                (_13 * _32 - _12 * _33), (_11 * _33 - _13 * _31), (_31 * _12 - _11 * _32),
                (_12 * _23 - _13 * _22), (_21 * _13 - _11 * _23), (_11 * _22 - _21 * _12)
            );
        }

        /// @brief 역행렬을 리턴합니다.
        inline cmat3 inverse() const {
            float d = det();
            if(d == 0) LOGWITH(this,": no inverse?");
            return adjugate() / d;
        }

        /// @brief 전치 행렬을 리턴합니다.
        inline cmat3 transpose() const { return cmat3(_11, _12, _13, _21, _22, _23, _31, _32, _33); }

        /// @brief 좌측 상단 2x2 행렬로 캐스트합니다.
        inline operator mat2() { return mat2(_11, _12, _21, _22); }

        /// @brief 2차원 병진 행렬을 계산합니다.
        inline static cmat3 translate(const vec2 &t) { return cmat3(1, 0, 0, 0, 1, 0, t.x, t.y, 1); }
        /// @brief 2차원 병진 행렬을 계산합니다.
        inline static cmat3 translate(float x, float y) { return cmat3(1, 0, 0, 0, 1, 0, x, y, 1); }
        /// @brief 2차원 크기 변환 행렬을 계산합니다.
        inline static cmat3 scale(const vec2 &t) { return cmat3(t.x, 0, 0, 0, t.y, 1, 0, 0, 1); }
        /// @brief 2차원 크기 변환 행렬을 계산합니다.
        inline static cmat3 scale(float x, float y) { return cmat3(x, 0, 0, 0, y, 1, 0, 0, 1); }
        /// @brief Z축 기준의 2차원 회전을 리턴합니다. 이와 다른 회전을 원하는 경우 3x3이 아닌 4x4 변환을 사용해야 합니다.
        inline static cmat3 rotate(float z) { return cmat3(std::cos(z),std::sin(z), 0, -std::sin(z), std::cos(z), 0, 0, 0, 1); }
        /// @brief 3차원 오일러 회전에 의한 행렬을 리턴합니다. 
        /// @param roll X축 방향 회전
        /// @param pitch Y축 방향 회전
        /// @param yaw Z축 방향 회전
        inline static cmat3 rotate(float roll, float pitch, float yaw);

        /// @brief 디버그 출력을 위한 스트림 함수입니다. 주의: 이 행렬은 열 우선 순서 행렬이지만, 출력은 행 우선 순서로 됩니다.
        friend inline std::ostream& operator<<(std::ostream& os, const cmat3& v) {
            return os << '[' << v._11 << ' ' << v._12 << ' ' << v._13 << "]\n"
                    << '[' << v._21 << ' ' << v._22 << ' ' << v._23 << "]\n"
                    << '[' << v._31 << ' ' << v._32 <<  ' ' << v._33 << ']';
        }
    };

    /// @brief 열 우선 순서의 4x4 행렬입니다.
    struct alignas(16) mat4: public align16{
        union{
            float a[16];
            struct{ float _11,_12,_13,_14,_21,_22,_23,_24,_31,_32,_33,_34, _41,_42,_43,_44; };
        };
        /// @brief 단위행렬을 생성합니다.
        inline mat4() { memset(a, 0, sizeof(a)); _11 = _22 = _33 = _44 = 1.0f; }
        /// @brief 행 우선 순서로 매개변수를 주어 행렬을 생성합니다.
        inline mat4(float _11, float _12, float _13, float _14, float _21, float _22, float _23, float _24, float _31, float _32, float _33, float _34, float _41, float _42, float _43, float _44)
            :a{ _11,_12,_13,_14,_21,_22,_23,_24,_31,_32,_33,_34,_41,_42,_43,_44 } { }
        /// @brief 행렬의 성분을 복사해서 생성합니다.
        inline mat4(const mat4 &m) { memcpy(a, m.a, sizeof(a)); }

        /// @brief 행 우선 순서로 성분에 접근합니다. 행과 열을 따로 주고자 한다면 at() 함수를 이용해 주세요.
        inline float &operator[](unsigned i) { assert(i<16); return a[i]; }
        /// @brief 행 우선 순서로 성분에 접근합니다. 행과 열을 따로 주고자 한다면 at() 함수를 이용해 주세요.
        inline const float &operator[](unsigned i) const { assert(i<16); return a[i]; }

        /// @brief row행 col열 성분의 참조를 리턴합니다. (0 베이스)
        inline float &at(unsigned row, unsigned col) { assert(row < 4 && col < 4); return a[row * 4 + col]; }
        /// @brief row행 col열 성분의 참조를 리턴합니다. (0 베이스)
        inline const float &at(unsigned row, unsigned col) const { assert(row < 4 && col < 4); return a[row * 4 + col]; }
        /// @brief 단위행렬로 바꿉니다.
        inline void toI() { memset(a, 0, sizeof(a)); _11 = _22 = _33 = _44 = 1.0f; }
        /// @brief 단위행렬이면 true를 리턴합니다.
        inline bool isI() const { return memcmp(mat4().a, a, sizeof(a)) == 0; }

        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline mat4& operator+=(const mat4& m) { add4(a, m.a); add4(a + 4, m.a + 4); add4(a + 8, m.a + 8); add4(a + 12, m.a + 12); return *this; }
        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline mat4& operator-=(const mat4& m) { sub4(a, m.a); sub4(a + 4, m.a + 4); sub4(a + 8, m.a + 8); sub4(a + 12, m.a + 12); return *this; }
        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline mat4 operator+(const mat4& m) const { return mat4(*this) += m; }
        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline mat4 operator-(const mat4& m) const { return mat4(*this) -= m; }
        /// @brief n행 벡터를 리턴합니다. (0 베이스)
        inline vec4 row(size_t i) const { assert(i < 4); return vec4(a + 4 * i); }
        /// @brief n열 벡터를 리턴합니다. (0 베이스)
        inline vec4 col(size_t i) const { assert(i < 4); return vec4(a[i], a[i + 4], a[i + 8], a[i + 12]); }

        /// @brief 행렬끼리 곱합니다.
        inline mat4 operator*(const mat4& m) const{
            mat4 ret;
            for (int i = 0, ent = 0; i < 4; i++) {
                vec4 r = row(i);
                for (int j = 0; j < 4; j++, ent++) {
                    vec4 c = m.col(j);
                    ret[ent] = r.dot(c);
                }
            }
            return ret;
        }

        /// @brief 행렬끼리 곱합니다.
        inline mat4& operator*=(const mat4& m) { return *this = operator*(m); }
        /// @brief 벡터에 선형변환을 적용하여 리턴합니다.
        inline vec4 operator*(const vec4& v) const{ return vec4(row(0).dot(v), row(1).dot(v), row(2).dot(v), row(3).dot(v)); }

        /// @brief 행렬에 실수배를 합니다.
        inline mat4& operator*=(float f) { mulAll(a, f, 16); return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat4 operator*(float f) const { mat4 r(*this); r *= f; return r; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat4& operator/=(float f) { divAll(a, f, 16); return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline mat4 operator/(float f) const { mat4 r(*this); r /= f; return r; }

        /// @brief 행렬식을 반환합니다.
        inline float det() const {
            return 
                _41 * _32 * _23 * _14 - _31 * _42 * _23 * _14 - _41 * _22 * _33 * _14 + _21 * _42 * _33 * _14 +
                _31 * _22 * _43 * _14 - _21 * _32 * _43 * _14 - _41 * _32 * _13 * _24 + _31 * _42 * _13 * _24 +
                _41 * _12 * _33 * _24 - _11 * _42 * _33 * _24 - _31 * _12 * _43 * _24 + _11 * _32 * _43 * _24 +
                _41 * _22 * _13 * _34 - _21 * _42 * _13 * _34 - _41 * _12 * _23 * _34 + _11 * _42 * _23 * _34 +
                _21 * _12 * _43 * _34 - _11 * _22 * _43 * _34 - _31 * _22 * _13 * _44 + _21 * _32 * _13 * _44 +
                _31 * _12 * _23 * _44 - _11 * _32 * _23 * _44 - _21 * _12 * _33 * _44 + _11 * _22 * _33 * _44;
        }
        /// @brief 행렬의 대각 성분 합을 리턴합니다.
        inline float trace() const { return _11 + _22 + _33 + _44; }
        /// @brief 좌측 상단 3x3 행렬로 캐스트합니다.
        inline operator mat3() const { return mat3(_11, _12, _13, _21, _22, _23, _31, _32, _33); }
        /// @brief 행렬이 아핀 변환인 경우 역행렬을 조금 더 효율적으로 구합니다.
        inline mat4 affineInverse() const {
            //https://stackoverflow.com/questions/2624422/efficient-4x4-matrix-inverse-affine-transform
            mat3 ir(mat3(*this).inverse());
            vec3 p = ir * -vec3(col(3).entry);
            return mat4(
                ir[0], ir[1], ir[2], p[0],
                ir[3], ir[4], ir[5], p[1],
                ir[6], ir[7], ir[8], p[2],
                0, 0, 0, 1
            );
        }

        /// @brief 수반 행렬을 리턴합니다.
        inline mat4 adjugate() const {
            return mat4(
                (_32 * _43 * _24 - _42 * _33 * _24 + _42 * _23 * _34 - _22 * _43 * _34 - _32 * _23 * _44 + _22 * _33 * _44),
                (_42 * _33 * _14 - _32 * _43 * _14 - _42 * _13 * _34 + _12 * _43 * _34 + _32 * _13 * _44 - _12 * _33 * _44),
                (_22 * _43 * _14 - _42 * _23 * _14 + _42 * _13 * _24 - _12 * _43 * _24 - _22 * _13 * _44 + _12 * _23 * _44),
                (_32 * _23 * _14 - _22 * _33 * _14 - _32 * _13 * _24 + _12 * _33 * _24 + _22 * _13 * _34 - _12 * _23 * _34),

                (_41 * _33 * _24 - _31 * _43 * _24 - _41 * _23 * _34 + _21 * _43 * _34 + _31 * _23 * _44 - _21 * _33 * _44),
                (_31 * _43 * _14 - _41 * _33 * _14 + _41 * _13 * _34 - _11 * _43 * _34 - _31 * _13 * _44 + _11 * _33 * _44),
                (_41 * _23 * _14 - _21 * _43 * _14 - _41 * _13 * _24 + _11 * _43 * _24 + _21 * _13 * _44 - _11 * _23 * _44),
                (_21 * _33 * _14 - _31 * _23 * _14 + _31 * _13 * _24 - _11 * _33 * _24 - _21 * _13 * _34 + _11 * _23 * _34),

                (_31 * _42 * _24 - _41 * _32 * _24 + _41 * _22 * _34 - _21 * _42 * _34 - _31 * _22 * _44 + _21 * _32 * _44),
                (_41 * _32 * _14 - _31 * _42 * _14 - _41 * _12 * _34 + _11 * _42 * _34 + _31 * _12 * _44 - _11 * _32 * _44),
                (_21 * _42 * _14 - _41 * _22 * _14 + _41 * _12 * _24 - _11 * _42 * _24 - _21 * _12 * _44 + _11 * _22 * _44),
                (_31 * _22 * _14 - _21 * _32 * _14 - _31 * _12 * _24 + _11 * _32 * _24 + _21 * _12 * _34 - _11 * _22 * _34),

                (_41 * _32 * _23 - _31 * _42 * _23 - _41 * _22 * _33 + _21 * _42 * _33 + _31 * _22 * _43 - _21 * _32 * _43),
                (_31 * _42 * _13 - _41 * _32 * _13 + _41 * _12 * _33 - _11 * _42 * _33 - _31 * _12 * _43 + _11 * _32 * _43),
                (_41 * _22 * _13 - _21 * _42 * _13 - _41 * _12 * _23 + _11 * _42 * _23 + _21 * _12 * _43 - _11 * _22 * _43),
                (_21 * _32 * _13 - _31 * _22 * _13 + _31 * _12 * _23 - _11 * _32 * _23 - _21 * _12 * _33 + _11 * _22 * _33));
        }

        /// @brief 역행렬을 리턴합니다.
        inline mat4 inverse() const {
            float d = det();
            if(d == 0) LOGWITH(this,": no inverse?");
            return adjugate() / d;
        }
        /// @brief 전치 행렬을 리턴합니다.
        inline mat4 transpose() const {
            return mat4(_11, _21, _31, _41, _12, _22, _32, _42, _13, _23, _33, _43, _14, _24, _34, _44); 
        }

        /// @brief 3차원 병진 행렬을 계산합니다.
        inline static mat4 translate(const vec3& t) {
            return mat4(
                1, 0, 0, t[0],
                0, 1, 0, t[1],
                0, 0, 1, t[2],
                0, 0, 0, 1
            );
        }
        /// @brief 3차원 병진 행렬을 계산합니다.
        inline static mat4 translate(float x, float y, float z) {
            return mat4(
                1, 0, 0, x,
                0, 1, 0, y,
                0, 0, 1, z,
                0, 0, 0, 1
            );
        }
        /// @brief 3차원 크기 변환 행렬을 계산합니다.
        inline static mat4 scale(const vec3& t) {
            return mat4(
                t[0], 0, 0, 0,
                0, t[1], 0, 0,
                0, 0, t[2], 0,
                0, 0, 0, 1
            );
        }

        /// @brief 3차원 크기 변환 행렬을 계산합니다.
        inline static mat4 scale(float x, float y, float z) {
            return mat4(
                x, 0, 0, 0,
                0, y, 0, 0,
                0, 0, z, 0,
                0, 0, 0, 1
            );
        }
        /// @brief 3차원 회전 행렬을 계산합니다.
        /// @param axis 회전축
        /// @param angle 회전각
        inline static mat4 rotate(const vec3& axis, float angle);
        /// @brief 3차원 회전 행렬을 계산합니다.
        /// @param roll X축 방향 회전
        /// @param pitch Y축 방향 회전
        /// @param yaw Z축 방향 회전
        inline static mat4 rotate(float roll, float pitch, float yaw);
        /// @brief 3차원 회전 행렬을 계산합니다.
        /// @param q 회전 사원수
        inline static mat4 rotate(const Quaternion &q);
        /// @brief lookAt 형식의 뷰 행렬을 계산합니다.
        /// @param eye 카메라 위치
        /// @param at 피사체 위치
        /// @param up 위쪽 방향: 화면 상 위쪽 방향이 이 벡터의 방향을 어느 정도 따릅니다.
        inline static mat4 lookAt(const vec3& eye, const vec3& at, const vec3& up){
            vec3 n = (eye - at).normal();
            vec3 u = cross(up, n).normal();
            vec3 v = cross(n, u).normal();
            return mat4(
                    u[0], u[1], u[2], -(u.dot(eye)),
                    v[0], v[1], v[2], -(v.dot(eye)),
                    n[0], n[1], n[2], -(n.dot(eye)),
                    0, 0, 0, 1
                );
        }

        /// @brief 병진, 회전, 배율 행렬 T, R, S를 각각 구하여 곱하는 것보다 조금 더 빠르게 계산합니다.
        /// @param translation 병진
        /// @param rotation 회전
        /// @param scale 배율
        inline static mat4 TRS(const vec3 &translation, const Quaternion &rotation, const vec3 &scale);
        /// @brief 주어진 병진, 회전, 배율을 포함하는 아핀 변환의 역변환을 단순계산보다 조금 빠르게 계산합니다. 역변환이 없는 경우(ex: 배율에 영이 있음) 비정상적인 값이 리턴될 것입니다.
        /// @param translation 병진
        /// @param rotation 회전
        /// @param scale 배율
        inline static mat4 iTRS(const vec3 &translation, const Quaternion &rotation, const vec3 &scale);

        /// @brief Vulkan 표준 뷰 볼륨 직육면체에 들어갈 공간인 뿔대(절두체, frustum)를 조절하는 투사 행렬을 계산합니다. 이 행렬 적용 이전의 공간은 GL과 같은 것(위쪽: +y, 화면 안쪽: +z, 오른쪽: +x)으로 가정합니다.
        /// @param fovy 뿔대의 Y축 방향(화면 기준 세로) 라디안 각도입니다.
        /// @param aspect 표시 뷰포트 비율(가로/세로)입니다.
        /// @param dnear 뿔대에서 가장 가까운 거리입니다. 이 이전의 거리는 보이지 않습니다.
        /// @param dfar 뿔대에서 가장 먼 거리입니다. 이 너머의 거리는 보이지 않습니다. 
        /// TODO: Vulkan 적합성 검사
        inline static mat4 perspective(float fovy, float aspect, float dnear, float dfar){
            return mat4(
                1 / (aspect * tanf(fovy * 0.5f)), 0, 0, 0,
                0, -1 / std::tan(fovy * 0.5f), 0, 0,
                0, 0, (dnear + dfar) * 0.5f / (dnear - dfar) - 0.5f, (dnear * dfar) / (dnear - dfar),
                0, 0, -1, 0
            );
        }

        /// @brief 한 직사각형을 다른 직사각형으로 매핑하는 행렬을 계산합니다. z 좌표는 동일하다고 가정하여 xy 평면에서만 이동합니다.
        /// @param r1 변환 전 직사각형 (좌-하-폭-높이)
        /// @param r2 변환 후 직사각형 (좌-하-폭-높이)
        /// @param z 직사각형이 위치할 z좌표(0이 가장 겉)
        /// TODO: Vulkan 적합성 검사
        inline static mat4 r2r(const vec4& r1, const vec4& r2, float z=0){
            vec4 sc = r2 / r1;	vec4 tr = r2 - r1 * vec4(sc[2], sc[3]);
            return mat4(
                sc[2], 0, 0, tr[0],
                0, sc[3], 0, tr[1],
                0, 0, 1, z,
                0, 0, 0, 1
            );
        }

        /// @brief YERM의 2D 객체를 위한 단위 직사각형 (중심이 0,0이고 한 변의 길이가 1인 정사각형)을 다른 직사각형으로 변환하는 행렬을 계산합니다.
        /// @param r2 변환 후 직사각형 (좌-하-폭-높이)
        /// @param z 직사각형이 위치할 z좌표(0이 가장 겉)
        /// TODO: Vulkan 적합성 검사
        inline static mat4 r2r(const vec4& r2, float z = 0) {
            return r2r(vec4(-0.5f, -0.5f, 1, 1), r2, z);
        }

        /// @brief 한 직사각형(L-D-W-H 형식)을 다른 직사각형의 안쪽에 맞게 변환합니다. 즉 중심을 공유하며, 원본 직사각형의 종횡비는 유지하면서 가장 큰 직사각형이 되도록 리턴합니다.
        /// @param r1 변환 전 직사각형
        /// @param r2 변환 전 직사각형
        /// @param z 직사각형이 위치할 z좌표(-1이 가장 겉)
        /// TODO: Vulkan 적합성 검사
        inline static mat4 r2r2(const vec4& r1, const vec4& r2, float z = 0) {
            float r = r1[2] / r1[3];
            vec4 targ(r2);
            if (targ[2] < targ[3] * r) {	// 세로선을 맞출 것
                targ[1] += (targ[3] - targ[2] / r) / 2;
                targ[3] = targ[2] / r;
            }
            else {	// 가로선을 맞출 것
                targ[0] += (targ[2] - targ[3] * r) / 2;
                targ[2] = targ[3] * r;
            }
            return r2r(r1, targ, z);
        }

        /// @brief 디버그 출력을 위한 스트림 함수입니다.
        friend inline std::ostream& operator<<(std::ostream& os, const mat4& v) {
            return os << '[' << v._11 << ' ' << v._12 << ' ' << v._13 << ' ' << v._14 << "]\n"
                    << '[' << v._21 << ' ' << v._22 << ' ' << v._23 << ' ' << v._24 << "]\n"
                    << '[' << v._31 << ' ' << v._32 << ' ' << v._33 << ' ' << v._34 << "]\n"
                    << '[' << v._41 << ' ' << v._42 << ' ' << v._43 << ' ' << v._44 << ']';
        }
    };

    /// @brief 열 우선 순서의 4x4 행렬입니다.
    struct alignas(16) cmat4: public align16{
        union{
            float a[16];
            struct{ float _11,_21,_31,_41,_12,_22,_32,_42,_13,_23,_33,_43, _14,_24,_34,_44; };
        };
        /// @brief 단위행렬을 생성합니다.
        inline cmat4() { memset(a, 0, sizeof(a)); _11 = _22 = _33 = _44 = 1.0f; }
        /// @brief 열 우선 순서로 매개변수를 주어 행렬을 생성합니다.
        inline cmat4(float _11, float _21, float _31, float _41, float _12, float _22, float _32, float _42, float _13, float _23, float _33, float _43, float _14, float _24, float _34, float _44)
            :a{ _11,_21,_31,_41,_12,_22,_32,_42,_13,_23,_33,_43, _14,_24,_34,_44 } { }
        /// @brief 행렬의 성분을 복사해서 생성합니다.
        inline cmat4(const cmat4 &m) { memcpy(a, m.a, sizeof(a)); }

        /// @brief 열 우선 순서로 성분에 접근합니다. 행과 열을 따로 주고자 한다면 at() 함수를 이용해 주세요.
        inline float &operator[](unsigned i) { assert(i<16); return a[i]; }
        /// @brief 열 우선 순서로 성분에 접근합니다. 행과 열을 따로 주고자 한다면 at() 함수를 이용해 주세요.
        inline const float &operator[](unsigned i) const { assert(i<16); return a[i]; }

        /// @brief row행 col열 성분의 참조를 리턴합니다. (0 베이스)
        inline float &at(unsigned row, unsigned col) { assert(row < 4 && col < 4); return a[col * 4 + row]; }
        /// @brief row행 col열 성분의 참조를 리턴합니다. (0 베이스)
        inline const float &at(unsigned row, unsigned col) const { assert(row < 4 && col < 4); return a[col * 4 + row]; }
        /// @brief 단위행렬로 바꿉니다.
        inline void toI() { memset(a, 0, sizeof(a)); _11 = _22 = _33 = _44 = 1.0f; }
        /// @brief 단위행렬이면 true를 리턴합니다.
        inline bool isI() const { return memcmp(mat4().a, a, sizeof(a)) == 0; }

        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline cmat4& operator+=(const cmat4& m) { add4(a, m.a); add4(a + 4, m.a + 4); add4(a + 8, m.a + 8); add4(a + 12, m.a + 12); return *this; }
        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline cmat4& operator-=(const cmat4& m) { sub4(a, m.a); sub4(a + 4, m.a + 4); sub4(a + 8, m.a + 8); sub4(a + 12, m.a + 12); return *this; }
        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline cmat4 operator+(const cmat4& m) const { return cmat4(*this) += m; }
        /// @brief 다른 행렬과 성분별로 연산합니다.
        inline cmat4 operator-(const cmat4& m) const { return cmat4(*this) -= m; }
        /// @brief n행 벡터를 리턴합니다. (0 베이스)
        inline vec4 row(size_t i) const { assert(i < 4); return vec4(a[i], a[i + 4], a[i + 8], a[i + 12]); }
        /// @brief n열 벡터를 리턴합니다. (0 베이스)
        inline vec4 col(size_t i) const { assert(i < 4); return vec4(a + 4 * i); }

        /// @brief 행렬끼리 곱합니다.
        inline cmat4 operator*(const cmat4& m) const{
            cmat4 ret;
            for (int i = 0, ent = 0; i < 4; i++) {
                vec4 c = col(i);
                for (int j = 0; j < 4; j++, ent++) {
                    vec4 r = m.row(j);
                    ret[ent] = c.dot(r);
                }
            }
            return ret;
        }

        /// @brief 행렬끼리 곱합니다.
        inline cmat4& operator*=(const cmat4& m) { return *this = operator*(m); }
        /// @brief 벡터에 선형변환을 적용하여 리턴합니다.
        inline vec4 operator*(const vec4& v) const{ return vec4(row(0).dot(v), row(1).dot(v), row(2).dot(v), row(3).dot(v)); }

        /// @brief 행렬에 실수배를 합니다.
        inline cmat4& operator*=(float f) { mulAll(a, f, 16); return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline cmat4 operator*(float f) const { cmat4 r(*this); r *= f; return r; }
        /// @brief 행렬에 실수배를 합니다.
        inline cmat4& operator/=(float f) { divAll(a, f, 16); return *this; }
        /// @brief 행렬에 실수배를 합니다.
        inline cmat4 operator/(float f) const { cmat4 r(*this); r /= f; return r; }

        /// @brief 행렬식을 반환합니다.
        inline float det() const {
            return 
                _41 * _32 * _23 * _14 - _31 * _42 * _23 * _14 - _41 * _22 * _33 * _14 + _21 * _42 * _33 * _14 +
                _31 * _22 * _43 * _14 - _21 * _32 * _43 * _14 - _41 * _32 * _13 * _24 + _31 * _42 * _13 * _24 +
                _41 * _12 * _33 * _24 - _11 * _42 * _33 * _24 - _31 * _12 * _43 * _24 + _11 * _32 * _43 * _24 +
                _41 * _22 * _13 * _34 - _21 * _42 * _13 * _34 - _41 * _12 * _23 * _34 + _11 * _42 * _23 * _34 +
                _21 * _12 * _43 * _34 - _11 * _22 * _43 * _34 - _31 * _22 * _13 * _44 + _21 * _32 * _13 * _44 +
                _31 * _12 * _23 * _44 - _11 * _32 * _23 * _44 - _21 * _12 * _33 * _44 + _11 * _22 * _33 * _44;
        }
        /// @brief 행렬의 대각 성분 합을 리턴합니다.
        inline float trace() const { return _11 + _22 + _33 + _44; }
        /// @brief 좌측 상단 3x3 행렬로 캐스트합니다.
        inline operator cmat3() const { return cmat3(_11, _21, _31, _12, _22, _32, _13, _23, _33); }
        /// @brief 행렬이 아핀 변환인 경우 역행렬을 조금 더 효율적으로 구합니다.
        inline cmat4 affineInverse() const {
            //https://stackoverflow.com/questions/2624422/efficient-4x4-matrix-inverse-affine-transform
            cmat3 ir(cmat3(*this).inverse());
            vec3 p = ir * -vec3(col(3).entry);
            return cmat4(
                ir[0], ir[3], ir[6], 0,
                ir[1], ir[4], ir[7], 0,
                ir[2], ir[5], ir[8], 0,
                p[0], p[1], p[2], 1
            );
        }

        /// @brief 수반 행렬을 리턴합니다.
        inline cmat4 adjugate() const {
            return cmat4(
                (_32 * _43 * _24 - _42 * _33 * _24 + _42 * _23 * _34 - _22 * _43 * _34 - _32 * _23 * _44 + _22 * _33 * _44),
                (_41 * _33 * _24 - _31 * _43 * _24 - _41 * _23 * _34 + _21 * _43 * _34 + _31 * _23 * _44 - _21 * _33 * _44),
                (_31 * _42 * _24 - _41 * _32 * _24 + _41 * _22 * _34 - _21 * _42 * _34 - _31 * _22 * _44 + _21 * _32 * _44),
                (_41 * _32 * _23 - _31 * _42 * _23 - _41 * _22 * _33 + _21 * _42 * _33 + _31 * _22 * _43 - _21 * _32 * _43),

                (_42 * _33 * _14 - _32 * _43 * _14 - _42 * _13 * _34 + _12 * _43 * _34 + _32 * _13 * _44 - _12 * _33 * _44),
                (_31 * _43 * _14 - _41 * _33 * _14 + _41 * _13 * _34 - _11 * _43 * _34 - _31 * _13 * _44 + _11 * _33 * _44),
                (_41 * _32 * _14 - _31 * _42 * _14 - _41 * _12 * _34 + _11 * _42 * _34 + _31 * _12 * _44 - _11 * _32 * _44),
                (_31 * _42 * _13 - _41 * _32 * _13 + _41 * _12 * _33 - _11 * _42 * _33 - _31 * _12 * _43 + _11 * _32 * _43),

                (_22 * _43 * _14 - _42 * _23 * _14 + _42 * _13 * _24 - _12 * _43 * _24 - _22 * _13 * _44 + _12 * _23 * _44),
                (_41 * _23 * _14 - _21 * _43 * _14 - _41 * _13 * _24 + _11 * _43 * _24 + _21 * _13 * _44 - _11 * _23 * _44),
                (_21 * _42 * _14 - _41 * _22 * _14 + _41 * _12 * _24 - _11 * _42 * _24 - _21 * _12 * _44 + _11 * _22 * _44),
                (_41 * _22 * _13 - _21 * _42 * _13 - _41 * _12 * _23 + _11 * _42 * _23 + _21 * _12 * _43 - _11 * _22 * _43),

                (_32 * _23 * _14 - _22 * _33 * _14 - _32 * _13 * _24 + _12 * _33 * _24 + _22 * _13 * _34 - _12 * _23 * _34),
                (_21 * _33 * _14 - _31 * _23 * _14 + _31 * _13 * _24 - _11 * _33 * _24 - _21 * _13 * _34 + _11 * _23 * _34),
                (_31 * _22 * _14 - _21 * _32 * _14 - _31 * _12 * _24 + _11 * _32 * _24 + _21 * _12 * _34 - _11 * _22 * _34),
                (_21 * _32 * _13 - _31 * _22 * _13 + _31 * _12 * _23 - _11 * _32 * _23 - _21 * _12 * _33 + _11 * _22 * _33));
        }

        /// @brief 역행렬을 리턴합니다.
        inline cmat4 inverse() const {
            float d = det();
            if(d == 0) LOGWITH(this,": no inverse?");
            return adjugate() / d;
        }
        /// @brief 전치 행렬을 리턴합니다.
        inline cmat4 transpose() const {
            return cmat4(_11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34, _41, _42, _43, _44);
        }

        /// @brief 3차원 병진 행렬을 계산합니다.
        inline static cmat4 translate(const vec3& t) {
            return cmat4(
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                t[0], t[1], t[2], 1
            );
        }
        /// @brief 3차원 병진 행렬을 계산합니다.
        inline static cmat4 translate(float x, float y, float z) {
            return cmat4(
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                x, y, z, 1
            );
        }
        /// @brief 3차원 크기 변환 행렬을 계산합니다.
        inline static cmat4 scale(const vec3& t) {
            return cmat4(
                t[0], 0, 0, 0,
                0, t[1], 0, 0,
                0, 0, t[2], 0,
                0, 0, 0, 1
            );
        }

        /// @brief 3차원 크기 변환 행렬을 계산합니다.
        inline static cmat4 scale(float x, float y, float z) {
            return cmat4(
                x, 0, 0, 0,
                0, y, 0, 0,
                0, 0, z, 0,
                0, 0, 0, 1
            );
        }
        /// @brief 3차원 회전 행렬을 계산합니다.
        /// @param axis 회전축
        /// @param angle 회전각
        inline static cmat4 rotate(const vec3& axis, float angle);
        /// @brief 3차원 회전 행렬을 계산합니다.
        /// @param roll X축 방향 회전
        /// @param pitch Y축 방향 회전
        /// @param yaw Z축 방향 회전
        inline static cmat4 rotate(float roll, float pitch, float yaw);
        /// @brief 3차원 회전 행렬을 계산합니다.
        /// @param q 회전 사원수
        inline static cmat4 rotate(const Quaternion &q);
        /// @brief lookAt 형식의 뷰 행렬을 계산합니다.
        /// @param eye 카메라 위치
        /// @param at 피사체 위치
        /// @param up 위쪽 방향: 화면 상 위쪽 방향이 이 벡터의 방향을 어느 정도 따릅니다.
        inline static cmat4 lookAt(const vec3& eye, const vec3& at, const vec3& up){
            vec3 n = (eye - at).normal();
            vec3 u = cross(up, n).normal();
            vec3 v = cross(n, u).normal();
            return cmat4(
                    u[0], v[0], n[0], 0,
                    u[1], v[1], n[1], 0,
                    u[2], v[2], n[2], 0,
                    -(u.dot(eye)), -(v.dot(eye)), -(n.dot(eye)), 1
                );
        }

        /// @brief 병진, 회전, 배율 행렬 T, R, S를 각각 구하여 곱하는 것보다 조금 더 빠르게 계산합니다.
        /// @param translation 병진
        /// @param rotation 회전
        /// @param scale 배율
        inline static cmat4 TRS(const vec3 &translation, const Quaternion &rotation, const vec3 &scale);
        /// @brief 주어진 병진, 회전, 배율을 포함하는 아핀 변환의 역변환을 단순계산보다 조금 빠르게 계산합니다. 역변환이 없는 경우(ex: 배율에 영이 있음) 비정상적인 값이 리턴될 것입니다.
        /// @param translation 병진
        /// @param rotation 회전
        /// @param scale 배율
        inline static cmat4 iTRS(const vec3 &translation, const Quaternion &rotation, const vec3 &scale);

        /// @brief Vulkan 표준 뷰 볼륨 직육면체에 들어갈 공간인 뿔대(절두체, frustum)를 조절하는 투사 행렬을 계산합니다. 이 행렬 적용 이전의 공간은 GL과 같은 것(위쪽: +y, 화면 안쪽: +z, 오른쪽: +x)으로 가정합니다.
        /// @param fovy 뿔대의 Y축 방향(화면 기준 세로) 라디안 각도입니다.
        /// @param aspect 표시 뷰포트 비율(가로/세로)입니다.
        /// @param dnear 뿔대에서 가장 가까운 거리입니다. 이 이전의 거리는 보이지 않습니다.
        /// @param dfar 뿔대에서 가장 먼 거리입니다. 이 너머의 거리는 보이지 않습니다. 
        /// TODO: Vulkan 적합성 검사
        inline static cmat4 perspective(float fovy, float aspect, float dnear, float dfar){
            return cmat4(
                1 / (aspect * tanf(fovy * 0.5f)), 0, 0, 0,
                0, -1 / std::tan(fovy * 0.5f), 0, 0,
                0, 0, (dnear + dfar) * 0.5f / (dnear - dfar) - 0.5f, -1,
                0, 0, (dnear * dfar) / (dnear - dfar), 0
            );
        }

        /// @brief 한 직사각형을 다른 직사각형으로 매핑하는 행렬을 계산합니다. z 좌표는 동일하다고 가정하여 xy 평면에서만 이동합니다.
        /// @param r1 변환 전 직사각형 (좌-하-폭-높이)
        /// @param r2 변환 후 직사각형 (좌-하-폭-높이)
        /// @param z 직사각형이 위치할 z좌표(0이 가장 겉)
        /// TODO: Vulkan 적합성 검사
        inline static cmat4 r2r(const vec4& r1, const vec4& r2, float z=0){
            vec4 sc = r2 / r1;	vec4 tr = r2 - r1 * vec4(sc[2], sc[3]);
            return cmat4(
                sc[2], 0, 0, 0,
                0, sc[3], 0, 0,
                0, 0, 1, 0,
                tr[0], tr[1], z, 1                
            );
        }

        /// @brief YERM의 2D 객체를 위한 단위 직사각형 (중심이 0,0이고 한 변의 길이가 1인 정사각형)을 다른 직사각형으로 변환하는 행렬을 계산합니다.
        /// @param r2 변환 후 직사각형 (좌-하-폭-높이)
        /// @param z 직사각형이 위치할 z좌표(0이 가장 겉)
        /// TODO: Vulkan 적합성 검사
        inline static cmat4 r2r(const vec4& r2, float z = 0) {
            return r2r(vec4(-0.5f, -0.5f, 1, 1), r2, z);
        }

        /// @brief 한 직사각형(L-D-W-H 형식)을 다른 직사각형의 안쪽에 맞게 변환합니다. 즉 중심을 공유하며, 원본 직사각형의 종횡비는 유지하면서 가장 큰 직사각형이 되도록 리턴합니다.
        /// @param r1 변환 전 직사각형
        /// @param r2 변환 전 직사각형
        /// @param z 직사각형이 위치할 z좌표(-1이 가장 겉)
        /// TODO: Vulkan 적합성 검사
        inline static cmat4 r2r2(const vec4& r1, const vec4& r2, float z = 0) {
            float r = r1[2] / r1[3];
            vec4 targ(r2);
            if (targ[2] < targ[3] * r) {	// 세로선을 맞출 것
                targ[1] += (targ[3] - targ[2] / r) / 2;
                targ[3] = targ[2] / r;
            }
            else {	// 가로선을 맞출 것
                targ[0] += (targ[2] - targ[3] * r) / 2;
                targ[2] = targ[3] * r;
            }
            return r2r(r1, targ, z);
        }

        /// @brief 디버그 출력을 위한 스트림 함수입니다. 주의: 출력은 행 우선 순서로 이루어집니다.
        friend inline std::ostream& operator<<(std::ostream& os, const cmat4& v) {
            return os << '[' << v._11 << ' ' << v._12 << ' ' << v._13 << ' ' << v._14 << "]\n"
                    << '[' << v._21 << ' ' << v._22 << ' ' << v._23 << ' ' << v._24 << "]\n"
                    << '[' << v._31 << ' ' << v._32 << ' ' << v._33 << ' ' << v._34 << "]\n"
                    << '[' << v._41 << ' ' << v._42 << ' ' << v._43 << ' ' << v._44 << ']';
        }
    }; 

    /// @brief 3차원 회전 등을 표현하는 사원수입니다. 1, i, j, k 부분에 해당하는 c1, ci, cj, ck 멤버를 가집니다. 각각 순서대로 일반적인 사원수 모듈의 w, x, y, z에 대응합니다.
    struct alignas(16) Quaternion: public align16{
        union {
            float128 rg;
            struct { float c1, ci, cj, ck; };
        };
        /// @brief 사원수를 생성합니다.
        inline Quaternion(float o = 1, float i = 0, float j = 0, float k = 0) : rg(load(o, i, j, k)) {  }
        explicit inline Quaternion(float128 rg):rg(rg){}
        /// @brief 각속도 벡터(초당 회전각(float) * 회전축(vec3))에 대응하는 사원수를 생성합니다.
        inline Quaternion(const vec3& av) :rg(swizzle<SWIZZLE_X,SWIZZLE_X,SWIZZLE_Y,SWIZZLE_Z>(av.rg)) { c1 = 0; }
        /// @brief 복사 생성자입니다.
        inline Quaternion(const Quaternion &q):rg(q.rg) { }

        /// @brief 사원수 크기의 제곱을 리턴합니다.
        inline float abs2() const { return reinterpret_cast<const vec4 *>(this)->length2(); }

        /// @brief 사원수 크기를 리턴합니다.
        inline float abs() const { return sqrtf(abs2()); }

        /// @brief 무회전 사원수인지 확인합니다.
        inline bool is1() const { return c1 == 1 && ci == 0 && cj == 0 && ck == 0; }

        /// @brief 켤레(공액)사원수를 리턴합니다.
        inline Quaternion conjugate() const { return Quaternion(toggleSigns<false, true, true, true>(rg)); }

        /// @brief 이 사원수의 우측에 곱해서 1이 되는 값을 리턴합니다.
        inline Quaternion inverse() const { return conjugate() / abs2(); }

        /// @brief 이 사원수의 우측에 곱해서 1이 되는 값을 리턴합니다. 조금 더 빠르지만 오차가 더 클 수 있습니다.
        inline Quaternion fastInverse() const { return conjugate() * fastReciprocal(abs2()); }

        /// @brief 사원수끼리 곱합니다. 교환 법칙이 성립하지 않는 점(순서를 바꾸면 방향이 반대가 됨)에 유의하세요.
        inline Quaternion operator*(const Quaternion& q) const {
            float128 q_c1 = mul(rg, load(c1));
            float128 q_ci = mul(toggleSigns<true, false, true, false>(swizzle<SWIZZLE_Y, SWIZZLE_X, SWIZZLE_W, SWIZZLE_Z>(rg)), load(ci));
            float128 q_cj = mul(toggleSigns<true, false, false, true>(swizzle<SWIZZLE_Z, SWIZZLE_W, SWIZZLE_X, SWIZZLE_Y>(rg)), load(cj));
            float128 q_ck = mul(toggleSigns<true, true, false, false>(swizzle<SWIZZLE_W, SWIZZLE_Z, SWIZZLE_Y, SWIZZLE_X>(rg)), load(ck));
            float128 res = add(add(q_c1, q_ci), add(q_cj, q_ck));
            
            return Quaternion(res);
        }

        /// @brief 사원수 간의 사칙 연산입니다.
        inline Quaternion& operator+=(const Quaternion& q) { rg = add(rg,q.rg); return *this; }
        /// @brief 사원수 간의 사칙 연산입니다.
        inline Quaternion& operator-=(const Quaternion& q) { rg = sub(rg,q.rg); return *this; }
        /// @brief 사원수 간의 사칙 연산입니다.
        inline Quaternion& operator*=(const Quaternion& q) { *this = *this * q; return *this; }	// multiplying on the left is more commonly used operation
        /// @brief 사원수 간의 사칙 연산입니다. / 연산자는 우측 피연산자에 대한 곱셈 역원을 곱합니다.
        inline Quaternion& operator/=(const Quaternion& q) { *this = *this * q.inverse(); return *this; }
        /// @brief 사원수에 실수배를 합니다.
        inline Quaternion operator*(float f) const { return Quaternion(mul(rg, load(f))); }
        /// @brief 사원수에 실수배를 합니다.
        inline Quaternion operator/(float f) const { return Quaternion(div(rg, load(f))); }
        /// @brief 사원수에 실수배를 합니다.
        inline Quaternion& operator*=(float f) { rg = mul(rg,load(f)); return *this; }
        /// @brief 사원수에 실수배를 합니다.
        inline Quaternion& operator/=(float f) { rg = div(rg,load(f)); return *this; }
        /// @brief 사원수 간의 사칙 연산입니다.
        inline Quaternion operator+(const Quaternion& q) const { return Quaternion(add(rg, q.rg)); }
        /// @brief 사원수 간의 사칙 연산입니다.
        inline Quaternion operator-(const Quaternion& q) const { return Quaternion(sub(rg, q.rg)); }
        /// @brief 사원수 간의 사칙 연산입니다. / 연산자는 우측 피연산자에 대한 곱셈 역원을 곱합니다.
        inline Quaternion operator/(const Quaternion& q) const { Quaternion r(*this); r /= q; return r; }

        /// @brief 단위사원수(회전 사원수)를 리턴합니다.
        inline Quaternion normal() const { return operator*(1.0f / abs()); }
        /// @brief 단위사원수(회전 사원수)를 리턴합니다.
        inline void normalize() { operator*=(1.0f / abs()); }
        /// @brief 단위사원수(회전 사원수)를 리턴합니다. 더 빠르지만 오차가 있을 수 있습니다.
        inline Quaternion fastNormal() const { return operator*(rsqrt(abs2())); }
        /// @brief 단위사원수(회전 사원수)를 리턴합니다. 더 빠르지만 오차가 있을 수 있습니다.
        inline void fastNormalize() { operator*=(rsqrt(abs2())); }
        /// @brief 부호를 반대로 합니다.
        inline Quaternion operator-() const { return Quaternion(neg(rg)); }
        /// @brief 사원수 회전을 합칩니다. 기존 사원수 회전에 다른 회전을 추가로 가한 것과 같으며, 매개변수로 주어진 사원수를 왼쪽에 곱한 것과 같습니다.
        /// @param q 이것이 왼쪽에 곱해집니다.
        inline void compound(const Quaternion &q) { assert(std::abs(q.abs2() - 1.0f) <= std::numeric_limits<float>::epsilon()); *this = q * (*this); }
        /// @brief 사원수 회전을 합칩니다. 기존 사원수 회전에 다른 회전을 추가로 가한 것과 같습니다.
        /// @param axis 회전축
        /// @param angle 회전각(라디안)
        inline void compound(const vec3 &axis, float angle) { compound(rotation(axis, angle)); }
        /// @brief 사원수 회전을 4x4 행렬로 표현합니다.
        inline mat4 toMat4() const {
            Quaternion i = operator*(ci);
            Quaternion j = operator*(cj);
            Quaternion k = operator*(ck);
            return mat4(
                1 - 2 * (j.cj + k.ck), 2 * (i.cj - k.c1), 2 * (i.ck + j.c1), 0,
                2 * (i.cj + k.c1), 1 - 2 * (i.ci + k.ck), 2 * (j.ck - i.c1), 0,
                2 * (i.ck - j.c1), 2 * (j.ck + i.c1), 1 - 2 * (i.ci + j.cj), 0,
                0, 0, 0, 1
            );
        }

        /// @brief 사원수 회전을 4x4 행렬로 표현합니다.
        inline cmat4 toCMat4() const {
            Quaternion i = operator*(ci);
            Quaternion j = operator*(cj);
            Quaternion k = operator*(ck);
            return cmat4(
                1 - 2 * (j.cj + k.ck), 2 * (i.cj + k.c1), 2 * (i.ck - j.c1), 0,
                2 * (i.cj - k.c1), 1 - 2 * (i.ci + k.ck), 2 * (j.ck + i.c1), 0,
                2 * (i.ck + j.c1), 2 * (j.ck - i.c1), 1 - 2 * (i.ci + j.cj), 0,
                0, 0, 0, 1
            );
        }

        /// @brief 사원수 회전을 3x3 행렬로 표현합니다.
        inline mat3 toMat3() const {
            Quaternion i = operator*(ci);
            Quaternion j = operator*(cj);
            Quaternion k = operator*(ck);
            return mat3(
                1 - 2 * (j.cj + k.ck), 2 * (i.cj - k.c1), 2 * (i.ck + j.c1),
                2 * (i.cj + k.c1), 1 - 2 * (i.ci + k.ck), 2 * (j.ck - i.c1),
                2 * (i.ck - j.c1), 2 * (j.ck + i.c1), 1 - 2 * (i.ci + j.cj)
            );
        }

        /// @brief 사원수 회전을 3x3 행렬로 표현합니다.
        inline cmat3 toCMat3() const {
            Quaternion i = operator*(ci);
            Quaternion j = operator*(cj);
            Quaternion k = operator*(ck);
            return cmat3(
                1 - 2 * (j.cj + k.ck), 2 * (i.cj + k.c1), 2 * (i.ck - j.c1),
                2 * (i.cj - k.c1), 1 - 2 * (i.ci + k.ck), 2 * (j.ck + i.c1),
                2 * (i.ck + j.c1), 2 * (j.ck - i.c1), 1 - 2 * (i.ci + j.cj)
            );
        }

        /// @brief 벡터의 첫 성분(x)에 회전각(라디안), 나머지 성분에 3차원 회전축을 담아 리턴합니다.
        /// 부동소수점 정밀도 문제를 고려하여 정규화하여 계산합니다. 회전사원수가 아니라도 nan이 발생하지 않는 점에 주의하세요.
        inline vec4 axis() const {
            Quaternion ax = normal();
            float angle = acosf(ax.c1) * 2;
            float sinha = sqrtf(1 - ax.c1 * ax.c1);
            ax /= sinha;
            ax.c1 = angle;
            return *(reinterpret_cast<vec4*>(&ax));
        }

        /// @brief 이 회전의 오일러 각 (x,y,z, 즉 roll, pitch, yaw 순서)의 형태로 리턴합니다.
        /// 부동소수점 정밀도 문제를 고려하여 정규화하여 계산합니다. 회전사원수가 아니라도 nan이 발생하지 않는 점에 주의하세요.
        /// https://en.wikipedia.org/wiki/Conversion_between_quaternions_and_Euler_angles#Quaternion_to_Euler_angles_conversion
        inline vec3 toEuler() const {
            Quaternion q = *this / abs();
            vec3 a;
            float sinrcosp = 2 * (q.c1 * q.ci + q.cj * q.ck);
            float cosrcosp = 1 - 2 * (q.ci * q.ci + q.cj * q.cj);
            a[0] = atan2f(sinrcosp, cosrcosp);
            float sinp = 2 * (q.c1 * q.cj - q.ck * q.ci);
            if (sinp >= 1) a[1] = PI<float> / 2;
            else if (sinp <= -1) a[1] = -PI<float> / 2;
            else a[1] = asinf(sinp);
            float sinycosp = 2 * (q.c1 * q.ck + q.ci * q.cj);
            float cosycosp = 1 - 2 * (q.cj * q.cj + q.ck * q.ck);
            a[2] = atan2f(sinycosp, cosycosp);
            return a;
        }

        /// @brief 주어진 축을 중심으로 주어진 각만큼 회전을 가하는 사원수를 리턴합니다. 회전축은 자동으로 정규화됩니다.
        /// @param axis 회전축
        /// @param angle 회전각(라디안)
        inline static Quaternion rotation(const vec3& axis, float angle){
            angle *= 0.5f;
            float c = std::cos(angle), s = std::sin(angle);
            vec4 ret = (axis.normal() * s).xxyz();
            ret[0] = c;
            return *reinterpret_cast<Quaternion*>(&ret);
        }

        /// @brief 오일러 회전에 해당하는 사원수를 생성합니다.
        /// @param roll roll(X축 방향 회전)
        /// @param pitch pitch(Y축 방향 회전)
        /// @param yaw yaw(Z축 방향 회전)
        inline static Quaternion rotation(float roll, float pitch, float yaw) {
            float cy = std::cos(yaw * 0.5f);	float sy = std::sin(yaw * 0.5f);
            float cp = std::cos(pitch * 0.5f);	float sp = std::sin(pitch * 0.5f);
            float cr = std::cos(roll * 0.5f);	float sr = std::sin(roll * 0.5f);
            return Quaternion(cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy);
        }

        /// @brief 주어진 축을 중심으로 주어진 각만큼 회전을 가하는 사원수를 리턴합니다. 주어지는 축이 단위벡터임이 확실한 경우에만 사용하세요.
        /// @param axis 회전축
        /// @param angle 회전각(라디안)
        inline static Quaternion rotationByUnit(const vec3& axis, float angle){
            angle *= 0.5f;
            float c = std::cos(angle), s = std::sin(angle);
            vec4 ret = (axis * s).xxyz();
            ret[0] = c;
            return *reinterpret_cast<Quaternion*>(&ret);
        }

        /// @brief 주어진 회전 간의 변화량을 리턴합니다. 즉, 첫 인자가 두 번째 인자로 변하기 위해 왼쪽에 곱해야 하는 사원수를 리턴합니다.
        /// @param q1 포즈 1
        /// @param q2 포즈 2
        inline static Quaternion q2q(const Quaternion& q1, const Quaternion& q2){ return q2 * q1.inverse(); }

        /// @brief 주어진 회전 간의 변화량을 리턴합니다. 즉, 첫 인자가 두 번째 인자로 변하기 위해 왼쪽에 곱해야 하는 사원수를 리턴합니다. q2q보다 빠르지만 오차가 더 클 수 있습니다.
        /// @param q1 포즈 1
        /// @param q2 포즈 2
        inline static Quaternion fastq2q(const Quaternion& q1, const Quaternion& q2){ return q2 * q1.fastInverse(); }

        /// @brief 실수 x 사원수를 가능하게 합니다.
        friend Quaternion operator*(float f, const Quaternion &q) { return q * f; }

        /// @brief 디버그 출력을 위한 스트림 함수입니다.
        friend inline std::ostream& operator<<(std::ostream& os, const Quaternion& q) {
            return os << q.c1 << " + " << q.ci << "i + " << q.cj << "j + " << q.ck << 'k';
        }
    };

    /// @brief 사원수의 선형 보간을 리턴합니다.
    /// @param q1 선형 보간 대상 1(t=0에 가까울수록 이것에 가깝습니다.)
    /// @param q2 선형 보간 대상 2(t=1에 가까울수록 이것에 가깝습니다.)
    /// @param t 선형 보간 값
    inline Quaternion lerp(const Quaternion& q1, const Quaternion& q2, float t){ return (q1 * (1 - t) + q2 * t).normal(); }

    /// @brief 사원수의 구면 선형 보간을 리턴합니다.
    /// @param q1 선형 보간 대상 1(t=0에 가까울수록 이것에 가깝습니다.)
    /// @param q2 선형 보간 대상 2(t=1에 가까울수록 이것에 가깝습니다.)
    /// @param t 선형 보간 값
    inline Quaternion slerp(const Quaternion& q1, const Quaternion& q2, float t) {
        float Wa, Wb;
        float costh = reinterpret_cast<const vec4*>(&q1)->dot(*reinterpret_cast<const vec4*>(&q2)) / std::sqrt(q1.abs2() * q2.abs2());
        // 정밀도 오차로 인한 nan 방지
        if (costh > 1) costh = 1;
        else if (costh < -1) costh = -1;
        float theta = std::acos(costh);
        float sn = std::sin(theta);

        // q1=q2이거나 180도 차이인 경우
        if (sn <= std::numeric_limits<float>::epsilon()) return q1;
        Wa = std::sin((1 - t) * theta) / sn;
        Wb = std::sin(t * theta) / sn;

        Quaternion r = q1 * Wa + q2 * Wb;
        return r / r.abs();
    }

    inline mat3 mat3::rotate(float roll, float pitch, float yaw) { return Quaternion::rotation(roll, pitch, yaw).toMat3(); }
    inline mat4 mat4::rotate(const vec3 &axis, float angle) { return Quaternion::rotation(axis, angle).toMat4(); }
    inline mat4 mat4::rotate(float roll, float pitch, float yaw) { return Quaternion::rotation(roll, pitch, yaw).toMat4(); }
    inline mat4 mat4::rotate(const Quaternion &q) { return q.toMat4(); }

    inline cmat3 cmat3::rotate(float roll, float pitch, float yaw) { return Quaternion::rotation(roll, pitch, yaw).toCMat3(); }
    inline cmat4 cmat4::rotate(const vec3 &axis, float angle) { return Quaternion::rotation(axis, angle).toCMat4(); }
    inline cmat4 cmat4::rotate(float roll, float pitch, float yaw) { return Quaternion::rotation(roll, pitch, yaw).toCMat4(); }
    inline cmat4 cmat4::rotate(const Quaternion &q) { return q.toCMat4(); }
    
    inline mat4 mat4::TRS(const vec3& translation, const Quaternion& rotation, const vec3& scale){
        // SIMD 미적용 시 곱 30회/합 6회, T*R*S 따로 하는 경우 곱 149회/합 102회
        // SIMD 적용 시 곱 15회/합 6회, 따로 하는 경우 곱 44회/합 102회
        mat4 r = rotation.toMat4();
        mul4(r.a, scale.entry);
        mul4(r.a + 4, scale.entry);
        mul4(r.a + 8, scale.entry);
        r[3] = translation[0];
        r[7] = translation[1];
        r[11] = translation[2];
        return r;
    }

    inline mat4 mat4::iTRS(const vec3& translation, const Quaternion& rotation, const vec3& scale) {
        // SIMD 적용 시 곱 19회/합 18회
        mat4 r = rotation.conjugate().toMat4();	// 공액사원수=역회전
        vec3 sc(1); sc /= scale;
        mul4(r.a, sc[0]);
        mul4(r.a + 4, sc[1]);
        mul4(r.a + 8, sc[2]);
        vec4 itr = r * vec4(-translation, 0);
        r[3] = itr[0];
        r[7] = itr[1];
        r[11] = itr[2];
        return r;
    }

    inline cmat4 cmat4::TRS(const vec3& translation, const Quaternion& rotation, const vec3& scale){
        // SIMD 미적용 시 곱 30회/합 6회, T*R*S 따로 하는 경우 곱 149회/합 102회
        // SIMD 적용 시 곱 15회/합 6회, 따로 하는 경우 곱 44회/합 102회
        cmat4 r = rotation.toCMat4();
        mul4(r.a, scale[0]);
        mul4(r.a + 4, scale[1]);
        mul4(r.a + 8, scale[2]);
        std::memcpy(&r[12], &translation, sizeof(vec3));
        r[15] = 1.0f;
        return r;
    }

    inline cmat4 cmat4::iTRS(const vec3& translation, const Quaternion& rotation, const vec3& scale) {
        // SIMD 적용 시 곱 19회/합 18회
        cmat4 r = rotation.conjugate().toCMat4();	// 공액사원수=역회전
        vec3 sc(1); sc /= scale;
        mul4(r.a, sc.entry);
        mul4(r.a + 4, sc.entry);
        mul4(r.a + 8, sc.entry);
        vec4 itr = r * vec4(-translation, 0);
        std::memcpy(&r[12], &itr, sizeof(vec3));
        r[15] = 1.0f;
        return r;
    }
}

#endif