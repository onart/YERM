#include "yr_d3d11.h"
#include "yr_sys.h"

#include <comdef.h>

#pragma comment(lib, "dxgi.lib")

namespace onart {
	D3D11Machine* D3D11Machine::singleton = nullptr;
	thread_local unsigned D3D11Machine::reason = 0;

    D3D11Machine::D3D11Machine(Window* window) {
        if (singleton) {
            LOGWITH("Tried to create multiple GLMachine objects");
            return;
        }

        constexpr D3D_FEATURE_LEVEL FEATURE_LEVELS[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL lv = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_1_0_CORE;
        
        D3D11CreateDevice(
            nullptr,
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

        int x, y;
        window->getFramebufferSize(&x, &y);

        createSwapchain(x, y, window);

        singleton = this;
    }

    void D3D11Machine::createSwapchain(uint32_t width, uint32_t height, Window* window) {

        if (swapchain) {
            swapchain->ResizeBuffers(2, width, height, DXGI_FORMAT_UNKNOWN, 0);
            return;
        }

        // https://learn.microsoft.com/ko-kr/windows/win32/api/dxgi/ns-dxgi-dxgi_swap_chain_desc?redirectedfrom=MSDN
        // https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/bb173064(v=vs.85)
        IDXGIDevice* dxgiDevice{};
        IDXGIAdapter* dxgiAdapter{};
        IDXGIFactory* dxgiFactory{};

        HRESULT result{};

        result = device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
        if (!dxgiDevice) {
            _com_error err(result);
            LOGWITH("Failed to query dxgi device:", result, err.ErrorMessage());
            return;
        }
        result = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
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
        swapchainInfo.BufferDesc.RefreshRate.Numerator = 60;
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
}