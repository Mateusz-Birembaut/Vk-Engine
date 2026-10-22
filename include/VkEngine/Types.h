#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace VkEngine {

struct FrameData {
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkFence renderFence{VK_NULL_HANDLE};
    VkSemaphore swapchainSem{VK_NULL_HANDLE};
};

struct ShaderCodeData {
    size_t codeSize; // must be a multiple of 4
    std::vector<uint32_t> codeData;
};

} // namespace VkEngine
