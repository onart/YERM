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
#include "../externals/glfw/glfw3.h"

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

    const Mesh* bound = nullptr;

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

        if constexpr(USE_OPENGL_DEBUG) {
            glEnable(GL_DEBUG_OUTPUT);
            glDebugMessageCallback(glOnError,0);
        }

        singleton = this;
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
        auto it = singleton->finalPasses.find(name);
        if(it != singleton->finalPasses.end()) return it->second;
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

    void GLMachine::createSwapchain(uint32_t width, uint32_t height, Window* window) {}

    void GLMachine::destroySwapchain(){}

    void GLMachine::free() {
        for(auto& cp: cubePasses) { delete cp.second; }
        for(auto& fp: finalPasses) { delete fp.second; }
        for(auto& rp: renderPasses) { delete rp.second; }
        for(auto& rt: renderTargets){ delete rt.second; }
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
        destroySwapchain();
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
        struct publicmesh:public Mesh{publicmesh(VkBuffer _1, VmaAllocation _2, size_t _3, size_t _4,size_t _5,void* _6,bool _7):Mesh(_1,_2,_3,_4,_5,_6,_7){}};
        if(name == INT32_MIN) return std::make_shared<publicmesh>(VK_NULL_HANDLE,VK_NULL_HANDLE,vcount,0,0,nullptr,false);
        return singleton->meshes[name] = std::make_shared<publicmesh>(VK_NULL_HANDLE,VK_NULL_HANDLE,vcount,0,0,nullptr,false);
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
        if(!singleton->allocator) {
            LOGWITH("Warning: Tried to create image before initialization");
            return nullptr;
        }
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

    void GLMachine::removeImageSet(GLMachine::ImageSet* set) {
        auto it = images.find(set);
        if(it != images.end()) {
            (*it)->free();
            delete *it;
            images.erase(it);
        }
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
            return ktxTexture2_TranscodeBasis(texture, tf, 0));
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
        singleton->loadThread.post([fileName, nChannels, srgb, hq, already](){
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
                            LOGWITH("Failed to transcode ktx texture:", k2result, glError);
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
        singleton->loadThread.post([already, fileName, srgb, option](){
            if(!already){
                int x, y, nChannels;
                uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
                if (!pix) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                ktxTexture2* texture = createKTX2FromImage(pix, x, y, 4, srgb, option);
                stbi_image_free(pix);
                if (!texture) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                if ((k2result = tryTranscode(texture, nChannels, srgb, hq)) != KTX_SUCCESS) {
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
                        LOGWITH("Failed to transcode ktx texture:", k2result, glError);
                        ktxTexture_Destroy(ktxTexture(texture));
                    }
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linearSampler ? GL_LINEAR : GL_NEAREST);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    struct txtr :public Texture { inline txtr(uint32_t _1, uint32_t _2, uint16_t _3, uint16_t _4) :Texture(_1, _2) {} };
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
        singleton->loadThread.post([already, mem, size, srgb, option]() {
            if (!already) {
                int x, y, nChannels;
                uint8_t* pix = stbi_load_from_memory(mem, size, &x, &y, &nChannels, 0);
                if (!pix) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                ktxTexture2* texture = createKTX2FromImage(pix, x, y, 4, srgb, option);
                stbi_image_free(pix);
                if (!texture) {
                    return new __asyncparam{ nullptr, ktx_error_code_e::KTX_FILE_READ_ERROR };
                }
                if ((k2result = tryTranscode(texture, nChannels, srgb, hq)) != KTX_SUCCESS) {
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
                            LOGWITH("Failed to transcode ktx texture:", k2result, glError);
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
        singleton->loadThread.post([mem, size, nChannels, srgb, hq, already, linearSampler]() {
            if (!already) {
                ktx_error_code_e k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture));
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
                        LOGWITH("Failed to transcode ktx texture:", k2result, glError);
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

    GLMachine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, unsigned color1, unsigned color2, unsigned color3, unsigned depthstencil, bool depthTexture)
        :type(type), width(width), height(height), color1(color1), color2(color2), color3(color3), depthStencil(depthstencil), depthTexture(depthTexture) {
        
    }

    uint32_t GLMachine::RenderTarget::getDescriptorSets(VkDescriptorSet* sets){
        int nim = 0;
        if(dset1) {
            sets[nim++]=dset1;
            if(dset2){
                sets[nim++]=dset2;
                if(dset3){
                    sets[nim++]=dset3;
                }
            }
        }
        if(depthstencil){
            sets[nim] = dsetDS;
        }
        return nim;
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
        glDeleteFramebuffers(1, &framebuffer);
    }

    GLMachine::RenderPass2Cube* GLMachine::createRenderPass2Cube(uint32_t width, uint32_t height, int32_t key, bool useColor, bool useDepth) {
        RenderPass2Cube* r = getRenderPass2Cube(key);
        if(r) return r;
        if(!(useColor || useDepth)){
            LOGWITH("At least one of useColor and useDepth should be true");
            return nullptr;
        }
        //const bool CAN_USE_GEOM = singleton->physicalDevice.features.geometryShader;

        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 6;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VkImage colorImage = VK_NULL_HANDLE, depthImage = VK_NULL_HANDLE;
        VmaAllocation colorAlloc = {}, depthAlloc = {};
        VkImageView targets[12]{}; // 앞 6개는 컬러, 뒤 6개는 깊이
        VkImageView texture = VK_NULL_HANDLE; // 컬러가 있으면 컬러, 깊이만 있으면 깊이. (큐브맵 텍스처가 가능한 경우에 한해서)

        if(useColor) {
            imgInfo.format = singleton->surface.format.format;
            imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &colorImage, &colorAlloc, nullptr);
            if(reason != VK_SUCCESS) {
                LOGWITH("Failed to create image:",reason,resultAsString(reason));
                return nullptr;
            }
        }
        if(useDepth) {
            imgInfo.format = VK_FORMAT_D32_SFLOAT;
            imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            if(!useColor) imgInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &depthImage, &depthAlloc, nullptr);
            if(reason != VK_SUCCESS) {
                LOGWITH("Failed to create image:",reason,resultAsString(reason));
                vmaDestroyImage(singleton->allocator, colorImage, colorAlloc);
                return nullptr;
            }
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        if(useColor) {
            viewInfo.image = colorImage;
            viewInfo.format = singleton->surface.format.format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            for(int i = 0; i < 6; i++){
                reason = vkCreateImageView(singleton->device, &viewInfo, nullptr, &targets[i]);
                if(reason != VK_SUCCESS) {
                    LOGWITH("Failed to create image view:",reason,resultAsString(reason));
                    for(int j = 0; j < i; j++) {
                        vkDestroyImageView(singleton->device, targets[j], nullptr);
                    }
                    vmaDestroyImage(singleton->allocator, colorImage, colorAlloc);
                    vmaDestroyImage(singleton->allocator, depthImage, depthAlloc);
                    return nullptr;
                }
            }
        }

        if(useDepth) {
            viewInfo.image = depthImage;
            viewInfo.format = VK_FORMAT_D32_SFLOAT;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            for(int i = 6; i < 12; i++){
                reason = vkCreateImageView(singleton->device, &viewInfo, nullptr, &targets[i]);
                if(reason != VK_SUCCESS) {
                    LOGWITH("Failed to create image view:",reason,resultAsString(reason));
                    for(int j = 0; j < i; j++) {
                        vkDestroyImageView(singleton->device, targets[j], nullptr);
                    }
                    vmaDestroyImage(singleton->allocator, colorImage, colorAlloc);
                    vmaDestroyImage(singleton->allocator, depthImage, depthAlloc);
                    return nullptr;
                }
            }
        }

        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.subresourceRange.layerCount = 6;
        viewInfo.image = useColor ? colorImage : depthImage;
        viewInfo.format = useColor ? singleton->surface.format.format : VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask = useColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT; // ??
        reason = vkCreateImageView(singleton->device, &viewInfo, nullptr, &texture);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create cube image view:",reason,resultAsString(reason));
            for(int j = 0; j < 12; j++) {
                vkDestroyImageView(singleton->device, targets[j], nullptr);
            }
            vmaDestroyImage(singleton->allocator, colorImage, colorAlloc);
            vmaDestroyImage(singleton->allocator, depthImage, depthAlloc);
            return nullptr;
        }


        VkSubpassDescription subpassDesc{};
        VkAttachmentReference refs[2]{};
        VkAttachmentDescription attachs[2]{};

        refs[0].attachment = 0;
        refs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        refs[1].attachment = useColor ? 1 : 0;
        refs[1].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // 스텐실 없어도 기본적으로 이 값을 써야 함. separate feature를 켜면 depth만 optimal 가능

        attachs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachs[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachs[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachs[0].format = singleton->surface.format.format;

        attachs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachs[1].finalLayout = useColor ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachs[1].storeOp = useColor ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
        attachs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachs[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachs[1].format = VK_FORMAT_D32_SFLOAT;
        subpassDesc.colorAttachmentCount = useColor ? 1 : 0;
        subpassDesc.pColorAttachments = refs;
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.pDepthStencilAttachment = useDepth ? &refs[1] : nullptr;

        // 큐브맵 샘플 좌표 기준: 0번부터 +x, -x, +y, -y, +z, -z
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpassDesc;
        rpInfo.attachmentCount = ((int)useColor + (int)useDepth);
        rpInfo.pAttachments = useColor ? attachs : attachs + 1;

        VkRenderPass rp{};
        VkFramebuffer fb[6]{};

        reason = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &rp);
        if(reason != VK_SUCCESS) {
            LOGWITH("Failed to create render pass:", reason, resultAsString(reason));
            for(int j = 0; j < 12; j++) {
                vkDestroyImageView(singleton->device, targets[j], nullptr);
            }
            vmaDestroyImage(singleton->allocator, colorImage, colorAlloc);
            vmaDestroyImage(singleton->allocator, depthImage, depthAlloc);
            return nullptr;
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.attachmentCount = rpInfo.attachmentCount;
        VkImageView fbatt[2] = {};
        fbInfo.pAttachments = fbatt;
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;
        fbInfo.renderPass = rp;
        for(int i = 0; i < 6; i++){
            fbatt[1] = targets[i+6];
            fbatt[0] = useColor ? targets[i] : targets[i+6];
            reason = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb[i]);
            if(reason != VK_SUCCESS){
                LOGWITH("Failed to create framebuffer:", reason, resultAsString(reason));
                for(int j = 0; j < i; j++){
                    vkDestroyFramebuffer(singleton->device, fb[j], nullptr);
                }
                for(int j = 0; j < 12; j++) {
                    vkDestroyImageView(singleton->device, targets[j], nullptr);
                }
                vmaDestroyImage(singleton->allocator, colorImage, colorAlloc);
                vmaDestroyImage(singleton->allocator, depthImage, depthAlloc);
                vkDestroyRenderPass(singleton->device, rp, nullptr);
                return nullptr;
            }
        }

        VkCommandBuffer prim, sec;
        VkCommandBuffer facewise[6]{};
        VkDescriptorSet dset;

        VkFence fence = singleton->createFence(true);
        VkSemaphore semaphore = singleton->createSemaphore();
        singleton->allocateCommandBuffers(1, true, true, &prim);
        singleton->allocateCommandBuffers(1, false, true, &sec);
        singleton->allocateCommandBuffers(6, false, true, facewise);
        
        singleton->allocateDescriptorSets(&singleton->textureLayout[1], 1, &dset); // 바인딩 1

        if(!prim || !sec || !fence || !semaphore || !dset || !facewise[0]){
            LOGHERE;
            vkDestroySemaphore(singleton->device, semaphore, nullptr);
            vkDestroyFence(singleton->device, fence, nullptr);
            vkFreeCommandBuffers(singleton->device, singleton->gCommandPool, 1, &prim);
            vkFreeCommandBuffers(singleton->device, singleton->gCommandPool, 1, &sec);
            vkFreeCommandBuffers(singleton->device, singleton->gCommandPool, 6, facewise);
            for(int j = 0; j < 6; j++){
                vkDestroyFramebuffer(singleton->device, fb[j], nullptr);
            }
            for(int j = 0; j < 12; j++) {
                vkDestroyImageView(singleton->device, targets[j], nullptr);
            }
            vmaDestroyImage(singleton->allocator, colorImage, colorAlloc);
            vmaDestroyImage(singleton->allocator, depthImage, depthAlloc);
            vkDestroyRenderPass(singleton->device, rp, nullptr);
            return nullptr;
        }

        VkWriteDescriptorSet writer{};
        VkDescriptorImageInfo diInfo{};
        diInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        diInfo.imageView = texture;
        diInfo.sampler = singleton->textureSampler[0];
        
        writer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writer.descriptorCount = 1;
        writer.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writer.dstBinding = 1;
        writer.dstSet = dset;
        writer.pImageInfo = &diInfo;
        writer.dstArrayElement = 0;
        vkUpdateDescriptorSets(singleton->device, 1, &writer, 0, nullptr);

        r = new RenderPass2Cube;
        std::copy(targets, targets+12, r->ivs);
        std::copy(fb, fb + 6, r->fbs);
        std::copy(facewise, facewise + 6, r->facewise);
        r->rp = rp;
        r->width = width;
        r->height = height;
        r->colorAlloc = colorAlloc;
        r->colorTarget = colorImage;
        r->depthAlloc = depthAlloc;
        r->depthTarget = depthImage;
        r->fence = fence;
        r->semaphore = semaphore;
        r->cb = prim;
        r->scb = sec;
        r->csamp = dset;
        r->tex = texture;
        for(int face = 0; face < 6; face++){ // 아무 동작 없이도 바로 제출할 수 있게
            r->beginFacewise(face);
            vkEndCommandBuffer(r->facewise[face]);
        }
        return singleton->cubePasses[key] = r;
    }

    GLMachine::RenderPass2Screen* GLMachine::createRenderPass2Screen(RenderTargetType* tgs, uint32_t subpassCount, int32_t name, bool useDepth, bool* useDepthAsInput){
        RenderPass2Screen* r = getRenderPass2Screen(name);
        if(r) return r;

        if(subpassCount == 0) return nullptr;
        std::vector<RenderTarget*> targets(subpassCount - 1);
        for(uint32_t i = 0; i < subpassCount - 1; i++){
            targets[i] = createRenderTarget2D(singleton->swapchain.extent.width, singleton->swapchain.extent.height, INT32_MIN, tgs[i], RenderTargetInputOption::INPUT_ATTACHMENT, useDepthAsInput ? useDepthAsInput[i] : false);
            if(!targets[i]){
                LOGHERE;
                for(RenderTarget* t:targets) delete t;
                return nullptr;
            }
        }

        VkImage dsImage = VK_NULL_HANDLE;
        VmaAllocation dsAlloc = VK_NULL_HANDLE;
        VkImageView dsImageView = VK_NULL_HANDLE;

        if(subpassCount == 1 && useDepth) {
            VkImageCreateInfo imgInfo{};
            imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.arrayLayers = 1;
            imgInfo.extent.depth = 1;
            imgInfo.extent.width = singleton->swapchain.extent.width;
            imgInfo.extent.height = singleton->swapchain.extent.height;
            imgInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
            imgInfo.mipLevels = 1;
            imgInfo.imageType = VK_IMAGE_TYPE_2D;
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            
            if((reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &dsImage, &dsAlloc, nullptr)) != VK_SUCCESS){
                LOGWITH("Failed to create depth/stencil image for last one");
                for(RenderTarget* t:targets) delete t;
                return nullptr;
            }

            dsImageView = createImageView(singleton->device, dsImage, VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
            if(!dsImageView){
                LOGHERE;
                vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
                for(RenderTarget* t:targets) delete t;
                return nullptr;
            }
        }

        std::vector<VkSubpassDescription> subpasses(subpassCount);
        std::vector<VkAttachmentDescription> attachments(subpassCount * 4);
        std::vector<VkAttachmentReference> colorRefs(subpassCount * 4);
        std::vector<VkAttachmentReference> inputRefs(subpassCount * 4);
        std::vector<VkSubpassDependency> dependencies(subpassCount);
        std::vector<VkImageView> ivs(subpassCount * 4);

        uint32_t totalAttachments = 0;
        uint32_t totalInputAttachments = 0;
        uint32_t inputAttachmentCount = 0;
    
        for(uint32_t i = 0; i < subpassCount - 1; i++){
            uint32_t colorCount = targets[i]->attachmentRefs(&attachments[totalAttachments], false);
            subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount = colorCount;
            subpasses[i].pColorAttachments = &colorRefs[totalAttachments];
            subpasses[i].inputAttachmentCount = inputAttachmentCount;
            subpasses[i].pInputAttachments = &inputRefs[totalInputAttachments - inputAttachmentCount];
            if(targets[i]->depthstencil) subpasses[i].pDepthStencilAttachment = &colorRefs[totalAttachments + colorCount];
            VkImageView views[4] = {
                targets[i]->color1 ? targets[i]->color1->view : VK_NULL_HANDLE,
                targets[i]->color2 ? targets[i]->color2->view : VK_NULL_HANDLE,
                targets[i]->color3 ? targets[i]->color3->view : VK_NULL_HANDLE,
                targets[i]->depthstencil ? targets[i]->depthstencil->view : VK_NULL_HANDLE
            };
            for(uint32_t j = 0; j < colorCount; j++) {
                colorRefs[totalAttachments].attachment = totalAttachments;
                colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                inputRefs[totalInputAttachments].attachment = totalAttachments;
                inputRefs[totalInputAttachments].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                ivs[totalAttachments] = views[j];
                totalAttachments++;
                totalInputAttachments++;
            }
            if(targets[i]->depthstencil){
                colorRefs[totalAttachments].attachment = totalAttachments;
                colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                if(targets[i]->dsetDS) {
                    inputRefs[totalInputAttachments].attachment = totalAttachments;
                    inputRefs[totalInputAttachments].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    totalInputAttachments++;
                }
                ivs[totalAttachments] = views[3];
                totalAttachments++;
            }
            dependencies[i+1].srcSubpass = i;
            dependencies[i+1].dstSubpass = i + 1;
            dependencies[i+1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[i+1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[i+1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[i+1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[i+1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            inputAttachmentCount = colorCount; if(targets[i]->dsetDS) inputAttachmentCount++;
        }

        attachments[totalAttachments].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[totalAttachments].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[totalAttachments].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[totalAttachments].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[totalAttachments].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[totalAttachments].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[totalAttachments].format = singleton->surface.format.format;
        attachments[totalAttachments].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;

        subpasses[subpassCount - 1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses[subpassCount - 1].pInputAttachments = &inputRefs[totalInputAttachments - inputAttachmentCount];
        subpasses[subpassCount - 1].inputAttachmentCount = inputAttachmentCount;
        subpasses[subpassCount - 1].colorAttachmentCount = 1;
        subpasses[subpassCount - 1].pColorAttachments = &colorRefs[totalAttachments];

        colorRefs[totalAttachments].attachment = totalAttachments;
        colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkImageView& swapchainImageViewPlace = ivs[totalAttachments];
        
        totalAttachments++;

        if(dsImage){
            attachments[totalAttachments].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[totalAttachments].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[totalAttachments].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[totalAttachments].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[totalAttachments].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[totalAttachments].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments[totalAttachments].format = VK_FORMAT_D24_UNORM_S8_UINT;
            attachments[totalAttachments].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            colorRefs[totalAttachments].attachment = totalAttachments;
            colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpasses[subpassCount - 1].pDepthStencilAttachment = &colorRefs[totalAttachments];
            ivs[totalAttachments] = dsImageView;
            totalAttachments++;
        }

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = subpassCount - 1;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.subpassCount = subpassCount;
        rpInfo.pSubpasses = subpasses.data();
        rpInfo.attachmentCount = totalAttachments;
        rpInfo.pAttachments = attachments.data();
        rpInfo.dependencyCount = subpassCount;
        rpInfo.pDependencies = &dependencies[0];
        VkRenderPass newPass;

        if((reason = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &newPass)) != VK_SUCCESS){
            LOGWITH("Failed to create renderpass:",reason,resultAsString(reason));
            for(RenderTarget* t:targets) delete t;
            vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
            return nullptr;
        }

        std::vector<VkFramebuffer> fbs(singleton->swapchain.imageView.size());
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.attachmentCount = totalAttachments;
        fbInfo.pAttachments = ivs.data();
        fbInfo.renderPass = newPass;
        fbInfo.width = singleton->swapchain.extent.width;
        fbInfo.height = singleton->swapchain.extent.height;
        fbInfo.layers = 1;
        uint32_t i = 0;
        for(VkFramebuffer& fb: fbs){
            swapchainImageViewPlace = singleton->swapchain.imageView[i++];
            if((reason = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb)) != VK_SUCCESS){
                LOGWITH("Failed to create framebuffer:",reason,resultAsString(reason));
                for(VkFramebuffer d: fbs) vkDestroyFramebuffer(singleton->device, d, nullptr);
                vkDestroyRenderPass(singleton->device, newPass, nullptr);
                vkDestroyImageView(singleton->device, dsImageView, nullptr);
                for(RenderTarget* t:targets) delete t;
                vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
                return nullptr;
            }
        }
        if(name == INT32_MIN) return new RenderPass2Screen(newPass, std::move(targets), std::move(fbs), dsImage, dsImageView, dsAlloc);
        RenderPass2Screen* ret = singleton->finalPasses[name] = new RenderPass2Screen(newPass, std::move(targets), std::move(fbs), dsImage, dsImageView, dsAlloc);
        return ret;
    }

    GLMachine::RenderPass* GLMachine::createRenderPass(RenderTarget** targets, uint32_t subpassCount, int32_t name){
        RenderPass* r = getRenderPass(name);
        if(r) return r;
        if(subpassCount == 0) return nullptr;

        RenderPass* ret = new RenderPass(newPass, fb, subpassCount);
        for(uint32_t i = 0; i < subpassCount; i++){ ret->targets[i] = targets[i]; }
        ret->setViewport(targets[0]->width, targets[0]->height, 0.0f, 0.0f);
        ret->setScissor(targets[0]->width, targets[0]->height, 0, 0);
        if(name == INT32_MIN) return ret;
        return singleton->renderPasses[name] = ret;
    }

    unsigned GLMachine::createPipeline(unsigned vs, unsigned fs, int32_t name, unsigned tc, unsigned te, unsigned gs){
        VkPipeline ret = getPipeline(name);
        if(ret) {
            pass->usePipeline(ret, layout, subpass);
            return ret;
        }

        if(!(vs | fs)){
            LOGWITH("Vertex and fragment shader should be provided.");
            return VK_NULL_HANDLE;
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
    GLMachine::Mesh::~Mesh(){ vmaDestroyBuffer(singleton->allocator, vb, vba); }

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

    GLMachine::RenderPass::RenderPass(RenderTarget** fb, uint16_t stageCount): stageCount(stageCount), pipelines(stageCount), pipelineLayouts(stageCount), targets(stageCount){
        std::memcpy(targets.data(), fb, sizeof(RenderTarget*) * stageCount);
    }

    GLMachine::RenderPass::~RenderPass(){}

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
        glBindBufferRange(GL_UNIFORM_BUFFER, ub->binding, ub->ubo, ub->offset(ubPos), ub->length);
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
        vkCmdPushConstants(cb, pipelineLayouts[currentPass], VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, start, end - start, input); // TODO: 스테이지 플래그를 살려야 함
    }

    void GLMachine::RenderPass::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        
        if((bound != mesh.get()) && (mesh->vb != VK_NULL_HANDLE)) {
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
            glDrawElements(GL_TRIANGLES, count, mesh->idxType, mesh->idxType == GL_UNSIGNED_INT ? (uint32_t*)0 + start : (uint16_t*)0 + start);
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
         if ((bound != mesh.get()) && (mesh->vb != VK_NULL_HANDLE)) {
             glBindVertexArray(mesh->vao);
         }
         instanceInfo->vaoBinder(0, 0, mesh->attrCount);
         for (int i = mesh->attrCount; i < mesh->attrCount + instanceInfo->attrCount) {
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
             glDrawElementsInstanced(GL_TRIANGLES, mesh->icount, mesh->idxType, mesh->idxType == GL_UNSIGNED_INT ? (uint32_t*)0 + start : (uint16_t*)0 + start, instanceCount);
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
         for (int i = mesh->attrCount; i < mesh->attrCount + instanceInfo->attrCount) {
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

        glBindFramebuffer(GL_FRAMEBUFFER, targets[currentPass]->framebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
        glDepthRange(viewport.minDepth, viewport.maxDepth);
        glScissor(scissor.x, scissor.y, scissor.width, scissor.height);
    }

    GLMachine::RenderPass2Cube::~RenderPass2Cube(){
        vkDestroyFence(singleton->device, fence, nullptr); fence = VK_NULL_HANDLE;
        vkDestroySemaphore(singleton->device, semaphore, nullptr); semaphore = VK_NULL_HANDLE;
        vkDestroyRenderPass(singleton->device, rp, nullptr); rp = VK_NULL_HANDLE;
        for(VkFramebuffer& fb: fbs) {vkDestroyFramebuffer(singleton->device, fb, nullptr); fb = VK_NULL_HANDLE;}
        vkDestroyImageView(singleton->device, tex, nullptr); tex = VK_NULL_HANDLE;
        vkFreeCommandBuffers(singleton->device, singleton->gCommandPool, 1, &cb); cb = VK_NULL_HANDLE;
        vkFreeCommandBuffers(singleton->device, singleton->gCommandPool, 1, &scb); scb = VK_NULL_HANDLE;
        for(VkImageView& iv:ivs) { vkDestroyImageView(singleton->device, iv, nullptr); iv = VK_NULL_HANDLE; }
        vmaDestroyImage(singleton->allocator, colorTarget, colorAlloc); colorTarget = VK_NULL_HANDLE; colorAlloc = {};
        vmaDestroyImage(singleton->allocator, depthTarget, depthAlloc); depthTarget = VK_NULL_HANDLE; depthAlloc = {};
        //vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &csamp); csamp = VK_NULL_HANDLE;
    }

    void GLMachine::RenderPass2Cube::beginFacewise(uint32_t pass){
        if(pass >= 6) return;
        VkCommandBufferInheritanceInfo ciInfo{};
        ciInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        ciInfo.renderPass = rp;
        ciInfo.framebuffer = fbs[pass];
        ciInfo.subpass = 0;
        VkCommandBufferBeginInfo cbInfo{};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        cbInfo.pInheritanceInfo = &ciInfo;
        
        if((reason = vkBeginCommandBuffer(facewise[pass], &cbInfo)) != VK_SUCCESS){
            LOGWITH("Failed to begin command buffer:",reason,resultAsString(reason));
        }
    }

    void GLMachine::RenderPass2Cube::bind(uint32_t pos, UniformBuffer* ub, uint32_t pass, uint32_t ubPos){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        ub->sync();
        uint32_t off = ub->offset(ubPos);
        if(pass >= 6) {
            vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, pos, 1, &ub->dset, ub->isDynamic, &off);
        }
        else{
            beginFacewise(pass);
            vkCmdBindDescriptorSets(facewise[pass], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, pos, 1, &ub->dset, ub->isDynamic, &off);
            vkEndCommandBuffer(facewise[pass]);
        }
    }

    void GLMachine::RenderPass2Cube::bind(uint32_t pos, const pTexture& tx){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void GLMachine::RenderPass2Cube::bind(uint32_t pos, RenderTarget* target, uint32_t index){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if(!target->sampled){
            LOGWITH("Invalid call: this target is not made with texture");
            return;
        }
        VkDescriptorSet dset;
        switch(index){
            case 0:
                dset = target->dset1;
                break;
            case 1:
                dset = target->dset2;
                break;
            case 2:
                dset = target->dset3;
                break;
            case 3:
                dset = target->dsetDS;
                break;
            default:
                LOGWITH("Invalid render target index");
                return;
        }
        if(!dset) {
            LOGWITH("Invalid render target index");
            return;
        }
        vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, pos, 1, &dset, 0, nullptr);
    }
    
    void GLMachine::RenderPass2Cube::usePipeline(VkPipeline pipeline, VkPipelineLayout layout){
        this->pipeline = pipeline;
        this->pipelineLayout = layout;
        if(recording) { vkCmdBindPipeline(scb, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); }
    }

    void GLMachine::RenderPass2Cube::push(void* input, uint32_t start, uint32_t end){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdPushConstants(scb, pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, start, end - start, input); // TODO: 스테이지 플래그를 살려야 함
    }

    void GLMachine::RenderPass2Cube::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if((bound != mesh.get()) && (mesh->vb != VK_NULL_HANDLE)) {
            VkDeviceSize offs = 0;
            vkCmdBindVertexBuffers(scb, 0, 1, &mesh->vb, &offs);
            if(mesh->icount) vkCmdBindIndexBuffer(scb, mesh->vb, mesh->ioff, mesh->idxType);
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
            vkCmdDrawIndexed(scb, count, 1, start, 0, 0);
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
            vkCmdDraw(scb, count, 1, start, 0);
        }
        bound = mesh.get();
    }

    void GLMachine::RenderPass2Cube::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count){
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        VkDeviceSize offs[2] = {0, 0};
        VkBuffer buffs[2] = {mesh->vb, instanceInfo->vb};
        vkCmdBindVertexBuffers(scb, 0, 2, buffs, offs);
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = mesh->icount - start;
            }
            vkCmdBindIndexBuffer(scb, mesh->vb, mesh->ioff, mesh->idxType);
            vkCmdDrawIndexed(scb, count, instanceCount, start, 0, istart);
        }
        else{
            if((uint64_t)start + count > mesh->vcount){
                LOGWITH("Invalid call: this mesh has",mesh->vcount,"vertices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = mesh->vcount - start;
            }
            vkCmdDraw(scb, count, instanceCount, start, istart);
        }
        bound = nullptr;
    }

    void GLMachine::RenderPass2Cube::execute(RenderPass* other){
        if(!recording){
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }

        reason = vkEndCommandBuffer(scb);
        if(reason != VK_SUCCESS){ // 호스트/GPU 메모리 부족 문제만 존재
            LOGWITH("Secondary command buffer begin failed:",reason, resultAsString(reason));
            return;
        }

        VkCommandBufferBeginInfo cbInfo{};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &cbInfo);
        if(reason != VK_SUCCESS){ // 호스트/GPU 메모리 부족 문제만 존재
            LOGWITH("Primary Command buffer begin failed:",reason,resultAsString(reason));
            return;
        }

        VkClearValue cvs[2]{};
        cvs[1].depthStencil.depth = 1.0f;
        VkRenderPassBeginInfo rpBeginInfo{};
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.clearValueCount = (colorTarget ? 1 : 0) + (depthTarget ? 1 : 0);
        rpBeginInfo.pClearValues = colorTarget ? cvs : &cvs[1];
        rpBeginInfo.renderPass = rp;
        rpBeginInfo.renderArea.extent.width = width;
        rpBeginInfo.renderArea.extent.height = height;
        rpBeginInfo.renderArea.offset = {};

        VkCommandBuffer ubNdraw[2] = {facewise[0], scb};
        
        for(int i=0;i<6;i++){
            rpBeginInfo.framebuffer = fbs[i];
            vkCmdBeginRenderPass(cb, &rpBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            ubNdraw[0] = facewise[i];
            vkCmdExecuteCommands(cb, 2, ubNdraw);
            vkCmdEndRenderPass(cb);
        }
        bound = nullptr;
        if((reason = vkEndCommandBuffer(scb)) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",reason);
            return;
        }
        
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cb;
        if(other){
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &other->semaphore;
            submitInfo.pWaitDstStageMask = waitStages;
        }
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &semaphore;

        if((reason = vkResetFences(singleton->device, 1, &fence)) != VK_SUCCESS){
            LOGWITH("Failed to reset fence. waiting or other operations will play incorrect");
            return;
        }

        if ((reason = singleton->qSubmit(true, 1, &submitInfo, fence)) != VK_SUCCESS) {
            LOGWITH("Failed to submit command buffer");
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
        vkResetCommandBuffer(cb, 0);
        vkResetCommandBuffer(scb, 0);
        VkCommandBufferInheritanceInfo ciInfo{};
        ciInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        ciInfo.renderPass = rp; // 공유..지만 다른 패스를 쓴대도 호환만 되면 명시 가능
        ciInfo.subpass = 0; // 무조건 실제와 일치해야 해서 서브패스에서 수행 불가능
        //ciInfo.framebuffer = nullptr; // : 여러 프레임버퍼에 대해 사용하기 때문에 명시할 수 없음

        VkCommandBufferBeginInfo cbInfo{};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        reason = vkBeginCommandBuffer(scb, &cbInfo);
        if(reason != VK_SUCCESS){
            recording = false;
            LOGWITH("Failed to begin secondary command buffer:",reason, resultAsString(reason));
            return;
        }
        
        vkCmdBindPipeline(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetViewport(scb, 0, 1, &viewport);
        vkCmdSetScissor(scb, 0, 1, &scissor);
    }

    void GLMachine::UniformBuffer::update(const void* input, uint32_t index, uint32_t offset, uint32_t size){
        glBindBuffer(GL_UNIFORM_BUFFER, ubo);
        glBufferSubData(GL_UNIFORM_BUFFER, offset, size, input);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void GLMachine::UniformBuffer::resize(uint32_t size) {    }

    GLMachine::UniformBuffer::~UniformBuffer(){
        glDeleteBuffers(1, &ubo);
    }


    // static함수들 구현

    int textureFormatFallback(uint32_t nChannels, bool srgb, bool hq); {
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
            return GL_SRG8;
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
            return GL_SR8;
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
        LOGWITH("Error",id,':',message,'(',severity,')');
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
                glVertexAttribPointer(index, dim, GL_FLOAT, GL_FALSE, stride, (void*)offset);
                break;
            case _vattr::t::F64:
                glVertexAttribPointer(index, dim, GL_DOUBLE, GL_FALSE, stride, (void*)offset);
                break;
            case _vattr::t::I8:
                glVertexAttribIPointer(index, dim, GL_BYTE, stride, (void*)offset);
                break;
            case _vattr::t::I16:
                glVertexAttribIPointer(index, dim, GL_SHORT, stride, (void*)offset);
                break;
            case _vattr::t::I32:
                glVertexAttribIPointer(index, dim, GL_INT, stride, (void*)offset);
                break;
            case _vattr::t::U8:
                glVertexAttribIPointer(index, dim, GL_UNSIGNED_BYTE, stride, (void*)offset);
                break;
            case _vattr::t::U16:
                glVertexAttribIPointer(index, dim, GL_UNSIGNED_SHORT, stride, (void*)offset);
                break;
            case _vattr::t::U32:
                glVertexAttribIPointer(index, dim, GL_UNSIGNED_INT, stride, (void*)offset);
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