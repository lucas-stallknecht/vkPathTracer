#pragma once
#include "types.h"
#include "constants.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <vector>
#include <cstdint>

#include "utils/descriptors.h"

namespace pt {

class Engine {
public:
    void init();
    void run();
    void cleanup();

private:
    void initWindow();
    void initVulkan();
    void initImgui();
    void createInstance();
    bool isDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void pickPhysicalDevice();
    void createLogicaldevice();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void createSwapchain();
    void createVmaAllocator();
    void createDrawImage();
    void createDescriptors();
    void createComputePipeline();
    void createFrameData();
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void draw();

    GLFWwindow* window_ = nullptr;
    ImGuiIO* io = nullptr;
    uint32_t frameNumber_ = 0;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VmaAllocator allocator_{};
    pt_utils::DescriptorAllocator globalDescriptorAllocator{};

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

    FrameData frames_[FRAME_OVERLAP];
    FrameData& getCurrentFrame() { return frames_[frameNumber_ % FRAME_OVERLAP]; }
};


} // pt
