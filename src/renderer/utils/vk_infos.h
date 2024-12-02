#pragma once
#include "types.h"

namespace vk_utils
{
    VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
    VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);
    VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                             VkSemaphoreSubmitInfo* waitSemaphoreInfo);
    VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout);
    VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
                                  VkRenderingAttachmentInfo* depthAttachment);
    VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, bool cubeMap = false);
    VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage iamge, VkImageAspectFlags aspectFlags, bool cubeMap = false);
}
