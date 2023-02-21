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

#include "yr_vulkan.h"
#include "logger.hpp"
#include "yr_sys.h"

#include "../externals/boost/predef/platform.h"

#if !BOOST_PLAT_ANDROID
#define KHRONOS_STATIC
#endif
#include "../externals/ktx/ktx.h"

#include <algorithm>
#include <vector>

namespace onart {

    /// @brief Vulkan 인스턴스를 생성합니다. 자동으로 호출됩니다.
    static VkInstance createInstance(Window*);
    /// @brief 사용할 Vulkan 물리 장치를 선택합니다. CPU 기반인 경우 경고를 표시하지만 선택에 실패하지는 않습니다.
    static VkPhysicalDevice findPhysicalDevice(VkInstance, VkSurfaceKHR, bool*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint64_t*);
    /// @brief 주어진 Vulkan 물리 장치에 대한 우선도를 매깁니다. 높을수록 좋게 취급합니다. 대부분의 경우 물리 장치는 하나일 것이므로 함수가 아주 중요하지는 않을 거라 생각됩니다.
    static uint64_t assessPhysicalDevice(VkPhysicalDevice);
    /// @brief 주어진 장치에 대한 가상 장치를 생성합니다.
    static VkDevice createDevice(VkPhysicalDevice, int, int, int, int);
    /// @brief 주어진 장치에 대한 메모리 관리자를 세팅합니다.
    static VmaAllocator createAllocator(VkInstance, VkPhysicalDevice, VkDevice);
    /// @brief 명령 풀을 생성합니다.
    static VkCommandPool createCommandPool(VkDevice, int qIndex);
    /// @brief 이미지로부터 뷰를 생성합니다.
    static VkImageView createImageView(VkDevice, VkImage, VkImageViewType, VkFormat, int, int, VkImageAspectFlags, VkComponentMapping={});
    /// @brief 주어진 만큼의 기술자 집합을 할당할 수 있는 기술자 풀을 생성합니다.
    static VkDescriptorPool createDescriptorPool(VkDevice device, uint32_t samplerLimit = 256, uint32_t dynUniLimit = 8, uint32_t uniLimit = 16, uint32_t intputAttachmentLimit = 16);
    /// @brief 주어진 기반 형식과 아귀가 맞는, 현재 장치에서 사용 가능한 압축 형식을 리턴합니다.
    static VkFormat textureFormatFallback(VkPhysicalDevice physicalDevice, int x, int y, uint32_t nChannels, bool srgb, bool hq, VkImageCreateFlagBits flags);
    /// @brief 파이프라인을 주어진 옵션에 따라 생성합니다.
    static VkPipeline createPipeline(VkDevice device, VkVertexInputAttributeDescription* vinfo, uint32_t size, uint32_t vattr, VkVertexInputAttributeDescription* iinfo, uint32_t isize, uint32_t iattr, VkRenderPass pass, uint32_t subpass, uint32_t flags, const uint32_t OPT_COLOR_COUNT, const bool OPT_USE_DEPTHSTENCIL, VkPipelineLayout layout, VkShaderModule vs, VkShaderModule fs, VkShaderModule tc, VkShaderModule te, VkShaderModule gs, VkStencilOpState* front, VkStencilOpState* back);
    /// @brief VkResult를 스트링으로 표현합니다. 리턴되는 문자열은 텍스트(코드) 영역에 존재합니다.
    inline static const char* resultAsString(VkResult);

    /// @brief 활성화할 장치 확장
    constexpr const char* VK_DESIRED_DEVICE_EXT[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };


    VkMachine* VkMachine::singleton = nullptr;

    VkMachine::VkMachine(Window* window){
        if(singleton) {
            LOGWITH("Tried to create multiple VkMachine objects");
            return;
        }

        if(!(instance = createInstance(window))) {
            return;
        }

        VkResult result;
        if((result = window->createWindowSurface(instance, &surface.handle)) != VK_SUCCESS){
            LOGWITH("Failed to create Window surface:", result,resultAsString(result));
            free();
            return;
        }

        bool isCpu;
        if(!(physicalDevice.card = findPhysicalDevice(instance, surface.handle, &isCpu, &physicalDevice.gq, &physicalDevice.pq, &physicalDevice.subq, &physicalDevice.subqIndex, &physicalDevice.minUBOffsetAlignment))) { // TODO: 모든 가용 graphics/transfer 큐 정보를 저장해 두고 버퍼/텍스처 등 자원 세팅은 다른 큐를 사용하게 만들자
            LOGWITH("Couldn\'t find any appropriate graphics device");
            free();
            return;
        }
        if(isCpu) LOGWITH("Warning: this device is CPU");
        // properties.limits.minMemorymapAlignment, minTexelBufferOffsetAlignment, minUniformBufferOffsetAlignment, minStorageBufferOffsetAlignment, optimalBufferCopyOffsetAlignment, optimalBufferCopyRowPitchAlignment를 저장

        vkGetPhysicalDeviceFeatures(physicalDevice.card, &physicalDevice.features);

        checkSurfaceHandle();

        if(!(device = createDevice(physicalDevice.card, physicalDevice.gq, physicalDevice.pq, physicalDevice.subq, physicalDevice.subqIndex))) {
            free();
            return;
        }

        vkGetDeviceQueue(device, physicalDevice.gq, 0, &graphicsQueue);
        vkGetDeviceQueue(device, physicalDevice.pq, 0, &presentQueue);
        vkGetDeviceQueue(device, physicalDevice.subq, physicalDevice.subqIndex, &transferQueue);
        gqIsTq = (graphicsQueue == transferQueue);
        pqIsTq = (graphicsQueue == transferQueue);
        //LOGRAW(physicalDevice.subqIndex, graphicsQueue, transferQueue, presentQueue);
        //LOGRAW(physicalDevice.gq, physicalDevice.pq, physicalDevice.subq);

        if(!(allocator = createAllocator(instance, physicalDevice.card, device))){
            free();
            return;
        }

        if(!(gCommandPool = createCommandPool(device, physicalDevice.gq))){
            free();
            return;
        }
        if(!(tCommandPool = createCommandPool(device, physicalDevice.subq))){
            free();
            return;
        }

        allocateCommandBuffers(sizeof(baseBuffer)/sizeof(baseBuffer[0]), true, true, baseBuffer);
        if(!baseBuffer[0]){
            free();
        }
        int w,h;
        window->getSize(&w,&h);
        createSwapchain(w, h);

        if(!(descriptorPool = createDescriptorPool(device))){
            free();
            return;
        }

        if(!createLayouts() || !createSamplers()){
            free();
            return;
        }

        singleton = this;
    }

    VkFence VkMachine::createFence(bool signaled){
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if(signaled) fenceInfo.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence ret;
        VkResult result = vkCreateFence(device, &fenceInfo, VK_NULL_HANDLE, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create fence:",result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        return ret;
    }

    VkSemaphore VkMachine::createSemaphore(){
        VkSemaphoreCreateInfo smInfo{};
        smInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore ret;
        VkResult result = vkCreateSemaphore(device, &smInfo, VK_NULL_HANDLE, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create fence:",result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        return ret;
    }

    VkPipeline VkMachine::getPipeline(int32_t name){
        auto it = singleton->pipelines.find(name);
        if(it != singleton->pipelines.end()) return it->second;
        else return VK_NULL_HANDLE;
    }

    VkPipelineLayout VkMachine::getPipelineLayout(int32_t name){
        auto it = singleton->pipelineLayouts.find(name);
        if(it != singleton->pipelineLayouts.end()) return it->second;
        else return VK_NULL_HANDLE;
    }

    VkMachine::pMesh VkMachine::getMesh(int32_t name) {
        auto it = singleton->meshes.find(name);
        if(it != singleton->meshes.end()) return it->second;
        else return pMesh();
    }

    VkMachine::RenderTarget* VkMachine::getRenderTarget(int32_t name){
        auto it = singleton->renderTargets.find(name);
        if(it != singleton->renderTargets.end()) return it->second;
        else return nullptr;
    }

    VkMachine::UniformBuffer* VkMachine::getUniformBuffer(int32_t name){
        auto it = singleton->uniformBuffers.find(name);
        if(it != singleton->uniformBuffers.end()) return it->second;
        else return nullptr;
    }

    VkMachine::RenderPass2Screen* VkMachine::getRenderPass2Screen(int32_t name){
        auto it = singleton->finalPasses.find(name);
        if(it != singleton->finalPasses.end()) return it->second;
        else return nullptr;
    }

    VkMachine::RenderPass* VkMachine::getRenderPass(int32_t name){
        auto it = singleton->renderPasses.find(name);
        if(it != singleton->renderPasses.end()) return it->second;
        else return nullptr;
    }

    VkMachine::RenderPass2Cube* VkMachine::getRenderPass2Cube(int32_t name){
        auto it = singleton->cubePasses.find(name);
        if(it != singleton->cubePasses.end()) return it->second;
        else return nullptr;
    }

    VkShaderModule VkMachine::getShader(int32_t name){
        auto it = singleton->shaders.find(name);
        if(it != singleton->shaders.end()) return it->second;
        else return VK_NULL_HANDLE;
    }

    VkMachine::pTexture VkMachine::getTexture(int32_t name, bool lock){
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

    void VkMachine::allocateCommandBuffers(int count, bool isPrimary, bool fromGraphics, VkCommandBuffer* buffers){
        VkCommandBufferAllocateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.level = isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        bufferInfo.commandPool = fromGraphics ? gCommandPool : tCommandPool;
        bufferInfo.commandBufferCount = count;
        VkResult result;
        if((result = vkAllocateCommandBuffers(device, &bufferInfo, buffers))!=VK_SUCCESS){
            LOGWITH("Failed to allocate command buffers:", result,resultAsString(result));
            buffers[0] = VK_NULL_HANDLE;
        }
    }

    void VkMachine::checkSurfaceHandle(){
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice.card, surface.handle, &surface.caps);
        uint32_t count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.card, surface.handle, &count, nullptr);
        if(count == 0) LOGWITH("Fatal: no available surface format?");
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice.card, surface.handle, &count, formats.data());
        surface.format = formats[0];
        for(VkSurfaceFormatKHR& form:formats){
            if(form.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR && form.format == VK_FORMAT_B8G8R8A8_SRGB){
                surface.format = form;
            }
        }
    }

    mat4 VkMachine::preTransform(){
#if BOOST_PLAT_ANDROID
        switch(singleton->surface.caps.currentTransform){            
            case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:
                return mat4::rotate(0, 0, PI<float> * 0.5f);
            case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:
                return mat4::rotate(0, 0, PI<float>);
            case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:
                return mat4::rotate(0, 0, PI<float> * 1.5f);
            default:
                return mat4();
        }   
#else
        return mat4();
#endif
    }

    void VkMachine::createSwapchain(uint32_t width, uint32_t height, Window* window){
        destroySwapchain();
        if(width == 0 || height == 0) { // 창 최소화 상태 등. VkMachine은 swapchain이 nullptr일 때 그것이 대상인 렌더패스를 사용할 수 없음 (그리기 호출 시 아무것도 하지 않음)
            return;
        }
        if(window) { // 안드로이드에서 홈키 누른 뒤로 surface lost 떠서 다시 안 그려지는 것 때문에
            vkDestroySurfaceKHR(singleton->instance, singleton->surface.handle, nullptr);
            window->createWindowSurface(singleton->instance, &singleton->surface.handle);
        }
        checkSurfaceHandle();
        VkSwapchainCreateInfoKHR scInfo{};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.surface = surface.handle;
        scInfo.minImageCount = std::min(3u, surface.caps.maxImageCount == 0 ? 3u : surface.caps.maxImageCount);
        scInfo.imageFormat = surface.format.format;
        scInfo.imageColorSpace = surface.format.colorSpace;
        scInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        scInfo.imageArrayLayers = 1;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
#if BOOST_PLAT_ANDROID
        // 아래처럼 currnetTransform을 주지 않으면 suboptimal이라는 문제가 생김. currentTransform을 적용하면 괜찮지만 응용단에서 최후에 회전을 추가로 가해야 함
        scInfo.preTransform = surface.caps.currentTransform; // IDENTITY: 1, 90: 2, 180: 4, 270: 8
        if (scInfo.preTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR || scInfo.preTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
	        // Pre-rotation: always use native orientation i.e. if rotated, use width and height of identity transform
	        std::swap(width, height);
        }
#else
        scInfo.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
#endif
        scInfo.imageExtent.width = std::clamp(width, surface.caps.minImageExtent.width, surface.caps.maxImageExtent.width);
        scInfo.imageExtent.height = std::clamp(height, surface.caps.minImageExtent.height, surface.caps.maxImageExtent.height);
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.clipped = VK_TRUE;
        scInfo.oldSwapchain = VK_NULL_HANDLE; // 같은 표면에 대한 핸들은 올드로 사용할 수 없음
        uint32_t qfi[2] = {physicalDevice.gq, physicalDevice.pq};
        if(physicalDevice.gq == physicalDevice.pq){
            scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else{
            scInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            scInfo.queueFamilyIndexCount = 2;
            scInfo.pQueueFamilyIndices = qfi;
        }

        VkResult result;
        if((result = vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain.handle))!=VK_SUCCESS){
            LOGWITH("Failed to create swapchain:",result,resultAsString(result));
            return;
        }
        swapchain.extent = scInfo.imageExtent;
        uint32_t count;
        vkGetSwapchainImagesKHR(device, swapchain.handle, &count, nullptr);
        std::vector<VkImage> images(count);
        swapchain.imageView.resize(count);
        vkGetSwapchainImagesKHR(device, swapchain.handle, &count, images.data());
        for(size_t i = 0;i < count;i++){
            swapchain.imageView[i] = createImageView(device,images[i],VK_IMAGE_VIEW_TYPE_2D,surface.format.format,1,1,VK_IMAGE_ASPECT_COLOR_BIT);
            if(swapchain.imageView[i] == 0) {
                return;
            }
        }

        for(auto& fpass: finalPasses){
            if(!fpass.second->reconstructFB(width, height)){
                LOGWITH("RenderPass",fpass.first,": Failed to be recreate framebuffer");
            }
        }
    }

    void VkMachine::destroySwapchain(){
        vkDeviceWaitIdle(device);
        for(VkImageView v: swapchain.imageView){ vkDestroyImageView(device, v, nullptr); }
        vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
        swapchain.imageView.clear();
        swapchain.handle = VK_NULL_HANDLE;
    }

    bool VkMachine::createLayouts(){
        VkDescriptorSetLayoutBinding txBinding{};
        txBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        txBinding.descriptorCount = 1;
        txBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &txBinding;

        for(txBinding.binding = 0; txBinding.binding < 4; txBinding.binding++){
            VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &textureLayout[txBinding.binding]);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to create texture descriptor set layout binding ",txBinding.binding,':', result,resultAsString(result));
                return false;
            }
        }

        txBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

        for(txBinding.binding = 0; txBinding.binding < 4; txBinding.binding++){
            VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &inputAttachmentLayout[txBinding.binding]);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to create input attachment descriptor set layout binding ",txBinding.binding,':', result,resultAsString(result));
                return false;
            }
        }

        return true;
    }

    void VkMachine::free() {
        vkDeviceWaitIdle(device);
        for(VkDescriptorSetLayout& layout: textureLayout) { vkDestroyDescriptorSetLayout(device, layout, nullptr); layout = VK_NULL_HANDLE; }
        for(VkDescriptorSetLayout& layout: inputAttachmentLayout) { vkDestroyDescriptorSetLayout(device, layout, nullptr); layout = VK_NULL_HANDLE; }
        for(VkSampler& sampler: textureSampler) { vkDestroySampler(device, sampler, nullptr); sampler = VK_NULL_HANDLE; }
        vkDestroySampler(device, nearestSampler, nullptr); nearestSampler = VK_NULL_HANDLE;
        for(auto& cp: cubePasses) { delete cp.second; }
        for(auto& fp: finalPasses) { delete fp.second; }
        for(auto& rp: renderPasses) { delete rp.second; }
        for(auto& rt: renderTargets){ delete rt.second; }
        for(auto& sh: shaders) { vkDestroyShaderModule(device, sh.second, nullptr); }
        for(auto& pp: pipelines) { vkDestroyPipeline(device, pp.second, nullptr); }
        for(auto& pp: pipelineLayouts) { vkDestroyPipelineLayout(device, pp.second, nullptr); }

        textures.clear();
        meshes.clear();
        pipelines.clear();
        pipelineLayouts.clear();
        cubePasses.clear();
        finalPasses.clear();
        renderPasses.clear();
        renderTargets.clear();
        shaders.clear();
        destroySwapchain();
        vmaDestroyAllocator(allocator);
        vkDestroyCommandPool(device, gCommandPool, nullptr);
        vkDestroyCommandPool(device, tCommandPool, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface.handle, nullptr);
        vkDestroyInstance(instance, nullptr);
        allocator = VK_NULL_HANDLE;
        gCommandPool = VK_NULL_HANDLE;
        tCommandPool = VK_NULL_HANDLE;
        descriptorPool = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        graphicsQueue = VK_NULL_HANDLE;
        presentQueue = VK_NULL_HANDLE;
        transferQueue = VK_NULL_HANDLE;
        surface.handle = VK_NULL_HANDLE;
        instance = VK_NULL_HANDLE;
    }

    void VkMachine::handle() {
        singleton->loadThread.handleCompleted();
    }

    void VkMachine::allocateDescriptorSets(VkDescriptorSetLayout* layouts, uint32_t count, VkDescriptorSet* output){
        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.pSetLayouts = layouts;
        dsAllocInfo.descriptorSetCount = count;
        dsAllocInfo.descriptorPool = descriptorPool;

        VkResult result = vkAllocateDescriptorSets(device, &dsAllocInfo, output);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to allocate descriptor sets:",result,resultAsString(result));
            output[0] = VK_NULL_HANDLE;
        }
    }

    VkResult VkMachine::qSubmit(bool gq_or_tq, uint32_t submitCount, const VkSubmitInfo* submitInfos, VkFence fence){
        bool shouldLock = gqIsTq && loadThread.waiting(); // 스레드 풀에 포스트를 한 스레드에서만 하는 경우라면 이걸로 안전, 아닌 경우 낮은 확률로 큐에 동시 제출
        if(shouldLock) { qGuard.lock(); }
        VkResult ret = vkQueueSubmit(gq_or_tq ? graphicsQueue : transferQueue, submitCount, submitInfos, fence);
        if(shouldLock) { qGuard.unlock(); }
        return ret;
    }

    VkResult VkMachine::qSubmit(const VkPresentInfoKHR* present){
        bool shouldLock = pqIsTq && loadThread.waiting(); // 스레드 풀에 포스트를 한 스레드에서만 하는 경우라면 이걸로 안전, 아닌 경우 낮은 확률로 큐에 동시 제출
        if(shouldLock) { qGuard.lock(); }
        VkResult ret = vkQueuePresentKHR(presentQueue, present);
        if(shouldLock) { qGuard.unlock(); }
        return ret;
    }

    bool VkMachine::createSamplers(){
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.magFilter = VK_FILTER_LINEAR; // TODO: NEAREST랑 선택권
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipLodBias = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        VkResult result;
        for(int i = 0; i < sizeof(textureSampler)/sizeof(textureSampler[0]);i++, samplerInfo.maxLod++){
            result = vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler[i]);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to create texture sampler:", result,resultAsString(result));
                return false;
            }
        }
        samplerInfo.maxLod = 1.0f;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        result = vkCreateSampler(device, &samplerInfo, nullptr, &nearestSampler);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create texture sampler:", result,resultAsString(result));
            return false;
        }
        return true;
    }

    VkMachine::~VkMachine(){
        free();
    }

    void VkMachine::ImageSet::free() {
        vkDestroyImageView(singleton->device, view, nullptr);
        vmaDestroyImage(singleton->allocator, img, alloc);
    }

    VkMachine::pMesh VkMachine::createNullMesh(size_t vcount, int32_t name) {
        pMesh m = getMesh(name);
        if(m) { return m; }
        struct publicmesh:public Mesh{publicmesh(VkBuffer _1, VmaAllocation _2, size_t _3, size_t _4,size_t _5,void* _6,bool _7):Mesh(_1,_2,_3,_4,_5,_6,_7){}};
        if(name == INT32_MIN) return std::make_shared<publicmesh>(VK_NULL_HANDLE,VK_NULL_HANDLE,vcount,0,0,nullptr,false);
        return singleton->meshes[name] = std::make_shared<publicmesh>(VK_NULL_HANDLE,VK_NULL_HANDLE,vcount,0,0,nullptr,false);
    }

    VkMachine::pMesh VkMachine::createMesh(void* vdata, size_t vsize, size_t vcount, void* idata, size_t isize, size_t icount, int32_t name, bool stage) {
        if(icount != 0 && isize != 2 && isize != 4){
            LOGWITH("Invalid isize");
            return pMesh();
        }
        pMesh m = getMesh(name);
        if(m) { return m; }

        VkBuffer vib, sb;
        VmaAllocation viba, sba;
        VkResult result;

        const size_t VBSIZE = vsize*vcount, IBSIZE = isize * icount;

        VkBufferCreateInfo vbInfo{};
        vbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vbInfo.size = VBSIZE + IBSIZE;
        
        VmaAllocationCreateInfo vbaInfo{};
        vbaInfo.usage = VMA_MEMORY_USAGE_AUTO;

        struct publicmesh:public Mesh{publicmesh(VkBuffer _1, VmaAllocation _2, size_t _3, size_t _4,size_t _5,void* _6,bool _7):Mesh(_1,_2,_3,_4,_5,_6,_7){}};

        if(stage) {
            vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // vma 목적지 버퍼 생성 시 HOST_VISIBLE이 있으면 스테이징을 할 필요가 없음, 그러면 재생성 필요 없이 그대로 리턴하도록
            vbaInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        else {
            vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        VmaAllocationInfo mapInfoV;
        result = vmaCreateBuffer(singleton->allocator, &vbInfo, &vbaInfo, &sb, &sba, &mapInfoV);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create stage buffer for vertex:",result,resultAsString(result));
            return pMesh();
        }
        if(vdata) std::memcpy(mapInfoV.pMappedData, vdata, VBSIZE);
        if(idata) std::memcpy((uint8_t*)mapInfoV.pMappedData + VBSIZE, idata, IBSIZE);
        vmaInvalidateAllocation(singleton->allocator, sba, 0, VK_WHOLE_SIZE);
        vmaFlushAllocation(singleton->allocator, sba, 0, VK_WHOLE_SIZE);
        
        if(!stage){
            if(name == INT32_MIN) return std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
            return singleton->meshes[name] = std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
        }

        vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vbInfo.size = VBSIZE + IBSIZE;
        vbaInfo.flags = 0;
        result = vmaCreateBuffer(singleton->allocator, &vbInfo, &vbaInfo, &vib, &viba, nullptr);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create vertex buffer:",result,resultAsString(result));
            vmaDestroyBuffer(singleton->allocator, sb, sba);
            return pMesh();
        }
        VkMemoryPropertyFlags props;
        vmaGetAllocationMemoryProperties(singleton->allocator, viba, &props);
        if(props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            if(name == INT32_MIN) return std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
            return singleton->meshes[name] = std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,nullptr,isize==4);
        }

        VkCommandBuffer copycb;
        singleton->allocateCommandBuffers(1, true, false, &copycb);
        if(!copycb) {
            LOGHERE;
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            if(name == INT32_MIN) return std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
            return singleton->meshes[name] = std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,nullptr,isize==4);
        }
        VkCommandBufferBeginInfo cbInfo{};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset=0;
        copyRegion.dstOffset=0;
        copyRegion.size = VBSIZE + IBSIZE;
        if((result = vkBeginCommandBuffer(copycb, &cbInfo)) != VK_SUCCESS){
            LOGWITH("Failed to begin command buffer:",result,resultAsString(result));
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if(name == INT32_MIN) return std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
            return singleton->meshes[name] = std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,nullptr,isize==4);
        }
        vkCmdCopyBuffer(copycb, sb, vib, 1, &copyRegion);
        if((result = vkEndCommandBuffer(copycb)) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",result,resultAsString(result));
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if(name == INT32_MIN) return std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
            return singleton->meshes[name] = std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,nullptr,isize==4);
        }
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copycb;
        VkFence fence = singleton->createFence(); // TODO: 생성 시 쓰는 이런 자잘한 펜스를 매번 만들었다 없애지 말고 하나 생성해 두고 쓰는 걸로 통일
        if(!fence) {
            LOGHERE;
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if(name == INT32_MIN) return std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
            return singleton->meshes[name] = std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,nullptr,isize==4);
        }
        if((result = singleton->qSubmit(false, 1, &submitInfo, fence)) != VK_SUCCESS){
            LOGWITH("Failed to submit copy command");
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if(name == INT32_MIN) return std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,mapInfoV.pMappedData,isize==4);
            return singleton->meshes[name] = std::make_shared<publicmesh>(sb,sba,vcount,icount,VBSIZE,nullptr,isize==4);
        }
        vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(singleton->device, fence, nullptr);
        vmaDestroyBuffer(singleton->allocator, sb, sba);
        vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
        if(name == INT32_MIN) return std::make_shared<publicmesh>(vib,viba,vcount,icount,VBSIZE,nullptr,isize==4);
        return singleton->meshes[name] = std::make_shared<publicmesh>(vib,viba,vcount,icount,VBSIZE,nullptr,isize==4);
    }

    VkMachine::RenderTarget* VkMachine::createRenderTarget2D(int width, int height, int32_t name, RenderTargetType type, bool sampled, bool useDepthInput, bool useStencil, bool mmap){
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
        ImageSet *color1 = nullptr, *color2 = nullptr, *color3 = nullptr, *ds = nullptr;
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VkImageType::VK_IMAGE_TYPE_2D;
        imgInfo.extent.width = width;
        imgInfo.extent.height = height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        imgInfo.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.tiling = mmap ? VkImageTiling::VK_IMAGE_TILING_LINEAR : VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        if(mmap) allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        else allocInfo.preferredFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkResult result;

        if((int)type & 0b1){
            color1 = new ImageSet;
            imgInfo.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (sampled ? VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT : VkImageUsageFlagBits::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
            imgInfo.format = singleton->surface.format.format;
            result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &color1->img, &color1->alloc, nullptr);
            if(result != VK_SUCCESS) {
                LOGWITH("Failed to create image:", result,resultAsString(result));
                delete color1;
                return nullptr;
            }
            color1->view = createImageView(singleton->device, color1->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
            if(!color1->view) {
                color1->free();
                delete color1;
                return nullptr;
            }
            if((int)type & 0b10){
                color2 = new ImageSet;
                result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &color2->img, &color2->alloc, nullptr);
                if(result != VK_SUCCESS) {
                    LOGWITH("Failed to create image:", result,resultAsString(result));
                    color1->free();
                    delete color1;
                    delete color2;
                    return nullptr;
                }
                color2->view = createImageView(singleton->device, color2->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
                if(!color2->view) {
                    color1->free();
                    color2->free();
                    delete color1;
                    delete color2;
                    return nullptr;
                }
                if((int)type & 0b100){
                    color3 = new ImageSet;
                    result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &color3->img, &color3->alloc, nullptr);
                    if(result != VK_SUCCESS) {
                        LOGWITH("Failed to create image:", result,resultAsString(result));
                        color1->free();
                        color2->free();
                        delete color1;
                        delete color2;
                        return nullptr;
                    }
                    color3->view = createImageView(singleton->device, color3->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
                    if(!color3->view) {
                        vmaDestroyImage(singleton->allocator, color1->img, color1->alloc);
                        color1->free();
                        color2->free();
                        color3->free();
                        delete color1;
                        delete color2;
                        delete color3;
                        return nullptr;
                    }
                }
            }
        }
        if((int)type & 0b1000) {
            ds = new ImageSet;
            imgInfo.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | (sampled ? VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT : (useDepthInput ? VkImageUsageFlagBits::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT : 0));
            imgInfo.format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
            result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &ds->img, &ds->alloc, nullptr);
            if(result != VK_SUCCESS) {
                LOGWITH("Failed to create image: ", result,resultAsString(result));
                if(color1) {color1->free(); delete color1;}
                if(color2) {color2->free(); delete color2;}
                if(color3) {color3->free(); delete color3;}
                delete ds;
                return nullptr;
            }
            VkImageAspectFlags dsFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            if(useStencil) dsFlags |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
            ds->view = createImageView(singleton->device, ds->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, dsFlags);
            if(!ds->view){
                if(color1) {color1->free(); delete color1;}
                if(color2) {color2->free(); delete color2;}
                if(color3) {color3->free(); delete color3;}
                ds->free(); delete ds;
                return nullptr;
            }
        }
        int nim = 0;
        if(color1) {singleton->images.insert(color1); nim++;}
        if(color2) {singleton->images.insert(color2); nim++;}
        if(color3) {singleton->images.insert(color3); nim++;}
        if(ds) {singleton->images.insert(ds); if(useDepthInput) nim++;}

        VkDescriptorSetLayout layout = sampled ? singleton->textureLayout[0] : singleton->inputAttachmentLayout[0];
        VkDescriptorSetLayout layouts[4] = {layout, layout, layout, layout};
        VkDescriptorSet dsets[4]{};

        singleton->allocateDescriptorSets(layouts, nim, dsets);
        if(!dsets[0]){
            LOGHERE;
            if(color1) {color1->free(); delete color1;}
            if(color2) {color2->free(); delete color2;}
            if(color3) {color3->free(); delete color3;}
            if(ds) { ds->free(); delete ds; }
            return nullptr;
        }
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // TODO: 주의
        VkWriteDescriptorSet wr{};
        wr.dstBinding = 0;
        wr.dstArrayElement = 0;
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.descriptorCount = 1;
        wr.pImageInfo = &imageInfo;
        if(sampled) {
            imageInfo.sampler = singleton->textureSampler[0];
            wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else {
            wr.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }
        nim = 0;
        if(color1){
            imageInfo.imageView = color1->view;
            wr.dstSet = dsets[nim++];
            vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
            if(color2){
                imageInfo.imageView = color2->view;
                wr.dstSet = dsets[nim++];
                vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
                if(color3){
                    imageInfo.imageView = color3->view;
                    wr.dstSet = dsets[nim++];
                    vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
                }
            }
        }
        if(ds && useDepthInput){
            imageInfo.imageView = ds->view;
            wr.dstSet = dsets[nim];
            vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr); // 입력 첨부물 기술자를 위한 이미지 뷰에서는 DEPTH, STENCIL을 동시에 명시할 수 없음. 솔직히 깊이를 입력첨부물로는 안 쓸 것 같긴 한데 
        }
        if(name == INT32_MIN) return new RenderTarget(type, width, height, color1, color2, color3, ds, sampled, mmap, dsets);
        return singleton->renderTargets[name] = new RenderTarget(type, width, height, color1, color2, color3, ds, sampled, mmap, dsets);
    }

    void VkMachine::removeImageSet(VkMachine::ImageSet* set) {
        auto it = images.find(set);
        if(it != images.end()) {
            (*it)->free();
            delete *it;
            images.erase(it);
        }
    }

    VkShaderModule VkMachine::createShader(const uint32_t* spv, size_t size, int32_t name) {
        VkShaderModule ret = getShader(name);
        if(ret) return ret;

        VkShaderModuleCreateInfo smInfo{};
        smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smInfo.codeSize = size;
        smInfo.pCode = spv;
        VkResult result = vkCreateShaderModule(singleton->device, &smInfo, nullptr, &ret);
        if(result != VK_SUCCESS) {
            LOGWITH("Failed to create shader moudle:", result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        if(name == INT32_MIN) return ret;
        return singleton->shaders[name] = ret;
    }

    VkMachine::pTexture VkMachine::createTexture(void* ktxObj, int32_t key, uint32_t nChannels, bool srgb, bool hq){
        ktxTexture2* texture = reinterpret_cast<ktxTexture2*>(ktxObj);
        if (texture->numLevels == 0) return pTexture();
        VkFormat availableFormat;
        ktx_error_code_e k2result;
        if(ktxTexture2_NeedsTranscoding(texture)){
            ktx_transcode_fmt_e tf;
            switch (availableFormat = textureFormatFallback(physicalDevice.card, texture->baseWidth, texture->baseHeight, nChannels, srgb, hq, texture->isCubemap ? VkImageCreateFlagBits::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlagBits)0))
            {
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
                tf = KTX_TTF_ASTC_4x4_RGBA;
                break;
            case VK_FORMAT_BC7_SRGB_BLOCK:
                tf = KTX_TTF_BC7_RGBA;
                break;
            case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
                tf = KTX_TTF_ETC2_RGBA;
                break;
            case VK_FORMAT_BC3_SRGB_BLOCK:
                tf = KTX_TTF_BC3_RGBA;
                break;
            default:
                tf = KTX_TTF_RGBA32;
                break;
            }
            if((k2result = ktxTexture2_TranscodeBasis(texture,tf, 0)) != KTX_SUCCESS){
                LOGWITH("Failed to transcode ktx texture:",k2result);
                ktxTexture_Destroy(ktxTexture(texture));
                return pTexture();
            }
        }
        else{
            // LOGWITH("Warning: this texture may fail to be created due to hardware condition");
            availableFormat = (VkFormat)texture->vkFormat;
        }
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.size = ktxTexture_GetDataSize(ktxTexture(texture));

        VkBuffer newBuffer;
        VmaAllocation newAlloc;
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        VkResult result;
        result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &newBuffer, &newAlloc, nullptr);
        if(result != VK_SUCCESS) {
            LOGWITH("Failed to create buffer:",result,resultAsString(result));
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
        void* mmap;
        result = vmaMapMemory(allocator, newAlloc, &mmap);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to map memory to buffer:",result,resultAsString(result));
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
        std::memcpy(mmap, ktxTexture_GetData(ktxTexture(texture)), bufferInfo.size);
        vmaInvalidateAllocation(singleton->allocator, newAlloc, 0, VK_WHOLE_SIZE);
        vmaFlushAllocation(singleton->allocator, newAlloc, 0, VK_WHOLE_SIZE);
        vmaUnmapMemory(allocator, newAlloc);

        std::vector<VkBufferImageCopy> bufferCopyRegions(texture->numLevels * texture->numFaces);
        uint32_t regionIndex = 0;
        for(uint32_t f = 0; f < texture->numFaces; f++){
            for(uint32_t i = 0; i < texture->numLevels; i++, regionIndex++){
                ktx_size_t offset;
                ktxTexture_GetImageOffset(ktxTexture(texture), i, 0, f, &offset);
                VkBufferImageCopy& region = bufferCopyRegions[regionIndex];
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.imageSubresource.mipLevel = i;
                region.imageSubresource.baseArrayLayer = f;
                region.imageSubresource.layerCount = 1;
                region.imageExtent.width = texture->baseWidth >> i;
                region.imageExtent.height = texture->baseHeight >> i;
                region.imageExtent.depth = 1;
                region.bufferOffset = offset;
                region.bufferImageHeight = 0;
            }
        }
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = availableFormat;
        imgInfo.mipLevels = texture->numLevels;
        imgInfo.arrayLayers = texture->numFaces;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.extent = {texture->baseWidth, texture->baseHeight, 1};
        imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.flags = texture->isCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

        VkImage newImg;
        VmaAllocation newAlloc2;
        allocInfo.flags = 0;
        vmaCreateImage(allocator, &imgInfo, &allocInfo, &newImg, &newAlloc2, nullptr);
        VkCommandBuffer copyCmd;
        allocateCommandBuffers(1, true, false, &copyCmd);

        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.image = newImg;
        imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgBarrier.subresourceRange.baseMipLevel = 0;
        imgBarrier.subresourceRange.levelCount = texture->numLevels;
        imgBarrier.subresourceRange.layerCount = texture->numFaces;
        imgBarrier.srcAccessMask = 0;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if((result = vkBeginCommandBuffer(copyCmd, &beginInfo)) != VK_SUCCESS){
            LOGWITH("Failed to begin command buffer:",result,resultAsString(result));
            ktxTexture_Destroy(ktxTexture(texture));
            vkFreeCommandBuffers(device, tCommandPool, 1, &copyCmd);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
            return pTexture();
        }
        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
        vkCmdCopyBufferToImage(copyCmd, newBuffer, newImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)bufferCopyRegions.size(), bufferCopyRegions.data());
        imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

        if((result = vkEndCommandBuffer(copyCmd)) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",result,resultAsString(result));
            ktxTexture_Destroy(ktxTexture(texture));
            vkFreeCommandBuffers(device, tCommandPool, 1, &copyCmd);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
            return pTexture();
        }
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCmd;
        VkFence fence = createFence();
        if(fence == VK_NULL_HANDLE) {
            LOGHERE;
            ktxTexture_Destroy(ktxTexture(texture));
            vkFreeCommandBuffers(device, tCommandPool, 1, &copyCmd);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
            return pTexture();
        }
        if((result = qSubmit(false, 1, &submitInfo, fence)) != VK_SUCCESS){
            LOGWITH("Failed to submit copy command:",result,resultAsString(result));
            ktxTexture_Destroy(ktxTexture(texture));
            vkFreeCommandBuffers(device, tCommandPool, 1, &copyCmd);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
            vkDestroyFence(device, fence, nullptr);
            return pTexture();
        }

        VkImageView newView;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = newImg;
        viewInfo.viewType = texture->isCubemap ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = availableFormat;
        viewInfo.subresourceRange = imgBarrier.subresourceRange;
        ktxTexture_Destroy(ktxTexture(texture));

        result = vkCreateImageView(device, &viewInfo, nullptr, &newView);

        vkWaitForFences(device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, tCommandPool, 1, &copyCmd);
        vmaDestroyBuffer(allocator, newBuffer, newAlloc);

        if(result != VK_SUCCESS){
            LOGWITH("Failed to create image view:",result,resultAsString(result));
            vmaDestroyImage(allocator, newImg, newAlloc2);
            return pTexture();
        }

        VkDescriptorSet newSet;
        singleton->allocateDescriptorSets(&textureLayout[0], 1, &newSet); // TODO: 여기 바인딩 번호 선택권
        if(!newSet){
            LOGHERE;
            vkDestroyImageView(device, newView, nullptr);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            return pTexture();
        }

        VkDescriptorImageInfo dsImageInfo{};
        dsImageInfo.imageView = newView;
        dsImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dsImageInfo.sampler = textureSampler[imgInfo.mipLevels - 1];

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = newSet;
        descriptorWrite.dstBinding = 0; // TODO: 위의 것과 함께 선택권
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &dsImageInfo;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

        struct txtr:public Texture{ inline txtr(VkImage _1, VkImageView _2, VmaAllocation _3, VkDescriptorSet _4, uint32_t _5):Texture(_1,_2,_3,_4,_5){} };
        if(key == INT32_MIN) return std::make_shared<txtr>(newImg, newView, newAlloc2, newSet, 0);
        return textures[key] = std::make_shared<txtr>(newImg, newView, newAlloc2, newSet, 0);
    }

    VkMachine::pTexture VkMachine::createTexture(const char* fileName, int32_t key, uint32_t nChannels, bool srgb, bool hq){
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
        return singleton->createTexture(texture, key, nChannels, srgb, hq);
    }

    VkMachine::pTexture VkMachine::createTexture(const uint8_t* mem, size_t size, uint32_t nChannels, int32_t key, bool srgb, bool hq){
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
        return singleton->createTexture(texture, key, nChannels, srgb, hq);
    }

    void VkMachine::asyncCreateTexture(const char* fileName, int32_t key, uint32_t nChannels, std::function<void(void*)> handler, bool srgb, bool hq){
        if(key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([fileName, key, nChannels, handler, srgb, hq, already](){
            if(!already){
                pTexture ret = singleton->createTexture(fileName, INT32_MIN, nChannels, srgb, hq);
                singleton->textureGuard.lock();
                singleton->textures[key] = std::move(ret);
                singleton->textureGuard.unlock();
            }
            return (void*)key;
        }, handler, vkm_strand::GENERAL);
    }

    void VkMachine::asyncCreateTexture(const uint8_t* mem, size_t size, uint32_t nChannels, std::function<void(void*)> handler, int32_t key, bool srgb, bool hq) {
        if(key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        bool already = (bool)getTexture(key, true);
        singleton->loadThread.post([mem, size, key, nChannels, handler, srgb, hq, already](){
            if(!already){
                pTexture ret = singleton->createTexture(mem, size, nChannels, INT32_MIN, srgb, hq);
                std::this_thread::sleep_for(std::chrono::seconds(3)); // async 테스트용
                singleton->textureGuard.lock();
                singleton->textures[key] = std::move(ret);
                singleton->textureGuard.unlock();
            }
            return (void*)key;
        }, handler, vkm_strand::GENERAL);
    }

    VkMachine::Texture::Texture(VkImage img, VkImageView view, VmaAllocation alloc, VkDescriptorSet dset, uint32_t binding):img(img), view(view), alloc(alloc), dset(dset), binding(binding){ }
    VkMachine::Texture::~Texture(){
        //vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset);
        vkDestroyImageView(singleton->device, view, nullptr);
        vmaDestroyImage(singleton->allocator, img, alloc);
    }

    VkDescriptorSetLayout VkMachine::Texture::getLayout(){
        return singleton->textureLayout[binding];
    }

    void VkMachine::Texture::collect(bool removeUsing) {
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

    void VkMachine::Texture::drop(int32_t name){
        singleton->textures.erase(name);
    }

    VkMachine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, VkMachine::ImageSet* color1, VkMachine::ImageSet* color2, VkMachine::ImageSet* color3, VkMachine::ImageSet* depthstencil, bool sampled, bool mmap, VkDescriptorSet* dsets)
    :type(type), width(width), height(height), color1(color1), color2(color2), color3(color3), depthstencil(depthstencil), sampled(sampled), mapped(mmap){
        int nim=0;
        if(color1) {
            dset1 = dsets[nim++];
            if(color2) {
                dset2 = dsets[nim++];
                if(color3){
                    dset3 = dsets[nim++];
                }
            }
        }
        if(depthstencil){
            dsetDS = dsets[nim];
        }
    }

    uint32_t VkMachine::RenderTarget::getDescriptorSets(VkDescriptorSet* sets){
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

    VkMachine::UniformBuffer* VkMachine::createUniformBuffer(uint32_t length, uint32_t size, VkShaderStageFlags stages, int32_t name, uint32_t binding){
        UniformBuffer* ret = getUniformBuffer(name);
        if(ret) return ret;

        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = binding;
        uboBinding.descriptorType = (length == 1) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1; // 이 카운트가 늘면 ubo 자체가 배열이 됨, 언젠가는 이를 선택으로 제공할 수도
        uboBinding.stageFlags = stages;

        uint32_t individual;
        VkDescriptorSetLayout layout;
        VkDescriptorSet dset;
        VkBuffer buffer;
        VmaAllocation alloc;
        void* mmap;

        if(length > 1){
            individual = (size + singleton->physicalDevice.minUBOffsetAlignment - 1);
            individual -= individual % singleton->physicalDevice.minUBOffsetAlignment;
        }
        else{
            individual = size;
        }

        // ex: layout(set = 1, binding = 1) uniform sampler2D tex[2]; 여기서 descriptor set은 1번 하나, binding은 그냥 정해 두는 번호, descriptor는 2개

        VkDescriptorSetLayoutCreateInfo uboInfo{};
        uboInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        uboInfo.bindingCount = 1;
        uboInfo.pBindings = &uboBinding;

        VkResult result;

        if((result = vkCreateDescriptorSetLayout(singleton->device, &uboInfo, nullptr, &layout)) != VK_SUCCESS){
            LOGWITH("Failed to create descriptor set layout:",result,resultAsString(result));
            return nullptr;
        }

        singleton->allocateDescriptorSets(&layout, 1, &dset);
        if(!dset){
            LOGHERE;
            vkDestroyDescriptorSetLayout(singleton->device, layout, nullptr);
            return nullptr;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.size = individual * length;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo bainfo{};
        bainfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        bainfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if(length > 1){
            result = vmaCreateBufferWithAlignment(singleton->allocator, &bufferInfo, &bainfo, singleton->physicalDevice.minUBOffsetAlignment, &buffer, &alloc, nullptr);
        }
        else{
            result = vmaCreateBuffer(singleton->allocator, &bufferInfo, &bainfo, &buffer, &alloc, nullptr);
        }
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create buffer:", result,resultAsString(result));
            return nullptr;
        }

        if((result = vmaMapMemory(singleton->allocator, alloc, &mmap)) != VK_SUCCESS){
            LOGWITH("Failed to map memory:", result,resultAsString(result));
            return nullptr;
        }

        VkDescriptorBufferInfo dsNBuffer{};
        dsNBuffer.buffer = buffer;
        dsNBuffer.offset = 0;
        dsNBuffer.range = individual * length;
        VkWriteDescriptorSet wr{};
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.descriptorType = uboBinding.descriptorType;
        wr.descriptorCount = uboBinding.descriptorCount;
        wr.dstArrayElement = 0;
        wr.dstBinding = uboBinding.binding;
        wr.pBufferInfo = &dsNBuffer;
        wr.dstSet = dset;
        vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
        if(name == INT32_MIN) return new UniformBuffer(length, individual, buffer, layout, dset, alloc, mmap, binding);
        return singleton->uniformBuffers[name] = new UniformBuffer(length, individual, buffer, layout, dset, alloc, mmap, binding);
    }

    uint32_t VkMachine::RenderTarget::attachmentRefs(VkAttachmentDescription* arr, bool forSample){
        uint32_t colorCount = 0;
        if(color1) {
            arr[0].format = singleton->surface.format.format;
            arr[0].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            arr[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            arr[0].storeOp = (sampled || mapped) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            arr[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            arr[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            arr[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            arr[0].finalLayout = forSample ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorCount = 1;
            if(color2){
                std::memcpy(arr + 1, arr, sizeof(arr[0]));
                colorCount = 2;
                if(color3) {
                    std::memcpy(arr + 2, arr, sizeof(arr[0]));
                    colorCount = 3;
                }
            }
        }
        if(depthstencil) {
            arr[colorCount].format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
            arr[colorCount].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            arr[colorCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            arr[colorCount].storeOp = (sampled || mapped) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE; // 그림자맵에서야 필요하고 그 외에는 필요없음
            arr[colorCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            arr[colorCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            arr[colorCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            arr[colorCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        return colorCount;
    }

    VkMachine::RenderTarget::~RenderTarget(){
        if(color1) { singleton->removeImageSet(color1); /*if(dset1) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset1);*/ }
        if(color2) { singleton->removeImageSet(color2); /*if(dset2) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset2);*/ }
        if(color3) { singleton->removeImageSet(color3); /*if(dset3) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset3);*/ }
        if(depthstencil) { singleton->removeImageSet(depthstencil); /*if (dsetDS) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dsetDS);*/ }
    }

    VkMachine::RenderPass2Cube* VkMachine::createRenderPass2Cube(uint32_t width, uint32_t height, int32_t key, bool useColor, bool useDepth) {
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
        VkResult result;
        if(useColor) {
            imgInfo.format = singleton->surface.format.format;
            imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &colorImage, &colorAlloc, nullptr);
            if(result != VK_SUCCESS) {
                LOGWITH("Failed to create image:",result,resultAsString(result));
                return nullptr;
            }
        }
        if(useDepth) {
            imgInfo.format = VK_FORMAT_D32_SFLOAT;
            imgInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            if(!useColor) imgInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &depthImage, &depthAlloc, nullptr);
            if(result != VK_SUCCESS) {
                LOGWITH("Failed to create image:",result,resultAsString(result));
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
                result = vkCreateImageView(singleton->device, &viewInfo, nullptr, &targets[i]);
                if(result != VK_SUCCESS) {
                    LOGWITH("Failed to create image view:",result,resultAsString(result));
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
                result = vkCreateImageView(singleton->device, &viewInfo, nullptr, &targets[i]);
                if(result != VK_SUCCESS) {
                    LOGWITH("Failed to create image view:",result,resultAsString(result));
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
        result = vkCreateImageView(singleton->device, &viewInfo, nullptr, &texture);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create cube image view:",result,resultAsString(result));
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

        result = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &rp);
        if(result != VK_SUCCESS) {
            LOGWITH("Failed to create render pass:", result, resultAsString(result));
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
            result = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb[i]);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to create framebuffer:", result, resultAsString(result));
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

    VkMachine::RenderPass2Screen* VkMachine::createRenderPass2Screen(RenderTargetType* tgs, uint32_t subpassCount, int32_t name, bool useDepth, bool* useDepthAsInput){
        RenderPass2Screen* r = getRenderPass2Screen(name);
        if(r) return r;

        if(subpassCount == 0) return nullptr;
        std::vector<RenderTarget*> targets(subpassCount - 1);
        for(uint32_t i = 0; i < subpassCount - 1; i++){
            targets[i] = createRenderTarget2D(singleton->swapchain.extent.width, singleton->swapchain.extent.height, INT32_MIN, tgs[i], false, useDepthAsInput ? useDepthAsInput[i] : false);
            if(!targets[i]){
                LOGHERE;
                for(RenderTarget* t:targets) delete t;
                return nullptr;
            }
        }

        VkImage dsImage = VK_NULL_HANDLE;
        VmaAllocation dsAlloc = VK_NULL_HANDLE;
        VkImageView dsImageView = VK_NULL_HANDLE;
        VkResult result;
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
            
            if((result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &dsImage, &dsAlloc, nullptr)) != VK_SUCCESS){
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

        if((result = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &newPass)) != VK_SUCCESS){
            LOGWITH("Failed to create renderpass:",result,resultAsString(result));
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
            if((result = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb)) != VK_SUCCESS){
                LOGWITH("Failed to create framebuffer:",result,resultAsString(result));
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

    VkMachine::RenderPass* VkMachine::createRenderPass(RenderTarget** targets, uint32_t subpassCount, int32_t name){
        RenderPass* r = getRenderPass(name);
        if(r) return r;
        if(subpassCount == 0) return nullptr;
        for(uint32_t i = 0; i < subpassCount - 1; i++) {
            if(targets[i]->sampled) {
                LOGWITH("Warning: the given target",i,"was not made to be an input attachment(sampled = true)");
                return nullptr;
            }
        }
        if(!targets[subpassCount - 1]->sampled){
            LOGWITH("Warning: the last given target was made to be an input attachment(sampled = false)");
            return nullptr;
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

        for(uint32_t i = 0; i < subpassCount; i++){
            uint32_t colorCount = targets[i]->attachmentRefs(&attachments[totalAttachments], i == (subpassCount - 1));
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
            dependencies[i].srcSubpass = i - 1;
            dependencies[i].dstSubpass = i;
            dependencies[i].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[i].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[i].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[i].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            inputAttachmentCount = colorCount; if(targets[i]->depthstencil) inputAttachmentCount++;
        }

        dependencies[0].srcSubpass = subpassCount - 1;
        dependencies[0].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.subpassCount = subpassCount;
        rpInfo.pSubpasses = subpasses.data();
        rpInfo.attachmentCount = totalAttachments;
        rpInfo.pAttachments = attachments.data();
        rpInfo.dependencyCount = subpassCount; // 스왑체인 의존성은 이 함수를 통해 만들지 않기 때문에 이대로 사용
        rpInfo.pDependencies = &dependencies[0];
        VkRenderPass newPass;
        VkResult result;
        if((result = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &newPass)) != VK_SUCCESS){
            LOGWITH("Failed to create renderpass:",result,resultAsString(result));
            return nullptr;
        }

        VkFramebuffer fb;
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.attachmentCount = totalAttachments;
        fbInfo.pAttachments = ivs.data();
        fbInfo.renderPass = newPass;
        fbInfo.width = targets[0]->width;
        fbInfo.height = targets[0]->height;
        fbInfo.layers = 1; // 큐브맵이면 6인데 일단 그건 다른 함수로 한다고 침
        if((result = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb)) != VK_SUCCESS){
            LOGWITH("Failed to create framebuffer:",result,resultAsString(result));
            return nullptr;
        }

        RenderPass* ret = new RenderPass(newPass, fb, subpassCount);
        for(uint32_t i = 0; i < subpassCount; i++){ ret->targets[i] = targets[i]; }
        ret->setViewport(targets[0]->width, targets[0]->height, 0.0f, 0.0f);
        ret->setScissor(targets[0]->width, targets[0]->height, 0, 0);
        if(name == INT32_MIN) return ret;
        return singleton->renderPasses[name] = ret;
    }

    VkPipeline VkMachine::createPipeline(VkVertexInputAttributeDescription* vinfo, uint32_t vsize, uint32_t vattr,
    VkVertexInputAttributeDescription* iinfo, uint32_t isize, uint32_t iattr, RenderPass* pass, uint32_t subpass,
    uint32_t flags, VkPipelineLayout layout, VkShaderModule vs, VkShaderModule fs, int32_t name, VkStencilOpState* front, VkStencilOpState* back, VkShaderModule tc, VkShaderModule te, VkShaderModule gs){
        VkPipeline ret = getPipeline(name);
        if(ret) {
            pass->usePipeline(ret, layout, subpass);
            return ret;
        }

        if(!(vs && fs)){
            LOGWITH("Vertex and fragment shader should be provided.");
            return VK_NULL_HANDLE;
        }

        if(tc && te) {
            if(!singleton->physicalDevice.features.tessellationShader) {
                LOGWITH("Tesselation shaders are inavailable in this device. Try to use another pipeline.");
                return VK_NULL_HANDLE;
            }
        }
        else if(tc || te){
            LOGWITH("Tesselation control shader and tesselation evaluation shader must be both null or both available.");
            return VK_NULL_HANDLE;
        }
        if(gs && !singleton->physicalDevice.features.geometryShader) {
            LOGWITH("Geometry shaders are inavailable in this device. Try to use another pipeline.");
            return VK_NULL_HANDLE;
        }

        const uint32_t OPT_COLOR_COUNT =
            (uint32_t)pass->targets[subpass]->type & 0b100 ? 3 :
            (uint32_t)pass->targets[subpass]->type & 0b10 ? 2 :
            (uint32_t)pass->targets[subpass]->type & 0b1 ? 1 :
            0;
        const bool OPT_USE_DEPTHSTENCIL = (int)pass->targets[subpass]->type & 0b1000;

        ret = onart::createPipeline(singleton->device, vinfo, vsize, vattr, iinfo, isize, iattr, pass->rp, subpass, flags, OPT_COLOR_COUNT, OPT_USE_DEPTHSTENCIL, layout, vs, fs, tc, te, gs, front, back);
        if(!ret){
            LOGHERE;
            return VK_NULL_HANDLE;
        }
        pass->usePipeline(ret, layout, subpass);
        if(name == INT32_MIN) return ret;
        return singleton->pipelines[name] = ret;
    }

    VkPipeline VkMachine::createPipeline(VkVertexInputAttributeDescription* vinfo, uint32_t size, uint32_t vattr,
    VkVertexInputAttributeDescription* iinfo, uint32_t isize, uint32_t iattr,
    RenderPass2Screen* pass, uint32_t subpass, uint32_t flags, VkPipelineLayout layout,
    VkShaderModule vs, VkShaderModule fs, int32_t name, VkStencilOpState* front, VkStencilOpState* back,
    VkShaderModule tc, VkShaderModule te, VkShaderModule gs) {
        VkPipeline ret = getPipeline(name);
        if(ret) {
            pass->usePipeline(ret, layout, subpass);
            return ret;
        }

        if(!(vs && fs)){
            LOGWITH("Vertex and fragment shader should be provided.");
            return VK_NULL_HANDLE;
        }

        if(tc && te) {
            if(!singleton->physicalDevice.features.tessellationShader) {
                LOGWITH("Tesselation shaders are inavailable in this device. Try to use another pipeline.");
                return VK_NULL_HANDLE;
            }
        }
        else if(tc || te){
            LOGWITH("Tesselation control shader and tesselation evaluation shader must be both null or both available.");
            return VK_NULL_HANDLE;
        }
        if(gs && !singleton->physicalDevice.features.geometryShader) {
            LOGWITH("Geometry shaders are inavailable in this device. Try to use another pipeline.");
            return VK_NULL_HANDLE;
        }

        uint32_t OPT_COLOR_COUNT;
        bool OPT_USE_DEPTHSTENCIL;
        if(subpass == pass->targets.size()) {
            OPT_COLOR_COUNT = 1;
            OPT_USE_DEPTHSTENCIL = pass->dsView;
        }
        else{
            OPT_COLOR_COUNT =
            (uint32_t)pass->targets[subpass]->type & 0b100 ? 3 :
            (uint32_t)pass->targets[subpass]->type & 0b10 ? 2 :
            (uint32_t)pass->targets[subpass]->type & 0b1 ? 1 :
            0;
            OPT_USE_DEPTHSTENCIL = (int)pass->targets[subpass]->type & 0b1000;
        }

        ret = onart::createPipeline(singleton->device, vinfo, size, vattr, iinfo, isize, iattr, pass->rp, subpass, flags, OPT_COLOR_COUNT, OPT_USE_DEPTHSTENCIL, layout, vs, fs, tc, te, gs, front, back);
        if(!ret){
            LOGHERE;
            return VK_NULL_HANDLE;
        }
        pass->usePipeline(ret, layout, subpass);
        if(name == INT32_MIN) return ret;
        return singleton->pipelines[name] = ret;
    }

    VkPipelineLayout VkMachine::createPipelineLayout(VkDescriptorSetLayout* layouts, uint32_t count,  VkShaderStageFlags stages, int32_t name){
        VkPipelineLayout ret = getPipelineLayout(name);
        if(ret) return ret;

        VkPushConstantRange pushRange{};

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.setLayoutCount = count;
        if(stages){
            pushRange.size = 128;
            pushRange.offset = 0;
            pushRange.stageFlags = stages;
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &pushRange;
        }

        VkResult result = vkCreatePipelineLayout(singleton->device, &layoutInfo, nullptr, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create pipeline layout:",result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        if(name == INT32_MIN) return ret;
        return singleton->pipelineLayouts[name] = ret;
    }

    VkMachine::Mesh::Mesh(VkBuffer vb, VmaAllocation vba, size_t vcount, size_t icount, size_t ioff, void *vmap, bool use32):vb(vb),vba(vba),vcount(vcount),icount(icount),ioff(ioff),vmap(vmap),idxType(use32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16){ }
    VkMachine::Mesh::~Mesh(){ vmaDestroyBuffer(singleton->allocator, vb, vba); }

    void VkMachine::Mesh::update(const void* input, uint32_t offset, uint32_t size){
        if(!vmap) return;
        std::memcpy((uint8_t*)vmap + offset, input, size);
    }

    void VkMachine::Mesh::updateIndex(const void* input, uint32_t offset, uint32_t size){
        if(!vmap || icount == 0) return;
        std::memcpy((uint8_t*)vmap + ioff + offset, input, size);
    }

    void VkMachine::Mesh::collect(bool removeUsing) {
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

    void VkMachine::Mesh::drop(int32_t name){
        singleton->meshes.erase(name);
    }

    VkMachine::RenderPass::RenderPass(VkRenderPass rp, VkFramebuffer fb, uint16_t stageCount): rp(rp), fb(fb), stageCount(stageCount), pipelines(stageCount), pipelineLayouts(stageCount), targets(stageCount){
        fence = singleton->createFence(true);
        semaphore = singleton->createSemaphore();
        singleton->allocateCommandBuffers(1, true, true, &cb);
    }

    VkMachine::RenderPass::~RenderPass(){
        vkFreeCommandBuffers(singleton->device, singleton->gCommandPool, 1, &cb);
        vkDestroySemaphore(singleton->device, semaphore, nullptr);
        vkDestroyFence(singleton->device, fence, nullptr);
        vkDestroyFramebuffer(singleton->device, fb, nullptr);
        vkDestroyRenderPass(singleton->device, rp, nullptr);
    }

    void VkMachine::RenderPass::usePipeline(VkPipeline pipeline, VkPipelineLayout layout, uint32_t subpass){
        if(subpass > stageCount){
            LOGWITH("Invalid subpass. This renderpass has", stageCount, "subpasses but", subpass, "given");
            return;
        }
        pipelines[subpass] = pipeline;
        pipelineLayouts[subpass] = layout;
        if(currentPass == subpass) { vkCmdBindPipeline(cb, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); }
    }

    void VkMachine::RenderPass::reconstructFB(VkMachine::RenderTarget** targets, uint32_t count){
        vkDestroyFramebuffer(singleton->device, fb, nullptr);
        fb = VK_NULL_HANDLE;
        if(stageCount != count) {
            LOGWITH("The given parameter is incompatible to this renderpass");
            return;
        }
        for(uint32_t i = 0; i < count; i++){
            if(this->targets[i]->type != targets[i]->type) {
                LOGWITH("The given parameter is incompatible to this renderpass");
                return;
            }
            this->targets[i] = targets[i];
        }
        std::vector<VkImageView> ivs;
        ivs.reserve(count * 4);
        for(uint32_t i = 0; i < count; i++){
            RenderTarget* target = targets[i];
            if(target->color1){
                ivs.push_back(targets[0]->color1->view);
                if(target->color2){
                    ivs.push_back(target->color2->view);
                    if(target->color3){
                        ivs.push_back(target->color3->view);
                    }
                }
            }
            if(target->depthstencil){
                ivs.push_back(target->depthstencil->view);
            }
        }
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.height = targets[0]->height;
        fbInfo.width = targets[0]->width;
        fbInfo.renderPass = rp;
        fbInfo.layers = 1;
        fbInfo.pAttachments = ivs.data();
        fbInfo.attachmentCount = ivs.size();
        VkResult result = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create framebuffer:", result,resultAsString(result));
        }
        setViewport(targets[0]->width, targets[0]->height, 0.0f, 0.0f);
        setScissor(targets[0]->width, targets[0]->height, 0, 0);
    }

    void VkMachine::RenderPass::setViewport(float width, float height, float x, float y, bool applyNow){
        viewport.height = height;
        viewport.width = width;
        viewport.maxDepth = 1.0f;
        viewport.minDepth = 0.0f;
        viewport.x = x;
        viewport.y = y;
        if(applyNow && currentPass != -1) {
            vkCmdSetViewport(cb, 0, 1, &viewport);
        }
    }

    void VkMachine::RenderPass::setScissor(uint32_t width, uint32_t height, int32_t x, int32_t y, bool applyNow){
        scissor.extent.width = width;
        scissor.extent.height = height;
        scissor.offset.x = x;
        scissor.offset.y = y;
        if(applyNow && currentPass != -1) {
            vkCmdSetScissor(cb, 0, 1, &scissor);
        }
    }

    void VkMachine::RenderPass::bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        ub->sync();
        uint32_t off = ub->offset(ubPos);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, 1, &ub->dset, ub->isDynamic, &off);
    }

    void VkMachine::RenderPass::bind(uint32_t pos, const pTexture& tx) {
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass::bind(uint32_t pos, RenderTarget* target, uint32_t index){
        if(currentPass == -1){
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
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, 1, &dset, 0, nullptr);
    }

    void VkMachine::RenderPass::push(void* input, uint32_t start, uint32_t end){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdPushConstants(cb, pipelineLayouts[currentPass], VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, start, end - start, input); // TODO: 스테이지 플래그를 살려야 함
    }

    void VkMachine::RenderPass::invoke(const pMesh& mesh){
         if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if((bound != mesh.get()) && (mesh->vb != VK_NULL_HANDLE)) {
            VkDeviceSize offs = 0;
            vkCmdBindVertexBuffers(cb, 0, 1, &mesh->vb, &offs);
            if(mesh->icount) vkCmdBindIndexBuffer(cb, mesh->vb, mesh->ioff, mesh->idxType);
        }
        if(mesh->icount) vkCmdDrawIndexed(cb, mesh->icount, 1, 0, 0, 0);
        else vkCmdDraw(cb, mesh->vcount, 1, 0, 0);
        bound = mesh.get();
    }

    void VkMachine::RenderPass::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount){
         if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        VkDeviceSize offs[2] = {0, 0};
        VkBuffer buffs[2] = {mesh->vb, instanceInfo->vb};
        vkCmdBindVertexBuffers(cb, 0, 2, buffs, offs);
        if(mesh->icount) {
            vkCmdBindIndexBuffer(cb, mesh->vb, mesh->ioff, mesh->idxType);
            vkCmdDrawIndexed(cb, mesh->icount, instanceCount, 0, 0, 0);
        }
        else{
            vkCmdDraw(cb, mesh->vcount, instanceCount, 0, 0);
        }
        bound = nullptr;
    }

    void VkMachine::RenderPass::execute(RenderPass* other){
        if(currentPass != pipelines.size() - 1){
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        vkCmdEndRenderPass(cb);
        bound = nullptr;
        VkResult result;
        if((result = vkEndCommandBuffer(cb)) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",result);
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

        if((result = vkResetFences(singleton->device, 1, &fence)) != VK_SUCCESS){
            LOGWITH("Failed to reset fence. waiting or other operations will play incorrect");
            return;
        }

        if ((result = singleton->qSubmit(true, 1, &submitInfo, fence)) != VK_SUCCESS) {
            LOGWITH("Failed to submit command buffer");
            return;
        }

        currentPass = -1;
    }

    bool VkMachine::RenderPass::wait(uint64_t timeout){
        return vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, timeout) == VK_SUCCESS; // VK_TIMEOUT이나 VK_ERROR_DEVICE_LOST
    }

    void VkMachine::RenderPass::start(uint32_t pos){
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
        VkResult result;
        if(currentPass == 0){
            wait();
            vkResetCommandBuffer(cb, 0);
            VkCommandBufferBeginInfo cbInfo{};
            cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cbInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = vkBeginCommandBuffer(cb, &cbInfo);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to begin command buffer:",result,resultAsString(result));
                currentPass = -1;
                return;
            }
            VkRenderPassBeginInfo rpInfo{};
            std::vector<VkClearValue> clearValues; // TODO: 이건 렌더타겟이 갖고 있는 게 자유도 면에서 나을 것 같음
            clearValues.reserve(stageCount * 4);
            for(RenderTarget* targ: targets){
                if((int)targ->type & 0b1) {
                    clearValues.push_back({0.03f, 0.03f, 0.03f, 0.0f});
                    if((int)targ->type & 0b10){
                        clearValues.push_back({0.03f, 0.03f, 0.03f, 0.0f});
                        if((int)targ->type & 0b100){
                            clearValues.push_back({0.03f, 0.03f, 0.03f, 0.0f});
                        }
                    }
                }
                if((int)targ->type & 0b1000){
                    clearValues.push_back({1.0f, 0u});
                }
            }
            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.framebuffer = fb;
            rpInfo.pClearValues = clearValues.data();
            rpInfo.clearValueCount = clearValues.size();
            rpInfo.renderArea.offset = {0,0};
            rpInfo.renderArea.extent = {targets[0]->width, targets[0]->height};
            rpInfo.renderPass = rp;

            vkCmdBeginRenderPass(cb, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        }
        else{
            vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
            VkDescriptorSet dset[4];
            uint32_t count = targets[currentPass - 1]->getDescriptorSets(dset);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, count, dset, 0, nullptr);
        }
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]);
        vkCmdSetViewport(cb, 0, 1, &viewport);
        vkCmdSetScissor(cb, 0, 1, &scissor);
    }

    VkMachine::RenderPass2Cube::~RenderPass2Cube(){
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

    void VkMachine::RenderPass2Cube::beginFacewise(uint32_t pass){
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
        VkResult result;
        if((result = vkBeginCommandBuffer(facewise[pass], &cbInfo)) != VK_SUCCESS){
            LOGWITH("Failed to begin command buffer:",result,resultAsString(result));
        }
    }

    void VkMachine::RenderPass2Cube::bind(uint32_t pos, UniformBuffer* ub, uint32_t pass, uint32_t ubPos){
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

    void VkMachine::RenderPass2Cube::bind(uint32_t pos, const pTexture& tx){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass2Cube::bind(uint32_t pos, RenderTarget* target, uint32_t index){
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
    
    void VkMachine::RenderPass2Cube::usePipeline(VkPipeline pipeline, VkPipelineLayout layout){
        this->pipeline = pipeline;
        this->pipelineLayout = layout;
        if(recording) { vkCmdBindPipeline(scb, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); }
    }

    void VkMachine::RenderPass2Cube::push(void* input, uint32_t start, uint32_t end){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdPushConstants(scb, pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, start, end - start, input); // TODO: 스테이지 플래그를 살려야 함
    }

    void VkMachine::RenderPass2Cube::invoke(const pMesh& mesh){
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if((bound != mesh.get()) && (mesh->vb != VK_NULL_HANDLE)) {
            VkDeviceSize offs = 0;
            vkCmdBindVertexBuffers(scb, 0, 1, &mesh->vb, &offs);
            if(mesh->icount) vkCmdBindIndexBuffer(scb, mesh->vb, mesh->ioff, mesh->idxType);
        }
        if(mesh->icount) vkCmdDrawIndexed(scb, mesh->icount, 1, 0, 0, 0);
        else vkCmdDraw(scb, mesh->vcount, 1, 0, 0);
        bound = mesh.get();
    }

    void VkMachine::RenderPass2Cube::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount){
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        VkDeviceSize offs[2] = {0, 0};
        VkBuffer buffs[2] = {mesh->vb, instanceInfo->vb};
        vkCmdBindVertexBuffers(scb, 0, 2, buffs, offs);
        if(mesh->icount) {
            vkCmdBindIndexBuffer(scb, mesh->vb, mesh->ioff, mesh->idxType);
            vkCmdDrawIndexed(scb, mesh->icount, instanceCount, 0, 0, 0);
        }
        else{
            vkCmdDraw(scb, mesh->vcount, instanceCount, 0, 0);
        }
        bound = nullptr;
    }

    void VkMachine::RenderPass2Cube::execute(RenderPass* other){
        if(!recording){
            LOGWITH("Renderpass not started. This message can be ignored safely if the rendering goes fine after now");
            return;
        }

        VkResult result = vkEndCommandBuffer(scb);
        if(result != VK_SUCCESS){ // 호스트/GPU 메모리 부족 문제만 존재
            LOGWITH("Secondary command buffer begin failed:",result, resultAsString(result));
            return;
        }

        VkCommandBufferBeginInfo cbInfo{};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cb, &cbInfo);
        if(result != VK_SUCCESS){ // 호스트/GPU 메모리 부족 문제만 존재
            LOGWITH("Primary Command buffer begin failed:",result,resultAsString(result));
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
        if((result = vkEndCommandBuffer(scb)) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",result);
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

        if((result = vkResetFences(singleton->device, 1, &fence)) != VK_SUCCESS){
            LOGWITH("Failed to reset fence. waiting or other operations will play incorrect");
            return;
        }

        if ((result = singleton->qSubmit(true, 1, &submitInfo, fence)) != VK_SUCCESS) {
            LOGWITH("Failed to submit command buffer");
            return;
        }

        recording = false;
    }

    bool VkMachine::RenderPass2Cube::wait(uint64_t timeout){
        return vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, timeout) == VK_SUCCESS; // VK_TIMEOUT이나 VK_ERROR_DEVICE_LOST
    }

    void VkMachine::RenderPass2Cube::start(){
        if(recording) {
            LOGWITH("Invalid call. The renderpass already started");
            return;
        }
        bound = nullptr;
        if(!pipeline) {
            LOGWITH("Pipeline not set:",this);
            return;
        }
        VkResult result;
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
        result = vkBeginCommandBuffer(scb, &cbInfo);
        if(result != VK_SUCCESS){
            recording = false;
            LOGWITH("Failed to begin secondary command buffer:",result, resultAsString(result));
            return;
        }
        
        vkCmdBindPipeline(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetViewport(scb, 0, 1, &viewport);
        vkCmdSetScissor(scb, 0, 1, &scissor);
    }

    VkMachine::RenderPass2Screen::RenderPass2Screen(VkRenderPass rp, std::vector<RenderTarget*>&& targets, std::vector<VkFramebuffer>&& fbs, VkImage dsImage, VkImageView dsView, VmaAllocation dsAlloc)
    : targets(targets), fbs(fbs), dsImage(dsImage), dsView(dsView), dsAlloc(dsAlloc), rp(rp){
        for(VkFence& fence: fences) fence = singleton->createFence(true);
        for(VkSemaphore& semaphore: acquireSm) semaphore = singleton->createSemaphore();
        for(VkSemaphore& semaphore: drawSm) semaphore = singleton->createSemaphore();
        singleton->allocateCommandBuffers(COMMANDBUFFER_COUNT, true, true, cbs);
        pipelines.resize(this->targets.size() + 1, VK_NULL_HANDLE);
        pipelineLayouts.resize(this->targets.size() + 1, VK_NULL_HANDLE);
        setViewport(singleton->swapchain.extent.width, singleton->swapchain.extent.height, 0.0f, 0.0f);
        setScissor(singleton->swapchain.extent.width, singleton->swapchain.extent.height, 0, 0);
        width = scissor.extent.width;
        height = scissor.extent.height;
    }

    VkMachine::RenderPass2Screen::~RenderPass2Screen(){
        for(VkFence& fence: fences) { vkDestroyFence(singleton->device, fence, nullptr); fence = VK_NULL_HANDLE; }
        for(VkSemaphore& semaphore: acquireSm) { vkDestroySemaphore(singleton->device, semaphore, nullptr); semaphore = VK_NULL_HANDLE; }
        for(VkSemaphore& semaphore: drawSm) { vkDestroySemaphore(singleton->device, semaphore, nullptr); semaphore = VK_NULL_HANDLE; }
        for(VkFramebuffer& fb: fbs){ vkDestroyFramebuffer(singleton->device, fb, nullptr); }
        for(RenderTarget* target: targets){ delete target; }
        vkDestroyImageView(singleton->device, dsView, nullptr);
        vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
        vkDestroyRenderPass(singleton->device, rp, nullptr);
        rp = VK_NULL_HANDLE; // null 초기화의 이유: 해제 이전에 소멸자를 따로 호출하는 데에도 사용하고 있기 때문 (2중 해제 방지)
        dsView = VK_NULL_HANDLE;
        dsImage = VK_NULL_HANDLE;
        dsAlloc = nullptr;
        fbs.clear();
        targets.clear();
    }

    bool VkMachine::RenderPass2Screen::reconstructFB(uint32_t width, uint32_t height){
        for(VkFramebuffer& fb: fbs) { vkDestroyFramebuffer(singleton->device, fb, nullptr); fb = VK_NULL_HANDLE; }
        const bool SHOULD_RECREATE_IMG = (this->width != width) || (this->height != height);
        if(SHOULD_RECREATE_IMG){
            this->width = width;
            this->height = height;
            vkDestroyImageView(singleton->device, dsView, nullptr);
            vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
            bool useFinalDepth = dsView != VK_NULL_HANDLE;
            dsView = VK_NULL_HANDLE;
            dsImage = VK_NULL_HANDLE;
            dsAlloc = nullptr;

            // ^^^ 스왑이 있으므로 위 파트는 이론상 없어도 알아서 해제되지만 있는 편이 메모리 때문에 더 좋을 것 같음

            std::vector<RenderTargetType> types(targets.size());
            struct bool8{bool b;};
            std::vector<bool8> useDepth(targets.size());
            for(uint32_t i = 0; i < targets.size(); i++){
                types[i] = targets[i]->type;
                useDepth[i].b = bool((int)targets[i]->type & 0b1000);
                delete targets[i];
            }
            targets.clear();
            RenderPass2Screen* newDat = singleton->createRenderPass2Screen(types.data(), pipelines.size(), INT32_MIN, useFinalDepth, (bool*)useDepth.data());
            if(!newDat) {
                this->~RenderPass2Screen();
                return false;
            }
            // 얕은 복사
            fbs.swap(newDat->fbs);
            targets.swap(newDat->targets);
            // 파이프라인과 레이아웃은 유지
#define SHALLOW_SWAP(a) std::swap(a, newDat->a)
            SHALLOW_SWAP(dsImage);
            SHALLOW_SWAP(dsView);
            SHALLOW_SWAP(dsAlloc);
            SHALLOW_SWAP(viewport);
            SHALLOW_SWAP(scissor);
#undef SHALLOW_SWAP
            delete newDat; // 문제점: 의미없이 펜스, 세마포어 등이 생성되었다 없어짐
            return true;
        }
        else{ // 새 스왑체인으로 프레임버퍼만 재생성
            fbs.resize(singleton->swapchain.imageView.size());
            std::vector<VkImageView> ivs;
            ivs.reserve(pipelines.size()*4);
            uint32_t totalAttachments = 0;
            for(RenderTarget* targ: targets) {
                if(targ->color1) {
                    ivs.push_back(targ->color1->view);
                    totalAttachments++;
                    if(targ->color2) {
                        ivs.push_back(targ->color2->view);
                        totalAttachments++;
                        if(targ->color3){
                            ivs.push_back(targ->color3->view);
                            totalAttachments++;
                        }
                    }
                }
                if(targ->depthstencil) {
                    ivs.push_back(targ->depthstencil->view);
                    totalAttachments++;
                }
            }
            VkImageView& swapchainImageViewPlace = ivs[totalAttachments];
            totalAttachments++;
            ivs[totalAttachments] = dsView;
            if(dsView) {
                totalAttachments++;
            }

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.attachmentCount = totalAttachments;
            fbInfo.pAttachments = ivs.data();
            fbInfo.renderPass = rp;
            fbInfo.width = width;
            fbInfo.height = height;
            fbInfo.layers = 1;
            uint32_t i = 0;
            VkResult result;
            for(VkFramebuffer& fb: fbs){
                swapchainImageViewPlace = singleton->swapchain.imageView[i++];
                if((result = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb)) != VK_SUCCESS){
                    LOGWITH("Failed to create framebuffer:",result,resultAsString(result));
                    this->~RenderPass2Screen();
                    return false;
                }
            }
            return true;
        }
    }

    void VkMachine::RenderPass2Screen::setViewport(float width, float height, float x, float y, bool applyNow){
        viewport.height = height;
        viewport.width = width;
        viewport.maxDepth = 1.0f;
        viewport.minDepth = 0.0f;
        viewport.x = x;
        viewport.y = y;
        if(applyNow && currentPass != -1) {
            vkCmdSetViewport(cbs[currentCB], 0, 1, &viewport);
        }
    }

    void VkMachine::RenderPass2Screen::setScissor(uint32_t width, uint32_t height, int32_t x, int32_t y, bool applyNow){
        scissor.extent.width = width;
        scissor.extent.height = height;
        scissor.offset.x = x;
        scissor.offset.y = y;
        if(applyNow && currentPass != -1) {
            vkCmdSetScissor(cbs[currentCB], 0, 1, &scissor);
        }
    }

    void VkMachine::RenderPass2Screen::bind(uint32_t pos, UniformBuffer* ub, uint32_t ubPos){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        ub->sync();
        uint32_t off = ub->offset(ubPos);
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, 1, &ub->dset, ub->isDynamic, &off);
    }

    void VkMachine::RenderPass2Screen::bind(uint32_t pos, const pTexture& tx) {
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass2Screen::bind(uint32_t pos, RenderTarget* target, uint32_t index){
        if(currentPass == -1){
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
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, 1, &dset, 0, nullptr);
    }

     void VkMachine::RenderPass2Screen::invoke(const pMesh& mesh){
         if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if((bound != mesh.get()) && (mesh->vb != VK_NULL_HANDLE)) {
            VkDeviceSize offs = 0;
            vkCmdBindVertexBuffers(cbs[currentCB], 0, 1, &mesh->vb, &offs);
            vkCmdBindVertexBuffers(cbs[currentCB], 0, 1, &mesh->vb, &offs);
            if(mesh->icount) vkCmdBindIndexBuffer(cbs[currentCB], mesh->vb, mesh->ioff, mesh->idxType);
        }
        if(mesh->icount) vkCmdDrawIndexed(cbs[currentCB], mesh->icount, 1, 0, 0, 0);
        else vkCmdDraw(cbs[currentCB], mesh->vcount, 1, 0, 0);
        bound = mesh.get();
    }

    void VkMachine::RenderPass2Screen::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount){
         if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        VkDeviceSize offs[2] = {0, 0};
        VkBuffer buffs[2] = {mesh->vb, instanceInfo->vb};
        vkCmdBindVertexBuffers(cbs[currentCB], 0, 2, buffs, offs);
        if(mesh->icount) {
            vkCmdBindIndexBuffer(cbs[currentCB], mesh->vb, mesh->ioff, mesh->idxType);
            vkCmdDrawIndexed(cbs[currentCB], mesh->icount, instanceCount, 0, 0, 0);
        }
        else{
            vkCmdDraw(cbs[currentCB], mesh->vcount, instanceCount, 0, 0);
        }
        bound = nullptr;
    }

    void VkMachine::RenderPass2Screen::start(uint32_t pos){
        if(currentPass == targets.size()) {
            LOGWITH("Invalid call. The last subpass already started");
            return;
        }
        if(!singleton->swapchain.handle) {
            LOGWITH("Swapchain not ready. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        currentPass++;
        if(!pipelines[currentPass]) {
            LOGWITH("Pipeline not set.");
            currentPass--;
            return;
        }
        VkResult result;
        if(currentPass == 0){
            result = vkAcquireNextImageKHR(singleton->device, singleton->swapchain.handle, UINT64_MAX, acquireSm[currentCB], VK_NULL_HANDLE, &imgIndex);
            if(result != VK_SUCCESS) {
                LOGWITH("Failed to acquire swapchain image:",result,resultAsString(result),"\nThis message can be ignored safely if the rendering goes fine after now");
                currentPass = -1;
                return;
            }

            vkWaitForFences(singleton->device, 1, &fences[currentCB], VK_FALSE, UINT64_MAX);
            vkResetCommandBuffer(cbs[currentCB], 0);
            VkCommandBufferBeginInfo cbInfo{};
            cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cbInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = vkBeginCommandBuffer(cbs[currentCB], &cbInfo);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to begin command buffer:",result,resultAsString(result));
                currentPass = -1;
                return;
            }
            VkRenderPassBeginInfo rpInfo{};

            std::vector<VkClearValue> clearValues; // TODO: 이건 렌더타겟이 갖고 있는 게 자유도 면에서 나을 것 같음
            clearValues.reserve(targets.size() * 4 + 2);
            for(RenderTarget* targ: targets){
                if((int)targ->type & 0b1) {
                    clearValues.push_back({0.03f, 0.03f, 0.03f, 0.0f});
                    if((int)targ->type & 0b10){
                        clearValues.push_back({0.03f, 0.03f, 0.03f, 0.0f});
                        if((int)targ->type & 0b100){
                            clearValues.push_back({0.03f, 0.03f, 0.03f, 0.0f});
                        }
                    }
                }
                if((int)targ->type & 0b1000){
                    clearValues.push_back({1.0f, 0u});
                }
            }

            clearValues.push_back({0.03f, 0.03f, 0.03f, 1.0f});
            if(dsImage) clearValues.push_back({1.0f, 0u});

            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.framebuffer = fbs[imgIndex];
            rpInfo.pClearValues = clearValues.data(); // TODO: 렌더패스 첨부물 인덱스에 대응하게 준비해야 함
            rpInfo.clearValueCount = clearValues.size();
            rpInfo.renderArea.offset = {0,0};
            rpInfo.renderArea.extent = singleton->swapchain.extent;
            rpInfo.renderPass = rp;

            vkCmdBeginRenderPass(cbs[currentCB], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        }
        else{
            vkCmdNextSubpass(cbs[currentCB], VK_SUBPASS_CONTENTS_INLINE);
            VkDescriptorSet dset[4];
            uint32_t count = targets[currentPass - 1]->getDescriptorSets(dset);
            vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts[currentPass], pos, count, dset, 0, nullptr);
        }
        vkCmdBindPipeline(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]);
        vkCmdSetViewport(cbs[currentCB], 0, 1, &viewport);
        vkCmdSetScissor(cbs[currentCB], 0, 1, &scissor);
    }

    void VkMachine::RenderPass2Screen::execute(RenderPass* other){
        if(currentPass != pipelines.size() - 1){
            LOGWITH("Renderpass not ready to execute. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        vkCmdEndRenderPass(cbs[currentCB]);
        bound = nullptr;
        VkResult result;
        if((result = vkEndCommandBuffer(cbs[currentCB])) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",result,resultAsString(result));
            return;
        }

        if(!singleton->swapchain.handle){
            LOGWITH("Swapchain is not ready. This message can be ignored safely if the rendering goes fine after now");
            return;
        }

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cbs[currentCB];
        VkSemaphore waits[2];
        waits[0] = acquireSm[currentCB];
        submitInfo.pWaitSemaphores = waits;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitDstStageMask = waitStages;
        if(other){
            submitInfo.waitSemaphoreCount = 2;
            waits[1] = other->semaphore;
        }
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &drawSm[currentCB];

        if((result = vkResetFences(singleton->device, 1, &fences[currentCB])) != VK_SUCCESS){
            LOGWITH("Failed to reset fence. waiting or other operations will play incorrect:",result, resultAsString(result));
            return;
        }

        if ((result = singleton->qSubmit(true, 1, &submitInfo, fences[currentCB])) != VK_SUCCESS) {
            LOGWITH("Failed to submit command buffer:",result,resultAsString(result));
            return;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &singleton->swapchain.handle;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &drawSm[currentCB];
        presentInfo.pImageIndices = &imgIndex;

        recently = currentCB;
        currentCB = (currentCB + 1) % COMMANDBUFFER_COUNT;
        currentPass = -1;

        if((result = singleton->qSubmit(&presentInfo)) != VK_SUCCESS){
            LOGWITH("Failed to submit command present operation:",result, resultAsString(result));
            return;
        }
    }

    void VkMachine::RenderPass2Screen::push(void* input, uint32_t start, uint32_t end){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdPushConstants(cbs[currentCB], pipelineLayouts[currentPass], VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT, start, end - start, input); // TODO: 스펙: 파이프라인 레이아웃 생성 시 단계마다 가용 푸시상수 범위를 분리할 수 있으며(꼭 할 필요는 없는 듯 하긴 함) 여기서 매개변수로 범위와 STAGEFLAGBIT은 일치해야 함
    }

    void VkMachine::RenderPass2Screen::usePipeline(VkPipeline pipeline, VkPipelineLayout layout, uint32_t subpass){
        if(subpass > targets.size()){
            LOGWITH("Invalid subpass. This renderpass has", targets.size() + 1, "subpasses but", subpass, "given");
            return;
        }
        pipelines[subpass] = pipeline;
        pipelineLayouts[subpass] = layout;
        if(currentPass == subpass) { vkCmdBindPipeline(cbs[currentCB], VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); }
    }

    bool VkMachine::RenderPass2Screen::wait(uint64_t timeout){
        return vkWaitForFences(singleton->device, 1, &fences[recently], VK_FALSE, timeout) == VK_SUCCESS; // VK_TIMEOUT이나 VK_ERROR_DEVICE_LOST
    }

    VkMachine::UniformBuffer::UniformBuffer(uint32_t length, uint32_t individual, VkBuffer buffer, VkDescriptorSetLayout layout, VkDescriptorSet dset, VmaAllocation alloc, void* mmap, uint32_t binding)
    :length(length), individual(individual), buffer(buffer), layout(layout), dset(dset), alloc(alloc), isDynamic(length > 1), mmap(mmap), binding(binding) {
        staged.resize(individual * length);
        std::vector<uint16_t> inds;
        inds.reserve(length);
        indices = std::move(decltype(indices)(std::greater<uint16_t>(), std::move(inds)));
        for(uint32_t i = 1; i <= length ; i++){ indices.push(i); }
    }

    uint16_t VkMachine::UniformBuffer::getIndex() {
        if(!isDynamic) return 0;
        if(indices.empty()){ resize(length * 3 / 2); }

        uint16_t ret = indices.top();
        if(ret >= length) {
            decltype(indices) sw;
            indices.swap(sw);
            resize(length * 3 / 2);
            ret = indices.top();
        }
        indices.pop();
        return ret;
    }

    void VkMachine::UniformBuffer::update(const void* input, uint32_t index, uint32_t offset, uint32_t size){
        std::memcpy(&staged[index * individual + offset], input, size);
        shouldSync = true;
    }

    void VkMachine::UniformBuffer::sync(){
        if(!shouldSync) return;
        std::memcpy(mmap, staged.data(), staged.size());
        vmaInvalidateAllocation(singleton->allocator, alloc, 0, VK_WHOLE_SIZE);
        vmaFlushAllocation(singleton->allocator, alloc, 0, VK_WHOLE_SIZE);
        shouldSync = false;
    }

    void VkMachine::UniformBuffer::resize(uint32_t size) {
        if(!isDynamic || size == length) return;
        shouldSync = true;
        staged.resize(individual * size);
        if(size > length) {
            for(uint32_t i = length; i < size; i++){
                indices.push(i);
            }
        }
        length = size;
        vmaUnmapMemory(singleton->allocator, alloc);
        vmaDestroyBuffer(singleton->allocator, buffer, alloc); // 이것 때문에 렌더링과 동시에 진행 불가능
        buffer = VK_NULL_HANDLE;
        mmap = nullptr;
        alloc = nullptr;

        VkResult result;
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.size = individual * size;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo bainfo{};
        bainfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        bainfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        result = vmaCreateBufferWithAlignment(singleton->allocator, &bufferInfo, &bainfo, singleton->physicalDevice.minUBOffsetAlignment, &buffer, &alloc, nullptr);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create VkBuffer:", result,resultAsString(result));
            return;
        }

        VkDescriptorBufferInfo dsNBuffer{};
        dsNBuffer.buffer = buffer;
        dsNBuffer.offset = 0;
        dsNBuffer.range = individual * length;
        VkWriteDescriptorSet wr{};
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        wr.descriptorCount = 1;
        wr.dstArrayElement = 0;
        wr.dstBinding = binding;
        wr.pBufferInfo = &dsNBuffer;
        vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);

        if((result = vmaMapMemory(singleton->allocator, alloc, &mmap)) != VK_SUCCESS){
            LOGWITH("Failed to map memory:", result,resultAsString(result));
            return;
        }
    }

    VkMachine::UniformBuffer::~UniformBuffer(){
        //vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset);
        vkDestroyDescriptorSetLayout(singleton->device, layout, nullptr);
        vmaDestroyBuffer(singleton->allocator, buffer, alloc);
    }


    // static함수들 구현

    VkInstance createInstance(Window* window){
        VkResult result;
        VkInstance instance;
        VkInstanceCreateInfo instInfo{};

        VkApplicationInfo appInfo{};
        appInfo.sType= VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pEngineName = "YERM";
        appInfo.pApplicationName = "YERM";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0,0,1,0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.engineVersion = VK_MAKE_API_VERSION(0,0,1,0);

        std::vector<const char*> windowExt = window->requiredInstanceExentsions();

        instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledExtensionCount = (uint32_t)windowExt.size();
        instInfo.ppEnabledExtensionNames = windowExt.data();

        const char* VLAYER[] = {"VK_LAYER_KHRONOS_validation"};
        if constexpr(VkMachine::USE_VALIDATION_LAYER){
            instInfo.ppEnabledLayerNames = VLAYER;
            instInfo.enabledLayerCount = 1;
        }

        if((result = vkCreateInstance(&instInfo, nullptr, &instance)) != VK_SUCCESS){
            LOGWITH("Failed to create vulkan instance:", result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        return instance;
    }

    VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool* isCpu, uint32_t* graphicsQueue, uint32_t* presentQueue, uint32_t* subQueue, uint32_t* subqIndex, uint64_t* minUBAlignment) {
        uint32_t count;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> cards(count);
        vkEnumeratePhysicalDevices(instance, &count, cards.data());

        uint64_t maxScore = 0;
        VkPhysicalDevice goodCard = VK_NULL_HANDLE;
        uint32_t maxGq = 0, maxPq = 0, maxSubq = 0;
        uint32_t maxSubqIndex = 0;
        for(VkPhysicalDevice card: cards) {

            uint32_t qfcount;
            uint64_t gq = ~0ULL, pq = ~0ULL, subq = ~0ULL;
            uint32_t si = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(card, &qfcount, nullptr);
            std::vector<VkQueueFamilyProperties> qfs(qfcount);
            vkGetPhysicalDeviceQueueFamilyProperties(card, &qfcount, qfs.data());

            // 큐 계열: GRAPHICS, PRESENT 사용이 안 되면 0점. GRAPHICS, PRESENT 사용이 같이 되면서 별도로 transfer 가능한 큐가 존재하는 것이 최상
            for(uint32_t i = 0; i < qfcount ; i++){
                if(qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    if(gq == ~0ULL) {
                        gq = i;
                        if(qfs[i].queueCount >= 2) {
                            subq = i;
                            si = 1;
                        }
                    }
                    else if(subq == ~0ULL) {
                        subq = i;
                        si = 0;
                    }
                }
                else if((qfs[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && (subq == ~0ULL)){
                    subq = i;
                    si = 0;
                }
                VkBool32 supported;
                vkGetPhysicalDeviceSurfaceSupportKHR(card, i, surface, &supported);
                if(supported) {
                    if(pq == ~0ULL){ pq = i; }
                    if(qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { // 큐 계열 하나로 다 된다면 EXCLUSIVE 모드를 사용할 수 있음
                        gq = pq = i;
                        if(qfs[i].queueCount >= 2) {
                            subq = i;
                            si = 1;
                            break;
                        }
                    }
                }
            }

            if (gq < 0 || pq < 0) continue; // 탈락
            if(subq == ~0ULL) subq = gq;

            uint64_t score = assessPhysicalDevice(card);
            if(score > maxScore) {
                maxScore = score;
                goodCard = card;
                maxGq = (uint32_t)gq;
                maxPq = (uint32_t)pq;
                maxSubq = (uint32_t)subq;
                maxSubqIndex = si;
            }
        }
        *isCpu = !(maxScore & (0b111ULL << 61));
        *graphicsQueue = maxGq;
        *presentQueue = maxPq;
        *subQueue = maxSubq;
        *subqIndex = maxSubqIndex;
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(goodCard, &props);
        *minUBAlignment = props.limits.minUniformBufferOffsetAlignment;
        return goodCard;
    }

    uint64_t assessPhysicalDevice(VkPhysicalDevice card) {
        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceProperties(card, &properties);
        vkGetPhysicalDeviceFeatures(card, &features);
        uint64_t score = 0;
        // device type: 현재 총 4비트 할당, 이후 여유를 위해 8비트로 가정함.
        switch (properties.deviceType)
        {
        case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:    // CPU와 별도의 GPU
            score |= (1ULL << 63);
            break;
        case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:     // GPU 자원의 일부가 가상화되었을 수 있음
            score |= (1ULL << 62);
            break;
        case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:  // CPU와 직접적으로 연계되는 부분이 있는 GPU (내장그래픽은 여기 포함)
            score |= (1ULL << 61);
            break;
        //case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_CPU:
        //case VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_OTHER:
        default:
            break;
        }

        // properties.limits: 대부분의 사양상의 limit은 매우 널널하여 지금은 검사하지 않음.

        // features
        if(features.textureCompressionASTC_LDR) score |= (1ULL << 54);
        if(features.textureCompressionBC) score |= (1ULL << 53);
        if(features.textureCompressionETC2) score |= (1ULL << 52);
        if(features.tessellationShader) score |= (1ULL << 51);
        if(features.geometryShader) score |= (1ULL << 50);
        return score;
    }

    VkDevice createDevice(VkPhysicalDevice card, int gq, int pq, int tq, int tqi) {
        VkDeviceQueueCreateInfo qInfo[3]{};
        float queuePriority = 1.0f;
        qInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qInfo[0].queueFamilyIndex = gq;
        qInfo[0].queueCount = 1 + tqi;
        qInfo[0].pQueuePriorities = &queuePriority;

        uint32_t qInfoCount = 1;
        
        if(gq == pq) {
            qInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo[1].queueFamilyIndex = tq;
            qInfo[1].queueCount = 1;
            qInfo[1].pQueuePriorities = &queuePriority;
            qInfoCount += (1 - tqi);
        }
        else{
            qInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo[1].queueFamilyIndex = pq;
            qInfo[1].queueCount = 1;
            qInfo[1].pQueuePriorities = &queuePriority;

            qInfoCount = 2;

            qInfo[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo[2].queueFamilyIndex = tq;
            qInfo[2].queueCount = 1;
            qInfo[2].pQueuePriorities = &queuePriority;
            
            qInfoCount += (1 - tqi);
        }

        VkPhysicalDeviceFeatures wantedFeatures{};
        VkPhysicalDeviceFeatures availableFeatures;
        vkGetPhysicalDeviceFeatures(card, &availableFeatures);
        wantedFeatures.textureCompressionASTC_LDR = availableFeatures.textureCompressionASTC_LDR;
        wantedFeatures.textureCompressionBC = availableFeatures.textureCompressionBC;
        wantedFeatures.textureCompressionETC2 = availableFeatures.textureCompressionETC2;
        wantedFeatures.tessellationShader = availableFeatures.tessellationShader;
        wantedFeatures.geometryShader = availableFeatures.geometryShader;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = qInfo;
        deviceInfo.queueCreateInfoCount = qInfoCount;
        deviceInfo.pEnabledFeatures = &wantedFeatures;
        deviceInfo.ppEnabledExtensionNames = VK_DESIRED_DEVICE_EXT;
        deviceInfo.enabledExtensionCount = sizeof(VK_DESIRED_DEVICE_EXT) / sizeof(VK_DESIRED_DEVICE_EXT[0]);

        VkDevice ret;
        VkResult result;
        if((result = vkCreateDevice(card, &deviceInfo, nullptr, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create Vulkan device:", result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        return ret;
    }

    VmaAllocator createAllocator(VkInstance instance, VkPhysicalDevice card, VkDevice device){
        VmaAllocator ret;

        VmaAllocatorCreateInfo allocInfo{};
        allocInfo.instance = instance;
        allocInfo.physicalDevice = card;
        allocInfo.device = device;
        allocInfo.vulkanApiVersion = VK_API_VERSION_1_0;
        VkResult result;
        if((result = vmaCreateAllocator(&allocInfo, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create VMA object:",result,resultAsString(result));
            return nullptr;
        }
        return ret;
    }

    VkCommandPool createCommandPool(VkDevice device, int qIndex) {
        VkCommandPool ret;
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = qIndex;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VkResult result;
        if((result = vkCreateCommandPool(device, &poolInfo, nullptr, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create command pool:", result,resultAsString(result));
            return 0;
        }
        return ret;
    }

    VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType type, VkFormat format, int levelCount, int layerCount, VkImageAspectFlags aspect, VkComponentMapping swizzle){
        VkImageViewCreateInfo ivInfo{};
        ivInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivInfo.format = format;
        ivInfo.image = image;
        ivInfo.viewType = type;
        ivInfo.subresourceRange.aspectMask = aspect;
        ivInfo.subresourceRange.baseArrayLayer = 0;
        ivInfo.subresourceRange.layerCount = layerCount;
        ivInfo.subresourceRange.levelCount = levelCount;
        ivInfo.components = swizzle;

        VkImageView ret;
        VkResult result;
        if((result = vkCreateImageView(device, &ivInfo, nullptr, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create image view:",result,resultAsString(result));
            return 0;
        }
        return ret;
    }

    VkDescriptorPool createDescriptorPool(VkDevice device, uint32_t samplerLimit, uint32_t dynUniLimit, uint32_t uniLimit, uint32_t intputAttachmentLimit){
        VkDescriptorPoolSize sizeInfo[4]{};
        sizeInfo[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        sizeInfo[0].descriptorCount = samplerLimit;
        sizeInfo[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; // 환경 무관하게 최소 보장되는 값이 8
        sizeInfo[1].descriptorCount = dynUniLimit;
        sizeInfo[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // 사용할 일이 별로 없을 것 같음
        sizeInfo[2].descriptorCount = uniLimit;
        sizeInfo[3].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; // 프로그램 내에서 굳이 그렇게 많은 디스크립터를 사용할 것 같진 않음
        sizeInfo[3].descriptorCount = intputAttachmentLimit;

        VkDescriptorPoolCreateInfo dPoolInfo{};
        dPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        dPoolInfo.maxSets = samplerLimit + dynUniLimit + uniLimit + intputAttachmentLimit;
        dPoolInfo.pPoolSizes = sizeInfo;
        dPoolInfo.poolSizeCount = sizeof(sizeInfo) / sizeof(VkDescriptorPoolSize);
        VkDescriptorPool ret;
        VkResult result;
        if((result = vkCreateDescriptorPool(device, &dPoolInfo, nullptr, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create descriptor pool:",result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        return ret;
    }

    bool isThisFormatAvailable(VkPhysicalDevice physicalDevice, VkFormat format, uint32_t x, uint32_t y, VkImageCreateFlagBits flags = (VkImageCreateFlagBits)0) {
    VkImageFormatProperties props;
    VkResult result = vkGetPhysicalDeviceImageFormatProperties(
        physicalDevice,
        format,
        VK_IMAGE_TYPE_2D,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        flags, // 경우에 따라 VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
        &props
    );
        return (result != VK_ERROR_FORMAT_NOT_SUPPORTED) &&
            (props.maxExtent.width >= x) &&
            (props.maxExtent.height >= y);
    }

    VkFormat textureFormatFallback(VkPhysicalDevice physicalDevice, int x, int y, uint32_t nChannels, bool srgb, bool hq, VkImageCreateFlagBits flags) {
    #define CHECK_N_RETURN(f) if(isThisFormatAvailable(physicalDevice,f,x,y,flags)) return f
        switch (nChannels)
        {
        case 4:
        if(srgb){
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            if(hq) return VK_FORMAT_R8G8B8A8_SRGB;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC3_SRGB_BLOCK);
            return VK_FORMAT_R8G8B8A8_SRGB;
        }
        else{
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return VK_FORMAT_R8G8B8A8_UNORM;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC3_UNORM_BLOCK);
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
            break;
        case 3:
        if(srgb){
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            if(hq) return VK_FORMAT_R8G8B8_SRGB;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
            return VK_FORMAT_R8G8B8_SRGB;
        }
        else{
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return VK_FORMAT_R8G8B8_UNORM;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
            return VK_FORMAT_R8G8B8_UNORM;
        }
        case 2:
        if(srgb){
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            return VK_FORMAT_R8G8_SRGB;
        }
        else{
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return VK_FORMAT_R8G8_UNORM;
            CHECK_N_RETURN(VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC5_UNORM_BLOCK);
            return VK_FORMAT_R8G8_UNORM;
        }
        case 1:
        if(srgb){
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            return VK_FORMAT_R8_SRGB;
        }
        else{
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return VK_FORMAT_R8_UNORM;
            CHECK_N_RETURN(VK_FORMAT_EAC_R11_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC4_UNORM_BLOCK);
            return VK_FORMAT_R8_UNORM;
        }
        default:
            return VK_FORMAT_UNDEFINED;
        }
    #undef CHECK_N_RETURN
    }

    VkPipeline createPipeline(VkDevice device, VkVertexInputAttributeDescription* vinfo, uint32_t size, uint32_t vattr,
    VkVertexInputAttributeDescription* iinfo, uint32_t isize, uint32_t iattr,
    VkRenderPass pass, uint32_t subpass, uint32_t flags, const uint32_t OPT_COLOR_COUNT, const bool OPT_USE_DEPTHSTENCIL,
    VkPipelineLayout layout, VkShaderModule vs, VkShaderModule fs, VkShaderModule tc, VkShaderModule te, VkShaderModule gs, VkStencilOpState* front, VkStencilOpState* back) {
        VkPipelineShaderStageCreateInfo shaderStagesInfo[5] = {};
        shaderStagesInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStagesInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStagesInfo[0].module = vs;
        shaderStagesInfo[0].pName = "main";

        uint32_t lastStage = 1;

        if(tc) {
            shaderStagesInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStagesInfo[1].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            shaderStagesInfo[1].module = tc;
            shaderStagesInfo[1].pName = "main";
            shaderStagesInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStagesInfo[2].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            shaderStagesInfo[2].module = te;
            shaderStagesInfo[2].pName = "main";
            lastStage = 3;
        }
        if(gs) {
            shaderStagesInfo[lastStage].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStagesInfo[lastStage].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            shaderStagesInfo[lastStage].module = gs;
            shaderStagesInfo[lastStage].pName = "main";
            lastStage++;
        }

        shaderStagesInfo[lastStage].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStagesInfo[lastStage].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStagesInfo[lastStage].module = fs;
        shaderStagesInfo[lastStage].pName = "main";
        lastStage++;

        VkVertexInputBindingDescription vbind[2]{};
        vbind[0].binding = 0;
        vbind[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vbind[0].stride = size;

        vbind[1].binding = 1;
        vbind[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        vbind[1].stride = isize;
        
        std::vector<VkVertexInputAttributeDescription> attrs(vattr + iattr);
        if(vattr) std::copy(vinfo, vinfo + vattr, attrs.data());
        if(iattr) std::copy(iinfo, iinfo + iattr, attrs.data() + vattr);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = (vattr ? 1 : 0) + (iattr ? 1 : 0);
        vertexInputInfo.pVertexBindingDescriptions = vattr ? vbind : vbind + 1;
        vertexInputInfo.vertexAttributeDescriptionCount = attrs.size();
        vertexInputInfo.pVertexAttributeDescriptions = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        VkPipelineRasterizationStateCreateInfo rtrInfo{};
        rtrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rtrInfo.cullMode= VK_CULL_MODE_BACK_BIT;
        rtrInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rtrInfo.lineWidth = 1.0f;
        rtrInfo.polygonMode = VK_POLYGON_MODE_FILL;

        VkPipelineDepthStencilStateCreateInfo dsInfo{};
        if(OPT_USE_DEPTHSTENCIL){
            dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            dsInfo.depthCompareOp = VK_COMPARE_OP_LESS;
            dsInfo.depthTestEnable = (flags & VkMachine::PipelineOptions::USE_DEPTH) ? VK_TRUE : VK_FALSE;
            dsInfo.depthWriteEnable = dsInfo.depthWriteEnable;
            dsInfo.stencilTestEnable = (flags & VkMachine::PipelineOptions::USE_STENCIL) ? VK_TRUE : VK_FALSE;
            if(front) dsInfo.front = *front;
            if(back) dsInfo.back = *back;
        }

        VkPipelineColorBlendAttachmentState blendStates[3]{};
        for(VkPipelineColorBlendAttachmentState& blendInfo: blendStates){
            blendInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendInfo.colorBlendOp = VK_BLEND_OP_ADD;
            blendInfo.alphaBlendOp = VK_BLEND_OP_ADD;
            blendInfo.blendEnable = VK_TRUE;
            blendInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blendInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blendInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        }

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.attachmentCount = OPT_COLOR_COUNT;
        colorBlendStateCreateInfo.pAttachments = blendStates;

        VkDynamicState dynStates[2] = {VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT, VkDynamicState::VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynInfo{};
        dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynInfo.pDynamicStates = dynStates;
        dynInfo.dynamicStateCount = sizeof(dynStates) / sizeof(dynStates[0]);

        VkPipelineViewportStateCreateInfo viewportInfo{};
        viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportInfo.viewportCount = 1;
        viewportInfo.scissorCount = 1;

        VkPipelineMultisampleStateCreateInfo msInfo{};
        msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // TODO: 선택권

        VkPipelineTessellationStateCreateInfo tessInfo{};
        tessInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        tessInfo.patchControlPoints = 3; // TODO: 선택권

        VkGraphicsPipelineCreateInfo pInfo{};
        pInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pInfo.stageCount = lastStage;
        pInfo.pStages = shaderStagesInfo;
        pInfo.pVertexInputState = &vertexInputInfo;
        pInfo.renderPass = pass;
        pInfo.subpass = subpass;
        pInfo.pDynamicState = &dynInfo;
        pInfo.layout = layout;
        pInfo.pRasterizationState = &rtrInfo;
        pInfo.pViewportState = &viewportInfo;
        pInfo.pMultisampleState = &msInfo;
        pInfo.pInputAssemblyState = &inputAssemblyInfo;
        if(tc) pInfo.pTessellationState = &tessInfo;
        if(OPT_COLOR_COUNT) { pInfo.pColorBlendState = &colorBlendStateCreateInfo; }
        if(OPT_USE_DEPTHSTENCIL){ pInfo.pDepthStencilState = &dsInfo; }
        VkPipeline ret;
        VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pInfo, nullptr, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create pipeline:",result,resultAsString(result));
            return VK_NULL_HANDLE;
        }
        return ret;
    }

    static const char* resultAsString(VkResult result) {
        switch (result)
        {
        case VK_SUCCESS:
            return "success";
        case VK_NOT_READY:
            return "not ready";
        case VK_TIMEOUT:
            return "timeout";
        case VK_EVENT_SET:
            return "event set";
        case VK_EVENT_RESET:
            return "event reset";
        case VK_INCOMPLETE:
            return "incomplete";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "out of host memory";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "out of device memory";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "initialization failed";
        case VK_ERROR_DEVICE_LOST:
            return "device lost";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "memory map failed";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "layer not present";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "extension not present";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "feature not present";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "incompatible driver";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "too many objects";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "format not supported";
        case VK_ERROR_FRAGMENTED_POOL:
            return "fragmented pool";
        case VK_ERROR_UNKNOWN:
            return "unknown";
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            return "out of pool memory";
        case VK_ERROR_INVALID_EXTERNAL_HANDLE:
            return "invalid external handle";
        case VK_ERROR_FRAGMENTATION:
            return "fragmentation";
        case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
            return "invalid opaque capture address";
        case VK_PIPELINE_COMPILE_REQUIRED:
            return "pipeline compile required";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "surface lost";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "native window in use";
        case VK_SUBOPTIMAL_KHR:
            return "swapchain suboptimal";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "swapchain out of date";
        case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
            return "incompatible display";
        case VK_ERROR_VALIDATION_FAILED_EXT:
            return "validation failed";
        case VK_ERROR_INVALID_SHADER_NV:
            return "invalid shader";
        case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
            return "invalid DRM format modifier plane layout";
        case VK_ERROR_NOT_PERMITTED_KHR:
            return "not permitted";
        case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
            return "full screen exclusive mode lost";
        case VK_THREAD_IDLE_KHR:
            return "thread idle";
        case VK_THREAD_DONE_KHR:
            return "thread done";
        case VK_OPERATION_DEFERRED_KHR:
            return "operation deferred";
        case VK_OPERATION_NOT_DEFERRED_KHR:
            return "operation not deferred";
        case VK_RESULT_MAX_ENUM:
            return "VK_RESULT_MAX_ENUM";
        default:
            return "not a VkResult code";
        }
        return "not a VkResult code";
    }

}