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
        
        singleton = this;
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

    void VkMachine::free() {
        for(auto& rt: renderTargets){
            delete rt.second;
        }
        renderTargets.clear();
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

    VkMachine::~VkMachine(){
        free();
    }

    void VkMachine::ImageSet::free(VkDevice device, VmaAllocator allocator) {
        vkDestroyImageView(device, view, nullptr);
        vmaDestroyImage(allocator, img, alloc);
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
                color1->free(device, allocator);
                delete color1;
                return nullptr;
            }
            if((int)type & 0b10){
                color2 = new ImageSet;
                result = vmaCreateImage(allocator, &imgInfo, &allocInfo, &color2->img, &color2->alloc, nullptr);
                if(!result) {
                    LOGWITH("Failed to create image:", result);
                    color1->free(device, allocator);
                    delete color1;
                    delete color2;
                    return nullptr;
                }
                color2->view = createImageView(device, color2->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
                if(!color2->view) {
                    color1->free(device, allocator);
                    color2->free(device, allocator);
                    delete color1;
                    delete color2;
                    return nullptr;
                }
                if((int)type & 0b100){
                    color3 = new ImageSet;
                    result = vmaCreateImage(allocator, &imgInfo, &allocInfo, &color3->img, &color3->alloc, nullptr);
                    if(!result) {
                        LOGWITH("Failed to create image:", result);
                        color1->free(device, allocator);
                        color2->free(device, allocator);
                        delete color1;
                        delete color2;
                        return nullptr;
                    }
                    color3->view = createImageView(device, color3->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
                    if(!color3->view) {
                        vmaDestroyImage(allocator, color1->img, color1->alloc);
                        color1->free(device, allocator);
                        color2->free(device, allocator);
                        color3->free(device, allocator);
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
                if(color1) {color1->free(device, allocator); delete color1;}
                if(color2) {color2->free(device, allocator); delete color2;}
                if(color3) {color3->free(device, allocator); delete color3;}
                delete ds;
                return nullptr;
            }
            ds->view = createImageView(device, color3->img, VkImageViewType::VK_IMAGE_VIEW_TYPE_2D, imgInfo.format, 1, 1, VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT);
            if(!ds->view){
                if(color1) {color1->free(device, allocator); delete color1;}
                if(color2) {color2->free(device, allocator); delete color2;}
                if(color3) {color3->free(device, allocator); delete color3;}
                ds->free(device, allocator); delete ds;
                return nullptr;
            }
        }
        if(color1) images.insert(color1);
        if(color2) images.insert(color2);
        if(color3) images.insert(color3);
        if(ds) images.insert(ds);
        return renderTargets.emplace(name, new RenderTarget(width, height, color1, color2, color3, ds)).first->second;
    }

    void VkMachine::removeImageSet(VkMachine::ImageSet* set) {
        auto it = images.find(set);
        if(it != images.end()) {
            (*it)->free(device, allocator);
            delete *it;
            images.erase(it);
        }
    }

    VkShaderModule VkMachine::createShader(const uint32_t* spv, size_t size, const string16& name) {
        auto it = shaders.find(name);
        if(it != shaders.end()){
            return it->second;
        }
        VkShaderModule ret;
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

    VkMachine::RenderTarget::RenderTarget(unsigned width, unsigned height, VkMachine::ImageSet* color1, VkMachine::ImageSet* color2, VkMachine::ImageSet* color3, VkMachine::ImageSet* depthstencil)
    :width(width), height(height), color1(color1), color2(color2), color3(color3), depthstencil(depthstencil){
    }

    VkMachine::RenderTarget::~RenderTarget(){
        if(color1) singleton->removeImageSet(color1);
        if(color2) singleton->removeImageSet(color2);
        if(color3) singleton->removeImageSet(color3);
        if(depthstencil) singleton->removeImageSet(depthstencil);
        for(RenderPass* p: passes) { p->dangle = true; }
    }

    VkMachine::RenderPass::RenderPass(RenderTarget* target, VkShaderModule vs, VkShaderModule fs){
        VkAttachmentDescription attachments[4]{};
        VkAttachmentReference attachmentRefs[4]{};
        uint32_t colorAttachmentCount = 0;
        if(target->color1){
            attachments[0].format = VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
            attachments[0].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachmentRefs[0].attachment = 0;
            attachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachmentCount = 1;
            if(target->color2) {
                std::memcpy(attachments+1,attachments,sizeof(attachments[0]));
                attachmentRefs[1].attachment = 1;
                attachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorAttachmentCount = 2;
                if(target->color3){
                    std::memcpy(attachments+2,attachments,sizeof(attachments[0]));
                    attachmentRefs[2].attachment = 2;
                    attachmentRefs[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    colorAttachmentCount = 3;
                }
            }
        }
        if(target->depthstencil){
            attachments[colorAttachmentCount].format = VkFormat::VK_FORMAT_B8G8R8A8_SRGB;
            attachments[colorAttachmentCount].samples = VkSampleCountFlagBits::VK_SAMPLE_COUNT_1_BIT;
            attachments[colorAttachmentCount].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[colorAttachmentCount].storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 그림자맵에서야 필요하고 그 외에는 필요없음
            attachments[colorAttachmentCount].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[colorAttachmentCount].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[colorAttachmentCount].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[colorAttachmentCount].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachmentRefs[3].attachment = colorAttachmentCount;
            attachmentRefs[3].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.colorAttachmentCount = colorAttachmentCount;
        subpass.pColorAttachments = attachmentRefs;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if(target->depthstencil) subpass.pDepthStencilAttachment = &attachmentRefs[3];
        VkSubpassDependency dependencies[1] = {};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = 0;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = colorAttachmentCount + (target->depthstencil ? 1 : 0);
        rpInfo.pAttachments = attachments;
        rpInfo.dependencyCount = sizeof(dependencies) / sizeof(VkSubpassDependency);
        rpInfo.pDependencies = dependencies;
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpass;
        VkResult result;
        if((result = vkCreateRenderPass(singleton->device, &rpInfo, nullptr, &rp)) != VK_SUCCESS){
            LOGWITH("Failed to create renderpass:",result);
            dangle = true;
            return;
        }
        constructFB(target);
    }

    void VkMachine::RenderPass::constructFB(VkMachine::RenderTarget* target) {
        vkDestroyFramebuffer(singleton->device, fb, nullptr);
        fb = nullptr;
        VkImageView attachments[4]{};
        uint32_t count = 0;
        if(target->color1){
            attachments[0] = target->color1->view;
            count = 1;
            if(target->color2){
                attachments[1] = target->color2->view;
                count = 2;
                if(target->color3){
                    attachments[2] = target->color3->view;
                    count = 3;
                }
            }
        }
        if(target->depthstencil){
            attachments[count] = target->depthstencil->view;
            count++;
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.height = target->height;
        fbInfo.width = target->width;
        fbInfo.renderPass = rp;
        fbInfo.layers = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.attachmentCount = count;
        VkResult result;
        if((result = vkCreateFramebuffer(singleton->device, &fbInfo, nullptr, &fb)) != VK_SUCCESS){
            LOGWITH("Failed to create framebuffer:",result);
        }
    }

    void VkMachine::RenderPass::constructPipeline(VkShaderModule vs, VkShaderModule fs){ // 템플릿함수로 가야 함
        VkVertexInputBindingDescription vbind{};
        VkVertexInputAttributeDescription vattrs[1]{};

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
        inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};

        VkPipelineViewportStateCreateInfo viewportStateInfo{};
        viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateInfo.viewportCount = 1;
        viewportStateInfo.scissorCount = 1;
        viewportStateInfo.pViewports = &viewport;
        viewportStateInfo.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rtrInfo{};
        rtrInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rtrInfo.cullMode= VK_CULL_MODE_BACK_BIT;
        rtrInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rtrInfo.lineWidth = 1.0f;
        rtrInfo.polygonMode = VK_POLYGON_MODE_FILL;
        
        VkPipelineDepthStencilStateCreateInfo dsInfo{};
        dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        dsInfo.depthTestEnable = VK_TRUE;
        dsInfo.depthWriteEnable = VK_TRUE;
        //dsInfo.stencilTestEnable

        VkPipelineColorBlendAttachmentState blendInfo{};
        blendInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendInfo.colorBlendOp = VK_BLEND_OP_ADD;
        blendInfo.alphaBlendOp = VK_BLEND_OP_ADD;
        blendInfo.blendEnable = VK_TRUE;
        blendInfo.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        VkPipelineColorBlendStateCreateInfo fbbInfo{};
        fbbInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        //fbbInfo.attachmentCount = 1; // 색 첨부물 수 필요

        VkDynamicState dynStates[2] = {VkDynamicState::VK_DYNAMIC_STATE_VIEWPORT, VkDynamicState::VK_DYNAMIC_STATE_SCISSOR};
        
        VkPipelineDynamicStateCreateInfo dynInfo{};
        dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynInfo.pDynamicStates = dynStates;
        dynInfo.dynamicStateCount = sizeof(dynStates)/sizeof(VkDynamicState);
        
        VkPushConstantRange pushRange{};
        pushRange.offset = 0;
        pushRange.size = 128;
        pushRange.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT | VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pPushConstantRanges = &pushRange;
        layoutInfo.pushConstantRangeCount = 1;        

        VkGraphicsPipelineCreateInfo plInfo{};
        plInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        // plInfo.stageCount = sizeof(shaderStagesInfo) / sizeof(shaderStagesInfo[0]);
        // plInfo.pStages = shaderStagesInfo;
        // plInfo.pVertexInputState
        plInfo.pInputAssemblyState = &inputAssemblyInfo;
        plInfo.renderPass = rp;
        plInfo.subpass = 0;
        plInfo.pRasterizationState = &rtrInfo;
        //plInfo.pDynamicState
        //plInfo.pDepthStencilState = &dsInfo;
    }

    VkMachine::UniformBuffer::UniformBuffer(uint32_t length, uint32_t size, VkShaderStageFlags stages, uint32_t binding):length(length), isDynamic(length > 1) {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = binding;
        uboBinding.descriptorType = (length == 1) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1; // 이 카운트가 늘면 ubo 자체가 배열이 됨
        uboBinding.stageFlags = stages;

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
            LOGWITH("Failed to create descriptor set layout:",result);
            return;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = singleton->descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        if((result = vkAllocateDescriptorSets(singleton->device, &allocInfo, &dset)) != VK_SUCCESS){
            LOGWITH("Failed to allocate descriptor set:",result);
            return;
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
            LOGWITH("Failed to create buffer:", result);
            return;
        }

        if((result = vmaMapMemory(singleton->allocator, alloc, &mmap)) != VK_SUCCESS){
            LOGWITH("Failed to map memory:", result);
            return;
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
        vkUpdateDescriptorSets(singleton->device, 1, &wr, 0, nullptr);
        staged.resize(individual * length);
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
}