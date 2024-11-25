#pragma once
#include "types.h"

#include <vector>
#include <fstream>

namespace pt_utils
{
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
}
