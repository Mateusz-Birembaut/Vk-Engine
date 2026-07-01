#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class VulkanCtx;

inline constexpr int FRAMES_IN_FLIGHT = 2;

struct FrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence renderFence;
    VkSemaphore swapchainSem;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init(VulkanCtx* pVulkanCtx);
    void initCommands();
    void initSyncStructs();
    void cleanup();
    void recreateSwapchain();
    void drawFrame();

    FrameData& getCurrentFrame();

private:
    VulkanCtx* m_ctx;

    FrameData m_frames[FRAMES_IN_FLIGHT];
    uint32_t m_frameNb{0};

    std::vector<VkSemaphore> m_renderSems; // 1 per swapchain image

    VkClearColorValue m_clearColorValue{};
};
