#pragma once

#include <vulkan/vulkan.h>

class VulkanCtx;

inline constexpr int FRAMES_IN_FLIGHT = 2;

struct FrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init(VulkanCtx* pVulkanCtx);
    void initCommands();
    void cleanup();

    FrameData& getCurrentFrame();

private:
    VulkanCtx* m_ctx;

    FrameData m_frames[FRAMES_IN_FLIGHT];
    uint32_t m_frameNb;
};
