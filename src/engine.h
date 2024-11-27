#pragma once
#include <Camera.h>

#include "types.h"
#include "constants.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <vector>
#include <cstdint>
#include <functional>


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
    void createGlobalBuffers();
    void createDescriptors();
    void createComputePipeline();
    void createCommands();
    void createSyncs();
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    void destroyBuffer(const AllocatedBuffer& buffer);
    void updateGlobalBuffers();
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void draw();

    void keyCallback(GLFWwindow* window, int key);
    void mouseCallback(GLFWwindow* window, float xpos, float ypos);
    void mouseButtonCallback(GLFWwindow* window, int button, int action);
    void keyInput();

    GLFWwindow* window_ = nullptr;
    ImGuiIO* io = nullptr;
    Camera camera_ = {50.0f, static_cast<float>(WIDTH) / static_cast<float>(HEIGHT)};
    bool focused = false;
    bool* keysArePressed = nullptr;
    bool isFirstMouseMove = true;
    glm::vec2 lastMousePosition = glm::vec2();

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

    AllocatedBuffer cameraBuffer_{};

    ImmediateHandles immediateHandles{};
    FrameData frames_[FRAME_OVERLAP];
    FrameData& getCurrentFrame() { return frames_[frameNumber_ % FRAME_OVERLAP]; }
};


} // pt
