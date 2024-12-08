#pragma once
#include "types.h"
#include <string>
#include <stb_image.h>

namespace vk_utils
{
    VkImageSubresourceRange getImageSubresourceRange(VkImageAspectFlags aspectMask);
    void transitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void transitionCubemap(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize,
                          VkExtent2D dstSize);
    stbi_uc* loadTextureData(const std::string& path, VkExtent3D& size);
    float* loadHDRTextureData(const std::string& path, VkExtent3D& size);
    void freeImageData(void* data);

}
