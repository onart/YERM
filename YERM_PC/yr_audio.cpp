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

#include "yr_audio.h"
#define STB_VORBIS_HEADER_ONLY
#include "../externals/single_header/stb_vorbis.c"
#include "logger.hpp"
#include "yr_simd.hpp"

#include <cstdlib>
#include <algorithm>
#include <cstring>

namespace onart{

    void* Audio::engine = nullptr;
    std::thread* Audio::producer;
    bool Audio::inLoop = false;
    unsigned Audio::Stream::_activeStreamCount = 0;
    const unsigned& Audio::Stream::activeStreamCount = Audio::Stream::_activeStreamCount;

    /// @brief 비동기적으로 PCM을 모아 두는 링 버퍼입니다. 생산자, 소비자 스레드가 하나씩만 있는 경우 명시적 동기화를 요구하지 않습니다.
    static class RingBuffer{
        public:
            /// @brief 현 시점에 더 채울 수 있는 샘플 수를 리턴합니다.
            unsigned writable();
            /// @brief 주어진 샘플을 더합니다.
            /// @param in 더할 샘플(배열)
            /// @param len 주어진 총 샘플 수 
            /// @param to 이 링버퍼의 가장 앞으로부터 더할 인덱스입니다.
            void add(const int16_t* in, unsigned len, unsigned to = 0);
            /// @brief 링버퍼의 입력 커서를 지금까지 쓴 것의 뒤로 세팅합니다.
            void addComplete();
            /// @brief 링버퍼의 데이터를 주어진 만큼 읽어옵니다.
            /// @param out 출력 위치
            /// @param count 카운트
            void read(void* out, unsigned count);
        private:
            alignas(16) int16_t body[Audio::RINGBUFFER_SIZE] = {0, };
            /// @brief 읽기 시작점
            unsigned readIndex = 0;
            /// @brief 쓰기 끝점
            unsigned limitIndex = 0;
            /// @brief 쓰기 시작점
            unsigned writeIndex = 0;
            /// @brief 0인 경우 더하기를, 그 외에는 복사를 수행합니다.
            int copying = 2;
    } ringBuffer;

    unsigned RingBuffer::writable() {
        if (copying == 2) { limitIndex = readIndex; }
        if (writeIndex > limitIndex) { return Audio::RINGBUFFER_SIZE - writeIndex + limitIndex;	}
        else { return limitIndex - writeIndex; }
    }

    void RingBuffer::addComplete() {
        copying = 2;
        writeIndex = limitIndex;
    }

    void RingBuffer::add(const int16_t* in, unsigned len, unsigned to){
        unsigned writePos = (writeIndex + to) % Audio::RINGBUFFER_SIZE;
        unsigned writeEnd = writePos + len;
        bool overflow = writeEnd >= Audio::RINGBUFFER_SIZE;
        if(to == 0) copying >>= 1;
        if(copying) {
            if(!overflow) {
                std::copy(in, in + len, body + writePos);
            }
            else{
                const unsigned WRITE1 = Audio::RINGBUFFER_SIZE - writePos;
                std::copy(in, in + WRITE1, body + writePos);
                std::copy(in + WRITE1, in + len, body);
            }
        }
        else {
            if(!overflow) {
                addsAll(body + writePos, in, len);
            }
            else{
                const unsigned WRITE1 = Audio::RINGBUFFER_SIZE - writePos;
                addsAll(body + writePos, in, WRITE1);
                addsAll(body, in + WRITE1, (writeEnd - Audio::RINGBUFFER_SIZE));
            }
        }
    }

    void RingBuffer::read(void* output, unsigned count) {
        unsigned readEnd = readIndex + count;
        if(readEnd >= Audio::RINGBUFFER_SIZE) {
            unsigned nextReadIndex = readEnd - Audio::RINGBUFFER_SIZE;
            std::copy(body + readIndex, body + Audio::RINGBUFFER_SIZE, output);
            std::copy(body, body + nextReadIndex, (int16_t*)output + Audio::RINGBUFFER_SIZE - readIndex);
            if(Audio::Stream::activeStreamCount == 0) {
                std::fill(body + readIndex, body + Audio::RINGBUFFER_SIZE, 0);
                std::fill(body, body + nextReadIndex, 0);
            }
            readIndex = nextReadIndex;
        }
        else{
            std::copy(body + readIndex, body + readEnd, output);
            if(Audio::Stream::activeStreamCount == 0) {
                std::fill(body + readIndex, body + readEnd, 0);
            }
            readIndex = readEnd;
        }
    }

    static void deliverPCM(ma_device* device, void* output, const void* input, ma_uint32 frameCount){
        ringBuffer.read(output, frameCount * 2);
    }

#define _HDEVICE reinterpret_cast<ma_device*>(engine)
    void Audio::init(){
        ma_device_config config = ma_device_config_init(ma_device_type::ma_device_type_playback);        
        config.playback.format = ma_format_s16;
        config.playback.channels = 2;
        config.sampleRate = SAMPLE_RATE;
        config.dataCallback = deliverPCM;

        engine = std::malloc(sizeof(ma_device));
        ma_result result;
        if((result = ma_device_init(nullptr, &config, _HDEVICE)) != MA_SUCCESS){
            LOGWITH("Failed to initialize miniaudio device:",result);
            finalize();
            return;
        }
        if((result = ma_device_start(_HDEVICE)) != MA_SUCCESS){
            LOGWITH("Failed to start miniaudio device:",result);
            finalize();
            return;
        }
        inLoop = true; // 생산자 하나, 소비자 하나이므로 동기화 조치 안함
        producer = new std::thread(audioThread);
    }

    void Audio::audioThread(){

    }

    void Audio::finalize(){
        producer->join();
        delete producer;
        producer = nullptr;
        ma_device_uninit(_HDEVICE);
        std::free(engine);
        engine = nullptr;
    }

#undef _HDEVICE
}