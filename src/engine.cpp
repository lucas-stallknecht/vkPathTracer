#define VK_USE_PLATFORM_WIN32_KHR
#define VMA_IMPLEMENTATION
#define GLFW_INCLUDE_VULKAN
#include "Engine.h"

#include <limits>
#include <algorithm>
#include <fstream>
#include <cassert>
#include <string>
#include <set>

#include "utils/images.h"
#include "utils/compatibility.h"
#include "utils/shaders.h"
#include "utils/create_infos.h"

namespace pt
{
    void Engine::init()
    {
        initWindow();
        initVulkan();
    }

    void Engine::initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Path Tracing Engine", nullptr, nullptr);
    }

    void Engine::initVulkan()
    {
        createInstance();
        pickPhysicalDevice();
        createLogicaldevice();
        createSwapchain();
        createVmaAllocator();
        createDrawImage();
        createDescriptors();
        createComputePipeline();
        createFrameData();
    }

    void Engine::createInstance()
    {
        if (ENABLE_VALIDATION_LAYERS && !pt_utils::checkValidationLayerSupport())
        {
            throw std::runtime_error("Validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_API_VERSION_1_3
        };

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);


        VkInstanceCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = glfwExtensionCount,
            .ppEnabledExtensionNames = glfwExtensions,
        };

        if (ENABLE_VALIDATION_LAYERS)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATIONS_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATIONS_LAYERS.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create instance !");
        }

        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    bool Engine::isDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        const bool discrete = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        const bool extensionsSupported = pt_utils::checkDeviceExtensionSupport(device);
        const bool hasNecessaryQueueFamilies = findQueueFamilies(device).isComplete();
        bool swapchainAdequate = false;
        if (extensionsSupported)
        {
            pt_utils::SwapchainSupportDetails swapchainSupport = pt_utils::querySwapchainSupport(device, surface_);
            swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
        }

        return discrete && hasNecessaryQueueFamilies && extensionsSupported && swapchainAdequate;
    }

    QueueFamilyIndices Engine::findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                indices.graphicsAndComputeFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            i++;
        }

        return indices;
    }

    void Engine::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

        for (const auto& device : devices)
        {
            if (isDeviceSuitable(device))
            {
                physicalDevice_ = device;
                break;
            }
        }

        if (physicalDevice_ == VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void Engine::createLogicaldevice()
    {
        VkPhysicalDeviceFeatures deviceFeatures = {};
        VkPhysicalDeviceSynchronization2Features sync2Feature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .synchronization2 = VK_TRUE
        };
        VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = &sync2Feature,
            .bufferDeviceAddress = VK_TRUE,
        };

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

        // The queues we want to create
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()
        };

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queueFamily,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority,
            };
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &bufferDeviceAddressFeature,
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size()),
            .ppEnabledExtensionNames = DEVICE_EXTENSIONS.data(),
            .pEnabledFeatures = &deviceFeatures,
        };


        if (ENABLE_VALIDATION_LAYERS)
        {
            deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATIONS_LAYERS.size());
            deviceCreateInfo.ppEnabledLayerNames = VALIDATIONS_LAYERS.data();
        }
        else
        {
            deviceCreateInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(device_, indices.graphicsAndComputeFamily.value(), 0, &computeQueue_);
        vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
    }

    VkSurfaceFormatKHR Engine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR Engine::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }

        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Engine::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        // the extent is set
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
        {
            return capabilities.currentExtent;
        }

        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }

    void Engine::createSwapchain()
    {
        pt_utils::SwapchainSupportDetails support = pt_utils::querySwapchainSupport(physicalDevice_, surface_);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
        VkExtent2D swapExtent = chooseSwapExtent(support.capabilities);

        VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface_,
            .minImageCount = FRAME_OVERLAP,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = swapExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .preTransform = support.capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };


        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
        uint32_t queueFamilyIndices[] = {indices.graphicsAndComputeFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsAndComputeFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create swap chain!");
        }

        uint32_t imageCount;
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
        if (imageCount == 0 || imageCount != FRAME_OVERLAP)
        {
            throw std::runtime_error("Wrong image count in the swap chain!");
        }

        swapchainImages_.resize(FRAME_OVERLAP);
        vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

        swapchainFormat_ = surfaceFormat.format;
        swapchainExtent_ = swapExtent;
    }

    void Engine::createVmaAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo = {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice_,
            .device = device_,
            .instance = instance_,
        };
        vmaCreateAllocator(&allocatorInfo, &allocator_);
    }

    void Engine::createDrawImage()
    {
        VkImageCreateInfo imgCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .extent = {swapchainExtent_.width, swapchainExtent_.height, 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        };

        VmaAllocationCreateInfo rimg_allocinfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        //allocate and create the image
        VmaAllocation imgAllocation;
        vmaCreateImage(allocator_, &imgCreateInfo, &rimg_allocinfo, &drawImage_.image, &imgAllocation, nullptr);

        VkImageViewCreateInfo imgViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = drawImage_.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        };
        imgViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imgViewCreateInfo.subresourceRange.levelCount = 1;
        imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imgViewCreateInfo.subresourceRange.layerCount = 1;


        if (vkCreateImageView(device_, &imgViewCreateInfo, nullptr, &drawImage_.imageView) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create intermediate image view!");
        }
        drawImage_.allocation = imgAllocation;
        drawImage_.imageExtent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        drawImage_.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    void Engine::createDescriptors()
    {
        pt_utils::DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawImageLayout_ = builder.build(device_, VK_SHADER_STAGE_COMPUTE_BIT);

        std::vector<pt_utils::DescriptorAllocator::PoolSizeRatio> sizes =
        {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
        };
        globalDescriptorAllocator.initPool(device_, 10, sizes);
        drawImageDescriptors_ = globalDescriptorAllocator.allocate(device_, drawImageLayout_);

        VkDescriptorImageInfo imgInfo = {
            .imageView = drawImage_.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        VkWriteDescriptorSet drawImageWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = drawImageDescriptors_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imgInfo,
        };

        vkUpdateDescriptorSets(device_, 1, &drawImageWrite, 0, nullptr);
    }

    void Engine::createComputePipeline()
    {
        auto compShaderCode = pt_utils::readFile("./shaders/simple.comp.spv");
        auto compModule = pt_utils::createShaderModule(device_, compShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compModule,
            .pName = "main",
        };

        VkPipelineLayoutCreateInfo computePipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &drawImageLayout_
        };

        if (vkCreatePipelineLayout(device_, &computePipelineLayoutInfo, nullptr, &computePipelineLayout_) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not create pipeline layout!");
        }

        VkComputePipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = computeShaderStageInfo,
            .layout = computePipelineLayout_,
        };


        if (vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline_) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create compute pipeline!");
        }

        vkDestroyShaderModule(device_, compModule, nullptr);
    }

    void Engine::createFrameData()
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice_);

        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value(),
        };
        VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };


        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            if (vkCreateCommandPool(device_, &poolInfo, nullptr, &frames_[i].commandPool) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create command pool!");
            }

            VkCommandBufferAllocateInfo allocInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = frames_[i].commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };


            if (vkAllocateCommandBuffers(device_, &allocInfo, &frames_[i].mainCommandBuffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate command buffers!");
            }
            if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frames_[i].swapSemaphore) != VK_SUCCESS ||
                vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frames_[i].renderSemaphore) != VK_SUCCESS ||
                vkCreateFence(device_, &fenceInfo, nullptr, &frames_[i].renderFence) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create semaphores and fence!");
            }
        }
    }


    void Engine::run()
    {
        while (!glfwWindowShouldClose(window_))
        {
            glfwPollEvents();
            draw();
        }
        vkDeviceWaitIdle(device_);
    }

    void Engine::draw()
    {
        vkWaitForFences(device_, 1, &getCurrentFrame().renderFence, true, 1000000000);
        vkResetFences(device_, 1, &getCurrentFrame().renderFence);

        unsigned int imageIndex;
        vkAcquireNextImageKHR(device_, swapchain_, 1000000000, getCurrentFrame().swapSemaphore, nullptr, &imageIndex);

        VkCommandBuffer cmd = getCurrentFrame().mainCommandBuffer;
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo cmdBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        if (vkBeginCommandBuffer(cmd, &cmdBeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not begin command buffer!");
        }

        pt_utils::transitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        // Draw the background
        VkClearColorValue clearValue = {0.2f, 0.2f, 0.2f, 1.0f};
        VkImageSubresourceRange clearRange = pt_utils::getImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkCmdClearColorImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

        // Draw the compute result on the intermediate image
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout_, 0, 1,
                                &drawImageDescriptors_, 0, nullptr);
        vkCmdDispatch(cmd, std::ceil(swapchainExtent_.width / 16.0), std::ceil(swapchainExtent_.height / 16.0), 1);

        // Copy the drawing result onto the swap chain image
        pt_utils::transitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        pt_utils::transitionImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        pt_utils::copyImageToImage(cmd, drawImage_.image, swapchainImages_[imageIndex], swapchainExtent_,
                                   swapchainExtent_);

        // Transition to present
        pt_utils::transitionImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not record command buffer!");
        }

        VkSemaphoreSubmitInfo waitInfo = pt_utils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapSemaphore);
        VkSemaphoreSubmitInfo signalInfo = pt_utils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);
        VkCommandBufferSubmitInfo cmdInfo = pt_utils::commandBufferSubmitInfo(cmd);
        VkSubmitInfo2 submitInfo = pt_utils::submitInfo(&cmdInfo, &signalInfo, &waitInfo);

        vkQueueSubmit2(computeQueue_, 1, &submitInfo, getCurrentFrame().renderFence);

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &getCurrentFrame().renderSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain_,
            .pImageIndices = &imageIndex,
        };

        if (vkQueuePresentKHR(presentQueue_, &presentInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Could not present");
        }
        frameNumber_++;
    }


    void Engine::cleanup()
    {
        for (auto frame : frames_)
        {
            vkFreeCommandBuffers(device_, frame.commandPool, 1, &frame.mainCommandBuffer);
            vkDestroyCommandPool(device_, frame.commandPool, nullptr);

            vkDestroySemaphore(device_, frame.renderSemaphore, nullptr);
            vkDestroySemaphore(device_, frame.swapSemaphore, nullptr);
            vkDestroyFence(device_, frame.renderFence, nullptr);
        }
        globalDescriptorAllocator.destroyPool(device_);
        vkDestroyPipeline(device_, computePipeline_, nullptr);
        vkDestroyPipelineLayout(device_, computePipelineLayout_, nullptr);
        vkDestroyDescriptorSetLayout(device_, drawImageLayout_, nullptr);
        vkDestroyImageView(device_, drawImage_.imageView, nullptr);
        vmaDestroyImage(allocator_, drawImage_.image, drawImage_.allocation);
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        vmaDestroyAllocator(allocator_);
        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
} // pt
