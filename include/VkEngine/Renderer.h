#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <time.h>
#include <vector>

#include "VkEngine/GPUResources.h"
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
    void drawFrame(const std::span<vke::RenderItem>& renderItems, const vke::RenderView& RenderView);

    Handle<vke::GPUMesh> createMesh(std::span<const vke::Vertex> vertices, std::span<const uint32_t> indices);

    Handle<vke::GPUImage> createTexture(const vke::CPUImage& cpuImg);

    vke::FrameData& getCurrentFrame();

private:
    void initPerFrameData();
    void initGraphicsPipeline();

    void createRenderSems();
    void cleanupRenderSems();
    void recreateSwapchain();

    VulkanCtx* m_ctx{nullptr};
    bool m_swapchainDirty{false};

    vke::FrameData m_frames[FRAMES_IN_FLIGHT];
    uint64_t m_frameNb{0};

    std::vector<VkSemaphore> m_renderSems; // 1 per swapchain image

    VkClearColorValue m_clearColorValue{};
    VkClearDepthStencilValue m_clearDepthValue{.depth = 1.0f, .stencil = 0};
    vke::Texture m_fallbackTexture{};

    Handle<vke::GPUMesh> m_testMesh;
    Handle<vke::GPUMesh> m_testMesh2;

    GPUResources m_gpuResources;
    ResourceDestroyer m_resourceDestroyer;

    std::filesystem::path m_shaderFolderPath{};

    VkPipeline m_graphicsPipeline;
    VkPipelineLayout m_graphicsPipelineLayout;
};
