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
#include "../externals/boost/predef/platform.h"
#include "../externals/single_header/miniaudio.h"
#include "logger.hpp"
#include "yr_simd.hpp"
#include "yr_pool.hpp"
#include "yr_game.h"

#include <algorithm>

#include <cstdlib>
#include <cstring>

namespace onart{

    void* Audio::engine = nullptr;
    std::thread* Audio::producer;
    volatile bool Audio::inLoop = false;
    float Audio::master = 1.0f;

    bool Audio::Source::shouldReap = false;
    std::vector<pAudioSource> Audio::Source::sources;
    std::map<string128, size_t> Audio::Source::name2index;
    std::mutex Audio::Source::g;

    static DynamicPool<std::array<int16_t,16384>, 8> pool8192;

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
            /// @brief 링버퍼 사용을 종료합니다.
            inline void finalize() { writeIndex = 0; limitIndex = 1; }
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
            std::copy(body + readIndex, body + Audio::RINGBUFFER_SIZE, ((decltype(body + 0))output));
            std::copy(body, body + nextReadIndex, (int16_t*)output + Audio::RINGBUFFER_SIZE - readIndex);
            if(Audio::Stream::activeStreamCount == 0) {
                std::fill(body + readIndex, body + Audio::RINGBUFFER_SIZE, 0);
                std::fill(body, body + nextReadIndex, 0);
            }
            readIndex = nextReadIndex;
        }
        else{
            std::copy(body + readIndex, body + readEnd, ((decltype(body + 0))output));
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
        while(inLoop){
            unsigned writable;
            while ((writable = ringBuffer.writable()) == 0) {
                if (!inLoop) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 전력 절약
            }
            size_t sz = Source::sources.size();
            for(size_t i = 0; i < sz; i++) {
                pAudioSource& sourceI = Source::sources[i];
                if(sourceI->close) { continue; }
                size_t ps = sourceI->streams.size();
                for(size_t j = 0; j < ps; j++){
                    sourceI->present(*(sourceI->streams[j]), writable / 2);
                }
            }
            ringBuffer.addComplete();
            Source::reapAll();
        }
    }

    void Audio::finalize(){
        inLoop = false;
        ringBuffer.finalize();
        
        producer->join();
        delete producer;
        producer = nullptr;
        ma_device_uninit(_HDEVICE);
        std::free(engine);
        engine = nullptr;
    }

    void Audio::setMasterVolume(float f) {
        master = std::clamp(f, 0.0f, 1.0f);
    }

#define _HSTBV reinterpret_cast<stb_vorbis*>(source)

    pAudioSource Audio::Source::load(const string128& path, const string128& name) {
        const string128& _name = name.size() == 0 ? path : name;
        pAudioSource ret = get(_name);
        if(ret) return ret;
        int stbErr;
#if BOOST_PLAT_ANDROID
        std::basic_string<uint8_t> buf;
        Game::readFile(path.c_str(),&buf);
        stb_vorbis* fp = stb_vorbis_open_memory(buf.data(), buf.size(), &stbErr, nullptr);
#else
        stb_vorbis* fp = stb_vorbis_open_filename(path.c_str(), &stbErr, nullptr);
#endif
        if(!fp){
            LOGWITH(path, "Load failed:", stbErr); // enum STBVorbisError 확인
            return pAudioSource();
        }
        stb_vorbis_info info = stb_vorbis_get_info(fp);
        if(info.channels != 2 || info.sample_rate != SAMPLE_RATE){ // Vorbis의 프레임당 샘플 수는 64 ~ 8192(2의 거듭제곱만)
            LOGWITH(_name, "Load failed: set the source\'s channel to 2 and sample rate to",SAMPLE_RATE);
            stb_vorbis_close(fp);
            return pAudioSource();
        }
        struct audiosource:public Audio::Source{inline audiosource(void* _1):Source(_1){}};
        pAudioSource nw = std::make_shared<audiosource>(fp);
        std::unique_lock<std::mutex> _(g);
        name2index.insert({_name,sources.size()});
        nw->it = name2index.find(_name);
        sources.push_back(nw);
#if BOOST_PLAT_ANDROID
        nw->dat = std::move(buf);
#endif
        return nw;
    }

    pAudioSource Audio::Source::load(const uint8_t* mem, unsigned size, const string128& name){
        pAudioSource ret = get(name);
        if(ret) return ret;
        int stbErr;
        stb_vorbis* fp = stb_vorbis_open_memory(mem, size, &stbErr, nullptr);
        if(!fp) {
            LOGWITH(name, "Load failed:", stbErr); // enum STBVorbisError 확인
            return pAudioSource();
        }
        stb_vorbis_info info = stb_vorbis_get_info(fp);
        if(info.channels != 2 || info.sample_rate != SAMPLE_RATE){ // Vorbis의 프레임당 샘플 수는 64 ~ 8192(2의 거듭제곱만)
            LOGWITH(name, "Load failed: set the source\'s channel to 2 and sample rate to",SAMPLE_RATE);
            stb_vorbis_close(fp);
            return pAudioSource();
        }
        struct audiosource:public Audio::Source{inline audiosource(void* _1):Source(_1){}};
        pAudioSource nw = std::make_shared<audiosource>(fp);
        std::unique_lock<std::mutex> _(g);
        name2index.insert({name,sources.size()});
        nw->it = name2index.find(name);
        sources.push_back(nw);
        return nw;
    }

    Audio::Source::Source(void* p):source(p){ }

    Audio::Source::~Source(){
        stb_vorbis_close(_HSTBV);
    }

    pAudioSource Audio::Source::get(const string128& name) {
        std::unique_lock<std::mutex> _(g);
        auto it = name2index.find(name);
        if(it != name2index.end()) {
            return sources[it->second];
        }
        return pAudioSource();
    }

    void Audio::Source::collect(bool removeUsing){
        std::unique_lock<std::mutex> _(g);
        size_t sz = sources.size();
        for(size_t i = 0; i < sz; i++){ sources[i]->close = removeUsing || (sources[i].use_count() == 1); }
    }

    void Audio::Source::drop(const string128& name) {
        std::unique_lock<std::mutex> _(g);
        auto it = name2index.find(name);
        if(it != name2index.end()) {
            sources[it->second]->close = true;
            shouldReap = true;
        }
    }

    void Audio::Source::reapAll() {
        if(!shouldReap) return;
        std::unique_lock<std::mutex> _(g);
        for(size_t i = 0; i < sources.size();){
            pAudioSource& sourceI = sources[i];
            if(sourceI->close) {
                name2index.erase(sourceI->it);
                sourceI.swap(sources.back());
                sourceI->it->second = i;
                sources.pop_back();
            }
            else{
                for(size_t j = 0; j < sourceI->streams.size();) {
                    pAudioStream& streamJ = sourceI->streams[j];
                    if(streamJ->seekOffset == -1) {
                        streamJ.swap(sourceI->streams.back());
                        sourceI->streams.pop_back();
                    }
                    else{
                        j++;
                    }
                }
                i++;
            }
        }
        shouldReap = false;
    }

    pAudioStream Audio::Source::play(int loop) {
        struct audiostream:public Audio::Stream{ inline audiostream(int _1):Audio::Stream(_1){} };
        pAudioStream nw = std::make_shared<audiostream>(loop);
        streams.push_back(nw);
        return nw;
    }

    void Audio::Source::setVolume(float f){
        volume = std::clamp(f, 0.0f, 1.0f);
    }

    void Audio::Source::present(Audio::Stream& stream, int nSamples) {
        if(stream.stopped || stream.seekOffset == -1ll) return;
        if(stream.seekOffset != stb_vorbis_get_sample_offset(_HSTBV)) stb_vorbis_seek_frame(_HSTBV, stream.seekOffset);
        unsigned produced = 0;
        const float vol = Audio::master * volume * stream.volume;
        while(nSamples){
            int rest = stream.len - stream.offset;
            if (rest == 0) {
                stream.offset = 0;
                rest = stream.len = stb_vorbis_get_frame_short_interleaved(_HSTBV, 2, stream.buffer->data(), (int)stream.buffer->size());
                if (stream.len == 0) {
                    stream.loop--;
                    if (stream.loop == 0) {
                        stream.end();
                        return;
                    }
                    stb_vorbis_seek_start(_HSTBV);
                    continue;
                }
            }
            int toPresent = nSamples > rest ? rest : nSamples;
            if (vol != 1.0f) { mulAll(stream.buffer->data() + (stream.offset * 2), vol, toPresent * 2); }
            ringBuffer.add(stream.buffer->data() + (stream.offset * 2), toPresent * 2, produced);
            stream.offset += toPresent;
            produced += toPresent * 2;
            nSamples -= toPresent;
        }
        if(stream.seekOffset != -1) stream.seekOffset = stb_vorbis_get_sample_offset(_HSTBV);
    }

#undef _HSTBV

    Audio::Stream::Stream(int loop) :loop(loop), buffer(pool8192.get()) { 
        _activeStreamCount++;
    }
    void Audio::Stream::pause() { 
        if(!stopped){
            stopped = true;
            _activeStreamCount--;
        }
    }
    void Audio::Stream::restart() { 
        if(seekOffset == -1) return;
        seekOffset = 0;
        if(stopped){
            stopped = false;
            _activeStreamCount++;
        }
    }
    void Audio::Stream::end() { 
        seekOffset = -1;
        if(!stopped){
            _activeStreamCount--;
        }
    }
    void Audio::Stream::resume() { 
        if(stopped){
            stopped = false;
            _activeStreamCount++;
        }
    }

    void Audio::Stream::setVolume(float f) { 
        volume = std::clamp(f, 0.0f, 1.0f);
    }

#undef _HDEVICE
}