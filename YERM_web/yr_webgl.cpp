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

#include "yr_webgl.h"

#include <emscripten.h>
#include "../YERM_PC/logger.hpp"
#include "../YERM_PC/yr_sys.h"

#define GLFW_INCLUDE_ES3
#include "../externals/glfw/include/GLFW/glfw3.h"
#include "../externals/boost/predef/platform.h"
#include "../externals/single_header/stb_image.h"

#ifndef KHRONOS_STATIC
#define KHRONOS_STATIC
#endif
#include "../externals/ktx/include/ktx.h"

#include <algorithm>
#include <vector>
#include <unordered_set>

// extensions
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#endif

#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM_ARB
#define GL_COMPRESSED_RGBA_BPTC_UNORM_ARB 0x8E8C
#define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB 0x8E8D
#define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB 0x8E8E
#define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB 0x8E8F
#endif

#ifndef GL_COMPRESSED_RED_RGTC1
#define GL_COMPRESSED_RED_RGTC1 0x8DBB
#define GL_COMPRESSED_SIGNED_RED_RGTC1 0x8DBC
#define GL_COMPRESSED_RG_RGTC2 0x8DBD
#define GL_COMPRESSED_SIGNED_RG_RGTC2 0x8DBE
#endif

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif /* GL_EXT_texture_compression_s3tc */

#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0x8C4C
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0x8C4D
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0x8C4E
#define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0x8C4F
#endif

// https://registry.khronos.org/OpenGL-Refpages/es3.0/
// https://registry.khronos.org/OpenGL-Refpages/es3/

namespace onart
{

    static void enableAttribute(int stride, const WGLMachine::PipelineInputVertexSpec& type);
    /// @brief 주어진 기반 형식과 아귀가 맞는, 현재 장치에서 사용 가능한 압축 형식을 리턴합니다.
    static int textureFormatFallback(uint32_t nChannels, bool srgb, bool hq);
    /// @brief OpenGL 에러 코드를 스트링으로 표현합니다. 리턴되는 문자열은 텍스트(코드) 영역에 존재합니다.
    inline static const char* resultAsString(unsigned);

    static int getGLBlendFactorConstant(BlendFactor factor) {
        constexpr static int consts[] = { GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA, GL_SRC_ALPHA_SATURATE };
        return consts[(int)factor];
    }

    static int getGLBlendOpConstant(BlendOperator op) {
        constexpr static int consts[] = { GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, GL_MIN, GL_MAX };
        return consts[(int)op];
    }

    static std::unordered_set<int> availableTextureFormats;

    static void checkTextureAvailable() {
        //glEnable(GL_DEBUG_OUTPUT);
        //glDebugMessageCallback(textureChecker, 0);
        ktxTextureCreateInfo info{};
        info.baseDepth = 1;
        info.baseWidth = 1024;
        info.baseHeight = 1024;
        info.numFaces = 1;
        info.numLayers = 1;
        info.numLevels = 1;
        info.numDimensions = 2;
        int count;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
        std::vector<int> availableFormat(count);
        glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, availableFormat.data());
        unsigned tex, target, err;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        ktxTexture1* texture{};
        for (int fmt : availableFormat) {
            info.glInternalformat = fmt;
            if(ktxTexture1_Create(&info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) != KTX_SUCCESS){
                continue;
            }
            ktxTexture_GLUpload(ktxTexture(texture), &tex, &target, &err);
            ktxTexture_Destroy(ktxTexture(texture));
            if (err == GL_NO_ERROR) { 
                availableTextureFormats.insert(fmt);                
            }
            else{
                LOGWITH(fmt);
            }
        }
        glDeleteTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, 0);
        //glDisable(GL_DEBUG_OUTPUT);
    }

    int32_t WGLMachine::currentWindowContext = INT32_MIN;
    WGLMachine* WGLMachine::singleton = nullptr;
    thread_local unsigned WGLMachine::reason = GL_NO_ERROR;

    const WGLMachine::Mesh* bound = nullptr;

    WGLMachine::WGLMachine(): loadThread(0){
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ZERO);
        singleton = this;

        checkTextureAvailable();

        UniformBufferCreationOptions uopts;
        uopts.size = 128;

        pUniformBuffer push = createUniformBuffer(INT32_MIN + 1, uopts);
        if (!push) {
            singleton = nullptr;
            return;
        }
        glBindBufferRange(GL_UNIFORM_BUFFER, 11, push->ubo, 0, 128);
    }

    WGLMachine::pPipeline WGLMachine::getPipeline(int32_t name){
        auto it = singleton->pipelines.find(name);
        if(it != singleton->pipelines.end()) return it->second;
        else return {};
    }

    WGLMachine::pMesh WGLMachine::getMesh(int32_t name) {
        auto it = singleton->meshes.find(name);
        if(it != singleton->meshes.end()) return it->second;
        else return pMesh();
    }

    void WGLMachine::dropRenderPass2Screen(int32_t key){
        singleton->finalPasses.erase(key);
    }

    void WGLMachine::dropRenderPass(int32_t key){
        singleton->renderPasses.erase(key);
    }

    void WGLMachine::dropShaderModule(int32_t key){
        auto it = singleton->shaders.find(key);
        if(it == singleton->shaders.end()) return;
        glDeleteShader(it->second);
        singleton->shaders.erase(it);
    }

    void WGLMachine::reap() {
        
    }

    WGLMachine::pUniformBuffer WGLMachine::getUniformBuffer(int32_t name){
        auto it = singleton->uniformBuffers.find(name);
        if(it != singleton->uniformBuffers.end()) return it->second;
        else return nullptr;
    }

    WGLMachine::pRenderPass2Screen WGLMachine::getRenderPass2Screen(int32_t name){
        auto it = singleton->finalPasses.find(name);
        if(it != singleton->finalPasses.end()) return it->second;
        else return nullptr;
    }

    WGLMachine::pRenderPass WGLMachine::getRenderPass(int32_t name){
        auto it = singleton->renderPasses.find(name);
        if(it != singleton->renderPasses.end()) return it->second;
        else return nullptr;
    }

    WGLMachine::pRenderPass2Cube WGLMachine::getRenderPass2Cube(int32_t name){
        auto it = singleton->cubePasses.find(name);
        if(it != singleton->cubePasses.end()) return it->second;
        else return nullptr;
    }

    unsigned WGLMachine::getShader(int32_t name){
        auto it = singleton->shaders.find(name);
        if(it != singleton->shaders.end()) return it->second;
        else return 0;
    }

    WGLMachine::pTexture WGLMachine::getTexture(int32_t name){
        auto it = singleton->textures.find(name);
        if (it != singleton->textures.end()) return it->second;
        else return pTexture();
    }

    WGLMachine::WindowSystem::WindowSystem(Window* window):window(window) {
        window->getFramebufferSize((int*)&width, (int*)&height);
    }

    bool WGLMachine::addWindow(int32_t key, Window* window) {
        if (windowSystems.find(key) != windowSystems.end()) { return true; }
        auto w = new WindowSystem(window);
        windowSystems[key] = w;
        if (windowSystems.size() == 1) { 
            window->setMainThread();
            currentWindowContext = key;
        }
        return true;
    }

    void WGLMachine::removeWindow(int32_t key) {
        for (auto it = finalPasses.begin(); it != finalPasses.end();) {
            if (it->second->windowIdx == key) {
                finalPasses.erase(it++);
            }
            else {
                ++it;
            }
        }
        windowSystems.erase(key);
    }

    void WGLMachine::setVsync(bool vsync) {
        if (singleton->vsync != vsync) {
            singleton->vsync = vsync;
            for (auto& w : singleton->windowSystems) {
                w.second->window->glRefreshInterval(vsync ? 1 : 0);
            }
        }
    }

    void WGLMachine::resetWindow(int32_t key, bool) {
        auto it = singleton->windowSystems.find(key);
        if (it == singleton->windowSystems.end()) { return; }
        WindowSystem* ws = it->second;
        ws->window->getFramebufferSize((int*)&ws->width, (int*)&ws->height);
        for (auto& renderPass : finalPasses) {
            renderPass.second->resize(ws->width, ws->height);
        }
    }

    void WGLMachine::free() {
        for(auto& sh: shaders) { glDeleteShader(sh.second); }
        for (auto& ws : windowSystems) { delete ws.second; }

        streamTextures.clear();
        textures.clear();
        meshes.clear();
        pipelines.clear();
        cubePasses.clear();
        finalPasses.clear();
        renderPasses.clear();
        shaders.clear();
    }

    void WGLMachine::handle() {
        singleton->loadThread.handleCompleted();
    }

    void WGLMachine::post(std::function<variant8(void)> exec, std::function<void(variant8)> handler, uint8_t strand) {
        singleton->loadThread.post(exec, handler, strand);
    }

    WGLMachine::~WGLMachine(){
        free();
    }

    WGLMachine::pMesh WGLMachine::createNullMesh(int32_t name, size_t vcount) {
        pMesh m = getMesh(name);
        if(m) { return m; }
        struct publicmesh:public Mesh{publicmesh(unsigned _1, unsigned _2, size_t _3, size_t _4, bool _5):Mesh(_1,_2,_3,_4,_5){}};
        pMesh ret = std::make_shared<publicmesh>(0, 0, vcount, 0, false);
        if(name == INT32_MIN) return ret;
        return singleton->meshes[name] = ret;
    }

    WGLMachine::pMesh WGLMachine::createMesh(int32_t key, const MeshCreationOptions& opts) {
        if (pMesh m = getMesh(key)) { return m; }

        unsigned vb, ib = 0;
        glGenBuffers(1, &vb);
        if (vb == 0) {
            LOGWITH("Failed to create vertex buffer");
            return {};
        }
        
        if (opts.indexCount != 0 && opts.singleIndexSize != 2 && opts.singleIndexSize != 4) {
            LOGWITH("Invalid isize");
            return pMesh();
        }

        if (opts.indexCount != 0) {
            glGenBuffers(1, &ib);
            if (ib == 0) {
                LOGWITH("Failed to create index buffer");
                glDeleteBuffers(1, &vb);
                return {};
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, opts.singleIndexSize * opts.indexCount, opts.indices, opts.fixed ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferData(GL_ARRAY_BUFFER, opts.singleVertexSize * opts.vertexCount, opts.vertices, opts.fixed ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        struct publicmesh :public Mesh { publicmesh(unsigned _1, unsigned _2, size_t _3, size_t _4, bool _5) :Mesh(_1, _2, _3, _4, _5) {} };
        return singleton->meshes[key] = std::make_shared<publicmesh>(vb, ib, opts.vertexCount, opts.indexCount, opts.singleIndexSize == 4);
    }

    WGLMachine::RenderTarget* WGLMachine::createRenderTarget2D(int width, int height, RenderTargetType type, bool useDepthInput, bool linear) {
        unsigned color1{}, color2{}, color3{}, ds{}, fb{};
        glGenFramebuffers(1, &fb);
        if (fb == 0) {
            LOGWITH("Failed to create framebuffer:", reason, resultAsString(reason));
            return nullptr;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        if ((int)type & 0b1) {
            glGenTextures(1, &color1);
            if (color1 == 0) {
                LOGWITH("Failed to create image:", reason, resultAsString(reason));
                return nullptr;
            }
            glBindTexture(GL_TEXTURE_2D, color1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color1, 0);
            if ((int)type & 0b10) {
                glGenTextures(1, &color2);
                if (color2 == 0) {
                    LOGWITH("Failed to create image:", reason, resultAsString(reason));
                    glDeleteTextures(1, &color1);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    return nullptr;
                }
                glBindTexture(GL_TEXTURE_2D, color2);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, color2, 0);
                if ((int)type & 0b100) {
                    glGenTextures(1, &color3);
                    if (color3 == 0) {
                        LOGWITH("Failed to create image:", reason, resultAsString(reason));
                        glDeleteTextures(1, &color1);
                        glDeleteTextures(1, &color2);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        glBindFramebuffer(GL_FRAMEBUFFER, 0);
                        return nullptr;
                    }
                    glBindTexture(GL_TEXTURE_2D, color3);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, color3, 0);
                }
            }
        }
        else {
            const GLenum glNone = GL_NONE;
            glDrawBuffers(1, &glNone);
        }
        if ((int)type & 0b1000) {
            if (useDepthInput) {
                glGenTextures(1, &ds);
                if (ds == 0) {
                    LOGWITH("Failed to create image:", reason, resultAsString(reason));
                    if (color1) glDeleteTextures(1, &color1);
                    if (color2) glDeleteTextures(1, &color2);
                    if (color3) glDeleteTextures(1, &color3);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    return nullptr;
                }
                glBindTexture(GL_TEXTURE_2D, ds);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, ds, 0);
            }
            else {
                glGenRenderbuffers(1, &ds);
                if (ds == 0) {
                    LOGWITH("Failed to create renderbuffer:", reason, resultAsString(reason));
                    if (color1) glDeleteTextures(1, &color1);
                    if (color2) glDeleteTextures(1, &color2);
                    if (color3) glDeleteTextures(1, &color3);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    return nullptr;
                }
                glBindRenderbuffer(GL_RENDERBUFFER, ds);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, ds);
            }
        }
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOGWITH("Framebuffer incomplete");
            if (color1) glDeleteTextures(1, &color1);
            if (color2) glDeleteTextures(1, &color2);
            if (color3) glDeleteTextures(1, &color3);
            if (ds) {
                if (useDepthInput) { glDeleteTextures(1, &ds); }
                else { glDeleteRenderbuffers(1, &ds); }
            }
            glDeleteFramebuffers(1, &fb);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return nullptr;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return new RenderTarget(type, width, height, color1, color2, color3, ds, useDepthInput, fb);
    }

    unsigned WGLMachine::createShader(int32_t key, const ShaderModuleCreationOptions& opts) {
        if (unsigned ret = getShader(key)) { return ret; }
        unsigned shType;
        switch (opts.stage) {
        case ShaderStage::VERTEX:
            shType = GL_VERTEX_SHADER;
            break;
        case ShaderStage::FRAGMENT:
            shType = GL_FRAGMENT_SHADER;
            break;
            /*
        case ShaderStage::GEOMETRY:
            shType = GL_GEOMETRY_SHADER;
            break;
        case ShaderStage::TESS_CTRL:
            shType = GL_TESS_CONTROL_SHADER;
            break;
        case ShaderStage::TESS_EVAL:
            shType = GL_TESS_EVALUATION_SHADER;
            break;
            */
        default:
            LOGWITH("Invalid shader type");
            return 0;
        }
        unsigned prog = glCreateShader(shType);
        int sz = (int)opts.size;
        glShaderSource(prog, 1, (const char**)&opts.source, &sz);
        glCompileShader(prog);
        int buf;
        glGetShaderiv(prog, GL_COMPILE_STATUS, &buf);
        if (buf != GL_TRUE) {
            LOGWITH("Shader compilation error:");
            glGetShaderiv(prog, GL_INFO_LOG_LENGTH, &buf);
            if (buf > 0 && buf < 4096) {
                char log[4096]{};
                int length;
                glGetShaderInfoLog(prog, buf, &length, log);
                LOGWITH(log);
            }
            return 0;
        }
        return singleton->shaders[key] = prog;
    }

    static ktx_error_code_e tryTranscode(ktxTexture2* texture, uint32_t nChannels, bool srgb, bool hq) {
        if (ktxTexture2_NeedsTranscoding(texture)) {
            ktx_transcode_fmt_e tf;
            uint32_t vkf{};
            switch (textureFormatFallback(nChannels, srgb, hq))
            {
                
            case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
            case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
                tf = KTX_TTF_ASTC_4x4_RGBA;
                vkf = srgb ? VK_FORMAT_ASTC_4x4_SRGB_BLOCK : VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
                break;
            case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
            case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
                tf = KTX_TTF_BC7_RGBA;
                vkf = srgb ? VK_FORMAT_BC7_SRGB_BLOCK : VK_FORMAT_BC7_UNORM_BLOCK;
                break;
                
            case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
            case GL_COMPRESSED_RGBA8_ETC2_EAC:
                tf = KTX_TTF_ETC2_RGBA;
                vkf = srgb ? VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK : VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
                break;
                
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                tf = KTX_TTF_BC3_RGBA;
                vkf = srgb ? VK_FORMAT_BC3_SRGB_BLOCK : VK_FORMAT_BC3_UNORM_BLOCK;
                break;
                
            default:
                tf = KTX_TTF_RGBA32;
                vkf = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
                break;
            }
            ktx_error_code_e ret = ktxTexture2_TranscodeBasis(texture, tf, 0);
            texture->vkFormat = vkf;
            return ret;
        }
        return KTX_SUCCESS;
    }

    WGLMachine::pTexture WGLMachine::createTexture(void* ktxObj, int32_t key, const TextureCreationOptions& opts) {
        ktxTexture2* texture = reinterpret_cast<ktxTexture2*>(ktxObj);
        if (texture->numLevels == 0) return pTexture();
        ktx_error_code_e k2result = tryTranscode(texture, opts.nChannels, opts.srgb, opts.opts == TextureFormatOptions::IT_PREFER_QUALITY);
        if (k2result != KTX_SUCCESS) {
            LOGWITH("Failed to transcode ktx texture:", k2result);
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
        unsigned tex = 0, target, glError;
        k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &target, &glError);
        if (k2result != KTX_SUCCESS) {
            LOGWITH("Failed to transcode ktx texture:", k2result, glError);
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
        uint16_t width = texture->baseWidth, height = texture->baseHeight;
        ktxTexture_Destroy(ktxTexture(texture));

        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, opts.linearSampled ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, opts.linearSampled ? GL_LINEAR : GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        struct txtr :public Texture { inline txtr(uint32_t _1, uint16_t _2, uint16_t _3) :Texture(_1, _2, _3) {} };
        if (key == INT32_MIN) return std::make_shared<txtr>(tex, width, height);
        return textures[key] = std::make_shared<txtr>(tex, width, height);
    }

    WGLMachine::pTextureSet WGLMachine::createTextureSet(int32_t key, const pTexture& binding0, const pTexture& binding1, const pTexture& binding2, const pTexture& binding3) {
        if (!binding0 || !binding1) {
            LOGWITH("At least 2 textures must be given");
            return {};
        }
        int length = binding2 ? (binding3 ? 4 : 3) : 2;

        pTexture textures[4] = { binding0, binding1, binding2, binding3 };

        struct __tset :public TextureSet {};
        pTextureSet ret = std::make_shared<__tset>();
        ret->textureCount = length;
        ret->textures[0] = textures[0];
        ret->textures[1] = textures[1];
        ret->textures[2] = textures[2];
        ret->textures[3] = textures[3];
        if (key == INT32_MIN) return ret;
        return singleton->textureSets[key] = std::move(ret);
    }

    WGLMachine::pStreamTexture WGLMachine::createStreamTexture(int32_t key, uint32_t width, uint32_t height, bool linearSampler) {
        
        if ((width | height) == 0) return {};
        unsigned tex{};
        glGenTextures(1, &tex);
        if (tex == 0) {
            LOGWITH("Failed to create texture");
            return {};
        }
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
        
        unsigned pbo{};
        glGenBuffers(1, &pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 4, nullptr, GL_STREAM_DRAW);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        struct txtr :public StreamTexture { inline txtr(uint32_t _1, uint32_t _2, uint16_t _3, uint16_t _4) :StreamTexture(_1, _2, _3, _4) {} };
        if (key == INT32_MIN) return std::make_shared<txtr>(tex, pbo, width, height);
        return singleton->streamTextures[key] = std::make_shared<txtr>(tex, pbo, width, height);
    }

    WGLMachine::StreamTexture::StreamTexture(uint32_t txo, uint32_t pbo, uint16_t width, uint16_t height) :width(width), height(height), txo(txo), pbo(pbo) {
        
    }

    WGLMachine::StreamTexture::~StreamTexture() {
        glDeleteBuffers(1, &pbo);
        glDeleteTextures(1, &txo);
    }

    void WGLMachine::StreamTexture::update(void* src) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, src);
    }

    void WGLMachine::StreamTexture::updateBy(std::function<void(void*, uint32_t)> function) {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
        void* data = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, width * height * 4, GL_MAP_WRITE_BIT);
        function(data, width * 4);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }


    static ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, TextureFormatOptions option){
        ktxTexture2* texture;
        ktx_error_code_e k2result;
        ktxTextureCreateInfo texInfo{};
        texInfo.baseDepth = 1;
        texInfo.baseWidth = x;
        texInfo.baseHeight = y;
        texInfo.numFaces = 1;
        texInfo.numLevels = 1;
        texInfo.numDimensions = 2;
        texInfo.numLayers = 1;
        
        switch (nChannels)
        {
        case 1:
            texInfo.vkFormat = srgb ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
            break;
        case 2:
            texInfo.vkFormat = srgb ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
            break;
        case 3:
            texInfo.vkFormat = srgb ? VK_FORMAT_R8G8B8_SRGB : VK_FORMAT_R8G8B8_UNORM;
            break;
        case 4:
            texInfo.vkFormat = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            break;
        default:
            LOGWITH("nChannels should be 1~4");
            return nullptr;
        }
        if((k2result = ktxTexture2_Create(&texInfo, ktxTextureCreateStorageEnum::KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture)) != KTX_SUCCESS){
            LOGWITH("Failed to create texture:",k2result);
            return nullptr;
        }
        if((k2result = ktxTexture_SetImageFromMemory(ktxTexture(texture),0,0,0,pix,x*y*nChannels)) != KTX_SUCCESS){
            LOGWITH("Failed to set texture image data:",k2result);
            ktxTexture_Destroy(ktxTexture(texture));
            return nullptr;
        }
        if (option == TextureFormatOptions::IT_PREFER_COMPRESS) {
            ktxBasisParams params{};
            params.compressionLevel = 5;// KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL == 2;
            params.uastc = KTX_TRUE;
            params.verbose = KTX_FALSE;
            params.structSize = sizeof(params);

            k2result = ktxTexture2_CompressBasisEx(texture, &params);
            if(k2result != KTX_SUCCESS){
                LOGWITH("Compress failed:", k2result);
                ktxTexture_Destroy(ktxTexture(texture));
                return nullptr;
            }
        }
        return texture;
    }

    WGLMachine::pTexture WGLMachine::createTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, const TextureCreationOptions& opts) {
        if (auto tex = getTexture(key)) { return tex; }
        ktxTexture2* texture = createKTX2FromImage(color, width, height, opts.nChannels, opts.srgb, opts.opts);
        if (!texture) {
            LOGHERE;
            return {};
        }
        return singleton->createTexture(texture, key, opts);
    }

    void WGLMachine::aysncCreateTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        if (getTexture(key)) {
            variant8 _k;
            _k.bytedata4[0] = key;
            handler(_k);
            return;
        }
        TextureCreationOptions options = opts;
        singleton->loadThread.post([key, color, width, height, options]() {
            pTexture ret = singleton->createTextureFromColor(INT32_MIN, color, width, height, options);
            if (!ret) {
                variant8 _k;
                _k.bytedata4[0] = key;
                _k.bytedata4[1] = reason;
                return _k;
            }
            singleton->textureGuard.lock();
            singleton->textures[key] = std::move(ret);
            singleton->textureGuard.unlock();
            variant8 _k;
            _k.bytedata4[0] = key;
            return _k;
            }, handler, vkm_strand::GENERAL);
    }

    WGLMachine::pTexture WGLMachine::createTextureFromImage(int32_t key, const char* fileName, const TextureCreationOptions& opts) {
        int x, y, nChannels;
        uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return pTexture();
        }
        TextureCreationOptions channelOpts = opts;
        channelOpts.nChannels = nChannels;
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, opts.srgb, opts.opts);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, channelOpts);
    }

    WGLMachine::pTexture WGLMachine::createTextureFromImage(int32_t key, const void* mem, size_t size, const TextureCreationOptions& opts) {
        int x, y, nChannels;
        uint8_t* pix = stbi_load_from_memory((const uint8_t*)mem, (int)size, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return pTexture();
        }
        TextureCreationOptions channelOpts = opts;
        channelOpts.nChannels = nChannels;
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, opts.srgb, opts.opts);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, channelOpts);
    }

    WGLMachine::pTexture WGLMachine::createTexture(int32_t key, const char* fileName, const TextureCreationOptions& opts) {
        if (opts.nChannels > 4 || opts.nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }
        if (pTexture ret = getTexture(key)) {
            return ret;
        }

        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if ((k2result = ktxTexture2_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
            LOGWITH("Failed to load ktx texture:", k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, opts);
    }

    WGLMachine::pTexture WGLMachine::createTexture(int32_t key, const uint8_t* mem, size_t size, const TextureCreationOptions& opts) {
        if (opts.nChannels > 4 || opts.nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }
        if (pTexture ret = getTexture(key)) { return ret; }

        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if ((k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
            LOGWITH("Failed to load ktx texture:", k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, opts);
    }

    void WGLMachine::asyncCreateTexture(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        if (getTexture(key)) {
            variant8 _k;
            _k.bytedata2[0] = key;
            handler(_k);
            return;
        }
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        TextureCreationOptions options = opts;
        singleton->loadThread.post([fileName, options]()->variant8 {
            ktxTexture2* texture;
            ktx_error_code_e k2result;
            if ((k2result = ktxTexture2_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
                return new __asyncparam{ nullptr, k2result };
            }
            if ((k2result = tryTranscode(texture, options.nChannels, options.srgb, options.opts == TextureFormatOptions::IT_PREFER_QUALITY)) != KTX_SUCCESS) {
                return new __asyncparam{ nullptr, k2result };
            }
            return new __asyncparam{ texture, KTX_SUCCESS };
            }, [key, options, handler](variant8 param) { // upload on GL context thread
                if (!param.vp) {
                    handler((uint64_t)(uint32_t)key);
                }
                else {
                    __asyncparam* ap = reinterpret_cast<__asyncparam*>(param.vp);
                    ktxTexture2* texture = ap->texture;
                    int32_t k2result = ap->k2result;
                    delete ap;
                    if (k2result != KTX_SUCCESS) {
                        variant8 p;
                        p.bytedata2[0] = key;
                        p.bytedata2[1] = k2result;
                        handler(p);
                    }
                    else {
                        unsigned tex = 0, targ, err;
                        glGenTextures(1, &tex);
                        glBindTexture(GL_TEXTURE_2D, tex);
                        k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                        if (k2result != KTX_SUCCESS) {
                            LOGWITH("Failed to transcode ktx texture:", k2result, err);
                            ktxTexture_Destroy(ktxTexture(texture));
                        }
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                        glBindTexture(GL_TEXTURE_2D, 0);

                        struct txtr :public Texture { inline txtr(uint32_t _1, uint16_t _2, uint16_t _3) :Texture(_1, _2, _3) {} };
                        pTexture ret = std::make_shared<txtr>(tex, texture->baseWidth, texture->baseHeight);
                        singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                        ktxTexture_Destroy(ktxTexture(texture));
                        handler((uint64_t)(uint32_t)key);
                    }
                }
                }, vkm_strand::GENERAL);
    }

    void WGLMachine::asyncCreateTextureFromImage(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        if (getTexture(key)) {
            variant8 _k;
            _k.bytedata4[0] = key;
            handler(_k);
            return;
        }
        TextureCreationOptions options = opts;
        singleton->loadThread.post([fileName, options]()->variant8 {
            int x, y, nChannels;
            uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
            ktx_error_code_e k2result;
            if (!pix) {
                return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
            }
            ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, options.srgb, options.opts);
            stbi_image_free(pix);
            if (!texture) {
                return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
            }
            if ((k2result = tryTranscode(texture, nChannels, options.srgb, options.opts != TextureFormatOptions::IT_PREFER_COMPRESS)) != KTX_SUCCESS) {
                return new __asyncparam{ nullptr, k2result };
            }
            return new __asyncparam{ texture, KTX_SUCCESS };
            }, [key, handler, options](variant8 param) {
                __asyncparam* ap = reinterpret_cast<__asyncparam*>(param.vp);
                ktxTexture2* texture = ap->texture;
                int32_t k2result = ap->k2result;
                delete ap;
                if (k2result != KTX_SUCCESS) {
                    variant8 p;
                    p.u64 = (uint64_t)(uint32_t)key | ((uint64_t)k2result << 32);
                    handler(p);
                }
                else {
                    unsigned tex = 0, targ, err;
                    k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                    if (k2result != KTX_SUCCESS) {
                        LOGWITH("Failed to upload ktx texture:", k2result, err);
                        ktxTexture_Destroy(ktxTexture(texture));
                        variant8 _k;
                        _k.bytedata2[0] = key;
                        _k.bytedata2[1] = err;
                        handler(_k);
                        return;
                    }
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    struct txtr :public Texture { inline txtr(uint32_t _1, uint16_t _2, uint16_t _3) :Texture(_1, _2, _3) {} };
                    pTexture ret = std::make_shared<txtr>(tex, texture->baseWidth, texture->baseHeight);
                    singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                    ktxTexture_Destroy(ktxTexture(texture));
                    variant8 _k;
                    _k.bytedata2[0] = key;
                    handler(_k);
                }
            }, vkm_strand::GENERAL);
    }

    void WGLMachine::asyncCreateTextureFromImage(int32_t key, const void* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        if (getTexture(key)) {
            variant8 _k;
            _k.bytedata4[0] = key;
            handler(_k);
            return;
        }
        TextureCreationOptions options = opts;
        singleton->loadThread.post([mem, size, options]()->variant8 {
            int x, y, nChannels;
            uint8_t* pix = stbi_load_from_memory((const uint8_t*)mem, (int)size, &x, &y, &nChannels, 4);
            if (!pix) {
                return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
            }
            ktx_error_code_e k2result;
            ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, options.srgb, options.opts);
            stbi_image_free(pix);
            if (!texture) {
                return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
            }
            if ((k2result = tryTranscode(texture, nChannels, options.srgb, options.opts != TextureFormatOptions::IT_PREFER_COMPRESS)) != KTX_SUCCESS) {
                return new __asyncparam{ nullptr, k2result };
            }
            return new __asyncparam{ texture,k2result };
            }, [key, handler, options](variant8 param) {
                __asyncparam* ap = reinterpret_cast<__asyncparam*>(param.vp);
                ktxTexture2* texture = ap->texture;
                int32_t k2result = ap->k2result;
                delete ap;
                if (k2result != KTX_SUCCESS) {
                    variant8 p;
                    p.u64 = (uint64_t)(uint32_t)key | ((uint64_t)k2result << 32);
                    handler(p);
                }
                else {
                    unsigned tex = 0, targ, err;
                    k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                    if (k2result != KTX_SUCCESS) {
                        LOGWITH("Failed to transcode ktx texture:", k2result, err);
                        ktxTexture_Destroy(ktxTexture(texture));
                        variant8 _k;
                        _k.bytedata2[0] = key;
                        _k.bytedata2[1] = err;
                        handler(_k);
                        return;
                    }
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    struct txtr :public Texture { inline txtr(uint32_t _1, uint16_t _2, uint16_t _3) :Texture(_1, _2, _3) {} };
                    pTexture ret = std::make_shared<txtr>(tex, texture->baseWidth, texture->baseHeight);
                    ktxTexture_Destroy(ktxTexture(texture));
                    singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                    variant8 _k;
                    _k.bytedata2[0] = key;
                    handler(_k);
                    handler((uint64_t)(uint32_t)key);
                }
                }, vkm_strand::GENERAL);
    }

    void WGLMachine::asyncCreateTexture(int32_t key, const uint8_t* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        if (getTexture(key)) {
            variant8 _k;
            _k.bytedata2[0] = key;
            handler(_k);
        }
        TextureCreationOptions options = opts;
        singleton->loadThread.post([mem, size, options]()->variant8 {
            ktxTexture2* texture;
            ktx_error_code_e k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture);
            if (k2result != KTX_SUCCESS) {
                return new __asyncparam{ nullptr, k2result };
            }
            if ((k2result = tryTranscode(texture, options.nChannels, options.srgb, options.opts == TextureFormatOptions::IT_PREFER_QUALITY)) != KTX_SUCCESS) {
                return new __asyncparam{ nullptr, k2result };
            }
            return new __asyncparam{ texture, KTX_SUCCESS };
            }, [key, handler, options](variant8 param) {
                __asyncparam* ap = reinterpret_cast<__asyncparam*>(param.vp);
                ktxTexture2* texture = ap->texture;
                int32_t k2result = ap->k2result;
                delete ap;
                if (k2result != KTX_SUCCESS) {
                    variant8 p;
                    p.bytedata2[0] = (uint32_t)key;
                    p.bytedata2[1] = k2result;
                    handler(p);
                }
                else {
                    unsigned tex = 0, targ, err=0;
                    glBindTexture(GL_TEXTURE_2D, tex);
                    err = glGetError();
                    k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                    if (k2result != KTX_SUCCESS) {
                        LOGWITH(texture->vkFormat);
                        LOGWITH("Failed to upload ktx texture:", k2result, err);
                        ktxTexture_Destroy(ktxTexture(texture));
                    }
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, options.linearSampled ? GL_LINEAR : GL_NEAREST);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    struct txtr :public Texture { inline txtr(uint32_t _1, uint16_t _2, uint16_t _3) :Texture(_1, _2, _3) {} };
                    pTexture ret = std::make_shared<txtr>(tex, texture->baseWidth, texture->baseHeight);
                    singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                    ktxTexture_Destroy(ktxTexture(texture));
                    handler((uint64_t)(uint32_t)key);
                }
                }, vkm_strand::GENERAL);
    }

    WGLMachine::Texture::Texture(uint32_t txo, uint16_t width, uint16_t height) :width(width), height(height), txo(txo) { }
    WGLMachine::Texture::~Texture(){
        glDeleteTextures(1, &txo);
    }

    void WGLMachine::Texture::collect(bool removeUsing) {
        if(removeUsing) {
            singleton->textures.clear();
        }
        else{
            for(auto it = singleton->textures.cbegin(); it != singleton->textures.cend();){
                if(it->second.use_count() == 1){
                    singleton->textures.erase(it++);
                }
                else{
                    ++it;
                }
            }
        }
    }

    void WGLMachine::Texture::drop(int32_t name){
        singleton->textures.erase(name);
    }

    void WGLMachine::TextureSet::drop(int32_t key){
        singleton->textureSets.erase(key);
    }

    void WGLMachine::StreamTexture::collect(bool removeUsing) {
        if (removeUsing) {
            singleton->streamTextures.clear();
        }
        else {
            for (auto it = singleton->streamTextures.cbegin(); it != singleton->streamTextures.cend();) {
                if (it->second.use_count() == 1) {
                    singleton->streamTextures.erase(it++);
                }
                else {
                    ++it;
                }
            }
        }
    }

    void WGLMachine::StreamTexture::drop(int32_t name) {
        singleton->streamTextures.erase(name);
    }

    WGLMachine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, unsigned c1, unsigned c2, unsigned c3, unsigned ds, bool depthAsTexture, unsigned framebuffer)
        :color1(c1), color2(c2), color3(c3), depthStencil(ds), framebuffer(framebuffer), width(width), height(height), dsTexture(depthAsTexture), type(type) {
    }

    WGLMachine::pUniformBuffer WGLMachine::createUniformBuffer(int32_t key, const UniformBufferCreationOptions& opts) {
        if (pUniformBuffer ret = getUniformBuffer(key)) { return ret; }

        unsigned ubo;
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, opts.size, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        pUniformBuffer ret = std::make_shared<shp_t<UniformBuffer>>(opts.size, ubo);
        if(key == INT32_MIN) return ret;
        return singleton->uniformBuffers[key] = std::move(ret);
    }

    WGLMachine::RenderTarget::~RenderTarget(){
        if (color1) { glDeleteTextures(1, &color1); }
        if (color2) { glDeleteTextures(1, &color2); }
        if (color3) { glDeleteTextures(1, &color3); }
        if(depthStencil) { 
            if (dsTexture) {
                glDeleteTextures(1, &depthStencil);
            }
            else {
                glDeleteRenderbuffers(1, &depthStencil);
            }
        }
        if(framebuffer) glDeleteFramebuffers(1, &framebuffer);
    }

    WGLMachine::pRenderPass2Cube WGLMachine::createRenderPass2Cube(int32_t key, uint32_t width, uint32_t height, bool useColor, bool useDepth) {
        pRenderPass2Cube r = getRenderPass2Cube(key);
        if(r) return r;
        if(!(useColor || useDepth)){
            LOGWITH("At least one of useColor and useDepth should be true");
            return nullptr;
        }
        unsigned fbo;
        glGenFramebuffers(1, &fbo);
        if (fbo == 0) {
            LOGWITH("Failed to create framebuffer");
            return nullptr;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        unsigned color = 0, depth = 0;
        if (useColor) {
            glGenTextures(1, &color);
            if (color == 0) {
                LOGWITH("Failed to create texture");
                glDeleteFramebuffers(1, &fbo);
                return nullptr;
            }
            glBindTexture(GL_TEXTURE_CUBE_MAP, color);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            // gles <3.2 : Must use 6 calls of vertex shader draw
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, color, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }
        else {
            const GLenum glNone = GL_NONE;
            glDrawBuffers(1, &glNone);
            glReadBuffer(GL_NONE);
        }
        if (useDepth) {
            glGenTextures(1, &depth);
            if (depth == 0) {
                LOGWITH("Failed to create texture");
                glDeleteFramebuffers(1, &fbo);
                if (color) {
                    glDeleteTextures(1, &color);
                }
                return nullptr;
            }
            glBindTexture(GL_TEXTURE_CUBE_MAP, depth);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            // gles <3.2 : Must use 6 calls of vertex shader draw
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, depth, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOGWITH("Failed to create framebuffer");
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            if (color) glDeleteTextures(1, &color);
            if (depth) glDeleteTextures(1, &depth);
            glDeleteFramebuffers(1, &fbo);
            return nullptr;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        r = std::make_shared<shp_t<RenderPass2Cube>>();
        r->targetCubeC = color;
        r->targetCubeD = depth;
        r->fbo = fbo;
        r->viewport.x = 0;
        r->viewport.y = 0;
        r->viewport.width = (float)width;
        r->viewport.height = (float)height;
        r->viewport.minDepth = -1;
        r->viewport.maxDepth = 1;
        r->scissor.x = 0;
        r->scissor.y = 0;
        r->scissor.width = width;
        r->scissor.height = height;

        if(key == INT32_MIN) return r;
        return singleton->cubePasses[key] = std::move(r);
    }

    WGLMachine::pRenderPass2Screen WGLMachine::createRenderPass2Screen(int32_t key, int32_t windowIdx, const RenderPassCreationOptions& opts) {
        auto it = singleton->windowSystems.find(windowIdx);
        if (it == singleton->windowSystems.end()) {
            LOGWITH("Invalid window number");
            return nullptr;
        }
        WindowSystem* window = it->second;
        if (pRenderPass2Screen ret = getRenderPass2Screen(key)) { return ret; }
        if (opts.subpassCount == 0) { return nullptr; }

        std::vector<RenderTarget*> targets(opts.subpassCount);
        for (uint32_t i = 0; i < opts.subpassCount - 1; i++) {
            targets[i] = createRenderTarget2D(window->width, window->height, opts.targets[i], opts.depthInput, opts.linearSampled);
            if (!targets[i]) {
                LOGHERE;
                for (RenderTarget* t : targets) delete t;
                return nullptr;
            }
        }

        pRenderPass2Screen ret = std::make_shared<shp_t<RenderPass2Screen>>(opts.subpassCount, false, opts.autoclear.use ? (float*)opts.autoclear.color : nullptr);
        ret->targets = std::move(targets);
        ret->setViewport((float)window->width, (float)window->height, 0.0f, 0.0f);
        ret->setScissor(window->width, window->height, 0, 0);
        ret->windowIdx = windowIdx;
        ret->is4Screen = true;
        if(key == INT32_MIN) return ret;
        return singleton->finalPasses[key] = std::move(ret);
    }

    WGLMachine::pRenderPass WGLMachine::createRenderPass(int32_t key, const RenderPassCreationOptions& opts) {
        if (pRenderPass r = getRenderPass(key)) { return r; }
        if (opts.subpassCount == 0) { 
            return nullptr;
        }

        std::vector<RenderTarget*> targets(opts.subpassCount);
        for (uint32_t i = 0; i < opts.subpassCount; i++) {
            targets[i] = createRenderTarget2D(opts.width, opts.height, opts.targets ? opts.targets[i] : RenderTargetType::RTT_COLOR1, opts.depthInput, opts.linearSampled);
            if (!targets[i]) {
                LOGHERE;
                for (uint32_t j = 0; j < i; j++) {
                    delete targets[j];
                }
                return {};
            }
        }

        pRenderPass ret = std::make_shared<shp_t<RenderPass>>(opts.subpassCount, opts.canCopy, opts.autoclear.use ? (float*)opts.autoclear.color : nullptr);
        ret->targets = std::move(targets);
        ret->setViewport(opts.width, opts.height, 0.0f, 0.0f);
        ret->setScissor(opts.width, opts.height, 0, 0);
        if(key == INT32_MIN) return ret;
        return singleton->renderPasses[key] = std::move(ret);
    }

    WGLMachine::pPipeline WGLMachine::createPipeline(int32_t key, const PipelineCreationOptions& opts) {
        if (pPipeline ret = getPipeline(key)) { return ret; }
        if (!(opts.vertexShader | opts.fragmentShader)) {
            LOGWITH("Vertex and fragment shader should be provided.");
            return 0;
        }
        unsigned prog = glCreateProgram();
        if (prog == 0) {
            LOGWITH("Failed to create program");
            return 0;
        }

        glAttachShader(prog, opts.vertexShader);
        if (opts.tessellationControlShader) glAttachShader(prog, opts.tessellationControlShader);
        if (opts.tessellationEvaluationShader) glAttachShader(prog, opts.tessellationEvaluationShader);
        if (opts.geometryShader) glAttachShader(prog, opts.geometryShader);
        glAttachShader(prog, opts.fragmentShader);
        glLinkProgram(prog);

        constexpr int MAX_LOG = 4096;
        static char msg[MAX_LOG] = { 0, };
        int logLen;
        glGetProgramInfoLog(prog, MAX_LOG, &logLen, msg);
        if (logLen > 1 && logLen <= MAX_LOG) {
            LOGWITH(msg);
        }
        int linkStatus;
        glGetProgramiv(prog, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            LOGWITH("Failed to link shader into pipeline");
            glDeleteProgram(prog);
            return 0;
        }
        glValidateProgram(prog);
        glGetProgramInfoLog(prog, MAX_LOG, &logLen, msg);
        if (logLen > 1 && logLen <= MAX_LOG) {
            LOGWITH(msg);
        }
        int valStatus;
        glGetProgramiv(prog, GL_VALIDATE_STATUS, &valStatus);
        if (valStatus != GL_TRUE) {
            LOGWITH("Failed to link shader into pipeline");
            glDeleteProgram(prog);
            return 0;
        }
        pPipeline ret = std::make_shared<shp_t<Pipeline>>(prog, vec4(), opts.vertexSize, opts.instanceDataStride);
        std::memcpy(ret->blendOperation, opts.alphaBlend, sizeof(opts.alphaBlend));
        std::memcpy(ret->blendConstant, opts.blendConstant, sizeof(opts.blendConstant));

        ret->vspec.resize(opts.vertexAttributeCount);
        ret->ispec.resize(opts.instanceAttributeCount);
        std::memcpy(ret->vspec.data(), opts.vertexSpec, sizeof(opts.vertexSpec[0]) * opts.vertexAttributeCount);
        std::memcpy(ret->ispec.data(), opts.instanceSpec, sizeof(opts.instanceSpec[0]) * opts.instanceAttributeCount);
        ret->depthStencilOperation = opts.depthStencil;
        ret->cullMode = opts.cullMode;
        constexpr const char* ubnames[4] = { "ub0", "ub1", "ub2", "ub3" };
        constexpr const char* texnames[4] = { "t0", "t1", "t2", "t3" };
        for(int i=0;i<4;i++){
            unsigned ui = glGetUniformBlockIndex(prog, ubnames[i]);
            if(ui != GL_INVALID_INDEX) { glUniformBlockBinding(prog, ui, i); }
            ret->textureIndices[i] = glGetUniformLocation(prog, texnames[i]);
        }
        unsigned pui = glGetUniformBlockIndex(prog, "push");
        if(pui != GL_INVALID_INDEX) { glUniformBlockBinding(prog, pui, 11); }
        if(key == INT32_MIN) return ret;
        return singleton->pipelines[key] = std::move(ret);
    }

    WGLMachine::Pipeline::Pipeline(unsigned program, vec4 clearColor, unsigned vstr, unsigned istr) :program(program), vertexSize(vstr), instanceAttrStride(istr), clearColor(clearColor) {}
    WGLMachine::Pipeline::~Pipeline() {
        glDeleteProgram(program);
    }

    void WGLMachine::Pipeline::drop(int32_t key){
        singleton->pipelines.erase(key);
    }

    WGLMachine::Mesh::Mesh(unsigned vb, unsigned ib, size_t vcount, size_t icount, bool use32) :vb(vb), ib(ib), vcount(vcount), icount(icount), idxType(use32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT) {}
    WGLMachine::Mesh::~Mesh(){ 
        glDeleteBuffers(1, &vb);
        glDeleteBuffers(1, &ib);
        if (vao) { glDeleteVertexArrays(1, &vao); }
    }

    void WGLMachine::Mesh::update(const void* input, uint32_t offset, uint32_t size){
        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, input);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void WGLMachine::Mesh::updateIndex(const void* input, uint32_t offset, uint32_t size){
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, input);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void WGLMachine::Mesh::collect(bool removeUsing) {
        if(removeUsing) {
            singleton->meshes.clear();
        }
        else{
            for(auto it = singleton->meshes.cbegin(); it != singleton->meshes.cend();){
                if(it->second.use_count() == 1){
                    singleton->meshes.erase(it++);
                }
                else{
                    ++it;
                }
            }
        }
    }

    void WGLMachine::Mesh::drop(int32_t name){
        singleton->meshes.erase(name);
    }

    WGLMachine::RenderPass::RenderPass(uint16_t stageCount, bool canBeRead, float* autoclear) : stageCount(stageCount), pipelines(stageCount), targets(stageCount), canBeRead(canBeRead), autoclear(false) {
        if (autoclear) {
            this->autoclear = true;
            std::memcpy(clearColor, autoclear, sizeof(clearColor));
        }
    }

    WGLMachine::RenderPass::~RenderPass(){
        for (RenderTarget* targ : targets) {
            delete targ;
        }
    }

    static void setBlendParam(const AlphaBlend& blendop) {
        glBlendEquationSeparate(getGLBlendOpConstant(blendop.colorOp), getGLBlendOpConstant(blendop.alphaOp));
        glBlendFuncSeparate(
            getGLBlendFactorConstant(blendop.srcColorFactor),
            getGLBlendFactorConstant(blendop.dstColorFactor),
            getGLBlendFactorConstant(blendop.srcAlphaFactor),
            getGLBlendFactorConstant(blendop.dstAlphaFactor)
        );
    }

    void WGLMachine::RenderPass::usePipeline(Pipeline* pipeline, unsigned subpass){
        if(subpass > stageCount){
            LOGWITH("Invalid subpass. This renderpass has", stageCount, "subpasses but", subpass, "given");
            return;
        }
        pipelines[subpass] = pipeline;
        if(currentPass == subpass) { 
            glUseProgram(pipeline->program);
            if (pipeline->cullMode != CULL_NONE) {
                static_assert(Culling::CULL_NONE == 0);
                glEnable(GL_CULL_FACE);
                const GLuint cullMode[] = { GL_NONE, GL_BACK, GL_FRONT, GL_FRONT_AND_BACK };
                glCullFace(cullMode[pipeline->cullMode]);
            }
            else {
                glDisable(GL_CULL_FACE);
            }
            if(is4Screen){
                glBlendColor(pipeline->blendConstant[0], pipeline->blendConstant[1], pipeline->blendConstant[2], pipeline->blendConstant[3]);
                setBlendParam(pipeline->blendOperation[0]);
            }
            else if (targets[subpass]->color1) {
                glBlendColor(pipeline->blendConstant[0], pipeline->blendConstant[1], pipeline->blendConstant[2], pipeline->blendConstant[3]);
                setBlendParam(pipeline->blendOperation[0]);
                // gles: no separate blend per attachment
                /*
                if (targets[subpass]->color2) {
                    setBlendParam(1, pipeline->blendOperation[1]);
                    if (targets[subpass]->color3) {
                        setBlendParam(2, pipeline->blendOperation[2]);
                    }
                }
                */
            }
        }
    }

    void WGLMachine::RenderPass::setViewport(float width, float height, float x, float y, bool applyNow){
        viewport.height = height;
        viewport.width = width;
        viewport.maxDepth = 1.0f;
        viewport.minDepth = 0.0f;
        viewport.x = x;
        viewport.y = y;
        if(applyNow && currentPass != -1) {
            glViewport((int)x, (int)y, (int)width, (int)height);            
        }
    }

    void WGLMachine::RenderPass::setScissor(uint32_t width, uint32_t height, int32_t x, int32_t y, bool applyNow){
        scissor.width = width;
        scissor.height = height;
        scissor.x = x;
        scissor.y = y;
        if(applyNow && currentPass != -1) {
            glScissor(x,y,width,height);
        }
    }

    void WGLMachine::RenderPass::bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        glBindBufferRange(GL_UNIFORM_BUFFER, pos, ub->ubo, 0, ub->length);
    }

    void WGLMachine::RenderPass::bind(uint32_t pos, const pTexture& tx) {
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if(pos >= sizeof(pipelines[currentPass]->textureIndices) / sizeof(pipelines[currentPass]->textureIndices[0])){
            LOGWITH("Texture slot insufficient. Expand if wanted");
            return;
        }
        glActiveTexture(GL_TEXTURE0 + pos);
        glBindTexture(GL_TEXTURE_2D, tx->txo);
        glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
    }

    void WGLMachine::RenderPass::bind(uint32_t pos, const pTextureSet& tx) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        for (int i = 0; i < tx->textureCount; i++) {
            bind(pos + i, tx->textures[i]);
        }
    }

    void WGLMachine::RenderPass::bind(uint32_t pos, const pStreamTexture& tx) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if(pos >= sizeof(pipelines[currentPass]->textureIndices) / sizeof(pipelines[currentPass]->textureIndices[0])){
            LOGWITH("Texture slot insufficient. Expand if wanted");
            return;
        }
        glActiveTexture(GL_TEXTURE0 + pos);
        glBindTexture(GL_TEXTURE_2D, tx->txo);
        glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
    }

    void WGLMachine::RenderPass::bind(uint32_t pos, RenderPass2Cube* prev) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        glActiveTexture(GL_TEXTURE0 + pos);
        if (prev->targetCubeC) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, prev->targetCubeC);
            glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
        }
        else if (prev->targetCubeD) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, prev->targetCubeD);
            glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
        }
        else {
            LOGWITH("given renderpass2cube does not seem to be normal");
            return;
        }
    }

    void WGLMachine::RenderPass::bind(uint32_t pos, RenderPass* prev) {
        if (prev == this) {
            LOGWITH("Invalid call: input and output renderpass cannot be same");
            return;
        }
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        RenderTarget* lastOne = prev->targets.back();
        if (!lastOne) {
            LOGWITH("Invalid call: renderpass2screen cannot be an input");
            return;
        }
        if (lastOne->color1) {
            glActiveTexture(GL_TEXTURE0 + pos);
            glBindTexture(GL_TEXTURE_2D, lastOne->color1);
            glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
            pos++;
            if (lastOne->color2) {
                glActiveTexture(GL_TEXTURE0 + pos);
                glBindTexture(GL_TEXTURE_2D, lastOne->color2);
                glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
                pos++;
                if (lastOne->color3) {
                    glActiveTexture(GL_TEXTURE0 + pos);
                    glBindTexture(GL_TEXTURE_2D, lastOne->color2);
                    glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
                    pos++;
                }
            }
        }
        if (lastOne->depthStencil && lastOne->dsTexture) {
            glActiveTexture(GL_TEXTURE0 + pos);
            glBindTexture(GL_TEXTURE_2D, lastOne->depthStencil);
            glUniform1i(pipelines[currentPass]->textureIndices[pos], pos);
        }
    }

    void WGLMachine::RenderPass::push(void* input, uint32_t start, uint32_t end){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        getUniformBuffer(INT32_MIN + 1)->update(input, 0, start, end - start);
    }

    void WGLMachine::RenderPass::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        
        if((bound != mesh.get())) {
            if (!mesh->vao) {
                glGenVertexArrays(1, &mesh->vao);
                if (mesh->vao == 0) {
                    LOGWITH("Failed to create vertex array object");
                    return;
                }
                glBindVertexArray(mesh->vao);
                glBindBuffer(GL_ARRAY_BUFFER, mesh->vb);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ib);
                Pipeline* p = pipelines[currentPass];

                uint32_t location = 0;
                for (; location < p->vspec.size(); location++) {
                    enableAttribute(p->vertexSize, p->vspec[location]);
                }
                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
            glBindVertexArray(mesh->vao);
        }
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->icount - start);
            }
            glDrawElements(GL_TRIANGLES, count, mesh->idxType, mesh->idxType == GL_UNSIGNED_INT ? (void*)((uint32_t*)0 + start) : (void*)((uint16_t*)0 + start));
        }
        else {
            if((uint64_t)start + count > mesh->vcount){
                LOGWITH("Invalid call: this mesh has",mesh->vcount,"vertices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->vcount - start);
            }
            glDrawArrays(GL_TRIANGLES, start, count);
        }
        bound = mesh.get();
    }

    void WGLMachine::RenderPass::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count){
         if(currentPass == -1){
             LOGWITH("Invalid call: render pass not begun");
             return;
         }
         if (!mesh->vao) {
             glGenVertexArrays(1, &mesh->vao);
             if (mesh->vao == 0) {
                 LOGWITH("Failed to create vertex array object");
                 return;
             }
             glBindVertexArray(mesh->vao);
             glBindBuffer(GL_ARRAY_BUFFER, mesh->vb);
             glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ib);
             Pipeline* p = pipelines[currentPass];

             uint32_t location = 0;
             for (; location < p->vspec.size(); location++) {
                 enableAttribute(p->vertexSize, p->vspec[location]);
             }
             if (instanceInfo) {
                 glBindBuffer(GL_ARRAY_BUFFER, instanceInfo->vb);
                 uint32_t iloc = 0;
                 for (; iloc < p->ispec.size(); iloc++, location++) {
                     enableAttribute(p->instanceAttrStride, p->ispec[iloc]);
                     glVertexAttribDivisor(location, 1);
                 }
             }
             glBindVertexArray(0);
             glBindBuffer(GL_ARRAY_BUFFER, 0);
             glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
         }
         glBindVertexArray(mesh->vao);
         if(mesh->icount) {
             if((uint64_t)start + count > mesh->icount){
                 LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                 bound = nullptr;
                 return;
             }
             if(count == 0){
                 count = uint32_t(mesh->icount - start);
             }
             glDrawElementsInstanced(GL_TRIANGLES, mesh->icount, mesh->idxType, mesh->idxType == GL_UNSIGNED_INT ? (void*)((uint32_t*)0 + start) : (void*)((uint16_t*)0 + start), instanceCount);
         }
         else {
             if((uint64_t)start + count > mesh->vcount){
                 LOGWITH("Invalid call: this mesh has",mesh->vcount,"vertices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                 bound = nullptr;
                 return;
             }
             if(count == 0){
                 count = mesh->vcount - start;
             }
             glDrawArraysInstanced(GL_TRIANGLES, start, count, instanceCount);
         }
         bound = nullptr;
    }

    void WGLMachine::RenderPass::execute(...){
        if(currentPass != pipelines.size() - 1){
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        currentPass = -1;
        if (is4Screen) {
            singleton->windowSystems[windowIdx]->window->glPresent();
        }        
    }

    bool WGLMachine::RenderPass::wait(uint64_t timeout){
        return true;
    }

    void WGLMachine::RenderPass::resize(int width, int height, bool linear) {
        if (targets[0] && targets[0]->width == width && targets[0]->height == height) { // equal size
            return;
        }
        for (uint32_t i = 0; i < stageCount; i++) {
            RenderTarget* t = targets[i];
            if (t) {
                targets[i] = createRenderTarget2D(width, height, t->type, t->dsTexture, false);
                delete t;
                if (!targets[i]) {
                    LOGHERE;
                    for (RenderTarget*& tg : targets) {
                        delete tg;
                        tg = nullptr;
                    }
                    return;
                }
            }
        }

        setViewport(width, height, 0.0f, 0.0f);
        setScissor(width, height, 0, 0);
    }

    void WGLMachine::RenderPass::clear(RenderTargetType toClear, float* colors) {
        if (currentPass < 0) {
            LOGWITH("This renderPass is currently not running");
            return;
        }
        if (toClear == 0) {
            LOGWITH("no-op");
            return;
        }
        int type = targets[currentPass] ? targets[currentPass]->type : (RTT_COLOR1 | RTT_DEPTH | RTT_STENCIL);
        if ((toClear & type) != toClear) {
            LOGWITH("Invalid target selected");
            return;
        }
        if (autoclear) {
            LOGWITH("Autoclear specified. Maybe this call is a mistake?");
        }

        GLenum clearTarg[4]{};
        int clearCount = 0;
        if (toClear & 0b1) {
            clearTarg[clearCount++] = GL_COLOR_ATTACHMENT0;
        }
        if (toClear & 0b10) {
            clearTarg[clearCount++] = GL_COLOR_ATTACHMENT1;
        }
        if (toClear & 0b100) {
            clearTarg[clearCount++] = GL_COLOR_ATTACHMENT2;
        }
        if (toClear & 0b11000) {
            glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
        if (targets[currentPass]) {
            glDrawBuffers(clearCount, clearTarg);
            for (int i = 0; i < clearCount; i++) {
                glClearBufferfv(GL_COLOR, i, colors);
                colors += 4;
            }
            clearCount = 0;
            if (type & 0b1) {
                clearTarg[clearCount++] = GL_COLOR_ATTACHMENT0;
                if (type & 0b10) {
                    clearTarg[clearCount++] = GL_COLOR_ATTACHMENT1;
                    if (type & 0b100) {
                        clearTarg[clearCount++] = GL_COLOR_ATTACHMENT2;
                    }
                }
            }
            glDrawBuffers(clearCount, clearTarg);
        }
        else {
            if (toClear & 0b1) glClear(GL_COLOR_BUFFER_BIT);
        }
    }

    void WGLMachine::RenderPass::start(uint32_t pos, bool) {
        if(currentPass == stageCount - 1) {
            LOGWITH("Invalid call. The last subpass already started");
            return;
        }
        bound = nullptr;
        currentPass++;
        if(!pipelines[currentPass]) {
            LOGWITH("Pipeline not set.");
            currentPass--;
            return;
        }

        if (targets[currentPass]) {
            glBindFramebuffer(GL_FRAMEBUFFER, targets[currentPass]->framebuffer);
            if (targets[currentPass]->depthStencil) glEnable(GL_DEPTH_TEST);
            else glDisable(GL_DEPTH_TEST);
        }
        else {
            if (currentWindowContext != windowIdx) {
                singleton->windowSystems[windowIdx]->window->setMainThread();
                currentWindowContext = windowIdx;
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glEnable(GL_DEPTH_TEST);
        }

        if (currentPass > 0) {
            RenderTarget* prev = targets[currentPass - 1];
            if (!prev) {
                LOGWITH("Invalid call: renderpass2screen cannot be an input");
                return;
            }
            if (prev->color1) {
                glActiveTexture(GL_TEXTURE0 + pos);
                glBindTexture(GL_TEXTURE_2D, prev->color1);
                pos++;
                if (prev->color2) {
                    glActiveTexture(GL_TEXTURE0 + pos);
                    glBindTexture(GL_TEXTURE_2D, prev->color2);
                    pos++;
                    if (prev->color3) {
                        glActiveTexture(GL_TEXTURE0 + pos);
                        glBindTexture(GL_TEXTURE_2D, prev->color2);
                        pos++;
                    }
                }
            }
            if (prev->depthStencil && prev->dsTexture) {
                glActiveTexture(GL_TEXTURE0 + pos);
                glBindTexture(GL_TEXTURE_2D, prev->depthStencil);
            }
        }
        auto pp = pipelines[currentPass];
        glUseProgram(pp->program);
        if (targets[currentPass]) {
            if (targets[currentPass]->color1) {
                glBlendColor(pp->blendConstant[0], pp->blendConstant[1], pp->blendConstant[2], pp->blendConstant[3]);
                setBlendParam(pp->blendOperation[0]);
                // gles <3.2: no separate blend per attachment
                /*
                if (targets[currentPass]->color2) {
                    setBlendParam(1, pp->blendOperation[1]);
                    if (targets[currentPass]->color3) {
                        setBlendParam(2, pp->blendOperation[2]);
                    }
                }
                */
            }
        }
        else {
            glBlendColor(pp->blendConstant[0], pp->blendConstant[1], pp->blendConstant[2], pp->blendConstant[3]);
            setBlendParam(pp->blendOperation[0]);
        }
        if (autoclear) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        }
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
        glDepthRangef(viewport.minDepth, viewport.maxDepth);
        glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
    }

    /// @brief 렌더타겟에 직전 execute 이후 그려진 내용을 별도의 텍스처로 복사합니다.
    /// @param key 텍스처 키입니다.
    WGLMachine::pTexture WGLMachine::RenderPass::copy2Texture(int32_t key, const RenderTarget2TextureOptions& opts) {
        if (getTexture(key)) {
            LOGWITH("Invalid key");
            return {};
        }
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return {};
        }
        RenderTarget* targ = targets.back();
        if (!targ) {
            LOGWITH("Reading back from pass to screen is currently not available");
            return {};
        }
        unsigned src = 0;
        if (opts.index < 3) {
            unsigned sources[] = { targ->color1, targ->color2, targ->color3 };
            src = sources[opts.index];
        }
        if (!src) {
            LOGWITH("Invalid index");
            return {};
        }
        unsigned newTex = 0;
        glGenTextures(1, &newTex);
        if (!newTex) {
            LOGWITH("Failed to create copy target texture");
            return {};
        }
        glBindFramebuffer(GL_FRAMEBUFFER, targ->framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0 + opts.index);
        glBindTexture(GL_TEXTURE_2D, newTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, opts.linearSampled ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, opts.linearSampled ? GL_LINEAR : GL_NEAREST);
        if (opts.area.width && opts.area.height) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, opts.area.width, opts.area.height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, opts.area.x, targ->height - opts.area.y - opts.area.height, opts.area.width, opts.area.height);
        }
        else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, targ->width, targ->height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, targ->width, targ->height);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        struct txtr :public Texture { inline txtr(uint32_t _1, uint16_t _2, uint16_t _3) :Texture(_1, _2, _3) {} };
        pTexture ret = std::make_shared<txtr>(newTex, targ->width, targ->height);
        if (key != INT32_MIN) { singleton->textures[key] = ret; }
        return ret;
    }

    void WGLMachine::RenderPass::asyncCopy2Texture(int32_t key, std::function<void(variant8)> handler, const RenderTarget2TextureOptions& opts) {
        LOGWITH("Warning: Currently there is no async copy in OpenGL API; This call will be executed now");
        if (key == INT32_MIN) {
            LOGWITH("INT32_MIN can\'t be used in OpenGL API for consistency with other Graphics API bases");
            return;
        }
        pTexture newTex = copy2Texture(key, opts);
        bool succeeded = newTex.operator bool();
        singleton->loadThread.post([key, succeeded]() {
            variant8 ret;
            ret.bytedata4[0] = key;
            ret.bytedata4[1] = !succeeded;
            return ret;
        }, handler);
    }

    std::unique_ptr<uint8_t[]> WGLMachine::RenderPass::readBack(uint32_t index, const TextureArea2D& area) {
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return {};
        }
        RenderTarget* targ = targets.back();
        if (!targ) {
            LOGWITH("Reading back from pass to screen is currently not available");
            return {};
        }
        unsigned src = 0;
        if (index < 4) {
            unsigned sources[] = { targ->color1, targ->color2, targ->color3, targ->depthStencil };
            src = sources[index];
        }
        if (!src) {
            LOGWITH("Invalid index");
            return {};
        }

        glBindFramebuffer(GL_FRAMEBUFFER, targ->framebuffer);
        if (index <= 2) {
            glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
        }
        else {
            glReadBuffer(GL_DEPTH_ATTACHMENT);
        }

        uint32_t width, height, x, y;
        if (area.width && area.height) {
            width = area.width;
            height = area.height;
            x = area.x;
            y = targ->height - area.y - area.height;
        }
        else {
            width = targ->width;
            height = targ->height;
            x = y = 0;
        }
        std::unique_ptr<uint8_t[]> ret(new uint8_t[width * height * 4]);

        glReadPixels(x, y, width, height, index == 3 ? GL_DEPTH_COMPONENT : GL_RGBA, GL_UNSIGNED_BYTE, ret.get());
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return ret;
    }

    void WGLMachine::RenderPass::asyncReadBack(int32_t key, uint32_t index, std::function<void(variant8)> handler, const TextureArea2D& area) {
        LOGWITH("Warning: Currently there is no async copy in OpenGL API; This call will be executed now");
        std::unique_ptr<uint8_t[]> up = readBack(index, area);
        uint8_t* dat = up.release();
        ReadBackBuffer ret;
        ret.key = key;
        ret.data = dat;
        if (handler) handler(&ret);
    }

    WGLMachine::RenderPass2Cube::~RenderPass2Cube(){
        glDeleteFramebuffers(1, &fbo);
        if (targetCubeC) glDeleteTextures(1, &targetCubeC);
        if (targetCubeD) glDeleteTextures(1, &targetCubeD);
    }

    void WGLMachine::RenderPass2Cube::drop(int32_t key){
        singleton->cubePasses.erase(key);
    }

    void WGLMachine::RenderPass2Cube::bind(uint32_t pos, UniformBuffer* ub, uint32_t pass, uint32_t ubPos){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if (pass >= 6) {
            glBindBufferRange(GL_UNIFORM_BUFFER, pos, ub->ubo, 0, ub->length);
        }
        else {
            facewise[pass].ub = ub;
            facewise[pass].ubPos = ubPos;
            facewise[pass].setPos = pos;
        }
    }

    void WGLMachine::RenderPass2Cube::bind(uint32_t pos, const pTexture& tx){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        glActiveTexture(GL_TEXTURE0 + pos);
        glBindTexture(GL_TEXTURE_2D, tx->txo);
    }

    void WGLMachine::RenderPass2Cube::bind(uint32_t pos, const pStreamTexture& tx) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        glActiveTexture(GL_TEXTURE0 + pos);
        glBindTexture(GL_TEXTURE_2D, tx->txo);
    }

    void WGLMachine::RenderPass2Cube::bind(uint32_t pos, RenderPass* prev){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        RenderTarget* lastOne = prev->targets.back();
        if (!lastOne) {
            LOGWITH("Invalid call: renderpass2screen cannot be an input");
            return;
        }
        if (lastOne->color1) {
            glActiveTexture(GL_TEXTURE0 + pos);
            glBindTexture(GL_TEXTURE_2D, lastOne->color1);
            pos++;
            if (lastOne->color2) {
                glActiveTexture(GL_TEXTURE0 + pos);
                glBindTexture(GL_TEXTURE_2D, lastOne->color2);
                pos++;
                if (lastOne->color3) {
                    glActiveTexture(GL_TEXTURE0 + pos);
                    glBindTexture(GL_TEXTURE_2D, lastOne->color2);
                    pos++;
                }
            }
        }
        if (lastOne->depthStencil && lastOne->dsTexture) {
            glActiveTexture(GL_TEXTURE0 + pos);
            glBindTexture(GL_TEXTURE_2D, lastOne->depthStencil);
        }
    }
    
    void WGLMachine::RenderPass2Cube::usePipeline(unsigned pipeline){
        this->pipeline = pipeline;
        if (recording) { glUseProgram(pipeline); }
    }

    void WGLMachine::RenderPass2Cube::push(void* input, uint32_t start, uint32_t end){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        getUniformBuffer(INT32_MIN + 1)->update(input, 0, start, end - start);
    }

    void WGLMachine::RenderPass2Cube::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
         glBindFramebuffer(GL_FRAMEBUFFER, fbo);
         if ((bound != mesh.get()) && (mesh->vb != 0)) {
             glBindBuffer(GL_ARRAY_BUFFER, mesh->vb);
         }
         if (mesh->icount) {
             if (bound != mesh.get() && (mesh->ib != 0)) {
                 glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ib);
             }
             if ((uint64_t)start + count > mesh->icount) {
                 LOGWITH("Invalid call: this mesh has", mesh->icount, "indices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                 bound = nullptr;
                 return;
             }
             if (count == 0) {
                 count = mesh->icount - start;
             }
             for (int i = 0; i < 6; i++) {
                 if(targetCubeC) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeC, 0);
                 if(targetCubeD) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeD, 0);
                 auto& fwi = facewise[i];
                 if (fwi.ub) {
                     glBindBufferRange(GL_UNIFORM_BUFFER, i, fwi.ub->ubo, 0, fwi.ub->length);
                 }
                 glDrawElements(GL_TRIANGLES, count, mesh->idxType, mesh->idxType == GL_UNSIGNED_INT ? (void*)((uint32_t*)0 + start) : (void*)((uint16_t*)0 + start));
             }
         }
         else {
             if ((uint64_t)start + count > mesh->vcount) {
                 LOGWITH("Invalid call: this mesh has", mesh->vcount, "vertices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                 bound = nullptr;
                 return;
             }
             if (count == 0) {
                 count = mesh->vcount - start;
             }
             for (int i = 0; i < 6; i++) {
                 if (targetCubeC) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeC, 0);
                 if (targetCubeD) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeD, 0);
                 auto& fwi = facewise[i];
                 if (fwi.ub) {
                     glBindBufferRange(GL_UNIFORM_BUFFER, i, fwi.ub->ubo, 0, fwi.ub->length);
                 }
                 glDrawArrays(GL_TRIANGLES, start, count);
             }
         }
         bound = mesh.get();
    }

    void WGLMachine::RenderPass2Cube::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count) {
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
         }
         if ((bound != mesh.get()) && (mesh->vb != 0)) {
             glBindVertexArray(mesh->vb);
         }
         if (mesh->icount) {
             if ((uint64_t)start + count > mesh->icount) {
                 LOGWITH("Invalid call: this mesh has", mesh->icount, "indices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                 bound = nullptr;
                 return;
             }
             if (count == 0) {
                 count = mesh->icount - start;
             }
             for (int i = 0; i < 6; i++) {
                 if (targetCubeC) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeC, 0);
                 if (targetCubeD) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeD, 0);
                 auto& fwi = facewise[i];
                 if (fwi.ub) {
                     glBindBufferRange(GL_UNIFORM_BUFFER, i, fwi.ub->ubo, 0, fwi.ub->length);
                 }
                 glDrawElementsInstanced(GL_TRIANGLES, mesh->icount, mesh->idxType, mesh->idxType == GL_UNSIGNED_INT ? (void*)((uint32_t*)0 + start) : (void*)((uint16_t*)0 + start), instanceCount);
             }
         }
         else {
             if ((uint64_t)start + count > mesh->vcount) {
                 LOGWITH("Invalid call: this mesh has", mesh->vcount, "vertices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                 bound = nullptr;
                 return;
             }
             if (count == 0) {
                 count = mesh->vcount - start;
             }
             for (int i = 0; i < 6; i++) {
                 if (targetCubeC) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeC, 0);
                 if (targetCubeD) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeD, 0);
                 auto& fwi = facewise[i];
                 if (fwi.ub) {
                     glBindBufferRange(GL_UNIFORM_BUFFER, i, fwi.ub->ubo, 0, fwi.ub->length);
                 }
                 glDrawArraysInstanced(GL_TRIANGLES, start, count, instanceCount);
             }
         }
        bound = nullptr;
    }

    void WGLMachine::RenderPass2Cube::execute(RenderPass* other){
        if(!recording){
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        recording = false;
    }

    bool WGLMachine::RenderPass2Cube::wait(uint64_t timeout){
        return true;
    }

    void WGLMachine::RenderPass2Cube::start(){
        if(recording) {
            LOGWITH("Invalid call. The renderpass already started");
            return;
        }
        bound = nullptr;
        if(!pipeline) {
            LOGWITH("Pipeline not set:",this);
            return;
        }
        wait();
        recording = true;
        glUseProgram(pipeline);
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
        glDepthRangef(viewport.minDepth, viewport.maxDepth);
        glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
    }

    void WGLMachine::UniformBuffer::update(const void* input, uint32_t index, uint32_t offset, uint32_t size){
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, input);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void WGLMachine::UniformBuffer::updatePush(const void* input, uint32_t offset, uint32_t size) {
        singleton->uniformBuffers[INT32_MIN + 1]->update(input, 0, offset, size);
    }

    void WGLMachine::UniformBuffer::resize(uint32_t size) {    }

    WGLMachine::UniformBuffer::UniformBuffer(uint32_t length, unsigned ubo) :ubo(ubo), length(length) {}

    WGLMachine::UniformBuffer::~UniformBuffer(){
        glDeleteBuffers(1, &ubo);
    }

    void WGLMachine::UniformBuffer::drop(int32_t key){
        singleton->uniformBuffers.erase(key);
    }


    // static함수들 구현

    int textureFormatFallback(uint32_t nChannels, bool srgb, bool hq) {
        /*
        int count;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
        std::vector<int> availableFormat(count);
        glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, availableFormat.data());
        std::unordered_set<int> formatSet(availableFormat.begin(), availableFormat.end());
        */
    #define CHECK_N_RETURN(f) if(availableTextureFormats.find(f) != availableTextureFormats.end()) return f
        switch (nChannels)
        {
        case 4:
        if(srgb){
            CHECK_N_RETURN(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB);
            if(hq) return GL_SRGB8_ALPHA8;
            CHECK_N_RETURN(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
            CHECK_N_RETURN(GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT);
            return GL_SRGB8_ALPHA8;
        }
        else{
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_BPTC_UNORM_ARB);
            if(hq) return GL_RGBA8;
            CHECK_N_RETURN(GL_COMPRESSED_RGBA8_ETC2_EAC);
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);
            return GL_RGBA8;
        }
            break;
        case 3:
        if(srgb){
            CHECK_N_RETURN(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB);
            if(hq) return GL_SRGB8;
            CHECK_N_RETURN(GL_COMPRESSED_SRGB8_ETC2);
            CHECK_N_RETURN(GL_COMPRESSED_SRGB_S3TC_DXT1_EXT);
            return GL_SRGB8;
        }
        else{
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_BPTC_UNORM_ARB);
            if(hq) return GL_RGB8;
            CHECK_N_RETURN(GL_COMPRESSED_RGB8_ETC2);
            CHECK_N_RETURN(GL_COMPRESSED_RGB_S3TC_DXT1_EXT);
            return GL_RGB8;
        }
        case 2:
        if(srgb){
            CHECK_N_RETURN(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB);
            return GL_RG8;
        }
        else{
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_BPTC_UNORM_ARB);
            if(hq) return GL_RG8;
            CHECK_N_RETURN(GL_COMPRESSED_RG11_EAC);
            CHECK_N_RETURN(GL_COMPRESSED_RG_RGTC2);
            return GL_RG8;
        }
        case 1:
        if(srgb){
            CHECK_N_RETURN(GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB);
            return GL_R8;
        }
        else{
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_ASTC_4x4_KHR);
            CHECK_N_RETURN(GL_COMPRESSED_RGBA_BPTC_UNORM_ARB);
            if(hq) return GL_R8;
            CHECK_N_RETURN(GL_COMPRESSED_R11_EAC);
            CHECK_N_RETURN(GL_COMPRESSED_RED_RGTC1);
            return GL_R8;
        }
        default:
            return -1;
        }
    #undef CHECK_N_RETURN
    }

    void enableAttribute(int stride, const WGLMachine::PipelineInputVertexSpec& type) {
        glEnableVertexAttribArray(type.index);
        using __elem_t = decltype(type.type);
        switch(type.type){
            case __elem_t::F32:
                glVertexAttribPointer(type.index, type.dim, GL_FLOAT, GL_FALSE, stride, (void*)type.offset);
                break;
            case __elem_t::F64:
                LOGWITH("No float64 format in webgl/gles.");
                //glVertexAttribPointer(type.index, type.dim, GL_DOUBLE, GL_FALSE, stride, (void*)type.offset);
                break;
            case __elem_t::I8:
                glVertexAttribIPointer(type.index, type.dim, GL_BYTE, stride, (void*)type.offset);
                break;
            case __elem_t::I16:
                glVertexAttribIPointer(type.index, type.dim, GL_SHORT, stride, (void*)type.offset);
                break;
            case __elem_t::I32:
                glVertexAttribIPointer(type.index, type.dim, GL_INT, stride, (void*)type.offset);
                break;
            case __elem_t::U8:
                glVertexAttribIPointer(type.index, type.dim, GL_UNSIGNED_BYTE, stride, (void*)type.offset);
                break;
            case __elem_t::U16:
                glVertexAttribIPointer(type.index, type.dim, GL_UNSIGNED_SHORT, stride, (void*)type.offset);
                break;
            case __elem_t::U32:
                glVertexAttribIPointer(type.index, type.dim, GL_UNSIGNED_INT, stride, (void*)type.offset);
                break;
        }
    }

    /// @brief OpenGL 에러 코드를 스트링으로 표현합니다. 리턴되는 문자열은 텍스트(코드) 영역에 존재합니다.
    static const char* resultAsString(unsigned code) {
        switch (code)
        {
        case GL_NO_ERROR:
            return "Success";
        case GL_INVALID_ENUM:
            return "Invalid enum parameter";
        case GL_INVALID_VALUE:
            return "Invalid parameter value";
        case GL_INVALID_OPERATION:
            return "The operation should not be done in this state";
        case GL_OUT_OF_MEMORY:
            return "Out of memory";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "Cannot do this operation for this framebuffer";
        default:
            return "Unknown Error";
            break;
        }
    }
}

