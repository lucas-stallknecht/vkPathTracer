#define VK_USE_PLATFORM_WIN32_KHR
#define VMA_IMPLEMENTATION
#define GLFW_INCLUDE_VULKAN
#include "renderer.h"

#include <limits>
#include <algorithm>
#include <fstream>
#include <cassert>
#include <string>
#include <set>
#include <array>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "utils/images.h"
#include "utils/shaders.h"
#include "utils/compatibility.h"
#include "utils/vk_infos.h"
#include "path_tracing/trace_mesh.h"

namespace renderer
{
    void Renderer::init(GLFWwindow* window)
    {
        initVulkan(window);
        initImguiBackend(window);
    }

    void Renderer::initVulkan(GLFWwindow* window)
    {
        createInstance(window);
        pickPhysicalDevice();
        createLogicaldevice();
        createSwapchain(window);
        createVmaAllocator();
        createDrawImage();
        createCommands();
        createSyncs();
        createGlobalBuffer();
        createGlobalDescriptors();
        createPathTracingDescriptors();
        createPathTracingPipeline();
        createPostProcessingDescriptors();
        createPostProcessingPipeline();
    }

    void Renderer::createInstance(GLFWwindow* window)
    {
#ifdef ENABLE_VALIDATION_LAYERS
        if (!vk_utils::checkValidationLayerSupport())
        {
            throw std::runtime_error("Validation layers requested, but not available!");
        }
#endif


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

#ifdef ENABLE_VALIDATION_LAYERS
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATIONS_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATIONS_LAYERS.data();
#else
        createInfo.enabledLayerCount = 0;
#endif


        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance_), "Failed to create instance !");
        VK_CHECK(glfwCreateWindowSurface(instance_, window, nullptr, &surface_), "failed to create window surface!");

        deletionQueue_.push_function([=]()
        {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            vkDestroyInstance(instance_, nullptr);
        });
    }

    bool Renderer::isDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);

        const bool discrete = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        const bool extensionsSupported = vk_utils::checkDeviceExtensionSupport(device);
        findQueueFamily(device);
        const bool hasNecessaryQueueFamilies = queueFamily_.has_value();
        bool swapchainAdequate = false;
        if (extensionsSupported)
        {
            vk_utils::SwapchainSupportDetails swapchainSupport = vk_utils::querySwapchainSupport(
                device, surface_);
            swapchainAdequate = !swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty();
        }

        return discrete && hasNecessaryQueueFamilies && extensionsSupported && swapchainAdequate;
    }

    void Renderer::findQueueFamily(VkPhysicalDevice device)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT &&
                presentSupport)
            {
                queueFamily_ = i;
            }
            i++;
        }
        if (!queueFamily_.has_value())
            throw std::runtime_error("Could not find a queue (grahics, compute and present)!");
    }

    void Renderer::pickPhysicalDevice()
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

    void Renderer::createLogicaldevice()
    {
        VkPhysicalDeviceFeatures deviceFeatures = {};
        VkPhysicalDeviceSynchronization2Features sync2Feature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
            .synchronization2 = VK_TRUE
        };
        VkPhysicalDeviceDynamicRenderingFeatures dynRendFeature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .pNext = &sync2Feature,
            .dynamicRendering = VK_TRUE,
        };
        VkPhysicalDeviceVulkan12Features features12 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &dynRendFeature,
            .descriptorIndexing = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE,
            .bufferDeviceAddress = VK_TRUE,
        };

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamily_.value(),
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &features12,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size()),
            .ppEnabledExtensionNames = DEVICE_EXTENSIONS.data(),
            .pEnabledFeatures = &deviceFeatures,
        };


#ifdef ENABLE_VALIDATION_LAYERS
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATIONS_LAYERS.size());
        deviceCreateInfo.ppEnabledLayerNames = VALIDATIONS_LAYERS.data();
#else
        deviceCreateInfo.enabledLayerCount = 0;
#endif


        VK_CHECK(vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_),
                 "Failed to create logical device!");

        vkGetDeviceQueue(device_, queueFamily_.value(), 0, &queue_);

        deletionQueue_.push_function([=]()
        {
            vkDestroyDevice(device_, nullptr);
        });
    }

    VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace ==
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
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

    VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
    {
        // the extent is set
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
        {
            return capabilities.currentExtent;
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

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

    void Renderer::createSwapchain(GLFWwindow* window)
    {
        vk_utils::SwapchainSupportDetails support = vk_utils::querySwapchainSupport(
            physicalDevice_, surface_);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
        VkExtent2D swapExtent = chooseSwapExtent(support.capabilities, window);

        VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface_,
            .minImageCount = FRAME_OVERLAP,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = swapExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &queueFamily_.value(),
            .preTransform = support.capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };

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

        deletionQueue_.push_function([=]()
        {
            for (auto imageView : swapchainImageViews_)
            {
                vkDestroyImageView(device_, imageView, nullptr);
            }
            vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        });
    }

    void Renderer::createVmaAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo = {
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice_,
            .device = device_,
            .instance = instance_,
        };
        vmaCreateAllocator(&allocatorInfo, &allocator_);
        deletionQueue_.push_function([=]()
        {
            vmaDestroyAllocator(allocator_);
        });
    }

    void Renderer::createDrawImage()
    {
        drawImage_ = createImage(
            {swapchainExtent_.width, swapchainExtent_.height, 1},
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false
        );

        postProcessImage_ = createImage(
            {swapchainExtent_.width, swapchainExtent_.height, 1},
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, false
        );
        deletionQueue_.push_function([=]()
        {
            destroyImage(drawImage_);
            destroyImage(postProcessImage_);
        });
    }

    void Renderer::createGlobalBuffer()
    {
        globalBuffer_ = createBuffer(
            sizeof(CameraUniform),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );

        VkSamplerCreateInfo sampl = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR
        };
        vkCreateSampler(device_, &sampl, nullptr, &defaultLinearSampler_);

        deletionQueue_.push_function([=]()
        {
            destroyBuffer(globalBuffer_);
            vkDestroySampler(device_, defaultLinearSampler_, nullptr);
        });
    }

    void Renderer::uploadSkybox(const std::string& skyboxDirectory)
    {
        if (skybox_.image != VK_NULL_HANDLE)
        {
            destroyImage(skybox_);
        }
        std::array<std::string, 6> faces{
            skyboxDirectory + "/px.png", // right
            skyboxDirectory + "/nx.png", // left
            skyboxDirectory + "/py.png", // top
            skyboxDirectory + "/ny.png", // bottom
            skyboxDirectory + "/pz.png", // front
            skyboxDirectory + "/nz.png" // back
        };

        VkExtent3D texSize = {1, 1, 1};
        std::array<stbi_uc*, 6> textureData{};
        for (int i = 0; i < faces.size(); i++)
        {
            textureData[i] = vk_utils::loadTextureData(faces[i], texSize);
        }
        skybox_ = createCubemap(textureData, texSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
        for (int i = 0; i < faces.size(); i++)
        {
            vk_utils::freeImageData(textureData[i]);
        }

        VkDescriptorImageInfo skyboxInfo = {
            .sampler = defaultLinearSampler_,
            .imageView = skybox_.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkWriteDescriptorSet imgWrtie = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = globalDescriptors_,
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &skyboxInfo
        };
        vkUpdateDescriptorSets(device_, 1, &imgWrtie, 0, nullptr);
        deletionQueue_.push_function([=]()
        {
            destroyImage(skybox_);
        });
    }

    void Renderer::createGlobalDescriptors()
    {
        // Global descriptors
        vk_utils::DescriptorLayoutBuilder globalBuilder;
        globalBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        globalBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        VkDescriptorBindingFlags bindingFlgas[2] = {0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = 2,
            .pBindingFlags = &bindingFlgas[0]
        };
        globalDescLayout_ = globalBuilder.build(device_, VK_SHADER_STAGE_COMPUTE_BIT, &bindingFlagsInfo);

        std::vector<vk_utils::DescriptorAllocator::PoolSizeRatio> sizes =
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20},
        };
        globalDescriptorAllocator_.initPool(device_, 10, sizes);
        globalDescriptors_ = globalDescriptorAllocator_.allocate(device_, globalDescLayout_);

        VkDescriptorBufferInfo globalBufferInfo = {
            .buffer = globalBuffer_.buffer,
            .offset = 0,
            .range = globalBuffer_.allocation->GetSize()
        };
        VkWriteDescriptorSet globalBufferWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = globalDescriptors_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &globalBufferInfo,
        };

        vkUpdateDescriptorSets(device_, 1, &globalBufferWrite, 0, nullptr);
        deletionQueue_.push_function([=]()
        {
            vkDestroyDescriptorSetLayout(device_, globalDescLayout_, nullptr);
        });
    }

    void Renderer::createPathTracingDescriptors()
    {
        vk_utils::DescriptorLayoutBuilder ptBuilder;
        ptBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        ptBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20);

        VkDescriptorBindingFlags bindingFlgas[2] = {
            0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
        };
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = 2,
            .pBindingFlags = bindingFlgas
        };
        ptDescLayout_ = ptBuilder.build(device_, VK_SHADER_STAGE_COMPUTE_BIT, &bindingFlagsInfo);

        const uint32_t maxTextureDescCount = 20;
        VkDescriptorSetVariableDescriptorCountAllocateInfo texturesVariableInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &maxTextureDescCount
        };

        ptDescriptors_ = globalDescriptorAllocator_.allocate(device_, ptDescLayout_, &texturesVariableInfo);

        VkDescriptorImageInfo imgInfo = {
            .imageView = drawImage_.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        VkWriteDescriptorSet drawImageWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ptDescriptors_,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &imgInfo,
        };

        vkUpdateDescriptorSets(device_, 1, &drawImageWrite, 0, nullptr);
        deletionQueue_.push_function([=]()
        {
            vkDestroyDescriptorSetLayout(device_, ptDescLayout_, nullptr);
            globalDescriptorAllocator_.destroyPool(device_);
        });
    }

    void Renderer::createPathTracingPipeline()
    {
        auto compShaderCode = vk_utils::readFile("./shaders/path_tracing.comp.spv");
        auto compModule = vk_utils::createShaderModule(device_, compShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compModule,
            .pName = "main",
        };
        std::vector<VkDescriptorSetLayout> setLayouts = {globalDescLayout_, ptDescLayout_};

        VkPushConstantRange constantRange = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(path_tracing::PushConstants)
        };

        VkPipelineLayoutCreateInfo computePipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = setLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &constantRange
        };

        VK_CHECK(vkCreatePipelineLayout(device_, &computePipelineLayoutInfo, nullptr, &ptPipelineLayout_),
                 "Could not create pipeline layout!");

        VkComputePipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = computeShaderStageInfo,
            .layout = ptPipelineLayout_,
        };

        VK_CHECK(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &ptPipeline_),
                 "Failed to create compute pipeline!");
        vkDestroyShaderModule(device_, compModule, nullptr);

        deletionQueue_.push_function([=]()
        {
            vkDestroyPipeline(device_, ptPipeline_, nullptr);
            vkDestroyPipelineLayout(device_, ptPipelineLayout_, nullptr);
        });
    }

    void Renderer::createPostProcessingDescriptors()
    {
        vk_utils::DescriptorLayoutBuilder ppBuilder;
        ppBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        ppBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        ppDescLayout_ = ppBuilder.build(device_, VK_SHADER_STAGE_COMPUTE_BIT);
        ppDescriptors_ = globalDescriptorAllocator_.allocate(device_, ppDescLayout_);

        VkDescriptorImageInfo imgInfo = {
            .imageView = drawImage_.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        VkDescriptorImageInfo imgInfo2 = {
            .imageView = postProcessImage_.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };
        VkDescriptorImageInfo descs[2] = {imgInfo, imgInfo2};
        VkWriteDescriptorSet drawImageWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ppDescriptors_,
            .dstBinding = 0,
            .descriptorCount = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = descs,
        };

        vkUpdateDescriptorSets(device_, 1, &drawImageWrite, 0, nullptr);
        deletionQueue_.push_function([=]()
        {
            vkDestroyDescriptorSetLayout(device_, ppDescLayout_, nullptr);
        });
    }

    void Renderer::createPostProcessingPipeline()
    {
        auto compShaderCode = vk_utils::readFile("./shaders/tonemapping.comp.spv");
        auto compModule = vk_utils::createShaderModule(device_, compShaderCode);

        VkPipelineShaderStageCreateInfo computeShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = compModule,
            .pName = "main",
        };
        std::vector<VkDescriptorSetLayout> setLayouts = {ppDescLayout_};

        VkPushConstantRange constantRange = {
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .offset = 0,
            .size = sizeof(PostProcessingPushConstants)
        };

        VkPipelineLayoutCreateInfo computePipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = setLayouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &constantRange
        };

        VK_CHECK(vkCreatePipelineLayout(device_, &computePipelineLayoutInfo, nullptr, &ppPipelineLayout_),
                 "Could not create post processing pipeline layout!");

        VkComputePipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = computeShaderStageInfo,
            .layout = ppPipelineLayout_,
        };

        VK_CHECK(vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &ppPipeline_),
                 "Failed to create post processing compute pipeline!");
        vkDestroyShaderModule(device_, compModule, nullptr);

        deletionQueue_.push_function([=]()
        {
            vkDestroyPipeline(device_, ppPipeline_, nullptr);
            vkDestroyPipelineLayout(device_, ppPipelineLayout_, nullptr);
        });
    }

    void Renderer::createCommands()
    {
        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamily_.value(),
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

            frames_[i].deletionQueue.push_function([=]()
            {
                vkFreeCommandBuffers(device_, frames_[i].commandPool, 1, &frames_[i].mainCommandBuffer);
                vkDestroyCommandPool(device_, frames_[i].commandPool, nullptr);
            });
        }

        // Immediate command pool and buffer
        VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &immediateHandles_.commandPool),
                 "Failed to create immediate command pool!");

        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = immediateHandles_.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &immediateHandles_.commandBuffer),
                 "Failed to allocate immediate command buffer!");
        deletionQueue_.push_function([=]()
        {
            vkFreeCommandBuffers(device_, immediateHandles_.commandPool, 1, &immediateHandles_.commandBuffer);
            vkDestroyCommandPool(device_, immediateHandles_.commandPool, nullptr);
        });
    }

    void Renderer::createSyncs()
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

            frames_[i].deletionQueue.push_function([=]()
            {
                vkDestroySemaphore(device_, frames_[i].renderSemaphore, nullptr);
                vkDestroySemaphore(device_, frames_[i].swapSemaphore, nullptr);
                vkDestroyFence(device_, frames_[i].renderFence, nullptr);
            });
        }
        VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &immediateHandles_.fence),
                 "Could not create immedaite fence!");
        deletionQueue_.push_function([=]()
        {
            vkDestroyFence(device_, immediateHandles_.fence, nullptr);
        });
    }

    void Renderer::initImguiBackend(GLFWwindow* wwindow)
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

        ImGui_ImplGlfw_InitForVulkan(wwindow, true);
        ImGui_ImplVulkan_InitInfo imguiVulkInitInfo = {
            .Instance = instance_,
            .PhysicalDevice = physicalDevice_,
            .Device = device_,
            .Queue = queue_,
            .DescriptorPool = imguiPool,
            .MinImageCount = 2,
            .ImageCount = 2,
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

        deletionQueue_.push_function([=]()
        {
            vkDestroyDescriptorPool(device_, imguiPool, nullptr);
        });
    }


    void Renderer::render(const core::Camera& camera)
    {
        updateGlobalBuffer(camera);
        draw();
    }

    void Renderer::updateGlobalBuffer(const core::Camera& camera) const
    {
        void* data = globalBuffer_.allocation->GetMappedData();
        CameraUniform cam = {
            .position = camera.position,
            .invView = glm::inverse(camera.viewMatrix),
            .invProj = glm::inverse(camera.projMatrix)
        };
        memcpy(data, &cam, sizeof(CameraUniform));
    }

    void Renderer::resetAccumulation()
    {
        ptPushConstants_.frame = 0;
    }

    void Renderer::uploadPathTracingScene(const std::vector<path_tracing::Mesh>& scene)
    {
        // Aggregate scene data to build global buffers
        std::vector<core::Vertex> vertices;
        std::vector<path_tracing::Triangle> triangles;
        std::vector<path_tracing::BVHNode> nodes;
        std::vector<path_tracing::GPUMaterial> materials;
        std::vector<path_tracing::MeshInfo> meshInfos;

        std::unordered_map<std::string, int> texturePaths;
        int currentTexIndex = -1;

        path_tracing::MeshInfo offsets{};

        for (const auto& mesh : scene)
        {
            vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
            triangles.insert(triangles.end(), mesh.triangles.begin(), mesh.triangles.end());
            nodes.insert(nodes.end(), mesh.nodes.begin(), mesh.nodes.end());

            path_tracing::GPUMaterial m = {
                .baseCol = mesh.material.color,
                .baseColMapIndex = path_tracing::handleMapProperty(mesh.material.colorMap, texturePaths,
                                                                   currentTexIndex),
                .emissiveStrength = mesh.material.emissiveStrength,
                .roughness = mesh.material.roughness,
                .roughnessMapIndex = path_tracing::handleMapProperty(mesh.material.roughnessMap, texturePaths,
                                                                     currentTexIndex),
                .metallic = mesh.material.metallic,
                .metallicMapIndex = path_tracing::handleMapProperty(mesh.material.metallicMap, texturePaths,
                                                                    currentTexIndex),
            };
            materials.push_back(m);
            meshInfos.push_back(offsets);

            offsets.vertexOffset += mesh.vertices.size();
            offsets.triangleOffset += mesh.triangles.size();
            offsets.nodeOffset += mesh.nodes.size();
            offsets.materialIndex++;
        }

        // Calculate buffer sizes
        const size_t vertexBufferSize = vertices.size() * sizeof(core::Vertex);
        const size_t triangleBufferSize = triangles.size() * sizeof(path_tracing::Triangle);
        const size_t nodeBufferSize = nodes.size() * sizeof(path_tracing::BVHNode);
        const size_t materialBufferSize = materials.size() * sizeof(path_tracing::GPUMaterial);
        const size_t meshInfoBufferSize = meshInfos.size() * sizeof(path_tracing::MeshInfo);

        path_tracing::SceneBuffers newScene;

        // Helper to create GPU buffer and retrieve address
        auto createGPUBuffer = [&](size_t size, void* data, AllocatedBuffer& buffer, VkDeviceAddress& address)
        {
            buffer = createBuffer(
                size,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VMA_MEMORY_USAGE_GPU_ONLY);

            VkBufferDeviceAddressInfo deviceAddressInfo = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buffer.buffer
            };
            address = vkGetBufferDeviceAddress(device_, &deviceAddressInfo);
            return size;
        };

        AllocatedBuffer staging = createBuffer(
            vertexBufferSize + triangleBufferSize + nodeBufferSize + materialBufferSize + meshInfoBufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY);
        void* stagingData = staging.allocation->GetMappedData();

        struct BufferData
        {
            void* data;
            size_t size;
            AllocatedBuffer* dstBuffer;
            VkDeviceAddress* dstBufferAddress;
        };

        const std::vector<BufferData> bufferData = {
            {vertices.data(), vertexBufferSize, &newScene.vertexBuffer, &newScene.vertexBufferAddress},
            {triangles.data(), triangleBufferSize, &newScene.triangleBuffer, &newScene.triangleBufferAddress},
            {nodes.data(), nodeBufferSize, &newScene.nodeBuffer, &newScene.nodeBufferAddress},
            {materials.data(), materialBufferSize, &newScene.materialBuffer, &newScene.materialBufferAddress},
            {meshInfos.data(), meshInfoBufferSize, &newScene.meshInfoBuffer, &newScene.meshInfoBufferAddress},
        };


        // Automatically create the GPU buffers and copy data from the vectors to staging buffer
        size_t currentOffset = 0;
        for (auto b : bufferData)
        {
            memcpy((char*)stagingData + currentOffset, b.data, b.size);
            currentOffset += createGPUBuffer(b.size, b.data, *b.dstBuffer, *b.dstBufferAddress);
        }

        // CPU to GPU copy
        immediateSubmit([&](VkCommandBuffer cmd)
        {
            size_t currSrcOffset = 0;
            for (const auto& b : bufferData)
            {
                VkBufferCopy copy{};
                copy.srcOffset = currSrcOffset;
                copy.dstOffset = 0;
                copy.size = b.size;

                vkCmdCopyBuffer(cmd, staging.buffer, b.dstBuffer->buffer, 1, &copy);
                currSrcOffset += b.size;
            }
        });
        ptScene_ = newScene;
        ptPushConstants_ = {
            newScene.vertexBufferAddress,
            newScene.triangleBufferAddress,
            newScene.nodeBufferAddress,
            newScene.materialBufferAddress,
            newScene.meshInfoBufferAddress,
            static_cast<uint32_t>(scene.size()),
            0
        };
        destroyBuffer(staging);

        std::vector<std::string> texVector;
        texVector.resize(texturePaths.size());
        for (auto& tex : texturePaths)
            texVector[tex.second] = (tex.first);

        if (texVector.size() > 0)
        {
            uploadTextures(texVector);
        }

        deletionQueue_.push_function([&]()
        {
            destroyBuffer(ptScene_.vertexBuffer);
            destroyBuffer(ptScene_.triangleBuffer);
            destroyBuffer(ptScene_.nodeBuffer);
            destroyBuffer(ptScene_.materialBuffer);
            destroyBuffer(ptScene_.meshInfoBuffer);
        });
    }

    void Renderer::uploadTextures(const std::vector<std::string>& texturePaths)
    {
        std::vector<VkDescriptorImageInfo> texturesInfo;
        for (auto& path : texturePaths)
        {
            VkExtent3D texSize = {1, 1, 1};
            stbi_uc* data = vk_utils::loadTextureData(path, texSize);

            textures_.push_back(createImage(data, texSize, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT,
                                            false));

            texturesInfo.emplace_back(defaultLinearSampler_, textures_.back().imageView, VK_IMAGE_LAYOUT_GENERAL);
        }
        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ptDescriptors_,
            .dstBinding = 1,
            .descriptorCount = static_cast<uint32_t>(texturesInfo.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = texturesInfo.data()
        };
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);

        deletionQueue_.push_function([&]()
        {
            for (auto& img : textures_)
                destroyImage(img);
        });
    }


    AllocatedBuffer Renderer::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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

    void Renderer::destroyBuffer(const AllocatedBuffer& buffer)
    {
        vmaDestroyBuffer(allocator_, buffer.buffer, buffer.allocation);
    }

    AllocatedImage Renderer::createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
    {
        AllocatedImage newImage;
        newImage.imageFormat = format;
        newImage.imageExtent = size;

        VkImageCreateInfo imgInfo = vk_utils::imageCreateInfo(format, usage, size);
        if (mipmapped)
        {
            imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
        }

        // always allocate images on dedicated GPU memory
        VmaAllocationCreateInfo allocinfo = {};
        allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // allocate and create the image
        VK_CHECK(vmaCreateImage(allocator_, &imgInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr),
                 "Could not create image!");

        // if the format is a depth format, we will need to have it use the correct
        // aspect flag
        VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
        if (format == VK_FORMAT_D32_SFLOAT)
        {
            aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        // build a image-view for the image
        VkImageViewCreateInfo viewInfo = vk_utils::imageViewCreateInfo(format, newImage.image, aspectFlag);
        viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

        VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &newImage.imageView), "Could not create image view!");

        return newImage;
    }

    AllocatedImage Renderer::createImage(stbi_uc* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                         bool mipmapped)
    {
        size_t dataSize = size.depth * size.width * size.height * 4;
        AllocatedBuffer uploadbuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

        memcpy(uploadbuffer.info.pMappedData, data, dataSize);

        AllocatedImage newImage = createImage(size, format,
                                              usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                              mipmapped);

        immediateSubmit([&](VkCommandBuffer cmd)
        {
            vk_utils::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageExtent = size
            };
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;

            // copy the buffer into the image
            vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &copyRegion);

            vk_utils::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_GENERAL);
        });

        destroyBuffer(uploadbuffer);

        return newImage;
    }

    AllocatedImage Renderer::createCubemap(std::array<stbi_uc*, 6> data, VkExtent3D size, VkFormat format,
                                           VkImageUsageFlags usage)
    {
        AllocatedImage newImage;
        newImage.imageFormat = format;
        newImage.imageExtent = size;

        VkImageCreateInfo imgInfo = vk_utils::imageCreateInfo(format,
                                                              usage |
                                                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                                              , size, true);

        VmaAllocationCreateInfo allocinfo = {};
        allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        allocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vmaCreateImage(allocator_, &imgInfo, &allocinfo, &newImage.image, &newImage.allocation, nullptr),
                 "Could not create image!");

        VkImageViewCreateInfo viewInfo = vk_utils::imageViewCreateInfo(
            format, newImage.image, VK_IMAGE_ASPECT_COLOR_BIT, true);
        VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &newImage.imageView), "Could not create image view!");


        const size_t dataSize = size.width * size.height * 4 * 6;
        AllocatedBuffer uploadbuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

        const size_t layerSize = dataSize / 6;
        for (int i = 0; i < 6; ++i)
        {
            memcpy(static_cast<char*>(uploadbuffer.info.pMappedData) + i * layerSize, data[i], layerSize);
        }


        immediateSubmit([&](VkCommandBuffer cmd)
        {
            vk_utils::transitionCubemap(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion = {
                .bufferOffset = 0,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageExtent = size
            };
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 6;

            // copy the buffer into the image
            vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &copyRegion);

            vk_utils::transitionCubemap(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                        VK_IMAGE_LAYOUT_GENERAL);
        });

        destroyBuffer(uploadbuffer);

        return newImage;
    }

    void Renderer::destroyImage(const AllocatedImage& image)
    {
        vkDestroyImageView(device_, image.imageView, nullptr);
        vmaDestroyImage(allocator_, image.image, image.allocation);
    }


    void Renderer::draw()
    {
        vkWaitForFences(device_, 1, &getCurrentFrame().renderFence, true, 1000000000);
        vkResetFences(device_, 1, &getCurrentFrame().renderFence);

        unsigned int imageIndex;
        vkAcquireNextImageKHR(device_, swapchain_, 1000000000, getCurrentFrame().swapSemaphore, nullptr, &imageIndex);

        VkCommandBuffer cmd = getCurrentFrame().mainCommandBuffer;
        VK_CHECK(vkResetCommandBuffer(cmd, 0), "");

        VkCommandBufferBeginInfo cmdBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo), "Could not begin command buffer!");

        if (frameNumber_ == 0)
        {
            vk_utils::transitionImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            vk_utils::transitionImage(cmd, postProcessImage_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            VkClearColorValue clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
            VkImageSubresourceRange clearRange = vk_utils::getImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
            vkCmdClearColorImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
            vkCmdClearColorImage(cmd, postProcessImage_.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
        }
        else
        {
            vk_utils::transitionImage(cmd, postProcessImage_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                      VK_IMAGE_LAYOUT_GENERAL);
        }

        // Draw the compute result on the intermediate image
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ptPipeline_);
        std::vector<VkDescriptorSet> sets = {globalDescriptors_, ptDescriptors_};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ptPipelineLayout_, 0, 2,
                                sets.data(), 0, nullptr);
        vkCmdPushConstants(cmd, ptPipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(path_tracing::PushConstants),
                           &ptPushConstants_);
        vkCmdDispatch(cmd, std::ceil(swapchainExtent_.width / 16.0), std::ceil(swapchainExtent_.height / 16.0), 1);


        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ppPipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ppPipelineLayout_, 0, 1, &ppDescriptors_, 0,
                                nullptr);
        vkCmdPushConstants(cmd, ppPipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PostProcessingPushConstants),
                   &ppPushConstants_);
        vkCmdDispatch(cmd, std::ceil(swapchainExtent_.width / 16.0), std::ceil(swapchainExtent_.height / 16.0), 1);

        // Copy the drawing result onto the swap chain image
        vk_utils::transitionImage(cmd, postProcessImage_.image, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vk_utils::transitionImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vk_utils::copyImageToImage(cmd, postProcessImage_.image, swapchainImages_[imageIndex], swapchainExtent_,
                                   swapchainExtent_);

        // Transition to draw gui onto swap chain image
        vk_utils::transitionImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_GENERAL);
        drawImgui(cmd, swapchainImageViews_[imageIndex]);

        // Transition to present
        vk_utils::transitionImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_GENERAL,
                                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_CHECK(vkEndCommandBuffer(cmd), "Could not record command buffer!");

        VkSemaphoreSubmitInfo waitInfo = vk_utils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapSemaphore);
        VkSemaphoreSubmitInfo signalInfo = vk_utils::semaphoreSubmitInfo(
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR, getCurrentFrame().renderSemaphore);
        VkCommandBufferSubmitInfo cmdInfo = vk_utils::commandBufferSubmitInfo(cmd);
        VkSubmitInfo2 submitInfo = vk_utils::submitInfo(&cmdInfo, &signalInfo, &waitInfo);

        VK_CHECK(vkQueueSubmit2(queue_, 1, &submitInfo, getCurrentFrame().renderFence),
                 "Could not submit queue!");

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &getCurrentFrame().renderSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain_,
            .pImageIndices = &imageIndex,
        };

        VK_CHECK(vkQueuePresentKHR(queue_, &presentInfo), "Could not present");
        frameNumber_++;
        ptPushConstants_.frame++;
    }

    void Renderer::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
    {
        VK_CHECK(vkResetFences(device_, 1, &immediateHandles_.fence), "");
        VK_CHECK(vkResetCommandBuffer(immediateHandles_.commandBuffer, 0), "");

        VkCommandBuffer cmd = immediateHandles_.commandBuffer;

        VkCommandBufferBeginInfo cmdBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo), "");
        function(cmd);
        VK_CHECK(vkEndCommandBuffer(cmd), "");

        VkCommandBufferSubmitInfo cmdinfo = vk_utils::commandBufferSubmitInfo(cmd);
        VkSubmitInfo2 submit = vk_utils::submitInfo(&cmdinfo, nullptr, nullptr);

        // submit command buffer to the queue and execute it.
        // the fence will now block until the function commands finish execution
        VK_CHECK(vkQueueSubmit2(queue_, 1, &submit, immediateHandles_.fence), "");
        VK_CHECK(vkWaitForFences(device_, 1, &immediateHandles_.fence, true, 9999999999), "");
    }

    void Renderer::drawImgui(VkCommandBuffer cmd, VkImageView targetImageView)
    {
        VkRenderingAttachmentInfo colorAttachment = vk_utils::attachmentInfo(
            targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo = vk_utils::renderingInfo(swapchainExtent_, &colorAttachment, nullptr);

        vkCmdBeginRendering(cmd, &renderInfo);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        vkCmdEndRendering(cmd);
    }

    void Renderer::newImGuiFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }


    void Renderer::cleanup()
    {
        vkDeviceWaitIdle(device_);
        ImGui_ImplVulkan_Shutdown();
        for (auto frame : frames_)
        {
            frame.deletionQueue.flush();
        }
        deletionQueue_.flush();
    }
} // pt
