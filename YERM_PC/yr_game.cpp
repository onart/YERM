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
#include "../externals/glad/glad.h"
#include "yr_game.h"
#include "yr_sys.h"
#include "yr_graphics.h"
#include "yr_input.h"
#include "yr_audio.h"

#include "logger.hpp"
#include "yr_math.hpp"
#include "yr_bits.hpp"

#include "yr_constants.hpp"

#include "../externals/boost/predef/platform.h"
#include "../externals/boost/predef/compiler.h"

#include <thread>

#if BOOST_PLAT_ANDROID
#include <game-activity/native_app_glue/android_native_app_glue.h>
#endif

namespace onart{

    const std::chrono::steady_clock::time_point longTp = std::chrono::steady_clock::now();
    float Game::_dt = 0.016f, Game::_idt = 60.0f;
    uint64_t Game::_tp = (longTp - std::chrono::steady_clock::time_point()).count();
    const float &Game::dt(Game::_dt), &Game::idt(Game::_idt);
    const uint64_t& Game::tp(Game::_tp);
    int32_t Game::_frame = 1;
    const int32_t& Game::frame(Game::_frame);
    YRGraphics* Game::vk = nullptr;
    Window* Game::window = nullptr;
    void* Game::hd = nullptr;

#ifndef YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
#define YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD (BOOST_PLAT_ANDROID)
#else
#undef YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
#define YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD 1
#endif

    static std::mutex eventQMutex;

    enum class WindowEvent{ WE_SIZE = 0, WE_KEYBOARD = 1, WE_CLICK = 2, WE_CURSOR = 3, WE_SCROLL = 4 };
    struct EV{
        WindowEvent sType;
        union{
            struct{int sizeX, sizeY;};
            struct{int key, scancode, action, mods;};
            struct{int mouseKey, mouseAction, mouseMods;};
            struct{double mouseX, mouseY;};
            struct{double scrollX, scrollY;};
        };
    };

    static std::vector<EV> eventQ;
    static std::vector<EV> tempQ;

    static void recordSizeEvent(int x, int y) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_SIZE;
        back.sizeX = x;
        back.sizeY = y;
    }

    static void recordKeyEvent(int key, int scancode, int action, int mods) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_KEYBOARD;
        back.key = key;
        back.scancode = scancode;
        back.action = action;
        back.mods = mods;
    }

    static void recordClickEvent(int key, int action, int mods) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_CLICK;
        back.mouseKey = key;
        back.mouseAction = action;
        back.mouseMods = mods;
    }

    static void recordCursorEvent(double x, double y) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_CURSOR;
        back.mouseX = x;
        back.mouseY = y;
    }

    static void recordScrollEvent(double x, double y) {
        std::unique_lock<std::mutex> _(eventQMutex);
        eventQ.push_back({});
        EV& back = eventQ.back();
        back.sType = WindowEvent::WE_SCROLL;
        back.scrollX = x;
        back.scrollY = y;
    }

    bool loaded = false; // async 테스트용
    static float thr = 1.f;

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

#if !YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
        std::thread gamethread([]() {
#endif
            window->setMainThread();
            vk = new YRGraphics();
            vk->addWindow(0, window);
            if (!init()) {
                delete window;
                Window::terminate();
                return;
            }
            for (;; _frame++) {
                pollEvents();
                if (window->windowShouldClose()) break;
                std::chrono::duration<uint64_t, std::nano> longDt = std::chrono::steady_clock::now() - longTp;
                // [0, 2^52 - 1] 범위 정수를 균등 간격의 [1.0, 2.0) 범위 double로 대응시키는 함수에 십억을 적용한 후 1을 뺀 결과에 곱하여 1로 만들 수 있는 수
                constexpr double ONE_SECOND = 1.0 / 0.0000002220446049250313080847263336181640625;
                
                uint64_t ndt = longDt.count() - _tp;
                double ddt = (fixedPointConversion64i(ndt) - 1.0) * ONE_SECOND; // 함수를 사용할 수 없는 ndt의 상한선: 4,503,599,627,370,496 > 52일
                double iddt = 1.0 / ddt;
                _tp = longDt.count();
                _dt = static_cast<float>(ddt);
                _idt = static_cast<float>(iddt);
                YRGraphics::handle();
                auto off = YRGraphics::getRenderPass(0);
                auto rp2s = YRGraphics::getRenderPass2Screen(1);
                auto vb = YRGraphics::getMesh(0);
                auto tx = YRGraphics::getTexture(0);
                int x, y;
                if (Input::isKeyDown(Input::KeyCode::down)) {
                    thr -= 0.01f;
                    if (thr < 0) thr = 0.0f;

                }
                if (Input::isKeyDown(Input::KeyCode::up)) {
                    thr += 0.01f;
                    if (thr > 4)  thr = 4.0f;
                }
                printf("%f\r", thr);
                window->getFramebufferSize(&x, &y);
                ivec2 scr(x, y);
                float aspect = 1.0f;// (float)x / y;
                float pushed = PI<float> / 2;// std::abs(std::sin((double)_tp * 0.000000001));
                mat4 rot = mat4(1, 0, 0, 0, 0, aspect, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1) * mat4::rotate(0, 0, (double)_tp * 0.000000001);
                off->start();
                if (loaded) {
                    off->push(&rot, 0, 64);
                    off->push(&thr, 64, 68);
                    off->bind(0, tx);
                    off->invoke(vb);
                }
                off->start();
                off->invoke(vb);
                off->execute();
                rp2s->start();
                rp2s->push(&rot, 0, 64);
                rp2s->push(&thr, 64, 68);
                rp2s->bind(0, off);
                rp2s->invoke(vb);
                rp2s->execute(off);
            }
#if !YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
        });
        while(!window->windowShouldClose()) { window->waitEvents(); }
        gamethread.join();
#endif

        finalize();
        return 0;
    }

    void Game::pollEvents(){
#if YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
        window->pollEvents();
#else
        eventQMutex.lock();
        tempQ.swap(eventQ);
        eventQMutex.unlock();
        int sx = -1, sy = -1;
        double cx = -1, cy = -1;
        for(EV& ev: tempQ) {
            switch(ev.sType){
                case WindowEvent::WE_SIZE:
                    sx = ev.sizeX;
                    sy = ev.sizeY;
                    break;
                case WindowEvent::WE_KEYBOARD:
                    Input::keyboard(ev.key, ev.scancode, ev.action, ev.mods);
                    break;
                case WindowEvent::WE_CLICK:
                    Input::click(ev.mouseKey, ev.mouseAction, ev.mouseMods);
                    break;
                case WindowEvent::WE_CURSOR:
                    cx = ev.mouseX;
                    cy = ev.mouseY;
                    break;
                case WindowEvent::WE_SCROLL:
                    // TODO. ev.scrollX, ev.scrollY
                    break;
            }
        }
        tempQ.clear();
        if(sx != -1) {
            windowResized(sx, sy);
        }
        if(cx != -1) {
            Input::moveCursor(cx, cy);
        }
#endif
    }

    void Game::finalize(){
        Audio::finalize();
        delete vk;
        delete window; // Window보다 스왑체인을 먼저 없애야 함 (안 그러면 X11에서 막혀서 프로그램이 안 끝남)
        window = nullptr;
        vk = nullptr;
        Window::terminate();
    }

    bool Game::init() {
        Audio::init();
#if YR_NO_NEED_TO_USE_SEPARATE_EVENT_THREAD
        window->clickCallback = Input::click;
        window->keyCallback = Input::keyboard;
        window->posCallback = Input::moveCursor;
        window->touchCallback = Input::touch;
        window->windowSizeCallback = windowResized;
#else
        window->clickCallback = recordClickEvent;
        window->keyCallback = recordKeyEvent;
        window->posCallback = recordCursorEvent;
        window->windowSizeCallback = recordSizeEvent;
        window->scrollCallback = recordScrollEvent;
#endif

        YRGraphics::createRenderPass2Cube(123, 512, 512, false, true);
        YRGraphics::RenderPassCreationOptions rpopts{};
        rpopts.width = 128;
        rpopts.height = 128;
        rpopts.linearSampled = false;
        rpopts.subpassCount = 2;
        auto offrp = YRGraphics::createRenderPass(0, rpopts);
        rpopts.subpassCount = 1;
        auto rp2s = YRGraphics::createRenderPass2Screen(1, 0, rpopts);
        using testv_t = YRGraphics::Vertex<vec3, vec2>;
        //YRGraphics::createTexture(0, TEX0, sizeof(TEX0));
        //loaded = true;
        YRGraphics::asyncCreateTexture(0, TEX0, sizeof(TEX0), [](variant8) { loaded = true; });
        testv_t verts[]{ {{-1,-1,0},{0,0}},{{-1,1,0},{0,1}},{{1,-1,0},{1,0}},{{1,1,0},{1,1}} };
        uint16_t inds[]{ 0,1,2,2,1,3 };
        YRGraphics::MeshCreationOptions mopts;
        mopts.fixed = true;
        mopts.vertexCount = sizeof(verts) / sizeof(verts[0]);
        mopts.vertices = verts;
        mopts.indices = inds;
        mopts.indexCount = sizeof(inds) / sizeof(inds[0]);
        mopts.singleVertexSize = sizeof(verts[0]);
        mopts.singleIndexSize = sizeof(inds[0]);
        YRGraphics::createMesh(0, mopts);
        YRGraphics::createNullMesh(3, 1);
#ifdef YR_USE_VULKAN
        if constexpr (YRGraphics::VULKAN_GRAPHICS) {
            VkVertexInputAttributeDescription desc[2];
            testv_t::info(desc, 0);
            auto vs = YRGraphics::createShader(0, { TEST_VERT, sizeof(TEST_VERT) });
            auto fs = YRGraphics::createShader(1, { TEST_FRAG, sizeof(TEST_FRAG) });
            YRGraphics::PipelineCreationOptions pipeInfo{};
            pipeInfo.vertexSpec = desc;
            pipeInfo.vertexAttributeCount = 2;
            pipeInfo.vertexSize = sizeof(testv_t);
            pipeInfo.pass = offrp;
            pipeInfo.subpassIndex = 0;
            pipeInfo.vertexShader = vs;
            pipeInfo.fragmentShader = fs;
            pipeInfo.shaderResources.pos0 = YRGraphics::ShaderResourceType::TEXTURE_1;
            pipeInfo.shaderResources.usePush = true;
            YRGraphics::createPipeline(0, pipeInfo);
            vs = YRGraphics::createShader(2, { TEST_IA_VERT, sizeof(TEST_IA_VERT) });
            fs = YRGraphics::createShader(3, { TEST_IA_FRAG, sizeof(TEST_IA_FRAG) });
            pipeInfo.vertexShader = vs;
            pipeInfo.fragmentShader = fs;
            pipeInfo.subpassIndex = 1;
            pipeInfo.shaderResources.pos0 = YRGraphics::ShaderResourceType::INPUT_ATTACHMENT_1;
            pipeInfo.vertexSpec = nullptr;
            pipeInfo.vertexAttributeCount = 0;
            YRGraphics::createPipeline(1, pipeInfo);
            vs = YRGraphics::createShader(4, { TEST_TX_VERT, sizeof(TEST_TX_VERT) });
            fs = YRGraphics::createShader(5, { SCALEPX, sizeof(SCALEPX) });
            pipeInfo.vertexShader = vs;
            pipeInfo.fragmentShader = fs;
            pipeInfo.shaderResources.pos0 = YRGraphics::ShaderResourceType::TEXTURE_1;
            pipeInfo.pass = nullptr;
            pipeInfo.pass2screen = rp2s;
            pipeInfo.subpassIndex = 0;
            YRGraphics::createPipeline(2, pipeInfo);
            //YRGraphics::createTextureFromImage("g256.png", 0,YRGraphics::isSurfaceSRGB(),YRGraphics::IT_USE_ORIGINAL,false); loaded = true;
            //loaded = true; YRGraphics::createTexture(TEX0, sizeof(TEX0), 4, 0, YRGraphics::isSurfaceSRGB());
        }
#elif defined(YR_USE_OPENGL)
        const char TEST_GL_VERT1[] = R"(
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTc;

layout(location = 0) out vec2 tc;

layout(std140, binding=11) uniform ui{
    mat4 aspect;
    float t;
};

void main() {
    gl_Position = aspect * vec4(inPosition, 1.0);
    tc = inTc;
}
)";

        const char TEST_GL_FRAG1[] = R"(
#version 450

layout(location = 0) in vec2 tc;

out vec4 outColor;
layout(binding = 0) uniform sampler2D tex;

layout(std140, binding=11) uniform ui{
    mat4 aspect;
    float t;
};

void main() {
    outColor = texture(tex, tc);
}
)";
        if constexpr (YRGraphics::OPENGL_GRAPHICS) {
            YRGraphics::ShaderModuleCreationOptions shaderOpts;
            shaderOpts.size = sizeof(TEST_GL_VERT1);
            shaderOpts.source = TEST_GL_VERT1;
            shaderOpts.stage = YRGraphics::ShaderStage::VERTEX;
            auto vs = YRGraphics::createShader(0, shaderOpts);
            shaderOpts.size = sizeof(TEST_GL_FRAG1);
            shaderOpts.source = TEST_GL_FRAG1;
            shaderOpts.stage = YRGraphics::ShaderStage::FRAGMENT;
            auto fs = YRGraphics::createShader(1, shaderOpts);
            YRGraphics::PipelineInputVertexSpec sp[2];
            testv_t::info(sp);
            YRGraphics::PipelineCreationOptions opts;
            opts.vertexShader = vs;
            opts.fragmentShader = fs;
            opts.vertexAttributeCount = 2;
            opts.vertexSize = sizeof(testv_t);
            opts.vertexSpec = sp;
            opts.pass = offrp;
            opts.subpassIndex = 0;
            auto pp = YRGraphics::createPipeline(0, opts);
            offrp->usePipeline(pp, 0);
            offrp->usePipeline(pp, 1);
            rp2s->usePipeline(pp, 0);
        }
#elif defined(YR_USE_D3D11)
        const char TEST_D11_VERT1[] = R"(
struct VS_INPUT{
    float3 inPosition: _0_;
    float2 inTc: _1_;
};

struct PS_INPUT{
    float4 pos: SV_POSITION;
    float2 tc: TEXCOORD0;
};

cbuffer _0: register(b13){
    float4x4 aspect;
    float t;
}

PS_INPUT main(VS_INPUT input) {
    PS_INPUT ret = (PS_INPUT)0;
    ret.pos = mul(float4(input.inPosition, 1.0), aspect);
    ret.tc = input.inTc;
    return ret;
}
)";

        const char TEST_D11_FRAG1[] = R"(

struct PS_INPUT{
    float4 pos: SV_POSITION;
    float2 tc: TEXCOORD0;
};

Texture2D tex: register(t0);
SamplerState spr: register(s0);

cbuffer _0: register(b13){
    float4x4 aspect;
    float t;
}

float4 main(PS_INPUT input): SV_TARGET {
    return tex.Sample(spr, input.tc);
}
)";
        ID3DBlob* vsb = compileShader(TEST_D11_VERT1, sizeof(TEST_D11_VERT1), YRGraphics::ShaderStage::VERTEX);
        ID3DBlob* psb = compileShader(TEST_D11_FRAG1, sizeof(TEST_D11_FRAG1), YRGraphics::ShaderStage::FRAGMENT);
        YRGraphics::ShaderModuleCreationOptions shopts;
        shopts.source = vsb->GetBufferPointer();
        shopts.size = vsb->GetBufferSize();
        shopts.stage = YRGraphics::ShaderStage::VERTEX;
        auto vs = YRGraphics::createShader(0, shopts);
        shopts.source = psb->GetBufferPointer();
        shopts.size = psb->GetBufferSize();
        shopts.stage = YRGraphics::ShaderStage::FRAGMENT;
        auto ps = YRGraphics::createShader(1, shopts);
        YRGraphics::PipelineInputVertexSpec desc[2];
        testv_t::info(desc, 0);
        YRGraphics::PipelineCreationOptions pipeInfo{};
        pipeInfo.vertexSpec = desc;
        pipeInfo.vertexAttributeCount = 2;
        pipeInfo.vertexSize = sizeof(testv_t);
        pipeInfo.pass = offrp;
        pipeInfo.subpassIndex = 0;
        pipeInfo.vertexShader = vs;
        pipeInfo.fragmentShader = ps;
        pipeInfo.vsByteCode = vsb->GetBufferPointer();
        pipeInfo.vsByteCodeSize = vsb->GetBufferSize();
        pipeInfo.shaderResources.pos0 = YRGraphics::ShaderResourceType::TEXTURE_1;
        pipeInfo.shaderResources.usePush = true;
        auto pp = YRGraphics::createPipeline(0, pipeInfo);
        offrp->usePipeline(pp, 0);
        offrp->usePipeline(pp, 1);
        rp2s->usePipeline(pp, 0);
        vsb->Release();
        psb->Release();
#endif
        return true;
    }

    void Game::windowResized(int x, int y){
#if BOOST_PLAT_ANDROID
        vk->resetWindow(0, true);
#else
        vk->resetWindow(0);
#endif
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