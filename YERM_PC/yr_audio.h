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
#ifndef __YR_AUDIO_H__
#define __YR_AUDIO_H__

#include <memory>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <array>

#include <cstdint>

#include "yr_string.hpp"

namespace onart{
    /// @brief 음원을 재생하기 위한 클래스입니다. 모든 멤버는 static입니다.
    /// TODO: 일단 저수준 PCM 콜백 모듈만 가지고 기존처럼 구현, 짬이 나면 차차 (더 성능이 좋을) 고수준 모듈로 필요한 부분을 대체
    /// 이유: 문서가 인덱스 없이 줄글인데 대충 보니 메모리 관리자가 따로 있어서 기존 인터페이스와 융합하려면 시간을 두고 잘 봐야 할 것 같음
    class Audio{
        public:
            class Source;
            class Stream;
            constexpr static unsigned RINGBUFFER_SIZE = 8820;
            constexpr static unsigned SAMPLE_RATE = 44100;

            /// @brief 모듈 사용을 시작합니다.
            static void init();
            /// @brief 모듈 사용을 종료합니다.
            static void finalize();
            /// @brief 마스터 볼륨을 설정합니다.
            /// @param f 0~1 사이만 가능합니다.
            static void setMasterVolume(float f);
        private:
            static float master;
            static void* engine;
            static std::thread* producer;
            static volatile bool inLoop;
            static void audioThread();
    };

    using pAudioSource = std::shared_ptr<Audio::Source>;
    using pAudioStream = std::shared_ptr<Audio::Stream>;

    /// @brief 음원에 대한 불투명 타입입니다. 현재 ogg vorbis, 스테레오, 샘플레이트 44100Hz(수정 가능) 파일만 사용할 수 있습니다. 맞지 않는 경우 별도의 도구를 이용하여 미리 통일하여 리샘플링해 두는 것이 좋습니다.
    class Audio::Source{
        friend class Audio;
        friend class Stream;
        public:
            /// @brief 소스의 볼륨을 조절합니다. 0~1 범위만 입력할 수 있습니다.
            void setVolume(float v);
            /// @brief 이것을 재생합니다. 재생한 스트림의 포인터가 리턴되며, 해당 객체를 통해 중단/재개/정지를 할 수 있습니다.
            /// @param loop 주어진 횟수만큼 반복합니다. 0 이하의 값을 받으면 무한 반복합니다.
            /// @return 재생한 스트림입니다. 
            pAudioStream play(int loop = 1);
            /// @brief 음원을 불러옵니다.
            /// @param path 파일 경로. 안드로이드의 경우 자동으로 Asset 폴더로부터 불러옵니다.
            /// @param name 프로그램 내부적으로 사용할 이름입니다. 입력하지 않는 경우 파일 이름 그대로 들어가며, 이 값이 중복되는 경우 파일 이름과 무관하게 기존의 것을 리턴합니다.
            /// @return 불러온 객체의 포인터, 불러오기에 실패하면 빈 포인터가 리턴됩니다.
            static pAudioSource load(const string128& path, const string128& name = "");
            /// @brief 메모리에서 음성 파일을 불러옵니다.
            /// @param mem 메모리 변수
            /// @param name 프로그램 내에서 사용할 별명입니다. 이것이 기존의 것과 중복되는 경우 메모리 주소와 무관하게 기존의 것을 리턴합니다.
            /// @param size 메모리 상 배열의 길이
            /// @return 불러온 객체의 포인터. 불러오기에 실패하면 빈 포인터가 리턴됩니다.
            static pAudioSource load(const uint8_t* mem, unsigned size, const string128& name);
            /// @brief 불러온 음원의 포인터를 가져옵니다.
            /// @param name 음원 이름
            /// @return 불러온 객체의 포인터. 이름을 찾을 수 없으면 빈 포인터를 리턴합니다.
            static pAudioSource get(const string128& name);
            /// @brief 불러온 음원을 메모리에서 내립니다. 현재 참조가 있는 음원인 경우 재생이 즉시 종료되며, 참조가 없어지는 즉시 메모리가 해제됩니다.
            /// @param name 음원 이름. 찾을 수 없으면 아무 동작도 하지 않습니다.
            static void drop(const string128& name);
            /// @brief 현재 참조가 없는 음원을 모두 제거합니다.
            /// @param removeUsing true를 주는 경우 참조가 있는 음원의 재생도 즉시 종료되며, 참조가 없어지면 즉시 메모리가 해제됩니다.
            static void collect(bool removeUsing = false);
        private:
            static void reapAll();
            static std::vector<std::shared_ptr<Audio::Source>> sources;
            static std::map<string128, size_t> name2index;
            static bool shouldReap;
            static std::mutex g;

            void present(Audio::Stream& stream, int nSamples);

            Source(void*);
            ~Source();
            
            void* source;
            float volume = 1.0f;
            decltype(name2index)::iterator it;
            std::basic_string<uint8_t> dat; // 안드로이드 환경에서 임시로 사용
            std::vector<pAudioStream> streams;
            bool close = false;
    };

    /// @brief 재생 중인 음원에 대한 타입입니다. 스트림의 재생이 종료되거나 해당 음원을 내린 후에도 안전하게 함수를 호출할 수는 있지만, 아무런 동작도 하지 않습니다. 기본적으로 무한 루프가 아닌 이상 스트림 객체를 보유하는 것은 권장되지 않습니다.
    class Audio::Stream{
        friend class Audio;
        friend class Audio::Source;
        public:
            /// @brief 현재 재생 중인 스트림의 수입니다.
            static const unsigned& activeStreamCount;
            /// @brief 스트림을 중지합니다. 이미 중단되어 있는 경우 아무 동작도 하지 않습니다.
            void pause();
            /// @brief 스트림을 끝냅니다. (이 함수가 호출되거나 주어진 루프 횟수가 종료되고 나서 모든 참조가 없어지면 메모리가 해제됩니다.)
            void end();
            /// @brief 중지한 스트림을 재개합니다. 중지 상태가 아닌 경우 아무 동작도 하지 않습니다.
            void resume();
            /// @brief 스트림을 처음부터 시작합니다. 중지 상태인지에 관계 없이 재생됩니다.
            void restart();
            /// @brief 스트림의 볼륨을 결정합니다.
            void setVolume(float v);
        private:
            using vorbisStereoBuff_t = std::array<int16_t,16384>;
            Stream(int loop);
            ~Stream() = default;
            int frame = 0;
            std::atomic_int32_t seekOffset = 0;
            int offset = 0;
            std::shared_ptr<vorbisStereoBuff_t> buffer;
            int len = 0;
            static unsigned _activeStreamCount;
            float volume = 1.0f;
            
            bool stopped = false;
            int loop;
    };

}

#endif