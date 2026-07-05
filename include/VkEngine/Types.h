#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace VkEngine {

struct FrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence renderFence;
    VkSemaphore swapchainSem;
};

struct ShaderCodeData {
    size_t codeSize; // must be a multiple of 4
    std::vector<uint32_t> codeData;
};

} // namespace VkEngine
