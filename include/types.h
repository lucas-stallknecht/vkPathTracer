#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <optional>
#include <glm/glm.hpp>

namespace pt
{
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsAndComputeFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete()
        {
            return graphicsAndComputeFamily.has_value() && presentFamily.has_value();
        }
    };

    struct FrameData
    {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer mainCommandBuffer = VK_NULL_HANDLE;
        VkFence renderFence = VK_NULL_HANDLE;
        VkSemaphore swapSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderSemaphore = VK_NULL_HANDLE;
    };

    struct ImmediateHandles
    {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
    };

    struct AllocatedImage {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkExtent3D imageExtent = {};
        VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    };

    struct AllocatedBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocationInfo info{};
    };

    struct CameraUniform
    {
        glm::vec3 position;
        float padding;
        glm::mat4 invView;
        glm::mat4 invProj;
    };
}
