#pragma once
#include "types.h"
#include "constants.h"

#include <GLFW/glfw3.h>
#include <vector>
#include <cstdint>
#include <functional>
#include <stb_image.h>
#include <array>

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
        void uploadPathTracingScene(const std::vector<path_tracing::Mesh>& scene);
        void uploadTextures(const std::vector<std::string>& texturePaths);
        void uploadSkybox(const std::string& skyboxPath);
        void resetAccumulation();
        void toggleSkyboxUse();
        void toggleSkyboxVisibility();
        void cleanup();

        path_tracing::PushConstants ptPushConstants_{};
        PostProcessingPushConstants ppPushConstants_{};

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
        void createGlobalBuffer();
        void createGlobalDescriptors();
        void createPathTracingDescriptors();
        void createPathTracingPipeline();
        void createPostProcessingDescriptors();
        void createPostProcessingPipeline();
        void createCommands();
        void createSyncs();
        void updateGlobalBuffer(const core::Camera& camera) const;
        AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        void destroyBuffer(const AllocatedBuffer& buffer);
        AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
        AllocatedImage createImage(stbi_uc* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
        AllocatedImage createCubemap(std::array<stbi_uc*, 6> data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage);
        void destroyImage(const AllocatedImage& image);
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
        vk_utils::DescriptorAllocator globalDescriptorAllocator_{};
        AllocatedImage skybox_;
        std::vector<AllocatedImage> textures_;
        VkSampler defaultLinearSampler_ = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage> swapchainImages_;
        std::vector<VkImageView> swapchainImageViews_;
        VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
        VkExtent2D swapchainExtent_ = {0, 0};

        AllocatedImage drawImage_;
        AllocatedImage postProcessImage_;

        AllocatedBuffer globalBuffer_;
        VkDescriptorSetLayout globalDescLayout_ = VK_NULL_HANDLE;
        VkDescriptorSet globalDescriptors_ = VK_NULL_HANDLE;

        VkDescriptorSetLayout ptDescLayout_ = VK_NULL_HANDLE;
        VkDescriptorSet ptDescriptors_ = VK_NULL_HANDLE;
        VkPipeline ptPipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout ptPipelineLayout_ = VK_NULL_HANDLE;
        path_tracing::SceneBuffers ptScene_;

        VkDescriptorSetLayout ppDescLayout_ = VK_NULL_HANDLE;
        VkDescriptorSet ppDescriptors_ = VK_NULL_HANDLE;
        VkPipeline ppPipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout ppPipelineLayout_ = VK_NULL_HANDLE;

        ImmediateHandles immediateHandles_;
        DeletionQueue deletionQueue_;
        FrameData frames_[FRAME_OVERLAP];
        FrameData& getCurrentFrame() { return frames_[frameNumber_ % FRAME_OVERLAP]; }
    };
} // rend
