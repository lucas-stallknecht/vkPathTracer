#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
// double ended queue
#include <deque>
#include <functional>
#include <optional>
#include <glm/glm.hpp>

namespace core
{
    struct Vertex {
        glm::vec3 position;
        float uv1;
        glm::vec3 normal;
        float uv2;
    }; // 32 bytes

    struct TraceTriangle
    {
        uint32_t v0;
        uint32_t v1;
        uint32_t v2;
        float padding1;
        glm::vec3 centroid;
        float padding2;
    }; // 32 bytes

    struct TraceMaterial
    {
        glm::vec3 diffuseCol;
        float emissiveStrength;
        float roughness;
        float metallic;
        glm::vec2 padding;
    }; // 32 bytes

    struct TraceBVHNode {
        glm::vec3 aabbMin;
        uint32_t triangleCount;
        glm::vec3 aabbMax;
        uint32_t index; // triangleIndex if leaf node, otherwise childIndex
    }; // 32 bytes

    struct TraceMesh
    {
        std::vector<Vertex> vertices;
        std::vector<TraceTriangle> triangles;
        TraceMaterial material;
        std::vector<TraceBVHNode> nodes;
    };
}

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

    struct TraceSceneBuffers
    {
        AllocatedBuffer vertexBuffer{};
        VkDeviceAddress vertexBufferAddress = 0;
        AllocatedBuffer triangleBuffer{};
        VkDeviceAddress triangleBufferAddress = 0;
        AllocatedBuffer materialBuffer{};
        VkDeviceAddress materialBufferAddress = 0;
        AllocatedBuffer nodeBuffer{};
        VkDeviceAddress nodeBufferAddress = 0;
        AllocatedBuffer meshInfoBuffer{};
        VkDeviceAddress meshInfoBufferAddress = 0;
    };

    struct TraceMeshInfo
    {
        uint32_t vertexOffset = 0;
        uint32_t triangleOffset = 0;
        uint32_t nodeOffset = 0;
        uint32_t materialIndex = 0;
    };

    struct TracePushConstants {
        VkDeviceAddress vertexBuffer;
        VkDeviceAddress triangleBuffer;
        VkDeviceAddress nodeBuffer;
        VkDeviceAddress materialBuffer;
        VkDeviceAddress meshInfoBuffer;
        uint32_t meshCount;
        uint32_t frame;
        uint32_t useSkybox;
        uint32_t skyboxVisible;
    };
}


