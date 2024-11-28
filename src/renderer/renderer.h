#pragma once
#include "types.h"
#include "constants.h"

#include <GLFW/glfw3.h>
#include <vector>
#include <cstdint>
#include <functional>

#include "utils/descriptors.h"
#include "core/camera.h"

namespace renderer
{
    class Renderer
    {
    public:
        void init(GLFWwindow* window);
        void newImGuiFrame();
        void render(const core::Camera& camera);
        void cleanup();

    private:
        void initVulkan(GLFWwindow* window);
        void initImguiBackend(GLFWwindow* wwindow);
        void createInstance(GLFWwindow* window);
        bool isDeviceSuitable(VkPhysicalDevice device);
        void findQueueFamily(VkPhysicalDevice device);
        void pickPhysicalDevice();
        void createLogicaldevice();
        static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window);
        void createSwapchain(GLFWwindow* window);
        void createVmaAllocator();
        void createDrawImage();
        void createGlobalBuffers();
        void createDescriptors();
        void createComputePipeline();
        void createCommands();
        void createSyncs();
        AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        void destroyBuffer(const AllocatedBuffer& buffer);
        void updateGlobalBuffers(const core::Camera& camera) const;
        void draw();
        void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
        void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

        uint32_t frameNumber_ = 0;

        VkInstance instance_ = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue queue_ = VK_NULL_HANDLE;
        std::optional<uint32_t> queueFamily_;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;

        VmaAllocator allocator_{};
        renderer_utils::DescriptorAllocator globalDescriptorAllocator{};

        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage> swapchainImages_;
        std::vector<VkImageView> swapchainImageViews_;
        VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
        VkExtent2D swapchainExtent_ = {0, 0};

        AllocatedImage drawImage_{};
        VkDescriptorSetLayout drawImageLayout_ = VK_NULL_HANDLE;
        VkDescriptorSet drawImageDescriptors_ = VK_NULL_HANDLE;

        VkPipeline computePipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout computePipelineLayout_ = VK_NULL_HANDLE;

        AllocatedBuffer cameraBuffer_{};

        ImmediateHandles immediateHandles{};
        DeletionQueue deletionQueue_;
        FrameData frames_[FRAME_OVERLAP];
        FrameData& getCurrentFrame() { return frames_[frameNumber_ % FRAME_OVERLAP]; }
    };
} // rend
