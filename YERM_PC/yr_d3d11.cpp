#include "yr_d3d11.h"
#include "yr_sys.h"

#include <comdef.h>

#pragma comment(lib, "dxgi.lib")

namespace onart {
	D3D11Machine* D3D11Machine::singleton = nullptr;
	thread_local unsigned D3D11Machine::reason = 0;

    static uint64_t assessAdapter(IDXGIAdapter*);

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

        selectedAdapter->Release();

        if (result != S_OK) {
            LOGWITH("Failed to create d3d11 device:", result);
            return;
        }

        int x, y;
        window->getFramebufferSize(&x, &y);

        createSwapchain(x, y, window);
        if (!swapchain) {
            LOGWITH("Failed to create swapchain");
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
            return;
        }
        result = dxgiDevice->GetAdapter(&dxgiAdapter);
        if (!dxgiAdapter) {
            _com_error err(result);
            LOGWITH("Failed to query dxgi adapter:", result, err.ErrorMessage());
            return;
        }
        result = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);
        if (!dxgiFactory) {
            _com_error err(result);
            LOGWITH("Failed to query dxgi factory:", result, err.ErrorMessage());
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
                score |= 1LL << 53;
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
            return {};
        }

        if (idata) {
            bufferInfo.ByteWidth = isize * icount;
            bufferInfo.BindFlags = D3D11_BIND_INDEX_BUFFER;
            HRESULT result = singleton->device->CreateBuffer(&bufferInfo, &vertexData, &ib);
            if (result != S_OK) {
                LOGWITH("Failed to create index buffer:", result);
                vb->Release();
                return {};
            }
        }

        struct publicmesh :public Mesh { publicmesh(ID3D11Buffer* _1, ID3D11Buffer* _2) :Mesh(_1,_2) {} };
        ret = std::make_shared<publicmesh>(vb, ib);
        return ret;
    }
}