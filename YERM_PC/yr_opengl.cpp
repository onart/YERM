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

#include "yr_opengl.h"
#include "logger.hpp"
#include "../externals/glad/glad.h"
#include "yr_sys.h"
#include "../externals/glfw/include/GLFW/glfw3.h"

#include "../externals/boost/predef/platform.h"
#include "../externals/single_header/stb_image.h"

#if !BOOST_PLAT_ANDROID
#define KHRONOS_STATIC
#endif
#include "../externals/ktx/ktx.h"

#include <algorithm>
#include <vector>
#include <unordered_set>

namespace onart {

    static void GLAPIENTRY glOnError(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

    /// @brief 주어진 기반 형식과 아귀가 맞는, 현재 장치에서 사용 가능한 압축 형식을 리턴합니다.
    static int textureFormatFallback(uint32_t nChannels, bool srgb, bool hq);
    /// @brief OpenGL 에러 코드를 스트링으로 표현합니다. 리턴되는 문자열은 텍스트(코드) 영역에 존재합니다.
    inline static const char* resultAsString(unsigned);

    /// @brief 활성화할 장치 확장
    constexpr const char* GL_DESIRED_ARB[] = {
        "GL_ARB_vertex_buffer_object",
        "GL_ARB_vertex_array_object",
        "GL_ARB_vertex_shader",
        "GL_ARB_fragment_shader",
        "GL_ARB_shader_objects",
    };


    GLMachine* GLMachine::singleton = nullptr;
    thread_local unsigned GLMachine::reason = GL_NO_ERROR;

    const GLMachine::Mesh* bound = nullptr;

    GLMachine::GLMachine(Window* window){
        if(singleton) {
            LOGWITH("Tried to create multiple GLMachine objects");
            return;
        }

        // 생성 당시 glfwmakecontextcurrent는 yr_game.cpp에서 호출되어 있고 생성이 성공했다면 별도의 렌더링 스레드로 컨텍스트가 넘어감

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { 
            LOGWITH("Failed to load GL loader");
            return;
        }

        std::set<std::string> ext;
        int next; glGetIntegerv(GL_NUM_EXTENSIONS, &next);
		for (int k = 0; k < next; k++) ext.insert((const char*)glGetStringi(GL_EXTENSIONS, k));
        for(const char* arb: GL_DESIRED_ARB) {
            if(ext.find(arb) == ext.end()) {
                LOGWITH("No support for essential extension:",arb);
                return;
            }
        }

        int x, y;
        window->getFramebufferSize(&x, &y);
        createSwapchain(x, y);

        if constexpr(USE_OPENGL_DEBUG) {
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageCallback(glOnError,0);
        }
        singleton = this;
        UniformBuffer* push = createUniformBuffer(1, 128, 0, INT32_MIN + 1, 11);
        if (!push) {
            singleton = nullptr;
            return;
        }
        glBindBufferRange(GL_UNIFORM_BUFFER, 11, push->ubo, 0, 128);
    }

    unsigned GLMachine::getPipeline(int32_t name){
        auto it = singleton->pipelines.find(name);
        if(it != singleton->pipelines.end()) return it->second;
        else return 0;
    }

    unsigned GLMachine::getPipelineLayout(int32_t name){ return 0; }

    GLMachine::pMesh GLMachine::getMesh(int32_t name) {
        auto it = singleton->meshes.find(name);
        if(it != singleton->meshes.end()) return it->second;
        else return pMesh();
    }

    GLMachine::RenderTarget* GLMachine::getRenderTarget(int32_t name){
        auto it = singleton->renderTargets.find(name);
        if(it != singleton->renderTargets.end()) return it->second;
        else return nullptr;
    }

    GLMachine::UniformBuffer* GLMachine::getUniformBuffer(int32_t name){
        auto it = singleton->uniformBuffers.find(name);
        if(it != singleton->uniformBuffers.end()) return it->second;
        else return nullptr;
    }

    GLMachine::RenderPass2Screen* GLMachine::getRenderPass2Screen(int32_t name){
        auto it = singleton->renderPasses.find(name);
        if(it != singleton->renderPasses.end()) return it->second;
        else return nullptr;
    }

    GLMachine::RenderPass* GLMachine::getRenderPass(int32_t name){
        auto it = singleton->renderPasses.find(name);
        if(it != singleton->renderPasses.end()) return it->second;
        else return nullptr;
    }

    GLMachine::RenderPass2Cube* GLMachine::getRenderPass2Cube(int32_t name){
        auto it = singleton->cubePasses.find(name);
        if(it != singleton->cubePasses.end()) return it->second;
        else return nullptr;
    }

    unsigned GLMachine::getShader(int32_t name){
        auto it = singleton->shaders.find(name);
        if(it != singleton->shaders.end()) return it->second;
        else return 0;
    }

    GLMachine::pTexture GLMachine::getTexture(int32_t name, bool lock){
        if(lock){
            std::unique_lock<std::mutex> _(singleton->textureGuard);
            auto it = singleton->textures.find(name);
            if(it != singleton->textures.end()) return it->second;
            else return pTexture();
        }
        else{
            auto it = singleton->textures.find(name);
            if(it != singleton->textures.end()) return it->second;
            else return pTexture();
        }
    }

    void GLMachine::checkSurfaceHandle(){ }

    mat4 GLMachine::preTransform(){
        return mat4();
    }

    void GLMachine::createSwapchain(uint32_t width, uint32_t height, Window* window) {
        surfaceWidth = width;
        surfaceHeight = height;
        for (auto& renderPass: renderPasses) {
            if (renderPass.second->targets[renderPass.second->stageCount - 1] == nullptr) { // renderpass 2 screen
                renderPass.second->setViewport(width, height, 0, 0, true); // 이후 수정 필요: 기존과 동등한 비중
                renderPass.second->setScissor(width, height, 0, 0, true); // 이후 수정 필요: 기존과 동등한 비중
            }
        }
    }

    void GLMachine::destroySwapchain(){}

    void GLMachine::free() {
        for(auto& cp: cubePasses) { delete cp.second; }
        for(auto& fp: finalPasses) { delete fp.second; }
        for(auto& rp: renderPasses) { delete rp.second; }
        for(auto& rt : renderTargets) { delete rt.second; }
        for(auto& sh: shaders) { glDeleteShader(sh.second); }
        for(auto& pp: pipelines) { glDeleteProgram(pp.second); }

        textures.clear();
        meshes.clear();
        pipelines.clear();
        cubePasses.clear();
        finalPasses.clear();
        renderPasses.clear();
        renderTargets.clear();
        shaders.clear();
    }

    void GLMachine::handle() {
        singleton->loadThread.handleCompleted();
    }

    GLMachine::~GLMachine(){
        free();
    }

    GLMachine::pMesh GLMachine::createNullMesh(size_t vcount, int32_t name) {
        pMesh m = getMesh(name);
        if(m) { return m; }
        struct publicmesh:public Mesh{publicmesh(unsigned _1, unsigned _2, size_t _3, size_t _4, bool _5):Mesh(_1,_2,_3,_4,_5){}};
        if(name == INT32_MIN) return std::make_shared<publicmesh>(0,0,vcount,0,false);
        return singleton->meshes[name] = std::make_shared<publicmesh>(0,0,vcount,0,false);
    }

    GLMachine::pMesh GLMachine::createMesh(void* vdata, size_t vsize, size_t vcount, void* idata, size_t isize, size_t icount, int32_t name, bool stage) {
        
        pMesh m = getMesh(name);
        if(m) { return m; }

        unsigned vb, ib = 0;
        glGenBuffers(1, &vb);
        if(vb == 0) {
            LOGWITH("Failed to create vertex buffer");
            return {};
        }

        if(icount != 0 && isize != 2 && isize != 4){
            LOGWITH("Invalid isize");
            return pMesh();
        }

        if(icount != 0){
            glGenBuffers(1, &ib);
            if (ib == 0) {
                LOGWITH("Failed to create index buffer");
                glDeleteBuffers(1, &vb);
                return {};
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, isize * icount, idata, stage ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }
        
        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferData(GL_ARRAY_BUFFER, vsize * vcount, vdata, stage ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        struct publicmesh:public Mesh{publicmesh(unsigned _1, unsigned _2, size_t _3, size_t _4,bool _5):Mesh(_1,_2,_3,_4,_5){}};
        return singleton->meshes[name] = std::make_shared<publicmesh>(vb, ib, vcount, icount, isize==4);
    }

    GLMachine::RenderTarget* GLMachine::createRenderTarget2D(int width, int height, int32_t name, RenderTargetType type, RenderTargetInputOption sampled, bool useDepthInput, bool useStencil, bool mmap){
        if(useDepthInput && useStencil) {
            LOGWITH("Warning: Can\'t use stencil buffer while using depth buffer as sampled image or input attachment"); // TODO? 엄밀히 말하면 스텐실만 입력첨부물로 쓸 수는 있는데 이걸 꼭 해야 할지
            return nullptr;
        }
        auto it = singleton->renderTargets.find(name);
        if(it != singleton->renderTargets.end()) {return it->second;}

        unsigned color1{}, color2{}, color3{}, ds{}, fb{};
        glGenFramebuffers(1, &fb);
        if (fb == 0) {
            LOGWITH("Failed to create framebuffer:", reason, resultAsString(reason));
            return nullptr;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fb);

        if((int)type & 0b1){
            glGenTextures(1, &color1);
            if (color1 == 0) {
                LOGWITH("Failed to create image:", reason, resultAsString(reason));
                return nullptr;
            }
            glBindTexture(GL_TEXTURE_2D, color1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color1, 0);
            if((int)type & 0b10){
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
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, color2, 0);
                if((int)type & 0b100){
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
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, color3, 0);
                }
            }
        }
        else {
            glDrawBuffer(GL_NONE);
        }
        if((int)type & 0b1000) {
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
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (name == INT32_MIN) return new RenderTarget(type, width, height, color1, color2, color3, ds, useDepthInput, fb);
        return singleton->renderTargets[name] = new RenderTarget(type, width, height, color1, color2, color3, ds, useDepthInput, fb);
    }

    unsigned GLMachine::createShader(const char* spv, size_t size, int32_t name, ShaderType type) {
        unsigned ret = getShader(name);
        if(ret) return ret;

        unsigned shType;
        switch(type){
            case ShaderType::VERTEX:
                shType = GL_VERTEX_SHADER;
                break;
            case ShaderType::FRAGMENT:
                shType = GL_FRAGMENT_SHADER;
                break;
            case ShaderType::GEOMETRY:
                shType = GL_GEOMETRY_SHADER;
                break;
            case ShaderType::TESS_CTRL:
                shType = GL_TESS_CONTROL_SHADER;
                break;
            case ShaderType::TESS_EVAL:
                shType = GL_TESS_EVALUATION_SHADER;
                break;
            default:
                LOGWITH("Invalid shader type");
                return 0;
        }

        unsigned prog = glCreateShader(shType);
        int sz = size;
        glShaderSource(prog, 1, &spv, &sz);
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
        if(name == INT32_MIN) return prog;
		return singleton->shaders[name] = prog;
    }

    static ktx_error_code_e tryTranscode(ktxTexture2* texture, uint32_t nChannels, bool srgb, bool hq) {
        if (ktxTexture2_NeedsTranscoding(texture)) {
            ktx_transcode_fmt_e tf;
            switch (textureFormatFallback(nChannels, srgb, hq))
            {
            case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
            case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
                tf = KTX_TTF_ASTC_4x4_RGBA;
                break;
            case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:
            case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:
                tf = KTX_TTF_BC7_RGBA;
                break;
            case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
            case GL_COMPRESSED_RGBA8_ETC2_EAC:
                tf = KTX_TTF_ETC2_RGBA;
                break;
            case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
                tf = KTX_TTF_BC3_RGBA;
                break;
            default:
                tf = KTX_TTF_RGBA32;
                break;
            }
            return ktxTexture2_TranscodeBasis(texture, tf, 0);
        }
        return KTX_SUCCESS;
    }

    GLMachine::pTexture GLMachine::createTexture(void* ktxObj, int32_t key, uint32_t nChannels, bool srgb, bool hq, bool linearSampler){
        ktxTexture2* texture = reinterpret_cast<ktxTexture2*>(ktxObj);
        if (texture->numLevels == 0) return pTexture();
        ktx_error_code_e k2result = tryTranscode(texture, nChannels, srgb, hq);
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        struct txtr :public Texture { inline txtr(uint32_t _1, uint32_t _2, uint16_t _3, uint16_t _4) :Texture(_1, _2, _3, _4) {} };
        if (key == INT32_MIN) return std::make_shared<txtr>(tex, 0, width, height);
        return textures[key] = std::make_shared<txtr>(tex, 0, width, height);
    }

    static ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, GLMachine::ImageTextureFormatOptions& option){
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
        if(option == GLMachine::ImageTextureFormatOptions::IT_USE_HQCOMPRESS || option == GLMachine::ImageTextureFormatOptions::IT_USE_COMPRESS){
            ktxBasisParams params{};
            params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
            params.uastc = KTX_TRUE;
            params.verbose = KTX_FALSE;
            params.structSize = sizeof(params);

            k2result = ktxTexture2_CompressBasisEx(texture, &params);
            if(k2result != KTX_SUCCESS){
                LOGWITH("Compress failed:",k2result);
                option = GLMachine::ImageTextureFormatOptions::IT_USE_ORIGINAL;
            }
        }
        return texture;
    }

    GLMachine::pTexture GLMachine::createTextureFromImage(const char* fileName, int32_t key, bool srgb, ImageTextureFormatOptions option, bool linearSampler) {
        int x, y, nChannels;
        uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
        if(!pix) {
            LOGWITH("Failed to load image:",stbi_failure_reason());
            return pTexture();
        }
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, 4, srgb, option);
        stbi_image_free(pix);
        if(!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, 4, srgb, option != ImageTextureFormatOptions::IT_USE_COMPRESS, linearSampler);
    }

    GLMachine::pTexture GLMachine::createTextureFromImage(const uint8_t* mem, size_t size, int32_t key, bool srgb, ImageTextureFormatOptions option, bool linearSampler){
        int x, y, nChannels;
        uint8_t* pix = stbi_load_from_memory(mem, size, &x, &y, &nChannels, 0);
        if(!pix) {
            LOGWITH("Failed to load image:",stbi_failure_reason());
            return pTexture();
        }
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, srgb, option);
        stbi_image_free(pix);
        if(!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, nChannels, srgb, option != ImageTextureFormatOptions::IT_USE_COMPRESS, linearSampler);
    }

    GLMachine::pTexture GLMachine::createTexture(const char* fileName, int32_t key, uint32_t nChannels, bool srgb, bool hq, bool linearSampler){
        if(nChannels > 4 || nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }
        pTexture ret(std::move(getTexture(key)));
        if(ret) return ret;

        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if((k2result= ktxTexture2_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS){
            LOGWITH("Failed to load ktx texture:",k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, nChannels, srgb, hq, linearSampler);
    }

    GLMachine::pTexture GLMachine::createTexture(const uint8_t* mem, size_t size, uint32_t nChannels, int32_t key, bool srgb, bool hq, bool linearSampler){
        if(nChannels > 4 || nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }
        pTexture ret(std::move(getTexture(key)));
        if(ret) return ret;
        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if((k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS){
            LOGWITH("Failed to load ktx texture:",k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, nChannels, srgb, hq, linearSampler);
    }

    void GLMachine::asyncCreateTexture(const char* fileName, int32_t key, uint32_t nChannels, std::function<void(void*)> handler, bool srgb, bool hq, bool linearSampler){
        if(key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        bool already = (bool)getTexture(key, true);
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        singleton->loadThread.post([fileName, nChannels, srgb, hq, already]()->void*{
            if(!already){
                ktxTexture2* texture;
                ktx_error_code_e k2result;
                if ((k2result = ktxTexture2_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
                    return new __asyncparam{ nullptr, k2result };
                }
                if ((k2result = tryTranscode(texture, nChannels, srgb, hq)) != KTX_SUCCESS) {
                    return new __asyncparam{ nullptr, k2result };
                }
                return new __asyncparam{ texture, KTX_SUCCESS };
            }
            return nullptr;
            }, [key, linearSampler, handler](void* param) { // upload on GL context thread
                if (!param) {
                    size_t p = key;
                    handler((void*)p);
                }
                else {
                    __asyncparam* ap = reinterpret_cast<__asyncparam*>(param);
                    ktxTexture2* texture = ap->texture;
                    int32_t k2result = ap->k2result;
                    delete ap;
                    if (k2result != KTX_SUCCESS) {
                        size_t p = key | (size_t)(k2result << 32);
                        handler((void*)p);
                    }
                    else {
                        unsigned tex = 0, targ, err;
                        k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                        if (k2result != KTX_SUCCESS) {
                            LOGWITH("Failed to transcode ktx texture:", k2result, err);
                            ktxTexture_Destroy(ktxTexture(texture));
                        }
                        glBindTexture(GL_TEXTURE_2D, tex);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                        glBindTexture(GL_TEXTURE_2D, 0);

                        struct txtr :public Texture { inline txtr(uint32_t _1, uint32_t _2, uint16_t _3, uint16_t _4) :Texture(_1, _2, _3, _4) {} };
                        pTexture ret = std::make_shared<txtr>(tex, 0, texture->baseWidth, texture->baseHeight);
                        singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                        ktxTexture_Destroy(ktxTexture(texture));
                        size_t p = key;
                        handler((void*)p);
                    }
                }
            }, vkm_strand::GENERAL);
    }

    

    void GLMachine::asyncCreateTextureFromImage(const char* fileName, int32_t key, std::function<void(void*)> handler, bool srgb, ImageTextureFormatOptions option, bool linearSampler){
        if(key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([already, fileName, srgb, option]() -> void*{
            if(!already){
                int x, y, nChannels;
                uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
                ImageTextureFormatOptions opt = option;
                ktx_error_code_e k2result;
                if (!pix) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                ktxTexture2* texture = createKTX2FromImage(pix, x, y, 4, srgb, opt);
                stbi_image_free(pix);
                if (!texture) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                if ((k2result = tryTranscode(texture, nChannels, srgb, option != ImageTextureFormatOptions::IT_USE_COMPRESS)) != KTX_SUCCESS) {
                    return new __asyncparam{ nullptr, k2result };
                }
                return new __asyncparam{ texture, KTX_SUCCESS };
            }
            return nullptr;
        }, [key, handler, linearSampler](void* param) {
            if (!param) {
                size_t p = key;
                handler((void*)p);
            }
            else {
                __asyncparam* ap = reinterpret_cast<__asyncparam*>(param);
                ktxTexture2* texture = ap->texture;
                int32_t k2result = ap->k2result;
                delete ap;
                if (k2result != KTX_SUCCESS) {
                    size_t p = key | (size_t)(k2result << 32);
                    handler((void*)p);
                }
                else {
                    unsigned tex = 0, targ, err;
                    k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                    if (k2result != KTX_SUCCESS) {
                        LOGWITH("Failed to transcode ktx texture:", k2result, err);
                        ktxTexture_Destroy(ktxTexture(texture));
                    }
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    struct txtr :public Texture { inline txtr(uint32_t _1, uint32_t _2, uint16_t _3, uint16_t _4) :Texture(_1, _2, _3, _4) {} };
                    pTexture ret = std::make_shared<txtr>(tex, 0, texture->baseWidth, texture->baseHeight);
                    singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                    ktxTexture_Destroy(ktxTexture(texture));
                    size_t p = key;
                    handler((void*)p);
                }
            }
        }, vkm_strand::GENERAL);
    }

    void GLMachine::asyncCreateTextureFromImage(const uint8_t* mem, size_t size, int32_t key, std::function<void(void*)> handler, bool srgb, ImageTextureFormatOptions option, bool linearSampler){
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([already, mem, size, srgb, option]()->void* {
            if (!already) {
                int x, y, nChannels;
                uint8_t* pix = stbi_load_from_memory(mem, size, &x, &y, &nChannels, 0);
                if (!pix) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                ImageTextureFormatOptions opt = option;
                ktx_error_code_e k2result;
                ktxTexture2* texture = createKTX2FromImage(pix, x, y, 4, srgb, opt);
                stbi_image_free(pix);
                if (!texture) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                if ((k2result = tryTranscode(texture, nChannels, srgb, option != ImageTextureFormatOptions::IT_USE_COMPRESS)) != KTX_SUCCESS) {
                    return new __asyncparam{ nullptr, k2result };
                }
                return new __asyncparam{ texture, KTX_SUCCESS };
            }
            return nullptr;
            }, [key, handler, linearSampler](void* param) {
                if (!param) {
                    size_t p = key;
                    handler((void*)p);
                }
                else {
                    __asyncparam* ap = reinterpret_cast<__asyncparam*>(param);
                    ktxTexture2* texture = ap->texture;
                    int32_t k2result = ap->k2result;
                    delete ap;
                    if (k2result != KTX_SUCCESS) {
                        size_t p = key | (size_t)(k2result << 32);
                        handler((void*)p);
                    }
                    else {
                        unsigned tex = 0, targ, err;
                        k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                        if (k2result != KTX_SUCCESS) {
                            LOGWITH("Failed to transcode ktx texture:", k2result, err);
                            ktxTexture_Destroy(ktxTexture(texture));
                        }
                        glBindTexture(GL_TEXTURE_2D, tex);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                        glBindTexture(GL_TEXTURE_2D, 0);

                        struct txtr :public Texture { inline txtr(uint32_t _1, uint32_t _2, uint16_t _3, uint16_t _4) :Texture(_1, _2, _3, _4) {} };
                        pTexture ret = std::make_shared<txtr>(tex, 0, texture->baseWidth, texture->baseHeight);
                        ktxTexture_Destroy(ktxTexture(texture));
                        singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                        size_t p = key;
                        handler((void*)p);
                    }
                }
            }, vkm_strand::GENERAL);
    }

    void GLMachine::asyncCreateTexture(const uint8_t* mem, size_t size, uint32_t nChannels, std::function<void(void*)> handler, int32_t key, bool srgb, bool hq, bool linearSampler) {
        if(key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        struct __asyncparam {
            ktxTexture2* texture;
            int32_t k2result;
        };
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([mem, size, nChannels, srgb, hq, already, linearSampler, key]()->void* {
            if (!already) {
                ktxTexture2* texture;
                ktx_error_code_e k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture);
                if (k2result != KTX_SUCCESS) {
                    return new __asyncparam{ nullptr, k2result };
                }
                if ((k2result = tryTranscode(texture, nChannels, srgb, hq)) != KTX_SUCCESS) {
                    return new __asyncparam{ nullptr, k2result };
                }
                return new __asyncparam{ texture, KTX_SUCCESS };
            }
            return (void*)key;
        }, [key, handler, linearSampler](void* param) {
            if (!param) {
                size_t p = key;
                handler((void*)p);
            }
            else {
                __asyncparam* ap = reinterpret_cast<__asyncparam*>(param);
                ktxTexture2* texture = ap->texture;
                int32_t k2result = ap->k2result;
                delete ap;
                if (k2result != KTX_SUCCESS) {
                    size_t p = key | (size_t)(k2result << 32);
                    handler((void*)p);
                }
                else {
                    unsigned tex = 0, targ, err;
                    k2result = ktxTexture_GLUpload(ktxTexture(texture), &tex, &targ, &err);
                    if (k2result != KTX_SUCCESS) {
                        LOGWITH("Failed to transcode ktx texture:", k2result, err);
                        ktxTexture_Destroy(ktxTexture(texture));
                    }
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    struct txtr :public Texture { inline txtr(uint32_t _1, uint32_t _2, uint16_t _3, uint16_t _4) :Texture(_1, _2, _3, _4) {} };
                    pTexture ret = std::make_shared<txtr>(tex, 0, texture->baseWidth, texture->baseHeight);
                    singleton->textures[key] = std::move(ret); // 메인 스레드라서 락 안함
                    ktxTexture_Destroy(ktxTexture(texture));
                    size_t p = key;
                    handler((void*)p);
                }
            }
        }, vkm_strand::GENERAL);
    }

    GLMachine::Texture::Texture(uint32_t txo, uint32_t binding, uint16_t width, uint16_t height) :txo(txo), binding(binding), width(width), height(height) { }
    GLMachine::Texture::~Texture(){
        glDeleteTextures(1, &txo);
    }

    void GLMachine::Texture::collect(bool removeUsing) {
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

    void GLMachine::Texture::drop(int32_t name){
        singleton->textures.erase(name);
    }

    GLMachine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, unsigned c1, unsigned c2, unsigned c3, unsigned ds, bool depthAsTexture, unsigned framebuffer)
        :type(type), width(width), height(height), color1(c1), color2(c2), color3(c3), depthStencil(ds), dsTexture(depthAsTexture), framebuffer(framebuffer) {
    }

    GLMachine::UniformBuffer* GLMachine::createUniformBuffer(uint32_t length, uint32_t size, size_t stages, int32_t name, uint32_t binding){
        UniformBuffer* ret = getUniformBuffer(name);
        if(ret) return ret;
        unsigned ubo;
        glGenBuffers(1, &ubo);
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        if(name == INT32_MIN) return new UniformBuffer(length, ubo, binding);
        return singleton->uniformBuffers[name] = new UniformBuffer(length, ubo, binding);
    }

    GLMachine::RenderTarget::~RenderTarget(){
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

    GLMachine::RenderPass2Cube* GLMachine::createRenderPass2Cube(uint32_t width, uint32_t height, int32_t key, bool useColor, bool useDepth) {
        RenderPass2Cube* r = getRenderPass2Cube(key);
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
            glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, color, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        }
        else {
            glDrawBuffer(GL_NONE);
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
            glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depth, 0);
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

        r = new RenderPass2Cube;
        r->targetCubeC = color;
        r->targetCubeD = depth;
        r->fbo = fbo;
        r->viewport.x = 0;
        r->viewport.y = 0;
        r->viewport.width = width;
        r->viewport.height = height;
        r->viewport.minDepth = -1;
        r->viewport.maxDepth = 1;
        r->scissor.x = 0;
        r->scissor.y = 0;
        r->scissor.width = width;
        r->scissor.height = height;
        
        return singleton->cubePasses[key] = r;
    }

    GLMachine::RenderPass2Screen* GLMachine::createRenderPass2Screen(RenderTargetType* tgs, uint32_t subpassCount, int32_t name, bool useDepth, bool* useDepthAsInput){
        RenderPass2Screen* r = getRenderPass2Screen(name);
        if(r) return r;

        if(subpassCount == 0) return nullptr;
        std::vector<RenderTarget*> targets(subpassCount);
        for(uint32_t i = 0; i < subpassCount - 1; i++){
            targets[i] = createRenderTarget2D(singleton->surfaceWidth, singleton->surfaceHeight, INT32_MIN, tgs[i], RenderTargetInputOption::SAMPLED_LINEAR, useDepthAsInput ? useDepthAsInput[i] : false);
            if(!targets[i]){
                LOGHERE;
                for(RenderTarget* t:targets) delete t;
                return nullptr;
            }
        }

        RenderPass* ret = new RenderPass(targets.data(), subpassCount);
        ret->targets = std::move(targets);
        ret->setViewport(singleton->surfaceWidth, singleton->surfaceHeight, 0.0f, 0.0f);
        ret->setScissor(singleton->surfaceWidth, singleton->surfaceHeight, 0, 0);
        if (name == INT32_MIN) return ret;
        return singleton->renderPasses[name] = ret;
    }

    GLMachine::RenderPass* GLMachine::createRenderPass(RenderTarget** targets, uint32_t subpassCount, int32_t name){
        RenderPass* r = getRenderPass(name);
        if(r) return r;
        if(subpassCount == 0) return nullptr;

        RenderPass* ret = new RenderPass(targets, subpassCount);
        std::memcpy(ret->targets.data(), targets, sizeof(RenderTarget*) * subpassCount);
        ret->setViewport(targets[0]->width, targets[0]->height, 0.0f, 0.0f);
        ret->setScissor(targets[0]->width, targets[0]->height, 0, 0);
        if(name == INT32_MIN) return ret;
        return singleton->renderPasses[name] = ret;
    }

    unsigned GLMachine::createPipeline(unsigned vs, unsigned fs, int32_t name, unsigned tc, unsigned te, unsigned gs){
        unsigned ret = getPipeline(name);
        if(ret) {
            return ret;
        }

        if(!(vs | fs)){
            LOGWITH("Vertex and fragment shader should be provided.");
            return 0;
        }

        unsigned prog = glCreateProgram();
        if (prog == 0) {
            LOGWITH("Failed to create program");
            return 0;
        }

        glAttachShader(prog, vs);
        if (tc) glAttachShader(prog, tc);
        if (te) glAttachShader(prog, te);
        if (gs) glAttachShader(prog, gs);
        glAttachShader(prog, fs);
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

        if (name == INT32_MIN) return prog;
        return singleton->pipelines[name] = prog;
    }

    unsigned GLMachine::createPipelineLayout(...){
        return 0;
    }

    static void dummyBinder(size_t, uint32_t, uint32_t) {}

    GLMachine::Mesh::Mesh(unsigned vb, unsigned ib, size_t vcount, size_t icount, bool use32) :vb(vb), ib(ib), vcount(vcount), icount(icount), idxType(use32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT), vao(0), vaoBinder(dummyBinder) {}
    GLMachine::Mesh::~Mesh(){ 
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vb);
        glDeleteBuffers(1, &ib);
    }

    void GLMachine::Mesh::unbindVAO() {
        glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void GLMachine::Mesh::update(const void* input, uint32_t offset, uint32_t size){
        glBindBuffer(GL_ARRAY_BUFFER, vb);
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, input);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLMachine::Mesh::updateIndex(const void* input, uint32_t offset, uint32_t size){
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, input);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void GLMachine::Mesh::collect(bool removeUsing) {
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

    void GLMachine::Mesh::drop(int32_t name){
        singleton->meshes.erase(name);
    }

    GLMachine::RenderPass::RenderPass(RenderTarget** fb, uint16_t stageCount): stageCount(stageCount), pipelines(stageCount), targets(stageCount){}

    GLMachine::RenderPass::~RenderPass(){
        if (targets[stageCount - 1] == nullptr) { // renderpass to screen이므로 타겟을 자체 생성해서 보유
            for (RenderTarget* targ : targets) {
                delete targ;
            }
        }
    }

    void GLMachine::RenderPass::usePipeline(unsigned pipeline, unsigned subpass){
        if(subpass > stageCount){
            LOGWITH("Invalid subpass. This renderpass has", stageCount, "subpasses but", subpass, "given");
            return;
        }
        pipelines[subpass] = pipeline;
        if(currentPass == subpass) { glUseProgram(pipeline); }
    }

    void GLMachine::RenderPass::setViewport(float width, float height, float x, float y, bool applyNow){
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

    void GLMachine::RenderPass::setScissor(uint32_t width, uint32_t height, int32_t x, int32_t y, bool applyNow){
        scissor.width = width;
        scissor.height = height;
        scissor.x = x;
        scissor.y = y;
        if(applyNow && currentPass != -1) {
            glScissor(x,y,width,height);
        }
    }

    void GLMachine::RenderPass::bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        glBindBufferRange(GL_UNIFORM_BUFFER, ub->binding, ub->ubo, 0, ub->length);
    }

    void GLMachine::RenderPass::bind(uint32_t pos, const pTexture& tx) {
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        glActiveTexture(GL_TEXTURE0 + tx->binding);
        glBindTexture(GL_TEXTURE_2D, tx->txo);
    }

    void GLMachine::RenderPass::bind(uint32_t pos, RenderTarget* target, uint32_t index){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        unsigned dset;
        switch(index){
            case 0:
                dset = target->color1;
                break;
            case 1:
                dset = target->color2;
                break;
            case 2:
                dset = target->color3;
                break;
            case 3:
                dset = target->depthStencil;
                break;
            default:
                LOGWITH("Invalid render target index");
                return;
        }
        if(!dset) {
            LOGWITH("Invalid render target index");
            return;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, dset);
    }

    void GLMachine::RenderPass::push(void* input, uint32_t start, uint32_t end){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        getUniformBuffer(INT32_MIN + 1)->update(input, 0, start, end - start);
    }

    void GLMachine::RenderPass::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        
        if((bound != mesh.get()) && (mesh->vao != 0)) {
            glBindVertexArray(mesh->vao);
        }
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = mesh->icount - start;
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
                count = mesh->vcount - start;
            }
            glDrawArrays(GL_TRIANGLES, start, count);
        }
        bound = mesh.get();
    }

    void GLMachine::RenderPass::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count){
         if(currentPass == -1){
             LOGWITH("Invalid call: render pass not begun");
             return;
         }
         if ((bound != mesh.get()) && (mesh->vb != 0)) {
             glBindVertexArray(mesh->vao);
         }
         instanceInfo->vaoBinder(0, 0, mesh->attrCount);
         for (int i = mesh->attrCount; i < mesh->attrCount + instanceInfo->attrCount; i++) {
             glVertexAttribDivisor(i, 1);
         }
         if(mesh->icount) {
             if((uint64_t)start + count > mesh->icount){
                 LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                 bound = nullptr;
                 return;
             }
             if(count == 0){
                 count = mesh->icount - start;
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
         for (int i = mesh->attrCount; i < mesh->attrCount + instanceInfo->attrCount; i++) {
             glDisableVertexArrayAttrib(mesh->vao, i);
         }
         bound = nullptr;
    }

    void GLMachine::RenderPass::execute(RenderPass* other){
        if(currentPass != pipelines.size() - 1){
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        currentPass = -1;
    }

    bool GLMachine::RenderPass::wait(uint64_t timeout){
        return true;
    }

    void GLMachine::RenderPass::start(uint32_t pos){
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
        }
        else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        glUseProgram(pipelines[currentPass]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
        glDepthRange(viewport.minDepth, viewport.maxDepth);
        glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
    }

    GLMachine::RenderPass2Cube::~RenderPass2Cube(){
        glDeleteFramebuffers(1, &fbo);
        if (targetCubeC) glDeleteTextures(1, &targetCubeC);
        if (targetCubeD) glDeleteTextures(1, &targetCubeD);
    }

    void GLMachine::RenderPass2Cube::bind(uint32_t pos, UniformBuffer* ub, uint32_t pass, uint32_t ubPos){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if (pass >= 6) {
            glBindBufferRange(GL_UNIFORM_BUFFER, ub->binding, ub->ubo, 0, ub->length);
        }
        else {
            facewise[pass].ub = ub;
            facewise[pass].ubPos = ubPos;
            facewise[pass].setPos = pos;
        }
    }

    void GLMachine::RenderPass2Cube::bind(uint32_t pos, const pTexture& tx){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        glActiveTexture(GL_TEXTURE0 + tx->binding);
        glBindTexture(GL_TEXTURE_2D, tx->txo);
    }

    void GLMachine::RenderPass2Cube::bind(uint32_t pos, RenderTarget* target, uint32_t index){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        unsigned dset;
        switch (index) {
        case 0:
            dset = target->color1;
            break;
        case 1:
            dset = target->color2;
            break;
        case 2:
            dset = target->color3;
            break;
        case 3:
            dset = target->depthStencil;
            break;
        default:
            LOGWITH("Invalid render target index");
            return;
        }
        if (!dset) {
            LOGWITH("Invalid render target index");
            return;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, dset);
    }
    
    void GLMachine::RenderPass2Cube::usePipeline(unsigned pipeline){
        this->pipeline = pipeline;
        if (recording) { glUseProgram(pipeline); }
    }

    void GLMachine::RenderPass2Cube::push(void* input, uint32_t start, uint32_t end){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        getUniformBuffer(INT32_MIN + 1)->update(input, 0, start, end - start);
    }

    void GLMachine::RenderPass2Cube::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
         glBindFramebuffer(GL_FRAMEBUFFER, fbo);
         if ((bound != mesh.get()) && (mesh->vao != 0)) {
             glBindVertexArray(mesh->vao);
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
                 if(targetCubeC) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeC, 0);
                 if(targetCubeD) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, targetCubeD, 0);
                 auto& fwi = facewise[i];
                 if (fwi.ub) {
                     glBindBufferRange(GL_UNIFORM_BUFFER, fwi.ub->binding, fwi.ub->ubo, 0, fwi.ub->length);
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
                     glBindBufferRange(GL_UNIFORM_BUFFER, fwi.ub->binding, fwi.ub->ubo, 0, fwi.ub->length);
                 }
                 glDrawArrays(GL_TRIANGLES, start, count);
             }
         }
         bound = mesh.get();
    }

    void GLMachine::RenderPass2Cube::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count) {
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
         }
         if ((bound != mesh.get()) && (mesh->vao != 0)) {
             glBindVertexArray(mesh->vao);
         }
         instanceInfo->vaoBinder(0, 0, mesh->attrCount);
         for (int i = mesh->attrCount; i < mesh->attrCount + instanceInfo->attrCount; i++) {
             glVertexAttribDivisor(i, 1);
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
                     glBindBufferRange(GL_UNIFORM_BUFFER, fwi.ub->binding, fwi.ub->ubo, 0, fwi.ub->length);
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
                     glBindBufferRange(GL_UNIFORM_BUFFER, fwi.ub->binding, fwi.ub->ubo, 0, fwi.ub->length);
                 }
                 glDrawArraysInstanced(GL_TRIANGLES, start, count, instanceCount);
             }
         }
         for (int i = mesh->attrCount; i < mesh->attrCount + instanceInfo->attrCount; i++) {
             glDisableVertexArrayAttrib(mesh->vao, i);
         }
        bound = nullptr;
    }

    void GLMachine::RenderPass2Cube::execute(RenderPass* other){
        if(!recording){
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        recording = false;
    }

    bool GLMachine::RenderPass2Cube::wait(uint64_t timeout){
        return true;
    }

    void GLMachine::RenderPass2Cube::start(){
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
        glDepthRange(viewport.minDepth, viewport.maxDepth);
        glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
    }

    void GLMachine::UniformBuffer::update(const void* input, uint32_t index, uint32_t offset, uint32_t size){
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, input);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void GLMachine::UniformBuffer::updatePush(const void* input, uint32_t offset, uint32_t size) {
        singleton->uniformBuffers[INT32_MIN + 1]->update(input, 0, offset, size);
    }

    void GLMachine::UniformBuffer::resize(uint32_t size) {    }

    GLMachine::UniformBuffer::UniformBuffer(uint32_t length, unsigned ubo, uint32_t binding) :length(length), ubo(ubo), binding(binding) {}

    GLMachine::UniformBuffer::~UniformBuffer(){
        glDeleteBuffers(1, &ubo);
    }


    // static함수들 구현

    int textureFormatFallback(uint32_t nChannels, bool srgb, bool hq) {
        int count;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &count);
        std::vector<int> availableFormat(count);
        glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, availableFormat.data());
        std::unordered_set<int> formatSet(availableFormat.begin(), availableFormat.end());
    #define CHECK_N_RETURN(f) if(formatSet.find(f) != formatSet.end()) return f
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

    static void GLAPIENTRY glOnError(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
        const char* sev = "OpenGL";
        switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:
            sev = "Error";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
        case GL_DEBUG_SEVERITY_LOW:
            sev = "Warning";
            break;
        }
        LOGWITH(sev,id,':',message,'(',severity,')');
        GLMachine::reason = id;
    }

    unsigned createVA(unsigned vb, unsigned ib) {
        unsigned vao;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vb);
		if(ib) glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib);
        return vao;
    }

    void enableAttribute(int index, int stride, int offset, _vattr type){
        glEnableVertexAttribArray(index);
        switch(type.type){
            case _vattr::t::F32:
                glVertexAttribPointer(index, type.dim, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                break;
            case _vattr::t::F64:
                glVertexAttribPointer(index, type.dim, GL_DOUBLE, GL_FALSE, stride, (void*)offset);
                break;
            case _vattr::t::I8:
                glVertexAttribIPointer(index, type.dim, GL_BYTE, stride, (void*)offset);
                break;
            case _vattr::t::I16:
                glVertexAttribIPointer(index, type.dim, GL_SHORT, stride, (void*)offset);
                break;
            case _vattr::t::I32:
                glVertexAttribIPointer(index, type.dim, GL_INT, stride, (void*)offset);
                break;
            case _vattr::t::U8:
                glVertexAttribIPointer(index, type.dim, GL_UNSIGNED_BYTE, stride, (void*)offset);
                break;
            case _vattr::t::U16:
                glVertexAttribIPointer(index, type.dim, GL_UNSIGNED_SHORT, stride, (void*)offset);
                break;
            case _vattr::t::U32:
                glVertexAttribIPointer(index, type.dim, GL_UNSIGNED_INT, stride, (void*)offset);
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
        case GL_STACK_OVERFLOW:
            return "Stack overflow";
        case GL_STACK_UNDERFLOW:
            return "Stack underflow";
        case GL_OUT_OF_MEMORY:
            return "Out of memory";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "Cannot do this operation for this framebuffer";
        case GL_CONTEXT_LOST:
            return "GL context lost";
        default:
            return "Unknown Error";
            break;
        }
    }
}