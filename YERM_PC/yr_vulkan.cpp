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

#define VMA_IMPLEMENTATION
#include "yr_vulkan.h"
#include "logger.hpp"
#include "yr_sys.h"

#include <vector>

namespace onart {

    /// @brief Vulkan 인스턴스를 생성합니다. 자동으로 호출됩니다.
    static VkInstance createInstance(Window*);
    /// @brief 사용할 Vulkan 물리 장치를 선택합니다. CPU 기반인 경우 경고를 표시하지만 선택에 실패하지는 않습니다.
    static VkPhysicalDevice findPhysicalDevice(VkInstance, VkSurfaceKHR, bool*, int*, int*);
    /// @brief 주어진 Vulkan 물리 장치에 대한 우선도를 매깁니다. 높을수록 좋게 취급합니다. 대부분의 경우 물리 장치는 하나일 것이므로 함수가 아주 중요하지는 않을 거라 생각됩니다.
    static uint64_t assessPhysicalDevice(VkPhysicalDevice);
    /// @brief 주어진 장치에 대한 가상 장치를 생성합니다.
    static VkDevice createDevice(VkPhysicalDevice, int, int);
    /// @brief 주어진 장치에 대한 메모리 관리자를 세팅합니다.
    static VmaAllocator createAllocator(VkInstance, VkPhysicalDevice, VkDevice);
    /// @brief 명령 풀을 생성합니다.
    static VkCommandPool createCommandPool(VkDevice, int qIndex);
    /// @brief 이미지로부터 뷰를 생성합니다.
    static VkImageView createImageView(VkDevice, VkImage, VkImageViewType, VkFormat, int, int, VkImageAspectFlagBits);

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
            vkDestroyInstance(instance, nullptr); instance = nullptr;
            return;
        }

        bool isCpu;
        int gq, pq;
        if(!(card = findPhysicalDevice(instance, surface.handle, &isCpu, &gq, &pq))) {
            LOGWITH("Couldn\'t find any appropriate graphics device");
            vkDestroySurfaceKHR(instance, surface.handle, nullptr); surface.handle = 0;
            vkDestroyInstance(instance, nullptr); instance = nullptr;
            return;
        }
        if(isCpu) LOGWITH("Warning: this device is CPU");
        // properties.limits.minMemorymapAlignment, minTexelBufferOffsetAlignment, minUniformBufferOffsetAlignment, minStorageBufferOffsetAlignment, optimalBufferCopyOffsetAlignment, optimalBufferCopyRowPitchAlignment를 저장
        
        checkSurfaceHandle();

        if(!(device = createDevice(card, gq, pq))) {
            vkDestroySurfaceKHR(instance, surface.handle, nullptr); surface.handle = 0;
            vkDestroyInstance(instance, nullptr); instance = nullptr;
            return;
        }

        vkGetDeviceQueue(device, gq, 0, &graphicsQueue);
        vkGetDeviceQueue(device, pq, 0, &presentQueue);


        if(!(allocator = createAllocator(instance, card, device))){
            vkDestroyDevice(device, nullptr); device = nullptr;
            vkDestroySurfaceKHR(instance, surface.handle, nullptr); surface.handle = 0;
            vkDestroyInstance(instance, nullptr); instance = nullptr;
            return;
        }

        if(!(gCommandPool = createCommandPool(device, gq))){
            vmaDestroyAllocator(allocator); allocator = nullptr;
            vkDestroyDevice(device, nullptr); device = nullptr;
            vkDestroySurfaceKHR(instance, surface.handle, nullptr); surface.handle = 0;
            vkDestroyInstance(instance, nullptr); instance = nullptr;
            return;
        }

        allocateCommandBuffers(sizeof(baseBuffer)/sizeof(baseBuffer[0]), true, baseBuffer);
        if(!baseBuffer[0]){
            vkDestroyCommandPool(device, gCommandPool, nullptr); gCommandPool = 0;
            vmaDestroyAllocator(allocator); allocator = nullptr;
            vkDestroyDevice(device, nullptr); device = nullptr;
            vkDestroySurfaceKHR(instance, surface.handle, nullptr); surface.handle = 0;
            vkDestroyInstance(instance, nullptr); instance = nullptr;
        }
        int w,h;
        window->getSize(&w,&h);
        createSwapchain(w, h, gq, pq);

        singleton = this;
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
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(card, surface.handle, &surface.caps);
        uint32_t count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(card, surface.handle, &count, nullptr);
        if(count == 0) LOGWITH("Fatal: no available surface format?");
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(card, surface.handle, &count, formats.data());
        surface.format = formats[0];
        for(VkSurfaceFormatKHR& form:formats){
            if(form.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR && form.format == VK_FORMAT_B8G8R8A8_SRGB){
                surface.format = form;
            }
        }
    }

    void VkMachine::createSwapchain(uint32_t width, uint32_t height, uint32_t gq, uint32_t pq){
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
        scInfo.oldSwapchain = swapchain.handle;
        uint32_t qfi[2] = {gq, pq};
        if(gq == pq){
            scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else{
            scInfo.queueFamilyIndexCount = 2;
            scInfo.pQueueFamilyIndices = qfi;
        }

        VkSwapchainKHR newSc;
        VkResult result;
        if((result = vkCreateSwapchainKHR(device, &scInfo, nullptr, &newSc))!=VK_SUCCESS){
            LOGWITH("Failed to create swapchain:",result);
            return;
        }
        for(VkImageView v: swapchain.imageView){
            vkDestroyImageView(device, v, nullptr);
        }
        swapchain.imageView.clear();
        vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
        swapchain.handle = newSc;
        swapchain.extent = scInfo.imageExtent;
        uint32_t count;
        vkGetSwapchainImagesKHR(device, swapchain.handle, &count, nullptr);
        std::vector<VkImage> images(count);
        swapchain.imageView.resize(count);
        vkGetSwapchainImagesKHR(device, swapchain.handle, &count, images.data());
        for(size_t i = 0;i < count;i++){
            swapchain.imageView[i] = createImageView(device,images[i],VK_IMAGE_VIEW_TYPE_2D,surface.format.format,1,0,VK_IMAGE_ASPECT_COLOR_BIT);
            if(swapchain.imageView[i] == 0) {
                return;
            }
        }
        
    }

    VkMachine::~VkMachine(){
        for(VkImageView v: swapchain.imageView){ vkDestroyImageView(device, v, nullptr); }
        vkDestroySwapchainKHR(device, swapchain.handle, nullptr);
        vmaDestroyAllocator(allocator);
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface.handle, nullptr);
        vkDestroyInstance(instance, nullptr);
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
        instInfo.enabledExtensionCount = windowExt.size();
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

    VkPhysicalDevice findPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, bool* isCpu, int* graphicsQueue, int* presentQueue) {
        uint32_t count;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        std::vector<VkPhysicalDevice> cards(count);
        vkEnumeratePhysicalDevices(instance, &count, cards.data());

        uint64_t maxScore = 0;
        VkPhysicalDevice goodCard = nullptr;
        uint32_t maxGq, maxPq;
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
                maxGq = gq;
                maxPq = pq;
            }
        }
        *isCpu = !(maxScore & (0b11ULL << 62));
        *graphicsQueue = maxGq;
        *presentQueue = maxPq;
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

        uint32_t count;
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

    VkImageView createImageView(VkDevice device, VkImage image, VkImageViewType type, VkFormat format, int levelCount, int layerCount, VkImageAspectFlagBits aspect){
        VkImageViewCreateInfo ivInfo{};
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
}