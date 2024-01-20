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
#include "../externals/single_header/stb_image.h"

#if !BOOST_PLAT_ANDROID
#ifndef KHRONOS_STATIC
#define KHRONOS_STATIC
#endif
#endif
#include "../externals/ktx/include/ktx.h"

#include <algorithm>
#include <vector>

namespace onart {

    /// @brief Vulkan 인스턴스를 생성합니다. 자동으로 호출됩니다.
    static VkInstance createInstance();
    /// @brief 사용할 Vulkan 물리 장치를 선택합니다. CPU 기반인 경우 경고를 표시하지만 선택에 실패하지는 않습니다.
    static VkPhysicalDevice findPhysicalDevice(VkInstance, bool*, uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint64_t*);
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
    static VkFormat textureFormatFallback(VkPhysicalDevice physicalDevice, int x, int y, uint32_t nChannels, bool srgb, VkMachine::TextureFormatOptions hq, VkImageCreateFlagBits flags);
    /// @brief VkResult를 스트링으로 표현합니다. 리턴되는 문자열은 텍스트(코드) 영역에 존재합니다.
    inline static const char* resultAsString(VkResult);

    /// @brief 활성화할 장치 확장
    constexpr const char* VK_DESIRED_DEVICE_EXT[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };


    VkMachine* VkMachine::singleton = nullptr;
    thread_local VkResult VkMachine::reason = VK_SUCCESS;

    VkMachine::VkMachine(){
        if(singleton) {
            LOGWITH("Tried to create multiple VkMachine objects");
            return;
        }

        if(!(instance = createInstance())) {
            return;
        }

        /*if ((reason = window->createWindowSurface(instance, &surface.handle)) != VK_SUCCESS) {
            LOGWITH("Failed to create Window surface:", reason,resultAsString(reason));
            free();
            return;
        }*/

        bool isCpu;
        if(!(physicalDevice.card = findPhysicalDevice(instance, &isCpu, &physicalDevice.gq, &physicalDevice.pq, &physicalDevice.subq, &physicalDevice.subqIndex, &physicalDevice.minUBOffsetAlignment))) { // TODO: 모든 가용 graphics/transfer 큐 정보를 저장해 두고 버퍼/텍스처 등 자원 세팅은 다른 큐를 사용하게 만들자
            LOGWITH("Couldn\'t find any appropriate graphics device");
            free();
            reason = VK_RESULT_MAX_ENUM;
            return;
        }
        if(isCpu) LOGWITH("Warning: this device is CPU");
        // properties.limits.minMemorymapAlignment, minTexelBufferOffsetAlignment, minUniformBufferOffsetAlignment, minStorageBufferOffsetAlignment, optimalBufferCopyOffsetAlignment, optimalBufferCopyRowPitchAlignment를 저장

        vkGetPhysicalDeviceFeatures(physicalDevice.card, &physicalDevice.features);

        if(!(device = createDevice(physicalDevice.card, physicalDevice.gq, physicalDevice.pq, physicalDevice.subq, physicalDevice.subqIndex))) {
            free();
            return;
        }

        vkGetDeviceQueue(device, physicalDevice.gq, 0, &graphicsQueue);
        vkGetDeviceQueue(device, physicalDevice.pq, 0, &presentQueue);
        vkGetDeviceQueue(device, physicalDevice.subq, physicalDevice.subqIndex, &transferQueue);
        gqIsTq = (graphicsQueue == transferQueue);
        pqIsTq = (graphicsQueue == presentQueue);
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

        if(!(descriptorPool = createDescriptorPool(device))){
            free();
            return;
        }

        if(!createSamplers()){
            free();
            return;
        }

        singleton = this;
    }

    void VkMachine::setVsync(bool vsync) {
        if (singleton->vsync != vsync) {
            singleton->vsync = vsync;
            for (auto& w : singleton->windowSystems) {
                w.second->needReset = true;
            }
        }
    }

    bool VkMachine::addWindow(int32_t key, Window* window) {
        if (windowSystems.find(key) != windowSystems.end()) { return true; }
        auto w = new WindowSystem(window);
        if (w->swapchain.handle) {
            windowSystems[key] = w;
            if (windowSystems.size() == 1) {
                baseSurfaceRendertargetFormat = w->surface.format.format;
            }
            return true;
        }
        else {
            delete w;
            return false;
        }
    }

    void VkMachine::removeWindow(int32_t key) {
        bool waited = false;
        for (auto it = finalPasses.begin(); it != finalPasses.end();) {
            if (it->second->windowIdx == key) {
                if (!waited) {
                    vkDeviceWaitIdle(singleton->device);
                    waited = true;
                }
                delete it->second;
                finalPasses.erase(it++);
            }
            else {
                ++it;
            }
        }
        windowSystems.erase(key);
        if (windowSystems.size() == 1) {
            baseSurfaceRendertargetFormat = windowSystems.begin()->second->surface.format.format;
        }
    }

    VkFence VkMachine::createFence(bool signaled){
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if(signaled) fenceInfo.flags = VkFenceCreateFlagBits::VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence ret;
        reason = vkCreateFence(device, &fenceInfo, VK_NULL_HANDLE, &ret);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create fence:",reason,resultAsString(reason));
            return VK_NULL_HANDLE;
        }
        return ret;
    }

    VkSemaphore VkMachine::createSemaphore(){
        VkSemaphoreCreateInfo smInfo{};
        smInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore ret;
        reason = vkCreateSemaphore(device, &smInfo, VK_NULL_HANDLE, &ret);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create fence:",reason,resultAsString(reason));
            return VK_NULL_HANDLE;
        }
        return ret;
    }

    VkMachine::Pipeline* VkMachine::getPipeline(int32_t name){
        auto it = singleton->pipelines.find(name);
        if(it != singleton->pipelines.end()) return it->second;
        else return VK_NULL_HANDLE;
    }

    VkMachine::pMesh VkMachine::getMesh(int32_t name) {
        auto it = singleton->meshes.find(name);
        if(it != singleton->meshes.end()) return it->second;
        else return pMesh();
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

    VkMachine::pTexture VkMachine::getTexture(int32_t name){
        std::unique_lock<std::mutex> _(singleton->textureGuard);
        auto it = singleton->textures.find(name);
        if (it != singleton->textures.end()) return it->second;
        else return pTexture();
    }

    VkMachine::pTextureSet VkMachine::getTextureSet(int32_t name) {
        auto it = singleton->textureSets.find(name);
        if (it != singleton->textureSets.end()) return it->second;
        else return pTextureSet();
    }

    VkMachine::pStreamTexture VkMachine::getStreamTexture(int32_t name) {
        auto it = singleton->streamTextures.find(name);
        if (it != singleton->streamTextures.end()) return it->second;
        else return {};
    }

    void VkMachine::allocateCommandBuffers(int count, bool isPrimary, bool fromGraphics, VkCommandBuffer* buffers){
        VkCommandBufferAllocateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.level = isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        bufferInfo.commandPool = fromGraphics ? gCommandPool : tCommandPool;
        bufferInfo.commandBufferCount = count;
        if((reason = vkAllocateCommandBuffers(device, &bufferInfo, buffers))!=VK_SUCCESS){
            LOGWITH("Failed to allocate command buffers:", reason,resultAsString(reason));
            buffers[0] = VK_NULL_HANDLE;
        }
    }

    void VkMachine::resetWindow(int32_t key, bool recreateSurface) {
        auto it = singleton->windowSystems.find(key);
        if (it == singleton->windowSystems.end()) { return; }

        it->second->recreateSwapchain(recreateSurface);
        uint32_t width = it->second->swapchain.extent.width;
        uint32_t height = it->second->swapchain.extent.height;
        if (width && height) { // 값이 0이면 크기 0이라 스왑체인 재생성 실패한 것
            for (auto& fpass : singleton->finalPasses) {
                if (fpass.second->windowIdx == key) {
                    if (!fpass.second->reconstructFB(width, height)) {
                        LOGWITH("RenderPass", fpass.first, ": Failed to be recreate framebuffer");
                    }
                }
            }
        }
    }

    void VkMachine::__reaper::reap() {
        if (!empty) {
            vkDeviceWaitIdle(singleton->device);
            for (auto& dset : descriptorsets) { vkFreeDescriptorSets(singleton->device, dset.second, 1, &dset.first); } // 동일 풀에 대한 것은 한 번에 해제하도록 최적화 가능
            for (VkImageView v : views) { vkDestroyImageView(singleton->device, v, nullptr); }
            for (auto& img : images) { vmaDestroyImage(singleton->allocator, img.first, img.second); }
            for (auto& buf : buffers) { vmaDestroyBuffer(singleton->allocator, buf.first, buf.second); }

            descriptorsets.clear();
            views.clear();
            images.clear();
            buffers.clear();

            empty = true;
        }
    }

    void VkMachine::reap() {
        /*
        * 리핑 방법 후보
        * 1. 자원을 바인드한 렌더패스가 항상 참조수를 보유
        * 1.1. 연관 컨테이너로 중복을 방지할 경우 룩업비용과 메모리, 대부분의 상황에서 의미없는 shared ptr 소멸자 비용
        * 1.2. 순차 컨테이너로 보유만 할 경우 더 많은 메모리 사용, 대부분의 상황에서 의미없는 shared ptr 소멸자 비용
        * 2. 자원을 보유한 클래스가 렌더패스를 참조: 소멸 시에만, 패스마다의 대기가 발생하나
        * 이를 무결하게 구현하려면 메모리 사용량은 더 많아질 수밖에 없음.
        * 자원은 최근에 들어간 렌더패스가 사라졌는지도 알아야 하는데 역시 무결하게 구현하려면 양방향 참조가 들어갈 수 있음
        * 3. 렌더패스 관계없이 디바이스 대기: device wait이 필요하나 구분 비용이 없음. vsync를 제외하면 단일 렌더패스 대기 비용이나 디바이스 대기나 비슷할 때도 있음
        */
        singleton->reaper.reap();
    }

    void VkMachine::WindowSystem::checkSurfaceHandle(){
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(singleton->physicalDevice.card, surface.handle, &surface.caps);
        uint32_t count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(singleton->physicalDevice.card, surface.handle, &count, nullptr);
        if(count == 0) LOGWITH("Fatal: no available surface format?");
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(singleton->physicalDevice.card, surface.handle, &count, formats.data());
        surface.format = formats[0];
        for(VkSurfaceFormatKHR& form:formats){
            if(form.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR && form.format == VK_FORMAT_B8G8R8A8_SRGB){
                surface.format = form;
            }
        }
    }

    VkMachine::WindowSystem::WindowSystem(Window* window) {
        VkResult result = window->createWindowSurface(singleton->instance, &surface.handle);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to create window surface:", result, resultAsString(result));
            return;
        }
        VkBool32 supported;
        result = vkGetPhysicalDeviceSurfaceSupportKHR(singleton->physicalDevice.card, singleton->physicalDevice.pq, surface.handle, &supported);
        if (result != VK_SUCCESS || supported == 0) {
            LOGWITH("Window surface does not seem to compatible with the best adapter");
            return;
        }
        this->window = window;
        checkSurfaceHandle();
        recreateSwapchain();
    }

    void VkMachine::WindowSystem::recreateSwapchain(bool resetSurface) {
        if (swapchain.handle) {
            destroySwapchain();
        }
        needReset = false;
        int width, height;
        window->getFramebufferSize(&width, &height);
        swapchain.extent.width = width;
        swapchain.extent.height = height;
        if (width == 0 || height == 0) {
            return;
        }
        if (resetSurface) {
            vkDestroySurfaceKHR(singleton->instance, surface.handle, nullptr);
            window->createWindowSurface(singleton->instance, &surface.handle);
            checkSurfaceHandle();
        }
        checkSurfaceHandle();
        VkSwapchainCreateInfoKHR scInfo{};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.surface = surface.handle;
        scInfo.minImageCount = std::min(3u, surface.caps.maxImageCount == 0 ? 3u : surface.caps.maxImageCount);
        scInfo.imageFormat = surface.format.format;
        scInfo.imageColorSpace = surface.format.colorSpace;
        scInfo.presentMode = singleton->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        scInfo.imageArrayLayers = 1;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // pretransform 대신 항상 surface 재생성으로 처리
        scInfo.imageExtent.width = std::clamp(swapchain.extent.width, surface.caps.minImageExtent.width, surface.caps.maxImageExtent.width);
        scInfo.imageExtent.height = std::clamp(swapchain.extent.height, surface.caps.minImageExtent.height, surface.caps.maxImageExtent.height);
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.clipped = VK_TRUE;
        scInfo.oldSwapchain = VK_NULL_HANDLE; // 같은 표면에 대한 핸들은 올드로 사용할 수 없음
        uint32_t qfi[2] = { singleton->physicalDevice.gq, singleton->physicalDevice.pq };
        if (singleton->physicalDevice.gq == singleton->physicalDevice.pq) {
            scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else {
            scInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            scInfo.queueFamilyIndexCount = 2;
            scInfo.pQueueFamilyIndices = qfi;
        }
        if ((reason = vkCreateSwapchainKHR(singleton->device, &scInfo, nullptr, &swapchain.handle)) != VK_SUCCESS) {
            LOGWITH("Failed to create swapchain:", reason, resultAsString(reason));
            return;
        }
        uint32_t count;
        vkGetSwapchainImagesKHR(singleton->device, swapchain.handle, &count, nullptr);
        std::vector<VkImage> images(count);
        swapchain.imageView.resize(count);
        vkGetSwapchainImagesKHR(singleton->device, swapchain.handle, &count, images.data());
        for (size_t i = 0; i < count; i++) {
            swapchain.imageView[i] = createImageView(singleton->device, images[i], VK_IMAGE_VIEW_TYPE_2D, surface.format.format, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT);
            if (swapchain.imageView[i] == 0) {
                return;
            }
        }
    }

    void VkMachine::free() {
        vkDeviceWaitIdle(device);
        for(VkSampler& sampler: textureSampler) { vkDestroySampler(device, sampler, nullptr); sampler = VK_NULL_HANDLE; }
        vkDestroySampler(device, nearestSampler, nullptr); nearestSampler = VK_NULL_HANDLE;
        for(auto& ly: descriptorSetLayouts) { vkDestroyDescriptorSetLayout(device, ly.second, nullptr); }
        for(auto& cp: cubePasses) { delete cp.second; }
        for(auto& fp: finalPasses) { delete fp.second; }
        for(auto& rp: renderPasses) { delete rp.second; }
        for(auto& rt: renderTargets){ delete rt.second; }
        for(auto& sh: shaders) { vkDestroyShaderModule(device, sh.second, nullptr); }
        for(auto& pp: pipelines) { vkDestroyPipeline(device, pp.second->pipeline, nullptr); }
        for(auto& pp: pipelineLayouts) { vkDestroyPipelineLayout(device, pp.second, nullptr); }

        descriptorSetLayouts.clear();
        streamTextures.clear();
        textures.clear();
        meshes.clear();
        pipelines.clear();
        pipelineLayouts.clear();
        textureSets.clear();
        cubePasses.clear();
        finalPasses.clear();
        renderPasses.clear();
        renderTargets.clear();
        shaders.clear();

        reap();
        
        for (auto& wsi : windowSystems) {
            delete wsi.second;
        }
        windowSystems.clear();

        vmaDestroyAllocator(allocator);
        vkDestroyCommandPool(device, gCommandPool, nullptr);
        vkDestroyCommandPool(device, tCommandPool, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroyInstance(instance, nullptr);
        allocator = VK_NULL_HANDLE;
        gCommandPool = VK_NULL_HANDLE;
        tCommandPool = VK_NULL_HANDLE;
        descriptorPool = VK_NULL_HANDLE;
        device = VK_NULL_HANDLE;
        graphicsQueue = VK_NULL_HANDLE;
        presentQueue = VK_NULL_HANDLE;
        transferQueue = VK_NULL_HANDLE;
        instance = VK_NULL_HANDLE;
    }

    void VkMachine::WindowSystem::destroySwapchain() {
        for (VkImageView v : swapchain.imageView) { vkDestroyImageView(singleton->device, v, nullptr); }
        vkDestroySwapchainKHR(singleton->device, swapchain.handle, nullptr);
        swapchain.imageView.clear();
        swapchain.handle = VK_NULL_HANDLE;
    }

    VkMachine::WindowSystem::~WindowSystem() {
        destroySwapchain();
        vkDestroySurfaceKHR(singleton->instance, surface.handle, nullptr);
        std::memset(&surface, 0, sizeof(surface));
    }

    void VkMachine::handle() {
        singleton->loadThread.handleCompleted();
    }

    void VkMachine::post(std::function<variant8(void)> exec, std::function<void(variant8)> handler, uint8_t strand) {
        singleton->loadThread.post(exec, handler, strand);
    }

    void VkMachine::allocateDescriptorSets(VkDescriptorSetLayout* layouts, uint32_t count, VkDescriptorSet* output){
        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.pSetLayouts = layouts;
        dsAllocInfo.descriptorSetCount = count;
        dsAllocInfo.descriptorPool = descriptorPool;

        reason = vkAllocateDescriptorSets(device, &dsAllocInfo, output);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to allocate descriptor sets:",reason,resultAsString(reason));
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
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipLodBias = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        for(int i = 0; i < sizeof(textureSampler)/sizeof(textureSampler[0]);i++, samplerInfo.maxLod++){
            reason = vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler[i]);
            if(reason != VK_SUCCESS){
                LOGWITH("Failed to create texture sampler:", reason,resultAsString(reason));
                return false;
            }
        }
        samplerInfo.maxLod = 1.0f;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        reason = vkCreateSampler(device, &samplerInfo, nullptr, &nearestSampler);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create texture sampler:", reason,resultAsString(reason));
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

    VkMachine::pMesh VkMachine::createNullMesh(int32_t name, size_t vcount) {
        pMesh m = getMesh(name);
        if(m) { return m; }
        struct publicmesh:public Mesh{publicmesh(VkBuffer _1, VmaAllocation _2, size_t _3, size_t _4,size_t _5,void* _6,bool _7):Mesh(_1,_2,_3,_4,_5,_6,_7){}};
        if(name == INT32_MIN) return std::make_shared<publicmesh>(VK_NULL_HANDLE,VK_NULL_HANDLE,vcount,0,0,nullptr,false);
        return singleton->meshes[name] = std::make_shared<publicmesh>(VK_NULL_HANDLE,VK_NULL_HANDLE,vcount,0,0,nullptr,false);
    }

    VkMachine::pMesh VkMachine::createMesh(int32_t key, const MeshCreationOptions& opts) {
        if (opts.indexCount != 0 && opts.singleIndexSize != 2 && opts.singleIndexSize != 4) {
            LOGWITH("Invalid isize");
            return pMesh();
        }
        if ((opts.indexCount != 0) != (opts.indices != nullptr)) {
            LOGWITH("Index count and opts.indices should be both non-null or both null. Perhaps this can be a mistake");
            return pMesh();
        }
        if (!opts.fixed && (opts.vertices == nullptr || opts.singleVertexSize * opts.vertexCount == 0)) {
            LOGWITH("Vertex data should be given when making fixed Mesh");
            return pMesh();
        }

        if (pMesh m = getMesh(key)) { return m; }
        VkBuffer vib, sb;
        VmaAllocation viba, sba;

        const size_t VBSIZE = opts.singleVertexSize * opts.vertexCount, IBSIZE = opts.singleIndexSize * opts.indexCount;

        VkBufferCreateInfo vbInfo{};
        vbInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vbInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        const auto& physicalDevice = singleton->physicalDevice;
        uint32_t qfi[2] = { physicalDevice.gq, physicalDevice.subq };
        if (physicalDevice.gq != physicalDevice.subq) {
            vbInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            vbInfo.pQueueFamilyIndices = qfi;
            vbInfo.queueFamilyIndexCount = 2;
        }
        vbInfo.size = VBSIZE + IBSIZE;

        VmaAllocationCreateInfo vbaInfo{};
        vbaInfo.usage = VMA_MEMORY_USAGE_AUTO;

        struct publicmesh :public Mesh { publicmesh(VkBuffer _1, VmaAllocation _2, size_t _3, size_t _4, size_t _5, void* _6, bool _7) :Mesh(_1, _2, _3, _4, _5, _6, _7) {} };

        if (opts.fixed) {
            vbInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT; // vma 목적지 버퍼 생성 시 HOST_VISIBLE이 있으면 스테이징을 할 필요가 없음, 그러면 재생성 필요 없이 그대로 리턴하도록
            vbaInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }
        else {
            vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }
        VmaAllocationInfo mapInfoV;
        reason = vmaCreateBuffer(singleton->allocator, &vbInfo, &vbaInfo, &sb, &sba, &mapInfoV);
        if (reason != VK_SUCCESS) {
            LOGWITH("Failed to create stage buffer for vertex:", reason, resultAsString(reason));
            return pMesh();
        }
        if (opts.vertices) std::memcpy(mapInfoV.pMappedData, opts.vertices, VBSIZE);
        if (opts.indices) std::memcpy((uint8_t*)mapInfoV.pMappedData + VBSIZE, opts.indices, IBSIZE);
        vmaInvalidateAllocation(singleton->allocator, sba, 0, VK_WHOLE_SIZE);
        vmaFlushAllocation(singleton->allocator, sba, 0, VK_WHOLE_SIZE);

        if (!opts.fixed) {
            if (key == INT32_MIN) return std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
            return singleton->meshes[key] = std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
        }

        vbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vbInfo.size = VBSIZE + IBSIZE;
        vbaInfo.flags = 0;
        reason = vmaCreateBuffer(singleton->allocator, &vbInfo, &vbaInfo, &vib, &viba, nullptr);
        if (reason != VK_SUCCESS) {
            LOGWITH("Failed to create vertex buffer:", reason, resultAsString(reason));
            vmaDestroyBuffer(singleton->allocator, sb, sba);
            return pMesh();
        }
        VkMemoryPropertyFlags props;
        vmaGetAllocationMemoryProperties(singleton->allocator, viba, &props);
        if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            if (key == INT32_MIN) return std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
            return singleton->meshes[key] = std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
        }

        VkCommandBuffer copycb;
        singleton->allocateCommandBuffers(1, true, false, &copycb);
        if (!copycb) {
            LOGHERE;
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            if (key == INT32_MIN) return std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
            return singleton->meshes[key] = std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
        }
        VkCommandBufferBeginInfo cbInfo{};
        cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cbInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = VBSIZE + IBSIZE;
        if ((reason = vkBeginCommandBuffer(copycb, &cbInfo)) != VK_SUCCESS) {
            LOGWITH("Failed to begin command buffer:", reason, resultAsString(reason));
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if (key == INT32_MIN) return std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
            return singleton->meshes[key] = std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
        }
        vkCmdCopyBuffer(copycb, sb, vib, 1, &copyRegion);
        if ((reason = vkEndCommandBuffer(copycb)) != VK_SUCCESS) {
            LOGWITH("Failed to end command buffer:", reason, resultAsString(reason));
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if (key == INT32_MIN) return std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
            return singleton->meshes[key] = std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
        }
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copycb;
        VkFence fence = singleton->createFence(); // TODO: 생성 시 쓰는 이런 자잘한 펜스를 매번 만들었다 없애지 말고 하나 생성해 두고 쓰는 걸로 통일
        if (!fence) {
            LOGHERE;
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if (key == INT32_MIN) return std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
            return singleton->meshes[key] = std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
        }
        if ((reason = singleton->qSubmit(false, 1, &submitInfo, fence)) != VK_SUCCESS) {
            LOGWITH("Failed to submit copy command");
            vmaDestroyBuffer(singleton->allocator, vib, viba);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
            if (key == INT32_MIN) return std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, mapInfoV.pMappedData, opts.singleIndexSize == 4);
            return singleton->meshes[key] = std::make_shared<publicmesh>(sb, sba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
        }
        vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(singleton->device, fence, nullptr);
        vmaDestroyBuffer(singleton->allocator, sb, sba);
        vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copycb);
        if (key == INT32_MIN) return std::make_shared<publicmesh>(vib, viba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
        return singleton->meshes[key] = std::make_shared<publicmesh>(vib, viba, opts.vertexCount, opts.indexCount, VBSIZE, nullptr, opts.singleIndexSize == 4);
    }

    VkMachine::RenderTarget* VkMachine::createRenderTarget2D(int width, int height, RenderTargetType type, bool useDepthInput, bool sampled, bool linear, bool canRead){
        if(!singleton->allocator) {
            LOGWITH("Warning: Tried to create image before initialization");
            return nullptr;
        }
        if (useDepthInput && (type & RenderTargetType::RTT_STENCIL)) {
            LOGWITH("Warning: Can\'t use stencil buffer while using depth buffer as sampled image or input attachment"); // TODO? 엄밀히 말하면 스텐실만 입력첨부물로 쓸 수는 있는데 이걸 꼭 해야 할지
            return nullptr;
        }
        if (!sampled) {
            canRead = false;
        }

        uint32_t qfi[] = { singleton->physicalDevice.gq, singleton->physicalDevice.subq };

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
        imgInfo.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;

        if (canRead && sampled) {
            if (singleton->physicalDevice.gq != singleton->physicalDevice.subq) { 
                imgInfo.sharingMode = VkSharingMode::VK_SHARING_MODE_CONCURRENT;
                imgInfo.queueFamilyIndexCount = 2;
                imgInfo.pQueueFamilyIndices = qfi;
            }
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.preferredFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if((int)type & 0b1){
            color1 = new ImageSet;
            imgInfo.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | (sampled ? VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT : VkImageUsageFlagBits::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
            if (canRead && sampled) imgInfo.usage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            imgInfo.format = singleton->baseSurfaceRendertargetFormat;
            reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &color1->img, &color1->alloc, nullptr);
            if(reason != VK_SUCCESS) {
                LOGWITH("Failed to create image:", reason,resultAsString(reason));
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
                reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &color2->img, &color2->alloc, nullptr);
                if(reason != VK_SUCCESS) {
                    LOGWITH("Failed to create image:", reason,resultAsString(reason));
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
                    reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &color3->img, &color3->alloc, nullptr);
                    if(reason != VK_SUCCESS) {
                        LOGWITH("Failed to create image:", reason,resultAsString(reason));
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
            if (canRead && sampled) imgInfo.usage |= VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            imgInfo.format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
            reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &ds->img, &ds->alloc, nullptr);
            if(reason != VK_SUCCESS) {
                LOGWITH("Failed to create image: ", reason,resultAsString(reason));
                if(color1) {color1->free(); delete color1;}
                if(color2) {color2->free(); delete color2;}
                if(color3) {color3->free(); delete color3;}
                delete ds;
                return nullptr;
            }
            VkImageAspectFlags dsFlags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT;
            if (type & RenderTargetType::RTT_STENCIL) dsFlags |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;
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
        VkDescriptorSetLayout layout = sampled ? 
            getDescriptorSetLayout((ShaderResourceType)((int)ShaderResourceType::TEXTURE_1 + nim - 1)) : 
            getDescriptorSetLayout((ShaderResourceType)((int)ShaderResourceType::INPUT_ATTACHMENT_1 + nim - 1))
            ;
        VkDescriptorSet dset;

        singleton->allocateDescriptorSets(&layout, 1, &dset);

        if(!dset){
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
        wr.dstArrayElement = 0;
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.descriptorCount = 1;
        wr.pImageInfo = &imageInfo;
        wr.dstSet = dset;
        if(sampled && linear) {
            imageInfo.sampler = singleton->textureSampler[0];
            wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else if (sampled) {
            imageInfo.sampler = singleton->nearestSampler;
            wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else {
            wr.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }
        nim = 0;
        if(color1){
            imageInfo.imageView = color1->view;
            wr.dstBinding = nim++;
            vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
            if(color2){
                wr.dstBinding = nim++;
                imageInfo.imageView = color2->view;
                vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
                if(color3){
                    wr.dstBinding = nim++;
                    imageInfo.imageView = color3->view;
                    vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
                }
            }
        }
        if(ds && useDepthInput){
            imageInfo.imageView = ds->view;
            wr.dstBinding = nim++;
            vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr); // 입력 첨부물 기술자를 위한 이미지 뷰에서는 DEPTH, STENCIL을 동시에 명시할 수 없음. 솔직히 깊이를 입력첨부물로는 안 쓸 것 같긴 한데 
        }
        return new RenderTarget(type, width, height, color1, color2, color3, ds, dset, sampled, useDepthInput);
    }

    void VkMachine::removeImageSet(VkMachine::ImageSet* set) {
        auto it = images.find(set);
        if(it != images.end()) {
            (*it)->free();
            delete *it;
            images.erase(it);
        }
    }

    VkShaderModule VkMachine::createShader(int32_t name, const ShaderModuleCreationOptions& opts) {
        VkShaderModule ret = getShader(name);
        if(ret) return ret;

        VkShaderModuleCreateInfo smInfo{};
        smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smInfo.codeSize = opts.size;
        smInfo.pCode = (uint32_t*)opts.source;
        reason = vkCreateShaderModule(singleton->device, &smInfo, nullptr, &ret);
        if(reason != VK_SUCCESS) {
            LOGWITH("Failed to create shader moudle:", reason,resultAsString(reason));
            return VK_NULL_HANDLE;
        }
        if(name == INT32_MIN) return ret;
        return singleton->shaders[name] = ret;
    }

    VkMachine::pTexture VkMachine::createTexture(void* ktxObj, int32_t key, const TextureCreationOptions& opts){
        ktxTexture2* texture = reinterpret_cast<ktxTexture2*>(ktxObj);
        if (texture->numLevels == 0) return pTexture();
        VkFormat availableFormat;
        ktx_error_code_e k2result;
        if(ktxTexture2_NeedsTranscoding(texture)){
            ktx_transcode_fmt_e tf;
            switch (availableFormat = textureFormatFallback(physicalDevice.card, texture->baseWidth, texture->baseHeight, opts.nChannels, opts.srgb, opts.opts, texture->isCubemap ? VkImageCreateFlagBits::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlagBits)0))
            {
            case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
            case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
                tf = KTX_TTF_ASTC_4x4_RGBA;
                break;
            case VK_FORMAT_BC7_SRGB_BLOCK:
            case VK_FORMAT_BC7_UNORM_BLOCK:
                tf = KTX_TTF_BC7_RGBA;
                break;
            case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK:
                tf = KTX_TTF_ETC2_RGBA;
                break;
            case VK_FORMAT_BC3_SRGB_BLOCK:
            case VK_FORMAT_BC3_UNORM_BLOCK:
                tf = KTX_TTF_BC3_RGBA;
                break;
            case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK:
            case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK:
                tf = KTX_TTF_ETC;
                break;
            case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
                tf = KTX_TTF_BC1_RGB;
                break;
            case VK_FORMAT_EAC_R11G11_UNORM_BLOCK:
                tf = KTX_TTF_ETC2_EAC_RG11;
                break;
            case VK_FORMAT_BC5_UNORM_BLOCK:
                tf = KTX_TTF_BC5_RG;
                break;
            case VK_FORMAT_BC4_UNORM_BLOCK:
                tf = KTX_TTF_BC4_R;
                break;
            case VK_FORMAT_EAC_R11_UNORM_BLOCK:
                tf = KTX_TTF_ETC2_EAC_R11;
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
        reason = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &newBuffer, &newAlloc, nullptr);
        if(reason != VK_SUCCESS) {
            LOGWITH("Failed to create buffer:",reason,resultAsString(reason));
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
        void* mmap;
        reason = vmaMapMemory(allocator, newAlloc, &mmap);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to map memory to buffer:",reason,resultAsString(reason));
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
        uint32_t qfi[2] = { physicalDevice.gq, physicalDevice.subq };
        if (physicalDevice.gq != physicalDevice.subq) {
            imgInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            imgInfo.queueFamilyIndexCount = 2;
            imgInfo.pQueueFamilyIndices = qfi;
        }
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.extent = {texture->baseWidth, texture->baseHeight, 1};
        imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.flags = texture->isCubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

        VkImage newImg;
        VmaAllocation newAlloc2;
        allocInfo.flags = 0;
        if ((reason = vmaCreateImage(allocator, &imgInfo, &allocInfo, &newImg, &newAlloc2, nullptr)) != VK_SUCCESS) {
            LOGWITH("Failed to create image space:", reason, resultAsString(reason));
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
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

        if((reason = vkBeginCommandBuffer(copyCmd, &beginInfo)) != VK_SUCCESS){
            LOGWITH("Failed to begin command buffer:",reason,resultAsString(reason));
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
        if((reason = vkEndCommandBuffer(copyCmd)) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",reason,resultAsString(reason));
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
        if((reason = qSubmit(false, 1, &submitInfo, fence)) != VK_SUCCESS){
            LOGWITH("Failed to submit copy command:",reason,resultAsString(reason));
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

        reason = vkCreateImageView(device, &viewInfo, nullptr, &newView);

        vkWaitForFences(device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, tCommandPool, 1, &copyCmd);
        vmaDestroyBuffer(allocator, newBuffer, newAlloc);

        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create image view:",reason,resultAsString(reason));
            vmaDestroyImage(allocator, newImg, newAlloc2);
            return pTexture();
        }

        VkDescriptorSet newSet;
        auto layout = getDescriptorSetLayout(ShaderResourceType::TEXTURE_1);
        singleton->allocateDescriptorSets(&layout, 1, &newSet);
        if(!newSet){
            LOGHERE;
            vkDestroyImageView(device, newView, nullptr);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            return pTexture();
        }

        VkDescriptorImageInfo dsImageInfo{};
        dsImageInfo.imageView = newView;
        dsImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if (opts.linearSampled) {
            dsImageInfo.sampler = textureSampler[imgInfo.mipLevels - 1];
        }
        else {
            dsImageInfo.sampler = nearestSampler;
        }

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = newSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &dsImageInfo;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        
        struct txtr:public Texture{ inline txtr(VkImage _1, VkImageView _2, VmaAllocation _3, VkDescriptorSet _4, uint16_t _5, uint16_t _6):Texture(_1,_2,_3,_4,_5,_6){} };
        pTexture ret = std::make_shared<txtr>(newImg, newView, newAlloc2, newSet, imgInfo.extent.width, imgInfo.extent.height);
        ret->linearSampled = opts.linearSampled;
        if (key == INT32_MIN) return ret;
        std::unique_lock<std::mutex> _(textureGuard);
        return textures[key] = std::move(ret);
    }

    VkMachine::pStreamTexture VkMachine::createStreamTexture(int32_t key, uint32_t width, uint32_t height, bool linearSampler) {
        if (pStreamTexture ret = getStreamTexture(key)) return ret;
        if ((width | height) == 0) return {};
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.extent = { width, height, 1 };
        imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.flags = 0;
        const auto& physicalDevice = singleton->physicalDevice;
        uint32_t qfi[2] = { physicalDevice.gq, physicalDevice.subq };
        if (physicalDevice.gq == physicalDevice.subq) {
            imgInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            imgInfo.pQueueFamilyIndices = qfi;
            imgInfo.queueFamilyIndexCount = 2;
        }
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        //allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        //allocInfo.requiredFlags = VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        VkImage img;
        VmaAllocation alloc;
        VkResult result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &img, &alloc, nullptr);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to create vkimage", resultAsString(result));
            LOGWITH(width, height, key);
            return nullptr;
        }

        VkCommandBuffer copyCmd;
        singleton->allocateCommandBuffers(1, true, false, &copyCmd);

        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.image = img;
        imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgBarrier.subresourceRange.baseMipLevel = 0;
        imgBarrier.subresourceRange.levelCount = 1;
        imgBarrier.subresourceRange.layerCount = 1;
        imgBarrier.srcAccessMask = 0;
        imgBarrier.dstAccessMask = 0;// VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if ((reason = vkBeginCommandBuffer(copyCmd, &beginInfo)) != VK_SUCCESS) {
            LOGWITH("Failed to begin command buffer:", reason, resultAsString(reason));
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copyCmd);
            vmaDestroyImage(singleton->allocator, img, alloc);
            return nullptr;
        }
        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
        if ((reason = vkEndCommandBuffer(copyCmd)) != VK_SUCCESS) {
            LOGWITH("Failed to end command buffer:", reason, resultAsString(reason));
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copyCmd);
            vmaDestroyImage(singleton->allocator, img, alloc);
            return nullptr;
        }
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCmd;
        VkFence fence = singleton->createFence();
        if (fence == VK_NULL_HANDLE) {
            LOGHERE;
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copyCmd);
            vmaDestroyImage(singleton->allocator, img, alloc);
            return nullptr;
        }
        if ((reason = singleton->qSubmit(false, 1, &submitInfo, fence)) != VK_SUCCESS) {
            LOGWITH("Failed to submit copy command:", reason, resultAsString(reason));
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copyCmd);
            vmaDestroyImage(singleton->allocator, img, alloc);
            vkDestroyFence(singleton->device, fence, nullptr);
            return nullptr;
        }
        VkImageView newView;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = img;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
        viewInfo.subresourceRange = imgBarrier.subresourceRange;

        reason = vkCreateImageView(singleton->device, &viewInfo, nullptr, &newView);

        vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(singleton->device, fence, nullptr);
        vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &copyCmd);

        if (reason != VK_SUCCESS) {
            LOGWITH("Failed to create image view:", reason, resultAsString(reason));
            vmaDestroyImage(singleton->allocator, img, alloc);
            return nullptr;
        }

        auto layout = getDescriptorSetLayout(ShaderResourceType::TEXTURE_1);
        VkDescriptorSet newSet;
        singleton->allocateDescriptorSets(&layout, 1, &newSet);
        if (!newSet) {
            LOGHERE;
            vkDestroyImageView(singleton->device, newView, nullptr);
            vmaDestroyImage(singleton->allocator, img, alloc);
            return nullptr;
        }

        VkDescriptorImageInfo dsImageInfo{};
        dsImageInfo.imageView = newView;
        dsImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        if (linearSampler) {
            dsImageInfo.sampler = singleton->textureSampler[0];
        }
        else {
            dsImageInfo.sampler = singleton->nearestSampler;
        }

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = newSet;
        descriptorWrite.dstBinding = 0; // TODO: 위의 것과 함께 선택권
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &dsImageInfo;
        vkUpdateDescriptorSets(singleton->device, 1, &descriptorWrite, 0, nullptr);

        struct txtr :public StreamTexture { inline txtr(VkImage _1, VkImageView _2, VmaAllocation _3, VkDescriptorSet _4, uint32_t _5, uint16_t _6, uint16_t _7) :StreamTexture(_1, _2, _3, _4, _5, _6, _7) {} };
        if (key == INT32_MIN) return std::make_shared<txtr>(img, newView, alloc, newSet, 0, imgInfo.extent.width, imgInfo.extent.height);
        std::unique_lock<std::mutex> _(singleton->textureGuard);
        return singleton->streamTextures[key] = std::make_shared<txtr>(img, newView, alloc, newSet, 0, imgInfo.extent.width, imgInfo.extent.height);
    }

    VkMachine::StreamTexture::StreamTexture(VkImage img, VkImageView view, VmaAllocation alloc, VkDescriptorSet dset, uint32_t binding, uint16_t width, uint16_t height) :img(img), view(view), alloc(alloc), dset(dset), binding(binding), width(width), height(height) {
        VmaAllocationCreateInfo ainfo{};
        ainfo.usage = VMA_MEMORY_USAGE_AUTO;
        ainfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufInfo.size = (VkDeviceSize)width * height * 4;
        bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        vmaCreateBuffer(singleton->allocator, &bufInfo, &ainfo, &buf, &allocb, nullptr);
        vmaMapMemory(singleton->allocator, allocb, &mmap);
        fence = singleton->createFence(true);
        singleton->allocateCommandBuffers(1, true, false, &cb);
    }

    VkMachine::StreamTexture::~StreamTexture() {
        vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(singleton->device, fence, nullptr);
        vmaUnmapMemory(singleton->allocator, allocb);
        vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &cb);
        singleton->reaper.push(dset, singleton->descriptorPool);
        singleton->reaper.push(view);
        singleton->reaper.push(img, alloc);
        singleton->reaper.push(buf, allocb);
    }

    void VkMachine::StreamTexture::update(void* src) {
        memcpy(mmap, src, (size_t)width * height * 4);
        vmaInvalidateAllocation(singleton->allocator, alloc, 0, VK_WHOLE_SIZE);
        vmaFlushAllocation(singleton->allocator, alloc, 0, VK_WHOLE_SIZE);
        VkCommandBuffer cb;
        VkMachine::singleton->allocateCommandBuffers(1, true, false, &cb);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = width;
        region.imageExtent.height = height;
        region.imageExtent.depth = 1;
        region.bufferOffset = 0;
        region.bufferImageHeight = 0;

        vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkResetFences(singleton->device, 1, &fence);
        vkResetCommandBuffer(cb, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cb, &beginInfo);
        vkCmdCopyBufferToImage(cb, buf, img, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        vkEndCommandBuffer(cb);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cb;
        VkMachine::singleton->qSubmit(false, 1, &submitInfo, fence);
    }

    VkMachine::TextureSet::~TextureSet() {
        singleton->reaper.push(dset, singleton->descriptorPool);
    }

    static ktxTexture2* createKTX2FromImage(const uint8_t* pix, int x, int y, int nChannels, bool srgb, VkMachine::TextureFormatOptions option){
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
        if(option == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS){
            ktxBasisParams params{};
            params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
            params.uastc = KTX_TRUE;
            params.verbose = KTX_FALSE;
            params.structSize = sizeof(params);

            k2result = ktxTexture2_CompressBasisEx(texture, &params);
            if(k2result != KTX_SUCCESS){
                LOGWITH("Compress failed:",k2result);
                ktxTexture_Destroy(ktxTexture(texture));
                return nullptr;
            }
        }
        return texture;
    }

    VkMachine::pTexture VkMachine::createTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, const TextureCreationOptions& opts) {
        if (auto tex = getTexture(key)) { return tex; }
        ktxTexture2* texture = createKTX2FromImage(color, width, height, opts.nChannels, opts.srgb, opts.opts);
        if (!texture) {
            LOGHERE;
            return {};
        }
        return singleton->createTexture(texture, key, opts);
    }

    VkMachine::pTexture VkMachine::createTextureFromImage(int32_t key, const char* fileName, const TextureCreationOptions& opts) {
        if (auto tex = getTexture(key)) { return tex; }
        int x, y, nChannels;
        uint8_t* pix = stbi_load(fileName, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return {};
        }
        TextureCreationOptions channelOpts = opts;
        channelOpts.nChannels = nChannels;
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, opts.srgb, opts.opts);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return {};
        }
        return singleton->createTexture(texture, key, channelOpts);
    }

    VkMachine::pTexture VkMachine::createTextureFromImage(int32_t key, const void* mem, size_t size, const TextureCreationOptions& opts) {
        if (auto tex = getTexture(key)) { return tex; }
        int x, y, nChannels;
        uint8_t* pix = stbi_load_from_memory((uint8_t*)mem, (int)size, &x, &y, &nChannels, 4);
        if (!pix) {
            LOGWITH("Failed to load image:", stbi_failure_reason());
            return pTexture();
        }
        TextureCreationOptions channelOpts = opts;
        channelOpts.nChannels = nChannels;
        ktxTexture2* texture = createKTX2FromImage(pix, x, y, nChannels, opts.srgb, opts.opts);
        stbi_image_free(pix);
        if (!texture) {
            LOGHERE;
            return pTexture();
        }
        return singleton->createTexture(texture, key, opts);
    }

    VkMachine::pTexture VkMachine::createTexture(int32_t key, const char* fileName, const TextureCreationOptions& opts) {
        pTexture ret(std::move(getTexture(key)));
        if(ret) return ret;

        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if((k2result= ktxTexture2_CreateFromNamedFile(fileName, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS){
            LOGWITH("Failed to load ktx texture:",k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, opts);
    }

    VkMachine::pTexture VkMachine::createTexture(int32_t key, const uint8_t* mem, size_t size, const TextureCreationOptions& opts){
        if (auto ret = getTexture(key)) { return ret; }
        ktxTexture2* texture;
        ktx_error_code_e k2result;

        if((k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS){
            LOGWITH("Failed to load ktx texture:",k2result);
            return pTexture();
        }
        return singleton->createTexture(texture, key, opts);
    }

    void VkMachine::asyncCreateTexture(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts){
        if(key == INT32_MIN) {
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
        singleton->loadThread.post([fileName, key, options](){
            pTexture ret = singleton->createTexture(INT32_MIN, fileName, options);
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

    void VkMachine::aysncCreateTextureFromColor(int32_t key, const uint8_t* color, uint32_t width, uint32_t height, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
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

    void VkMachine::asyncCreateTextureFromImage(int32_t key, const char* fileName, std::function<void(variant8)> handler, const TextureCreationOptions& opts){
        if(key == INT32_MIN) {
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
        singleton->loadThread.post([ fileName, key, options](){
            pTexture ret = singleton->createTextureFromImage(INT32_MIN, fileName, options);
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

    void VkMachine::asyncCreateTextureFromImage(int32_t key, const void* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts){
        if(key == INT32_MIN) {
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
        singleton->loadThread.post([mem, size, key, options](){
            pTexture ret = singleton->createTextureFromImage(INT32_MIN, mem, size, options);
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

    VkMachine::pTextureSet VkMachine::createTextureSet(int32_t key, const pTexture& binding0, const pTexture& binding1, const pTexture& binding2, const pTexture& binding3) {
        if (!binding0 || !binding1) {
            LOGWITH("At least 2 textures must be given");
            return {};
        }
        int length = binding2 ? (binding3 ? 4 : 3) : 2;
        VkDescriptorSetLayout layout;
        switch (length)
        {
        case 4:
            layout = getDescriptorSetLayout(ShaderResourceType::TEXTURE_4);
            break;
        case 3:
            layout = getDescriptorSetLayout(ShaderResourceType::TEXTURE_3);
            break;
        case 2:
            layout = getDescriptorSetLayout(ShaderResourceType::TEXTURE_2);
            break;
        default:
            break;
        }
        VkDescriptorSet dset{};
        singleton->allocateDescriptorSets(&layout, 1, &dset);
        if (!dset) {
            LOGHERE;
            return {};
        }

        VkWriteDescriptorSet wr[4]{};
        VkDescriptorImageInfo imageInfo[4]{};
        pTexture textures[4] = { binding0, binding1, binding2, binding3 };

        for (int i = 0; i < length; i++) {
            wr[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            wr[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[i].descriptorCount = 1;
            wr[i].dstArrayElement = 0;
            wr[i].dstBinding = i;
            wr[i].pImageInfo = &imageInfo[i];
            wr[i].dstSet = dset;

            imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo[i].sampler = singleton->textureSampler[0];
            imageInfo[i].imageView = textures[i]->view;
        }

        vkUpdateDescriptorSets(singleton->device, length, wr, 0, nullptr);
        struct __tset:public TextureSet {};
        pTextureSet ret = std::make_shared<__tset>();
        ret->dset = dset;
        ret->textureCount = length;
        ret->textures[0] = textures[0];
        ret->textures[1] = textures[1];
        ret->textures[2] = textures[2];
        ret->textures[3] = textures[3];
        if (key == INT32_MIN) return ret;
        return singleton->textureSets[key] = std::move(ret);
    }

    void VkMachine::asyncCreateTexture(int32_t key, const uint8_t* mem, size_t size, std::function<void(variant8)> handler, const TextureCreationOptions& opts) {
        if(key == INT32_MIN) {
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
        singleton->loadThread.post([mem, size, key, options](){
            pTexture ret = singleton->createTexture(INT32_MIN, mem, size, options);
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

    VkMachine::Texture::Texture(VkImage img, VkImageView view, VmaAllocation alloc, VkDescriptorSet dset, uint16_t width, uint16_t height) :img(img), view(view), alloc(alloc), dset(dset), width(width), height(height) { }
    VkMachine::Texture::~Texture(){
        singleton->reaper.push(dset, singleton->descriptorPool);
        singleton->reaper.push(img, alloc);
        singleton->reaper.push(view);
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

    void VkMachine::StreamTexture::drop(int32_t key) {
        VkMachine::singleton->streamTextures.erase(key);
    }

    VkMachine::RenderTarget::RenderTarget(RenderTargetType type, unsigned width, unsigned height, VkMachine::ImageSet* color1, VkMachine::ImageSet* color2, VkMachine::ImageSet* color3, VkMachine::ImageSet* depthstencil, VkDescriptorSet dset, bool sampled, bool depthInput)
        :type(type), width(width), height(height), color1(color1), color2(color2), color3(color3), depthstencil(depthstencil), sampled(sampled), depthInput(depthInput), dset(dset) {
    }

    VkMachine::UniformBuffer* VkMachine::createUniformBuffer(int32_t name, const UniformBufferCreationOptions& opts){
        UniformBuffer* ret = getUniformBuffer(name);
        if(ret) return ret;

        uint32_t individual;
        VkDescriptorSetLayout layout = getDescriptorSetLayout(opts.count == 1 ? ShaderResourceType::UNIFORM_BUFFER_1 : ShaderResourceType::DYNAMIC_UNIFORM_BUFFER_1);;
        if (!layout) {
            LOGHERE;
            return {};
        }
        VkDescriptorSet dset;
        VkBuffer buffer;
        VmaAllocation alloc;
        void* mmap;

        if(opts.count > 1){
            individual = (opts.size + (uint32_t)singleton->physicalDevice.minUBOffsetAlignment - 1);
            individual -= individual % singleton->physicalDevice.minUBOffsetAlignment;
        }
        else{
            individual = opts.size;
        }

        singleton->allocateDescriptorSets(&layout, 1, &dset);
        if(!dset){
            LOGHERE;
            return nullptr;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.size = individual * opts.count;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo bainfo{};
        bainfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        bainfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if(opts.count > 1){
            reason = vmaCreateBufferWithAlignment(singleton->allocator, &bufferInfo, &bainfo, singleton->physicalDevice.minUBOffsetAlignment, &buffer, &alloc, nullptr);
        }
        else{
            reason = vmaCreateBuffer(singleton->allocator, &bufferInfo, &bainfo, &buffer, &alloc, nullptr);
        }
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create buffer:", reason,resultAsString(reason));
            return nullptr;
        }

        if((reason = vmaMapMemory(singleton->allocator, alloc, &mmap)) != VK_SUCCESS){
            LOGWITH("Failed to map memory:", reason,resultAsString(reason));
            return nullptr;
        }

        VkDescriptorBufferInfo dsNBuffer{};
        dsNBuffer.buffer = buffer;
        dsNBuffer.offset = 0;
        dsNBuffer.range = individual;
        VkWriteDescriptorSet wr{};
        wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wr.descriptorType = opts.count == 1 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        wr.descriptorCount = 1;
        wr.dstArrayElement = 0;
        wr.dstBinding = 0;
        wr.pBufferInfo = &dsNBuffer;
        wr.dstSet = dset;
        vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
        return singleton->uniformBuffers[name] = new UniformBuffer(opts.count, individual, buffer, layout, dset, alloc, mmap);
    }

    uint32_t VkMachine::RenderTarget::attachmentRefs(VkAttachmentDescription* arr, bool forSample, bool autoclear){
        uint32_t colorCount = 0;
        VkAttachmentLoadOp loadOp = autoclear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
        if(color1) {
            arr[0].format = singleton->baseSurfaceRendertargetFormat;
            arr[0].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            arr[0].loadOp = loadOp;
            arr[0].storeOp = sampled ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
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
            arr[colorCount].loadOp = loadOp;
            arr[colorCount].storeOp = sampled ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE; // 그림자맵에서야 필요하고 그 외에는 필요없음
            arr[colorCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            arr[colorCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            arr[colorCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            arr[colorCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        return colorCount;
    }

    VkMachine::RenderTarget::~RenderTarget(){
        if(color1) { singleton->removeImageSet(color1); }
        if(color2) { singleton->removeImageSet(color2); }
        if(color3) { singleton->removeImageSet(color3); }
        if(depthstencil) { singleton->removeImageSet(depthstencil); }
        vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset);
    }

    VkMachine::RenderPass2Cube* VkMachine::createRenderPass2Cube(int32_t key, uint32_t width, uint32_t height, bool useColor, bool useDepth) {
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
            imgInfo.format = singleton->baseSurfaceRendertargetFormat;
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
            viewInfo.format = singleton->baseSurfaceRendertargetFormat;
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
        viewInfo.format = useColor ? singleton->baseSurfaceRendertargetFormat : VK_FORMAT_D32_SFLOAT;
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
        attachs[0].format = singleton->baseSurfaceRendertargetFormat;

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
        auto layout = getDescriptorSetLayout(ShaderResourceType::TEXTURE_1);
        singleton->allocateDescriptorSets(&layout, 1, &dset);

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
        writer.dstBinding = 0;
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

    VkMachine::RenderPass2Screen* VkMachine::createRenderPass2Screen(int32_t name, int32_t windowIdx, const RenderPassCreationOptions& opts) {
        auto it = singleton->windowSystems.find(windowIdx);
        if (it == singleton->windowSystems.end()) {
            LOGWITH("Invalid window number");
            return nullptr;
        }
        WindowSystem* window = it->second;
        RenderPass2Screen* r = getRenderPass2Screen(name);
        if (r) return r;
        if (opts.subpassCount == 0) return nullptr;
        std::vector<RenderTarget*> targets(opts.subpassCount - 1);
        for (uint32_t i = 0; i < opts.subpassCount - 1; i++) {
            targets[i] = createRenderTarget2D(window->swapchain.extent.width, window->swapchain.extent.height, opts.targets[i], opts.depthInput ? opts.depthInput[i] : false, false, false, opts.canCopy);
            if (!targets[i]) {
                LOGHERE;
                for (RenderTarget* t : targets) delete t;
                return nullptr;
            }
        }

        VkImage dsImage = VK_NULL_HANDLE;
        VmaAllocation dsAlloc = VK_NULL_HANDLE;
        VkImageView dsImageView = VK_NULL_HANDLE;

        if (opts.subpassCount == 1 && (opts.screenDepthStencil & (RenderTargetType::RTT_DEPTH | RenderTargetType::RTT_STENCIL))) {
            VkImageCreateInfo imgInfo{};
            imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.arrayLayers = 1;
            imgInfo.extent.depth = 1;
            imgInfo.extent.width = window->swapchain.extent.width;
            imgInfo.extent.height = window->swapchain.extent.height;
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

            if ((reason = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &dsImage, &dsAlloc, nullptr)) != VK_SUCCESS) {
                LOGWITH("Failed to create depth/stencil image for last one");
                for (RenderTarget* t : targets) delete t;
                return nullptr;
            }

            dsImageView = createImageView(singleton->device, dsImage, VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
            if (!dsImageView) {
                LOGHERE;
                vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
                for (RenderTarget* t : targets) delete t;
                return nullptr;
            }
        }

        std::vector<VkSubpassDescription> subpasses(opts.subpassCount);
        std::vector<VkAttachmentDescription> attachments(opts.subpassCount * 4);
        std::vector<VkAttachmentReference> colorRefs(opts.subpassCount * 4);
        std::vector<VkAttachmentReference> inputRefs(opts.subpassCount * 4);
        std::vector<VkSubpassDependency> dependencies(opts.subpassCount);
        std::vector<VkImageView> ivs(opts.subpassCount * 4);

        uint32_t totalAttachments = 0;
        uint32_t totalInputAttachments = 0;
        uint32_t inputAttachmentCount = 0;

        for (uint32_t i = 0; i < opts.subpassCount - 1; i++) {
            uint32_t colorCount = targets[i]->attachmentRefs(&attachments[totalAttachments], false, opts.autoclear.use);
            subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount = colorCount;
            subpasses[i].pColorAttachments = &colorRefs[totalAttachments];
            subpasses[i].inputAttachmentCount = inputAttachmentCount;
            subpasses[i].pInputAttachments = &inputRefs[totalInputAttachments - inputAttachmentCount];
            if (targets[i]->depthstencil) subpasses[i].pDepthStencilAttachment = &colorRefs[totalAttachments + colorCount];
            VkImageView views[4] = {
                targets[i]->color1 ? targets[i]->color1->view : VK_NULL_HANDLE,
                targets[i]->color2 ? targets[i]->color2->view : VK_NULL_HANDLE,
                targets[i]->color3 ? targets[i]->color3->view : VK_NULL_HANDLE,
                targets[i]->depthstencil ? targets[i]->depthstencil->view : VK_NULL_HANDLE
            };
            for (uint32_t j = 0; j < colorCount; j++) {
                colorRefs[totalAttachments].attachment = totalAttachments;
                colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                inputRefs[totalInputAttachments].attachment = totalAttachments;
                inputRefs[totalInputAttachments].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                ivs[totalAttachments] = views[j];
                totalAttachments++;
                totalInputAttachments++;
            }
            if (targets[i]->depthstencil) {
                colorRefs[totalAttachments].attachment = totalAttachments;
                colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                if (targets[i]->depthInput) {
                    inputRefs[totalInputAttachments].attachment = totalAttachments;
                    inputRefs[totalInputAttachments].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    totalInputAttachments++;
                }
                ivs[totalAttachments] = views[3];
                totalAttachments++;
            }
            dependencies[i + 1].srcSubpass = i;
            dependencies[i + 1].dstSubpass = i + 1;
            dependencies[i + 1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[i + 1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[i + 1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[i + 1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[i + 1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            inputAttachmentCount = colorCount; if (targets[i]->depthInput) inputAttachmentCount++;
        }

        attachments[totalAttachments].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[totalAttachments].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[totalAttachments].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[totalAttachments].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[totalAttachments].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[totalAttachments].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachments[totalAttachments].format = window->surface.format.format;
        attachments[totalAttachments].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;

        subpasses[opts.subpassCount - 1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpasses[opts.subpassCount - 1].pInputAttachments = &inputRefs[totalInputAttachments - inputAttachmentCount];
        subpasses[opts.subpassCount - 1].inputAttachmentCount = inputAttachmentCount;
        subpasses[opts.subpassCount - 1].colorAttachmentCount = 1;
        subpasses[opts.subpassCount - 1].pColorAttachments = &colorRefs[totalAttachments];

        colorRefs[totalAttachments].attachment = totalAttachments;
        colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkImageView& swapchainImageViewPlace = ivs[totalAttachments];

        totalAttachments++;

        if (dsImage) {
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
            subpasses[opts.subpassCount - 1].pDepthStencilAttachment = &colorRefs[totalAttachments];
            ivs[totalAttachments] = dsImageView;
            totalAttachments++;
        }

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = opts.subpassCount - 1;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.subpassCount = opts.subpassCount;
        rpInfo.pSubpasses = subpasses.data();
        rpInfo.attachmentCount = totalAttachments;
        rpInfo.pAttachments = attachments.data();
        rpInfo.dependencyCount = opts.subpassCount;
        rpInfo.pDependencies = &dependencies[0];
        VkRenderPass newPass;

        if ((reason = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &newPass)) != VK_SUCCESS) {
            LOGWITH("Failed to create renderpass:", reason, resultAsString(reason));
            for (RenderTarget* t : targets) delete t;
            vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
            return nullptr;
        }

        std::vector<VkFramebuffer> fbs(window->swapchain.imageView.size());
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.attachmentCount = totalAttachments;
        fbInfo.pAttachments = ivs.data();
        fbInfo.renderPass = newPass;
        fbInfo.width = window->swapchain.extent.width;
        fbInfo.height = window->swapchain.extent.height;
        fbInfo.layers = 1;
        uint32_t i = 0;
        for (VkFramebuffer& fb : fbs) {
            swapchainImageViewPlace = window->swapchain.imageView[i++];
            if ((reason = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb)) != VK_SUCCESS) {
                LOGWITH("Failed to create framebuffer:", reason, resultAsString(reason));
                for (VkFramebuffer d : fbs) vkDestroyFramebuffer(singleton->device, d, nullptr);
                vkDestroyRenderPass(singleton->device, newPass, nullptr);
                vkDestroyImageView(singleton->device, dsImageView, nullptr);
                for (RenderTarget* t : targets) delete t;
                vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
                return nullptr;
            }
        }
        RenderPass2Screen* ret = new RenderPass2Screen(newPass, std::move(targets), std::move(fbs), dsImage, dsImageView, dsAlloc, opts.autoclear.use ? (float*)opts.autoclear.color : nullptr);
        ret->setViewport((float)window->swapchain.extent.width, (float)window->swapchain.extent.height, 0.0f, 0.0f);
        ret->setScissor(window->swapchain.extent.width, window->swapchain.extent.height, 0, 0);
        ret->width = window->swapchain.extent.width;
        ret->height = window->swapchain.extent.height;
        ret->windowIdx = windowIdx;
        if (name == INT32_MIN) return ret;
        return singleton->finalPasses[name] = ret;
        return ret;
    }

    VkMachine::RenderPass* VkMachine::createRenderPass(int32_t key, const RenderPassCreationOptions& opts){
        if (RenderPass* r = getRenderPass(key)) { return r; }
        if (opts.subpassCount == 0 || opts.subpassCount > 16) { return {}; }
        RenderTarget* targets[16]{};
        for (uint32_t i = 0; i < opts.subpassCount; i++) {
            RenderTargetType rtype = opts.targets ? opts.targets[i] : RenderTargetType::RTT_COLOR1;
            bool diType = opts.depthInput ? opts.depthInput[i] : false;
            targets[i] = createRenderTarget2D(opts.width, opts.height, rtype, diType, i == opts.subpassCount - 1, opts.linearSampled, opts.canCopy);
            if (!targets[i]) {
                LOGHERE;
                for (uint32_t j = 0; j < i; j++) {
                    delete targets[i];
                }
                return {};
            }
        }

        std::vector<VkSubpassDescription> subpasses(opts.subpassCount);
        std::vector<VkAttachmentDescription> attachments(opts.subpassCount * 4);
        std::vector<VkAttachmentReference> colorRefs(opts.subpassCount * 4);
        std::vector<VkAttachmentReference> inputRefs(opts.subpassCount * 4);
        std::vector<VkSubpassDependency> dependencies(opts.subpassCount);
        std::vector<VkImageView> ivs(opts.subpassCount * 4);

        uint32_t totalAttachments = 0;
        uint32_t totalInputAttachments = 0;
        uint32_t inputAttachmentCount = 0;

        for (uint32_t i = 0; i < opts.subpassCount; i++) {
            uint32_t colorCount = targets[i]->attachmentRefs(&attachments[totalAttachments], i == (opts.subpassCount - 1), opts.autoclear.use);
            subpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount = colorCount;
            subpasses[i].pColorAttachments = &colorRefs[totalAttachments];
            subpasses[i].inputAttachmentCount = inputAttachmentCount;
            subpasses[i].pInputAttachments = &inputRefs[totalInputAttachments - inputAttachmentCount];
            if (targets[i]->depthstencil) subpasses[i].pDepthStencilAttachment = &colorRefs[totalAttachments + colorCount];
            VkImageView views[4] = {
                targets[i]->color1 ? targets[i]->color1->view : VK_NULL_HANDLE,
                targets[i]->color2 ? targets[i]->color2->view : VK_NULL_HANDLE,
                targets[i]->color3 ? targets[i]->color3->view : VK_NULL_HANDLE,
                targets[i]->depthstencil ? targets[i]->depthstencil->view : VK_NULL_HANDLE
            };
            for (uint32_t j = 0; j < colorCount; j++) {
                colorRefs[totalAttachments].attachment = totalAttachments;
                colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                inputRefs[totalInputAttachments].attachment = totalAttachments;
                inputRefs[totalInputAttachments].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                ivs[totalAttachments] = views[j];
                totalAttachments++;
                totalInputAttachments++;
            }
            if (targets[i]->depthstencil) {
                colorRefs[totalAttachments].attachment = totalAttachments;
                colorRefs[totalAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                if (targets[i]->depthInput) {
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
            inputAttachmentCount = colorCount; if (targets[i]->depthstencil) inputAttachmentCount++;
        }

        dependencies[0].srcSubpass = opts.subpassCount - 1;
        dependencies[0].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.subpassCount = opts.subpassCount;
        rpInfo.pSubpasses = subpasses.data();
        rpInfo.attachmentCount = totalAttachments;
        rpInfo.pAttachments = attachments.data();
        rpInfo.dependencyCount = opts.subpassCount; // 스왑체인 의존성은 이 함수를 통해 만들지 않기 때문에 이대로 사용
        rpInfo.pDependencies = &dependencies[0];
        VkRenderPass newPass;
        if ((reason = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &newPass)) != VK_SUCCESS) {
            LOGWITH("Failed to create renderpass:", reason, resultAsString(reason));
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
        if ((reason = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb)) != VK_SUCCESS) {
            LOGWITH("Failed to create framebuffer:", reason, resultAsString(reason));
            return nullptr;
        }

        RenderPass* ret = new RenderPass(newPass, fb, opts.subpassCount, opts.canCopy, opts.autoclear.use ? (float*)opts.autoclear.color : (float*)nullptr);
        for (uint32_t i = 0; i < opts.subpassCount; i++) { ret->targets[i] = targets[i]; }
        ret->setViewport((float)targets[0]->width, (float)targets[0]->height, 0.0f, 0.0f);
        ret->setScissor(targets[0]->width, targets[0]->height, 0, 0);
        return singleton->renderPasses[key] = ret;
    }

    VkMachine::Pipeline* VkMachine::createPipeline(int32_t key, const PipelineCreationOptions& opts) {
        if (auto ret = getPipeline(key)) { return ret; }
        if (!(opts.vertexShader && opts.fragmentShader)) {
            LOGWITH("Vertex and fragment shader should be provided.");
            return VK_NULL_HANDLE;
        }
        if (opts.tessellationControlShader && opts.tessellationEvaluationShader) {
            if (!singleton->physicalDevice.features.tessellationShader) {
                LOGWITH("Tesselation shaders are inavailable in this device. Try to use another pipeline.");
                return VK_NULL_HANDLE;
            }
        }
        else if (opts.tessellationEvaluationShader || opts.tessellationControlShader) {
            LOGWITH("Tesselation control shader and tesselation evaluation shader must be both null or both available.");
            return VK_NULL_HANDLE;
        }

        if (opts.geometryShader && !singleton->physicalDevice.features.geometryShader) {
            LOGWITH("Geometry shaders are inavailable in this device. Try to use another pipeline.");
            return VK_NULL_HANDLE;
        }

        uint32_t OPT_COLOR_COUNT = 0;
        bool OPT_USE_DEPTHSTENCIL = false;
        VkRenderPass rp;

        if (opts.pass) {
            if (opts.subpassIndex >= opts.pass->stageCount) {
                LOGWITH("Invalid subpass index.");
                return VK_NULL_HANDLE;
            }
            OPT_COLOR_COUNT = opts.pass->targets[opts.subpassIndex]->type & 0b100 ? 3 :
                opts.pass->targets[opts.subpassIndex]->type & 0b10 ? 2 :
                opts.pass->targets[opts.subpassIndex]->type & 0b1 ? 1 :
                0;
            OPT_USE_DEPTHSTENCIL = opts.pass->targets[opts.subpassIndex]->type & 0b1000;
            rp = opts.pass->rp;
        }
        else if (opts.pass2screen) {
            if (opts.subpassIndex >= opts.pass2screen->pipelines.size()) {
                LOGWITH("Invalid subpass index.");
                return VK_NULL_HANDLE;
            }
            if (opts.subpassIndex == opts.pass2screen->targets.size()) {
                OPT_COLOR_COUNT = 1;
                OPT_USE_DEPTHSTENCIL = opts.pass2screen->dsView;
            }
            else {
                OPT_COLOR_COUNT = opts.pass2screen->targets[opts.subpassIndex]->type & 0b100 ? 3 :
                    opts.pass2screen->targets[opts.subpassIndex]->type & 0b10 ? 2 :
                    opts.pass2screen->targets[opts.subpassIndex]->type & 0b1 ? 1 :
                    0;
                OPT_USE_DEPTHSTENCIL = opts.pass2screen->targets[opts.subpassIndex]->type & 0b1000;
            }
            rp = opts.pass2screen->rp;
        }
        else {
            LOGWITH("RenderPass or RenderPass2Screen or RenderPass2Cube must be given");
            return VK_NULL_HANDLE;
        }
        VkPipelineLayout layout = createPipelineLayout(opts.shaderResources);

        VkPipelineShaderStageCreateInfo shaderStagesInfo[5] = {};
        shaderStagesInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStagesInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStagesInfo[0].module = opts.vertexShader;
        shaderStagesInfo[0].pName = "main";

        uint32_t lastStage = 1;

        if (opts.tessellationControlShader) {
            shaderStagesInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStagesInfo[1].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            shaderStagesInfo[1].module = opts.tessellationControlShader;
            shaderStagesInfo[1].pName = "main";
            shaderStagesInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStagesInfo[2].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            shaderStagesInfo[2].module = opts.tessellationEvaluationShader;
            shaderStagesInfo[2].pName = "main";
            lastStage = 3;
        }
        if (opts.geometryShader) {
            shaderStagesInfo[lastStage].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStagesInfo[lastStage].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            shaderStagesInfo[lastStage].module = opts.geometryShader;
            shaderStagesInfo[lastStage].pName = "main";
            lastStage++;
        }

        shaderStagesInfo[lastStage].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStagesInfo[lastStage].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStagesInfo[lastStage].module = opts.fragmentShader;
        shaderStagesInfo[lastStage].pName = "main";
        lastStage++;

        VkVertexInputBindingDescription vbind[2]{};
        vbind[0].binding = 0;
        vbind[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vbind[0].stride = opts.vertexSize;

        vbind[1].binding = 1;
        vbind[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        vbind[1].stride = opts.instanceDataStride;

        std::vector<VkVertexInputAttributeDescription> attrs(opts.vertexAttributeCount + opts.instanceAttributeCount);
        if (opts.vertexAttributeCount) std::copy(opts.vertexSpec, opts.vertexSpec + opts.vertexAttributeCount, attrs.data());
        if (opts.instanceAttributeCount) std::copy(opts.instanceSpec, opts.instanceSpec + opts.instanceAttributeCount, attrs.data() + opts.vertexAttributeCount);

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = (opts.vertexAttributeCount ? 1 : 0) + (opts.instanceAttributeCount ? 1 : 0);
        vertexInputInfo.pVertexBindingDescriptions = opts.vertexAttributeCount ? vbind : vbind + 1;
        vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
        vertexInputInfo.pVertexAttributeDescriptions = attrs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        VkPipelineRasterizationStateCreateInfo rtrInfo{};
        rtrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rtrInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        rtrInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rtrInfo.lineWidth = 1.0f;
        rtrInfo.polygonMode = VK_POLYGON_MODE_FILL;

        VkPipelineDepthStencilStateCreateInfo dsInfo{};
        dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsInfo.depthCompareOp = (VkCompareOp)opts.depthStencil.comparison;
        dsInfo.depthTestEnable = opts.depthStencil.depthTest;
        dsInfo.depthWriteEnable = opts.depthStencil.depthWrite;
        dsInfo.stencilTestEnable = opts.depthStencil.stencilTest;
        dsInfo.front.compareMask = opts.depthStencil.stencilFront.compareMask;
        dsInfo.front.writeMask = opts.depthStencil.stencilFront.writeMask;
        dsInfo.front.reference = opts.depthStencil.stencilFront.reference;
        dsInfo.front.compareOp = (VkCompareOp)opts.depthStencil.stencilFront.compare;
        dsInfo.front.failOp = (VkStencilOp)opts.depthStencil.stencilFront.onFail;
        dsInfo.front.depthFailOp = (VkStencilOp)opts.depthStencil.stencilFront.onDepthFail;
        dsInfo.front.passOp = (VkStencilOp)opts.depthStencil.stencilFront.onPass;

        dsInfo.back.compareMask = opts.depthStencil.stencilBack.compareMask;
        dsInfo.back.writeMask = opts.depthStencil.stencilBack.writeMask;
        dsInfo.back.reference = opts.depthStencil.stencilBack.reference;
        dsInfo.back.compareOp = (VkCompareOp)opts.depthStencil.stencilBack.compare;
        dsInfo.back.failOp = (VkStencilOp)opts.depthStencil.stencilBack.onFail;
        dsInfo.back.depthFailOp = (VkStencilOp)opts.depthStencil.stencilBack.onDepthFail;
        dsInfo.back.passOp = (VkStencilOp)opts.depthStencil.stencilBack.onPass;

        VkPipelineColorBlendAttachmentState blendStates[3]{};
        for (int i = 0; i < OPT_COLOR_COUNT; i++) {
            auto& blendInfo = blendStates[i];
            blendInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendInfo.colorBlendOp = (VkBlendOp)opts.alphaBlend[i].colorOp;
            blendInfo.alphaBlendOp = (VkBlendOp)opts.alphaBlend[i].alphaOp;
            blendInfo.blendEnable = opts.alphaBlend[i] != AlphaBlend::overwrite();
            blendInfo.srcColorBlendFactor = (VkBlendFactor)opts.alphaBlend[i].srcColorFactor;
            blendInfo.dstColorBlendFactor = (VkBlendFactor)opts.alphaBlend[i].dstColorFactor;
            blendInfo.srcAlphaBlendFactor = (VkBlendFactor)opts.alphaBlend[i].srcAlphaFactor;
            blendInfo.dstAlphaBlendFactor = (VkBlendFactor)opts.alphaBlend[i].dstAlphaFactor;
        }

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.attachmentCount = OPT_COLOR_COUNT;
        colorBlendStateCreateInfo.pAttachments = blendStates;
        colorBlendStateCreateInfo.blendConstants[0] = opts.blendConstant[0];
        colorBlendStateCreateInfo.blendConstants[1] = opts.blendConstant[1];
        colorBlendStateCreateInfo.blendConstants[2] = opts.blendConstant[2];
        colorBlendStateCreateInfo.blendConstants[3] = opts.blendConstant[3];

        VkDynamicState dynStates[2] = { VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT, VkDynamicState::VK_DYNAMIC_STATE_SCISSOR };
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
        pInfo.renderPass = rp;
        pInfo.subpass = opts.subpassIndex;
        pInfo.pDynamicState = &dynInfo;
        pInfo.layout = layout;
        pInfo.pRasterizationState = &rtrInfo;
        pInfo.pViewportState = &viewportInfo;
        pInfo.pMultisampleState = &msInfo;
        pInfo.pInputAssemblyState = &inputAssemblyInfo;
        if (opts.tessellationEvaluationShader) pInfo.pTessellationState = &tessInfo;
        if (OPT_COLOR_COUNT) { pInfo.pColorBlendState = &colorBlendStateCreateInfo; }
        if (opts.depthStencil.depthTest || opts.depthStencil.stencilTest) { pInfo.pDepthStencilState = &dsInfo; }
        VkPipeline pipeline{};
        VkResult result = vkCreateGraphicsPipelines(singleton->device, VK_NULL_HANDLE, 1, &pInfo, nullptr, &pipeline);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to create pipeline:", result, resultAsString(result));
            return {};
            VkMachine::reason = result;
        }
        Pipeline* ret = new Pipeline;
        ret->pipeline = pipeline;
        ret->pipelineLayout = layout;
        VkMachine::reason = result;
        if (opts.pass) { 
            opts.pass->usePipeline(ret, opts.subpassIndex);
        }
        else if (opts.pass2screen) {
            opts.pass2screen->usePipeline(ret, opts.subpassIndex);
        }
        return singleton->pipelines[key] = ret;
    }

    VkDescriptorSetLayout VkMachine::getDescriptorSetLayout(ShaderResourceType type) {
        auto it = singleton->descriptorSetLayouts.find(type);
        if (it != singleton->descriptorSetLayouts.end()) {
            return it->second;
        }
        VkDescriptorSetLayoutCreateInfo info{};
        VkDescriptorSetLayoutBinding bindings[4]{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pBindings = bindings;
        switch (type) {
        case onart::VkMachine::ShaderResourceType::NONE: {
            return {};
        }
        case onart::VkMachine::ShaderResourceType::UNIFORM_BUFFER_1:
        {
            info.bindingCount = 1;
            bindings[0].binding = 0;
            bindings[0].descriptorCount = 1;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            break;
        }
        case onart::VkMachine::ShaderResourceType::DYNAMIC_UNIFORM_BUFFER_1:
        {
            info.bindingCount = 1;
            bindings[0].binding = 0;
            bindings[0].descriptorCount = 1;
            bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            bindings[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            break;
        }
        case onart::VkMachine::ShaderResourceType::TEXTURE_1:
        {
            info.bindingCount = 1;
            for (int i = 0; i < 1; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            break;
        }
        case onart::VkMachine::ShaderResourceType::TEXTURE_2:
        {
            info.bindingCount = 2;
            for (int i = 0; i < 2; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            break;
        }
        case onart::VkMachine::ShaderResourceType::TEXTURE_3:
        {
            info.bindingCount = 3;
            for (int i = 0; i < 3; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            break;
        }
        case onart::VkMachine::ShaderResourceType::TEXTURE_4:
        {
            info.bindingCount = 4;
            for (int i = 0; i < 4; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            break;
        }
        case onart::VkMachine::ShaderResourceType::INPUT_ATTACHMENT_1:
        {
            info.bindingCount = 1;
            for (int i = 0; i < 1; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            }
            break;
        }
        case onart::VkMachine::ShaderResourceType::INPUT_ATTACHMENT_2:
        {
            info.bindingCount = 2;
            for (int i = 0; i < 1; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            break;
        }
        case onart::VkMachine::ShaderResourceType::INPUT_ATTACHMENT_3:
        {
            info.bindingCount = 3;
            for (int i = 0; i < 3; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            break;
        }
        case onart::VkMachine::ShaderResourceType::INPUT_ATTACHMENT_4:
        {
            info.bindingCount = 4;
            for (int i = 0; i < 4; i++) {
                bindings[i].binding = i;
                bindings[i].descriptorCount = 1;
                bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            break;
        }
        default:
        {
            LOGWITH("Invalid resource type");
            return {};
        }
        }

        VkDescriptorSetLayout layout;
        VkResult result = vkCreateDescriptorSetLayout(singleton->device, &info, nullptr, &layout);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to create descriptor set layout:", result);
            reason = result;
            return {};
        }
        return singleton->descriptorSetLayouts[type] = layout;
    }

    VkPipelineLayout VkMachine::createPipelineLayout(const PipelineLayoutOptions& opts) {
        int64_t key = ((int64_t)opts.pos0) | ((int64_t)opts.pos1 << 8) | ((int64_t)opts.pos2 << 16) | ((int64_t)opts.pos3 << 24);
        if (opts.usePush) key |= (0xffLL << 32);
        auto it = singleton->pipelineLayouts.find(key);
        if (it != singleton->pipelineLayouts.end()) { return it->second; }

        if (opts.pos0 == ShaderResourceType::NONE && !opts.usePush) {
            LOGWITH("Shader resource type must be specified sequentially. Cannot make pipeline layout with no resource type and no push constant");
            return {};
        }

        VkDescriptorSetLayout layouts[4]{
            getDescriptorSetLayout(opts.pos0),
            getDescriptorSetLayout(opts.pos1),
            getDescriptorSetLayout(opts.pos2),
            getDescriptorSetLayout(opts.pos3)
        };

        uint32_t layoutCount = 0;
        for (layoutCount = 0; layoutCount < 4 && layouts[layoutCount]; layoutCount++);

        VkPushConstantRange pushRange{};
        pushRange.offset = 0;
        pushRange.size = 128;
        pushRange.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
        
        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pSetLayouts = layouts;
        layoutInfo.setLayoutCount = layoutCount;
        layoutInfo.pPushConstantRanges = &pushRange;
        layoutInfo.pushConstantRangeCount = opts.usePush ? 1 : 0;

        VkPipelineLayout layout;
        VkResult result = vkCreatePipelineLayout(singleton->device, &layoutInfo, nullptr, &layout);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to create pipeline layout:", result, resultAsString(result));
            return {};
        }
        return singleton->pipelineLayouts[key] = layout;
    }

    VkMachine::Mesh::Mesh(VkBuffer vb, VmaAllocation vba, size_t vcount, size_t icount, size_t ioff, void *vmap, bool use32):vb(vb),vba(vba),vcount(vcount),icount(icount),ioff(ioff),vmap(vmap),idxType(use32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16){ }
    VkMachine::Mesh::~Mesh() { 
        if (vmap) { vmaUnmapMemory(singleton->allocator, vba); }
        singleton->reaper.push(vb, vba);
    }

    void VkMachine::Mesh::update(const void* input, uint32_t offset, uint32_t size){
        if(!vmap) return;
        std::memcpy((uint8_t*)vmap + offset, input, size);
        vmaInvalidateAllocation(singleton->allocator, vba, offset, size);
        vmaFlushAllocation(singleton->allocator, vba, offset, size);
    }

    void VkMachine::Mesh::updateIndex(const void* input, uint32_t offset, uint32_t size){
        if(!vmap || icount == 0) return;
        std::memcpy((uint8_t*)vmap + ioff + offset, input, size);
        vmaInvalidateAllocation(singleton->allocator, vba, ioff + offset, size);
        vmaFlushAllocation(singleton->allocator, vba, ioff + offset, size);
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

    VkMachine::RenderPass::RenderPass(VkRenderPass rp, VkFramebuffer fb, uint16_t stageCount, bool canBeRead, float* autoclear) : rp(rp), fb(fb), stageCount(stageCount), pipelines(stageCount), targets(stageCount), canBeRead(canBeRead), autoclear(false) {
        if (autoclear) {
            std::memcpy(clearColor, autoclear, sizeof(clearColor));
            this->autoclear = true;
        }
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
        for (RenderTarget* targ : targets) {
            delete targ;
        }
    }

    void VkMachine::RenderPass::usePipeline(Pipeline* pipeline, uint32_t subpass){
        if(subpass >= stageCount){
            LOGWITH("Invalid subpass. This renderpass has", stageCount, "subpasses but", subpass, "given");
            return;
        }
        pipelines[subpass] = pipeline;
        if(currentPass == subpass) { vkCmdBindPipeline(cb, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline); }
    }

    void VkMachine::RenderPass::resize(int width, int height, bool linear) {
        wait();
        RenderTarget* targets[16]{};
        for (uint32_t i = 0; i < stageCount; i++) {
            RenderTargetType rtype = this->targets[i]->type;
            bool diType = this->targets[i]->depthInput;
            targets[i] = createRenderTarget2D(width, height, rtype, diType, i == stageCount - 1, linear, canBeRead);
            if (!targets[i]) {
                LOGHERE;
                for (uint32_t j = 0; j < i; j++) {
                    delete targets[i];
                }
                return;
            }
        }
        reconstructFB(targets);
    }
    
    VkMachine::pTexture VkMachine::RenderPass::copy2Texture(int32_t key, const RenderTarget2TextureOptions& opts) {
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
        
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VkImageType::VK_IMAGE_TYPE_2D;
        if (opts.area.width && opts.area.height) {
            imgInfo.extent.width = opts.area.width;
            imgInfo.extent.height = opts.area.height;
        }
        else {
            imgInfo.extent.width = targets.back()->width;
            imgInfo.extent.height = targets.back()->height;
        }
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
        imgInfo.sharingMode = VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.tiling = VkImageTiling::VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.format = singleton->baseSurfaceRendertargetFormat;
        imgInfo.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

        VkImage img{};
        VmaAllocation alloc{};
        VkResult result = vmaCreateImage(singleton->allocator, &imgInfo, &allocInfo, &img, &alloc, nullptr);
        if (result != VK_SUCCESS) {
            singleton->reason = result;
            LOGWITH("Failed to create texture image:", result, resultAsString(result));
            return {};
        }

        VkCommandBuffer tcb{};
        singleton->allocateCommandBuffers(1, true, false, &tcb);
        if (!tcb) {
            LOGWITH("Failed to allocate transfer command buffer");
            return {};
        }

        VkCommandBufferBeginInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(tcb, &info);
        if (result != VK_SUCCESS) {
            singleton->reason = result;
            LOGWITH("Failed to begin transfer command buffer:", result, resultAsString(result));
            vmaDestroyImage(singleton->allocator, img, alloc);
            return {};
        }

        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.image = img;
        imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgBarrier.subresourceRange.baseMipLevel = 0;
        imgBarrier.subresourceRange.levelCount = 1;
        imgBarrier.subresourceRange.layerCount = 1;
        imgBarrier.srcAccessMask = 0;
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        vkCmdPipelineBarrier(tcb, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

        imgBarrier.image = srcSet->img;
        imgBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; // 이전 렌더패스 종료 이후
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 이전 렌더패스 종료 이후
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        vkCmdPipelineBarrier(tcb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

        VkImageCopy copyArea{};
        copyArea.srcSubresource.aspectMask = imgBarrier.subresourceRange.aspectMask;
        copyArea.srcSubresource.baseArrayLayer = 0;
        copyArea.srcSubresource.layerCount = 1;
        copyArea.srcSubresource.mipLevel = 0;
        copyArea.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyArea.dstSubresource.mipLevel = 0;
        copyArea.dstSubresource.baseArrayLayer = 0;
        copyArea.dstSubresource.layerCount = 1;
        copyArea.extent = imgInfo.extent;
        if (opts.area.width && opts.area.height) {
            copyArea.srcOffset.x = opts.area.x;
            copyArea.srcOffset.y = opts.area.y;
        }

        vkCmdCopyImage(tcb, srcSet->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyArea);

        std::swap(imgBarrier.srcAccessMask, imgBarrier.dstAccessMask);
        std::swap(imgBarrier.oldLayout, imgBarrier.newLayout);
        vkCmdPipelineBarrier(tcb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

        imgBarrier.image = img;
        imgBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imgBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(tcb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

        vkEndCommandBuffer(tcb);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        bool needSemaphore = !wait(0);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &tcb;
        submitInfo.waitSemaphoreCount = needSemaphore ? 1 : 0;
        submitInfo.pWaitSemaphores = &semaphore;
        submitInfo.pWaitDstStageMask = &waitStage;
        
        VkFence fence = singleton->createFence();
        result = singleton->qSubmit(false, 1, &submitInfo, fence);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to submit commands:", result, resultAsString(result));
            return {};
        }

        VkImageView newView{};

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = img;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = singleton->baseSurfaceRendertargetFormat;
        viewInfo.subresourceRange = imgBarrier.subresourceRange;

        result = vkCreateImageView(singleton->device, &viewInfo, nullptr, &newView);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to create image view:", result, resultAsString(result));
            vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &tcb);
            vmaDestroyImage(singleton->allocator, img, alloc);
            return {};
        }

        VkDescriptorSet newSet;
        auto layout = getDescriptorSetLayout(ShaderResourceType::TEXTURE_1);
        singleton->allocateDescriptorSets(&layout, 1, &newSet);
        if (!newSet) {
            LOGHERE;
            vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
            vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &tcb);
            vmaDestroyImage(singleton->allocator, img, alloc);
            return {};
        }

        VkDescriptorImageInfo dsImageInfo{};
        dsImageInfo.imageView = newView;
        dsImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        if (opts.linearSampled) {
            dsImageInfo.sampler = singleton->textureSampler[imgInfo.mipLevels - 1];
        }
        else {
            dsImageInfo.sampler = singleton->nearestSampler;
        }

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = newSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &dsImageInfo;
        vkUpdateDescriptorSets(singleton->device, 1, &descriptorWrite, 0, nullptr);

        result = vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(singleton->device, fence, nullptr);
        vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &tcb);
        struct txtr :public Texture { inline txtr(VkImage _1, VkImageView _2, VmaAllocation _3, VkDescriptorSet _4, uint16_t _5, uint16_t _6) :Texture(_1, _2, _3, _4, _5, _6) {} };
        pTexture ret = std::make_shared<txtr>(img, newView, alloc, newSet, imgInfo.extent.width, imgInfo.extent.height);
        if (key != INT32_MIN) { 
            std::unique_lock _(singleton->textureGuard);
            singleton->textures[key] = ret;
        }
        return ret;
    }

    void VkMachine::RenderPass::asyncCopy2Texture(int32_t key, std::function<void(variant8)> handler, const RenderTarget2TextureOptions& opts) {
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return;
        }
        if (key == INT32_MIN) {
            LOGWITH("Key INT32_MIN is not allowed in this async function to provide simplicity of handler. If you really want to do that, you should use thread pool manually.");
            return;
        }
        if (getTexture(key)) {
            return;
        }
        uint32_t index = opts.index;
        bool linear = opts.linearSampled;
        singleton->loadThread.post([index, linear, key, this]() {
            RenderTarget2TextureOptions opts;
            opts.index = index;
            opts.linearSampled = linear;
            pTexture tex = copy2Texture(key, opts);
            if (!tex) {
                variant8 ret;
                ret.bytedata4[0] = key;
                ret.bytedata4[1] = reason;
                return ret;
            }
            variant8 ret;
            ret.bytedata4[0] = key;
            return ret;
        }, handler, vkm_strand::GENERAL);
    }

    std::unique_ptr<uint8_t[]> VkMachine::RenderPass::readBack(uint32_t index, const TextureArea2D& area) {
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return {};
        }
        RenderTarget* targ = targets.back();
        ImageSet* srcSet{};
        if (index < 4) {
            ImageSet* sources[] = { targ->color1,targ->color2,targ->color3,targ->depthstencil };
            srcSet = sources[index];
        }
        if (!srcSet) {
            LOGWITH("Invalid index");
            return {};
        }

        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (area.width && area.height) {
            bufInfo.size = area.width * area.height * 4;
        }
        else {
            bufInfo.size = targ->width * targ->height * 4;
        }

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        VkBuffer buf{};
        VmaAllocation alloc{};
        VkResult result = vmaCreateBuffer(singleton->allocator, &bufInfo, &allocInfo, &buf, &alloc, nullptr);
        if (result != VK_SUCCESS) {
            singleton->reason = result;
            LOGWITH("Failed to create intermediate buffer:", result, resultAsString(result));
            return {};
        }

        VkCommandBuffer tcb{};
        singleton->allocateCommandBuffers(1, true, false, &tcb);
        if (!tcb) {
            LOGWITH("Failed to allocate transfer command buffer");
            return {};
        }

        VkCommandBufferBeginInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(tcb, &info);
        if (result != VK_SUCCESS) {
            singleton->reason = result;
            LOGWITH("Failed to begin transfer command buffer:", result, resultAsString(result));
            vmaDestroyBuffer(singleton->allocator, buf, alloc);
            return {};
        }

        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.image = srcSet->img;
        imgBarrier.subresourceRange.aspectMask = index == 3 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imgBarrier.subresourceRange.baseMipLevel = 0;
        imgBarrier.subresourceRange.levelCount = 1;
        imgBarrier.subresourceRange.layerCount = 1;
        imgBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; // 이전 렌더패스 종료 이후
        imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imgBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 이전 렌더패스 종료 이후
        imgBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        vkCmdPipelineBarrier(tcb, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

        VkBufferImageCopy copyArea{};
        copyArea.imageExtent.depth = 1;
        copyArea.imageSubresource.aspectMask = imgBarrier.subresourceRange.aspectMask;
        copyArea.imageSubresource.baseArrayLayer = 0;
        copyArea.imageSubresource.mipLevel = 0;
        copyArea.imageSubresource.layerCount = 1;
        if (area.width && area.height) {
            copyArea.imageOffset.x = area.x;
            copyArea.imageOffset.y = area.y;
            copyArea.imageExtent.width = area.width;
            copyArea.imageExtent.height = area.height;
        }
        else {
            copyArea.imageExtent.width = targ->width;
            copyArea.imageExtent.height = targ->height;
        }
        // buffer 관련을 0으로 비워 두면 피치 없음으로 간주. 값을 주는 경우 단위는 텍셀
        vkCmdCopyImageToBuffer(tcb, srcSet->img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &copyArea);

        std::swap(imgBarrier.srcAccessMask, imgBarrier.dstAccessMask);
        std::swap(imgBarrier.oldLayout, imgBarrier.newLayout);
        vkCmdPipelineBarrier(tcb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);

        vkEndCommandBuffer(tcb);

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        bool needSemaphore = !wait(0);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &tcb;
        submitInfo.waitSemaphoreCount = needSemaphore ? 1 : 0;
        submitInfo.pWaitSemaphores = &semaphore;
        submitInfo.pWaitDstStageMask = &waitStage;

        VkFence fence = singleton->createFence();
        result = singleton->qSubmit(false, 1, &submitInfo, fence);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to submit commands:", result, resultAsString(result));
            return {};
        }

        std::unique_ptr<uint8_t[]> ptr(new uint8_t[bufInfo.size]);
        
        result = vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, UINT64_MAX);
        vkDestroyFence(singleton->device, fence, nullptr);
        vkFreeCommandBuffers(singleton->device, singleton->tCommandPool, 1, &tcb);

        void* mapped{};
        result = vmaMapMemory(singleton->allocator, alloc, &mapped);
        if (result != VK_SUCCESS) {
            LOGWITH("Failed to map buffer memory");
            vmaDestroyBuffer(singleton->allocator, buf, alloc);
            return {};
        }
        std::memcpy(ptr.get(), mapped, bufInfo.size);
        vmaUnmapMemory(singleton->allocator, alloc);

        vmaDestroyBuffer(singleton->allocator, buf, alloc);
        return ptr;
    }

    void VkMachine::RenderPass::asyncReadBack(int32_t key, uint32_t index, std::function<void(variant8)> handler, const TextureArea2D& area) {
        if (!canBeRead) {
            LOGWITH("Can\'t copy the target. Create this render pass with canCopy flag");
            return;
        }
        TextureArea2D _area = area;
        singleton->loadThread.post([key, index, this, _area]() {
            ReadBackBuffer* ret = new ReadBackBuffer;
            ret->key = key;
            std::unique_ptr<uint8_t[]> p = readBack(index, _area);
            ret->data = p.release();
            return variant8(ret);
            }, [handler](variant8 param) {
                if (handler) handler(param);
                ReadBackBuffer* result = (ReadBackBuffer*)param.vp;
                delete result;
            }, vkm_strand::GENERAL);
    }

    void VkMachine::RenderPass::reconstructFB(VkMachine::RenderTarget** targets){
        vkDestroyFramebuffer(singleton->device, fb, nullptr);
        fb = VK_NULL_HANDLE;
        std::vector<VkImageView> ivs;
        ivs.reserve(stageCount * 4);
        for(uint32_t i = 0; i < stageCount; i++){
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
        fbInfo.attachmentCount = (uint32_t)ivs.size();
        reason = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create framebuffer:", reason,resultAsString(reason));
        }
        setViewport((float)targets[0]->width, (float)targets[0]->height, 0.0f, 0.0f);
        setScissor(targets[0]->width, targets[0]->height, 0, 0);
        for (int i = 0; i < stageCount; i++) {
            delete this->targets[i];
            this->targets[i] = targets[i];
        }
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
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &ub->dset, ub->isDynamic, &off);
    }

    void VkMachine::RenderPass::bind(uint32_t pos, const pTexture& tx) {
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass::bind(uint32_t pos, const pStreamTexture& tx) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass::bind(uint32_t pos, const pTextureSet& tx) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass::bind(uint32_t pos, RenderPass* prevPass){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        RenderTarget* target = prevPass->targets.back();
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &target->dset, 0, nullptr);
    }

    void VkMachine::RenderPass::bind(uint32_t pos, RenderPass2Cube* prevPass) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &prevPass->csamp, 0, nullptr);
    }

    void VkMachine::RenderPass::push(void* input, uint32_t start, uint32_t end){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdPushConstants(cb, pipelines[currentPass]->pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_ALL_GRAPHICS, start, end - start, input); // TODO: 스테이지 플래그를 살려야 함
    }

    void VkMachine::RenderPass::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
         if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        if((bound != mesh.get()) && (mesh->vb != VK_NULL_HANDLE)) {
            VkDeviceSize offs = 0;
            vkCmdBindVertexBuffers(cb, 0, 1, &mesh->vb, &offs);
            if(mesh->icount) vkCmdBindIndexBuffer(cb, mesh->vb, mesh->ioff, mesh->idxType);
        }
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->icount - start);
            }
            vkCmdDrawIndexed(cb, count, 1, start, 0, 0);
        }
        else {
            if((uint64_t)start + count > mesh->vcount){
                LOGWITH("Invalid call: this mesh has",mesh->vcount,"vertices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = (uint32_t)(mesh->vcount - start);
            }
            vkCmdDraw(cb, count, 1, start, 0);
        }
        bound = mesh.get();
    }

    void VkMachine::RenderPass::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count){
         if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        VkDeviceSize offs[2] = {0, 0};
        VkBuffer buffs[2] = { mesh->vb };
        if (instanceInfo->vb) { buffs[1] = instanceInfo->vb; }
        vkCmdBindVertexBuffers(cb, 0, instanceInfo ? 2 : 1, buffs, offs);
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->icount - start);
            }
            vkCmdBindIndexBuffer(cb, mesh->vb, mesh->ioff, mesh->idxType);
            vkCmdDrawIndexed(cb, count, instanceCount, start, 0, istart);
        }
        else{
            if((uint64_t)start + count > mesh->vcount){
                LOGWITH("Invalid call: this mesh has",mesh->vcount,"vertices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->vcount - start);
            }
            vkCmdDraw(cb, count, instanceCount, start, istart);
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

        if((reason = vkEndCommandBuffer(cb)) != VK_SUCCESS){
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

        currentPass = -1;
    }

    bool VkMachine::RenderPass::wait(uint64_t timeout){
        return vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, timeout) == VK_SUCCESS; // VK_TIMEOUT이나 VK_ERROR_DEVICE_LOST
    }

    void VkMachine::RenderPass::clear(RenderTargetType toClear, float* colors) {
        if (currentPass < 0) {
            LOGWITH("This renderPass is currently not running");
            return;
        }
        if (toClear == 0) {
            LOGWITH("no-op");
            return;
        }
        if ((toClear & targets[currentPass]->type) != toClear) {
            LOGWITH("Invalid target selected");
            return;
        }
        if (autoclear) {
            LOGWITH("Autoclear specified. Maybe this call is a mistake?");
        }

        VkClearRect rect{};
        rect.baseArrayLayer = 0;
        rect.layerCount = 1;
        rect.rect.extent.width = targets[0]->width;
        rect.rect.extent.height = targets[0]->height;
        VkClearAttachment clearParam[4];
        int clearCount = 0;
        if (toClear & 0b1) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            pr.colorAttachment = 0;
            std::memcpy(pr.clearValue.color.float32, colors, sizeof(pr.clearValue.color.float32));
            colors += 4;
        }
        if (toClear & 0b10) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            pr.colorAttachment = 1;
            std::memcpy(pr.clearValue.color.float32, colors, sizeof(pr.clearValue.color.float32));
            colors += 4;
        }
        if (toClear & 0b100) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            pr.colorAttachment = 2;
            std::memcpy(pr.clearValue.color.float32, colors, sizeof(pr.clearValue.color.float32));
        }
        if (toClear & 0b11000) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = 0;
            if (toClear & 0b1000) pr.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (toClear & 0b10000) pr.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            pr.clearValue.depthStencil.depth = 1.0f;
            pr.clearValue.depthStencil.stencil = 0;
        }
        vkCmdClearAttachments(cb, clearCount, clearParam, 1, &rect);
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

        if(currentPass == 0){
            wait();
            vkResetCommandBuffer(cb, 0);
            VkCommandBufferBeginInfo cbInfo{};
            cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cbInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            reason = vkBeginCommandBuffer(cb, &cbInfo);
            if(reason != VK_SUCCESS){
                LOGWITH("Failed to begin command buffer:",reason,resultAsString(reason));
                currentPass = -1;
                return;
            }
            VkRenderPassBeginInfo rpInfo{};
            std::vector<VkClearValue> clearValues;
            if (autoclear) {
                VkClearValue colorClear;
                std::memcpy(colorClear.color.float32, clearColor, sizeof(clearColor));
                clearValues.reserve(stageCount * 4);
                for (RenderTarget* targ : targets) {
                    if ((int)targ->type & 0b1) {
                        clearValues.push_back(colorClear);
                        if ((int)targ->type & 0b10) {
                            clearValues.push_back(colorClear);
                            if ((int)targ->type & 0b100) {
                                clearValues.push_back(colorClear);
                            }
                        }
                    }
                    if ((int)targ->type & 0b1000) {
                        clearValues.push_back({ 1.0f, 0u });
                    }
                }
            }

            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.framebuffer = fb;
            rpInfo.pClearValues = clearValues.data();
            rpInfo.clearValueCount = (uint32_t)clearValues.size();
            rpInfo.renderArea.offset = {0,0};
            rpInfo.renderArea.extent = {targets[0]->width, targets[0]->height};
            rpInfo.renderPass = rp;

            vkCmdBeginRenderPass(cb, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        }
        else{
            vkCmdNextSubpass(cb, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &targets[currentPass - 1]->dset, 0, nullptr); // 서브패스는 무조건 0부터 시작해야 이게 유지되긴 할듯..
        }
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipeline);
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
        vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &csamp); csamp = VK_NULL_HANDLE;
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
        
        if((reason = vkBeginCommandBuffer(facewise[pass], &cbInfo)) != VK_SUCCESS){
            LOGWITH("Failed to begin command buffer:",reason,resultAsString(reason));
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
            vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, pos, 1, &ub->dset, ub->isDynamic, &off);
        }
        else{
            beginFacewise(pass);
            vkCmdBindDescriptorSets(facewise[pass], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, pos, 1, &ub->dset, ub->isDynamic, &off);
            vkEndCommandBuffer(facewise[pass]);
        }
    }

    void VkMachine::RenderPass2Cube::bind(uint32_t pos, const pTexture& tx){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass2Cube::bind(uint32_t pos, const pStreamTexture& tx) {
        if (!recording) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass2Cube::bind(uint32_t pos, RenderPass* prevPass){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        RenderTarget* target = prevPass->targets.back();
        vkCmdBindDescriptorSets(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipelineLayout, pos, 1, &target->dset, 0, nullptr);
    }
  
    void VkMachine::RenderPass2Cube::usePipeline(Pipeline* pipeline){
        this->pipeline = pipeline;
        if(recording) { vkCmdBindPipeline(scb, VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline); }
    }

    void VkMachine::RenderPass2Cube::push(void* input, uint32_t start, uint32_t end){
        if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdPushConstants(scb, pipeline->pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_ALL_GRAPHICS, start, end - start, input); // TODO: 스테이지 플래그를 살려야 함
    }

    void VkMachine::RenderPass2Cube::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
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
                count = uint32_t(mesh->icount - start);
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
                count = uint32_t(mesh->vcount - start);
            }
            vkCmdDraw(scb, count, 1, start, 0);
        }
        bound = mesh.get();
    }

    void VkMachine::RenderPass2Cube::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count){
         if(!recording){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        VkDeviceSize offs[2] = {0, 0};
        VkBuffer buffs[2] = { mesh->vb };
        if (instanceInfo) { buffs[1] = instanceInfo->vb; }
        vkCmdBindVertexBuffers(scb, 0, instanceInfo ? 2 : 1, buffs, offs);
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->icount - start);
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
                count = uint32_t(mesh->vcount - start);
            }
            vkCmdDraw(scb, count, instanceCount, start, istart);
        }
        bound = nullptr;
    }

    void VkMachine::RenderPass2Cube::execute(RenderPass* other){
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
        
        vkCmdBindPipeline(scb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdSetViewport(scb, 0, 1, &viewport);
        vkCmdSetScissor(scb, 0, 1, &scissor);
    }

    VkMachine::RenderPass2Screen::RenderPass2Screen(VkRenderPass rp, std::vector<RenderTarget*>&& targets, std::vector<VkFramebuffer>&& fbs, VkImage dsImage, VkImageView dsView, VmaAllocation dsAlloc, float* autoclear)
        : targets(targets), fbs(fbs), dsImage(dsImage), dsView(dsView), dsAlloc(dsAlloc), rp(rp), autoclear(false) {
        if (autoclear) {
            std::memcpy(clearColor, autoclear, sizeof(clearColor));
            this->autoclear = true;
        }
        for(VkFence& fence: fences) fence = singleton->createFence(true);
        for(VkSemaphore& semaphore: acquireSm) semaphore = singleton->createSemaphore();
        for(VkSemaphore& semaphore: drawSm) semaphore = singleton->createSemaphore();
        singleton->allocateCommandBuffers(COMMANDBUFFER_COUNT, true, true, cbs);
        pipelines.resize(this->targets.size() + 1, {});
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
        this->width = width;
        this->height = height;
        vkDestroyImageView(singleton->device, dsView, nullptr);
        vmaDestroyImage(singleton->allocator, dsImage, dsAlloc);
        bool useFinalDepth = dsView != VK_NULL_HANDLE;
        dsView = VK_NULL_HANDLE;
        dsImage = VK_NULL_HANDLE;
        dsAlloc = nullptr;

        // ^^^ 스왑이 있으므로 위 파트는 이론상 없어도 알아서 해제되지만 있는 편이 메모리 때문에 더 좋을 것 같음
        RenderPassCreationOptions opts{};
        opts.subpassCount = pipelines.size();
        opts.screenDepthStencil = RenderTargetType(useFinalDepth ? RenderTargetType::RTT_DEPTH | RenderTargetType::RTT_STENCIL : RenderTargetType::RTT_COLOR1);

        std::vector<RenderTargetType> types(targets.size());
        struct bool8 { bool b; };
        std::vector<bool8> useDepth(targets.size());
        for (uint32_t i = 0; i < targets.size(); i++) {
            types[i] = targets[i]->type;
            useDepth[i].b = bool((int)targets[i]->type & 0b1000);
            delete targets[i];
        }
        targets.clear();
        opts.depthInput = (bool*)useDepth.data();

        RenderPass2Screen* newDat = singleton->createRenderPass2Screen(INT32_MIN, windowIdx, opts);
        if (!newDat) {
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
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &ub->dset, ub->isDynamic, &off);
    }

    void VkMachine::RenderPass2Screen::bind(uint32_t pos, const pTexture& tx) {
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass2Screen::bind(uint32_t pos, const pStreamTexture& tx) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &tx->dset, 0, nullptr);
    }

    void VkMachine::RenderPass2Screen::bind(uint32_t pos, RenderPass* prevPass) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        RenderTarget* target = prevPass->targets.back();
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &target->dset, 0, nullptr);
    }

    void VkMachine::RenderPass2Screen::bind(uint32_t pos, RenderPass2Cube* prevPass) {
        if (currentPass == -1) {
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &prevPass->csamp, 0, nullptr);
    }

     void VkMachine::RenderPass2Screen::invoke(const pMesh& mesh, uint32_t start, uint32_t count){
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
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->icount - start);
            }
            vkCmdDrawIndexed(cbs[currentCB], count, 1, start, 0, 0);
        }
        else {
            if((uint64_t)start + count > mesh->vcount){
                LOGWITH("Invalid call: this mesh has",mesh->vcount,"vertices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->vcount - start);
            }
            vkCmdDraw(cbs[currentCB], count, 1, start, 0);
        }
        bound = mesh.get();
    }

    void VkMachine::RenderPass2Screen::invoke(const pMesh& mesh, const pMesh& instanceInfo, uint32_t instanceCount, uint32_t istart, uint32_t start, uint32_t count){
         if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        VkDeviceSize offs[2] = {0, 0};
        VkBuffer buffs[2] = { mesh->vb };
        if (instanceInfo) { buffs[1] = instanceInfo->vb; }
        vkCmdBindVertexBuffers(cbs[currentCB], 0, instanceInfo ? 2 : 1, buffs, offs);
        if(mesh->icount) {
            if((uint64_t)start + count > mesh->icount){
                LOGWITH("Invalid call: this mesh has",mesh->icount,"indices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->icount - start);
            }
            vkCmdBindIndexBuffer(cbs[currentCB], mesh->vb, mesh->ioff, mesh->idxType);
            vkCmdDrawIndexed(cbs[currentCB], count, instanceCount, start, 0, istart);
        }
        else{
            if((uint64_t)start + count > mesh->vcount){
                LOGWITH("Invalid call: this mesh has",mesh->vcount,"vertices but",start,"~",(uint64_t)start+count,"requested to be drawn");
                bound = nullptr;
                return;
            }
            if(count == 0){
                count = uint32_t(mesh->vcount - start);
            }
            vkCmdDraw(cbs[currentCB], count, instanceCount, start, istart);
        }
        bound = nullptr;
    }

    void VkMachine::RenderPass2Screen::clear(RenderTargetType toClear, float* colors) {
        if (currentPass < 0) {
            LOGWITH("This renderPass is currently not running");
            return;
        }
        if (toClear == 0) {
            LOGWITH("no-op");
            return;
        }
        int type = (currentPass == targets.size()) ?
            (dsImage ? (RenderTargetType::RTT_COLOR1 | RenderTargetType::RTT_DEPTH | RenderTargetType::RTT_STENCIL) : RenderTargetType::RTT_COLOR1)
            : targets[currentPass]->type;
        if ((toClear & type) != toClear) {
            LOGWITH("Invalid target selected");
            return;
        }
        if (autoclear) {
            LOGWITH("Autoclear specified. Maybe this call is a mistake?");
        }

        VkClearRect rect{};
        rect.baseArrayLayer = 0;
        rect.layerCount = 1;
        rect.rect.extent = singleton->windowSystems[windowIdx]->swapchain.extent;
        VkClearAttachment clearParam[4];
        int clearCount = 0;
        if (toClear & 0b1) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            pr.colorAttachment = 0;
            std::memcpy(pr.clearValue.color.float32, colors, sizeof(pr.clearValue.color.float32));
            colors += 4;
        }
        if (toClear & 0b10) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            pr.colorAttachment = 1;
            std::memcpy(pr.clearValue.color.float32, colors, sizeof(pr.clearValue.color.float32));
            colors += 4;
        }
        if (toClear & 0b100) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            pr.colorAttachment = 2;
            std::memcpy(pr.clearValue.color.float32, colors, sizeof(pr.clearValue.color.float32));
        }
        if (toClear & 0b11000) {
            VkClearAttachment& pr = clearParam[clearCount++];
            pr.aspectMask = 0;
            if (toClear & 0b1000) pr.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (toClear & 0b10000) pr.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            pr.clearValue.depthStencil.depth = 1.0f;
            pr.clearValue.depthStencil.stencil = 0;
        }
        vkCmdClearAttachments(cbs[currentCB], clearCount, clearParam, 1, &rect);
    }

    void VkMachine::RenderPass2Screen::start(uint32_t pos){
        if(currentPass == targets.size()) {
            LOGWITH("Invalid call. The last subpass already started");
            return;
        }
        WindowSystem* window = singleton->windowSystems[windowIdx];
        if(!window->swapchain.handle) {
            LOGWITH("Swapchain not ready. This message can be ignored safely if the rendering goes fine after now");
            return;
        }
        if (window->needReset) {
            singleton->resetWindow(windowIdx);
            return;
        }
        currentPass++;
        if(!pipelines[currentPass]) {
            LOGWITH("Pipeline not set.");
            currentPass--;
            return;
        }
        if(currentPass == 0){
            reason = vkAcquireNextImageKHR(singleton->device, window->swapchain.handle, UINT64_MAX, acquireSm[currentCB], VK_NULL_HANDLE, &imgIndex);
            if(reason != VK_SUCCESS) {
                LOGWITH("Failed to acquire swapchain image:",reason,resultAsString(reason),"\nThis message can be ignored safely if the rendering goes fine after now");
                currentPass = -1;
                return;
            }

            vkWaitForFences(singleton->device, 1, &fences[currentCB], VK_FALSE, UINT64_MAX);
            vkResetCommandBuffer(cbs[currentCB], 0);
            VkCommandBufferBeginInfo cbInfo{};
            cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            cbInfo.flags = VkCommandBufferUsageFlagBits::VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            reason = vkBeginCommandBuffer(cbs[currentCB], &cbInfo);
            if(reason != VK_SUCCESS){
                LOGWITH("Failed to begin command buffer:",reason,resultAsString(reason));
                currentPass = -1;
                return;
            }
            VkRenderPassBeginInfo rpInfo{};
            std::vector<VkClearValue> clearValues;
            if (autoclear) {
                clearValues.reserve(targets.size() * 4 + 2);
                VkClearValue colorClear;
                std::memcpy(colorClear.color.float32, clearColor, sizeof(clearColor));
                for (RenderTarget* targ : targets) {
                    if ((int)targ->type & 0b1) {
                        clearValues.push_back(colorClear);
                        if ((int)targ->type & 0b10) {
                            clearValues.push_back(colorClear);
                            if ((int)targ->type & 0b100) {
                                clearValues.push_back(colorClear);
                            }
                        }
                    }
                    if ((int)targ->type & 0b1000) {
                        clearValues.push_back({ 1.0f, 0u });
                    }
                }

                clearValues.push_back(colorClear);
                if (dsImage) clearValues.push_back({ 1.0f, 0u });
            }

            rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpInfo.framebuffer = fbs[imgIndex];
            rpInfo.pClearValues = clearValues.data(); // TODO: 렌더패스 첨부물 인덱스에 대응하게 준비해야 함
            rpInfo.clearValueCount = (uint32_t)clearValues.size();
            rpInfo.renderArea.offset = {0,0};
            rpInfo.renderArea.extent = window->swapchain.extent;
            rpInfo.renderPass = rp;
            vkCmdBeginRenderPass(cbs[currentCB], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        }
        else{
            vkCmdNextSubpass(cbs[currentCB], VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindDescriptorSets(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipelineLayout, pos, 1, &targets[currentPass - 1]->dset, 0, nullptr);
        }
        vkCmdBindPipeline(cbs[currentCB], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[currentPass]->pipeline);
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
        if((reason = vkEndCommandBuffer(cbs[currentCB])) != VK_SUCCESS){
            LOGWITH("Failed to end command buffer:",reason,resultAsString(reason));
            return;
        }
        WindowSystem* window = singleton->windowSystems[windowIdx];
        if(!window->swapchain.handle){
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

        if((reason = vkResetFences(singleton->device, 1, &fences[currentCB])) != VK_SUCCESS){
            LOGWITH("Failed to reset fence. waiting or other operations will play incorrect:",reason, resultAsString(reason));
            return;
        }

        if ((reason = singleton->qSubmit(true, 1, &submitInfo, fences[currentCB])) != VK_SUCCESS) {
            LOGWITH("Failed to submit command buffer:",reason,resultAsString(reason));
            return;
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &window->swapchain.handle;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &drawSm[currentCB];
        presentInfo.pImageIndices = &imgIndex;

        recently = currentCB;
        currentCB = (currentCB + 1) % COMMANDBUFFER_COUNT;
        currentPass = -1;

        if((reason = singleton->qSubmit(&presentInfo)) != VK_SUCCESS){
            LOGWITH("Failed to submit command present operation:",reason, resultAsString(reason));
            return;
        }
    }

    void VkMachine::RenderPass2Screen::push(void* input, uint32_t start, uint32_t end){
        if(currentPass == -1){
            LOGWITH("Invalid call: render pass not begun");
            return;
        }
        vkCmdPushConstants(cbs[currentCB], pipelines[currentPass]->pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_ALL_GRAPHICS, start, end - start, input); // TODO: 스펙: 파이프라인 레이아웃 생성 시 단계마다 가용 푸시상수 범위를 분리할 수 있으며(꼭 할 필요는 없는 듯 하긴 함) 여기서 매개변수로 범위와 STAGEFLAGBIT은 일치해야 함
    }

    void VkMachine::RenderPass2Screen::usePipeline(Pipeline* pipeline, uint32_t subpass){
        if(subpass > targets.size()){
            LOGWITH("Invalid subpass. This renderpass has", targets.size() + 1, "subpasses but", subpass, "given");
            return;
        }
        pipelines[subpass] = pipeline;
        if(currentPass == subpass) { vkCmdBindPipeline(cbs[currentCB], VkPipelineBindPoint::VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline); }
    }

    bool VkMachine::RenderPass2Screen::wait(uint64_t timeout){
        return vkWaitForFences(singleton->device, 1, &fences[recently], VK_FALSE, timeout) == VK_SUCCESS; // VK_TIMEOUT이나 VK_ERROR_DEVICE_LOST
    }

    VkMachine::UniformBuffer::UniformBuffer(uint32_t length, uint32_t individual, VkBuffer buffer, VkDescriptorSetLayout layout, VkDescriptorSet dset, VmaAllocation alloc, void* mmap)
    :length(length), individual(individual), buffer(buffer), layout(layout), dset(dset), alloc(alloc), isDynamic(length > 1), mmap(mmap) {
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

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.size = individual * size;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo bainfo{};
        bainfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO;
        bainfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        reason = vmaCreateBufferWithAlignment(singleton->allocator, &bufferInfo, &bainfo, singleton->physicalDevice.minUBOffsetAlignment, &buffer, &alloc, nullptr);
        if(reason != VK_SUCCESS){
            LOGWITH("Failed to create VkBuffer:", reason,resultAsString(reason));
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
        wr.dstBinding = 0;
        wr.pBufferInfo = &dsNBuffer;
        vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);

        if((reason = vmaMapMemory(singleton->allocator, alloc, &mmap)) != VK_SUCCESS){
            LOGWITH("Failed to map memory:", reason,resultAsString(reason));
            return;
        }
    }

    VkMachine::UniformBuffer::~UniformBuffer(){
        vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset);
        vmaDestroyBuffer(singleton->allocator, buffer, alloc);
    }


    // static함수들 구현

    VkInstance createInstance(){
        VkInstance instance;
        VkInstanceCreateInfo instInfo{};

        VkApplicationInfo appInfo{};
        appInfo.sType= VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pEngineName = "YERM";
        appInfo.pApplicationName = "YERM";
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0,0,1,0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        appInfo.engineVersion = VK_MAKE_API_VERSION(0,0,1,0);

        std::vector<const char*> windowExt = Window::requiredInstanceExentsions();

        instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instInfo.pApplicationInfo = &appInfo;
        instInfo.enabledExtensionCount = (uint32_t)windowExt.size();
        instInfo.ppEnabledExtensionNames = windowExt.data();

        const char* VLAYER[] = {"VK_LAYER_KHRONOS_validation"};
        if constexpr(VkMachine::USE_VALIDATION_LAYER){
            instInfo.ppEnabledLayerNames = VLAYER;
            instInfo.enabledLayerCount = 1;
        }

        VkResult result;

        if((result = vkCreateInstance(&instInfo, nullptr, &instance)) != VK_SUCCESS){
            LOGWITH("Failed to create vulkan instance:", result,resultAsString(result));
            VkMachine::reason = result;
            return VK_NULL_HANDLE;
        }
        VkMachine::reason = result;
        return instance;
    }

    VkPhysicalDevice findPhysicalDevice(VkInstance instance, bool* isCpu, uint32_t* graphicsQueue, uint32_t* presentQueue, uint32_t* subQueue, uint32_t* subqIndex, uint64_t* minUBAlignment) {
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
                if (pq == ~0ULL) { pq = i; }
                if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { // 큐 계열 하나로 다 된다면 EXCLUSIVE 모드를 사용할 수 있음
                    gq = pq = i;
                    if (qfs[i].queueCount >= 2) {
                        subq = i;
                        si = 1;
                        break;
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
        float queuePriority[] = { 1.0f, 1.0f, 1.0f };
        qInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qInfo[0].queueFamilyIndex = gq;
        qInfo[0].queueCount = 1 + tqi;
        qInfo[0].pQueuePriorities = queuePriority;

        uint32_t qInfoCount = 1;
        
        if(gq == pq) {
            qInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo[1].queueFamilyIndex = tq;
            qInfo[1].queueCount = 1;
            qInfo[1].pQueuePriorities = queuePriority;
            qInfoCount += (1 - tqi);
        }
        else{
            qInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo[1].queueFamilyIndex = pq;
            qInfo[1].queueCount = 1;
            qInfo[1].pQueuePriorities = queuePriority;

            qInfoCount = 2;

            qInfo[2].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qInfo[2].queueFamilyIndex = tq;
            qInfo[2].queueCount = 1;
            qInfo[2].pQueuePriorities = queuePriority;
            
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
            VkMachine::reason = result;
            return VK_NULL_HANDLE;
        }
        VkMachine::reason = result;
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
            VkMachine::reason = result;
            return nullptr;
        }
        VkMachine::reason = result;
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
            VkMachine::reason = result;
            return 0;
        }
        VkMachine::reason = result;
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
            VkMachine::reason = result;
            return 0;
        }
        VkMachine::reason = result;
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
        dPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        VkDescriptorPool ret;
        VkResult result;
        if((result = vkCreateDescriptorPool(device, &dPoolInfo, nullptr, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create descriptor pool:",result,resultAsString(result));
            VkMachine::reason = result;
            return VK_NULL_HANDLE;
        }
        VkMachine::reason = result;
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

    VkFormat textureFormatFallback(VkPhysicalDevice physicalDevice, int x, int y, uint32_t nChannels, bool srgb, VkMachine::TextureFormatOptions hq, VkImageCreateFlagBits flags) {
    #define CHECK_N_RETURN(f) if(isThisFormatAvailable(physicalDevice,f,x,y,flags)) return f
        switch (nChannels)
        {
        case 4:
        if(srgb){
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC3_SRGB_BLOCK);
            }
            return VK_FORMAT_R8G8B8A8_SRGB;
        }
        else{
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC3_UNORM_BLOCK);
            }
            return VK_FORMAT_R8G8B8A8_UNORM;
        }
        case 3:
        if(srgb){
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
            }
            return VK_FORMAT_R8G8B8_SRGB;
        }
        else{
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
            }
            return VK_FORMAT_R8G8B8_UNORM;
        }
        case 2:
        if(srgb){
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            }
            return VK_FORMAT_R8G8_SRGB;
        }
        else{
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC5_UNORM_BLOCK);
            }
            return VK_FORMAT_R8G8_UNORM;
        }
        case 1:
        if(srgb){
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                // 용량이 줄어드는 포맷이 없음
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                // 용량이 줄어드는 포맷이 없음
            }
            return VK_FORMAT_R8_SRGB;
        }
        else{
            if (hq == VkMachine::TextureFormatOptions::IT_PREFER_QUALITY) {
                // 용량이 줄어드는 포맷이 없음
            }
            else if (hq == VkMachine::TextureFormatOptions::IT_PREFER_COMPRESS) {
                CHECK_N_RETURN(VK_FORMAT_EAC_R11_UNORM_BLOCK);
                CHECK_N_RETURN(VK_FORMAT_BC4_UNORM_BLOCK);
            }
            return VK_FORMAT_R8_UNORM;
        }
        default:
            return VK_FORMAT_UNDEFINED;
        }
    #undef CHECK_N_RETURN
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