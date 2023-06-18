#include "yr_webgpu.h"

namespace onart{
    WGMachine* WGMachine::singleton = nullptr;

    struct spin_t{
        WGMachine* temp;
        int spin;
    };

    struct unlocker{ 
        public:
            inline unlocker(int& targ):targ(targ) {}
            inline ~unlocker(){ targ=1; }
        private:
            int& targ;
    };

    WGMachine::WGMachine(Window*){
        WGpuRequestAdapterOptions options = {};
        options.powerPreference = WGPU_POWER_PREFERENCE_LOW_POWER;
        spin_t spin{this, 0};
        if(navigator_gpu_request_adapter_async(&options, onGetWebGpuAdapter, &spin) != EM_TRUE){
            LOGWITH("Can\'t use webgpu");
            return;
        }
        while(!spin.spin);
        spin.spin = 0;
        if(!adapter || !device || !queue || !canvas){
            return;
        }

    }

    void WGMachine::onGetWebGpuAdapter(WGpuAdapter adapter, void* sync){
        spin_t* spin = reinterpret_cast<spin_t*>(sync);
        spin->temp->adapter = adapter;
        if(!adapter){ 
            LOGWITH("Failed to get WebGPU adapter");
            spin->spin = 1;
            return;
        }
        WGpuDeviceDescriptor deviceDesc = {};
        // https://gpuweb.github.io/gpuweb/#gpudevicedescriptor
        
        deviceDesc.requiredFeatures = wgpu_adapter_or_device_get_features(adapter); // 가능 피처 모두 활성화
        wgpu_adapter_or_device_get_limits(adapter, &spin->temp->limits);
        wgpu_adapter_request_device_async(adapter, &deviceDesc, onGetWebGpuDevice, sync);
    }

    void WGMachine::onGetWebGpuDevice(WGpuDevice device, void *sync){
        spin_t* spin = reinterpret_cast<spin_t*>(sync);
        spin->temp->device = device;
        unlocker _(spin->spin);
        if(!device){
            LOGWITH("Failed to get WebGPU device");
            return;
        }
        spin->temp->queue = wgpu_device_get_queue(device);
        if(!spin->temp->queue){
            LOGWITH("Failed to get WebGPU device queue");
            return;
        }
        spin->temp->canvas = wgpu_canvas_get_webgpu_context("canvas");
        if(!spin->temp->canvas){
            LOGWITH("Failed to get HTML canvas context");
            return;
        }
        // https://gpuweb.github.io/gpuweb/#dom-gpucanvascontext-configure
        WGpuCanvasConfiguration config{};
        config.colorSpace = HTML_PREDEFINED_COLOR_SPACE_SRGB;
        config.alphaMode = WGPU_CANVAS_ALPHA_MODE_PREMULTIPLIED;
        config.device = device;
        config.format=  navigator_gpu_get_preferred_canvas_format();
        config.usage = WGPU_TEXTURE_USAGE_RENDER_ATTACHMENT;
        wgpu_canvas_context_configure(spin->temp->canvas, &config);
    }
}