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
#ifndef __YR_STRING_HPP__
#define __YR_STRING_HPP__

#include <type_traits>
#include <cstdint>
#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>

namespace onart{
    /// @brief 길이가 한정되었지만 모든 내용이 스택 상에 배치되며 C++ basic_string과 호환됩니다.
    /// @tparam T 타입입니다. 정수 계열 타입만 허용됩니다.
    /// @tparam CAPACITY 이 값은 최대 길이 + 1입니다. 0 종료 규칙이 없는 문자열 역시 제한합니다. 어차피 스택에는 많은 내용이 들어가지 못하므로 최대 255까지로 제한합니다.
    template <class T, uint8_t CAPACITY, std::enable_if_t<std::is_integral_v<T> && (CAPACITY > 0),bool> = true>
    class BasicStackString{
        public:
            /// @brief 빈 문자열을 생성합니다.
            inline BasicStackString():_size(0) { data[0]=0; }
            /// @brief 고정 크기 배열로부터 문자를 생성합니다.
            template<uint8_t N> inline BasicStackString(const T (&literal)[N]):_size(N - 1) { 
                static_assert((size_t)N < (size_t)CAPACITY - 1, "The given array is bigger than capacity.");
                memcpy(data, literal, sizeof(T)*N);
                data[N] = 0;
            }
            /// @brief 다른 문자열로부터 복사 생성합니다. 길이가 넘치면 잘립니다.
            template<uint8_t N> inline BasicStackString(const BasicStackString<T, N>& other):_size(std::min((size_t)CAPACITY-1,other.size())){
                memcpy(data, other.begin(), _size * sizeof(T));
                data[_size] = 0;
            }
            /// @brief 다른 문자열로부터 복사 생성합니다.
            inline BasicStackString(const BasicStackString& other){ memcpy(this, &other, sizeof(BasicStackString)); }
            /// @brief C++ 표준 문자열로부터 복사 생성합니다. 용량이 부족한 경우 최대한 채웁니다.
            inline BasicStackString(const std::basic_string<T>& other) {
                _size = std::min(other.size(), (size_t)CAPACITY - 1);
                memcpy(data, other.data(), _size * sizeof(T));
                data[_size] = 0;
            }

            /// @brief 다른 문자열을 이어붙입니다. 길이가 넘치면 잘립니다.
            template<uint8_t N> inline BasicStackString& operator+=(const BasicStackString<T, N>& other) {
                uint8_t newSize = std::min((size_t)CAPACITY - 1, (size_t)_size + other._size);
                memcpy(data + _size, other.data, newSize - _size);
                _size = newSize;
                data[_size] = 0;
                return *this;
            }

            /// @brief 다른 문자열을 이 자리로 복사합니다.
            inline BasicStackString& operator=(const BasicStackString& other) { memcpy(this, &other, sizeof(BasicStackString)); return *this; }

            /// @brief 다른 문자열을 이 자리로 복사합니다. 길이가 넘치면 잘립니다. 
            template<uint8_t N> inline BasicStackString& operator=(const BasicStackString<T,N>& other) { _size = 0; return operator+=(other); }

            /// @brief 내용을 비웁니다.
            inline void clear() { _size = 0; data[0]=0; }
            /// @brief 문자열의 실제 길이를 리턴합니다.
            inline size_t size() const { return _size; }
            /// @brief 크기를 변경합니다.
            inline void resize(uint8_t s) {}
            /// @brief 다른 문자를 뒤에 붙입니다. 용량을 초과하면 아무 동작도 하지 않습니다.
            inline BasicStackString& operator+=(T ch){
                if(_size < CAPACITY - 1){
                    data[_size++]=ch;
                    data[_size]=0;
                }
                return *this;
            }
            /// @brief 같은 타입 간의 사전순 비교 연산자입니다.
            inline bool operator>(const BasicStackString& other) const {
                const uint8_t MIN = std::min(other._size, _size);
                for(uint8_t i = 0;i < MIN;i++){ if(data[i] != other[i]) return data[i] > other[i]; }
                return _size > other._size;
            }

            /// @brief 같은 타입 간의 사전순 비교 연산자입니다.
            inline bool operator<(const BasicStackString& other) const {
                const uint8_t MIN = std::min(other._size, _size);
                for(uint8_t i = 0;i < MIN;i++){ if(data[i] != other[i]) return data[i] < other[i]; }
                return _size < other._size;
            }

            /// @brief enhanced for 루프를 사용할 수 있는 기본적인 begin 함수입니다.
            inline T* begin(){ return data; }
            /// @brief enhanced for 루프를 사용할 수 있는 기본적인 end 함수입니다.
            inline T* end(){return data + _size;}

            /// @brief enhanced for 루프를 사용할 수 있는 기본적인 begin 함수입니다.
            inline const T* begin() const { return data; }
            /// @brief enhanced for 루프를 사용할 수 있는 기본적인 end 함수입니다.
            inline const T* end() const { return data + _size; }

            /// @brief enhanced for 루프를 사용할 수 있는 기본적인 cbegin 함수입니다.
            inline const T* cbegin() const { return data; }
            /// @brief enhanced for 루프를 사용할 수 있는 기본적인 cend 함수입니다.
            inline const T* cend() const { return data + _size; }

            /// @brief 문자열의 상등 비교 연산자입니다.
            inline bool operator==(const BasicStackString& other){ return (other._size == _size) && (memcmp(data, other.data, _size * sizeof(T)) == 0); }
            /// @brief 문자열의 상등 비교 연산자입니다.
            inline bool operator!=(const BasicStackString& other){ return !operator==(other); }
            /// @brief 인덱스 연산자입니다. 경계값 검사를 하지 않습니다.
            inline T& operator[](uint8_t i){ return data[i]; }
            /// @brief 인덱스 연산자입니다. 경계값 검사를 하지 않습니다.
            inline const T& operator[](uint8_t i) const { return data[i]; }
            /// @brief 다른 문자를 뒤에 붙인 것을 리턴합니다. 용량을 초과하면 그대로 리턴합니다.
            inline BasicStackString operator+(T ch) const { return BasicStackString(*this) += ch; }
            /// @brief 문자열끼리 이어붙인 것을 리턴합니다. 원본의 용량을 초과하면 잘려서 리턴됩니다.
            inline BasicStackString operator+(const BasicStackString& other) const { return BasicStackString(*this) += other; }
            /// @brief 0 종료 문자열을 리턴합니다.
            inline T* c_str() { return data; }
            /// @brief 0 종료 문자열을 리턴합니다.
            inline const T* c_str() const { return data; }
            /// @brief C++ 표준 string으로 전환합니다.
            inline operator std::basic_string<T>() const { return std::basic_string((T*)data, _size); }
        private:
            T data[CAPACITY];
            uint8_t _size = 0;
    };

    template <uint8_t C> using string = BasicStackString<char, C>;
    template <uint8_t C> using u16string = BasicStackString<char16_t, C>;

    using string8 = string<8>;
    using string16 = string<16>;
    using string128 = string<128>;
    using string255 = string<255>;

    using u16string8 = u16string<8>;
    using u16string16 = u16string<16>;
    using u16string128 = u16string<128>;
    using u16string255 = u16string<255>;

    /// @brief 주어진 char 배열을 utf8 문자열로 취급하여 가장 앞 유니코드를 int로 리턴하고, 포인터를 다음 문자로 이동시킵니다.
    /// @param src xvalue 포인터여야 합니다.
    inline int u82int(const char*& src){
        if((unsigned char)*src >= 0b11111100) {
            int ret = 
            (int(src[0] & 0b000001) << 30) | 
            (int(src[1] & 0b111111) << 24) | 
            (int(src[2] & 0b111111) << 18) | 
            (int(src[3] & 0b111111) << 12) | 
            (int(src[4] & 0b111111) << 6) | 
            (int(src[5] & 0b111111) << 0);
            src += 6;
            return ret;
        }
        else if((unsigned char)*src >= 0b11111000) {
            int ret = 
            (int(src[0] & 0b000011) << 24) | 
            (int(src[1] & 0b111111) << 18) | 
            (int(src[2] & 0b111111) << 12) | 
            (int(src[3] & 0b111111) << 6) | 
            (int(src[4] & 0b111111) << 0);
            src += 5;
            return ret;
        }
        else if((unsigned char)*src >= 0b11110000) {
            int ret = 
            (int(src[0] & 0b000111) << 18) | 
            (int(src[1] & 0b111111) << 12) | 
            (int(src[2] & 0b111111) << 6) | 
            (int(src[3] & 0b111111) << 0);
            src += 4;
            return ret;
        }
        else if((unsigned char)*src >= 0b11100000) {
            int ret = 
            (int(src[0] & 0b001111) << 12) | 
            (int(src[1] & 0b111111) << 6) | 
            (int(src[2] & 0b111111) << 0);
            src += 3;
            return ret;
        }
        else if((unsigned char)*src >= 0b11000000){
            int ret = 
            (int(src[0] & 0b011111) << 6) | 
            (int(src[1] & 0b111111) << 0);
            src += 2;
            return ret;
        }
        else {
            int ret = src[0] & 0b1111111;
            src += 1;
            return ret;
        }
    }

    /// @brief 주어진 유니코드를 utf8로 변환하여 저장합니다.
    /// @param ch 유니코드
    /// @param dst 목적지
    /// @return utf8로 변형된 후의 길이
    inline int int2u8(uint32_t ch, char* dst){
        if(ch <= 0x7f){
            dst[0] = (char)ch;
            return 1;
        }
        else if(ch <= 0x7ff){
            dst[0] = 0b11000000 | ((ch >> 6) & 0b111111);
            dst[1] = 0b10000000 | ((ch >> 0) & 0b111111);
            return 2;
        }
        else if(ch <= 0xffff) {
            dst[0] = 0b11100000 | ((ch >> 12) & 0b111111);
            dst[1] = 0b10000000 | ((ch >> 6) & 0b111111);
            dst[2] = 0b10000000 | ((ch >> 0) & 0b111111);
            return 3;
        }
        else if(ch <= 0x1fffff) {
            dst[0] = 0b10000000 | ((ch >> 18) & 0b111111);
            dst[1] = 0b10000000 | ((ch >> 12) & 0b111111);
            dst[2] = 0b10000000 | ((ch >> 6) & 0b111111);
            dst[3] = 0b10000000 | ((ch >> 0) & 0b111111);
            return 4;
        }
        else if(ch <= 0x3ffffff) {
            dst[0] = 0b10000000 | ((ch >> 24) & 0b111111);
            dst[1] = 0b10000000 | ((ch >> 18) & 0b111111);
            dst[2] = 0b10000000 | ((ch >> 12) & 0b111111);
            dst[3] = 0b10000000 | ((ch >> 6) & 0b111111);
            dst[4] = 0b10000000 | ((ch >> 0) & 0b111111);
            return 5;
        }
        else {
            dst[0] = 0b11111100 | ((ch >> 30) & 0b111111);
            dst[1] = 0b10000000 | ((ch >> 24) & 0b111111);
            dst[2] = 0b10000000 | ((ch >> 18) & 0b111111);
            dst[3] = 0b10000000 | ((ch >> 12) & 0b111111);
            dst[4] = 0b10000000 | ((ch >> 6) & 0b111111);
            dst[5] = 0b10000000 | ((ch >> 0) & 0b111111);
            return 6;
        }
    }

    /// @brief utf8 문자열을 utf16으로 바꾸어 리턴합니다.
    template<uint8_t N>
    inline u16string<N> convert(const string<N>& u8s){
        u16string255 ret;
        for(const char* i=u8s.cbegin();i<u8s.cend();){
            ret += (char16_t)u82int(i);
        }
        return ret;
    }

    /// @brief utf8 문자열을 utf16 문자열로 바꾸어 주어진 문자열에 채웁니다. 단, 주어진 utf8 문자열에 16비트 범위를 넘어가는 문자가 있으면 그 문자는 16비트로 잘려서 깨집니다.
    /// @param src 변환할 utf8 문자열
    /// @param dst 변환된 문자열의 목적지입니다. 원래 내용이 있었다면 비워집니다.
    template<uint8_t N>
    inline void convert(const string<N>& src, u16string<N>& dst){
        dst.clear();
        for(const char* i=src.cbegin();i<src.cend();){
            dst += (char16_t)u82int(i);
        }
    }

    /// @brief 로그를 위한 스트림 함수입니다. 통일을 위해 utf8로 변환 후 출력합니다.
    template<class T, uint8_t C>
    inline std::ostream& operator<<(std::ostream& stream, const BasicStackString<T, C>& s) {
        char c[8]={};
        for(T t: s){ 
            int sz = int2u8(t, c);
            for(int i=0;i<sz;i++) stream << c[i];
        }
        return stream;
    }

    /// @brief 로그를 위한 스트림 함수입니다.
    template<uint8_t C>
    inline std::ostream& operator<<(std::ostream& stream, const BasicStackString<char, C>& s) {
        return stream << s.c_str();
    }

}

#endif