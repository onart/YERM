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

#include "yr_simd.hpp"
#include <cstring>
#include <type_traits>
#include <cmath>
#include <iostream>

namespace onart{
    /// @brief 2~4차원 벡터입니다. 길이에 관계없이 상호 변환이 가능합니다.
    /// @tparam T 성분의 타입입니다. 사칙연산 및 부호 반전이 가능해야 합니다.
    /// @tparam D 벡터의 차원수입니다. 2~4차원만 사용할 수 있습니다.
    template<class T, unsigned D>
    struct alignas(16) nvec{
        union{
            T entry[4];
            struct { T x, y, z, w; };
            struct { T s, t, p, q; };
            struct { T r, g, b, a; };
        };
        /// @brief 영벡터를 초기화합니다.
        inline nvec():entry{}{}
        /// @brief 벡터의 모든 성분을 하나의 값으로 초기화합니다.
        explicit inline nvec(T a) { static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); set4(entry,a); }
        /// @brief 벡터의 값 중 앞 2~4개를 초기화합니다.
        inline nvec(T x, T y, T z = 0, T w = 0) : entry{x, y, z, w} { static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); }
        /// @brief 벡터를 복사합니다.
        inline nvec(const nvec &v) { memcpy(entry, v.entry, sizeof(entry)); }
        /// @brief 배열을 이용하여 벡터를 생성합니다.
        explicit inline nvec(const T* v) { static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); set4(entry,v);}
        /// @brief 한 차원 낮은 벡터와 나머지 한 성분을 이어붙여 벡터를 생성합니다.
        inline nvec(const nvec<T,D-1>& v, T a){ static_assert(D >= 2 && D <= 4, "nvec은 2~4차원만 생성할 수 있습니다."); set4(entry, v.entry); entry[D-1] = a; }

        /// @brief 벡터의 모든 성분을 하나의 값으로 초기화합니다.
        inline void set(T a) { set4(entry, a); }
        /// @brief 다른 벡터의 값을 복사해 옵니다. 차원수는 달라도 됩니다.
        template <unsigned E> inline void set(const nvec<T, E> &v) { set4(entry, v.entry); }

        /// @brief 벡터의 모든 성분을 하나의 값으로 초기화합니다.
        inline nvec &operator=(T a) { set(a); return *this; }
        /// @brief 다른 벡터의 값을 복사해 옵니다. 차원수는 달라도 됩니다.
        template<unsigned E> inline nvec &operator=(const nvec<T, E>& v) { set(v); return *this; }

        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec &operator+=(const nvec &v) { add4(entry, v.entry); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec &operator-=(const nvec &v) { sub4(entry, v.entry); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec &operator*=(const nvec &v) { mul4(entry, v.entry); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec &operator/=(const nvec &v) { div4(entry, v.entry); return *this; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator+(const nvec &v) const { return nvec(*this) += v; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator-(const nvec &v) const { return nvec(*this) -= v; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator*(const nvec &v) const { return nvec(*this) *= v; }
        /// @brief 다른 벡터와 성분별 연산을 합니다.
        inline nvec operator/(const nvec &v) const { return nvec(*this) /= v; }

        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec &operator+=(T a) { add4(entry, a); return *this; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec &operator-=(T a) { sub4(entry, a); return *this; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec &operator*=(T a) { mul4(entry, a); return *this; }
        /// @brief 다른 스칼라와 성분별 연산을 합니다.
        inline nvec &operator/=(T a) { div4(entry, a); return *this; }
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
                nvec z = operator-(v);
                abs4(z.entry);
                if constexpr(std::is_same_v<float, T>){
                    bool b2 = z.x <= __FLT_EPSILON__ && z.y <= __FLT_EPSILON__;
                    if constexpr(D > 2) b2 = b2 && z.z <= __FLT_EPSILON__;
                    if constexpr(D > 3) b2 = b2 && z.w <= __FLT_EPSILON__;
                    return b2;
                }
                else if constexpr(std::is_same_v<double, T>){
                    bool b2 = z.x <= __DBL_EPSILON__ && z.y <= __DBL_EPSILON__;
                    if constexpr(D > 2) b2 = b2 && z.z <= __FLT_EPSILON__;
                    if constexpr(D > 3) b2 = b2 && z.w <= __FLT_EPSILON__;
                    return b2;
                }
                else if constexpr(std::is_same_v<long double, T>){ // 사실 대상 시스템에서는 double과 long double이 같음
                    bool b2 = z.x <= __LDBL_EPSILON__ && z.y <= __LDBL_EPSILON__;
                    if constexpr(D > 2) b2 = b2 && z.z <= __FLT_EPSILON__;
                    if constexpr(D > 3) b2 = b2 && z.w <= __FLT_EPSILON__;
                    return b2;
                }
            }
        }

        /// @brief 벡터의 모든 성분이 동일한 경우가 아니라면 참을 리턴합니다.
        inline bool operator!=(const nvec &v) const { return !operator==(v); }

        /// @brief 부호를 반전시켜 리턴합니다.
        inline nvec operator-() const { return (*this) * -1; }
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
        friend inline T dot(const nvec& v1, const nvec& v2) {
            nvec nv(v1 * v2);
            if constexpr(D == 2) return nv[0] + nv[1];
            else if constexpr(D == 3) return nv[0] + nv[1] + nv[2];
            else if constexpr(D == 4) return (nv[0] + nv[1]) + (nv[2] + nv[3]);
            else return T();
        }

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
    };

    using vec2 = nvec<float, 2>; using vec3 = nvec<float, 3>; using vec4 = nvec<float, 4>;
    using ivec2 = nvec<int32_t, 2>; using ivec3 = nvec<int32_t, 3>; using ivec4 = nvec<int32_t, 4>;
    using uvec2 = nvec<uint32_t, 2>; using uvec3 = nvec<uint32_t, 3>; using uvec4 = nvec<uint32_t, 4>;
    using dvec2 = nvec<double, 2>; using dvec3 = nvec<double, 3>; using dvec4 = nvec<double, 4>;

    /// @brief 2개의 2차원 실수 벡터 외적의 z축 성분을 계산합니다.
    inline float cross2(const vec2 &a, const vec2 &b) { return a[0] * b[1] - a[1] * b[0]; }
    /// @brief 2개의 2차원 실수 벡터 외적의 z축 성분을 계산합니다.
    inline double cross2(const dvec2 &a, const dvec2 &b) { return a[0] * b[1] - a[1] * b[0]; }

    /// @brief 2개의 3차원 실수 벡터의 외적을 계산합니다.
    inline vec3 cross(const vec3& a, const vec3& b){
        vec3 mul = a * vec3(b[1], b[2], b[0]) - b * vec3(a[1], a[2], a[0]);
		return vec3(mul[1], mul[2], mul[0]);
    }

    /// @brief 2개의 3차원 실수 벡터의 외적을 계산합니다.
    inline dvec3 cross(const dvec3& a, const dvec3& b){
        dvec3 mul = a * dvec3(b[1], b[2], b[0]) - b * dvec3(a[1], a[2], a[0]);
		return dvec3(mul[1], mul[2], mul[0]);
    }

    /// @brief 2개 단위벡터의 구면선형보간을 리턴합니다.
    /// @param a 구면 선형 보간 대상 1(t=0에 가까울수록 이 벡터에 가깝습니다.)
    /// @param b 구면 선형 보간 대상 2(t=1에 가까울수록 이 벡터에 가깝습니다.)
    /// @param t 구면 선형 보간 값
    inline vec3 slerp(const vec3 a, const vec3& b, float t){
        float sinx = cross(a, b).length();
		float theta = std::asin(sinx);
		if (theta <= __FLT_EPSILON__) return a;
		return a * std::sin(theta * (1 - t)) + b * std::sin(theta * t);
    }

    /// @brief 2개 단위벡터의 구면선형보간을 리턴합니다.
    /// @param a 구면 선형 보간 대상 1(t=0에 가까울수록 이 벡터에 가깝습니다.)
    /// @param b 구면 선형 보간 대상 2(t=1에 가까울수록 이 벡터에 가깝습니다.)
    /// @param t 구면 선형 보간 값
    inline dvec3 slerp(const dvec3 a, const dvec3& b, double t){
        double sinx = cross(a, b).length();
		double theta = std::asin(sinx);
		if (theta <= __DBL_EPSILON__) return a;
		return a * std::sin(theta * (1 - t)) + b * std::sin(theta * t);
    }
}

#endif