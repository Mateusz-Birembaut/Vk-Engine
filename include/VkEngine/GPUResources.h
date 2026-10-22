#pragma once

#include <stdint.h>
#include <vector>

#include "VkEngine/Pool.h"
#include "VkEngine/Types.h"
#include "VkEngine/VulkanCtx.h"

class GPUResources {
public:
    GPUResources() = default;
    ~GPUResources() = default;

    void init(VulkanCtx* pVulkanCtx);

    Handle<VkEngine::GPUImage> createTexture(const VkEngine::CPUImage& cpuImg);
    uint32_t getOrCreateSampler();
    void cleanup();

private:
    VkEngine::GPUBuffer createStagingBuffer(const uint64_t size);
    void copyImageToStagingBuffer(const VkEngine::CPUImage& cpuImg, VkEngine::GPUBuffer& stagingBuffer);
    void recordTransientCommandBuffer(
        VkCommandBuffer& transientCmdBuff, VkEngine::GPUBuffer& stagingBuffer, const VkEngine::CPUImage& cpuImg,
        const VkEngine::GPUImage& gpuImage
    );
    void submitTransientCommandBuffer(VkCommandBuffer& transientCmdBuff);

    Pool<VkEngine::GPUImage> m_gpuImgs;
    Pool<VkEngine::GPUBuffer> m_gpuBuffs;
    std::vector<VkSampler> m_samplers;

    VkCommandPool m_transientBuffCmdPool = VK_NULL_HANDLE;
    VulkanCtx* m_ctx = nullptr;
};
