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

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "utils/images.h"
#include "utils/compatibility.h"
#include "utils/shaders.h"
#include "utils/create_infos.h"

namespace pt
{
    void Engine::init()
    {
        // Controls init
        keysArePressed = new bool[512]{false};
        camera_.position = glm::vec3(0.0, 0.0, 3.0);

        initWindow();
        initVulkan();
        initImgui();
    }

    void Engine::initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Path Tracing Engine", nullptr, nullptr);

        glfwSetWindowUserPointer(window_, this);
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        auto keyCallback = [](GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
            engine->keyCallback(window, key);
        };
        auto mouseCallback = [](GLFWwindow* window, double xpos, double ypos)
        {
            auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
            engine->mouseCallback(window, static_cast<float>(xpos), static_cast<float>(ypos));
        };
        auto mouseButtonCallback = [](GLFWwindow* window, int button, int action, int mods)
        {
            auto* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
            engine->mouseButtonCallback(window, button, action);
        };
        glfwSetCursorPosCallback(window_, mouseCallback);
        glfwSetKeyCallback(window_, keyCallback);
        glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    }

    void Engine::keyCallback(GLFWwindow* window, int key)
    {
        keysArePressed[key] = (glfwGetKey(window, key) == GLFW_PRESS);
    }

    void Engine::keyInput()
    {
        float deltaTime = io->DeltaTime;

        if (keysArePressed['W'] && focused)
        {
            camera_.moveForward(deltaTime);
        }
        if (keysArePressed['S'] && focused)
        {
            camera_.moveBackward(deltaTime);
        }
        if (keysArePressed['A'] && focused)
        {
            camera_.moveLeft(deltaTime);
        }
        if (keysArePressed['D'] && focused)
        {
            camera_.moveRight(deltaTime);
        }
        if (keysArePressed['Q'] && focused)
        {
            camera_.moveDown(deltaTime);
        }
        if (keysArePressed[' '] && focused)
        {
            camera_.moveUp(deltaTime);
        }
    }



    void Engine::mouseCallback(GLFWwindow* window, float xpos, float ypos)
    {
        if (!focused)
            return;

        // the mouse was not focused the frame before
        if (isFirstMouseMove)
        {
            lastMousePosition.x = xpos;
            lastMousePosition.y = ypos;
            isFirstMouseMove = false;
        }
        float xOffset = xpos - lastMousePosition.x;
        float yOffset = lastMousePosition.y - ypos;

        lastMousePosition.x = xpos;
        lastMousePosition.y = ypos;

        camera_.updateCamDirection(xOffset, yOffset);
    }


    void Engine::mouseButtonCallback(GLFWwindow* window, int button, int action)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        {
            focused = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
        {
            focused = false;
            isFirstMouseMove = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }


    void Engine::initVulkan()
    {
        createInstance();
        pickPhysicalDevice();
        createLogicaldevice();
        createSwapchain();
        createVmaAllocator();
        createDrawImage();
        createGlobalBuffers();
        createDescriptors();
        createComputePipeline();
        createCommands();
        createSyncs();
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

        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance_), "Failed to create instance !");
        VK_CHECK(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_), "failed to create window surface!");
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
        VkPhysicalDeviceDynamicRenderingFeatures dynRendFeature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = &bufferDeviceAddressFeature,
            .dynamicRendering = VK_TRUE,
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
            .pNext = &dynRendFeature,
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

        VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_),
                 "Failed to create logical device!");

        vkGetDeviceQueue(device_, indices.graphicsAndComputeFamily.value(), 0, &computeQueue_);
        vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
    }

    VkSurfaceFormatKHR Engine::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace ==
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

        VK_CHECK(vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapchain_), "Could not create swap chain!");

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

        swapchainImageViews_.resize(FRAME_OVERLAP);
        for (int i = 0; i < swapchainImages_.size(); i++)
        {
            VkImageViewCreateInfo imgViewCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchainImages_[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchainFormat_
            };
            imgViewCreateInfo.subresourceRange.baseMipLevel = 0;
            imgViewCreateInfo.subresourceRange.levelCount = 1;
            imgViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            imgViewCreateInfo.subresourceRange.layerCount = 1;
            imgViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

            VK_CHECK(vkCreateImageView(device_, &imgViewCreateInfo, nullptr, &swapchainImageViews_[i]),
                     "Could not create swap chain image views!");
        }
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

        VK_CHECK(vkCreateImageView(device_, &imgViewCreateInfo, nullptr, &drawImage_.imageView),
                 "Failed to create intermediate image view!");

        drawImage_.allocation = imgAllocation;
        drawImage_.imageExtent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        drawImage_.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    }

    void Engine::createGlobalBuffers()
    {
        cameraBuffer_ = createBuffer(
            sizeof(CameraUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
    }

    void Engine::createDescriptors()
    {
        pt_utils::DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        builder.addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        drawImageLayout_ = builder.build(device_, VK_SHADER_STAGE_COMPUTE_BIT);

        std::vector<pt_utils::DescriptorAllocator::PoolSizeRatio> sizes =
        {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
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
        VkDescriptorBufferInfo camBufferInfo = {
            .buffer = cameraBuffer_.buffer,
            .offset = 0,
            .range = cameraBuffer_.allocation->GetSize()
        };
        VkWriteDescriptorSet camBufferWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = drawImageDescriptors_,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &camBufferInfo,
        };
        VkWriteDescriptorSet setsWrite[2] = {drawImageWrite, camBufferWrite};

        vkUpdateDescriptorSets(device_, 2, &setsWrite[0], 0, nullptr);
    }

    void Engine::createComputePipeline()
    {
        auto compShaderCode = pt_utils::readFile("./shaders/camera_test.comp.spv");
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

        VK_CHECK(vkCreatePipelineLayout(device_, &computePipelineLayoutInfo, nullptr, &computePipelineLayout_),
                 "Could not create pipeline layout!");

        VkComputePipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = computeShaderStageInfo,
            .layout = computePipelineLayout_,
        };

        VK_CHECK(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline_),
                 "Failed to create compute pipeline!");
        vkDestroyShaderModule(device_, compModule, nullptr);
    }

    void Engine::createCommands()
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice_);

        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndices.graphicsAndComputeFamily.value(),
        };

        // Frame command pools and buffers
        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &frames_[i].commandPool),
                     "Failed to create frame command pool!");

            VkCommandBufferAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = frames_[i].commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };

            VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &frames_[i].mainCommandBuffer),
                     "Failed to allocate frame command buffer!");
        }

        // Immediate command pool and buffer
        VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &immediateHandles.commandPool),
                     "Failed to create immediate command pool!");

        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = immediateHandles.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &immediateHandles.commandBuffer),
                 "Failed to allocate immediate command buffer!");
    }

    void Engine::createSyncs()
    {
        VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        for (int i = 0; i < FRAME_OVERLAP; i++)
        {
            if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frames_[i].swapSemaphore) != VK_SUCCESS ||
                vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &frames_[i].renderSemaphore) != VK_SUCCESS ||
                vkCreateFence(device_, &fenceInfo, nullptr, &frames_[i].renderFence) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create semaphores and fence!");
            }
        }
        VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &immediateHandles.fence), "Could not create immedaite fence!");
    }

    void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
    {
        VK_CHECK(vkResetFences(device_, 1, &immediateHandles.fence), "");
        VK_CHECK(vkResetCommandBuffer(immediateHandles.commandBuffer, 0), "");

        VkCommandBuffer cmd = immediateHandles.commandBuffer;

        VkCommandBufferBeginInfo cmdBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo), "");
        function(cmd);
        VK_CHECK(vkEndCommandBuffer(cmd), "");

        VkCommandBufferSubmitInfo cmdinfo = pt_utils::commandBufferSubmitInfo(cmd);
        VkSubmitInfo2 submit = pt_utils::submitInfo(&cmdinfo, nullptr, nullptr);

        // submit command buffer to the queue and execute it.
        // the fence will now block until the graphic commands finish execution
        VK_CHECK(vkQueueSubmit2(computeQueue_, 1, &submit, immediateHandles.fence), "");
        VK_CHECK(vkWaitForFences(device_, 1, &immediateHandles.fence, true, 9999999999), "");
    }


    AllocatedBuffer Engine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
    {
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = allocSize,
            .usage = usage,
        };

        VmaAllocationCreateInfo vmaallocInfo = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = memoryUsage,
        };

        AllocatedBuffer newBuffer;
        VK_CHECK(vmaCreateBuffer(allocator_, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
                     &newBuffer.info), "Could not create buffer!");

        return newBuffer;
    }


    void Engine::destroyBuffer(const AllocatedBuffer& buffer)
    {
        vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
    }


    void Engine::updateGlobalBuffers()
    {
        void* data = cameraBuffer_.allocation->GetMappedData();
        CameraUniform cam = {
            .position = camera_.position,
            .invView = glm::inverse(camera_.viewMatrix),
            .invProj = glm::inverse(camera_.projMatrix)
        };
        memcpy(data, &cam, sizeof(CameraUniform));
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

        // Transition to draw gui onto swap chain image
        pt_utils::transitionImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_GENERAL);
        drawImgui(cmd, swapchainImageViews_[imageIndex]);

        // Transition to present
        pt_utils::transitionImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_CHECK(vkEndCommandBuffer(cmd), "Could not record command buffer!");

        VkSemaphoreSubmitInfo waitInfo = pt_utils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapSemaphore);
        VkSemaphoreSubmitInfo signalInfo = pt_utils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);
        VkCommandBufferSubmitInfo cmdInfo = pt_utils::commandBufferSubmitInfo(cmd);
        VkSubmitInfo2 submitInfo = pt_utils::submitInfo(&cmdInfo, &signalInfo, &waitInfo);

        VK_CHECK(vkQueueSubmit2(computeQueue_, 1, &submitInfo, getCurrentFrame().renderFence),
                 "Could not submit queue!");

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &getCurrentFrame().renderSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain_,
            .pImageIndices = &imageIndex,
        };

        VK_CHECK(vkQueuePresentKHR(presentQueue_, &presentInfo), "Could not present");
        frameNumber_++;
    }


    void Engine::initImgui()
    {
        // Create a pool for imgui
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
        };
        VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets = 1000,
            .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
            .pPoolSizes = poolSizes,
        };

        VkDescriptorPool imguiPool;
        VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &imguiPool), "Failed to create ImGui pool!");

        // Intialize the library
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = &ImGui::GetIO();
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(window_, true);
        ImGui_ImplVulkan_InitInfo imguiVulkInitInfo = {
            .Instance = instance_,
            .PhysicalDevice = physicalDevice_,
            .Device = device_,
            .Queue = computeQueue_,
            .DescriptorPool = imguiPool,
            .MinImageCount = 3,
            .ImageCount = 3,
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .UseDynamicRendering = true
        };
        imguiVulkInitInfo.PipelineRenderingCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &swapchainFormat_
        };
        ImGui_ImplVulkan_Init(&imguiVulkInitInfo);
        ImGui_ImplVulkan_CreateFontsTexture();
    }

    void Engine::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
    {
        VkRenderingAttachmentInfo colorAttachment = pt_utils::attachmentInfo(
            targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo = pt_utils::renderingInfo(swapchainExtent_, &colorAttachment, nullptr);

        vkCmdBeginRendering(cmd, &renderInfo);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        vkCmdEndRendering(cmd);
    }


    void Engine::run()
    {
        while (!glfwWindowShouldClose(window_))
        {
            keyInput();
            glfwPollEvents();
            camera_.updateMatrix();

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::ShowDemoWindow();
            ImGui::Render();

            updateGlobalBuffers();

            draw();
        }
        vkDeviceWaitIdle(device_);
    }


    void Engine::cleanup()
    {
        ImGui_ImplVulkan_Shutdown();
        // vkDestroyDescriptorPool(device_, imguiPool, nullptr);
        destroyBuffer(cameraBuffer_);
        vkFreeCommandBuffers(device_, immediateHandles.commandPool, 1, &immediateHandles.commandBuffer);
        vkDestroyCommandPool(device_, immediateHandles.commandPool, nullptr);
        vkDestroyFence(device_, immediateHandles.fence, nullptr);
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
        for (auto imageView : swapchainImageViews_)
        {
            vkDestroyImageView(device_, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        vmaDestroyAllocator(allocator_);
        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
} // pt
