#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <time.h>
#include <vector>

#include "VkEngine/ResourceDestroyer.h"
#include "VkEngine/Types.h"

class VulkanCtx;

inline constexpr int FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init(VulkanCtx* pVulkanCtx, const std::filesystem::path& shaderFolderPath);
    void cleanup();
    void drawFrame();

    VkEngine::FrameData& getCurrentFrame();

private:
    void initCommands();
    void initSyncStructs();
    void initGraphicsPipeline();

    void createRenderSems();
    void cleanupRenderSems();
    void recreateSwapchain();

    VulkanCtx* m_ctx = nullptr;
    bool m_swapchainDirty = false;

    VkEngine::FrameData m_frames[FRAMES_IN_FLIGHT];
    uint64_t m_frameNb{0};

    std::vector<VkSemaphore> m_renderSems; // 1 per swapchain image

    VkClearColorValue m_clearColorValue{};

    ResourceDestroyer m_resourceDestroyer;

    std::filesystem::path m_shaderFolderPath{};

    VkPipeline m_graphicsPipeline;
    VkPipelineLayout m_graphicsPipelineLayout;

    double start = 100 * ((double) clock()) / (double) CLOCKS_PER_SEC;
};
