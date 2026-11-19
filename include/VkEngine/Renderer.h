#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <time.h>
#include <vector>

#include "VkEngine/GPUResources.h"
#include "VkEngine/ResourceDestroyer.h"
#include "VkEngine/Types/Handle.h"
#include "VkEngine/Types/Texturing.h"
#include "VkEngine/Types/Vertex.h"

namespace vke {

class VulkanCtx;

inline constexpr int FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
    Renderer();
    ~Renderer();

    void init(VulkanCtx* pVulkanCtx, const std::filesystem::path& shaderFolderPath);
    void cleanup();
    void drawFrame(const std::span<RenderItem>& renderItems, const RenderView& RenderView);

    Handle<GPUMesh> createMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices);

    Handle<GPUImage> createGpuImage(const ImageDesc& imgDesc);

    Handle<GPUMaterial> createMaterial(const MaterialDesc& material);

    uint32_t getOrCreateSampler(const Sampler& sampler);

    FrameData& getCurrentFrame();

    Texture fallbackTexture() const;
    Handle<GPUMaterial> fallbackMaterial() const;

private:
    void initPerFrameData();
    void initGraphicsPipeline();

    void createRenderSems();
    void cleanupRenderSems();
    void recreateSwapchain();

    VulkanCtx* m_ctx{nullptr};
    bool m_swapchainDirty{false};

    FrameData m_frames[FRAMES_IN_FLIGHT];
    uint64_t m_frameNb{0};

    std::vector<VkSemaphore> m_renderSems; // 1 per swapchain image

    VkClearColorValue m_clearColorValue{
        .float32 = {
            0.0f,      // R (0.0)
            0.033105f, // G (0.2 sRGB)
            0.132868f, // B (0.4 sRGB)
            1.0f,      // A
        }
    };
    VkClearDepthStencilValue m_clearDepthValue{.depth = 1.0f, .stencil = 0};

    Texture m_fallbackTexture{};
    Handle<GPUMaterial> m_fallbackMaterial{};

    GPUResources m_gpuResources;
    ResourceDestroyer m_resourceDestroyer;

    std::filesystem::path m_shaderFolderPath{};

    VkPipeline m_graphicsPipeline;
    VkPipelineLayout m_graphicsPipelineLayout;
};

} // namespace vke