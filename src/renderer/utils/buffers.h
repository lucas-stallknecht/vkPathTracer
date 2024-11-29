#pragma once
#include "types.h"
#include "constants.h"

namespace renderer_utils
{
    renderer::AllocatedBuffer createBuffer(VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage,
                                           VmaMemoryUsage memoryUsage);
    void destroyBuffer(VmaAllocator allocator, const renderer::AllocatedBuffer& buffer);
} // renderer_utils
