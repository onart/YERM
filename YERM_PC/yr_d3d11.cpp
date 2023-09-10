#include "yr_d3d11.h"
#include "yr_sys.h"

#ifndef KHRONOS_STATIC
#define KHRONOS_STATIC
#endif
#include "../externals/ktx/include/ktx.h"
#include "../externals/single_header/stb_image.h"

#include <comdef.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace onart {
	D3D11Machine* D3D11Machine::singleton = nullptr;
    uint64_t D3D11Machine::currentRenderPass = 0;
    const D3D11Machine::Mesh* D3D11Machine::RenderPass::bound = nullptr;

	thread_local HRESULT D3D11Machine::reason = 0;

    constexpr uint64_t BC7_SCORE = 1LL << 53;

    static uint64_t assessAdapter(IDXGIAdapter*);
    static ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, D3D11Machine::ImageTextureFormatOptions& option);
    static ktx_error_code_e tryTranscode(ktxTexture2* texture, ID3D11Device* device, uint32_t nChannels, bool srgb, bool hq);
    static DXGI_FORMAT textureFormatFallback(ID3D11Device* device, uint32_t nChannels, bool srgb, bool hq, UINT flags);

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
        if (!swapchain.handle) {
            LOGWITH("Failed to create swapchain");
            reason = result;
            return;
        }

        D3D11_BLEND_DESC blendInfo{};
        blendInfo.RenderTarget[0].BlendEnable = true;
        blendInfo.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendInfo.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendInfo.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendInfo.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendInfo.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendInfo.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        device->CreateBlendState(&blendInfo, &basicBlend);

        D3D11_SAMPLER_DESC samplerInfo{};
        samplerInfo.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerInfo.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerInfo.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerInfo.MaxAnisotropy = 1;
        samplerInfo.MaxLOD = D3D11_FLOAT32_MAX;

        samplerInfo.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        device->CreateSamplerState(&samplerInfo, &linearBorderSampler);

        samplerInfo.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
        device->CreateSamplerState(&samplerInfo, &nearestBorderSampler);

        createUniformBuffer(1, 128, 0, INT32_MIN + 1);

        singleton = this;
    }

    void D3D11Machine::createSwapchain(uint32_t width, uint32_t height, Window* window) {
        HRESULT result{};
        if (swapchain.handle) {
            for (auto& buf : screenTargets) {
                buf.first->Release();
                buf.second->Release();
            }
            screenTargets.clear();

            if (screenDSView) {
                screenDSView->Release();
                screenDSView = nullptr;
            }
            result = swapchain.handle->ResizeBuffers(2, width, height, DXGI_FORMAT_UNKNOWN, 0);
            if (result != S_OK) {
                _com_error err(result);
                LOGWITH("Failed to resize swapchain:", result, err.ErrorMessage());
                swapchain.handle->Release();
                swapchain.handle = nullptr;
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
        swapchainInfo.BufferCount = 1; // TODO: 트리플버퍼링 하려면 seqencial_flip으로 하고 여기 2로 조절, 스레드 따로
        swapchainInfo.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // TODO: 트리플버퍼링 하려면 seqencial_flip으로 하고 기타 조절
        swapchainInfo.OutputWindow = (HWND)window->getWin32Handle();
        //swapchainInfo.BufferDesc.ScanlineOrdering = 0;

        // HW 조사 필요
        swapchainInfo.BufferDesc.RefreshRate.Numerator = window->getMonitorRefreshRate();;
        swapchainInfo.BufferDesc.RefreshRate.Denominator = 1;
        swapchainInfo.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        
        result = dxgiFactory->CreateSwapChain(device, &swapchainInfo, &swapchain.handle);
        if (!swapchain.handle) {
            _com_error err(result);
            LOGWITH("Failed to create swapchain:", result, err.ErrorMessage());
            reason = result;
            dxgiFactory->Release();
            dxgiAdapter->Release();
            dxgiDevice->Release();
            return;
        }
        
        dxgiFactory->Release();
        dxgiAdapter->Release();
        dxgiDevice->Release();

        ID3D11Texture2D* dsTex{};

        D3D11_TEXTURE2D_DESC dsInfo{};
        dsInfo.Width = width;
        dsInfo.Height = height;
        dsInfo.MipLevels = 1;
        dsInfo.ArraySize = 1;
        dsInfo.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsInfo.Usage = D3D11_USAGE_DEFAULT;
        dsInfo.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        dsInfo.SampleDesc.Count = 1;
        dsInfo.SampleDesc.Quality = 0;
        result = device->CreateTexture2D(&dsInfo, nullptr, &dsTex);
        if (result != S_OK) {
            LOGWITH("Failed to create screen target depth stencil buffer:", result);
            reason = result;
            return;
        }
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvInfo{};
        dsvInfo.Format = dsInfo.Format;
        dsvInfo.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvInfo.Texture2D.MipSlice = 0;
        result = device->CreateDepthStencilView(dsTex, &dsvInfo, &screenDSView);
        if (result != S_OK) {
            LOGWITH("Failed to create screen target depth stencil buffer view:", result);
            reason = result;
            dsTex->Release();
            return;
        }
        
        dsTex->Release();
        swapchain.width = width;
        swapchain.height = height;
    }

    D3D11Machine::~D3D11Machine() {
        free();
    }

    void D3D11Machine::free() {
        basicBlend->Release();
        linearBorderSampler->Release();
        nearestBorderSampler->Release();
        for (auto& shader : shaders) {
            shader.second->Release();
        }
        shaders.clear();
        meshes.clear();
        textures.clear();

        for (auto& buf : screenTargets) {
            buf.first->Release();
            buf.second->Release();
        }
        screenTargets.clear();

        if (screenDSView) screenDSView->Release();
        if (swapchain.handle) swapchain.handle->Release();
        device->Release();
        context->Release();
    }

    ID3D11RenderTargetView* D3D11Machine::getSwapchainTarget() {
        ID3D11Texture2D* pBackBuffer = nullptr;
        HRESULT result = swapchain.handle->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
        if (result != S_OK) {
            LOGWITH("Failed to get swap chain buffer image:", result);
            return {};
        }
        auto it = screenTargets.find(pBackBuffer);
        if (it != screenTargets.end()) {
            pBackBuffer->Release();
            return it->second;
        }
        else {
            D3D11_RENDER_TARGET_VIEW_DESC rtvInfo{};
            D3D11_TEXTURE2D_DESC txInfo{};
            pBackBuffer->GetDesc(&txInfo);
            rtvInfo.Format = txInfo.Format;
            rtvInfo.Texture2D.MipSlice = 0;
            rtvInfo.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            ID3D11RenderTargetView* rtv{};
            result = device->CreateRenderTargetView(pBackBuffer, &rtvInfo, &rtv);
            if (result != S_OK) {
                LOGWITH("Failed to create swapchain render target view:", result);
                pBackBuffer->Release();
                return {};
            }
            return screenTargets[pBackBuffer] = rtv;
        }
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

    void D3D11Machine::UniformBuffer::updatePush(const void* input, uint32_t offset, uint32_t size) {
        singleton->uniformBuffers[INT32_MIN + 1]->update(input, 0, offset, size);
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

    D3D11Machine::pMesh D3D11Machine::createNullMesh(size_t vcount, int32_t key) {
        pMesh ret = getMesh(key);
        if (ret) {
            return ret;
        }
        D3D11_BUFFER_DESC bufferInfo{};
        bufferInfo.ByteWidth = vcount;
        bufferInfo.Usage = D3D11_USAGE_IMMUTABLE;
        bufferInfo.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferInfo.CPUAccessFlags = 0;

        ID3D11Buffer* vb{};
        DXGI_FORMAT iFormat{};

        HRESULT result = singleton->device->CreateBuffer(&bufferInfo, nullptr, &vb);
        if (result != S_OK) {
            LOGWITH("Failed to create vertex buffer:", result);
            reason = result;
            return {};
        }

        struct publicmesh :public Mesh { publicmesh(ID3D11Buffer* _1, ID3D11Buffer* _2, DXGI_FORMAT _3, size_t _4, size_t _5) :Mesh(_1, _2, _3, _4, _5) {} };
        ret = std::make_shared<publicmesh>(vb, nullptr, iFormat, vcount, 0);
        return singleton->meshes[key] = ret;
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
        DXGI_FORMAT iFormat{};

        HRESULT result = singleton->device->CreateBuffer(&bufferInfo, &vertexData, &vb);
        if (result != S_OK) {
            LOGWITH("Failed to create vertex buffer:", result);
            reason = result;
            return {};
        }

        if (idata) {
            bufferInfo.ByteWidth = isize * icount;
            bufferInfo.BindFlags = D3D11_BIND_INDEX_BUFFER;
            if (isize == 2) {
                iFormat = DXGI_FORMAT_R16_UINT;
            }
            else if (isize == 4) {
                iFormat = DXGI_FORMAT_R32_UINT;
            }
            else {
                LOGWITH("Warning: index buffer size is not 2 nor 4");
            }
            HRESULT result = singleton->device->CreateBuffer(&bufferInfo, &vertexData, &ib);
            if (result != S_OK) {
                LOGWITH("Failed to create index buffer:", result);
                vb->Release();
                reason = result;
                return {};
            }
        }

        struct publicmesh :public Mesh { publicmesh(ID3D11Buffer* _1, ID3D11Buffer* _2, DXGI_FORMAT _3, size_t _4, size_t _5) :Mesh(_1,_2,_3,_4,_5) {} };
        ret = std::make_shared<publicmesh>(vb, ib, iFormat, vcount, icount);
        return singleton->meshes[key] = ret;
    }

    ID3D11DeviceChild* D3D11Machine::getShader(int32_t key) {
        if (auto it = singleton->shaders.find(key); it != singleton->shaders.end()) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::Pipeline* D3D11Machine::getPipeline(int32_t key) {
        if (auto it = singleton->pipelines.find(key); it != singleton->pipelines.end()) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::RenderPass2Screen* D3D11Machine::getRenderPass2Screen(int32_t key) {
        if (auto it = singleton->finalPasses.find(key); it != singleton->finalPasses.end()) {
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
        if (key == INT32_MIN) return std::make_shared<txtr>(newTex, srv, width, height, texture->isCubemap, linearSampler);
        return textures[key] = std::make_shared<txtr>(newTex, srv, width, height, texture->isCubemap, linearSampler);
    }

    D3D11Machine::pStreamTexture D3D11Machine::createStreamTexture(uint32_t width, uint32_t height, int32_t key, bool linearSampler) {
        D3D11_TEXTURE2D_DESC info{};
        info.Width = width;
        info.Height = height;
        info.MipLevels = 1;
        info.ArraySize = 1;
        info.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        info.Usage = D3D11_USAGE_DYNAMIC;
        info.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        info.SampleDesc.Count = 1;
        info.SampleDesc.Quality = 0;

        ID3D11Texture2D* newTex{};
        ID3D11ShaderResourceView* srv{};
        HRESULT result = singleton->device->CreateTexture2D(&info, nullptr, &newTex);
        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 texture:", result);
            reason = result;
            return {};
        }

        D3D11_MAPPED_SUBRESOURCE mapInfo;
        result = singleton->context->Map(newTex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapInfo);
        if (result != S_OK) {
            LOGWITH("Failed to map memory", result);
            memset(&mapInfo, 0, sizeof(mapInfo));
        }

        struct txtr :public StreamTexture { inline txtr(ID3D11Texture2D* _1, ID3D11ShaderResourceView* _2, uint16_t _3, uint16_t _4, bool _5, void* _6, uint64_t _7) :StreamTexture(_1, _2, _3, _4, _5, _6, _7) {} };
        if (key == INT32_MIN) return std::make_shared<txtr>(newTex, srv, width, height, linearSampler, mapInfo.pData, mapInfo.RowPitch);
        return singleton->streamTextures[key] = std::make_shared<txtr>(newTex, srv, width, height, linearSampler, mapInfo.pData, mapInfo.RowPitch);
    }

    D3D11Machine::ImageSet::~ImageSet() {
        if (srv) { srv->Release(); }
        if (tex) { tex->Release(); }
    }

    D3D11Machine::RenderPass2Screen* D3D11Machine::createRenderPass2Screen(RenderTargetType* targets, uint32_t subpassCount, int32_t key, bool useDepth, bool* useDepthAsInput) {
        RenderPass2Screen* r = getRenderPass2Screen(key);
        if (r) return r;
        if (subpassCount == 0) return nullptr;
        std::vector<RenderTarget*> targs(subpassCount);
        for (uint32_t i = 0; i < subpassCount - 1; i++) {
            targs[i] = createRenderTarget2D(singleton->surfaceWidth, singleton->surfaceHeight, INT32_MIN, targets[i], RenderTargetInputOption::SAMPLED_LINEAR, useDepthAsInput ? useDepthAsInput[i] : false);
            if (!targs[i]) {
                LOGHERE;
                for (RenderTarget* t : targs) delete t;
                return nullptr;
            }
        }

        RenderPass* ret = new RenderPass(targs.data(), subpassCount);
        ret->targets = std::move(targs);
        ret->setViewport((float)singleton->surfaceWidth, (float)singleton->surfaceHeight, 0.0f, 0.0f);
        ret->setScissor(singleton->surfaceWidth, singleton->surfaceHeight, 0, 0);
        if (key == INT32_MIN) return ret;
        return singleton->finalPasses[key] = ret;
    }

    D3D11Machine::RenderPass* D3D11Machine::createRenderPass(RenderTarget** targets, uint32_t subpassCount, int32_t key) {
        if (RenderPass* r = getRenderPass(key)) return r;
        if (subpassCount == 0) return nullptr;

        if (targets[subpassCount - 1] == nullptr) {
            LOGWITH("Inavailable targets.");
            return {};
        }

        RenderPass* ret = new RenderPass(targets, subpassCount);
        std::memcpy(ret->targets.data(), targets, sizeof(RenderTarget*) * subpassCount);
        ret->setViewport((float)targets[0]->width, (float)targets[0]->height, 0.0f, 0.0f);
        ret->setScissor(targets[0]->width, targets[0]->height, 0, 0);
        if (key == INT32_MIN) return ret;
        return singleton->renderPasses[key] = ret;
    }

    D3D11Machine::RenderPass2Cube* D3D11Machine::createRenderPass2Cube(uint32_t width, uint32_t height, int32_t key, bool useColor, bool useDepth) {
        if (RenderPass2Cube* r = getRenderPass2Cube(key)) return r;
        if (!useColor && !useDepth) {
            LOGWITH("Either useColor or useDepth must be true");
            return {};
        }
        D3D11_TEXTURE2D_DESC textureInfo{};
        textureInfo.Width = width;
        textureInfo.Height = height;
        textureInfo.MipLevels = 1;
        textureInfo.ArraySize = 6;
        textureInfo.SampleDesc.Count = 1;
        textureInfo.SampleDesc.Quality = 0;
        textureInfo.Usage = D3D11_USAGE_DEFAULT;
        textureInfo.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

        ID3D11Texture2D* colorMap{}, *depthMap{};
        ID3D11RenderTargetView* colorView{};
        ID3D11DepthStencilView* depthView{};
        ID3D11ShaderResourceView* srv{};
        if (useColor) {
            textureInfo.Format = DXGI_FORMAT_R8G8B8A8_UINT;
            textureInfo.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET | D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
            HRESULT result = singleton->device->CreateTexture2D(&textureInfo, nullptr, &colorMap);
            if (result != S_OK) {
                LOGWITH("Failed to create color target:", result);
                return {};
            }
            D3D11_RENDER_TARGET_VIEW_DESC targetInfo{};
            targetInfo.Format = textureInfo.Format;
            targetInfo.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            targetInfo.Texture2DArray.ArraySize = 6; // TODO: 그냥 GS 쓰게 만들거나 / RTV랑 DSV 6개로 분리하거나
            targetInfo.Texture2DArray.FirstArraySlice = 0;
            targetInfo.Texture2DArray.MipSlice = 0;
            result = singleton->device->CreateRenderTargetView(colorMap, &targetInfo, &colorView);
            if (result != S_OK) {
                LOGWITH("Failed to create color target view:", result);
                colorMap->Release();
                return {};
            }
        }
        if (useDepth) {
            textureInfo.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            textureInfo.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_DEPTH_STENCIL;
            if (!useColor) textureInfo.BindFlags |= D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;
            HRESULT result = singleton->device->CreateTexture2D(&textureInfo, nullptr, &depthMap);
            if (result != S_OK) {
                LOGWITH("Failed to create depth target:", result);
                if (colorMap) { 
                    colorMap->Release();
                    colorView->Release();
                }
                return {};
            }
            D3D11_DEPTH_STENCIL_VIEW_DESC targetInfo{};
            targetInfo.Format = textureInfo.Format;
            targetInfo.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            targetInfo.Texture2DArray.ArraySize = 6;
            targetInfo.Texture2DArray.FirstArraySlice = 0;
            targetInfo.Texture2DArray.MipSlice = 0;
            result = singleton->device->CreateDepthStencilView(depthMap, &targetInfo, &depthView);
            if (result != S_OK) {
                LOGWITH("Failed to create depth target view:", result);
                depthMap->Release();
                if (colorMap) {
                    colorMap->Release();
                    colorView->Release();
                    return {};
                }
            }
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC viewInfo{};
        viewInfo.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        viewInfo.TextureCube.MipLevels = 1;
        viewInfo.TextureCube.MostDetailedMip = 0;
        HRESULT result = singleton->device->CreateShaderResourceView(colorMap ? colorMap : depthMap, &viewInfo, &srv);
        if (result != S_OK) {
            LOGWITH("Failed to create shader resource view:", result);
            if (colorMap) {
                colorMap->Release();
                colorView->Release();
            }
            if (depthMap) {
                depthMap->Release();
                depthView->Release();
            }
            return {};
        }
        
        RenderPass2Cube* newPass = new RenderPass2Cube;
        newPass->width = width;
        newPass->height = height;

        newPass->colorMap = colorMap;
        newPass->depthMap = depthMap;
        newPass->rtv = colorView;
        newPass->dsv = depthView;
        newPass->srv = srv;

        newPass->viewport.TopLeftX = 0;
        newPass->viewport.TopLeftY = 0;
        newPass->viewport.Width = width;
        newPass->viewport.Height = height;
        newPass->viewport.MinDepth = 0.0f;
        newPass->viewport.MaxDepth = 1.0f;

        newPass->scissor.left = 0;
        newPass->scissor.top = 0;
        newPass->scissor.right = width;
        newPass->scissor.bottom = height;
    }

    D3D11Machine::RenderPass2Cube::~RenderPass2Cube() {
        if (colorMap) {
            colorMap->Release();
            rtv->Release();
        }
        if (depthMap) {
            depthMap->Release();
            dsv->Release();
        }
        srv->Release();
    }

    void D3D11Machine::RenderPass2Cube::bind(uint32_t pos, UniformBuffer* ub, uint32_t pass, uint32_t ubPos) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if (pass >= 6) {
            singleton->context->VSSetConstantBuffers(pos, 1, &ub->ubo);
            singleton->context->PSSetConstantBuffers(pos, 1, &ub->ubo);
        }
        else {
            facewise[pass].ub = ub;
            facewise[pass].ubPos = ubPos;
            facewise[pass].setPos = pos;
        }
    }

    void D3D11Machine::RenderPass2Cube::bind(uint32_t pos, const pTexture& tx) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        singleton->context->VSSetShaderResources(pos, 1, &tx->dset);
        singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // 임시
        singleton->context->PSSetSamplers(pos, 1, tx->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
    }

    void D3D11Machine::RenderPass2Cube::bind(uint32_t pos, RenderTarget* target, uint32_t index) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        ID3D11ShaderResourceView* srv{};
        switch (index)
        {
        case 0:
            srv = target->color1->srv;
            break;
        case 1:
            srv = target->color2->srv;
            break;
        case 2:
            srv = target->color3->srv;
            break;
        case 3:
            srv = target->ds->srv;
            break;
        default:
            break;
        }
        if (!srv) {
            LOGWITH("Warning: requested texture is empty");
        }
        singleton->context->VSSetShaderResources(pos, 1, &srv);
        singleton->context->PSSetShaderResources(pos, 1, &srv); // 임시
        singleton->context->PSSetSamplers(pos, 1, target->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
    }

    void D3D11Machine::RenderPass2Cube::bind(uint32_t pos, const pStreamTexture& tx) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        singleton->context->VSSetShaderResources(pos, 1, &tx->dset);
        singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // 임시
        singleton->context->PSSetSamplers(pos, 1, tx->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
    }

    void D3D11Machine::RenderPass2Cube::usePipeline(Pipeline* pipeline) {
        this->pipeline = pipeline;
        if (recording) {
            singleton->context->IASetInputLayout(pipeline->layout);
            singleton->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            singleton->context->VSSetShader(pipeline->vs, nullptr, 0);
            singleton->context->HSSetShader(pipeline->tcs, nullptr, 0);
            singleton->context->DSSetShader(pipeline->tes, nullptr, 0);
            singleton->context->GSSetShader(pipeline->gs, nullptr, 0);
            singleton->context->PSSetShader(pipeline->fs, nullptr, 0);
            singleton->context->OMSetDepthStencilState(pipeline->dsState, 0);
            singleton->context->OMSetBlendState(singleton->basicBlend, nullptr, 0xffffffff);
        }
    }

    void D3D11Machine::RenderPass2Cube::push(void* input, uint32_t start, uint32_t end) {
        singleton->uniformBuffers[INT32_MIN + 1]->update(input, 0, start, end - start);
    }

    void D3D11Machine::RenderPass2Cube::invoke(const pMesh& mesh, uint32_t start, uint32_t count) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        singleton->context->IASetVertexBuffers(0, 1, &mesh->vb, nullptr, nullptr);
        if (mesh->icount) {
            singleton->context->IASetIndexBuffer(mesh->ib, mesh->indexFormat, 0);
            if ((uint64_t)start + count > mesh->icount) {
                LOGWITH("Invalid call: this mesh has", mesh->icount, "indices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                return;
            }
            if (count == 0) {
                count = mesh->icount - start;
            }
            for (int i = 0; i < 6; i++) {
                auto& fwi = facewise[i];
                if (fwi.ub) {
                    singleton->context->VSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                    singleton->context->PSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                }
                singleton->context->DrawIndexed(count, start, 0);
            }
        }
        else {
            if ((uint64_t)start + count > mesh->vcount) {
                LOGWITH("Invalid call: this mesh has", mesh->vcount, "vertices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                return;
            }
            if (count == 0) {
                count = mesh->vcount - start;
            }
            for (int i = 0; i < 6; i++) {
                auto& fwi = facewise[i];
                if (fwi.ub) {
                    singleton->context->VSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                    singleton->context->PSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                }
                singleton->context->Draw(count, start);
            }
        }
    }

    void D3D11Machine::RenderPass2Cube::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        ID3D11Buffer* vb[2] = { mesh->vb, instanceInfo->vb };
        singleton->context->IASetVertexBuffers(0, 2, vb, nullptr, nullptr);
        if (mesh->icount) {
            singleton->context->IASetIndexBuffer(mesh->ib, mesh->indexFormat, 0);
            if ((uint64_t)start + count > mesh->icount) {
                LOGWITH("Invalid call: this mesh has", mesh->icount, "indices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                return;
            }
            if (count == 0) {
                count = mesh->icount - start;
            }
            for (int i = 0; i < 6; i++) {
                auto& fwi = facewise[i];
                if (fwi.ub) {
                    singleton->context->VSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                    singleton->context->PSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                }
                singleton->context->DrawIndexedInstanced(count, instanceCount, start, 0, istart);
            }
        }
        else {
            if ((uint64_t)start + count > mesh->vcount) {
                LOGWITH("Invalid call: this mesh has", mesh->vcount, "vertices but", start, "~", (uint64_t)start + count, "requested to be drawn");
                return;
            }
            if (count == 0) {
                count = mesh->vcount - start;
            }
            for (int i = 0; i < 6; i++) {
                auto& fwi = facewise[i];
                if (fwi.ub) {
                    singleton->context->VSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                    singleton->context->PSSetConstantBuffers(fwi.setPos, 1, &fwi.ub->ubo);
                }
                singleton->context->DrawInstanced(count, instanceCount, start, istart);
            }
        }
    }

    void D3D11Machine::RenderPass2Cube::start() {
        if (recording) {
            LOGWITH("Invalid call: renderpass already started");
            return;
        }
        if (!pipeline) {
            LOGWITH("Pipeline not set:", this);
            return;
        }
        wait();
        recording = true;
        usePipeline(pipeline);
        singleton->context->RSSetViewports(1, &viewport);
        singleton->context->RSSetScissorRects(1, &scissor);
        // TODO: VS 기반 렌더링을 위한 타겟 세팅
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
        ID3D11RenderTargetView* rtv1{}, * rtv2{}, * rtv3{};
        ID3D11DepthStencilView* rtvds{};

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
                LOGWITH("Failed to create depth-stencil shader resoruce view:", result);
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
            
            D3D11_DEPTH_STENCIL_VIEW_DESC dsInfo{};
            dsInfo.Format = textureInfo.Format;
            dsInfo.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            dsInfo.Texture2D.MipSlice = 0;

            result = singleton->device->CreateDepthStencilView(ds->tex, &dsInfo, &rtvds);
            if (result != S_OK) {
                LOGWITH("Failed to create color depth-stencil view:", result);
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
        ID3D11RenderTargetView* params2[] = {rtv1, rtv2, rtv3};
        if (key == INT32_MIN) return new RenderTarget(type, width, height, params, params2, mmap, rtvds, sampled == RenderTargetInputOption::SAMPLED_LINEAR);
        return singleton->renderTargets[key] = new RenderTarget(type, width, height, params, params2, mmap, rtvds, sampled == RenderTargetInputOption::SAMPLED_LINEAR);
    }

    template<class T>
    inline static T* downcastShader(ID3D11DeviceChild* child) {
        if (!child) return nullptr;
        T* obj;
        if (SUCCEEDED(child->QueryInterface(__uuidof(T), (void**)&obj))) {
            return obj;
        }
        return nullptr;
    }

    D3D11Machine::Pipeline* D3D11Machine::createPipeline(PipelineInputVertexSpec* vinfo, uint32_t vsize, uint32_t vattr, PipelineInputVertexSpec* iinfo, uint32_t isize, uint32_t iattr, void* vsBytecode, uint32_t codeSize, ID3D11DeviceChild* vs, ID3D11DeviceChild* fs, int32_t name, bool depth, vec4 clearColor = {}, UINT stencilRef = 0, D3D11_DEPTH_STENCILOP_DESC* front = nullptr, D3D11_DEPTH_STENCILOP_DESC* back = nullptr, ID3D11DeviceChild* tc = nullptr, ID3D11DeviceChild* te = nullptr, ID3D11DeviceChild* gs = nullptr) {
        if (auto ret = getPipeline(name)) { return ret; }
        std::vector<PipelineInputVertexSpec> inputLayoutInfo(vattr + iattr);
        std::memcpy(inputLayoutInfo.data(), vinfo, vattr * sizeof(PipelineInputVertexSpec));
        ID3D11InputLayout* layout{};
        HRESULT result = singleton->device->CreateInputLayout(inputLayoutInfo.data(), vattr + iattr, vsBytecode, codeSize, &layout);
        if (result != S_OK) { 
            LOGWITH("Failed to create vertex input layout:", result);
            return {};
        }
        ID3D11VertexShader* vert = downcastShader<ID3D11VertexShader>(vs);
        ID3D11PixelShader* frag = downcastShader<ID3D11PixelShader>(fs);
        if (!vert || !frag) {
            LOGWITH("Vertex shader and Pixel shader must be provided");
            return {};
        }

        ID3D11HullShader* tctrl = downcastShader<ID3D11HullShader>(tc);
        if (tc && !tctrl) {
            LOGWITH("Given hull shader is invalid");
            return {};
        }
        ID3D11DomainShader* teval = downcastShader<ID3D11DomainShader>(te);
        if (te && !teval) {
            LOGWITH("Given domain shader is invalid");
            return {};
        }
        ID3D11GeometryShader* geom = downcastShader<ID3D11GeometryShader>(gs);
        if (gs && !geom) {
            LOGWITH("Given geometry shader is invalid");
            return {};
        }
        ID3D11DepthStencilState* dsState = nullptr;
        D3D11_DEPTH_STENCIL_DESC dsStateInfo{};
        if (depth) {
            dsStateInfo.DepthEnable = true;
            dsStateInfo.DepthFunc = D3D11_COMPARISON_LESS;
        }
        dsStateInfo.StencilEnable = (back || front);
        dsStateInfo.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsStateInfo.StencilReadMask = 0xff;
        dsStateInfo.StencilReadMask = 0xff;
        if (back) dsStateInfo.BackFace = *back;
        if (front)dsStateInfo.FrontFace = *front;
        HRESULT result = singleton->device->CreateDepthStencilState(&dsStateInfo, &dsState);
        if (result != S_OK) {
            LOGWITH("Failed to create depth stencil state:", result);
            return {};
        }

        return singleton->pipelines[name] = new Pipeline(layout, vert, tctrl, teval, geom, frag, dsState, stencilRef, clearColor);
    }

    unsigned D3D11Machine::createPipelineLayout(...) { return 0; }
    unsigned D3D11Machine::getPipelineLayout(int32_t) { return 0; }

    D3D11Machine::Pipeline::Pipeline(ID3D11InputLayout* layout, ID3D11VertexShader* v, ID3D11HullShader* h, ID3D11DomainShader* d, ID3D11GeometryShader* g, ID3D11PixelShader* p, ID3D11DepthStencilState* dss, UINT stencilRef, vec4 clearColor)
        :vs(v), tcs(h), tes(d), gs(g), fs(p), dsState(dss), stencilRef(stencilRef), clearColor(clearColor), layout(layout) {
    }
    
    D3D11Machine::Pipeline::~Pipeline() {
        dsState->Release();
        layout->Release();
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

    void D3D11Machine::UniformBuffer::resize(uint32_t size) {
    }

    void D3D11Machine::UniformBuffer::update(const void* input, uint32_t index, uint32_t offset, uint32_t size) {
        if (offset + size > length) {
            LOGWITH("Requested buffer update range is invalid");
            return;
        }
        if (size == length) {
            singleton->context->UpdateSubresource(ubo, 0, nullptr, input, 0, 0);
        }
        else { // 일반적인 사용 패턴대로, 맵은 유지하지 않도록 함
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

    D3D11Machine::UniformBuffer* D3D11Machine::createUniformBuffer(uint32_t length, uint32_t size, size_t stages, int32_t key, uint32_t binding) {
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

    D3D11Machine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, ImageSet** sets, ID3D11RenderTargetView** rtvs, bool mmap, ID3D11DepthStencilView* dsv, bool linearSampled)
        :width(width), height(height), mapped(mmap), type(type), color1(sets[0]), color2(sets[1]), color3(sets[2]), ds(sets[3]), dset1(rtvs[0]), dset2(rtvs[1]), dset3(rtvs[2]), dsetDS(dsv), linearSampled(linearSampled) {

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

    D3D11Machine::RenderPass::RenderPass(RenderTarget** fb, uint16_t stageCount)
        :stageCount(stageCount), targets(stageCount) {

    }

    D3D11Machine::RenderPass::~RenderPass() {
        if (targets[stageCount - 1] == nullptr) { // renderpass to screen이므로 타겟을 자체 생성해서 보유
            for (RenderTarget* targ : targets) {
                delete targ;
            }
        }
    }

    void D3D11Machine::RenderPass::start(uint32_t pos, bool clearTarget) {
        if (currentRenderPass && currentRenderPass != reinterpret_cast<uint64_t>(this)) {
            LOGWITH("You can't make multiple renderpass being started in d3d11machine. Call RendrePass::execute() to end renderpass");
            return;
        }
        else {
            currentRenderPass = reinterpret_cast<uint64_t>(this);
        }

        if (currentPass == stageCount - 1) {
            LOGWITH("Invalid call. The last subpass already started");
            return;
        }
        bound = nullptr;
        currentPass++;
        if (!pipelines[currentPass]) {
            LOGWITH("Pipeline not set.");
            currentPass--;
            return;
        }

        if (targets[currentPass]) {
            ID3D11RenderTargetView* rtvs[4] = {};
            UINT idx = 0;
            if (targets[currentPass]->color1) {
                rtvs[idx++] = targets[currentPass]->dset1;
                if (targets[currentPass]->color2) {
                    rtvs[idx++] = targets[currentPass]->dset2;
                    if (targets[currentPass]->color3) {
                        rtvs[idx++] = targets[currentPass]->dset3;
                    }
                }
            }

            singleton->context->OMSetRenderTargets(idx, rtvs, targets[currentPass]->dsetDS);
        }
        else {
            if (currentPass != stageCount - 1) {
                LOGWITH("Warning: No render target set. Rendering to swapchain target");
            }
            ID3D11RenderTargetView* rtv = singleton->getSwapchainTarget();
            singleton->context->OMSetRenderTargets(1, &rtv, singleton->screenDSView);
        }

        if (currentPass > 0) {
            RenderTarget* prev = targets[currentPass - 1];
            if (prev->color1) bind(pos, prev, 0);
            if (prev->color2) bind(pos + 1, prev, 1);
            if (prev->color3) bind(pos + 2, prev, 2);
            if (prev->ds) bind(pos + 3, prev, 3);
        }
        singleton->context->RSSetViewports(1, &viewport);
        singleton->context->RSSetScissorRects(1, &scissor);
        usePipeline(pipelines[currentPass], currentPass);
        if (clearTarget) {
            if(targets[currentPass]->dsetDS) singleton->context->ClearDepthStencilView(targets[currentPass]->dsetDS, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            if (pipelines[currentPass]->clearColor.x >= 0 ||
                pipelines[currentPass]->clearColor.y >= 0 ||
                pipelines[currentPass]->clearColor.z >= 0 ||
                pipelines[currentPass]->clearColor.w >= 0 ) {
                if (targets[currentPass]->dset1) {
                    singleton->context->ClearRenderTargetView(targets[currentPass]->dset1, pipelines[currentPass]->clearColor.entry);
                    if (targets[currentPass]->dset2) {
                        singleton->context->ClearRenderTargetView(targets[currentPass]->dset2, pipelines[currentPass]->clearColor.entry);
                        if (targets[currentPass]->dset3) {
                            singleton->context->ClearRenderTargetView(targets[currentPass]->dset3, pipelines[currentPass]->clearColor.entry);
                        }
                    }
                }
            }
        }
    }

    void D3D11Machine::RenderPass::usePipeline(Pipeline* pipeline, unsigned subpass) {
        if (subpass >= stageCount) {
            LOGWITH("Invalid subpass. This renderpass has", stageCount, "subpasses but", subpass, "given");
            return;
        }
        pipelines[subpass] = pipeline;
        if (currentPass == subpass) { 
            singleton->context->IASetInputLayout(pipeline->layout);
            singleton->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            singleton->context->VSSetShader(pipeline->vs, nullptr, 0);
            singleton->context->HSSetShader(pipeline->tcs, nullptr, 0);
            singleton->context->DSSetShader(pipeline->tes, nullptr, 0);
            singleton->context->GSSetShader(pipeline->gs, nullptr, 0);
            singleton->context->PSSetShader(pipeline->fs, nullptr, 0);
            auto& currentTarget = targets[currentPass];
            singleton->context->OMSetDepthStencilState(pipelines[currentPass]->dsState, 0);
            singleton->context->OMSetBlendState(singleton->basicBlend, nullptr, 0xffffffff);
        }
    }

    void D3D11Machine::RenderPass::push(void* input, uint32_t start, uint32_t end) {
        singleton->uniformBuffers[INT32_MIN + 1]->update(input, 0, start, end - start);
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos) {
        if (currentPass >= 0) {
            singleton->context->VSSetConstantBuffers(pos, 1, &ub->ubo);
            singleton->context->PSSetConstantBuffers(pos, 1, &ub->ubo); // 임시
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, const pTexture& tx) {
        if (currentPass >= 0) {
            singleton->context->VSSetShaderResources(pos, 1, &tx->dset);
            singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // 임시
            singleton->context->PSSetSamplers(pos, 1, tx->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, const pStreamTexture& tx) {
        if (currentPass >= 0) {
            singleton->context->VSSetShaderResources(pos, 1, &tx->dset);
            singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // 임시
            singleton->context->PSSetSamplers(pos, 1, tx->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, RenderTarget* target, uint32_t index) {
        if (currentPass >= 0) {
            ID3D11ShaderResourceView* srv;
            switch (index)
            {
            case 0:
                srv = target->color1->srv;
                break;
            case 1:
                srv = target->color2->srv;
                break;
            case 2:
                srv = target->color3->srv;
                break;
            case 3:
                srv = target->ds->srv;
                break;
            default:
                LOGWITH("index must be 0~3");
                return;
            }
            singleton->context->VSSetShaderResources(pos, 1, &srv);
            singleton->context->PSSetShaderResources(pos, 1, &srv); // 임시
            singleton->context->PSSetSamplers(pos, 1, target->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::invoke(const pMesh& mesh, uint32_t start, uint32_t count) {
        if (bound != mesh.get()) {
            singleton->context->IASetVertexBuffers(0, 1, &mesh->vb, nullptr, nullptr);
            bound = mesh.get();
        }
        
        if (mesh->ib) {
            if (count == 0)count = mesh->icount - start;
            singleton->context->IASetIndexBuffer(mesh->ib, mesh->indexFormat, 0);
            singleton->context->DrawIndexed(count, start, 0);
        }
        else {
            if (count == 0)count = mesh->vcount - start;
            singleton->context->Draw(count, start);
        }        
    }

    void D3D11Machine::RenderPass::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count) {
        ID3D11Buffer* buf[2] = { mesh->vb,instanceInfo->vb };
        singleton->context->IASetVertexBuffers(0, 2, buf, nullptr, nullptr);
        bound = nullptr;
        if (mesh->ib) {
            if (count == 0)count = mesh->icount - start;
            singleton->context->IASetIndexBuffer(mesh->ib, mesh->indexFormat, 0);
            singleton->context->DrawIndexedInstanced(count, instanceCount, start, 0, istart);
        }
        else {
            if (count == 0)count = mesh->vcount - start;
            singleton->context->DrawInstanced(count, instanceCount, start, istart);
        }
    }

    void D3D11Machine::RenderPass::execute(RenderPass* other) {
        if (reinterpret_cast<uint64_t>(this) != currentRenderPass) {
            return;
        }
        if (currentPass != pipelines.size() - 1) {
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        if (targets.back() == nullptr) {
            singleton->swapchain.handle->Present(1, 0);
        }
        currentPass = -1;
        currentRenderPass = 0;
    }

    bool D3D11Machine::RenderPass::wait(uint64_t) {
        return true;
    }

    void D3D11Machine::RenderPass::setViewport(float width, float height, float x, float y, bool applyNow) {
        viewport.Height = height;
        viewport.Width = width;
        viewport.MaxDepth = 1.0f;
        viewport.MinDepth = 0.0f;
        viewport.TopLeftX = x;
        viewport.TopLeftY = y;
        if (applyNow && currentPass != -1) {
            singleton->context->RSSetViewports(1, &viewport);
        }
    }

    void D3D11Machine::RenderPass::setScissor(uint32_t width, uint32_t height, int32_t x, int32_t y, bool applyNow) {
        scissor.left = x;
        scissor.top = y;
        scissor.right = x + width;
        scissor.bottom = y + height;
        if (applyNow && currentPass != -1) {
            singleton->context->RSSetScissorRects(1, &scissor);
        }
    }

    D3D11Machine::Texture::Texture(ID3D11Resource* texture, ID3D11ShaderResourceView* dset, uint16_t width, uint16_t height, bool isCubemap, bool linearSampled)
        :texture(texture), dset(dset), width(width), height(height), isCubemap(isCubemap), linearSampled(linearSampled) {
    }

    D3D11Machine::Texture::~Texture() {
        dset->Release();
        texture->Release();
    }

    void D3D11Machine::Texture::collect(bool removeUsing) {
        if (removeUsing) {
            singleton->textures.clear();
        }
        else {
            for (auto it = singleton->textures.begin(); it != singleton->textures.end();) {
                if (it->second.use_count() == 1) {
                    singleton->textures.erase(it++);
                }
                else {
                    ++it;
                }
            }
        }
    }

    void D3D11Machine::Texture::drop(int32_t name) {
        singleton->textures.erase(name);
    }

    D3D11Machine::StreamTexture::StreamTexture(ID3D11Texture2D* txo, ID3D11ShaderResourceView* srv, uint16_t width, uint16_t height, bool linearSampler, void* mmap, uint64_t rowPitch)
        :txo(txo), dset(srv), width(width), height(height), linearSampled(linearSampler), mmap(mmap), rowPitch(rowPitch), copyFull(rowPitch == 4ULL * width) {

    }

    D3D11Machine::StreamTexture::~StreamTexture() {
        if (mmap) {
            singleton->context->Unmap(txo, 0);
        }
        dset->Release();
        txo->Release();
    }

    void D3D11Machine::StreamTexture::drop(int32_t name) {
        singleton->streamTextures.erase(name);
    }

    void D3D11Machine::StreamTexture::collect(bool removeUsing) {
        if (removeUsing) {
            singleton->textures.clear();
        }
        else {
            for (auto it = singleton->textures.begin(); it != singleton->textures.end();) {
                if (it->second.use_count() == 1) {
                    singleton->textures.erase(it++);
                }
                else {
                    ++it;
                }
            }
        }
    }

    void D3D11Machine::StreamTexture::update(void* img) {
        uint64_t rowSize = (uint64_t)4 * width;
        uint64_t size = rowSize * height;
        uint8_t* src = (uint8_t*)img;
        uint8_t* dest = (uint8_t*)mmap;
        if (mmap) {
            if (copyFull) {
                std::memcpy(dest, src, size);
            }
            else {
                for (uint16_t i = 0; i < height; i++) {
                    std::memcpy(mmap, src, rowSize);
                    src += rowSize;
                    dest += rowPitch;
                }
            }
        }
        else {
            singleton->context->UpdateSubresource(txo, 0, nullptr, img, rowSize, 0);
        }
    }

    D3D11Machine::Mesh::Mesh(ID3D11Buffer* vb, ID3D11Buffer* ib, DXGI_FORMAT indexFormat, size_t vcount, size_t icount)
        :vb(vb), ib(ib), indexFormat(indexFormat), vcount(vcount), icount(icount) {}

    D3D11Machine::Mesh::~Mesh() {
        if(vb) vb->Release();
        if (ib) ib->Release();
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