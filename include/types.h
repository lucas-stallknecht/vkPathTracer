#pragma once
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
// double ended queue
#include <deque>
#include <functional>
#include <optional>
#include <glm/glm.hpp>
#include <string>

namespace core
{
    struct Vertex
    {
        glm::vec3 position;
        float uv1;
        glm::vec3 normal;
        float uv2;

        bool operator==(const Vertex& other) const
        {
            return position == other.position && uv1 == other.uv1 && uv2 == other.uv2;
        }
    }; // 32 bytes
}

namespace renderer
{
    struct DeletionQueue
    {
        std::deque<std::function<void()>> deletors;

        void push_function(std::function<void()>&& function)
        {
            deletors.push_back(function);
        }

        void flush()
        {
            // reverse iterate the deletion queue to execute all the functions
            for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
            {
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

    struct AllocatedImage
    {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkExtent3D imageExtent = {};
        VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    };

    struct AllocatedBuffer
    {
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

    struct PostProcessingPushConstants
    {
        uint32_t method = 1;
        float exposure = 1.0;
    };
}

namespace path_tracing
{
    struct Triangle
    {
        uint32_t v0;
        uint32_t v1;
        uint32_t v2;
        float padding1;
        glm::vec3 tangent;
        float padding3;
    }; // 32 bytes

    struct TextureIterationSettings
    {
        int index;
        bool sRGB;
    };
    struct TextureCreateSettings
    {
        std::string name;
        bool sRGB;
    };

    struct Material
    {
        glm::vec3 color = glm::vec3(1.0f);
        float emissiveStrength = 0.0f;
        float roughness = 0.5f;
        float metallic = 0.0f;
        std::optional<std::string> colorMap = "assets/defaults/default_texture.png";
        std::optional<std::string> roughnessMap = "assets/defaults/default_texture.png";
        std::optional<std::string> metallicMap = "assets/defaults/default_texture.png";
        std::optional<std::string> normalMap;

        static int handleMapProperty(const std::optional<std::string>& property,
                                     std::unordered_map<std::string, TextureIterationSettings>& map, int& currentIndex,
                                     bool sRGB = false)
        {
            if (property.has_value())
            {
                const std::string& key = property.value();
                if (map.contains(key))
                    return map.at(key).index;

                TextureIterationSettings t = {.index = ++currentIndex, .sRGB = sRGB};
                map.emplace(key, t);
                return currentIndex;
            }
            return -1;
        }
    };

    struct GPUMaterial
    {
        glm::vec3 baseCol;
        int baseColMapIndex = -1;
        float emissiveStrength;
        int emissiveMapIndex = -1;
        float roughness;
        int roughnessMapIndex = -1;
        float metallic;
        int metallicMapIndex = -1;
        int normalMapIndex = -1;
        float padding;
    }; // 48 bytes

    struct BVHNode
    {
        glm::vec3 aabbMin;
        uint32_t triangleCount;
        glm::vec3 aabbMax;
        uint32_t index; // triangleIndex if leaf node, otherwise childIndex
    }; // 32 bytes


    struct SceneBuffers
    {
        renderer::AllocatedBuffer vertexBuffer{};
        VkDeviceAddress vertexBufferAddress = 0;
        renderer::AllocatedBuffer triangleBuffer{};
        VkDeviceAddress triangleBufferAddress = 0;
        renderer::AllocatedBuffer materialBuffer{};
        VkDeviceAddress materialBufferAddress = 0;
        renderer::AllocatedBuffer nodeBuffer{};
        VkDeviceAddress nodeBufferAddress = 0;
        renderer::AllocatedBuffer meshInfoBuffer{};
        VkDeviceAddress meshInfoBufferAddress = 0;
    };

    struct MeshInfo
    {
        uint32_t vertexOffset = 0;
        uint32_t triangleOffset = 0;
        uint32_t nodeOffset = 0;
        uint32_t materialIndex = 0;
    };

    struct PushConstants
    {
        VkDeviceAddress vertexBuffer;
        VkDeviceAddress triangleBuffer;
        VkDeviceAddress nodeBuffer;
        VkDeviceAddress materialBuffer;
        VkDeviceAddress meshInfoBuffer;
        uint32_t meshCount;
        uint32_t frame;
        uint32_t bounces = 5;
        uint32_t samples = 1;
        float jitter = 1.0;
        uint32_t smoothShading = 1;
        float envMapIntensity = 0.0;
        uint32_t envMapVisible = 0;
    };
}
