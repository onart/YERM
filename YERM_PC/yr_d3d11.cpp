#include "yr_d3d11.h"
#include "yr_sys.h"

#ifndef KHRONOS_STATIC
#define KHRONOS_STATIC
#endif
#include "../externals/ktx/include/ktx.h"
#include "../externals/single_header/stb_image.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
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
    static ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, D3D11Machine::TextureFormatOptions option);
    static ktx_error_code_e tryTranscode(ktxTexture2* texture, ID3D11Device* device, uint32_t nChannels, bool srgb, D3D11Machine::TextureFormatOptions hq);
    static DXGI_FORMAT textureFormatFallback(ID3D11Device* device, uint32_t nChannels, bool srgb, D3D11Machine::TextureFormatOptions hq, UINT flags);

    D3D11Machine::D3D11Machine() {
        if (singleton) {
            LOGWITH("Tried to create multiple D3D11Machine objects");
            return;
        }

        constexpr D3D_FEATURE_LEVEL FEATURE_LEVELS[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL lv = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_1_0_CORE;

        IDXGIFactory* factory;
        CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&factory));
        IDXGIAdapter* selectedAdapter{};
        IDXGIAdapter* currentAdapter{};
        uint64_t score = 0;

        for (UINT i = 0; factory->EnumAdapters(i, &currentAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
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
        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 device:", result);
            reason = result;
            return;
        }

        D3D11_RASTERIZER_DESC rasterizerInfo{};
        rasterizerInfo.CullMode = D3D11_CULL_NONE;
        rasterizerInfo.FillMode = D3D11_FILL_SOLID;
        rasterizerInfo.FrontCounterClockwise = true;
        rasterizerInfo.ScissorEnable = true;
        rasterizerInfo.MultisampleEnable = false;
        device->CreateRasterizerState(&rasterizerInfo, &basicRasterizer);
        context->RSSetState(basicRasterizer);

        D3D11_SAMPLER_DESC samplerInfo{};
        samplerInfo.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerInfo.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerInfo.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerInfo.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerInfo.MaxAnisotropy = 1;
        samplerInfo.MaxLOD = D3D11_FLOAT32_MAX;
        samplerInfo.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        device->CreateSamplerState(&samplerInfo, &linearBorderSampler);

        samplerInfo.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        device->CreateSamplerState(&samplerInfo, &nearestBorderSampler);
        singleton = this;
        UniformBufferCreationOptions opts;
        opts.size = 128;
        if (auto push = createUniformBuffer(INT32_MIN + 1, opts)) {
            context->VSSetConstantBuffers(13, 1, &push->ubo);
            context->PSSetConstantBuffers(13, 1, &push->ubo);
            context->GSSetConstantBuffers(13, 1, &push->ubo);
            context->HSSetConstantBuffers(13, 1, &push->ubo);
            context->DSSetConstantBuffers(13, 1, &push->ubo);
        }
        else {
            singleton = nullptr;
        }
    }

    void D3D11Machine::setVsync(bool vsync) {
        singleton->vsync = vsync ? 1 : 0;
    }

    bool D3D11Machine::addWindow(int32_t key, Window* window) {
        if (windowSystems.find(key) != windowSystems.end()) { return true; }
        auto w = new WindowSystem(window);
        if (w->swapchain.handle) {
            windowSystems[key] = w;            
            return true;
        }
        else {
            delete w;
            return false;
        }
    }

    void D3D11Machine::removeWindow(int32_t key) {
        for (auto it = finalPasses.begin(); it != finalPasses.end();) {
            if (it->second->windowIdx == key) {
                delete it->second;
                finalPasses.erase(it++);
            }
            else {
                ++it;
            }
        }
        windowSystems.erase(key);
    }

    void D3D11Machine::resetWindow(int32_t key, bool) {
        auto it = singleton->windowSystems.find(key);
        if (it == singleton->windowSystems.end()) { return; }

        it->second->resizeSwapchain();
        uint32_t width = it->second->swapchain.width;
        uint32_t height = it->second->swapchain.height;
        if (width && height) { // 값이 0이면 크기 0이라 스왑체인 재생성 실패한 것
            for (auto& fpass : singleton->finalPasses) {
                if (fpass.second->windowIdx == key) {
                    //fpass.second->reconstructFB(width, height);
                    fpass.second->setViewport(width, height, 0.5f, 0.5f, true);
                    fpass.second->setScissor(width, height, 0, 0, true);
                }
            }
        }
    }

    D3D11Machine::WindowSystem::WindowSystem(Window* window):window(window) {
        // https://learn.microsoft.com/ko-kr/windows/win32/api/dxgi/ns-dxgi-dxgi_swap_chain_desc?redirectedfrom=MSDN
        // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb173064(v=vs.85)
        IDXGIDevice* dxgiDevice{};
        IDXGIAdapter* dxgiAdapter{};
        IDXGIFactory* dxgiFactory{};

        HRESULT result = singleton->device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
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

        int width, height;
        window->getFramebufferSize(&width, &height);

        DXGI_SWAP_CHAIN_DESC swapchainInfo{};
        swapchainInfo.BufferDesc.Width = width;
        swapchainInfo.BufferDesc.Height = height;
        swapchainInfo.Windowed = true;
        swapchainInfo.SampleDesc.Count = 1;
        swapchainInfo.SampleDesc.Quality = 0;
        swapchainInfo.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchainInfo.BufferCount = 1;
        swapchainInfo.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swapchainInfo.OutputWindow = (HWND)window->getWin32Handle();
        //swapchainInfo.BufferDesc.ScanlineOrdering = 0;

        swapchainInfo.BufferDesc.RefreshRate.Numerator = window->getMonitorRefreshRate(); // only used in fullscreen
        swapchainInfo.BufferDesc.RefreshRate.Denominator = 1;
        swapchainInfo.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

        result = dxgiFactory->CreateSwapChain(singleton->device, &swapchainInfo, &swapchain.handle);
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
        result = singleton->device->CreateTexture2D(&dsInfo, nullptr, &dsTex);
        if (result != S_OK) {
            LOGWITH("Failed to create screen target depth stencil buffer:", result);
            reason = result;
            return;
        }
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvInfo{};
        dsvInfo.Format = dsInfo.Format;
        dsvInfo.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvInfo.Texture2D.MipSlice = 0;
        result = singleton->device->CreateDepthStencilView(dsTex, &dsvInfo, &screenDSView);
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

    D3D11Machine::WindowSystem::~WindowSystem() {
        for (auto& tg : screenTargets) {
            tg.first->Release();
            tg.second->Release();
        }
        if (screenDSView) { screenDSView->Release(); }
        if (swapchain.handle) { swapchain.handle->Release(); }
    }

    void D3D11Machine::WindowSystem::resizeSwapchain() {
        int w, h;
        window->getFramebufferSize(&w, &h);
        for (auto& targ : screenTargets) {
            targ.first->Release();
            targ.second->Release();
        }
        screenTargets.clear();
        HRESULT result = swapchain.handle->ResizeBuffers(1, w, h, DXGI_FORMAT_UNKNOWN, 0);
        screenDSView->Release();
        screenDSView = nullptr;
        
        if (result != S_OK) {
            _com_error err(result);
            LOGWITH("Failed to resize swapchain:", result, err.ErrorMessage());
            swapchain.handle->Release();
            swapchain.handle = nullptr;
            reason = result;
            return;
        }

        swapchain.width = w;
        swapchain.height = h;

        ID3D11Texture2D* dsTex{};

        D3D11_TEXTURE2D_DESC dsInfo{};
        dsInfo.Width = w;
        dsInfo.Height = h;
        dsInfo.MipLevels = 1;
        dsInfo.ArraySize = 1;
        dsInfo.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsInfo.Usage = D3D11_USAGE_DEFAULT;
        dsInfo.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        dsInfo.SampleDesc.Count = 1;
        dsInfo.SampleDesc.Quality = 0;
        result = singleton->device->CreateTexture2D(&dsInfo, nullptr, &dsTex);
        if (result != S_OK) {
            LOGWITH("Failed to create screen target depth stencil buffer:", result);
            reason = result;
            return;
        }
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvInfo{};
        dsvInfo.Format = dsInfo.Format;
        dsvInfo.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvInfo.Texture2D.MipSlice = 0;
        result = singleton->device->CreateDepthStencilView(dsTex, &dsvInfo, &screenDSView);
        if (result != S_OK) {
            LOGWITH("Failed to create screen target depth stencil buffer view:", result);
            reason = result;
            dsTex->Release();
            return;
        }

        dsTex->Release();
    }

    D3D11Machine::~D3D11Machine() {
        free();
    }

    void D3D11Machine::free() {
        basicRasterizer->Release();
        linearBorderSampler->Release();
        nearestBorderSampler->Release();
        for (auto& shader : shaders) { shader.second->Release(); }
        shaders.clear();
        meshes.clear();
        textures.clear();
        streamTextures.clear();
        for (auto& rt : renderTargets) { delete rt.second; }
        renderTargets.clear();
        for (auto& rp : renderPasses) { delete rp.second; }
        renderPasses.clear();
        for (auto& rp : finalPasses) { delete rp.second; }
        finalPasses.clear();
        for (auto& rp : cubePasses) { delete rp.second; }
        cubePasses.clear();
        for (auto& ub : uniformBuffers) { delete ub.second; }
        uniformBuffers.clear();
        for (auto& pp : pipelines) { delete pp.second; }
        pipelines.clear();
        for (auto ws : windowSystems) { delete ws.second; }
        windowSystems.clear();

        context->ClearState();
        context->Flush();
        context->Release();
        device->Release();
    }

    ID3D11RenderTargetView* D3D11Machine::WindowSystem::getSwapchainTarget() {
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
            result = singleton->device->CreateRenderTargetView(pBackBuffer, &rtvInfo, &rtv);
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

    void D3D11Machine::reap() {

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
        if (it != singleton->uniformBuffers.end()) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::RenderPass* D3D11Machine::getRenderPass(int32_t key) {
        auto it = singleton->renderPasses.find(key);
        if (it != singleton->renderPasses.end()) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::RenderPass2Cube* D3D11Machine::getRenderPass2Cube(int32_t key) {
        auto it = singleton->cubePasses.find(key);
        if (it != singleton->cubePasses.end()) {
            return it->second;
        }
        return {};
    }

    D3D11Machine::pMesh D3D11Machine::createNullMesh(int32_t key, size_t vcount) {
        pMesh ret = getMesh(key);
        if (ret) {
            return ret;
        }
        D3D11_BUFFER_DESC bufferInfo{};
        bufferInfo.ByteWidth = vcount * 4;
        bufferInfo.Usage = D3D11_USAGE_DEFAULT;
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

        struct publicmesh :public Mesh { publicmesh(ID3D11Buffer* _1, ID3D11Buffer* _2, DXGI_FORMAT _3, size_t _4, size_t _5, UINT _6) :Mesh(_1, _2, _3, _4, _5, _6) {} };
        ret = std::make_shared<publicmesh>(vb, nullptr, iFormat, vcount, 0, 1);
        return singleton->meshes[key] = ret;
    }

    D3D11Machine::pMesh D3D11Machine::createMesh(int32_t key, const MeshCreationOptions& opts) {
        pMesh ret = getMesh(key);
        if (ret) {
            return ret;
        }
        D3D11_BUFFER_DESC bufferInfo{};
        bufferInfo.ByteWidth = opts.singleVertexSize * opts.vertexCount;
        bufferInfo.Usage = opts.fixed ? D3D11_USAGE_IMMUTABLE : D3D11_USAGE_DYNAMIC;
        bufferInfo.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferInfo.CPUAccessFlags = opts.fixed ? 0 : D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = opts.vertices;

        ID3D11Buffer* vb{}, * ib{};
        DXGI_FORMAT iFormat{};
        HRESULT result = singleton->device->CreateBuffer(&bufferInfo, opts.vertices ? &vertexData : nullptr, &vb);
        if (result != S_OK) {
            LOGWITH("Failed to create vertex buffer:", result);
            reason = result;
            return {};
        }

        if (opts.indices) {
            bufferInfo.ByteWidth = opts.singleIndexSize * opts.indexCount;
            bufferInfo.BindFlags = D3D11_BIND_INDEX_BUFFER;
            if (opts.singleIndexSize == 2) {
                iFormat = DXGI_FORMAT_R16_UINT;
            }
            else if (opts.singleIndexSize == 4) {
                iFormat = DXGI_FORMAT_R32_UINT;
            }
            else {
                LOGWITH("Warning: index buffer size is not 2 nor 4");
            }
            D3D11_SUBRESOURCE_DATA indexData{};
            indexData.pSysMem = opts.indices;
            HRESULT result = singleton->device->CreateBuffer(&bufferInfo, &indexData, &ib);
            if (result != S_OK) {
                LOGWITH("Failed to create index buffer:", result);
                vb->Release();
                reason = result;
                return {};
            }
        }

        struct publicmesh :public Mesh { publicmesh(ID3D11Buffer* _1, ID3D11Buffer* _2, DXGI_FORMAT _3, size_t _4, size_t _5, UINT _6) :Mesh(_1, _2, _3, _4, _5, _6) {} };
        ret = std::make_shared<publicmesh>(vb, ib, iFormat, opts.vertexCount, opts.indexCount, opts.singleVertexSize);
        if (key == INT32_MIN) return ret;
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

    ID3D11DeviceChild* D3D11Machine::createShader(int32_t key, const ShaderModuleCreationOptions& opts) {
        if (auto sh = getShader(key)) return sh;
        HRESULT result{};
        ID3D11DeviceChild* ret{};
        switch (opts.stage)
        {
        case onart::D3D11Machine::ShaderStage::VERTEX:
            result = singleton->device->CreateVertexShader(opts.source, opts.size, nullptr, (ID3D11VertexShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderStage::FRAGMENT:
            result = singleton->device->CreatePixelShader(opts.source, opts.size, nullptr, (ID3D11PixelShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderStage::GEOMETRY:
            result = singleton->device->CreateGeometryShader(opts.source, opts.size, nullptr, (ID3D11GeometryShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderStage::TESS_CTRL:
            result = singleton->device->CreateHullShader(opts.source, opts.size, nullptr, (ID3D11HullShader**)&ret);
            break;
        case onart::D3D11Machine::ShaderStage::TESS_EVAL:
            result = singleton->device->CreateDomainShader(opts.source, opts.size, nullptr, (ID3D11DomainShader**)&ret);
            break;
        default:
            return {};
        }
        if (result != S_OK) {
            LOGWITH("Failed to create shader instance:", result);
            reason = result;
            return {};
        }
        return singleton->shaders[key] = ret;
    }

    ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, D3D11Machine::TextureFormatOptions option) {
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
        if (option == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
            ktxBasisParams params{};
            params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
            params.uastc = KTX_TRUE;
            params.verbose = KTX_FALSE;
            params.structSize = sizeof(params);

            k2result = ktxTexture2_CompressBasisEx(texture, &params);
            if (k2result != KTX_SUCCESS) {
                LOGWITH("Compress failed:", k2result);
                return nullptr;
            }
        }
        return texture;
    }

    ktx_error_code_e tryTranscode(ktxTexture2* texture, ID3D11Device* device, uint32_t nChannels, bool srgb, D3D11Machine::TextureFormatOptions hq) {
        if (ktxTexture2_NeedsTranscoding(texture)) {
            ktx_transcode_fmt_e tf;
            switch (textureFormatFallback(device, nChannels, srgb, hq, texture->isCubemap ? D3D11_FORMAT_SUPPORT_TEXTURECUBE : D3D11_FORMAT_SUPPORT_TEXTURE2D))
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

    D3D11Machine::pTexture D3D11Machine::getTexture(int32_t key) {
        std::unique_lock<std::mutex> _(singleton->textureGuard);
        auto it = singleton->textures.find(key);
        if (it != singleton->textures.end()) return it->second;
        else return pTexture();
    }

    void D3D11Machine::asyncCreateTexture(int32_t key, const uint8_t* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
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
        TextureCreationOptions options = opts;
        singleton->loadThread.post([mem, size, key, options]() {
            pTexture ret = singleton->createTexture(INT32_MIN, mem, size, options);
            if (!ret) {
                variant8 _k;
                _k.bytedata2[0] = key;
                _k.bytedata2[1] = D3D11Machine::reason;
                return _k;
            }
            singleton->textureGuard.lock();
            singleton->textures[key] = std::move(ret);
            singleton->textureGuard.unlock();
            variant8 _k;
            _k.bytedata2[0] = key;
            return _k;
            }, handler, vkm_strand::GENERAL);
    }

    void D3D11Machine::asyncCreateTextureFromImage(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
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
        TextureCreationOptions options;
        singleton->loadThread.post([fileName, key, options]() {
            pTexture ret = singleton->createTextureFromImage(INT32_MIN, fileName, options);
            if (!ret) {
                variant8 _k;
                _k.bytedata2[0] = key;
                _k.bytedata2[1] = D3D11Machine::reason;
                return _k;
            }
            singleton->textureGuard.lock();
            singleton->textures[key] = std::move(ret);
            singleton->textureGuard.unlock();
            variant8 _k;
            _k.bytedata2[0] = key;
            return _k;
            }, handler, vkm_strand::GENERAL);
    }

    void D3D11Machine::asyncCreateTextureFromImage(int32_t key, const void* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
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
        TextureCreationOptions options = opts;
        singleton->loadThread.post([mem, size, key, options]() {
            pTexture ret = singleton->createTextureFromImage(INT32_MIN, mem, size, options);
            if (!ret) {
                variant8 _k;
                _k.bytedata2[0] = key;
                _k.bytedata2[1] = D3D11Machine::reason;
                return _k;
            }
            singleton->textureGuard.lock();
            singleton->textures[key] = std::move(ret);
            singleton->textureGuard.unlock();
            variant8 _k;
            _k.bytedata2[0] = key;
            return _k;
            }, handler, vkm_strand::GENERAL);
    }

    void D3D11Machine::asyncCreateTexture(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
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
        TextureCreationOptions options = opts;
        singleton->loadThread.post([fileName, key, options]() {
            pTexture ret = singleton->createTexture(INT32_MIN, fileName, options);
            if (!ret) {
                variant8 _k;
                _k.bytedata2[0] = key;
                _k.bytedata2[1] = D3D11Machine::reason;
                return _k;
            }
            singleton->textureGuard.lock();
            singleton->textures[key] = std::move(ret);
            singleton->textureGuard.unlock();
            variant8 _k;
            _k.bytedata2[0] = key;
            return _k;
            }, handler, vkm_strand::GENERAL);
    }

    D3D11Machine::pTexture D3D11Machine::createTextureFromImage(int32_t key, const void* mem, size_t size, const TextureCreationOptions& opts) {
        if (auto ret = getTexture(key)) { return ret; }
        int x, y, nChannels;
        uint8_t* pix = stbi_load_from_memory((uint8_t*)mem, size, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return pTexture();
        }
        TextureCreationOptions options = opts;
        options.nChannels = nChannels;
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, opts.srgb, opts.opts);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return pTexture();
        }

        return singleton->createTexture(texture, key, options);
    }

    D3D11Machine::pTexture D3D11Machine::createTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, const TextureCreationOptions& opts) {
        if (auto tex = getTexture(key)) { return tex; }
        ktxTexture2* texture = createKTX2FromImage(color, width, height, opts.nChannels, opts.srgb, opts.opts);
        if (!texture) {
            LOGHERE;
            return {};
        }
        return singleton->createTexture(texture, key, opts);
    }

    void D3D11Machine::aysncCreateTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
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

    D3D11Machine::pTexture D3D11Machine::createTextureFromImage(int32_t key, const char* fileName, const TextureCreationOptions& opts) {
        if (auto ret = getTexture(key)) { return ret; }
        int x, y, nChannels;
        uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return pTexture();
        }
        TextureCreationOptions options = opts;
        options.nChannels = nChannels;
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, opts.srgb, opts.opts);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, options);
    }

    D3D11Machine::pTexture D3D11Machine::createTexture(int32_t key, const char* fileName, const TextureCreationOptions& opts) {
        if (auto ret = getTexture(key)) { return ret; }
        if (opts.nChannels > 4 || opts.nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }

        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if ((k2result = ktxTexture2_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
            LOGWITH("Failed to load ktx texture:", k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, opts);
    }

    D3D11Machine::pTexture D3D11Machine::createTexture(int32_t key, const uint8_t* mem, size_t size, const TextureCreationOptions& opts) {
        if (auto ret = getTexture(key)) { return ret; }
        if (opts.nChannels > 4 || opts.nChannels == 0) {
            LOGWITH("Invalid channel count. nChannels must be 1~4");
            return pTexture();
        }

        ktxTexture2* texture;
        ktx_error_code_e k2result;
        if ((k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS) {
            LOGWITH("Failed to load ktx texture:", k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, opts);
    }

    D3D11Machine::pTexture D3D11Machine::createTexture(void* ktxObj, int32_t key, const TextureCreationOptions& opts) {
        ktxTexture2* texture = reinterpret_cast<ktxTexture2*>(ktxObj);
        if (texture->numLevels == 0) return pTexture();
        ktx_error_code_e k2result = tryTranscode(texture, device, opts.nChannels, opts.srgb, opts.opts);
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
        info.Format = textureFormatFallback(device, opts.nChannels, opts.srgb, opts.opts, texture->isCubemap ? D3D11_FORMAT_SUPPORT_TEXTURECUBE : D3D11_FORMAT_SUPPORT_TEXTURE2D);
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
        descInfo.ViewDimension = (info.MiscFlags == D3D11_RESOURCE_MISC_FLAG::D3D11_RESOURCE_MISC_TEXTURECUBE) ? D3D11_SRV_DIMENSION_TEXTURECUBE : D3D11_SRV_DIMENSION_TEXTURE2D;
        descInfo.Texture2D.MipLevels = info.MipLevels;
        result = singleton->device->CreateShaderResourceView(newTex, &descInfo, &srv);
        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 shader resource view:", result);
            newTex->Release();
            reason = result;
            return pTexture();
        }
        
        struct txtr :public Texture { inline txtr(ID3D11Resource* _1, ID3D11ShaderResourceView* _2, uint16_t _3, uint16_t _4, bool _5, bool _6) :Texture(_1, _2, _3, _4, _5, _6) {} };
        if (key == INT32_MIN) return std::make_shared<txtr>(newTex, srv, width, height, texture->isCubemap, opts.linearSampled);
        return textures[key] = std::make_shared<txtr>(newTex, srv, width, height, texture->isCubemap, opts.linearSampled);
    }

    D3D11Machine::pStreamTexture D3D11Machine::createStreamTexture(int32_t key, uint32_t width, uint32_t height, bool linearSampler) {
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

    D3D11Machine::RenderPass2Screen* D3D11Machine::createRenderPass2Screen(int32_t key, int32_t windowIdx, const RenderPassCreationOptions& opts) {
        auto it = singleton->windowSystems.find(windowIdx);
        if (it == singleton->windowSystems.end()) {
            LOGWITH("Invalid window number");
            return nullptr;
        }
        WindowSystem* window = it->second;
        RenderPass2Screen* r = getRenderPass2Screen(key);
        if (r) return r;
        if (opts.subpassCount == 0) return nullptr;
        std::vector<RenderTarget*> targs(opts.subpassCount);
        for (uint32_t i = 0; i < opts.subpassCount - 1; i++) {
            targs[i] = createRenderTarget2D(window->swapchain.width, window->swapchain.height, opts.targets ? opts.targets[i] : RenderTargetType::RTT_COLOR1, opts.depthInput ? opts.depthInput[i] : false, false, false);
            if (!targs[i]) {
                LOGHERE;
                for (RenderTarget* t : targs) delete t;
                return nullptr;
            }
        }

        RenderPass* ret = new RenderPass(opts.subpassCount, false, opts.autoclear.use ? (float*)opts.autoclear.color : nullptr);
        ret->targets = std::move(targs);
        ret->setViewport((float)window->swapchain.width, (float)window->swapchain.height, 0.0f, 0.0f);
        ret->setScissor(window->swapchain.width, window->swapchain.height, 0, 0);
        ret->is4Screen = true;
        ret->windowIdx = windowIdx;
        if (key == INT32_MIN) return ret;
        return singleton->finalPasses[key] = ret;
    }

    D3D11Machine::RenderPass* D3D11Machine::createRenderPass(int32_t key, const RenderPassCreationOptions& opts) {
        if (RenderPass* r = getRenderPass(key)) return r;
        if (opts.subpassCount == 0) return nullptr;

        std::vector<RenderTarget*> targs(opts.subpassCount);

        for (uint32_t i = 0; i < opts.subpassCount; i++) {
            targs[i] = createRenderTarget2D(opts.width, opts.height, opts.targets ? opts.targets[i] : RenderTargetType::RTT_COLOR1, opts.depthInput ? opts.depthInput[i] : false, i == opts.subpassCount - 1, opts.linearSampled);
            if (!targs[i]) {
                LOGHERE;
                for (RenderTarget* t : targs) { if (t) delete t; }
                return {};
            }
        }

        RenderPass* ret = new RenderPass(opts.subpassCount, opts.canCopy, opts.autoclear.use ? (float*)opts.autoclear.color : nullptr);
        ret->targets = std::move(targs);
        ret->setViewport((float)opts.width, (float)opts.height, 0.0f, 0.0f);
        ret->setScissor(opts.width, opts.height, 0, 0);
        if (key == INT32_MIN) return ret;
        return singleton->renderPasses[key] = ret;
    }

    D3D11Machine::RenderPass2Cube* D3D11Machine::createRenderPass2Cube(int32_t key, uint32_t width, uint32_t height, bool useColor, bool useDepth) {
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
            targetInfo.Texture2DArray.ArraySize = 6; // TODO: �׳� GS ���� ����ų� / RTV�� DSV 6���� �и��ϰų�
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
            textureInfo.Format = DXGI_FORMAT_R24G8_TYPELESS;
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
            targetInfo.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
        viewInfo.Format = useColor ? DXGI_FORMAT_R8G8B8A8_UINT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
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
        return singleton->cubePasses[key] = newPass;
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
        if (ubPos == 13) {
            LOGWITH("Invalid call: bind pos 13 is reserved for push()");
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
        singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // �ӽ�
        singleton->context->PSSetSamplers(pos, 1, tx->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
    }

    void D3D11Machine::RenderPass2Cube::bind(uint32_t pos, RenderPass2Cube* prev) {
        if (recording) {
            singleton->context->VSSetShaderResources(pos, 1, &prev->srv);
            singleton->context->PSSetShaderResources(pos, 1, &prev->srv);
            ID3D11SamplerState* samp[1]{};
            for (int i = 0; i < 1; i++) {
                samp[i] = singleton->linearBorderSampler;
            }
            singleton->context->VSSetSamplers(pos, 1, samp);
            singleton->context->PSSetSamplers(pos, 1, samp);
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass2Cube::bind(uint32_t pos, RenderPass* prev) {
        if (recording) {
            if (auto targ = prev->targets.back()) {
                ID3D11ShaderResourceView* srv[4]{};
                int nim = 0;
                if (targ->color1) {
                    srv[nim++] = targ->color1->srv;
                    if (targ->color2) {
                        srv[nim++] = targ->color2->srv;
                        if (targ->color3) {
                            srv[nim++] = targ->color3->srv;
                        }
                    }
                }
                if (targ->ds && targ->ds->srv) {
                    srv[nim++] = targ->ds->srv;
                }
                singleton->context->VSSetShaderResources(pos, nim, srv);
                singleton->context->PSSetShaderResources(pos, nim, srv);
                ID3D11SamplerState* samp[4]{};
                for (int i = 0; i < 4; i++) {
                    samp[i] = targ->linearSampled ? singleton->linearBorderSampler : singleton->nearestBorderSampler;
                }
                singleton->context->VSSetSamplers(pos, nim, samp);
                singleton->context->PSSetSamplers(pos, nim, samp);
            }
            else {
                LOGWITH("Can\'t bind renderpas2screen");
            }
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass2Cube::bind(uint32_t pos, const pStreamTexture& tx) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        singleton->context->VSSetShaderResources(pos, 1, &tx->dset);
        singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // �ӽ�
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
            singleton->context->OMSetDepthStencilState(pipeline->dsState, pipeline->stencilRef);
            singleton->context->OMSetBlendState(pipeline->blendState, nullptr, 0xffffffff);
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
        UINT offset = 0;
        singleton->context->IASetVertexBuffers(0, 1, &mesh->vb, &mesh->vStride, &offset);
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
        UINT strides[2] = { mesh->vStride };
        if (instanceInfo) {
            vb[1] = instanceInfo->vb;
            strides[1] = instanceInfo->vStride;
        }
        UINT offsets[2]{};
        singleton->context->IASetVertexBuffers(0, instanceInfo ? 2 : 1, vb, strides, offsets);
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

    bool D3D11Machine::RenderPass2Cube::wait(uint64_t) {
        return true;
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
        // TODO: VS ��� �������� ���� Ÿ�� ����
    }

    D3D11Machine::RenderTarget* D3D11Machine::createRenderTarget2D(int width, int height, RenderTargetType type, bool useDepthInput, bool sampled, bool linear) {
        D3D11_TEXTURE2D_DESC textureInfo{};
        textureInfo.Width = width;
        textureInfo.Height = height;
        textureInfo.MipLevels = 1;
        textureInfo.ArraySize = 1;
        textureInfo.SampleDesc.Count = 1;
        textureInfo.SampleDesc.Quality = 0;
        textureInfo.Usage = D3D11_USAGE_DEFAULT;
        textureInfo.CPUAccessFlags = 0;

        textureInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // TODO: SRGB?
        textureInfo.BindFlags = D3D11_BIND_FLAG::D3D11_BIND_RENDER_TARGET | D3D11_BIND_FLAG::D3D11_BIND_SHADER_RESOURCE;

        D3D11_RENDER_TARGET_VIEW_DESC targetInfo{};
        targetInfo.Format = textureInfo.Format;
        targetInfo.ViewDimension = D3D11_RTV_DIMENSION::D3D11_RTV_DIMENSION_TEXTURE2D;
        targetInfo.Texture2D.MipSlice = 0;

        ImageSet* color1{}, * color2{}, * color3{}, * ds{};
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
            if (useDepthInput) {
                textureInfo.Format = DXGI_FORMAT_R24G8_TYPELESS;
            }
            else {
                textureInfo.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
            if (useDepthInput) {
                descInfo.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
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
            }

            D3D11_DEPTH_STENCIL_VIEW_DESC dsInfo{};
            dsInfo.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
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
        ImageSet* params[] = { color1, color2, color3, ds };
        ID3D11RenderTargetView* params2[] = { rtv1, rtv2, rtv3 };
        return new RenderTarget(type, width, height, params, params2, rtvds, linear, useDepthInput);
    }

    template<class T>
    inline static T* downcastShader(ID3D11DeviceChild* child) {
        if (!child) return nullptr;
        T* obj;
        if (SUCCEEDED(child->QueryInterface(__uuidof(T), (void**)&obj))) {
            obj->Release();
            return obj;
        }
        return nullptr;
    }

    D3D11Machine::Pipeline* D3D11Machine::createPipeline(int32_t key, const PipelineCreationOptions& opts) {
        if (auto ret = getPipeline(key)) { return ret; }
        std::vector<PipelineInputVertexSpec> inputLayoutInfo(opts.vertexAttributeCount + opts.instanceAttributeCount);
        std::memcpy(inputLayoutInfo.data(), opts.vertexSpec, opts.vertexAttributeCount * sizeof(PipelineInputVertexSpec));
        ID3D11InputLayout* layout{};
        HRESULT result{};
        if (opts.vsByteCodeSize == 0) {
            Pipeline* pp = reinterpret_cast<Pipeline*>(opts.vsByteCode);
            if (!pp) {
                LOGWITH("Failed to get vertex input layout");
                return {};
            }
            layout = pp->layout;
            layout->AddRef();
        }
        else {
            result = singleton->device->CreateInputLayout(inputLayoutInfo.data(), opts.vertexAttributeCount + opts.instanceAttributeCount, opts.vsByteCode, opts.vsByteCodeSize, &layout);
            if (result != S_OK) {
                LOGWITH("Failed to create vertex input layout:", result);
                return {};
            }
        }
        ID3D11VertexShader* vert = downcastShader<ID3D11VertexShader>(opts.vertexShader);
        ID3D11PixelShader* frag = downcastShader<ID3D11PixelShader>(opts.fragmentShader);
        if (!vert || !frag) {
            LOGWITH(vert, frag);
            LOGWITH("Vertex shader and Pixel shader must be provided");
            return {};
        }

        ID3D11HullShader* tctrl = downcastShader<ID3D11HullShader>(opts.tessellationControlShader);
        if (opts.tessellationControlShader && !tctrl) {
            LOGWITH("Given hull shader is invalid");
            return {};
        }
        ID3D11DomainShader* teval = downcastShader<ID3D11DomainShader>(opts.tessellationEvaluationShader);
        if (opts.tessellationEvaluationShader && !teval) {
            LOGWITH("Given domain shader is invalid");
            return {};
        }
        ID3D11GeometryShader* geom = downcastShader<ID3D11GeometryShader>(opts.geometryShader);
        if (opts.geometryShader && !geom) {
            LOGWITH("Given geometry shader is invalid");
            return {};
        }
        ID3D11DepthStencilState* dsState = nullptr;
        D3D11_DEPTH_STENCIL_DESC dsStateInfo{};
        if (opts.depthStencil.depthTest) {
            dsStateInfo.DepthEnable = true;
            dsStateInfo.DepthFunc = (D3D11_COMPARISON_FUNC)opts.depthStencil.comparison;
        }
        dsStateInfo.StencilEnable = opts.depthStencil.stencilTest;
        dsStateInfo.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        dsStateInfo.StencilReadMask = opts.depthStencil.stencilFront.compareMask;
        dsStateInfo.StencilWriteMask = opts.depthStencil.stencilFront.writeMask;

        dsStateInfo.BackFace.StencilFunc = (D3D11_COMPARISON_FUNC)opts.depthStencil.stencilBack.compare;
        dsStateInfo.BackFace.StencilFailOp = (D3D11_STENCIL_OP)opts.depthStencil.stencilBack.onFail;
        dsStateInfo.BackFace.StencilDepthFailOp = (D3D11_STENCIL_OP)opts.depthStencil.stencilBack.onDepthFail;
        dsStateInfo.BackFace.StencilPassOp = (D3D11_STENCIL_OP)opts.depthStencil.stencilBack.onPass;

        dsStateInfo.FrontFace.StencilFunc = (D3D11_COMPARISON_FUNC)opts.depthStencil.stencilFront.compare;
        dsStateInfo.FrontFace.StencilFailOp = (D3D11_STENCIL_OP)opts.depthStencil.stencilFront.onFail;
        dsStateInfo.FrontFace.StencilDepthFailOp = (D3D11_STENCIL_OP)opts.depthStencil.stencilFront.onDepthFail;
        dsStateInfo.FrontFace.StencilPassOp = (D3D11_STENCIL_OP)opts.depthStencil.stencilFront.onPass;

        result = singleton->device->CreateDepthStencilState(&dsStateInfo, &dsState);
        if (result != S_OK) {
            LOGWITH("Failed to create depth stencil state:", result);
            return {};
        }

        D3D11_BLEND_DESC alphaBlendInfo{};
        alphaBlendInfo.AlphaToCoverageEnable = false;
        alphaBlendInfo.IndependentBlendEnable = false;
        for (int i = 0; i < 3; i++) {
            auto& blendop = opts.alphaBlend[i];
            alphaBlendInfo.RenderTarget[i].BlendEnable = blendop != AlphaBlend::overwrite();
            alphaBlendInfo.RenderTarget[i].BlendOp = (D3D11_BLEND_OP)blendop.colorOp;
            alphaBlendInfo.RenderTarget[i].BlendOpAlpha = (D3D11_BLEND_OP)blendop.alphaOp;
            alphaBlendInfo.RenderTarget[i].RenderTargetWriteMask = 0xf;
            alphaBlendInfo.RenderTarget[i].SrcBlend = (D3D11_BLEND)blendop.srcColorFactor;
            alphaBlendInfo.RenderTarget[i].DestBlend = (D3D11_BLEND)blendop.dstColorFactor;
            alphaBlendInfo.RenderTarget[i].SrcBlendAlpha = (D3D11_BLEND)blendop.srcAlphaFactor;
            alphaBlendInfo.RenderTarget[i].DestBlendAlpha = (D3D11_BLEND)blendop.dstAlphaFactor;
        }
        ID3D11BlendState* blendState{};
        result = singleton->device->CreateBlendState(&alphaBlendInfo, &blendState);
        
        if (result != S_OK) {
            LOGWITH("Failed to create blend state:", result);
            dsState->Release();
            return {};
        }

        return singleton->pipelines[key] = new Pipeline(layout, vert, tctrl, teval, geom, frag, dsState, opts.depthStencil.stencilFront.reference, blendState);
    }

    D3D11Machine::Pipeline::Pipeline(ID3D11InputLayout* layout, ID3D11VertexShader* v, ID3D11HullShader* h, ID3D11DomainShader* d, ID3D11GeometryShader* g, ID3D11PixelShader* p, ID3D11DepthStencilState* dss, UINT stencilRef, ID3D11BlendState* blend)
        :vs(v), tcs(h), tes(d), gs(g), fs(p), dsState(dss), stencilRef(stencilRef), layout(layout), blendState(blend) {
    }
    
    D3D11Machine::Pipeline::~Pipeline() {
        blendState->Release();
        dsState->Release();
        layout->Release();
    }


    void D3D11Machine::handle() {
        singleton->loadThread.handleCompleted();
    }

    void D3D11Machine::post(std::function<variant8(void)> exec, std::function<void(variant8)> handler, uint8_t strand) {
        singleton->loadThread.post(exec, handler, strand);
    }

    mat4 D3D11Machine::preTransform() {
        return mat4();
    }

    D3D11Machine::UniformBuffer::UniformBuffer(uint32_t length, ID3D11Buffer* ubo)
        :length(length), ubo(ubo) {

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
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        if (singleton->context->Map(ubo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mappedResource) == S_OK)
        {
            std::memcpy((uint8_t*)mappedResource.pData + offset, input, size);
            singleton->context->Unmap(ubo, 0);
        }
        else {
            LOGWITH("Failed to map memory");
            return;
        }
    }

    D3D11Machine::UniformBuffer* D3D11Machine::createUniformBuffer(int32_t key, const UniformBufferCreationOptions& opts) {
        if (auto ret = getUniformBuffer(key)) { return ret; }
        ID3D11Buffer* buffer{};
        D3D11_BUFFER_DESC bufferInfo{};
        bufferInfo.ByteWidth = opts.size;
        bufferInfo.Usage = D3D11_USAGE_DYNAMIC;
        bufferInfo.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferInfo.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        HRESULT result = singleton->device->CreateBuffer(&bufferInfo, nullptr, &buffer);
        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 buffer:", result);
            reason = result;
            return {};
        }

        return singleton->uniformBuffers[key] = new UniformBuffer(opts.size, buffer);
    }

    D3D11Machine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, ImageSet** sets, ID3D11RenderTargetView** rtvs, ID3D11DepthStencilView* dsv, bool linearSampled, bool depthInput)
        :width(width), height(height), type(type), color1(sets[0]), color2(sets[1]), color3(sets[2]), ds(sets[3]), dset1(rtvs[0]), dset2(rtvs[1]), dset3(rtvs[2]), dsetDS(dsv), linearSampled(linearSampled), depthInput(depthInput) {

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

    D3D11Machine::RenderPass::RenderPass(uint16_t stageCount, bool canBeRead, float* autoclear)
        :stageCount(stageCount), targets(stageCount), pipelines(stageCount), canBeRead(canBeRead), autoclear(false) {
        if (autoclear) {
            this->autoclear = true;
            std::memcpy(clearColor, autoclear, sizeof(clearColor));
        }
    }

    D3D11Machine::RenderPass::~RenderPass() {
        for (RenderTarget* targ : targets) {
            delete targ;
        }
    }

    void D3D11Machine::RenderPass::clear(RenderTargetType toClear, float* colors) {
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
        
        int clearCount = 0;
        if (toClear & 0b1) {
            if (targets[currentPass]) {
                singleton->context->ClearRenderTargetView(targets[currentPass]->dset1, colors);
            }
            else {
                singleton->context->ClearRenderTargetView(singleton->windowSystems[windowIdx]->getSwapchainTarget(), colors);
            }
            colors += 4;
        }
        if (toClear & 0b10) {
            singleton->context->ClearRenderTargetView(targets[currentPass]->dset2, colors);
            colors += 4;
        }
        if (toClear & 0b100) {
            singleton->context->ClearRenderTargetView(targets[currentPass]->dset3, colors);
        }
        if (toClear & 0b11000) {
            if (targets[currentPass]) {
                UINT clearFlags = 0;
                if (toClear & 0b1000) clearFlags |= D3D11_CLEAR_DEPTH;
                if (toClear & 0b10000) clearFlags |= D3D11_CLEAR_STENCIL;
                singleton->context->ClearDepthStencilView(targets[currentPass]->dsetDS, clearFlags, 1.0f, 0);
            }
            else {
                singleton->context->ClearDepthStencilView(singleton->windowSystems[windowIdx]->screenDSView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            }
        }
    }

    void D3D11Machine::RenderPass::start(uint32_t pos) {
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
            WindowSystem* ws = singleton->windowSystems[windowIdx];
            ID3D11RenderTargetView* rtv = ws->getSwapchainTarget();
            singleton->context->OMSetRenderTargets(1, &rtv, ws->screenDSView);
        }
        if (currentPass > 0) {
            RenderTarget* prev = targets[currentPass - 1];
            ID3D11ShaderResourceView* srv[4]{};
            int nim = 0;
            if (prev->color1) {
                srv[nim++] = prev->color1->srv;
                if (prev->color2) {
                    srv[nim++] = prev->color2->srv;
                    if (prev->color3) {
                        srv[nim++] = prev->color3->srv;
                    }
                }
            }
            if (prev->ds && prev->ds->srv) {
                srv[nim++] = prev->ds->srv;
            }
            singleton->context->VSSetShaderResources(pos, nim, srv);
            singleton->context->PSSetShaderResources(pos, nim, srv);
            ID3D11SamplerState* samp[4]{};
            for (int i = 0; i < 4; i++) {
                samp[i] = singleton->nearestBorderSampler;
            }
            singleton->context->VSSetSamplers(pos, nim, samp);
            singleton->context->PSSetSamplers(pos, nim, samp);
        }
        singleton->context->RSSetViewports(1, &viewport);
        singleton->context->RSSetScissorRects(1, &scissor);
        usePipeline(pipelines[currentPass], currentPass);
        if (autoclear) {
            if (targets[currentPass] == nullptr) {
                WindowSystem* ws = singleton->windowSystems[windowIdx];
                singleton->context->ClearRenderTargetView(ws->getSwapchainTarget(), clearColor);
                singleton->context->ClearDepthStencilView(ws->screenDSView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
                return;
            }
            if(targets[currentPass]->dsetDS) singleton->context->ClearDepthStencilView(targets[currentPass]->dsetDS, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            if (targets[currentPass]->dset1) {
                singleton->context->ClearRenderTargetView(targets[currentPass]->dset1, clearColor);
                if (targets[currentPass]->dset2) {
                    singleton->context->ClearRenderTargetView(targets[currentPass]->dset2, clearColor);
                    if (targets[currentPass]->dset3) {
                        singleton->context->ClearRenderTargetView(targets[currentPass]->dset3, clearColor);
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
            singleton->context->OMSetDepthStencilState(pipelines[currentPass]->dsState, pipeline->stencilRef);
            singleton->context->OMSetBlendState(pipeline->blendState, nullptr, 0xffffffff);
        }
    }

    void D3D11Machine::RenderPass::push(void* input, uint32_t start, uint32_t end) {
        singleton->uniformBuffers[INT32_MIN + 1]->update(input, 0, start, end - start);
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos) {
        if (ubPos == 13) {
            LOGWITH("Invalid call: bind pos 13 is reserved for push()");
            return;
        }
        if (currentPass >= 0) {
            singleton->context->VSSetConstantBuffers(pos, 1, &ub->ubo);
            singleton->context->PSSetConstantBuffers(pos, 1, &ub->ubo); // �ӽ�
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, const pTexture& tx) {
        if (currentPass >= 0) {
            singleton->context->VSSetShaderResources(pos, 1, &tx->dset);
            singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // �ӽ�
            singleton->context->PSSetSamplers(pos, 1, tx->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, const pStreamTexture& tx) {
        if (currentPass >= 0) {
            singleton->context->VSSetShaderResources(pos, 1, &tx->dset);
            singleton->context->PSSetShaderResources(pos, 1, &tx->dset); // �ӽ�
            singleton->context->PSSetSamplers(pos, 1, tx->linearSampled ? &singleton->linearBorderSampler : &singleton->nearestBorderSampler);
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, RenderPass* prev) {
        if (currentPass >= 0) {
            if (auto targ = prev->targets.back()) {
                ID3D11ShaderResourceView* srv[4]{};
                int nim = 0;
                if (targ->color1) {
                    srv[nim++] = targ->color1->srv;
                    if (targ->color2) {
                        srv[nim++] = targ->color2->srv;
                        if (targ->color3) {
                            srv[nim++] = targ->color3->srv;
                        }
                    }
                }
                if (targ->ds && targ->ds->srv) {
                    srv[nim++] = targ->ds->srv;
                }
                singleton->context->VSSetShaderResources(pos, nim, srv);
                singleton->context->PSSetShaderResources(pos, nim, srv);
                ID3D11SamplerState* samp[4]{};
                for (int i = 0; i < 4; i++) {
                    samp[i] = targ->linearSampled ? singleton->linearBorderSampler : singleton->nearestBorderSampler;
                }
                singleton->context->VSSetSamplers(pos, nim, samp);
                singleton->context->PSSetSamplers(pos, nim, samp);
            }
            else {
                LOGWITH("Can\'t bind renderpas2screen");
            }
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::bind(uint32_t pos, RenderPass2Cube* prev) {
        if (currentPass >= 0) {
            singleton->context->VSSetShaderResources(pos, 1, &prev->srv);
            singleton->context->PSSetShaderResources(pos, 1, &prev->srv);
            ID3D11SamplerState* samp[1]{};
            for (int i = 0; i < 1; i++) {
                samp[i] = singleton->linearBorderSampler;
            }
            singleton->context->VSSetSamplers(pos, 1, samp);
            singleton->context->PSSetSamplers(pos, 1, samp);
        }
        else {
            LOGWITH("No subpass is running");
        }
    }

    void D3D11Machine::RenderPass::invoke(const pMesh& mesh, uint32_t start, uint32_t count) {
        if (bound != mesh.get()) {
            UINT offset = 0;
            singleton->context->IASetVertexBuffers(0, 1, &mesh->vb, &mesh->vStride, &offset);
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
        ID3D11Buffer* buf[2] = { mesh->vb };
        UINT strides[2] = { mesh->vStride };
        if (instanceInfo) {
            buf[1] = instanceInfo->vb;
            strides[1] = instanceInfo->vStride;
        }
        UINT offsets[2]{};
        singleton->context->IASetVertexBuffers(0, instanceInfo ? 2 : 1, buf, strides, offsets);
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
            singleton->windowSystems[windowIdx]->swapchain.handle->Present(singleton->vsync, 0);
        }
        currentPass = -1;
        currentRenderPass = 0;
    }

    bool D3D11Machine::RenderPass::wait(uint64_t) {
        return true;
    }

    void D3D11Machine::RenderPass::resize(int width, int height, bool linearSampled) {
        if (is4Screen) {
            LOGWITH("RenderPass2Screen cannot be resized with this function. Resize the window for that purpose.");
            return;
        }
        if (targets[0] && targets[0]->width == width && targets[0]->height == height) { // equal size
            return;
        }
        for (uint32_t i = 0; i < stageCount; i++) {            
            RenderTargetType rtype = targets[i]->type;
            bool ditype = targets[i]->depthInput;
            delete targets[i];
            targets[i] = createRenderTarget2D(width, height, rtype, ditype, true, linearSampled);
            if (!targets[i]) {
                LOGHERE;
                for (RenderTarget*& tg : targets) {
                    delete tg;
                    tg = nullptr;
                }
                return;
            }
        }
        setViewport(width, height, 0.0f, 0.0f);
        setScissor(width, height, 0, 0);
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

    D3D11Machine::pTexture D3D11Machine::RenderPass::copy2Texture(int32_t key, const RenderTarget2TextureOptions& opts) {
        if (getTexture(key)) {
            LOGWITH("Invalid key");
            return {};
        }
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return {};
        }
        RenderTarget* targ = targets.back();
        ImageSet* srcSet{};
        if (opts.index < 3) {
            ImageSet* sources[] = { targ->color1,targ->color2,targ->color3 };
            srcSet = sources[opts.index];
        }
        if (!srcSet) {
            LOGWITH("Invalid index");
            return {};
        }
        D3D11_TEXTURE2D_DESC texInfo{};
        texInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texInfo.ArraySize = 1;
        texInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texInfo.SampleDesc.Count = 1;
        texInfo.MipLevels = 1;
        if (opts.area.width && opts.area.height) {
            texInfo.Width = opts.area.width;
            texInfo.Height = opts.area.height;
        }
        else {
            texInfo.Width = targ->width;
            texInfo.Height = targ->height;
        }
        texInfo.Usage = D3D11_USAGE_DEFAULT;

        ID3D11Texture2D* newTex{};
        HRESULT result = singleton->device->CreateTexture2D(&texInfo, nullptr, &newTex);
        if (!SUCCEEDED(result)) {
            LOGWITH("Failed to create new texture:", result);
            reason = result;
            return {};
        }

        if (opts.area.width && opts.area.height) {
            D3D11_BOX srcBox;
            srcBox.left = opts.area.x;
            srcBox.top = opts.area.y;
            srcBox.front = 0;
            srcBox.back = 1;
            srcBox.right = opts.area.x + opts.area.width;
            srcBox.bottom = opts.area.y + opts.area.height;
            singleton->context->CopySubresourceRegion(newTex, 0, 0, 0, 0, srcSet->tex, 0, &srcBox);
        }
        else {
            singleton->context->CopyResource(newTex, srcSet->tex);
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC srvInfo{};
        srvInfo.Format = texInfo.Format;
        srvInfo.Texture2D.MipLevels = 1;
        srvInfo.Texture2D.MostDetailedMip = 0;
        srvInfo.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        ID3D11ShaderResourceView* newSrv{};
        result = singleton->device->CreateShaderResourceView(newTex, &srvInfo, &newSrv);
        if (!SUCCEEDED(result)) {
            LOGWITH("Failed to create new shader resource view:", result);
            newTex->Release();
            return {};
        }
        struct txtr :public Texture { inline txtr(ID3D11Resource* _1, ID3D11ShaderResourceView* _2, uint16_t _3, uint16_t _4, bool _5, bool _6) :Texture(_1, _2, _3, _4, _5, _6) {} };
        pTexture ret = std::make_shared<txtr>(newTex, newSrv, targ->width, targ->height, false, opts.linearSampled);
        if (key == INT32_MIN) return ret;
        return singleton->textures[key] = ret;
    }

    void D3D11Machine::RenderPass::asyncCopy2Texture(int32_t key, std::function<void(variant8)> handler, const RenderTarget2TextureOptions& opts) {
        if (getTexture(key)) {
            LOGWITH("Invalid key");
            return;
        }
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return;
        }
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        uint32_t index = opts.index;
        bool linear = opts.linearSampled;
        RenderTarget* targ = targets.back();
        ImageSet* srcSet{};
        if (index < 3) {
            ImageSet* sources[] = { targ->color1,targ->color2,targ->color3 };
            srcSet = sources[index];
        }
        if (!srcSet) {
            LOGWITH("Invalid index");
            return;
        }
        singleton->loadThread.post([this, key, index, linear]() {
            RenderTarget* targ = targets.back();
            D3D11_TEXTURE2D_DESC texInfo{};
            texInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texInfo.ArraySize = 1;
            texInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            texInfo.SampleDesc.Count = 1;
            texInfo.MipLevels = 1;
            texInfo.Width = targ->width;
            texInfo.Height = targ->height;
            texInfo.Usage = D3D11_USAGE_DEFAULT;

            ID3D11Texture2D* newTex{};
            reason = singleton->device->CreateTexture2D(&texInfo, nullptr, &newTex);
            return variant8(newTex);
            }, [key, this, targ, srcSet, handler, linear](variant8 param) {
                ID3D11Texture2D* newTex = (ID3D11Texture2D*)param.vp;
                if (!newTex) {
                    variant8 par2;
                    par2.bytedata4[0] = key;
                    par2.bytedata4[1] = E_FAIL;
                    if (handler) handler(par2);
                    return;
                }
                
                singleton->context->CopyResource(newTex, srcSet->tex);
                D3D11_SHADER_RESOURCE_VIEW_DESC srvInfo{};
                srvInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                srvInfo.Texture2D.MipLevels = 1;
                srvInfo.Texture2D.MostDetailedMip = 0;
                srvInfo.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                ID3D11ShaderResourceView* newSrv{};
                reason = singleton->device->CreateShaderResourceView(newTex, &srvInfo, &newSrv);
                if (!SUCCEEDED(reason)) {
                    variant8 par2;
                    par2.bytedata4[0] = key;
                    par2.bytedata4[1] = reason;
                    if (handler) handler(par2);
                    return;
                }
                struct txtr :public Texture { inline txtr(ID3D11Resource* _1, ID3D11ShaderResourceView* _2, uint16_t _3, uint16_t _4, bool _5, bool _6) :Texture(_1, _2, _3, _4, _5, _6) {} };
                singleton->textures[key] = std::make_shared<txtr>(newTex, newSrv, targ->width, targ->height, false, linear);
                variant8 par2;
                par2.bytedata4[0] = key;
                if(handler) handler(par2);
         });
    }
    
    std::unique_ptr<uint8_t[]> D3D11Machine::RenderPass::readBack(uint32_t index, const TextureArea2D& area) {
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return {};
        }
        RenderTarget* targ = targets.back();
        ImageSet* srcSet{};
        if (index < 3) {
            ImageSet* sources[] = { targ->color1,targ->color2,targ->color3 };
            srcSet = sources[index];
        }
        if (!srcSet) {
            LOGWITH("Invalid index");
            return {};
        }
        bool subArea = area.width && area.height;
        uint32_t width, height, x, y;
        if (subArea) {
            width = area.width;
            height = area.height;
            x = area.x;
            y = area.y;
        }
        else {
            width = targ->width;
            height = targ->height;
            x = 0;
            y = 0;
        }
        D3D11_TEXTURE2D_DESC texInfo{};
        texInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texInfo.ArraySize = 1;
        texInfo.BindFlags = 0;
        texInfo.SampleDesc.Count = 1;
        texInfo.MipLevels = 1;
        texInfo.Width = width;
        texInfo.Height = height;
        texInfo.Usage = D3D11_USAGE_STAGING;
        texInfo.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        ID3D11Texture2D* newTex{};
        HRESULT result = singleton->device->CreateTexture2D(&texInfo, nullptr, &newTex);
        if (!SUCCEEDED(result)) {
            LOGWITH("Failed to create staging target:", result);
            reason = result;
            return {};
        }
        if (subArea) {
            D3D11_BOX srcBox;
            srcBox.left = x;
            srcBox.top = y;
            srcBox.front = 0;
            srcBox.back = 1;
            srcBox.right = x + width;
            srcBox.bottom = y + height;
            singleton->context->CopySubresourceRegion(newTex, 0, 0, 0, 0, srcSet->tex, 0, &srcBox);
        }
        else {
            singleton->context->CopyResource(newTex, srcSet->tex);
        }
        std::unique_ptr<uint8_t[]> up(new uint8_t[width * height * 4]);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        result = singleton->context->Map(newTex, 0, D3D11_MAP_READ, 0, &mapped);
        if (!SUCCEEDED(result)) {
            LOGWITH("Failed to map staging target:", result);
            reason = result;
            newTex->Release();
            return {};
        }
        uint8_t* srcPos = (uint8_t*)mapped.pData;
        uint8_t* dstPos = up.get();
        for (uint32_t i = 0; i < height; i++) {
            std::memcpy(dstPos, srcPos, width * 4);
            srcPos += mapped.RowPitch;
            dstPos += width * 4;
        }
        singleton->context->Unmap(newTex, 0);
        newTex->Release();
        return up;
    }

    void D3D11Machine::RenderPass::asyncReadBack(int32_t key, uint32_t index, std::function<void(variant8)> handler, const TextureArea2D& area) {
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return;
        }
        RenderTarget* targ = targets.back();
        ImageSet* srcSet{};
        if (index < 3) {
            ImageSet* sources[] = { targ->color1,targ->color2,targ->color3 };
            srcSet = sources[index];
        }
        if (!srcSet) {
            LOGWITH("Invalid index");
            return;
        }
        bool subArea = area.width && area.height;
        uint32_t w, h, x, y;
        if (subArea) {
            w = area.width;
            h = area.height;
            x = area.x;
            y = area.y;
        }
        else {
            w = targ->width;
            h = targ->height;
            x = 0;
            y = 0;
        }
        uint32_t dataSize = w * h * 4;
        singleton->loadThread.post([this, w, h]() {
            D3D11_TEXTURE2D_DESC texInfo{};
            texInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texInfo.ArraySize = 1;
            texInfo.BindFlags = 0;
            texInfo.SampleDesc.Count = 1;
            texInfo.MipLevels = 1;
            texInfo.Width = w;
            texInfo.Height = h;
            texInfo.Usage = D3D11_USAGE_STAGING;
            texInfo.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Texture2D* newTex{};
            HRESULT result = singleton->device->CreateTexture2D(&texInfo, nullptr, &newTex);
            if (!SUCCEEDED(result)) {
                LOGWITH("Failed to create staging target:", result);
                reason = result;
                return variant8(nullptr);
            }
            return variant8(newTex);
            }, [this, key, srcSet, handler, dataSize, w, h, x, y](variant8 param) {
                ID3D11Texture2D* newTex = (ID3D11Texture2D*)param.vp;
                if (!newTex) {
                    variant8 par2;
                    par2.bytedata4[0] = key;
                    par2.bytedata4[1] = E_FAIL;
                    handler(par2);
                    return;
                }
                D3D11_BOX srcBox;
                srcBox.left = x;
                srcBox.top = y;
                srcBox.front = 0;
                srcBox.back = 1;
                srcBox.right = x + w;
                srcBox.bottom = y + h;
                singleton->context->CopySubresourceRegion(newTex, 0, 0, 0, 0, srcSet->tex, 0, &srcBox);
                D3D11_MAPPED_SUBRESOURCE mapped;
                HRESULT result = singleton->context->Map(newTex, 0, D3D11_MAP_READ, 0, &mapped);
                if (!SUCCEEDED(result)) {
                    newTex->Release();
                    variant8 par2;
                    par2.bytedata4[0] = key;
                    par2.bytedata4[1] = result;
                    handler(par2);
                    return;
                }
                singleton->loadThread.post([key, mapped, newTex, dataSize, w, h]() {
                    uint8_t* up = new uint8_t[dataSize];
                    uint8_t* srcPos = (uint8_t*)mapped.pData;
                    uint8_t* dstPos = up;
                    for (uint32_t i = 0; i < h; i++) {
                        std::memcpy(dstPos, srcPos, w * 4);
                        srcPos += mapped.RowPitch;
                        dstPos += w * 4;
                    }
                    ReadBackBuffer* result = new ReadBackBuffer;
                    result->data = up;
                    result->key = key;
                    return variant8(result);
                }, [newTex, handler](variant8 param) {
                    singleton->context->Unmap(newTex, 0);
                    newTex->Release();
                    if (handler) handler(param);
                    ReadBackBuffer* result = (ReadBackBuffer*)param.vp;
                    delete result;
                });
            });
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

    D3D11Machine::Mesh::Mesh(ID3D11Buffer* vb, ID3D11Buffer* ib, DXGI_FORMAT indexFormat, size_t vcount, size_t icount, UINT vStride)
        :vb(vb), ib(ib), indexFormat(indexFormat), vcount(vcount), icount(icount), vStride(vStride) {}

    D3D11Machine::Mesh::~Mesh() {
        if(vb) vb->Release();
        if (ib) ib->Release();
    }

    void D3D11Machine::Mesh::update(const void* input, uint32_t offset, uint32_t size) {
        // offset ������ ���ֱ� (d3d11 dynamic������ ��ǻ� offset�� �������ϱ� ����)
        D3D11_MAPPED_SUBRESOURCE subresource;
        HRESULT hr = singleton->context->Map(vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
        if (!SUCCEEDED(hr)) {
            LOGWITH("Failed to map memory");
            return;
        }
        std::memcpy((uint8_t*)subresource.pData + offset, input, size);
        singleton->context->Unmap(vb, 0);
    }

    void D3D11Machine::Mesh::updateIndex(const void* input, uint32_t offset, uint32_t size) {
        // �� �÷��� offset ������ ���ֱ�, vulkan�� dynamic ub ���� (d3d11 dynamic������ ��ǻ� offset�� �������ϱ� ����)
        if (!ib) { return; }
        D3D11_MAPPED_SUBRESOURCE subresource;
        HRESULT hr = singleton->context->Map(ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
        if (!SUCCEEDED(hr)) {
            LOGWITH("Failed to map memory");
            return;
        }
        std::memcpy((uint8_t*)subresource.pData + offset, input, size);
        singleton->context->Unmap(ib, 0);
    }

    bool isThisFormatAvailable(ID3D11Device* device, DXGI_FORMAT format, UINT flags) {
        D3D11_FEATURE_DATA_FORMAT_SUPPORT formatSupport{};
        formatSupport.InFormat = format;
        if (device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport)) == S_OK) {
            return (formatSupport.OutFormatSupport & flags) == flags;
        }
        return false;
    }

    DXGI_FORMAT textureFormatFallback(ID3D11Device* device, uint32_t nChannels, bool srgb, D3D11Machine::TextureFormatOptions hq, UINT flags) {
#define CHECK_N_RETURN(f) if(isThisFormatAvailable(device,f,flags)) return f
        switch (nChannels)
        {
        case 4:
            if (srgb) {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                    CHECK_N_RETURN(DXGI_FORMAT_BC3_UNORM_SRGB);
                }
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            }
            else {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM);
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM);
                    CHECK_N_RETURN(DXGI_FORMAT_BC3_UNORM);
                }
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            break;
        case 3:
            if (srgb) {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                    CHECK_N_RETURN(DXGI_FORMAT_BC1_UNORM_SRGB);
                }
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            else {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM);
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM);
                    CHECK_N_RETURN(DXGI_FORMAT_BC1_UNORM);
                }
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            }
        case 2:
            if (srgb) {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                }
                return DXGI_FORMAT_R8G8_UNORM; // SRGB �Ⱥ���..
            }
            else {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC7_UNORM_SRGB);
                    CHECK_N_RETURN(DXGI_FORMAT_BC5_UNORM);
                }
                return DXGI_FORMAT_R8G8_UNORM;
            }
        case 1:
            if (srgb) {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    // �뷮�� �پ��� ������ ����
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    // �뷮�� �پ��� ������ ����
                }
                return DXGI_FORMAT_R8_UNORM;
            }
            else {
                if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_QUALITY) {
                    // �뷮�� �پ��� ������ ����
                }
                else if (hq == D3D11Machine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                    CHECK_N_RETURN(DXGI_FORMAT_BC4_UNORM);
                }
                return DXGI_FORMAT_R8_UNORM;
            }
        default:
            return DXGI_FORMAT_UNKNOWN;
        }
#undef CHECK_N_RETURN
    }

    ID3DBlob* compileShader(const char* code, size_t size, D3D11Machine::ShaderStage type) {
        ID3DBlob* ret{};
        ID3DBlob* result{};
        char targ[] = { 'v','s','_','5','_','0',0 };
        switch (type)
        {
        case onart::D3D11Machine::ShaderStage::VERTEX:
            targ[0] = 'v';
            break;
        case onart::D3D11Machine::ShaderStage::FRAGMENT:
            targ[0] = 'p';
            break;
        case onart::D3D11Machine::ShaderStage::GEOMETRY:
            targ[0] = 'g';
            break;
        case onart::D3D11Machine::ShaderStage::TESS_CTRL:
            targ[0] = 'h';
            break;
        case onart::D3D11Machine::ShaderStage::TESS_EVAL:
            targ[0] = 'd';
            break;
        default:
            break;
        }
        HRESULT res = D3DCompile(code, size, toString((void*)code).c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", targ, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &ret, &result);
        if (res != S_OK) {
            LOGWITH((char*)result->GetBufferPointer());
            result->Release();
        }
        return ret;
    }
}