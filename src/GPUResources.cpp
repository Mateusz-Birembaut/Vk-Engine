#include "VkEngine/GPUResources.h"

#include <vulkan/vulkan.h>

#include <array>
#include <stdexcept>
#include <vk_mem_alloc.h>

#include "VkEngine/Helpers.h"
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

    VkEngine::check(
        vkCreateCommandPool(device, &transientBuffCmdPoolInfo, nullptr, &m_transientBuffCmdPool),
        "[GPUResources] Init : Failed to create command pool for transient buffers"
    );

    initDescriptorPool(device);
    initDescriptorSetLayout(device);
    initDescriptorSet(device);

    m_vertexMegaBuff = createMegaGPUBuffer(
        vertexMegaBuffCapacity, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );

    m_indexMegaBuff =
        createMegaGPUBuffer(indexMegaBuffCapacity, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void GPUResources::initDescriptorPool(VkDevice device)
{
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

    VkEngine::check(
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descPool),
        "[GPUResources] Failed to create descriptor pool"
    );
}

void GPUResources::initDescriptorSetLayout(VkDevice device)
{
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

    VkEngine::check(
        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descSetLayout),
        "[GPUResources] Failed to create descriptor set layout"
    );
}

void GPUResources::initDescriptorSet(VkDevice device)
{
    VkDescriptorSetAllocateInfo descSetAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_descPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_descSetLayout,
    };

    VkEngine::check(
        vkAllocateDescriptorSets(device, &descSetAllocInfo, &m_descSet),
        "[GPUResources] Failed to allocate descriptor set"
    );
}

VkEngine::GPUBuffer
GPUResources::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo& allocInfo)
{
    VkEngine::GPUBuffer gpuBuff;
    VkBufferCreateInfo stagingBufferCreateInfo = VkEngine::bufferCreateInfo(size, usage);

    VkEngine::check(
        vmaCreateBuffer(
            m_ctx->vmaAllocator(), &stagingBufferCreateInfo, &allocInfo, &gpuBuff.vkbuffer, &gpuBuff.allocation, nullptr
        ),
        "[GPUResources] Failed to create buffer"
    );

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfoKHR address_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
        address_info.buffer = gpuBuff.vkbuffer;

        gpuBuff.deviceAddress = vkGetBufferDeviceAddress(m_ctx->device(), &address_info);
    }

    return gpuBuff;
}

VkEngine::GPUBuffer GPUResources::createStagingBuffer(const uint64_t size)
{
    VmaAllocationCreateInfo stagingBufferAllocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST, // prefer_host for allocation in ram
    };

    return createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBufferAllocInfo);
}

VkEngine::GPUMegaBuffer GPUResources::createMegaGPUBuffer(const uint64_t capacity, VkBufferUsageFlags usage)
{
    VkEngine::GPUMegaBuffer megaBuff;

    VmaAllocationCreateInfo megaBuffVmaAllocInfo{.usage = VMA_MEMORY_USAGE_AUTO};

    megaBuff.gpuBuffer = createBuffer(capacity, usage, megaBuffVmaAllocInfo);

    VmaVirtualBlockCreateInfo megaBuffBlockInfo{.size = capacity};
    VkEngine::check(
        vmaCreateVirtualBlock(&megaBuffBlockInfo, &megaBuff.block),
        "[GPUResources] Failed to create virtual block for vertex Mega Buffer"
    );

    return megaBuff;
}

Handle<VkEngine::GPUImage> GPUResources::createGpuImage(const VkEngine::CPUImage& cpuImg)
{
    const auto device = m_ctx->device();

    // image and view creation
    VkImageCreateInfo whiteTextureCreateInfo = VkEngine::img2DCreateInfo(cpuImg.width, cpuImg.height);

    VkEngine::GPUImage gpuImage;

    VmaAllocationCreateInfo imgAllocInfo{.usage = VMA_MEMORY_USAGE_AUTO};

    VkEngine::check(
        vmaCreateImage(
            m_ctx->vmaAllocator(), &whiteTextureCreateInfo, &imgAllocInfo, &gpuImage.vkImage, &gpuImage.allocation,
            nullptr
        ),
        "[GPUResources] VMA Failed to create image"
    );

    VkImageViewCreateInfo whiteTextureViewCreateInfo = VkEngine::imgView2DCreateInfo(gpuImage.vkImage);

    VkEngine::check(
        vkCreateImageView(device, &whiteTextureViewCreateInfo, nullptr, &gpuImage.vkImageView),
        "[GPUResources] Failed to create image view for white texture"
    );

    // create transient command buff
    VkCommandBufferAllocateInfo transientBuffInfo =
        VkEngine::cmdBufferAllocInfo(m_transientBuffCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

    VkCommandBuffer transientCmdBuff = VK_NULL_HANDLE;
    VkEngine::check(
        vkAllocateCommandBuffers(device, &transientBuffInfo, &transientCmdBuff),
        "[GPUResources] Failed to create transient command buffer"
    );

    // staging buffer to hold texture data
    VkEngine::GPUBuffer stagingBuffer = createStagingBuffer(cpuImg.sizeBytes());

    copyToStagingBuffer(cpuImg.bytes(), 0, stagingBuffer);

    recordTransientCommandBuffer(transientCmdBuff, stagingBuffer, cpuImg, gpuImage);

    submitTransientCommandBuffer(transientCmdBuff); // submit and wait

    vmaDestroyBuffer(
        m_ctx->vmaAllocator(), stagingBuffer.vkbuffer, stagingBuffer.allocation
    ); // safe to destroy since waited before

    VkDescriptorImageInfo descImageInfo{
        .imageView = gpuImage.vkImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet descSetWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_descSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &descImageInfo,
    };

    vkUpdateDescriptorSets(device, 1, &descSetWrite, 0, nullptr);

    return m_gpuImgs.create(std::move(gpuImage));
}

uint32_t GPUResources::getOrCreateSampler()
{
    const auto device = m_ctx->device();

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
    VkEngine::check(
        vkCreateSampler(device, &samplerInfo, nullptr, &sampler), "[GPUResources] Failed to create sampler"
    );

    uint32_t samplerIndex = static_cast<uint32_t>(m_samplers.size());
    m_samplers.push_back(sampler);

    VkDescriptorImageInfo descImageInfo{.sampler = sampler};

    VkWriteDescriptorSet descSetWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_descSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &descImageInfo,
    };

    vkUpdateDescriptorSets(device, 1, &descSetWrite, 0, nullptr);

    return samplerIndex;
}

void GPUResources::copyToStagingBuffer(
    std::span<const std::byte> bytes, const uint64_t copyOffset, VkEngine::GPUBuffer& stagingBuffer
)
{
    VkEngine::check(
        vmaCopyMemoryToAllocation(
            m_ctx->vmaAllocator(), bytes.data(), stagingBuffer.allocation, copyOffset, bytes.size_bytes()
        ),
        "[GPUResources] Failed to copy memory to staging buffer"
    );
}

void GPUResources::recordTransientCommandBuffer(
    VkCommandBuffer& transientCmdBuff, VkEngine::GPUBuffer& stagingBuffer, const VkEngine::CPUImage& cpuImg,
    const VkEngine::GPUImage& gpuImage
)
{
    // begin recording
    VkCommandBufferBeginInfo transientBuffBegin =
        VkEngine::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkEngine::check(
        vkBeginCommandBuffer(transientCmdBuff, &transientBuffBegin),
        "[GPUResources] Failed to begin transient command buffer"
    );

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

    VkEngine::check(vkEndCommandBuffer(transientCmdBuff), "[GPUResources] Failed to end transient command buffer");
}

void GPUResources::submitTransientCommandBuffer(VkCommandBuffer& transientCmdBuff)
{
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &transientCmdBuff,
    };

    VkEngine::check(
        vkQueueSubmit(m_ctx->graphicsQueue(), 1, &submitInfo, nullptr),
        "[GPUResources] Failed to submit transient command buffer to the queue"
    );
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

    vmaDestroyBuffer(vmaAllocator, m_vertexMegaBuff.gpuBuffer.vkbuffer, m_vertexMegaBuff.gpuBuffer.allocation);
    vmaDestroyBuffer(vmaAllocator, m_indexMegaBuff.gpuBuffer.vkbuffer, m_indexMegaBuff.gpuBuffer.allocation);

    vkDestroyDescriptorSetLayout(device, m_descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, m_descPool, nullptr);
}

VkDescriptorSetLayout GPUResources::descSetLayout() const
{
    return m_descSetLayout;
}

VkDescriptorSet GPUResources::descSet() const
{
    return m_descSet;
}

uint64_t GPUResources::vertBuffAddr() const
{
    return m_vertexMegaBuff.gpuBuffer.deviceAddress;
}

VkBuffer GPUResources::indexBuffer() const
{
    return m_indexMegaBuff.gpuBuffer.vkbuffer;
}

const VkEngine::GPUMesh* GPUResources::getMesh(const Handle<VkEngine::GPUMesh>& meshHandle) const
{
    return m_gpuMeshes.get(meshHandle);
}
