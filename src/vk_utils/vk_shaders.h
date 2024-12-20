#pragma once
#include "types.h"
#include "constants.h"

#include <vector>

namespace vk_utils
{
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
}
