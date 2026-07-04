#pragma once

#include <vulkan/vulkan.h>

namespace VkEngine {

struct FrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence renderFence;
    VkSemaphore swapchainSem;
};

} // namespace VkEngine
