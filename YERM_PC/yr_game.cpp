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
#include "yr_game.h"
#include "yr_sys.h"
#include "yr_vulkan.h"
#include "yr_input.h"
#include "yr_audio.h"

#include "logger.hpp"
#include "yr_math.hpp"
#include "yr_shadercompile.hpp"
#include "yr_bits.hpp"

#include "../externals/boost/predef/platform.h"
#include "../externals/boost/predef/compiler.h"

#if BOOST_PLAT_ANDROID
#include <game-activity/native_app_glue/android_native_app_glue.h>
#endif

namespace onart{

    const std::chrono::steady_clock::time_point Game::longTp = std::chrono::steady_clock::now();
    float Game::_dt = 0.016f, Game::_idt = 60.0f;
    uint64_t Game::_tp = (longTp - std::chrono::steady_clock::time_point()).count();
    const float &Game::dt(Game::_dt), &Game::idt(Game::_idt);
    const uint64_t& Game::tp(Game::_tp);
    int32_t Game::_frame = 1;
    const int32_t& Game::frame(Game::_frame);
    VkMachine* Game::vk = nullptr;
    Window* Game::window = nullptr;
    void* Game::hd = nullptr;

    int Game::start(void* hd, Window::CreationOptions* opt){
        if(window) {
            LOGWITH("Warning: already started");
            return 2;
        }

        Game::hd = hd;

        delete window;        
        window = new Window(hd, opt);
        if(!window->isNormal()) {
            delete window;
            LOGWITH("Window creation failed");
            Window::terminate();
            return 1;
        }
        vk = new VkMachine(window);
        if(!init()){
            delete window;
            Window::terminate();
            return 1;
        }

#if !BOOST_PLAT_ANDROID
        //std::thread gamethread([](){
#endif
            for(;;_frame++){
                window->pollEvents();
                if(window->windowShouldClose()) break;
                std::chrono::duration<uint64_t, std::nano> longDt = std::chrono::steady_clock::now() - longTp;

                // [0, 2^52 - 1] 범위 정수를 균등 간격의 [1.0, 2.0) 범위 double로 대응시키는 함수에 십억을 적용한 후 1을 뺀 결과에 곱하여 1로 만들 수 있는 수
                constexpr double ONE_SECOND = 1.0 / 0.0000002220446049250313080847263336181640625;

                uint64_t ndt = longDt.count() - _tp;
                double ddt = (fixedPointConversion64i(ndt) - 1.0) * ONE_SECOND; // 함수를 사용할 수 없는 ndt의 상한선: 4,503,599,627,370,496 > 52일
                double iddt = 1.0 / ddt;
                _tp = longDt.count();
                _dt = static_cast<float>(ddt);
                _idt = static_cast<float>(iddt);
            }
#if !BOOST_PLAT_ANDROID
        //}); gamethread.join();
#endif

        finalize();
        return 0;
    }

    void Game::finalize(){
        Audio::finalize();
        delete window;
        delete vk;
        window = nullptr;
        vk = nullptr;
        Window::terminate();
    }

    bool Game::init() {
        Audio::init();
        window->clickCallback = Input::click;
        window->keyCallback = Input::keyboard;
        window->posCallback = Input::moveCursor;
        window->touchCallback = Input::touch;
        window->windowSizeCallback = windowResized;
        auto rp2s = VkMachine::createRenderPass2Screen(nullptr, 1, "main");
        auto lo = VkMachine::createPipelineLayout(nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, "test");
        VkVertexInputAttributeDescription desc[2];
        using testv_t = VkMachine::Vertex<vec3, vec3>;
        testv_t::info(desc, 0);
        std::vector<uint32_t> vec;
        compile("../../../shaders/test.vert", shaderc_shader_kind::shaderc_glsl_vertex_shader,&vec);
        auto vs = VkMachine::createShader(vec.data(), vec.size()*sizeof(vec[0]),"testv");
        compile("../../../shaders/test.frag", shaderc_shader_kind::shaderc_glsl_fragment_shader,&vec);
        auto fs = VkMachine::createShader(vec.data(), vec.size()*sizeof(vec[0]),"testf");
        auto pp = VkMachine::createPipeline(desc, sizeof(testv_t), 2, nullptr, 0, 0, rp2s, 0, 0, lo, vs, fs, "testpp");
        testv_t verts[3]{{{0,0,0},{1,0,0}},{{0,1,0},{1,0,0}},{{1,0,0},{0,1,0}}};
        auto vb = VkMachine::createMesh(verts,sizeof(testv_t),3,nullptr,2,0,"testvb");
        rp2s->start();
        float t = 1.0f;
        rp2s->push(&t,0,4);
        rp2s->invoke(vb);
        rp2s->execute();
        return true;
    }

    void Game::windowResized(int x, int y){
        vk->createSwapchain(x, y);
    }

    void Game::exit(){
        window->close();
    }

    void Game::readFile(const char* fileName, std::basic_string<uint8_t>* buffer){
        buffer->clear();
#if BOOST_PLAT_ANDROID
        AAsset* asset = AAssetManager_open(reinterpret_cast<android_app*>(hd)->activity->assetManager, fileName, AASSET_MODE_BUFFER);
        if(asset) {
            int len = AAsset_getLength(asset);
            buffer->resize(len);
            AAsset_read(asset, buffer->data(), len);
            AAsset_close(asset);
        }
#elif BOOST_COMP_MSVC
        FILE* fp;
        fopen_s(&fp, fileName, "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            int len = ftell(fp);
            buffer->resize(len);
            fseek(fp, 0, SEEK_SET);
            fread_s(buffer->data(), len, 1, len, fp);
            fclose(fp);
        }
#else
        FILE* fp = fopen(fileName, "rb");
        if(fp) {
            fseek(fp, 0, SEEK_END);
            int len = ftell(fp);
            buffer->resize(len);
            fseek(fp, 0, SEEK_SET);
            fread(buffer->data(), 1, len, fp);
            fclose(fp);
        }
#endif
    }
}