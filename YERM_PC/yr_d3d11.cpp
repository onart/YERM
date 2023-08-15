#include "yr_d3d11.h"
#include "yr_sys.h"

#ifndef KHRONOS_STATIC
#define KHRONOS_STATIC
#endif
#include "../externals/ktx/include/ktx.h"
#include "../externals/single_header/stb_image.h"

#include <comdef.h>

#pragma comment(lib, "dxgi.lib")

namespace onart {
	D3D11Machine* D3D11Machine::singleton = nullptr;
	thread_local HRESULT D3D11Machine::reason = 0;

    constexpr uint64_t BC7_SCORE = 1LL << 53;

    static uint64_t assessAdapter(IDXGIAdapter*);
    static DXGI_FORMAT ktx2Format2DX(VkFormat);
    static ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, D3D11Machine::ImageTextureFormatOptions& option);
    static ktx_error_code_e tryTranscode(ktxTexture2* texture, ID3D11Device* device, uint32_t nChannels, bool srgb, bool hq);

    D3D11Machine::D3D11Machine(Window* window) {
        if (singleton) {
            LOGWITH("Tried to create multiple GLMachine objects");
            return;
        }

        constexpr D3D_FEATURE_LEVEL FEATURE_LEVELS[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL lv = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_1_0_CORE;

        IDXGIFactory* factory;
        CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&factory));
        IDXGIAdapter* selectedAdapter{};
        IDXGIAdapter* currentAdapter{};
        uint64_t score = 0;

        for (UINT i = 0; factory->EnumAdapters(i,&currentAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
            uint64_t thisScore = assessAdapter(currentAdapter);
            if (thisScore > score) {
                if (selectedAdapter) {
                    selectedAdapter->Release();
                }
                selectedAdapter = currentAdapter;
                score = thisScore;
            }
            else {
                currentAdapter->Release();
            }
        }
        
        HRESULT result = D3D11CreateDevice(
            selectedAdapter,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            USE_D3D11_DEBUG ? D3D11_CREATE_DEVICE_DEBUG : 0,
            FEATURE_LEVELS,
            sizeof(FEATURE_LEVELS) / sizeof(FEATURE_LEVELS[0]),
            D3D11_SDK_VERSION,
            &device,
            &lv,
            &context
        );

        if (score & BC7_SCORE) {
            canUseBC7 = true;
        }

        selectedAdapter->Release();

        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 device:", result);
            reason = result;
            return;
        }

        int x, y;
        window->getFramebufferSize(&x, &y);

        createSwapchain(x, y, window);
        if (!swapchain) {
            LOGWITH("Failed to create swapchain");
            reason = result;
            return;
        }

        singleton = this;
    }

    void D3D11Machine::createSwapchain(uint32_t width, uint32_t height, Window* window) {
        HRESULT result{};
        if (swapchain) {
            result = swapchain->ResizeBuffers(2, width, height, DXGI_FORMAT_UNKNOWN, 0);
            if (result != S_OK) {
                _com_error err(result);
                LOGWITH("Failed to resize swapchain:", result, err.ErrorMessage());
                swapchain->Release();
                swapchain = nullptr;
                reason = result;
                return;
            }
            return;
        }

        // https://learn.microsoft.com/ko-kr/windows/win32/api/dxgi/ns-dxgi-dxgi_swap_chain_desc?redirectedfrom=MSDN
        // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb173064(v=vs.85)
        IDXGIDevice* dxgiDevice{};
        IDXGIAdapter* dxgiAdapter{};
        IDXGIFactory* dxgiFactory{};
        
        result = device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
        if (!dxgiDevice) {
            _com_error err(result);
            LOGWITH("Failed to query dxgi device:", result, err.ErrorMessage());
            reason = result;
            return;
        }
        result = dxgiDevice->GetAdapter(&dxgiAdapter);
        if (!dxgiAdapter) {
            _com_error err(result);
            LOGWITH("Failed to query dxgi adapter:", result, err.ErrorMessage());
            reason = result;
            return;
        }
        result = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);
        if (!dxgiFactory) {
            _com_error err(result);
            LOGWITH("Failed to query dxgi factory:", result, err.ErrorMessage());
            reason = result;
            return;
        }

        DXGI_SWAP_CHAIN_DESC swapchainInfo{};
        swapchainInfo.BufferDesc.Width = width;
        swapchainInfo.BufferDesc.Height = height;
        swapchainInfo.Windowed = true;
        swapchainInfo.SampleDesc.Count = 1;
        swapchainInfo.SampleDesc.Quality = 0;
        swapchainInfo.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchainInfo.BufferCount = 2;
        swapchainInfo.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapchainInfo.OutputWindow = (HWND)window->getWin32Handle();
        //swapchainInfo.BufferDesc.ScanlineOrdering = 0;

        // HW ���� �ʿ�
        swapchainInfo.BufferDesc.RefreshRate.Numerator = window->getMonitorRefreshRate();;
        swapchainInfo.BufferDesc.RefreshRate.Denominator = 1;
        swapchainInfo.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        
        result = dxgiFactory->CreateSwapChain(device, &swapchainInfo, &swapchain);
        if (!swapchain) {
            _com_error err(result);
            LOGWITH("Failed to create swapchain:", result, err.ErrorMessage());
            reason = result;
            return;
        }
        
        dxgiFactory->Release();
        dxgiAdapter->Release();
        dxgiDevice->Release();
    }

    D3D11Machine::~D3D11Machine() {
        free();
    }

    void D3D11Machine::free() {
        for (auto& shader : shaders) {
            shader.second->Release();
        }
        shaders.clear();
        meshes.clear();
        textures.clear();
        swapchain->Release();
        device->Release();
        context->Release();
    }

    uint64_t assessAdapter(IDXGIAdapter* adapter) {
        uint64_t score = 0;
        DXGI_ADAPTER_DESC info;
        HRESULT result = adapter->GetDesc(&info);
        if (result != S_OK) {
            return 0;
        }
        if (info.DedicatedVideoMemory) {
            score |= 1LL << 63;
        }
        else {
            score |= 1LL << 62;
        }

        constexpr D3D_FEATURE_LEVEL FEATURE_LEVELS[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL lv = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_1_0_CORE;

        ID3D11Device* temp{};
        ID3D11DeviceContext* tempctx{};

        result = D3D11CreateDevice(
            adapter,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            FEATURE_LEVELS,
            sizeof(FEATURE_LEVELS) / sizeof(FEATURE_LEVELS[0]),
            D3D11_SDK_VERSION,
            &temp,
            &lv,
            &tempctx
        );

        if (result != S_OK) {
            return 0;
        }

        D3D11_FEATURE_DATA_FORMAT_SUPPORT formatSupport{};
        formatSupport.InFormat = DXGI_FORMAT_BC7_UNORM;
        constexpr UINT BITS_FOR_BC7 = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_TEXTURECUBE;
        if (temp->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)) == S_OK) {
            if ((formatSupport.OutFormatSupport & BITS_FOR_BC7) == BITS_FOR_BC7) {
                score |= BC7_SCORE;
            }
        }

        temp->Release();
        tempctx->Release();
        return score;
    }

    D3D11Machine::pMesh D3D11Machine::getMesh(int32_t key) {
        auto it = singleton->meshes.find(key);
        if (it != singleton->meshes.end()) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::UniformBuffer* D3D11Machine::getUniformBuffer(int32_t key) {
        auto it = singleton->uniformBuffers.find(key);
        if (it != singleton->uniformBuffers.find(key)) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::RenderTarget* D3D11Machine::getRenderTarget(int32_t key) {
        auto it = singleton->renderTargets.find(key);
        if (it != singleton->renderTargets.find(key)) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::RenderPass* D3D11Machine::getRenderPass(int32_t key) {
        auto it = singleton->renderPasses.find(key);
        if (it != singleton->renderPasses.find(key)) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::RenderPass2Cube* D3D11Machine::getRenderPass2Cube(int32_t key) {
        auto it = singleton->cubePasses.find(key);
        if (it != singleton->cubePasses.find(key)) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::pMesh D3D11Machine::createMesh(void* vdata, size_t vsize, size_t vcount, void* idata, size_t isize, size_t icount, int32_t key, bool stage) {
        pMesh ret = getMesh(key);
        if (ret) {
            return ret;
        }
        D3D11_BUFFER_DESC bufferInfo{};
        bufferInfo.ByteWidth = vsize * vcount;
        bufferInfo.Usage = stage ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
        bufferInfo.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferInfo.CPUAccessFlags = stage ? 0 : D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = vdata;

        ID3D11Buffer* vb{}, *ib{};

        HRESULT result = singleton->device->CreateBuffer(&bufferInfo, &vertexData, &vb);
        if (result != S_OK) {
            LOGWITH("Failed to create vertex buffer:", result);
            reason = result;
            return {};
        }

        if (idata) {
            bufferInfo.ByteWidth = isize * icount;
            bufferInfo.BindFlags = D3D11_BIND_INDEX_BUFFER;
            HRESULT result = singleton->device->CreateBuffer(&bufferInfo, &vertexData, &ib);
            if (result != S_OK) {
                LOGWITH("Failed to create index buffer:", result);
                vb->Release();
                reason = result;
                return {};
            }
        }

        struct publicmesh :public Mesh { publicmesh(ID3D11Buffer* _1, ID3D11Buffer* _2) :Mesh(_1,_2) {} };
        ret = std::make_shared<publicmesh>(vb, ib);
        return singleton->meshes[key] = ret;
    }

    ID3D11DeviceChild* D3D11Machine::getShader(int32_t key) {
        if (auto it = singleton->shaders.find(key); it != singleton->shaders.end()) {
            return it->second;
        }
        return {};
    }

    ID3D11DeviceChild* D3D11Machine::createShader(const char* code, size_t size, int32_t key, ShaderType type) {
        if (auto sh = getShader(key)) return sh;
        HRESULT result{};
        ID3D11DeviceChild* ret{};
        switch (type)
        {
        case onart::D3D11Machine::ShaderType::VERTEX:
            result = singleton->device->CreateVertexShader(code, size, nullptr, (ID3D11VertexShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderType::FRAGMENT:
            result = singleton->device->CreatePixelShader(code, size, nullptr, (ID3D11PixelShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderType::GEOMETRY:
            result = singleton->device->CreateGeometryShader(code, size, nullptr, (ID3D11GeometryShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderType::TESS_CTRL:
            result = singleton->device->CreateHullShader(code, size, nullptr, (ID3D11HullShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderType::TESS_EVAL:
            result = singleton->device->CreateDomainShader(code, size, nullptr, (ID3D11DomainShader**)&ret);
            break;
        default:
            return {};
        }
        if (result != S_OK) {
            LOGWITH("Failed to create shader instance:",result);
            reason = result;
            return {};
        }
        return singleton->shaders[key] = ret;
    }

    ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, D3D11Machine::ImageTextureFormatOptions& option) {
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
        if ((k2result = ktxTexture2_Create(&texInfo, ktxTextureCreateStorageEnum::KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture)) != KTX_SUCCESS) {
            LOGWITH("Failed to create texture:", k2result);
            return nullptr;
        }
        if ((k2result = ktxTexture_SetImageFromMemory(ktxTexture(texture), 0, 0, 0, pix, x * y * nChannels)) != KTX_SUCCESS) {
            LOGWITH("Failed to set texture image data:", k2result);
            ktxTexture_Destroy(ktxTexture(texture));
            return nullptr;
        }
        if (option == D3D11Machine::ImageTextureFormatOptions::IT_USE_HQCOMPRESS || option == D3D11Machine::ImageTextureFormatOptions::IT_USE_COMPRESS) {
            ktxBasisParams params{};
            params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
            params.uastc = KTX_TRUE;
            params.verbose = KTX_FALSE;
            params.structSize = sizeof(params);

            k2result = ktxTexture2_CompressBasisEx(texture, &params);
            if (k2result != KTX_SUCCESS) {
                LOGWITH("Compress failed:", k2result);
                option = D3D11Machine::ImageTextureFormatOptions::IT_USE_ORIGINAL;
            }
        }
        return texture;
    }

    ktx_error_code_e tryTranscode(ktxTexture2* texture, ID3D11Device* device, uint32_t nChannels, bool srgb, bool hq) {
        if (ktxTexture2_NeedsTranscoding(texture)) {
            ktx_transcode_fmt_e tf;
            switch (textureFormatFallback(device, nChannels, srgb, hq, texture->isCubemap ? D3D11_FORMAT_SUPPORT_TEXTURECUBE : D3D11_FORMAT_SUPPORT_TEXTURE2D ))
            {
            case DXGI_FORMAT_BC7_UNORM_SRGB:
            case DXGI_FORMAT_BC7_UNORM:
                tf = KTX_TTF_BC7_RGBA;
                break;
            case DXGI_FORMAT_BC3_UNORM_SRGB:
            case DXGI_FORMAT_BC3_UNORM:
                tf = KTX_TTF_BC3_RGBA;
                break;
            case DXGI_FORMAT_BC1_UNORM_SRGB:
            case DXGI_FORMAT_BC1_UNORM:
                tf = KTX_TTF_BC1_RGB;
                break;
            case DXGI_FORMAT_BC4_UNORM:
                tf = KTX_TTF_BC4_R;
                break;
            case DXGI_FORMAT_BC5_UNORM:
                tf = KTX_TTF_BC5_RG;
                break;
            default:
                tf = KTX_TTF_RGBA32;
                break;
            }
            return ktxTexture2_TranscodeBasis(texture, tf, 0);
        }
        return KTX_SUCCESS;
    }

    D3D11Machine::pTexture D3D11Machine::getTexture(int32_t key, bool lock) {
        if (lock) {
            std::unique_lock<std::mutex> _(singleton->textureGuard);
            auto it = singleton->textures.find(key);
            if (it != singleton->textures.end()) return it->second;
            else return pTexture();
        }
        else {
            auto it = singleton->textures.find(key);
            if (it != singleton->textures.end()) return it->second;
            else return pTexture();
        }
    }

    void D3D11Machine::asyncCreateTexture(const uint8_t* mem, size_t size, uint32_t nChannels, std::function<void(variant8)> handler, int32_t key, bool srgb, bool hq, bool linearSampler) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([mem, size, key, nChannels, handler, srgb, hq, already, linearSampler]() {
            if (!already) {
                pTexture ret = singleton->createTexture(mem, size, nChannels, INT32_MIN, srgb, hq, linearSampler);
                if (!ret) {
                    variant8 _k = (uint32_t)key | ((uint64_t)D3D11Machine::reason << 32);
                    return _k;
                }
                singleton->textureGuard.lock();
                singleton->textures[key] = std::move(ret);
                singleton->textureGuard.unlock();
            }
            return variant8((uint64_t)(uint32_t)key);
            }, handler, vkm_strand::GENERAL);
    }

    void D3D11Machine::asyncCreateTextureFromImage(const char* fileName, int32_t key, std::function<void(variant8)> handler, bool srgb, ImageTextureFormatOptions option, bool linearSampler) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([already, fileName, key, handler, srgb, option, linearSampler]() {
            if (!already) {
                pTexture ret = singleton->createTextureFromImage(fileName, INT32_MIN, srgb, option, linearSampler);
                if (!ret) {
                    variant8 _k = (uint32_t)key | ((uint64_t)D3D11Machine::reason << 32);
                    return _k;
                }
                singleton->textureGuard.lock();
                singleton->textures[key] = std::move(ret);
                singleton->textureGuard.unlock();
            }
            return variant8((uint64_t)(uint32_t)key);
            }, handler, vkm_strand::GENERAL);
    }

    void D3D11Machine::asyncCreateTextureFromImage(const uint8_t* mem, size_t size, int32_t key, std::function<void(variant8)> handler, bool srgb, ImageTextureFormatOptions option, bool linearSampler) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([already, mem, size, key, handler, srgb, option, linearSampler]() {
            if (!already) {
                pTexture ret = singleton->createTextureFromImage(mem, size, INT32_MIN, srgb, option, linearSampler);
                if (!ret) {
                    variant8 _k = (uint32_t)key | ((uint64_t)D3D11Machine::reason << 32);
                    return _k;
                }
                singleton->textureGuard.lock();
                singleton->textures[key] = std::move(ret);
                singleton->textureGuard.unlock();
            }
            return variant8((uint64_t)(uint32_t)key);
            }, handler, vkm_strand::GENERAL);
    }

    void D3D11Machine::asyncCreateTexture(const char* fileName, int32_t key, uint32_t nChannels, std::function<void(variant8)> handler, bool srgb, bool hq, bool linearSampler) {
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([fileName, key, nChannels, handler, srgb, hq, already, linearSampler]() {
            if (!already) {
                pTexture ret = singleton->createTexture(fileName, INT32_MIN, nChannels, srgb, hq, linearSampler);
                if (!ret) {
                    variant8 _k = (uint32_t)key | ((uint64_t)D3D11Machine::reason << 32);
                    return _k;
                }
                singleton->textureGuard.lock();
                singleton->textures[key] = std::move(ret);
                singleton->textureGuard.unlock();
            }
            return variant8((uint64_t)(uint32_t)key);
            }, handler, vkm_strand::GENERAL);
    }

    D3D11Machine::pTexture D3D11Machine::createTextureFromImage(const uint8_t* mem, size_t size, int32_t key, bool srgb, ImageTextureFormatOptions option, bool linearSampler) {
        if (auto ret = getTexture(key)) { return ret; }
        int x, y, nChannels;
        uint8_t* pix = stbi_load_from_memory(mem, size, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return pTexture();
        }
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, 4, srgb, option);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, 4, srgb, option != ImageTextureFormatOptions::IT_USE_COMPRESS, linearSampler);
    }

    D3D11Machine::pTexture D3D11Machine::createTextureFromImage(const char* fileName, int32_t key, bool srgb, ImageTextureFormatOptions option, bool linearSampler) {
        if (auto ret = getTexture(key)) { return ret; }
        int x, y, nChannels;
        uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return pTexture();
        }
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, 4, srgb, option);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, 4, srgb, option != ImageTextureFormatOptions::IT_USE_COMPRESS, linearSampler);
    }

    D3D11Machine::pTexture D3D11Machine::createTexture(const char* fileName, int32_t key, uint32_t nChannels, bool srgb, bool hq, bool linearSampler) {
        if (auto ret = getTexture(key)) { return ret; }
        if (nChannels > 4 || nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }

        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if ((k2result = ktxTexture2_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
            LOGWITH("Failed to load ktx texture:", k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, nChannels, srgb, hq, linearSampler);
    }

    D3D11Machine::pTexture D3D11Machine::createTexture(const uint8_t* mem, size_t size, uint32_t nChannels, int32_t key, bool srgb, bool hq, bool linearSampler) {
        if (auto ret = getTexture(key)) { return ret; }
        if (nChannels > 4 || nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }

        ktxTexture2* texture;
        ktx_error_code_e k2result;
        if ((k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
            LOGWITH("Failed to load ktx texture:", k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, nChannels, srgb, hq, linearSampler);
    }

    D3D11Machine::pTexture D3D11Machine::createTexture(void* ktxObj, int32_t key, uint32_t nChannels, bool srgb, bool hq, bool linearSampler) {
        ktxTexture2* texture = reinterpret_cast<ktxTexture2*>(ktxObj);
        if (texture->numLevels == 0) return pTexture();
        ktx_error_code_e k2result = tryTranscode(texture, device, nChannels, srgb, hq);
        if (k2result != KTX_SUCCESS) {
            LOGWITH("Failed to transcode ktx texture:", k2result);
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
        
        D3D11_TEXTURE2D_DESC info{};
        info.Width = texture->baseWidth;
        info.Height = texture->baseHeight;
        info.MipLevels = texture->numLevels;
        info.ArraySize = texture->numFaces * texture->numLayers;
        info.Format = textureFormatFallback(device, nChannels, srgb, hq, texture->isCubemap ? D3D11_FORMAT_SUPPORT_TEXTURECUBE : D3D11_FORMAT_SUPPORT_TEXTURE2D);
        info.Usage = D3D11_USAGE_IMMUTABLE;
        info.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        info.SampleDesc.Count = 1;
        info.SampleDesc.Quality = 0;
        info.MiscFlags = texture->isCubemap ? D3D11_RESOURCE_MISC_FLAG::D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

        D3D11_SUBRESOURCE_DATA data{};
        data.pSysMem = texture->pData;
        data.SysMemPitch = ktxTexture_GetRowPitch(ktxTexture(texture), 0);
        ID3D11Texture2D* newTex{};
        ID3D11ShaderResourceView* srv{};
        HRESULT result = singleton->device->CreateTexture2D(&info, &data, &newTex);
        
        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 texture:", result);
            ktxTexture_Destroy(ktxTexture(texture));
            reason = result;
            return pTexture();
        }
        uint16_t width = texture->baseWidth, height = texture->baseHeight;
        ktxTexture_Destroy(ktxTexture(texture));

        D3D11_SHADER_RESOURCE_VIEW_DESC descInfo{};
        descInfo.Format = info.Format;
        descInfo.ViewDimension = texture->isCubemap ? D3D11_SRV_DIMENSION_TEXTURECUBE : D3D11_SRV_DIMENSION_TEXTURE2D;
        descInfo.Texture2D.MipLevels = info.MipLevels;
        result = singleton->device->CreateShaderResourceView(newTex, &descInfo, &srv);
        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 shader resource view:", result);
            newTex->Release();
            reason = result;
            return pTexture();
        }
        struct txtr :public Texture { inline txtr(ID3D11Resource* _1, ID3D11ShaderResourceView* _2, uint16_t _3, uint16_t _4, bool _5, bool _6) :Texture(_1, _2, _3, _4, _5, _6) {} };
        if (key == INT32_MIN) return std::make_shared<txtr>(texture, srv, width, height, texture->isCubemap, linearSampler);
        return textures[key] = std::make_shared<txtr>(texture, srv, width, height, texture->isCubemap, linearSampler);
    }

    D3D11Machine::ImageSet::~ImageSet() {
        if (srv) { srv->Release(); }
        if (tex) { tex->Release(); }
    }

    D3D11Machine::RenderTarget* D3D11Machine::createRenderTarget2D(int width, int height, int32_t key, RenderTargetType type, RenderTargetInputOption sampled, bool useDepthInput, bool useStencil, bool mmap) {
        auto it = singleton->renderTargets.find(key);
        if (it != singleton->renderTargets.end()) {
            return it->second;
        }

        D3D11_TEXTURE2D_DESC textureInfo{};
        textureInfo.Width = width;
        textureInfo.Height = height;
        textureInfo.MipLevels = 1;
        textureInfo.ArraySize = 1;
        textureInfo.SampleDesc.Count = 1;
        textureInfo.SampleDesc.Quality = 0;
        textureInfo.Usage = mmap ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;
        textureInfo.CPUAccessFlags = mmap ? D3D11_CPU_ACCESS_FLAG::D3D11_CPU_ACCESS_READ : 0;

        textureInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: SRGB?
        textureInfo.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET | D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;

        D3D11_RENDER_TARGET_VIEW_DESC targetInfo{};
        targetInfo.Format = textureInfo.Format;
        targetInfo.ViewDimension = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D;
        targetInfo.Texture2D.MipSlice = 0;

        ImageSet* color1{}, *color2{}, *color3{}, *ds{};
        ID3D11RenderTargetView* rtv1{}, * rtv2{}, * rtv3{}, * rtvds{};

        D3D11_SHADER_RESOURCE_VIEW_DESC descInfo{};
        descInfo.Format = textureInfo.Format;
        descInfo.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        descInfo.Texture2D.MipLevels = 1;

        if ((int)type & 0b1) {
            color1 = new ImageSet{};
            HRESULT result = singleton->device->CreateTexture2D(&textureInfo, nullptr, (ID3D11Texture2D**)&color1->tex);
            if (result != S_OK) {
                LOGWITH("Failed to create color target:", result);
                reason = result;
                delete color1;
                return {};
            }
            result = singleton->device->CreateShaderResourceView(color1->tex, &descInfo, &color1->srv);
            if (result != S_OK) {
                LOGWITH("Failed to create color target shader resoruce view:", result);
                reason = result;
                delete color1;
                return {};
            }
            result = singleton->device->CreateRenderTargetView(color1->tex, &targetInfo, &rtv1);
            if (result != S_OK) {
                LOGWITH("Failed to create color target render target view:", result);
                reason = result;
                delete color1;
                return {};
            }
            if ((int)type & 0b10) {
                color2 = new ImageSet{};
                result = singleton->device->CreateTexture2D(&textureInfo, nullptr, (ID3D11Texture2D**)&color2->tex);
                if (result != S_OK) {
                    LOGWITH("Failed to create color target:", result);
                    reason = result;
                    delete color1;
                    delete color2;
                    rtv1->Release();
                    return {};
                }
                result = singleton->device->CreateShaderResourceView(color2->tex, &descInfo, &color2->srv);
                if (result != S_OK) {
                    LOGWITH("Failed to create color target shader resoruce view:", result);
                    reason = result;
                    delete color1;
                    delete color2;
                    rtv1->Release();
                    return {};
                }
                result = singleton->device->CreateRenderTargetView(color2->tex, &targetInfo, &rtv2);
                if (result != S_OK) {
                    LOGWITH("Failed to create color target render target view:", result);
                    reason = result;
                    delete color1;
                    delete color2;
                    rtv1->Release();
                    return {};
                }
                if ((int)type & 0b100) {
                    color3 = new ImageSet{};
                    result = singleton->device->CreateTexture2D(&textureInfo, nullptr, (ID3D11Texture2D**)&color3->tex);
                    if (result != S_OK) {
                        LOGWITH("Failed to create color target:", result);
                        reason = result;
                        delete color1;
                        delete color2;
                        delete color3;
                        rtv1->Release();
                        rtv2->Release();
                        return {};
                    }
                    result = singleton->device->CreateShaderResourceView(color3->tex, &descInfo, &color3->srv);
                    if (result != S_OK) {
                        LOGWITH("Failed to create color target shader resoruce view:", result);
                        reason = result;
                        delete color1;
                        delete color2;
                        delete color3;
                        rtv1->Release();
                        rtv2->Release();
                        return {};
                    }
                    result = singleton->device->CreateRenderTargetView(color3->tex, &targetInfo, &rtv3);
                    if (result != S_OK) {
                        LOGWITH("Failed to create color target render target view:", result);
                        reason = result;
                        delete color1;
                        delete color2;
                        delete color3;
                        rtv1->Release();
                        rtv2->Release();
                        return {};
                    }
                }
            }
        }
        if ((int)type & 0b1000) {
            ds = new ImageSet{};
            textureInfo.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            if (!useDepthInput) {
                textureInfo.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET;
            }
            HRESULT result = singleton->device->CreateTexture2D(&textureInfo, nullptr, (ID3D11Texture2D**)&ds->tex);
            if (result != S_OK) {
                LOGWITH("Failed to create depth-stencil texture:", result);
                delete color1;
                delete color2;
                delete color3;
                delete ds;
                if (rtv1) rtv1->Release();
                if (rtv2) rtv2->Release();
                if (rtv3) rtv3->Release();
                return {};
            }
            result = singleton->device->CreateShaderResourceView(ds->tex, &descInfo, &ds->srv);
            if (result != S_OK) {
                LOGWITH("Failed to create color target shader resoruce view:", result);
                reason = result;
                delete color1;
                delete color2;
                delete color3;
                delete ds;
                if (rtv1) rtv1->Release();
                if (rtv2) rtv2->Release();
                if (rtv3) rtv3->Release();
                return {};
            }
            result = singleton->device->CreateRenderTargetView(ds->tex, &targetInfo, &rtvds);
            if (result != S_OK) {
                LOGWITH("Failed to create color target render target view:", result);
                reason = result;
                delete color1;
                delete color2;
                delete color3;
                if (rtv1) rtv1->Release();
                if (rtv2) rtv2->Release();
                if (rtv3) rtv3->Release();
                return {};
            }
        }
        ImageSet* params[] = {color1, color2, color3, ds};
        ID3D11RenderTargetView* params2[] = {rtv1, rtv2, rtv3, rtvds};
        if (key == INT32_MIN) return new RenderTarget(type, width, height, params, params2, mmap);
        return singleton->renderTargets[key] = new RenderTarget(type, width, height, params, params2, mmap);
    }


    void D3D11Machine::handle() {
        singleton->loadThread.handleCompleted();
    }

    mat4 D3D11Machine::preTransform() {
        return mat4();
    }

    D3D11Machine::UniformBuffer::UniformBuffer(uint32_t length, ID3D11Buffer* ubo, uint32_t binding)
        :length(length), ubo(ubo), binding(binding) {

    }

    D3D11Machine::UniformBuffer::~UniformBuffer() {
        ubo->Release();
    }

    void D3D11Machine::UniformBuffer::update(const void* input, uint32_t index, uint32_t offset, uint32_t size) {
        if (offset + size > length) {
            LOGWITH("Requested buffer update range is invalid");
            return;
        }
        if (size == length) {
            singleton->context->UpdateSubresource(ubo, 0, nullptr, input, 0, 0);
        }
        else { // �Ϲ����� ��� ���ϴ��, ���� �������� �ʵ��� ��
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            if (singleton->context->Map(ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource) == S_OK)
            {
                std::memcpy((uint8_t*)mappedResource.pData + offset, input, size);
                singleton->context->Unmap(ubo, 0);
            }
            else {
                LOGWITH("Failed to map memory");
                return;
            }
        }
    }

    D3D11Machine::UniformBuffer* D3D11Machine::createUniformBuffer(uint32_t length, uint32_t size, size_t stages, int32_t key, uint32_t binding = 0) {
        if (auto ret = getUniformBuffer(key)) { return ret; }
        ID3D11Buffer* buffer{};
        D3D11_BUFFER_DESC bufferInfo{};
        bufferInfo.ByteWidth = size;
        bufferInfo.Usage = D3D11_USAGE_DYNAMIC;
        bufferInfo.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferInfo.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        HRESULT result = singleton->device->CreateBuffer(&bufferInfo, nullptr, &buffer);
        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 buffer:", result);
            reason = result;
            return {};
        }

        if (key == INT32_MIN) return new UniformBuffer(size, buffer, binding);
        return singleton->uniformBuffers[key] = new UniformBuffer(size, buffer, binding);
    }

    D3D11Machine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, ImageSet** sets, ID3D11RenderTargetView** rtvs, bool mmap)
        :width(width), height(height), mapped(mmap), type(type), color1(sets[0]), color2(sets[1]), color3(sets[2]), ds(sets[3]), dset1(rtvs[0]), dset2(rtvs[1]), dset3(rtvs[2]), dsetDS(rtvs[3]) {

    }

    D3D11Machine::RenderTarget::~RenderTarget() {
        if (dset1) dset1->Release();
        if (dset2) dset2->Release();
        if (dset3) dset3->Release();
        if (dsetDS) dsetDS->Release();
        delete color1;
        delete color2;
        delete color3;
        delete ds;
    }

    D3D11Machine::Texture::Texture(ID3D11Resource* texture, ID3D11ShaderResourceView* dset, uint16_t width, uint16_t height, bool isCubemap, bool linearSampled)
        :texture(texture), dset(dset), width(width), height(height), isCubemap(isCubemap), linearSampled(linearSampled) {
    }

    D3D11Machine::Texture::~Texture() {
        dset->Release();
        texture->Release();
    }

    D3D11Machine::Mesh::Mesh(ID3D11Buffer* vb, ID3D11Buffer* ib) :vb(vb), ib(ib), layout{} {}

    D3D11Machine::Mesh::~Mesh() {
        if(vb) vb->Release();
        if (ib) ib->Release();
        if (layout) layout->Release();
    }

    DXGI_FORMAT ktx2Format2DX(VkFormat fmt) {
        switch (fmt)
        {
        case VK_FORMAT_BC7_UNORM_BLOCK: return DXGI_FORMAT_BC7_UNORM;
        case VK_FORMAT_BC3_UNORM_BLOCK: return DXGI_FORMAT_BC3_UNORM;
        default: return DXGI_FORMAT::DXGI_FORMAT_FORCE_UINT;
        }
    }

    bool isThisFormatAvailable(ID3D11Device* device, DXGI_FORMAT format, UINT flags) {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT formatSupport{};
        formatSupport.InFormat = format;
        if (device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)) == S_OK) {
            return (formatSupport.OutFormatSupport & flags) == flags;
        }
        return false;
    }

    DXGI_FORMAT textureFormatFallback(ID3D11Device* device, uint32_t nChannels, bool srgb, bool hq, UINT flags) {
#define CHECK_N_RETURN(f) if(isThisFormatAvailable(device,f,flags)) return f
        switch (nChannels)
        {
        case 4:
            if (srgb) {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                if (hq) return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                CHECK_N_RETURN(DXGI_FORMAT_BC3_UNORM_SRGB);
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }
            else {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM);
                if (hq) return DXGI_FORMAT_R8G8B8A8_UNORM;
                CHECK_N_RETURN(DXGI_FORMAT_BC3_UNORM);
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            break;
        case 3:
            if (srgb) {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                if (hq) return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                CHECK_N_RETURN(DXGI_FORMAT_BC1_UNORM_SRGB);
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }
            else {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM);
                if (hq) return DXGI_FORMAT_R8G8B8A8_UNORM;
                CHECK_N_RETURN(DXGI_FORMAT_BC1_UNORM);
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
        case 2:
            if (srgb) {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                return DXGI_FORMAT_R8G8_UNORM; // SRGB �Ⱥ���..
            }
            else {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM);
                if (hq) return DXGI_FORMAT_R8G8_UNORM;
                CHECK_N_RETURN(DXGI_FORMAT_BC5_UNORM);
                return DXGI_FORMAT_R8G8_UNORM;
            }
        case 1:
            if (srgb) {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                return DXGI_FORMAT_R8_UNORM;
            }
            else {
                CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                if (hq) return DXGI_FORMAT_R8_UNORM;
                CHECK_N_RETURN(DXGI_FORMAT_BC4_UNORM);
                return DXGI_FORMAT_R8_UNORM;
            }
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
#undef CHECK_N_RETURN
    }
}