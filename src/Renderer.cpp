#include "VkEngine/Renderer.h"

#include <cmath>
#include <stdexcept>
#include <time.h>

#include "VkEngine/VkInits.h"
#include "VkEngine/VulkanCtx.h"

Renderer::Renderer() = default;

Renderer::~Renderer()
{
    cleanup();
}

void Renderer::init(VulkanCtx* pVulkanCtx)
{
    m_ctx = pVulkanCtx;

    initCommands();
    initSyncStructs();

    m_clearColorValue.float32[0] = 1.0f; // R
    m_clearColorValue.float32[1] = 0.2f; // G
    m_clearColorValue.float32[2] = 0.4f; // B
    m_clearColorValue.float32[3] = 1.0f; // A
}

void Renderer::initCommands()
{
    VkCommandPoolCreateInfo cmdPoolInfo =
        VkEngine::cmdPoolCreateInfo(m_ctx->graphicsQueueFamily(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int Iframe = 0; Iframe < FRAMES_IN_FLIGHT; ++Iframe) {
        if (vkCreateCommandPool(m_ctx->device(), &cmdPoolInfo, nullptr, &m_frames[Iframe].commandPool) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create command pool " + std::to_string(Iframe));
        }

        VkCommandBufferAllocateInfo cmdBuffInfo =
            VkEngine::cmdBufferAllocInfo(m_frames[Iframe].commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

        if (vkAllocateCommandBuffers(m_ctx->device(), &cmdBuffInfo, &m_frames[Iframe].commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create command buffer " + std::to_string(Iframe));
        }
    }
}

void Renderer::initSyncStructs()
{
    VkFenceCreateInfo fenceCreateInfo = VkEngine::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = VkEngine::semaphoreCreateInfo();

    // one fence and swapchain semaphore per frame in flight
    for (int IFrame = 0; IFrame < FRAMES_IN_FLIGHT; ++IFrame) {
        if (vkCreateFence(m_ctx->device(), &fenceCreateInfo, nullptr, &m_frames[IFrame].renderFence) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create render fence " + std::to_string(IFrame));
        }
        if (vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_frames[IFrame].swapchainSem) !=
            VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create swapchain semaphore " + std::to_string(IFrame));
        }
    }

    createRenderSemaphores();
}

void Renderer::createRenderSemaphores()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = VkEngine::semaphoreCreateInfo();

    // one render semaphore per img in swapchain
    int imgCount = static_cast<int>(m_ctx->swapchainImgs().size());
    m_renderSems.reserve(imgCount);

    for (int ISwapChainImg = 0; ISwapChainImg < imgCount; ++ISwapChainImg) {
        if (vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_renderSems[ISwapChainImg]) !=
            VK_SUCCESS) {
            throw std::runtime_error(
                "[Renderer] Failed to create swapchain semaphore " + std::to_string(ISwapChainImg)
            );
        }
    }
}

FrameData& Renderer::getCurrentFrame()
{
    return m_frames[m_frameNb % FRAMES_IN_FLIGHT];
}

void Renderer::cleanup()
{
    vkDeviceWaitIdle(m_ctx->device());

    auto device = m_ctx->device();

    for (int Iframe = 0; Iframe < FRAMES_IN_FLIGHT; ++Iframe) {
        vkDestroyCommandPool(device, m_frames[Iframe].commandPool, nullptr);
        m_frames[Iframe].commandPool = VK_NULL_HANDLE;
        m_frames[Iframe].commandBuffer = VK_NULL_HANDLE;

        vkDestroyFence(device, m_frames[Iframe].renderFence, nullptr);
        vkDestroySemaphore(device, m_frames[Iframe].swapchainSem, nullptr);
    }

    cleanupRenderSems();
}

void Renderer::cleanupRenderSems()
{
    auto device = m_ctx->device();

    int imgCount = static_cast<int>(m_ctx->swapchainImgs().size());
    for (int ISwapChainImg = 0; ISwapChainImg < imgCount; ++ISwapChainImg) {
        vkDestroySemaphore(device, m_renderSems[ISwapChainImg], nullptr);
    }
}

/// @brief Recreating the swapchain, if img count changed, recreate the render semaphores
void Renderer::recreateSwapchain()
{
    vkDeviceWaitIdle(m_ctx->device());

    int imgCountBefore = m_ctx->swapchainImgCount();
    m_ctx->createSwapchain();
    int imgCountAfter = m_ctx->swapchainImgCount();

    if (imgCountBefore != imgCountAfter) {
        cleanupRenderSems();
        createRenderSemaphores();
    }
}

// no early return between acquire and submit
// the swapchain semaphore would be left signaled and next acquire would fail
void Renderer::drawFrame()
{
    auto& frameData = getCurrentFrame();
    VkSwapchainKHR swapChain = m_ctx->swapchain();

    vkWaitForFences(m_ctx->device(), 1, &frameData.renderFence, VK_TRUE, UINT64_MAX);

    uint32_t imgId;
    auto acquireImg =
        vkAcquireNextImageKHR(m_ctx->device(), swapChain, UINT64_MAX, frameData.swapchainSem, VK_NULL_HANDLE, &imgId);

    if (acquireImg == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    VkCommandBuffer cmdBuff = frameData.commandBuffer;
    vkResetCommandBuffer(cmdBuff, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    VkCommandBufferBeginInfo beginInfo = VkEngine::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (vkBeginCommandBuffer(cmdBuff, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] failed to begin cmd buffer");

    VkImage img = m_ctx->swapchainImgs()[imgId];
    VkSemaphore& renderSemaphore = m_renderSems[imgId];

    VkImageSubresourceRange imgSubresourceRange = VkEngine::imgSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    // Transition to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to clear
    VkImageMemoryBarrier2 undefinedToTransferBarrier = VkEngine::imageMemoryBarrier(
        img, imgSubresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_NONE,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_CLEAR_BIT
    );

    VkDependencyInfo dependencyInfo = VkEngine::dependencyInfo(&undefinedToTransferBarrier);

    vkCmdPipelineBarrier2(cmdBuff, &dependencyInfo);

    // not steady clock but just to see that color changes
    double end = 100 * ((double) clock()) / (double) CLOCKS_PER_SEC;
    double time_taken = end - start;
    m_clearColorValue.float32[0] = cos(time_taken) * 0.5f + 0.5f; // R
    vkCmdClearColorImage(
        cmdBuff, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &m_clearColorValue, 1, &imgSubresourceRange
    );

    VkImageMemoryBarrier2 transferToPresentBarrier = VkEngine::imageMemoryBarrier(
        img, imgSubresourceRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, VK_PIPELINE_STAGE_2_NONE
    );

    VkDependencyInfo dependencyInfo2 = VkEngine::dependencyInfo(&transferToPresentBarrier);

    vkCmdPipelineBarrier2(cmdBuff, &dependencyInfo2);

    if (vkEndCommandBuffer(cmdBuff) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] failed to end cmd buffer");

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo = VkEngine::cmdBuffSubmitInfo(cmdBuff);

    // gpu waits at the start of the pipeline
    VkSemaphoreSubmitInfo waitSemInfo =
        VkEngine::semSubmitInfo(frameData.swapchainSem, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

    // gpu signals ready to present at the end of the pipeline
    VkSemaphoreSubmitInfo signalSemInfo =
        VkEngine::semSubmitInfo(renderSemaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

    // reset here so if an early return happens before sumbit, on the next iteration renderFence would be left waiting
    // because never signaled
    vkResetFences(m_ctx->device(), 1, &frameData.renderFence);

    // submit to graphics queue and signal fence for next iteration that uses this frameData
    VkSubmitInfo2 submitInfo = VkEngine::submitInfo(&cmdBufferSubmitInfo, &waitSemInfo, &signalSemInfo);
    auto resSumbit = vkQueueSubmit2(m_ctx->graphicsQueue(), 1, &submitInfo, frameData.renderFence);
    if (resSumbit != VK_SUCCESS)
        throw std::runtime_error("[Renderer] submit failed");

    // present after render semaphore signaled
    VkPresentInfoKHR presentInfo = VkEngine::presentInfo(&swapChain, &imgId, &renderSemaphore);
    auto resPresent = vkQueuePresentKHR(m_ctx->graphicsQueue(), &presentInfo);
    if (resPresent == VK_ERROR_OUT_OF_DATE_KHR || resPresent == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (resPresent != VK_SUCCESS)
        throw std::runtime_error("[Renderer] present failed");

    ++m_frameNb;
}
