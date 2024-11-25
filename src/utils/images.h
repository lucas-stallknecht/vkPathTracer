#pragma once
#include "types.h"

namespace pt_utils {
    VkImageSubresourceRange getImageSubresourceRange(VkImageAspectFlags aspectMask);
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
}
