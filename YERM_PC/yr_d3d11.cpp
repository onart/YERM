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

        // HW 조사 필요
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
        info.SampleDesc.Quality = 1;
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

    void D3D11Machine::handle() {
        singleton->loadThread.handleCompleted();
    }

    mat4 D3D11Machine::preTransform() {
        return mat4();
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
                return DXGI_FORMAT_R8G8_UNORM; // SRGB 안보임..
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