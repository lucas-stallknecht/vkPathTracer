#include "descriptors.h"

#include <stdexcept>

namespace pt_utils
{
    void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
    {
        VkDescriptorSetLayoutBinding newbind{
            .binding = binding,
            .descriptorType = type,
            .descriptorCount = 1,
        };
        bindings.push_back(newbind);
    }

    void DescriptorLayoutBuilder::clear()
    {
        bindings.clear();
    }

    VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext,
                                                         VkDescriptorSetLayoutCreateFlags flags)
    {
        for (auto& b : bindings)
        {
            b.stageFlags |= shaderStages;
        }

        VkDescriptorSetLayoutCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = pNext,
            .flags = flags,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };


        VkDescriptorSetLayout setLayout;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout), "Could not create descriptor set layout!");

        return setLayout;
    }


    void DescriptorAllocator::initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        for (PoolSizeRatio ratio : poolRatios) {
            poolSizes.push_back(VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = static_cast<uint32_t>(ratio.ratio * maxSets)
            });
        }

        VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = 0,
            .maxSets = maxSets,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data(),
        };

        VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool), "Could not create descriptor pool!");

    }

    void DescriptorAllocator::clearDescriptors(VkDevice device)
    {
        vkResetDescriptorPool(device, pool, 0);
    }

    void DescriptorAllocator::destroyPool(VkDevice device)
    {
        vkDestroyDescriptorPool(device,pool,nullptr);
    }

    VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
    {
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };


        VkDescriptorSet ds;
       VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds), "Could not allocate descriptor set!");

        return ds;
    }
}
