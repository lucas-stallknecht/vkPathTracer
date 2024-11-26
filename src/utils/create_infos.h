#pragma once
#include "types.h"

namespace pt_utils
{
    VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
    VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd);
    VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                             VkSemaphoreSubmitInfo* waitSemaphoreInfo);
    VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout);
    VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
                                  VkRenderingAttachmentInfo* depthAttachment);
}
