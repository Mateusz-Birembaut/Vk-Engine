#pragma once

#include <span>
#include <stdint.h>
#include <vector>

#include "VkEngine/Helpers.h"
#include "VkEngine/Pool.h"
#include "VkEngine/Types.h"
#include "VkEngine/VkInits.h"
#include "VkEngine/VulkanCtx.h"

class GPUResources {
public:
    GPUResources() = default;
    ~GPUResources() = default;

    VkDescriptorSetLayout descSetLayout() const;
    VkDescriptorSet descSet() const;
    uint64_t vertBuffAddr() const;
    VkBuffer indexBuffer() const;
    const vke::GPUMesh* getMesh(const Handle<vke::GPUMesh>& meshHandle) const;

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
    Handle<vke::GPUImage> createGpuImage(const vke::CPUImage& cpuImg);

    Handle<vke::GPUMesh> createGpuMesh(std::span<const vke::Vertex> vertices, std::span<const uint32_t> indices);

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

    /**
     * @brief Creates a new GPUBuffer of the specified size

     * @param usage If contains VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, gpuBuff.deviceAddress will be filled
     *
     * @attention The Buffer is returned and the caller is responsible for buffer deletion
     */
    vke::GPUBuffer
    createBuffer(const uint64_t size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo& allocInfo);

    /**
     * @brief Creates a staging buffer
     *
     * @attention The Buffer is returned and the caller is responsible for buffer deletion
     */
    vke::GPUBuffer createStagingBuffer(const uint64_t size);

    /**
     * @brief Creates a mega buffer
     *
     * @attention The Buffer is returned and the caller is responsible for buffer deletion
     */
    vke::GPUMegaBuffer createMegaGPUBuffer(const uint64_t capacity, VkBufferUsageFlags usage);

    /**
     * @brief Copies bytes to staging buffer using VMA
     */
    void
    copyToStagingBuffer(std::span<const std::byte> bytes, const uint64_t copyOffset, vke::GPUBuffer& stagingBuffer);

    /**
     * @brief Records commands to copy image to GPU
     *
     * Final image layout is SHADER_READ_ONLY_OPTIMAL (ready to sample in shader)
     *
     * @param cmdBuff must be in recording state
     *
     * @attention First barrier layout is UNDEFINED so all original content is lost
     */
    void
    recordImgCopyCmds(VkCommandBuffer cmdBuff, VkBuffer vkStagingBuff, const VkExtent3D& imgExtent, VkImage gpuVkImage);

    /**
     * @brief Records copy to vertex and index data to both GPU mega buffers
     *
     * Vertices can be read from the vertex shader
     * Indices ready to be bound as index buffer
     *
     * @param cmdBuff must be in recording state
     *
     * @attention Sizes must be allocated in virtual blocks (so both regions have the correct src and dst offsets)
     * @attention Regions are in bytes
     */
    void recordMeshCopyCmds(
        VkCommandBuffer cmdBuff, VkBuffer vkStagingBuff, const VkBufferCopy& vertCopyRegion,
        const VkBufferCopy& indexCopyRegion
    );

    /**
     * @brief Records commands with @p f, submits them to the graphics queue and waits for completion.
     *
     * On return, all recorded commands have finished on the GPU (staging buffers can be destroyed)
     *
     * @param f Called once with a command buffer already in the recording state.
     *          Do not begin/end it, and do not keep it after @p f returns.
     *
     * @attention Blocks the CPU until the GPU is idle
     */
    template <typename F>
        requires std::invocable<F, VkCommandBuffer>
    void immediateSubmit(F&& f)
    {
        const auto device = m_ctx->device();
        const auto graphicsQueue = m_ctx->graphicsQueue();

        // create transient command buff
        VkCommandBufferAllocateInfo transientBuffInfo =
            vke::cmdBufferAllocInfo(m_transientBuffCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

        VkCommandBuffer transientCmdBuff = VK_NULL_HANDLE;
        vke::check(
            vkAllocateCommandBuffers(device, &transientBuffInfo, &transientCmdBuff),
            "[GPUResources] Failed to create transient command buffer"
        );

        // begin record
        VkCommandBufferBeginInfo transientBuffBegin =
            vke::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        vke::check(
            vkBeginCommandBuffer(transientCmdBuff, &transientBuffBegin),
            "[GPUResources] Failed to begin mesh transient command buffer"
        );

        // F holds commands to records
        f(transientCmdBuff);

        vke::check(vkEndCommandBuffer(transientCmdBuff), "[GPUResources] Failed to end mesh transient command buffer");

        // submit
        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &transientCmdBuff,
        };

        vke::check(
            vkQueueSubmit(graphicsQueue, 1, &submitInfo, nullptr),
            "[GPUResources] Failed to submit transient command buffer to the queue"
        );
        vkQueueWaitIdle(graphicsQueue); // bad but for now ok

        vkFreeCommandBuffers(device, m_transientBuffCmdPool, 1, &transientCmdBuff);
    }

    Pool<vke::GPUImage> m_gpuImgs;
    Pool<vke::GPUBuffer> m_gpuBuffs;
    Pool<vke::GPUMesh> m_gpuMeshes;
    std::vector<VkSampler> m_samplers;

    const uint64_t vertexMegaBuffCapacity{50000000};
    vke::GPUMegaBuffer m_vertexMegaBuff{};

    const uint64_t indexMegaBuffCapacity{1000000};
    vke::GPUMegaBuffer m_indexMegaBuff{};

    const uint32_t m_maxTextureCount{1024};
    const uint32_t m_maxSamplerCount{16};

    VkDescriptorPool m_descPool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_descSetLayout{VK_NULL_HANDLE};
    VkDescriptorSet m_descSet{VK_NULL_HANDLE};

    VkCommandPool m_transientBuffCmdPool{VK_NULL_HANDLE};
    VulkanCtx* m_ctx{nullptr};
};
