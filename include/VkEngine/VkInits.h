#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace VkEngine {

inline VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0)
{
    VkSemaphoreCreateInfo semInfo{};
    semInfo.flags = flags;
    semInfo.pNext = nullptr;
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    return semInfo;
}

inline VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.flags = flags;
    fenceInfo.pNext = nullptr;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    return fenceInfo;
}

inline VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = flags;
    beginInfo.pNext = nullptr;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    return beginInfo;
}

inline VkCommandPoolCreateInfo cmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.flags = flags;
    cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
    cmdPoolInfo.pNext = nullptr;
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    return cmdPoolInfo;
}

inline VkSemaphoreSubmitInfo semSubmitInfo(VkSemaphore sem, VkPipelineStageFlags2 stageMask)
{
    VkSemaphoreSubmitInfo semSubmitInfo{};
    semSubmitInfo.semaphore = sem;
    semSubmitInfo.stageMask = stageMask;
    semSubmitInfo.deviceIndex = 0;
    semSubmitInfo.value = 1;
    semSubmitInfo.pNext = nullptr;
    semSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;

    return semSubmitInfo;
}

inline VkCommandBufferSubmitInfo cmdBuffSubmitInfo(VkCommandBuffer commandBuffer)
{
    VkCommandBufferSubmitInfo cmdBuffSubmitInfo{};
    cmdBuffSubmitInfo.commandBuffer = commandBuffer;
    cmdBuffSubmitInfo.deviceMask = 0;
    cmdBuffSubmitInfo.pNext = nullptr;
    cmdBuffSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

    return cmdBuffSubmitInfo;
}

inline VkCommandBufferAllocateInfo
cmdBufferAllocInfo(VkCommandPool cmdPool, VkCommandBufferLevel lvl, uint32_t bufferCount)
{
    VkCommandBufferAllocateInfo cmdBuffInfo{};
    cmdBuffInfo.commandPool = cmdPool;
    cmdBuffInfo.level = lvl;
    cmdBuffInfo.commandBufferCount = bufferCount;
    cmdBuffInfo.pNext = nullptr;
    cmdBuffInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    return cmdBuffInfo;
}

inline VkSubmitInfo2 submitInfo(
    VkCommandBufferSubmitInfo* cmdBufferSubmitInfo, VkSemaphoreSubmitInfo* semWaitInfo,
    VkSemaphoreSubmitInfo* semSignalInfo
)
{
    VkSubmitInfo2 submitInfo{};
    submitInfo.commandBufferInfoCount = (cmdBufferSubmitInfo) ? 1 : 0;
    submitInfo.pCommandBufferInfos = cmdBufferSubmitInfo;
    submitInfo.waitSemaphoreInfoCount = (semWaitInfo) ? 1 : 0;
    submitInfo.pWaitSemaphoreInfos = semWaitInfo;
    submitInfo.signalSemaphoreInfoCount = (semSignalInfo) ? 1 : 0;
    submitInfo.pSignalSemaphoreInfos = semSignalInfo;
    submitInfo.pNext = nullptr;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

    return submitInfo;
}

inline VkPresentInfoKHR presentInfo(VkSwapchainKHR* swapchain, uint32_t* imgId, VkSemaphore* waitSem)
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.pImageIndices = imgId;
    presentInfo.swapchainCount = (swapchain) ? 1 : 0;
    presentInfo.pSwapchains = swapchain;
    presentInfo.waitSemaphoreCount = (waitSem) ? 1 : 0;
    presentInfo.pWaitSemaphores = waitSem;
    presentInfo.pResults = nullptr;
    presentInfo.pNext = nullptr;
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    return presentInfo;
}

inline VkImageSubresourceRange imgSubresourceRange(VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange subImage{};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return subImage;
}

inline VkDependencyInfo dependencyInfo(VkImageMemoryBarrier2* imgMemBarrier)
{
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = imgMemBarrier;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    return dependencyInfo;
}

inline VkImageMemoryBarrier2 imageMemoryBarrier(
    VkImage img, VkImageSubresourceRange imgSubRessource, VkImageLayout oldLayout, VkPipelineStageFlags2 srcStage,
    VkAccessFlags2 srcAccess, VkImageLayout newLayout, VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess
)
{
    VkImageMemoryBarrier2 imgMemBarrier{};

    imgMemBarrier.image = img;
    imgMemBarrier.subresourceRange = imgSubRessource;

    imgMemBarrier.oldLayout = oldLayout;
    imgMemBarrier.srcStageMask = srcStage;
    imgMemBarrier.srcAccessMask = srcAccess;
    imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imgMemBarrier.newLayout = newLayout;
    imgMemBarrier.dstStageMask = dstStage;
    imgMemBarrier.dstAccessMask = dstAccess;
    imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imgMemBarrier.pNext = nullptr;
    imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    return imgMemBarrier;
}

inline VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo(VkFormat* colorFormat)
{
    VkPipelineRenderingCreateInfo info{};
    info.colorAttachmentCount = 1;
    info.pColorAttachmentFormats = colorFormat;
    info.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    info.viewMask = 0;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    return info;
}

inline VkShaderModuleCreateInfo shaderModuleCreateInfo(size_t codeSize, const uint32_t* code)
{
    VkShaderModuleCreateInfo info{};
    info.codeSize = codeSize;
    info.pCode = code;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    return info;
}

inline VkPipelineShaderStageCreateInfo
shaderStageCreateInfo(VkShaderStageFlagBits shaderStage, VkShaderModuleCreateInfo* shaderModuleInfo)
{
    VkPipelineShaderStageCreateInfo info{};
    info.flags = 0;
    info.stage = shaderStage;
    info.module = VK_NULL_HANDLE;
    info.pName = "main";
    info.pSpecializationInfo = nullptr;

    info.pNext = shaderModuleInfo;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    return info;
}

inline VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo()
{
    VkPipelineVertexInputStateCreateInfo info{};
    info.vertexAttributeDescriptionCount = 0;
    info.vertexBindingDescriptionCount = 0;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    return info;
}

inline VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo(VkPrimitiveTopology primitiveTopology)
{
    VkPipelineInputAssemblyStateCreateInfo info{};
    info.primitiveRestartEnable = VK_FALSE;
    info.topology = primitiveTopology;
    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    return info;
}

inline VkPipelineViewportStateCreateInfo viewportStateCreateInfo()
{
    VkPipelineViewportStateCreateInfo info{};
    info.viewportCount = 1;
    info.pViewports = nullptr; // null, set dynamically
    info.scissorCount = 1;
    info.pScissors = nullptr; // null, set dynamically

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    return info;
}

inline VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo(VkPolygonMode polygonMode)
{
    VkPipelineRasterizationStateCreateInfo info{};
    info.depthClampEnable = VK_FALSE;
    info.rasterizerDiscardEnable = VK_FALSE;
    info.polygonMode = polygonMode;
    info.cullMode = VK_CULL_MODE_BACK_BIT;
    info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    info.depthBiasEnable = VK_FALSE;
    info.lineWidth = 1.0f;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    return info;
}

inline VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo()
{
    VkPipelineMultisampleStateCreateInfo info{};

    info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    info.sampleShadingEnable = VK_FALSE;
    info.pSampleMask = nullptr;
    info.alphaToCoverageEnable = VK_FALSE;
    info.alphaToOneEnable = VK_FALSE;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    return info;
}

inline VkPipelineColorBlendAttachmentState colorBlendAttachmentState()
{
    VkPipelineColorBlendAttachmentState state{};
    state.blendEnable = VK_FALSE;
    // the color components that can be written to by the framebuffer
    state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    return state;
}

inline VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo(VkPipelineColorBlendAttachmentState* attachState)
{
    VkPipelineColorBlendStateCreateInfo info{};
    info.flags = 0;
    info.logicOpEnable = VK_FALSE;
    info.attachmentCount = 1;
    info.pAttachments = attachState;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    return info;
}

inline VkPipelineDynamicStateCreateInfo
dynamicStateCreateInfo(uint32_t dynamicStateCount, const VkDynamicState* pDynamicStates)
{
    VkPipelineDynamicStateCreateInfo info{};
    info.dynamicStateCount = dynamicStateCount;
    info.pDynamicStates = pDynamicStates;
    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    return info;
}

inline VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo()
{
    VkPipelineLayoutCreateInfo info{};
    info.flags = 0;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    return info;
}

inline VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo(
    uint32_t shaderStagesCount, VkPipelineShaderStageCreateInfo* shaderStagesInfo,
    VkPipelineVertexInputStateCreateInfo* vertexStateInfo, VkPipelineInputAssemblyStateCreateInfo* assemblyStateInfo,
    VkPipelineViewportStateCreateInfo* viewportInfo, VkPipelineRasterizationStateCreateInfo* rasterStateInfo,
    VkPipelineMultisampleStateCreateInfo* multisampleStateInfo,
    VkPipelineColorBlendStateCreateInfo* colorBlendStateInfo, VkPipelineDynamicStateCreateInfo* dynamicStateInfo,
    VkPipelineLayout pipelineLayout, VkPipelineRenderingCreateInfo* renderingCreateInfo
)
{
    VkGraphicsPipelineCreateInfo info{};
    info.flags = 0;
    info.stageCount = shaderStagesCount;
    info.pStages = shaderStagesInfo;
    info.pVertexInputState = vertexStateInfo;
    info.pInputAssemblyState = assemblyStateInfo;
    info.pTessellationState = nullptr;
    info.pViewportState = viewportInfo;
    info.pRasterizationState = rasterStateInfo;
    info.pMultisampleState = multisampleStateInfo;
    info.pDepthStencilState = nullptr;
    info.pColorBlendState = colorBlendStateInfo;
    info.pDynamicState = dynamicStateInfo;

    info.layout = pipelineLayout;
    info.renderPass = VK_NULL_HANDLE;
    info.subpass = 0;

    info.pNext = renderingCreateInfo;
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    return info;
}

inline VkRenderingAttachmentInfo renderingAttachmentInfo(
    VkImageView imgView, VkImageLayout imgLayout, VkClearValue clearValue, VkAttachmentLoadOp loadOp,
    VkAttachmentStoreOp storeOp
)
{
    VkRenderingAttachmentInfo info{};
    info.imageView = imgView;
    info.imageLayout = imgLayout;
    info.resolveMode = VK_RESOLVE_MODE_NONE;
    info.resolveImageView = VK_NULL_HANDLE;
    info.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.clearValue = clearValue;
    info.loadOp = loadOp;
    info.storeOp = storeOp;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

    return info;
}

inline VkRenderingInfo renderingInfo(VkRenderingAttachmentInfo* colorAttachmentInfo, VkRect2D renderArea)
{
    VkRenderingInfo info{};
    info.colorAttachmentCount = 1;
    info.pColorAttachments = colorAttachmentInfo;
    info.flags = 0;
    info.layerCount = 1;
    info.viewMask = 0;
    info.pDepthAttachment = nullptr;
    info.pStencilAttachment = nullptr;
    info.renderArea = renderArea;

    info.pNext = nullptr;
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;

    return info;
}

inline VkImageCreateInfo img2DCreateInfo(uint32_t width, uint32_t height)
{
    VkImageCreateInfo imgCreateInfo{};

    imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imgCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgCreateInfo.extent = {.width = width, .height = height, .depth = 1};

    imgCreateInfo.mipLevels = 1;
    imgCreateInfo.arrayLayers = 1;
    imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    imgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCreateInfo.pNext = nullptr;

    return imgCreateInfo;
}

inline VkImageViewCreateInfo imgView2DCreateInfo(VkImage image)
{
    VkImageViewCreateInfo imgViewCreateInfo{};
    imgViewCreateInfo.image = image;
    imgViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imgViewCreateInfo.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .levelCount = 1,
        .layerCount = 1,
    };

    imgViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgViewCreateInfo.pNext = nullptr;
    return imgViewCreateInfo;
}

inline VkBufferCreateInfo bufferCreateInfo(uint64_t bufferSize, VkBufferUsageFlags usage)
{
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;

    return bufferCreateInfo;
}

} // namespace VkEngine
