#pragma once
#include "types.h"
#include "constants.h"

#include <GLFW/glfw3.h>
#include <vector>
#include <cstdint>
#include <functional>
#include <stb_image.h>
#include <array>

#include "vk_utils/vk_descriptors.h"
#include "path_tracing/mesh.h"
#include "core/camera.h"

namespace renderer
{
    class Renderer
    {
        struct Pipelines
        {
            VkPipeline pathTracing = VK_NULL_HANDLE;
            VkPipeline postProcessing = VK_NULL_HANDLE;
            VkPipeline cubemapCreation = VK_NULL_HANDLE;
        };

        struct PipelineLayouts
        {
            VkPipelineLayout pathTracing = VK_NULL_HANDLE;
            VkPipelineLayout postProcessing = VK_NULL_HANDLE;
            VkPipelineLayout cubemapCreation = VK_NULL_HANDLE;
        };

        struct DescriptorLayouts
        {
            VkDescriptorSetLayout global = VK_NULL_HANDLE;
            VkDescriptorSetLayout pathTracing = VK_NULL_HANDLE;
            VkDescriptorSetLayout postProcessing = VK_NULL_HANDLE;
            VkDescriptorSetLayout cubemapCreation = VK_NULL_HANDLE;
        };

        struct DescriptorSets
        {
            VkDescriptorSet glboal = VK_NULL_HANDLE;
            VkDescriptorSet pathTracing = VK_NULL_HANDLE;
            VkDescriptorSet postProcessing = VK_NULL_HANDLE;
            VkDescriptorSet cubemapCreation = VK_NULL_HANDLE;
        };

        struct GlobalResources
        {
            AllocatedBuffer buffer;
            AllocatedImage envMap;
            VkSampler defaultLinearSampler = VK_NULL_HANDLE;
        };

    public:
        void init(GLFWwindow* window);
        void newImGuiFrame();
        void render(const core::Camera& camera);
        void uploadPathTracingScene(const std::vector<path_tracing::Mesh>& scene);
        void uploadTextures(const std::vector<path_tracing::TextureCreateSettings>& settings);
        void uploadEnvMap(const std::string& path);
        void resetAccumulation();
        void cleanup();

        path_tracing::PushConstants ptPushConstants_{};
        PostProcessingPushConstants ppPushConstants_{};

    private:
        void initVulkan(GLFWwindow* window);
        void initImguiBackend(GLFWwindow* wwindow);
        void initGlobalResources();
        void initPathTracing();
        void initPostProcessing();
        void initEquiToCubeMap();
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
        void createCommands();
        void createSyncs();
        AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
        void destroyBuffer(const AllocatedBuffer& buffer);
        AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
        AllocatedImage createImage(stbi_uc* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                   bool mipmapped);
        AllocatedImage createCubemap(VkExtent3D size, VkFormat format, VkImageUsageFlags usage);
        void destroyImage(const AllocatedImage& image);
        void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
        void draw();
        void updateGlobalDescriptors(const core::Camera& camera) const;
        void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);

        uint32_t frameNumber_ = 0;
        VkInstance instance_ = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue queue_ = VK_NULL_HANDLE;
        std::optional<uint32_t> queueFamily_;
        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        std::vector<VkImage> swapchainImages_;
        std::vector<VkImageView> swapchainImageViews_;
        VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
        VkExtent2D swapchainExtent_ = {0, 0};

        VmaAllocator allocator_{};
        vk_utils::DescriptorAllocator globalDescriptorAllocator_{};

        GlobalResources globalResources_;
        AllocatedImage drawImage_;
        AllocatedImage postProcessImage_;
        path_tracing::SceneBuffers sceneBuffers_;
        std::vector<AllocatedImage> textures_;

        PipelineLayouts pipelineLayouts_;
        Pipelines pipelines_;
        DescriptorLayouts descriptorLayouts_;
        DescriptorSets descriptorSets_;

        ImmediateHandles immediateHandles_;
        DeletionQueue deletionQueue_;
        FrameData frames_[FRAME_OVERLAP];
        FrameData& getCurrentFrame() { return frames_[frameNumber_ % FRAME_OVERLAP]; }
    };
} // rend
