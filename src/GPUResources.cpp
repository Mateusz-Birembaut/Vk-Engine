#include "VkEngine/GPUResources.h"

#include <vulkan/vulkan.h>

#include <array>
#include <stdexcept>
#include <vk_mem_alloc.h>

#include "VkEngine/VkInits.h"

void GPUResources::init(VulkanCtx* pVulkanCtx)
{
    if (pVulkanCtx == nullptr)
        throw std::invalid_argument("[GPUResources] Init : context must not be null");

    m_ctx = pVulkanCtx;

    const auto device = m_ctx->device();

    // setup command pool for transfers
    VkCommandPoolCreateInfo transientBuffCmdPoolInfo =
        VkEngine::cmdPoolCreateInfo(m_ctx->graphicsQueueFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    if (vkCreateCommandPool(device, &transientBuffCmdPoolInfo, nullptr, &m_transientBuffCmdPool) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Init : Failed to create command pool for transient buffers");
    }

    std::array<VkDescriptorPoolSize, 2> poolSizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = m_maxSamplerCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = m_maxTextureCount,
        }
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descPool) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to create descriptor pool");
    }

    std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = m_maxSamplerCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = m_maxTextureCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };

    std::array<VkDescriptorBindingFlags, 2> flags{
        VkDescriptorBindingFlags{
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        },
        {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT},
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(flags.size()),
        .pBindingFlags = flags.data(),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flagsInfo,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
        .pBindings = layoutBindings.data(),
    };

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to create descriptor set layout");
    }

    VkDescriptorSetAllocateInfo descSetAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descSetLayout,
    };

    if (vkAllocateDescriptorSets(device, &descSetAllocInfo, &m_descSet) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to allocate descriptor set");
    }
}

VkEngine::GPUBuffer GPUResources::createStagingBuffer(const uint64_t size)
{
    VkEngine::GPUBuffer stagingBuffer;
    VkBufferCreateInfo stagingBufferCreateInfo = VkEngine::bufferCreateInfo(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    VmaAllocationCreateInfo stagingBufferAllocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST, // prefer_host for allocation in ram
    };

    if (vmaCreateBuffer(
            m_ctx->vmaAllocator(), &stagingBufferCreateInfo, &stagingBufferAllocInfo, &stagingBuffer.vkbuffer,
            &stagingBuffer.allocation, nullptr
        ) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to create staging buffer");
    }

    return stagingBuffer;
}

Handle<VkEngine::GPUImage> GPUResources::createTexture(const VkEngine::CPUImage& cpuImg)
{
    // image and view creation
    VkImageCreateInfo whiteTextureCreateInfo = VkEngine::img2DCreateInfo(cpuImg.width, cpuImg.height);

    VkEngine::GPUImage gpuImage;

    VmaAllocationCreateInfo imgAllocInfo{.usage = VMA_MEMORY_USAGE_AUTO};

    if (vmaCreateImage(
            m_ctx->vmaAllocator(), &whiteTextureCreateInfo, &imgAllocInfo, &gpuImage.vkImage, &gpuImage.allocation,
            nullptr
        ) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] VMA Failed to create image");
    }

    VkImageViewCreateInfo whiteTextureViewCreateInfo = VkEngine::imgView2DCreateInfo(gpuImage.vkImage);

    if (vkCreateImageView(m_ctx->device(), &whiteTextureViewCreateInfo, nullptr, &gpuImage.vkImageView) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to create image view for white texture");
    }

    // create transient command buff
    VkCommandBufferAllocateInfo transientBuffInfo =
        VkEngine::cmdBufferAllocInfo(m_transientBuffCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

    VkCommandBuffer transientCmdBuff = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_ctx->device(), &transientBuffInfo, &transientCmdBuff) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to create transient command buffer");
    }

    // staging buffer to hold texture data
    const size_t byteSize = cpuImg.width * cpuImg.height * cpuImg.channels;
    VkEngine::GPUBuffer stagingBuffer = createStagingBuffer(byteSize);

    copyImageToStagingBuffer(cpuImg, stagingBuffer);

    recordTransientCommandBuffer(transientCmdBuff, stagingBuffer, cpuImg, gpuImage);

    submitTransientCommandBuffer(transientCmdBuff); // submit and wait

    vmaDestroyBuffer(
        m_ctx->vmaAllocator(), stagingBuffer.vkbuffer, stagingBuffer.allocation
    ); // safe to destroy since waited before

    return m_gpuImgs.create(std::move(gpuImage));
}

uint32_t GPUResources::getOrCreateSampler()
{
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .compareEnable = VK_FALSE,
    };

    VkSampler sampler = nullptr;
    if (vkCreateSampler(m_ctx->device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to create sampler");
    };

    uint32_t samplerIndex = static_cast<uint32_t>(m_samplers.size());
    m_samplers.push_back(sampler);

    return samplerIndex;
}

void GPUResources::copyImageToStagingBuffer(const VkEngine::CPUImage& cpuImg, VkEngine::GPUBuffer& stagingBuffer)
{
    // map so cpu can write texture data in the buffer
    void* buffPtr = nullptr;
    if (vmaMapMemory(m_ctx->vmaAllocator(), stagingBuffer.allocation, &buffPtr) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to map staging buffer");
    }

    // write data
    const size_t byteSize = cpuImg.width * cpuImg.height * cpuImg.channels;
    std::memcpy(buffPtr, cpuImg.data, byteSize); // cast if offset needed

    vmaUnmapMemory(m_ctx->vmaAllocator(), stagingBuffer.allocation);
}

void GPUResources::recordTransientCommandBuffer(
    VkCommandBuffer& transientCmdBuff, VkEngine::GPUBuffer& stagingBuffer, const VkEngine::CPUImage& cpuImg,
    const VkEngine::GPUImage& gpuImage
)
{
    // begin recording
    VkCommandBufferBeginInfo transientBuffBegin =
        VkEngine::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (vkBeginCommandBuffer(transientCmdBuff, &transientBuffBegin) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to begin transient command buffer");
    }

    // wait for transition undefined image to transfer dst
    VkImageMemoryBarrier2 transferBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = gpuImage.vkImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkDependencyInfo transferDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &transferBarrier,
    };

    vkCmdPipelineBarrier2(transientCmdBuff, &transferDependencyInfo);

    // command to move to device buffer (vram)
    VkBufferImageCopy bufferImageCopy{
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageExtent = {
            .width = static_cast<uint32_t>(cpuImg.width), .height = static_cast<uint32_t>(cpuImg.height), .depth = 1
        },
    };

    vkCmdCopyBufferToImage(
        transientCmdBuff, stagingBuffer.vkbuffer, gpuImage.vkImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL /* current image layout */, 1, &bufferImageCopy
    );

    // wait for transition from transfer dst to shader read
    VkImageMemoryBarrier2 shaderSampleBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = gpuImage.vkImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkDependencyInfo shaderSampleDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &shaderSampleBarrier,
    };

    vkCmdPipelineBarrier2(transientCmdBuff, &shaderSampleDependencyInfo);

    if (vkEndCommandBuffer(transientCmdBuff) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to end transient command buffer");
    } // commands recorded
}

void GPUResources::submitTransientCommandBuffer(VkCommandBuffer& transientCmdBuff)
{
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &transientCmdBuff,
    };

    if (vkQueueSubmit(m_ctx->graphicsQueue(), 1, &submitInfo, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("[GPUResources] Failed to submit transient command buffer to the queue");
    }
    vkQueueWaitIdle(m_ctx->graphicsQueue()); // bad but for now ok
    vkFreeCommandBuffers(m_ctx->device(), m_transientBuffCmdPool, 1, &transientCmdBuff);
}

void GPUResources::cleanup()
{
    if (!m_ctx)
        return;

    const auto device = m_ctx->device();
    const auto vmaAllocator = m_ctx->vmaAllocator();

    vkDestroyCommandPool(device, m_transientBuffCmdPool, nullptr);

    for (auto& sampler : m_samplers) {
        vkDestroySampler(device, sampler, nullptr);
    }

    m_gpuImgs.releaseAllAlive([this, device, vmaAllocator](VkEngine::GPUImage& gpuImage) {
        vkDestroyImageView(device, gpuImage.vkImageView, nullptr);
        vmaDestroyImage(vmaAllocator, gpuImage.vkImage, gpuImage.allocation);
    });

    m_gpuBuffs.releaseAllAlive([this, vmaAllocator](VkEngine::GPUBuffer& gpuBuff) {
        vmaDestroyBuffer(vmaAllocator, gpuBuff.vkbuffer, gpuBuff.allocation);
    });

    vkDestroyDescriptorSetLayout(device, m_descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, m_descPool, nullptr);
}
