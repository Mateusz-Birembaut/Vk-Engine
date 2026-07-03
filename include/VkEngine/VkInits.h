#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace VkEngine {

VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0)
{
    VkSemaphoreCreateInfo semInfo{};
    semInfo.flags = flags;
    semInfo.pNext = nullptr;
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    return semInfo;
}

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.flags = flags;
    fenceInfo.pNext = nullptr;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    return fenceInfo;
}

VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = flags;
    beginInfo.pNext = nullptr;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    return beginInfo;
}

VkCommandPoolCreateInfo cmdPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandBufferUsageFlags flags)
{
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.flags = flags;
    cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
    cmdPoolInfo.pNext = nullptr;
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    return cmdPoolInfo;
}

VkSemaphoreSubmitInfo semSubmitInfo(VkSemaphore sem, VkPipelineStageFlags2 stageMask)
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

VkCommandBufferSubmitInfo cmdBuffSubmitInfo(VkCommandBuffer commandBuffer)
{
    VkCommandBufferSubmitInfo cmdBuffSubmitInfo{};
    cmdBuffSubmitInfo.commandBuffer = commandBuffer;
    cmdBuffSubmitInfo.deviceMask = 0;
    cmdBuffSubmitInfo.pNext = nullptr;
    cmdBuffSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

    return cmdBuffSubmitInfo;
}

VkCommandBufferAllocateInfo cmdBufferAllocInfo(VkCommandPool cmdPool, VkCommandBufferLevel lvl, uint32_t bufferCount)
{
    VkCommandBufferAllocateInfo cmdBuffInfo{};
    cmdBuffInfo.commandPool = cmdPool;
    cmdBuffInfo.level = lvl;
    cmdBuffInfo.commandBufferCount = bufferCount;
    cmdBuffInfo.pNext = nullptr;
    cmdBuffInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    return cmdBuffInfo;
}

VkSubmitInfo2 submitInfo(
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

VkPresentInfoKHR presentInfo(VkSwapchainKHR* swapchain, uint32_t* imgId, VkSemaphore* waitSem)
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

VkImageSubresourceRange imgSubresourceRange(VkImageAspectFlags aspectMask)
{
    VkImageSubresourceRange subImage{};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

    return subImage;
}

VkDependencyInfo dependencyInfo(VkImageMemoryBarrier2* imgMemBarrier)
{
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = imgMemBarrier;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    return dependencyInfo;
}

VkImageMemoryBarrier2 imageMemoryBarrier(
    VkImage img, VkImageSubresourceRange imgSubRessource, VkImageLayout oldLayout, VkAccessFlags2 srcAccess,
    VkPipelineStageFlags2 srcStage, VkImageLayout newLayout, VkAccessFlags2 dstAccess, VkPipelineStageFlags2 dstStage
)
{
    VkImageMemoryBarrier2 imgMemBarrier{};

    imgMemBarrier.image = img;
    imgMemBarrier.subresourceRange = imgSubRessource;

    imgMemBarrier.oldLayout = oldLayout;
    imgMemBarrier.srcAccessMask = srcAccess;
    imgMemBarrier.srcStageMask = srcStage;
    imgMemBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imgMemBarrier.newLayout = newLayout;
    imgMemBarrier.dstAccessMask = dstAccess;
    imgMemBarrier.dstStageMask = dstStage;
    imgMemBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    imgMemBarrier.pNext = nullptr;
    imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    return imgMemBarrier;
}

} // namespace VkEngine
