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
#define KHRONOS_STATIC
#include "../externals/ktx/ktx.h"

#include <algorithm>
#include <vector>

namespace onart {

    /// @brief Vulkan 인스턴스를 생성합니다. 자동으로 호출됩니다.
    static VkInstance createInstance(Window*);
    /// @brief 사용할 Vulkan 물리 장치를 선택합니다. CPU 기반인 경우 경고를 표시하지만 선택에 실패하지는 않습니다.
    static VkPhysicalDevice findPhysicalDevice(VkInstance, VkSurfaceKHR, bool*, uint32_t*, uint32_t*, uint64_t*);
    /// @brief 주어진 Vulkan 물리 장치에 대한 우선도를 매깁니다. 높을수록 좋게 취급합니다. 대부분의 경우 물리 장치는 하나일 것이므로 함수가 아주 중요하지는 않을 거라 생각됩니다.
    static uint64_t assessPhysicalDevice(VkPhysicalDevice);
    /// @brief 주어진 장치에 대한 가상 장치를 생성합니다.
    static VkDevice createDevice(VkPhysicalDevice, int, int);
    /// @brief 주어진 장치에 대한 메모리 관리자를 세팅합니다.
    static VmaAllocator createAllocator(VkInstance, VkPhysicalDevice, VkDevice);
    /// @brief 명령 풀을 생성합니다.
    static VkCommandPool createCommandPool(VkDevice, int qIndex);
    /// @brief 이미지로부터 뷰를 생성합니다.
    static VkImageView createImageView(VkDevice, VkImage, VkImageViewType, VkFormat, int, int, VkImageAspectFlags);
    /// @brief 주어진 만큼의 기술자 집합을 할당할 수 있는 기술자 풀을 생성합니다.
    static VkDescriptorPool createDescriptorPool(VkDevice device, uint32_t samplerLimit = 256, uint32_t dynUniLimit = 8, uint32_t uniLimit = 16, uint32_t intputAttachmentLimit = 16);
    /// @brief 주어진 기반 형식과 아귀가 맞는, 현재 장치에서 사용 가능한 압축 형식을 리턴합니다.
    static VkFormat textureFormatFallback(VkPhysicalDevice physicalDevice, int x, int y, VkFormat base, bool hq = true, VkImageCreateFlagBits flags = VkImageCreateFlagBits::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

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
            LOGWITH("Failed to create Window surface:", result);
            free();
            return;
        }

        bool isCpu;
        if(!(physicalDevice.card = findPhysicalDevice(instance, surface.handle, &isCpu, &physicalDevice.gq, &physicalDevice.pq, &physicalDevice.minUBOffsetAlignment))) {
            LOGWITH("Couldn\'t find any appropriate graphics device");
            free();
            return;
        }
        if(isCpu) LOGWITH("Warning: this device is CPU");
        // properties.limits.minMemorymapAlignment, minTexelBufferOffsetAlignment, minUniformBufferOffsetAlignment, minStorageBufferOffsetAlignment, optimalBufferCopyOffsetAlignment, optimalBufferCopyRowPitchAlignment를 저장

        checkSurfaceHandle();

        if(!(device = createDevice(physicalDevice.card, physicalDevice.gq, physicalDevice.pq))) {
            free();
            return;
        }

        vkGetDeviceQueue(device, physicalDevice.gq, 0, &graphicsQueue);
        vkGetDeviceQueue(device, physicalDevice.pq, 0, &presentQueue);


        if(!(allocator = createAllocator(instance, physicalDevice.card, device))){
            free();
            return;
        }

        if(!(gCommandPool = createCommandPool(device, physicalDevice.gq))){
            free();
            return;
        }

        allocateCommandBuffers(sizeof(baseBuffer)/sizeof(baseBuffer[0]), true, baseBuffer);
        if(!baseBuffer[0]){
            free();
        }
        int w,h;
        window->getSize(&w,&h);
        createSwapchain(w, h, physicalDevice.gq, physicalDevice.pq);

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
        VkResult result = vkCreateFence(device, &fenceInfo, nullptr, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create fence:",result);
            return nullptr;
        }
        return ret;
    }

    VkSemaphore VkMachine::createSemaphore(){
        VkSemaphoreCreateInfo smInfo{};
        smInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkSemaphore ret;
        VkResult result = vkCreateSemaphore(device, &smInfo, nullptr, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create fence:",result);
            return nullptr;
        }
        return ret;
    }

    VkPipeline VkMachine::getPipeline(const string16& name){
        auto it = pipelines.find(name);
        if(it != pipelines.end()) return it->second;
        else return nullptr;
    }

    VkPipelineLayout VkMachine::getPipelineLayout(const string16& name){
        auto it = pipelineLayouts.find(name);
        if(it != pipelineLayouts.end()) return it->second;
        else return nullptr;
    }

    VkMachine::RenderTarget* VkMachine::getRenderTarget(const string16& name){
        auto it = renderTargets.find(name);
        if(it != renderTargets.end()) return it->second;
        else return nullptr;
    }

    VkMachine::UniformBuffer* VkMachine::getUniformBuffer(const string16& name){
        auto it = uniformBuffers.find(name);
        if(it != uniformBuffers.end()) return it->second;
        else return nullptr;
    }
            
    VkShaderModule VkMachine::getShader(const string16& name){
        auto it = shaders.find(name);
        if(it != shaders.end()) return it->second;
        else return nullptr;
    }
            
    VkMachine::pTexture VkMachine::getTexture(const string128& name){
        auto it = textures.find(name);
        if(it != textures.end()) return it->second;
        else return pTexture();
    }

    void VkMachine::resetWindow(Window* window) {
        destroySwapchain();
        vkDestroySurfaceKHR(instance, surface.handle, nullptr);
        VkResult result = window->createWindowSurface(instance, &surface.handle);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create new window surface:", result);
            return;
        }
        checkSurfaceHandle();
        int x,y;
        window->getSize(&x,&y);
        createSwapchain(x, y, physicalDevice.gq, physicalDevice.pq);
    }

    void VkMachine::allocateCommandBuffers(int count, bool isPrimary, VkCommandBuffer* buffers){
        VkCommandBufferAllocateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.level = isPrimary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        bufferInfo.commandPool = gCommandPool;
        bufferInfo.commandBufferCount = count;
        VkResult result;
        if((result = vkAllocateCommandBuffers(device, &bufferInfo, buffers))!=VK_SUCCESS){
            LOGWITH("Failed to allocate command buffers:", result);
            buffers[0] = nullptr;
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

    void VkMachine::createSwapchain(uint32_t width, uint32_t height, uint32_t gq, uint32_t pq){
        destroySwapchain();
        VkSwapchainCreateInfoKHR scInfo{};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.surface = surface.handle;
        scInfo.minImageCount = 2;
        scInfo.imageFormat = surface.format.format;
        scInfo.imageColorSpace = surface.format.colorSpace;
        scInfo.imageExtent.width = std::clamp(width, surface.caps.minImageExtent.width, surface.caps.maxImageExtent.width);
        scInfo.imageExtent.height = std::clamp(height, surface.caps.minImageExtent.height, surface.caps.maxImageExtent.height);
        scInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        scInfo.imageArrayLayers = 1;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.preTransform = VkSurfaceTransformFlagBitsKHR::VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.clipped = VK_TRUE;
        scInfo.oldSwapchain = VK_NULL_HANDLE; // 같은 표면에 대한 핸들은 올드로 사용할 수 없음
        uint32_t qfi[2] = {gq, pq};
        if(gq == pq){
            scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else{
            scInfo.queueFamilyIndexCount = 2;
            scInfo.pQueueFamilyIndices = qfi;
        }

        VkResult result;
        if((result = vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain.handle))!=VK_SUCCESS){
            LOGWITH("Failed to create swapchain:",result);
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

    }

    void VkMachine::destroySwapchain(){
        for(VkImageView v: swapchain.imageView){ vkDestroyImageView(device, v, nullptr); }
        vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
        swapchain.imageView.clear();
        swapchain.handle = 0;
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
                LOGWITH("Failed to create texture descriptor set layout binding ",txBinding.binding,':', result);
                return false;
            }
        }

        txBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

        for(txBinding.binding = 0; txBinding.binding < 4; txBinding.binding++){
            VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &inputAttachmentLayout[txBinding.binding]);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to create input attachment descriptor set layout binding ",txBinding.binding,':', result);
                return false;
            }
        }

        return true;
    }

    void VkMachine::free() {
        for(VkDescriptorSetLayout& layout: textureLayout) { vkDestroyDescriptorSetLayout(device, layout, nullptr); layout = nullptr; }
        for(VkDescriptorSetLayout& layout: inputAttachmentLayout) { vkDestroyDescriptorSetLayout(device, layout, nullptr); layout = nullptr; }
        for(VkSampler& sampler: textureSampler) { vkDestroySampler(device, sampler, nullptr); sampler = nullptr; }
        for(auto& rp: renderPasses) { delete rp.second; }
        for(auto& rt: renderTargets){ delete rt.second; }
        for(auto& sh: shaders) { vkDestroyShaderModule(device, sh.second, nullptr); }
        for(auto& pp: pipelines) { vkDestroyPipeline(device, pp.second, nullptr); }
        for(auto& pp: pipelineLayouts) { vkDestroyPipelineLayout(device, pp.second, nullptr); }

        pipelines.clear();
        pipelineLayouts.clear();
        renderPasses.clear();
        renderTargets.clear();
        shaders.clear();
        destroySwapchain();
        vmaDestroyAllocator(allocator);
        vkDestroyCommandPool(device, gCommandPool, nullptr);
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface.handle, nullptr);
        vkDestroyInstance(instance, nullptr);
        allocator = nullptr;
        gCommandPool = 0;
        descriptorPool = nullptr;
        device = nullptr;
        graphicsQueue = nullptr;
        presentQueue = nullptr;
        surface.handle = nullptr;
        instance = nullptr;
    }

    void VkMachine::allocateDescriptorSets(VkDescriptorSetLayout* layouts, uint32_t count, VkDescriptorSet* output){
        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.pSetLayouts = layouts;
        dsAllocInfo.descriptorSetCount = count;
        dsAllocInfo.descriptorPool = descriptorPool;

        VkResult result = vkAllocateDescriptorSets(device, &dsAllocInfo, output);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to allocate descriptor sets:",result);
            output[0]=nullptr;
        }
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
        VkResult result;
        for(int i = 0; i < sizeof(textureSampler)/sizeof(textureSampler[0]);i++, samplerInfo.maxLod++){
            result = vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler[i]);
            if(result != VK_SUCCESS){
                LOGWITH("Failed to create texture sampler:", result);
                return false;
            }
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

    VkMachine::RenderTarget* VkMachine::createRenderTarget2D(int width, int height, const string16& name, RenderTargetType type, bool sampled, bool mmap){
        if(!allocator) {
            LOGWITH("Warning: Tried to create image before initialization");
            return nullptr;
        }
        auto it = renderTargets.find(name);
        if(it != renderTargets.end()) {return it->second;}
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
            imgInfo.format = VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
            //imgInfo.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            result = vmaCreateImage(allocator, &imgInfo, &allocInfo, &color1->img, &color1->alloc, nullptr);
            if(!result) {
                LOGWITH("Failed to create image:", result);
                delete color1;
                return nullptr;
            }
            color1->view = createImageView(device, color1->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
            if(!color1->view) {
                color1->free();
                delete color1;
                return nullptr;
            }
            if((int)type & 0b10){
                color2 = new ImageSet;
                result = vmaCreateImage(allocator, &imgInfo, &allocInfo, &color2->img, &color2->alloc, nullptr);
                if(!result) {
                    LOGWITH("Failed to create image:", result);
                    color1->free();
                    delete color1;
                    delete color2;
                    return nullptr;
                }
                color2->view = createImageView(device, color2->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
                if(!color2->view) {
                    color1->free();
                    color2->free();
                    delete color1;
                    delete color2;
                    return nullptr;
                }
                if((int)type & 0b100){
                    color3 = new ImageSet;
                    result = vmaCreateImage(allocator, &imgInfo, &allocInfo, &color3->img, &color3->alloc, nullptr);
                    if(!result) {
                        LOGWITH("Failed to create image:", result);
                        color1->free();
                        color2->free();
                        delete color1;
                        delete color2;
                        return nullptr;
                    }
                    color3->view = createImageView(device, color3->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
                    if(!color3->view) {
                        vmaDestroyImage(allocator, color1->img, color1->alloc);
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
            imgInfo.usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | (sampled ? VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT : VkImageUsageFlagBits::VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
            imgInfo.format = VkFormat::VK_FORMAT_D24_UNORM_S8_UINT;
            //imgInfo.initialLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            result = vmaCreateImage(allocator, &imgInfo, &allocInfo, &ds->img, &ds->alloc, nullptr);
            if(!result) {
                LOGWITH("Failed to create image: ", result);
                if(color1) {color1->free(); delete color1;}
                if(color2) {color2->free(); delete color2;}
                if(color3) {color3->free(); delete color3;}
                delete ds;
                return nullptr;
            }
            ds->view = createImageView(device, color3->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT);
            if(!ds->view){
                if(color1) {color1->free(); delete color1;}
                if(color2) {color2->free(); delete color2;}
                if(color3) {color3->free(); delete color3;}
                ds->free(); delete ds;
                return nullptr;
            }
        }
        int nim = 0;
        if(color1) {images.insert(color1); nim++;}
        if(color2) {images.insert(color2); nim++;}
        if(color3) {images.insert(color3); nim++;}
        if(ds) {images.insert(ds); nim++;}

        VkDescriptorSetLayout layout = sampled ? textureLayout[0] : inputAttachmentLayout[0];
        VkDescriptorSetLayout layouts[4] = {layout, layout, layout, layout};
        VkDescriptorSet dsets[4];

        allocateDescriptorSets(layouts, nim, dsets);
        if(!dsets[0]){
            LOGHERE;
            if(color1) {color1->free(); delete color1;}
            if(color2) {color2->free(); delete color2;}
            if(color3) {color3->free(); delete color3;}
            ds->free(); delete ds;
            return;
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
            imageInfo.sampler = textureSampler[0];
            wr.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else {
            wr.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }
        nim = 0;
        if(color1){
            imageInfo.imageView = color1->view;
            wr.dstSet = dsets[nim++];
            vkUpdateDescriptorSets(device, 1, &wr, 0, nullptr);
            if(color2){
                imageInfo.imageView = color2->view;
                wr.dstSet = dsets[nim++];
                vkUpdateDescriptorSets(device, 1, &wr, 0, nullptr);
                if(color3){
                    imageInfo.imageView = color3->view;
                    wr.dstSet = dsets[nim++];
                    vkUpdateDescriptorSets(device, 1, &wr, 0, nullptr);
                }
            }
        }
        if(ds){
            imageInfo.imageView = ds->view;
            wr.dstSet = dsets[nim];
            vkUpdateDescriptorSets(device, 1, &wr, 0, nullptr);
        }

        return renderTargets.emplace(name, new RenderTarget(type, width, height, color1, color2, color3, ds, sampled, mmap, dsets)).first->second;
    }

    void VkMachine::removeImageSet(VkMachine::ImageSet* set) {
        auto it = images.find(set);
        if(it != images.end()) {
            (*it)->free();
            delete *it;
            images.erase(it);
        }
    }

    VkShaderModule VkMachine::createShader(const uint32_t* spv, size_t size, const string16& name) {
        VkShaderModule ret = getShader(name);
        if(ret) return ret;
        
        VkShaderModuleCreateInfo smInfo{};
        smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        smInfo.codeSize = size;
        smInfo.pCode = spv;
        VkResult result = vkCreateShaderModule(device, &smInfo, nullptr, &ret);
        if(result != VK_SUCCESS) {
            LOGWITH("Failed to create shader moudle:", result);
            return nullptr;
        }
        return shaders[name] = ret;
    }

    VkMachine::pTexture VkMachine::createTexture(void* ktxObj, const string128& name){
        ktxTexture2* texture = reinterpret_cast<ktxTexture2*>(ktxObj);
        if (texture->numLevels == 0) return pTexture();
        VkFormat availableFormat;
        ktx_error_code_e k2result;
        if(ktxTexture2_NeedsTranscoding(texture)){
            ktx_transcode_fmt_e tf;
            switch (availableFormat = textureFormatFallback(physicalDevice.card, texture->baseWidth, texture->baseHeight, (VkFormat)texture->vkFormat, texture->isCubemap ? VkImageCreateFlagBits::VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : (VkImageCreateFlagBits)0))
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
            LOGWITH("Failed to create buffer:",result);
            ktxTexture_Destroy(ktxTexture(texture));
            return pTexture();
        }
        void* mmap;
        result = vmaMapMemory(allocator, newAlloc, &mmap);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to map memory to buffer:",result);
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
        allocateCommandBuffers(1, true, &copyCmd);

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
            LOGWITH("Failed to begin command buffer:",result);
            ktxTexture_Destroy(ktxTexture(texture));
            vkFreeCommandBuffers(device, gCommandPool, 1, &copyCmd);
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
            LOGWITH("Failed to end command buffer:",result);
            ktxTexture_Destroy(ktxTexture(texture));
            vkFreeCommandBuffers(device, gCommandPool, 1, &copyCmd);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
            return pTexture();
        }
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCmd;
        if((result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr)) != VK_SUCCESS){
            LOGWITH("Failed to submit copy command:",result);
            ktxTexture_Destroy(ktxTexture(texture));
            vkFreeCommandBuffers(device, gCommandPool, 1, &copyCmd);
            vmaDestroyImage(allocator, newImg, newAlloc2);
            vmaDestroyBuffer(allocator, newBuffer, newAlloc);
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
        if((result = vkCreateImageView(device, &viewInfo, nullptr, &newView)) != VK_SUCCESS){
            LOGWITH("Failed to create image view:",result);
            return pTexture();
        }

        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, gCommandPool, 1, &copyCmd);
        vmaDestroyBuffer(allocator, newBuffer, newAlloc);

        if(result != VK_SUCCESS){
            LOGWITH("Failed to create image view:",result);
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
        return textures[name] = std::make_shared<txtr>(newImg, newView, newAlloc2, newSet, 0);
    }

    VkMachine::pTexture VkMachine::createTexture(const string128& fileName, const string128& name, bool ubtcs1){
        const string128& _name = name.size() == 0 ? fileName : name;
        pTexture ret(std::move(getTexture(_name)));
        if(ret) return ret;
        
        ktxTexture2* texture;
        ktx_error_code_e k2result;
        if((k2result= ktxTexture2_CreateFromNamedFile(fileName.c_str(), KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS){
            LOGWITH("Failed to load ktx texture:",k2result);
            return pTexture();
        }
        return createTexture(texture, _name);
    }

    VkMachine::pTexture VkMachine::createTexture(const uint8_t* mem, size_t size, const string128& name){
        pTexture ret(std::move(getTexture(name)));
        if(ret) return ret;
        ktxTexture2* texture;
        ktx_error_code_e k2result;
        if((k2result = ktxTexture2_CreateFromMemory(mem, size, KTX_TEXTURE_CREATE_NO_FLAGS, &texture)) != KTX_SUCCESS){
            LOGWITH("Failed to load ktx texture:",k2result);
            return pTexture();
        }
        return createTexture(texture, name);
    }

    VkMachine::Texture::Texture(VkImage img, VkImageView view, VmaAllocation alloc, VkDescriptorSet dset, uint32_t binding):img(img), view(view), alloc(alloc), dset(dset), binding(binding){ }
    VkMachine::Texture::~Texture(){
        vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset);
        vkDestroyImageView(singleton->device, view, nullptr);
        vmaDestroyImage(singleton->allocator, img, alloc);
    }

    VkDescriptorSetLayout VkMachine::Texture::getLayout(){
        return singleton->textureLayout[binding];
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

    VkMachine::UniformBuffer* VkMachine::createUniformBuffer(uint32_t length, uint32_t size, VkShaderStageFlags stages, const string16& name, uint32_t binding){
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
            individual = (size + physicalDevice.minUBOffsetAlignment - 1);
            individual -= individual % physicalDevice.minUBOffsetAlignment;
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
        
        if((result = vkCreateDescriptorSetLayout(device, &uboInfo, nullptr, &layout)) != VK_SUCCESS){
            LOGWITH("Failed to create descriptor set layout:",result);
            return nullptr;
        }

        allocateDescriptorSets(&layout, 1, &dset);
        if(!dset){
            LOGHERE;
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
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
            result = vmaCreateBufferWithAlignment(allocator, &bufferInfo, &bainfo, physicalDevice.minUBOffsetAlignment, &buffer, &alloc, nullptr);
        }
        else{
            result = vmaCreateBuffer(allocator, &bufferInfo, &bainfo, &buffer, &alloc, nullptr);
        }
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create buffer:", result);
            return nullptr;
        }

        if((result = vmaMapMemory(allocator, alloc, &mmap)) != VK_SUCCESS){
            LOGWITH("Failed to map memory:", result);
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

        return uniformBuffers[name] = new UniformBuffer(length, individual, buffer, layout, dset, alloc, mmap, binding);
    }

    uint32_t VkMachine::RenderTarget::attachmentRefs(VkAttachmentDescription* arr){
        uint32_t colorCount = 0;
        if(color1) {
            arr[0].format = VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
            arr[0].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            arr[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            arr[0].storeOp = (sampled || mapped) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
            arr[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            arr[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            arr[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            arr[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
            arr[colorCount].storeOp = sampled || mapped ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE; // 그림자맵에서야 필요하고 그 외에는 필요없음
            arr[colorCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            arr[colorCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            arr[colorCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            arr[colorCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        return colorCount;
    }

    VkMachine::RenderTarget::~RenderTarget(){
        if(color1) { singleton->removeImageSet(color1); if(dset1) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset1); }
        if(color2) { singleton->removeImageSet(color2); if(dset2) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset2); }
        if(color3) { singleton->removeImageSet(color3); if(dset3) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset3); }
        if(depthstencil) { singleton->removeImageSet(depthstencil); if(dsetDS) vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dsetDS); }
    }

    VkMachine::RenderPass* VkMachine::createRenderPass(RenderTarget** targets, uint32_t subpassCount, const string16& name){
        auto it = renderPasses.find(name);
        if(it != renderPasses.end()) return it->second;
        if(subpassCount == 0) return nullptr;
        
        std::vector<VkSubpassDescription> subpasses(subpassCount);
        std::vector<VkAttachmentDescription> attachments(subpassCount * 4);
        std::vector<VkAttachmentReference> refs(subpassCount * 4);
        std::vector<VkSubpassDependency> dependencies(subpassCount);
        std::vector<VkImageView> ivs(subpassCount * 4);

        uint32_t totalAttachments = 0;
        uint32_t inputAttachmentCount = 0;
        for(uint32_t i = 0; i < subpassCount; i++){
            uint32_t colorCount = targets[i]->attachmentRefs(&attachments[totalAttachments]);
            subpasses[i].pipelineBindPoint= VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpasses[i].colorAttachmentCount = colorCount;
            subpasses[i].pColorAttachments = &refs[totalAttachments];
            subpasses[i].inputAttachmentCount = inputAttachmentCount;
            subpasses[i].pInputAttachments = &refs[totalAttachments - inputAttachmentCount];
            if(targets[i]->depthstencil) subpasses[i].pDepthStencilAttachment = &refs[totalAttachments + colorCount];
            VkImageView views[4] = {targets[i]->color1->view, targets[i]->color2->view, targets[i]->color3->view, targets[i]->depthstencil->view};
            for(uint32_t j = 0; j < colorCount; j++) { 
                refs[totalAttachments].attachment = totalAttachments;
                refs[totalAttachments].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                ivs[totalAttachments] = views[j];
                totalAttachments++;
            }
            if(targets[i]->depthstencil){
                refs[totalAttachments].attachment = totalAttachments;
                refs[totalAttachments].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                ivs[totalAttachments] = views[3];
                totalAttachments++;
            }
            dependencies[i].srcSubpass = i - 1;
            dependencies[i].dstSubpass = i;
            dependencies[i].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[i].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[i].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[i].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            dependencies[i].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            inputAttachmentCount = colorCount; if(targets[i]->depthstencil) inputAttachmentCount++;
        }
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.subpassCount = subpassCount;
        rpInfo.pSubpasses = subpasses.data();
        rpInfo.attachmentCount = totalAttachments;
        rpInfo.pAttachments = attachments.data();
        rpInfo.dependencyCount = subpassCount - 1; // 스왑체인 의존성은 이 함수를 통해 만들지 않기 때문에 이대로 사용
        rpInfo.pDependencies = &dependencies[1];
        VkRenderPass newPass;
        VkResult result;
        if((result = vkCreateRenderPass(device, &rpInfo, nullptr, &newPass)) != VK_SUCCESS){
            LOGWITH("Failed to create renderpass:",result);
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
        if((result = vkCreateFramebuffer(device, &fbInfo, nullptr, &fb)) != VK_SUCCESS){
            LOGWITH("Failed to create framebuffer:",result);
            return nullptr;
        }
        RenderPass* ret = renderPasses[name] = new RenderPass(newPass, fb, subpassCount);
        for(uint32_t i = 0; i < subpassCount; i++){ ret->targets[i] = targets[i]; }
        return ret;
    }

    VkPipeline VkMachine::createPipeline(VkVertexInputAttributeDescription* vinfo, uint32_t size, uint32_t vattr, RenderPass* pass, uint32_t subpass, uint32_t flags, VkPipelineLayout layout, VkShaderModule vs, VkShaderModule fs, const string16& name, VkStencilOpState* front, VkStencilOpState* back){
        VkPipeline ret = getPipeline(name);
        if(ret) return ret;

        const uint32_t OPT_COLOR_COUNT = 
            (uint32_t)pass->targets[subpass]->type & 0b100 ? 3 :
            (uint32_t)pass->targets[subpass]->type & 0b10 ? 2 :
            (uint32_t)pass->targets[subpass]->type & 0b1 ? 1 :
            0;
        const bool OPT_USE_DEPTHSTENCIL = (int)pass->targets[subpass]->type & 0b1000;

        VkPipelineShaderStageCreateInfo shaderStagesInfo[2] = {};
        shaderStagesInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStagesInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStagesInfo[0].module = vs;
        shaderStagesInfo[0].pName = "main";
        
        shaderStagesInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStagesInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStagesInfo[1].module = fs;
        shaderStagesInfo[1].pName = "main";

        VkVertexInputBindingDescription vbind{};
        vbind.binding = 0;
        vbind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vbind.stride = size;

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1; // TODO: 인스턴싱을 위한 인터페이스
        vertexInputInfo.pVertexBindingDescriptions = &vbind;
        vertexInputInfo.vertexAttributeDescriptionCount = vattr;
        vertexInputInfo.pVertexAttributeDescriptions = vinfo;

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
            dsInfo.depthTestEnable = (flags & PipelineOptions::USE_DEPTH) ? VK_TRUE : VK_FALSE;
            dsInfo.depthWriteEnable = dsInfo.depthWriteEnable;
            dsInfo.stencilTestEnable = (flags & PipelineOptions::USE_STENCIL) ? VK_TRUE : VK_FALSE;
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

        VkGraphicsPipelineCreateInfo pInfo{};
        pInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pInfo.stageCount = sizeof(shaderStagesInfo) / sizeof(shaderStagesInfo[0]);
        pInfo.pStages = shaderStagesInfo;
        pInfo.pVertexInputState = &vertexInputInfo;
        pInfo.renderPass = pass->rp;
        pInfo.subpass = subpass;
        pInfo.pDynamicState = &dynInfo;
        pInfo.layout = layout;
        pInfo.pRasterizationState = &rtrInfo;
        pInfo.pInputAssemblyState = &inputAssemblyInfo;
        // pInfo.pMultisampleState, pInfo.pTessellationState // TODO: 선택권
        if(OPT_COLOR_COUNT) { pInfo.pColorBlendState = &colorBlendStateCreateInfo; }
        if(OPT_USE_DEPTHSTENCIL){ pInfo.pDepthStencilState = &dsInfo; }
        VkResult result = vkCreateGraphicsPipelines(device, nullptr, 1, &pInfo, nullptr, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create pipeline:",result);
            return nullptr;
        }
        pass->usePipeline(ret, layout, subpass);
        return pipelines[name] = ret;
    }

    VkPipelineLayout VkMachine::createPipelineLayout(VkDescriptorSetLayout* layouts, uint32_t count,  VkShaderStageFlags stages, const string16& name){
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
        
        VkResult result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &ret);
        if(result != VK_SUCCESS){
            LOGWITH("Failed to create pipeline layout:",result);
            return nullptr;
        }
        return pipelineLayouts[name] = ret;
    }

    VkMachine::RenderPass::RenderPass(VkRenderPass rp, VkFramebuffer fb, uint16_t stageCount): rp(rp), fb(fb), stageCount(stageCount), pipelines(stageCount), targets(stageCount){
        fence = singleton->createFence(true);
        singleton->allocateCommandBuffers(1, true, &cb);
    }

    VkMachine::RenderPass::~RenderPass(){
        vkFreeCommandBuffers(singleton->device, singleton->gCommandPool, 1, &cb);
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
        fb = nullptr;
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
            LOGWITH("Failed to create framebuffer:", result);
        }
    }

    bool VkMachine::RenderPass::wait(uint64_t timeout){
        return vkWaitForFences(singleton->device, 1, &fence, VK_FALSE, timeout) == VK_SUCCESS; // VK_TIMEOUT이나 VK_ERROR_DEVICE_LOST
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
            indices.swap(decltype(indices)());
            resize(length * 3 / 2);
            ret = indices.top();
        }
        indices.pop();
        return ret;
    }

    void VkMachine::UniformBuffer::update(const void* input, uint32_t index, uint32_t offset, uint32_t size){
        std::memcpy(&staged[index * individual + offset], input, size);
    }

    void VkMachine::UniformBuffer::sync(){
        std::memcpy(mmap, staged.data(), staged.size());
        vmaInvalidateAllocation(singleton->allocator, alloc, 0, VK_WHOLE_SIZE);
        vmaFlushAllocation(singleton->allocator, alloc, 0, VK_WHOLE_SIZE);
    }

    void VkMachine::UniformBuffer::resize(uint32_t size) {
        if(!isDynamic || size == length) return;
        staged.resize(individual * size);
        if(size > length) {
            for(uint32_t i = length; i < size; i++){
                indices.push(i);
            }
        }
        length = size;
        vmaUnmapMemory(singleton->allocator, alloc);
        vmaDestroyBuffer(singleton->allocator, buffer, alloc); // 이것 때문에 렌더링과 동시에 진행 불가능
        buffer = nullptr;
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
            LOGWITH("Failed to create VkBuffer:", result);
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
            LOGWITH("Failed to map memory:", result);
            return;
        }
    }

    VkMachine::UniformBuffer::~UniformBuffer(){
        vkFreeDescriptorSets(singleton->device, singleton->descriptorPool, 1, &dset);
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
            LOGWITH("Failed to create vulkan instance:", result);
            return nullptr;
        }
        return instance;
    }

    VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool* isCpu, uint32_t* graphicsQueue, uint32_t* presentQueue, uint64_t* minUBAlignment) {
        uint32_t count;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> cards(count);
        vkEnumeratePhysicalDevices(instance, &count, cards.data());

        uint64_t maxScore = 0;
        VkPhysicalDevice goodCard = nullptr;
        uint32_t maxGq = 0, maxPq = 0;
        for(VkPhysicalDevice card: cards) {

            uint32_t qfcount;
            uint64_t gq = ~0ULL, pq = ~0ULL;
            vkGetPhysicalDeviceQueueFamilyProperties(card, &qfcount, nullptr);
            std::vector<VkQueueFamilyProperties> qfs(qfcount);
            vkGetPhysicalDeviceQueueFamilyProperties(card, &qfcount, qfs.data());

            // 큐 계열: GRAPHICS, PRESENT 사용이 안 되면 0점
            for(uint32_t i = 0; i < qfcount ; i++){
                if(qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    if(gq == ~0ULL) {
                        gq = i;
                    }
                }
                VkBool32 supported;
                vkGetPhysicalDeviceSurfaceSupportKHR(card, i, surface, &supported);
                if(supported) {
                    if(pq == ~0ULL){ pq = i; }
                    if(qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { // 큐 계열 하나로 다 된다면 EXCLUSIVE 모드를 사용할 수 있음
                        gq = pq = i; break;
                    }
                }
                if((gq == ~0ULL) && (pq == ~0ULL)) break;
            }

            if (gq < 0 || pq < 0) continue;

            uint64_t score = assessPhysicalDevice(card);
            if(score > maxScore) {
                maxScore = score;
                goodCard = card;
                maxGq = (uint32_t)gq;
                maxPq = (uint32_t)pq;
            }
        }
        *isCpu = !(maxScore & (0b111ULL << 61));
        *graphicsQueue = maxGq;
        *presentQueue = maxPq;
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
        if(features.imageCubeArray) score |= (1ULL << 55);
        if(features.textureCompressionASTC_LDR) score |= (1ULL << 54);
        if(features.textureCompressionBC) score |= (1ULL << 53);
        if(features.textureCompressionETC2) score |= (1ULL << 52);
        if(features.tessellationShader) score |= (1ULL << 51);
        if(features.geometryShader) score |= (1ULL << 50);
        return score;
    }

    VkDevice createDevice(VkPhysicalDevice card, int gq, int pq) {
        VkDeviceQueueCreateInfo qInfo[2]{};
        float queuePriority = 1.0f;
        qInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qInfo[0].queueFamilyIndex = gq;
        qInfo[0].queueCount = 1;
        qInfo[0].pQueuePriorities = &queuePriority;

        qInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qInfo[1].queueFamilyIndex = gq;
        qInfo[1].queueCount = 1;
        qInfo[1].pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures wantedFeatures{};
        VkPhysicalDeviceFeatures availableFeatures;
        vkGetPhysicalDeviceFeatures(card, &availableFeatures);
        wantedFeatures.imageCubeArray = availableFeatures.imageCubeArray;
        wantedFeatures.textureCompressionASTC_LDR = availableFeatures.textureCompressionASTC_LDR;
        wantedFeatures.textureCompressionBC = availableFeatures.textureCompressionBC;
        wantedFeatures.textureCompressionETC2 = availableFeatures.textureCompressionETC2;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = qInfo;
        deviceInfo.queueCreateInfoCount = 1 + (gq != pq);
        deviceInfo.pEnabledFeatures = &wantedFeatures;
        deviceInfo.ppEnabledExtensionNames = VK_DESIRED_DEVICE_EXT;
        deviceInfo.enabledExtensionCount = sizeof(VK_DESIRED_DEVICE_EXT) / sizeof(VK_DESIRED_DEVICE_EXT[0]);

        VkDevice ret;
        VkResult result;
        if((result = vkCreateDevice(card, &deviceInfo, nullptr, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create Vulkan device:", result);
            return nullptr;
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
            LOGWITH("Failed to create VMA object:",result);
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
            LOGWITH("Failed to create command pool:", result);
            return 0;
        }
        return ret;
    }

    VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType type, VkFormat format, int levelCount, int layerCount, VkImageAspectFlags aspect){
        VkImageViewCreateInfo ivInfo{};
        ivInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivInfo.format = format;
        ivInfo.image = image;
        ivInfo.viewType = type;
        ivInfo.subresourceRange.aspectMask = aspect;
        ivInfo.subresourceRange.baseArrayLayer = 0;
        ivInfo.subresourceRange.layerCount = layerCount;
        ivInfo.subresourceRange.levelCount = levelCount;

        VkImageView ret;
        VkResult result;
        if((result = vkCreateImageView(device, &ivInfo, nullptr, &ret)) != VK_SUCCESS){
            LOGWITH("Failed to create image view:",result);
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
            LOGWITH("Failed to create descriptor pool:",result);
            return nullptr;
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

    VkFormat textureFormatFallback(VkPhysicalDevice physicalDevice, int x, int y, VkFormat base, bool hq, VkImageCreateFlagBits flags) {
    #define CHECK_N_RETURN(f) if(isThisFormatAvailable(physicalDevice,f,x,y,flags)) return f
        switch (base)
        {
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_UNORM:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return base;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC3_UNORM_BLOCK);
            break;
        case VK_FORMAT_R8G8B8A8_SRGB:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            if(hq) return base;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC3_SRGB_BLOCK);
            break;
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_UNORM:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return base;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC1_RGB_UNORM_BLOCK);
            break;
        case VK_FORMAT_R8G8B8_SRGB:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            if(hq) return base;
            CHECK_N_RETURN(VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC1_RGB_SRGB_BLOCK);
            break;
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_UNORM:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return base;
            CHECK_N_RETURN(VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC5_UNORM_BLOCK);
            break;
        case VK_FORMAT_R8G8_SRGB:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            break;
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_UNORM:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_UNORM_BLOCK);
            if(hq) return base;
            CHECK_N_RETURN(VK_FORMAT_EAC_R11_UNORM_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC4_UNORM_BLOCK);
            break;
        case VK_FORMAT_R8_SRGB:
            CHECK_N_RETURN(VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
            CHECK_N_RETURN(VK_FORMAT_BC7_SRGB_BLOCK);
            break;
        default:
            break;
        }
        return base;
    #undef CHECK_N_RETURN
    }

}