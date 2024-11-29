#include "buffers.h"

namespace renderer_utils {
    renderer::AllocatedBuffer createBuffer(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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

        renderer::AllocatedBuffer newBuffer;
        VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
                     &newBuffer.info), "Could not create buffer!");

        return newBuffer;
    }

    void destroyBuffer(VmaAllocator allocator, const renderer::AllocatedBuffer& buffer)
    {
        vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
    }

} // renderer_utils