#include "create_infos.h"

namespace renderer_utils
{
    VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
    {
        VkSemaphoreSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = semaphore,
            .value = 1,
            .stageMask = stageMask,
            .deviceIndex = 0,
        };

        return submitInfo;
    }

    VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmd)
    {
        VkCommandBufferSubmitInfo info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd,
            .deviceMask = 0,
        };
        return info;
    }

    VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmd, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
                             VkSemaphoreSubmitInfo* waitSemaphoreInfo)
    {
        VkSubmitInfo2 info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,

            .waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfo == nullptr ? 0 : 1),
            .pWaitSemaphoreInfos = waitSemaphoreInfo,

            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = cmd,

            .signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfo == nullptr ? 0 : 1),
            .pSignalSemaphoreInfos = signalSemaphoreInfo,
        };
        return info;
    }

    VkRenderingAttachmentInfo attachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout)
    {
        VkRenderingAttachmentInfo colorAttachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = view,
            .imageLayout = layout,
            .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        };
        if (clear)
        {
            colorAttachment.clearValue = *clear;
        }

        return colorAttachment;
    }

    VkRenderingInfo renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment,
                                  VkRenderingAttachmentInfo* depthAttachment)
    {
        VkRenderingInfo info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = VkRect2D{VkOffset2D{0, 0}, renderExtent},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = colorAttachment,
            .pDepthAttachment = depthAttachment,
            .pStencilAttachment = nullptr
        };

        return info;
    }
}
