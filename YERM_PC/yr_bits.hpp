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
#ifndef __YR_BITS_HPP__
#define __YR_BITS_HPP__

#include <cstdint>

constexpr int SIZEOF_FLOAT = sizeof(float);
constexpr int SIZEOF_INT32 = sizeof(int32_t);
constexpr int SIZEOF_DOUBLE = sizeof(double);
constexpr int SIZEOF_INT64 = sizeof(int64_t);
constexpr int SIZEOF_REG = sizeof(size_t);
constexpr bool CAN_CONVERT_FLOAT32 = (SIZEOF_FLOAT == SIZEOF_INT32) && (SIZEOF_REG >= 4);
constexpr bool CAN_CONVERT_FLOAT64 = (SIZEOF_DOUBLE == SIZEOF_INT64) && (SIZEOF_REG >= 8);
constexpr uint32_t ZERO_EXP32 = 127 << 23;
constexpr uint64_t ZERO_EXP64 = 1023ull << 52;

namespace onart {

    /// @brief 비트 데이터 그대로 정수로 옮깁니다.
    inline int32_t regInt32(float f){
        union{
            float a;
            int32_t b;
        }_;
        _.a=f;
        return _.b;
        /* clang, gcc 기준 이렇게 한 줄로 가능. ARM도 마찬가지
        movd    eax, xmm0
        ret
        */
    }
    /// @brief 비트 데이터 그대로 부동소수점으로 옮깁니다.
    inline float regFloat32(int32_t i){
        union{
            float a;
            int32_t b;
        }_;
        _.b=i;
        return _.a;
    }

    /// @brief 0 ~ 2^23 구간의 정수를 [1.0, 2.0) 구간의 float로 바꿉니다. 그 외의 구간도 정해진 결과가 나오긴 하지만 원하는 결과는 아닐 것입니다.
    inline float fixedPointConversion32i(int32_t i){
        return regFloat32(i | ZERO_EXP32);
    }

    /// @brief [1.0, 2.0) 구간의 float를 0 ~ 2^23 - 1 구간의 정수로 바꿉니다. 그 외의 구간도 정해진 결과가 나오긴 하지만 원하는 결과는 아닐 것입니다.
    inline int32_t fixedPointConversion32f(float f){
        return regInt32(f) ^ ZERO_EXP32;
    }

    /// @brief 비트 데이터 그대로 정수로 옮깁니다.
    inline int64_t regInt64(double f){
        union{
            double a;
            int64_t b;
        }_;
        _.a=f;
        return _.b;
    }
    /// @brief 비트 데이터 그대로 부동소수점으로 옮깁니다.
    inline double regFloat64(int64_t i){
        union{
            double a;
            int64_t b;
        }_;
        _.b=i;
        return _.a;
    }

    /// @brief 0 ~ 2^52 - 1 구간의 정수를 [1.0, 2.0) 구간의 double로 바꿉니다. 그 외의 구간도 정해진 결과가 나오긴 하지만 원하는 결과는 아닐 것입니다.
    inline double fixedPointConversion64i(int64_t i){
        return regFloat64(i | ZERO_EXP64);
    }

    /// @brief [1.0, 2.0) 구간의 float를 0 ~ 2^52 - 1 구간의 정수로 바꿉니다. 그 외의 구간도 정해진 결과가 나오긴 하지만 원하는 결과는 아닐 것입니다.
    inline int64_t fixedPointConversion64f(double f){
        return regInt64(f) ^ ZERO_EXP64;
    }

}


#endif