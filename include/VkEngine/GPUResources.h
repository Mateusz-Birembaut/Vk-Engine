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

    VkDescriptorSetLayout descSetLayout() const;
    VkDescriptorSet descSet() const;
    uint64_t vertBuffAddr() const;
    VkBuffer indexBuffer() const;
    const VkEngine::GPUMesh* getMesh(const Handle<VkEngine::GPUMesh>& meshHandle) const;

    /**
     * @brief Sets vulkan context and creates a command pool for uploads to VRAM
     *
     */
    void init(VulkanCtx* pVulkanCtx);

    /**
     * @brief Create a 2D R8G8B8A8_SRGB image stored vram for the given img data
     *
     * @attention Image channels must be 4, blocks CPU (waits for upload to end)
     *
     * // TODO update function to use a description of the sampler before
     *
     * @return Handle of the created Texture { gpuImage index, gen }
     */
    Handle<VkEngine::GPUImage> createGpuImage(const VkEngine::CPUImage& cpuImg);

    /**
     * @brief Create a simple sampler
     *
     *
     * // TODO update function to use a description of the sampler before
     * @return Sampler index used for bindless texture sampling
     */
    uint32_t getOrCreateSampler();

    /**
     * @brief Frees all the memory of "Alive" slots in each pool and destroys every sampler.
     *
     * @attention must be called before m_ctx is destroyed
     *
     * @attention GPU must be idle.
     *
     */
    void cleanup();

private:
    void initDescriptorPool(VkDevice device);

    void initDescriptorSetLayout(VkDevice device);

    void initDescriptorSet(VkDevice device);

    VkEngine::GPUBuffer
    createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo& allocInfo);

    /**
     * @brief Creates a staging buffer (allocation via VMA).
     * @attention The Buffer is returned and the caller is responsible for buffer deletion
     *
     * @param size Size in bytes of the buffer
     *
     * @return Buffer created
     */
    VkEngine::GPUBuffer createStagingBuffer(const uint64_t size);

    VkEngine::GPUMegaBuffer createMegaGPUBuffer(const uint64_t capacity, VkBufferUsageFlags usage);

    /**
     * @brief Copies Img data to staging buffer
     *
     * @attention Image and buffer size must be the same
     */
    void copyImageToStagingBuffer(const VkEngine::CPUImage& cpuImg, VkEngine::GPUBuffer& stagingBuffer);

    /**
     * @brief Starts the recording of the command buffer.
     * sets up barrier to transition the VkImage to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
     * copy operation from buffer to image.
     * sets up barrier to transition the VkImage to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
     * Ends the recording.
     */
    void recordTransientCommandBuffer(
        VkCommandBuffer& transientCmdBuff, VkEngine::GPUBuffer& stagingBuffer, const VkEngine::CPUImage& cpuImg,
        const VkEngine::GPUImage& gpuImage
    );

    /**
     * @brief Submits command buffer to GPU using graphics queue,
     * use vkQueueWaitIdle to wait for upload to end
     *
     * // TODO update this to not use vkQueueWaitIdle
     */
    void submitTransientCommandBuffer(VkCommandBuffer& transientCmdBuff);

    Pool<VkEngine::GPUImage> m_gpuImgs;
    Pool<VkEngine::GPUBuffer> m_gpuBuffs;
    Pool<VkEngine::GPUMesh> m_gpuMeshes;
    std::vector<VkSampler> m_samplers;

    const uint64_t vertexMegaBuffCapacity{50000000};
    VkEngine::GPUMegaBuffer m_vertexMegaBuff{};

    const uint64_t indexMegaBuffCapacity{1000000};
    VkEngine::GPUMegaBuffer m_indexMegaBuff{};

    const uint32_t m_maxTextureCount{1024};
    const uint32_t m_maxSamplerCount{16};

    VkDescriptorPool m_descPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_descSet{VK_NULL_HANDLE};

    VkCommandPool m_transientBuffCmdPool{VK_NULL_HANDLE};
    VulkanCtx* m_ctx{nullptr};
};
