#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
// double ended queue
#include <deque>
#include <functional>
#include <optional>
#include <glm/glm.hpp>

namespace renderer
{
    struct DeletionQueue
    {
        std::deque<std::function<void()>> deletors;

        void push_function(std::function<void()>&& function) {
            deletors.push_back(function);
        }

        void flush() {
            // reverse iterate the deletion queue to execute all the functions
            for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
                (*it)(); //call the function
            }

            deletors.clear();
        }
    };

    struct FrameData
    {
        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkCommandBuffer mainCommandBuffer = VK_NULL_HANDLE;
        VkFence renderFence = VK_NULL_HANDLE;
        VkSemaphore swapSemaphore = VK_NULL_HANDLE;
        VkSemaphore renderSemaphore = VK_NULL_HANDLE;
        DeletionQueue deletionQueue;
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
