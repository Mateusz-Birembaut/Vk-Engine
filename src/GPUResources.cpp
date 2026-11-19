#include "VkEngine/GPUResources.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <vk_mem_alloc.h>

#include "VkEngine/Helpers.h"
#include "VkEngine/VkInits.h"

namespace vke {

void GPUResources::init(VulkanCtx* pVulkanCtx)
{
    if (pVulkanCtx == nullptr)
        throw std::invalid_argument("[GPUResources] Init : context must not be null");

    m_ctx = pVulkanCtx;

    const auto device = m_ctx->device();

    // setup command pool for transfers
    VkCommandPoolCreateInfo transientBuffCmdPoolInfo =
        cmdPoolCreateInfo(m_ctx->graphicsQueueFamily(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);

    check(
        vkCreateCommandPool(device, &transientBuffCmdPoolInfo, nullptr, &m_transientBuffCmdPool),
        "[GPUResources] Init : Failed to create command pool for transient buffers"
    );

    initDescriptorPool(device);
    initDescriptorSetLayout(device);
    initDescriptorSet(device);

    recreateDepthBuffer();

    m_vertexMegaBuff = createMegaGPUBuffer(
        vertexMegaBuffCapacity, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    );

    m_indexMegaBuff =
        createMegaGPUBuffer(indexMegaBuffCapacity, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    m_materialMegaBuffer = createBuffer(
        materialMegaBufferCapacity,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        {.usage = VMA_MEMORY_USAGE_AUTO}
    );
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

    check(
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

    check(
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

    check(
        vkAllocateDescriptorSets(device, &descSetAllocInfo, &m_descSet),
        "[GPUResources] Failed to allocate descriptor set"
    );
}

GPUBuffer GPUResources::createBuffer(
    const uint64_t size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo& allocCreateInfo
)
{
    GPUBuffer gpuBuff;
    VkBufferCreateInfo stagingBufferCreateInfo = bufferCreateInfo(size, usage);
    VmaAllocationInfo allocInfo;

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        check(
            vmaCreateBufferWithAlignment(
                m_ctx->vmaAllocator(), &stagingBufferCreateInfo, &allocCreateInfo,
                16, // buffer device address must be aligned to 16 bytes to be read by shader
                &gpuBuff.vkbuffer, &gpuBuff.allocation, &allocInfo
            ),
            "[GPUResources] Failed to create aligned buffer"
        );
        VkBufferDeviceAddressInfoKHR address_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR};
        address_info.buffer = gpuBuff.vkbuffer;

        gpuBuff.deviceAddress = vkGetBufferDeviceAddress(m_ctx->device(), &address_info);
    } else {
        check(
            vmaCreateBuffer(
                m_ctx->vmaAllocator(), &stagingBufferCreateInfo, &allocCreateInfo, &gpuBuff.vkbuffer,
                &gpuBuff.allocation, &allocInfo
            ),
            "[GPUResources] Failed to create buffer"
        );
    }

    if (allocCreateInfo.flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        gpuBuff.pMappedData = allocInfo.pMappedData;
    }

    return gpuBuff;
}

GPUBuffer GPUResources::createStagingBuffer(const uint64_t size)
{
    VmaAllocationCreateInfo stagingBufferAllocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST, // prefer_host for allocation in ram
    };

    return createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBufferAllocInfo);
}

Handle<GPUBuffer> GPUResources::createMappedBuffer(const uint64_t size)
{
    VmaAllocationCreateInfo stagingBufferAllocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                 VMA_ALLOCATION_CREATE_MAPPED_BIT, // keep memory mapped
        .usage = VMA_MEMORY_USAGE_AUTO,            // rebar usage possible
    };

    return m_gpuBuffs.create(createBuffer(
        size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, stagingBufferAllocInfo
    ));
}

GPUMegaBuffer GPUResources::createMegaGPUBuffer(const uint64_t capacity, VkBufferUsageFlags usage)
{
    GPUMegaBuffer megaBuff{.capacity = capacity};

    VmaAllocationCreateInfo megaBuffVmaAllocInfo{.usage = VMA_MEMORY_USAGE_AUTO};

    megaBuff.gpuBuffer = createBuffer(capacity, usage, megaBuffVmaAllocInfo);

    VmaVirtualBlockCreateInfo megaBuffBlockInfo{.size = capacity};
    check(
        vmaCreateVirtualBlock(&megaBuffBlockInfo, &megaBuff.block),
        "[GPUResources] Failed to create virtual block for vertex Mega Buffer"
    );

    return megaBuff;
}

GPUImage
GPUResources::create2dGpuImage(VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkImageAspectFlags aspect)
{
    const auto device = m_ctx->device();

    VkImageCreateInfo imCreateInfo = img2DCreateInfo(format, extent, usage);
    VmaAllocationCreateInfo imgAllocInfo{.usage = VMA_MEMORY_USAGE_AUTO};

    GPUImage gpuImage;

    check(
        vmaCreateImage(
            m_ctx->vmaAllocator(), &imCreateInfo, &imgAllocInfo, &gpuImage.vkImage, &gpuImage.allocation, nullptr
        ),
        "[GPUResources] VMA Failed to create image"
    );

    VkImageViewCreateInfo imgView = imgView2DCreateInfo(gpuImage.vkImage, format, aspect);

    check(
        vkCreateImageView(device, &imgView, nullptr, &gpuImage.vkImageView),
        "[GPUResources] Failed to create image view for white texture"
    );

    return gpuImage;
}

void GPUResources::recreateDepthBuffer()
{
    cleanupDepthBuffer();

    // TODO : check for best format
    m_depthBuffer = create2dGpuImage(
        VK_FORMAT_D32_SFLOAT, m_ctx->swapchainExtent(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
}

void GPUResources::cleanupDepthBuffer()
{
    if (m_depthBuffer.vkImage) {
        const auto device = m_ctx->device();
        const auto vmaAllocator = m_ctx->vmaAllocator();
        vkDestroyImageView(device, m_depthBuffer.vkImageView, nullptr);
        vmaDestroyImage(vmaAllocator, m_depthBuffer.vkImage, m_depthBuffer.allocation);
    }
}

Handle<GPUImage> GPUResources::createGpuImage(const ImageDesc& imgDesc)
{
    const auto device = m_ctx->device();

    VkExtent3D imgExtent{
        .width = static_cast<uint32_t>(imgDesc.width), .height = static_cast<uint32_t>(imgDesc.height), .depth = 1
    };

    VkFormat format = (imgDesc.colorSpace == ColorSpace::SRGB) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    // image and view creation
    // TODO : check for best format (only color space changes for now)
    GPUImage gpuImage = create2dGpuImage(
        format, {.width = imgExtent.width, .height = imgExtent.height},
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT
    );

    // staging buffer to hold texture data
    GPUBuffer stagingBuffer = createStagingBuffer(imgDesc.sizeBytes());

    copyToStagingBuffer(imgDesc.bytes(), 0, stagingBuffer);

    immediateSubmit([&](VkCommandBuffer transientCmdBuff) {
        recordImgCopyCmds(transientCmdBuff, stagingBuffer.vkbuffer, imgExtent, gpuImage.vkImage);
    });

    vmaDestroyBuffer(
        m_ctx->vmaAllocator(), stagingBuffer.vkbuffer, stagingBuffer.allocation
    ); // safe to destroy since waited before

    const auto imgView = gpuImage.vkImageView;
    const Handle<GPUImage> imgHandle = m_gpuImgs.create(std::move(gpuImage)); // add img to pool

    // update descriptor set at imgHandle index

    VkDescriptorImageInfo descImageInfo{
        .imageView = imgView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet descSetWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_descSet,
        .dstBinding = 1,
        .dstArrayElement = imgHandle.index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &descImageInfo,
    };

    vkUpdateDescriptorSets(device, 1, &descSetWrite, 0, nullptr);

    return imgHandle;
}

Handle<GPUMesh> GPUResources::createGpuMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices)
{
    GPUMesh gpuMesh;
    gpuMesh.indexCount = static_cast<uint32_t>(indices.size());

    // virtual allocate (checks for size available)
    VmaVirtualAllocationCreateInfo vertexVirtualAllocInfo{
        .size = vertices.size_bytes(),
    };

    uint64_t allocVertexOffset{}; // in byte
    check(
        vmaVirtualAllocate(
            m_vertexMegaBuff.block, &vertexVirtualAllocInfo, &gpuMesh.vAllocVertices, &allocVertexOffset
        ),
        "[GPUResources] Failed to allocate virtual memory for vertices"
    );

    // get the index from the offset in byte
    gpuMesh.baseVertex = static_cast<uint32_t>(allocVertexOffset / sizeof(Vertex));

    VmaVirtualAllocationCreateInfo indexVirtualAllocInfo{
        .size = indices.size() * sizeof(uint32_t),
    };

    uint64_t allocIndexOffset{}; // in byte
    check(
        vmaVirtualAllocate(m_indexMegaBuff.block, &indexVirtualAllocInfo, &gpuMesh.vAllocIndices, &allocIndexOffset),
        "[GPUResources] Failed to allocate virtual memory for indices"
    );
    gpuMesh.firstIndex = static_cast<uint32_t>(allocIndexOffset / sizeof(uint32_t));

    GPUBuffer stagingBuffer = createStagingBuffer(vertices.size_bytes() + indices.size_bytes());

    copyToStagingBuffer(std::as_bytes(vertices), 0, stagingBuffer);
    copyToStagingBuffer(std::as_bytes(indices), vertices.size_bytes(), stagingBuffer);

    VkBufferCopy vertexCopyRegion{
        .srcOffset = 0,
        .dstOffset = allocVertexOffset,
        .size = vertices.size_bytes(),
    };

    VkBufferCopy indexCopyRegion{
        .srcOffset = vertices.size_bytes(),
        .dstOffset = allocIndexOffset,
        .size = indices.size_bytes(),
    };

    immediateSubmit([&](VkCommandBuffer cmd) {
        recordMeshCopyCmds(cmd, stagingBuffer.vkbuffer, vertexCopyRegion, indexCopyRegion);
    });

    vmaDestroyBuffer(
        m_ctx->vmaAllocator(), stagingBuffer.vkbuffer, stagingBuffer.allocation
    ); // safe to destroy since waited before

    return m_gpuMeshes.create(std::move(gpuMesh));
}

Handle<GPUMaterial> GPUResources::createMaterial(const MaterialDesc& matInfo)
{
    GPUMaterial gpuMat{
        .baseColorFactor = matInfo.baseColorFactor,
        .metallicFactor = matInfo.metallicFactor,
        .roughnessFactor = matInfo.roughnessFactor,
    };

    if (m_gpuImgs.get(matInfo.metallicRoughnessTexture.texture.imageHandle)) {
        gpuMat.metallicRoughnessTexture.imageIndex = matInfo.metallicRoughnessTexture.texture.imageHandle.index;
        gpuMat.metallicRoughnessTexture.samplerIndex =
            static_cast<uint16_t>(matInfo.metallicRoughnessTexture.texture.samplerIndex);
        gpuMat.metallicRoughnessTexture.transform = matInfo.metallicRoughnessTexture.transform;
    }

    if (m_gpuImgs.get(matInfo.baseColorTexture.texture.imageHandle)) {
        gpuMat.baseColorTexture.imageIndex = matInfo.baseColorTexture.texture.imageHandle.index;
        gpuMat.baseColorTexture.samplerIndex = static_cast<uint16_t>(matInfo.baseColorTexture.texture.samplerIndex);
        gpuMat.baseColorTexture.transform = matInfo.baseColorTexture.transform;
    }

    if (m_gpuImgs.get(matInfo.normalTexture.texture.imageHandle)) {
        gpuMat.normalTexture.imageIndex = matInfo.normalTexture.texture.imageHandle.index;
        gpuMat.normalTexture.samplerIndex = static_cast<uint16_t>(matInfo.normalTexture.texture.samplerIndex);
        gpuMat.normalTexture.transform = matInfo.normalTexture.transform;
    }

    auto bytes = std::as_bytes(std::span<GPUMaterial>{&gpuMat, 1});

    GPUBuffer stagingBuffer = createStagingBuffer(bytes.size_bytes());

    copyToStagingBuffer(bytes, 0, stagingBuffer);

    // create mat handle here to know at which index to write its data
    const auto materialHandle = m_gpuMaterials.create(std::move(gpuMat));

    VkBufferCopy materialCopyRegion{
        .srcOffset = 0,
        .dstOffset = sizeof(GPUMaterial) * materialHandle.index,
        .size = bytes.size_bytes(),
    };

    immediateSubmit([&](VkCommandBuffer transientCmdBuff) {
        vkCmdCopyBuffer(
            transientCmdBuff, stagingBuffer.vkbuffer, m_materialMegaBuffer.vkbuffer, 1, &materialCopyRegion
        );

        VkBufferMemoryBarrier2 transferBarrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = m_materialMegaBuffer.vkbuffer,
            .offset = materialCopyRegion.dstOffset,
            .size = materialCopyRegion.size,
        };

        VkDependencyInfo transferDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &transferBarrier,
        };

        vkCmdPipelineBarrier2(transientCmdBuff, &transferDependencyInfo);
    });

    vmaDestroyBuffer(
        m_ctx->vmaAllocator(), stagingBuffer.vkbuffer, stagingBuffer.allocation
    ); // safe to destroy since waited before

    return materialHandle;
}

uint32_t GPUResources::getOrCreateSampler(const Sampler& samplerDesc)
{
    for (uint32_t ISampler = 0; ISampler < m_gpuSamplers.size(); ISampler++) {
        if (m_gpuSamplers[ISampler].samplerDesc == samplerDesc) {
            return ISampler;
        }
    }

    uint32_t newSamplerIndex = static_cast<uint32_t>(m_gpuSamplers.size());

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filterModeFrom(samplerDesc.magFilter),
        .minFilter = filterModeFrom(samplerDesc.minFilter),
        .addressModeU = wrapModeFrom(samplerDesc.wrapS),
        .addressModeV = wrapModeFrom(samplerDesc.wrapT),
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .compareEnable = VK_FALSE,
    };

    const auto device = m_ctx->device();

    VkSampler vkSampler = nullptr;
    check(vkCreateSampler(device, &samplerInfo, nullptr, &vkSampler), "[GPUResources] Failed to create sampler");

    VkDescriptorImageInfo descImageInfo{.sampler = vkSampler};

    VkWriteDescriptorSet descSetWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = m_descSet,
        .dstBinding = 0,
        .dstArrayElement = newSamplerIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &descImageInfo,
    };

    vkUpdateDescriptorSets(device, 1, &descSetWrite, 0, nullptr);

    m_gpuSamplers.push_back({vkSampler, std::move(samplerDesc)});

    return newSamplerIndex;
}

void GPUResources::copyToStagingBuffer(
    std::span<const std::byte> bytes, const uint64_t copyOffset, GPUBuffer& stagingBuffer
)
{
    check(
        vmaCopyMemoryToAllocation(
            m_ctx->vmaAllocator(), bytes.data(), stagingBuffer.allocation, copyOffset, bytes.size_bytes()
        ),
        "[GPUResources] Failed to copy memory to staging buffer"
    );
}

void GPUResources::recordImgCopyCmds(
    VkCommandBuffer transientCmdBuff, VkBuffer vkStagingBuff, const VkExtent3D& imgExtent, VkImage gpuVkImage
)
{
    // wait for transition undefined image to transfer dst
    VkImageMemoryBarrier2 transferBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = gpuVkImage,
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
        .imageExtent = imgExtent,
    };

    vkCmdCopyBufferToImage(
        transientCmdBuff, vkStagingBuff, gpuVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL /* current image layout */, 1,
        &bufferImageCopy
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
        .image = gpuVkImage,
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
}

void GPUResources::recordMeshCopyCmds(
    VkCommandBuffer transientCmdBuff, VkBuffer vkStagingBuff, const VkBufferCopy& vertCopyRegion,
    const VkBufferCopy& indexCopyRegion
)
{
    vkCmdCopyBuffer(transientCmdBuff, vkStagingBuff, m_vertexMegaBuff.gpuBuffer.vkbuffer, 1, &vertCopyRegion);

    vkCmdCopyBuffer(transientCmdBuff, vkStagingBuff, m_indexMegaBuff.gpuBuffer.vkbuffer, 1, &indexCopyRegion);

    std::array<VkBufferMemoryBarrier2, 2> barriers{
        VkBufferMemoryBarrier2{
            // vertex mega buffer barrier
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = m_vertexMegaBuff.gpuBuffer.vkbuffer,
            .offset = vertCopyRegion.dstOffset,
            .size = vertCopyRegion.size,
        },
        {
            // index mega buffer barrier
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = m_indexMegaBuff.gpuBuffer.vkbuffer,
            .offset = indexCopyRegion.dstOffset,
            .size = indexCopyRegion.size,
        },
    };

    VkDependencyInfo transferDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pBufferMemoryBarriers = barriers.data(),
    };

    vkCmdPipelineBarrier2(transientCmdBuff, &transferDependencyInfo);
}

VkSamplerAddressMode GPUResources::wrapModeFrom(WrapMode wrapMode)
{
    switch (wrapMode) {
        case WrapMode::CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case WrapMode::MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case WrapMode::REPEAT:
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkFilter GPUResources::filterModeFrom(FilterMode filterMode)
{
    switch (filterMode) {
        case FilterMode::NEAREST:
            return VK_FILTER_NEAREST;
        case FilterMode::LINEAR:
        default:
            return VK_FILTER_LINEAR;
    }
}

void GPUResources::cleanup()
{
    if (!m_ctx)
        return;

    const auto device = m_ctx->device();
    const auto vmaAllocator = m_ctx->vmaAllocator();
    const auto vertexBlock = m_vertexMegaBuff.block;
    const auto indexBlock = m_indexMegaBuff.block;

    vkDestroyCommandPool(device, m_transientBuffCmdPool, nullptr);

    for (auto& sampler : m_gpuSamplers) {
        vkDestroySampler(device, sampler.vkSampler, nullptr);
    }

    m_gpuImgs.releaseAllAlive([this, device, vmaAllocator](GPUImage& gpuImage) {
        vkDestroyImageView(device, gpuImage.vkImageView, nullptr);
        vmaDestroyImage(vmaAllocator, gpuImage.vkImage, gpuImage.allocation);
    });

    m_gpuBuffs.releaseAllAlive([this, vmaAllocator](GPUBuffer& gpuBuff) {
        vmaDestroyBuffer(vmaAllocator, gpuBuff.vkbuffer, gpuBuff.allocation);
    });

    m_gpuMeshes.releaseAllAlive([this, vertexBlock, indexBlock](GPUMesh& gpuMesh) {
        vmaVirtualFree(vertexBlock, gpuMesh.vAllocVertices);
        vmaVirtualFree(indexBlock, gpuMesh.vAllocIndices);
    });

    vmaDestroyVirtualBlock(vertexBlock);
    vmaDestroyVirtualBlock(indexBlock);

    vmaDestroyBuffer(vmaAllocator, m_vertexMegaBuff.gpuBuffer.vkbuffer, m_vertexMegaBuff.gpuBuffer.allocation);
    vmaDestroyBuffer(vmaAllocator, m_indexMegaBuff.gpuBuffer.vkbuffer, m_indexMegaBuff.gpuBuffer.allocation);

    vmaDestroyBuffer(vmaAllocator, m_materialMegaBuffer.vkbuffer, m_materialMegaBuffer.allocation);

    vkDestroyDescriptorSetLayout(device, m_descSetLayout, nullptr);
    vkDestroyDescriptorPool(device, m_descPool, nullptr);

    cleanupDepthBuffer();
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

uint64_t GPUResources::matBuffAddr() const
{
    return m_materialMegaBuffer.deviceAddress;
}

VkBuffer GPUResources::indexBuffer() const
{
    return m_indexMegaBuff.gpuBuffer.vkbuffer;
}

const GPUMesh* GPUResources::getMesh(const Handle<GPUMesh>& meshHandle) const
{
    return m_gpuMeshes.get(meshHandle);
}

const GPUBuffer* GPUResources::getBuffer(const Handle<GPUBuffer>& buffHandle) const
{
    return m_gpuBuffs.get(buffHandle);
}

const GPUImage* GPUResources::getDepthBuffer() const
{
    return &m_depthBuffer;
}

} // namespace vke