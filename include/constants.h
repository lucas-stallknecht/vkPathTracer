#pragma once
#include "types.h"
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>


#ifndef NDEBUG
    #define ENABLE_VALIDATION_LAYERS
#endif

constexpr uint32_t WIDTH = 1700;
constexpr uint32_t HEIGHT = 950;
constexpr uint32_t FRAME_OVERLAP = 2;

const std::vector<const char*> VALIDATIONS_LAYERS = {
    "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
};

#define VK_CHECK(x, message)                                                \
do {                                                                        \
VkResult err = x;                                                           \
    if (err != VK_SUCCESS) {                                                \
        printf("Detected Vulkan error: %s", string_VkResult(err));          \
        throw std::runtime_error(message);                                  \
    }                                                                       \
} while (0)
