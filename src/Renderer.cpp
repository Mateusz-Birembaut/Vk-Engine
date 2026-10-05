#include "VkEngine/Renderer.h"

#include <cmath>
#include <stdexcept>
#include <time.h>

#include "VkEngine/FileReader.h"
#include "VkEngine/VkInits.h"
#include "VkEngine/VulkanCtx.h"

Renderer::Renderer() = default;

Renderer::~Renderer()
{
    cleanup();
}

void Renderer::init(VulkanCtx* pVulkanCtx, const std::filesystem::path& shaderFolderPath)
{
    if (!pVulkanCtx)
        return;

    m_ctx = pVulkanCtx;
    m_shaderFolderPath = shaderFolderPath;

    initCommands();
    initSyncStructs();
    initGraphicsPipeline();

    m_clearColorValue.float32[0] = 0.0f; // R
    m_clearColorValue.float32[1] = 0.2f; // G
    m_clearColorValue.float32[2] = 0.4f; // B
    m_clearColorValue.float32[3] = 1.0f; // A
}

void Renderer::initCommands()
{
    VkCommandPoolCreateInfo cmdPoolInfo = VkEngine::cmdPoolCreateInfo(m_ctx->graphicsQueueFamily(), 0);

    for (int Iframe = 0; Iframe < FRAMES_IN_FLIGHT; ++Iframe) {
        if (vkCreateCommandPool(m_ctx->device(), &cmdPoolInfo, nullptr, &m_frames[Iframe].commandPool) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create command pool " + std::to_string(Iframe));
        }
        m_resourceDestroyer.push(m_frames[Iframe].commandPool);

        VkCommandBufferAllocateInfo cmdBuffInfo =
            VkEngine::cmdBufferAllocInfo(m_frames[Iframe].commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

        if (vkAllocateCommandBuffers(m_ctx->device(), &cmdBuffInfo, &m_frames[Iframe].commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create command buffer " + std::to_string(Iframe));
        }
        // will be destroyed with its command pool
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
        m_resourceDestroyer.push(m_frames[IFrame].renderFence);

        if (vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_frames[IFrame].swapchainSem) !=
            VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create swapchain semaphore " + std::to_string(IFrame));
        }
        m_resourceDestroyer.push(m_frames[IFrame].swapchainSem);
    }

    createRenderSems();
}

void Renderer::initGraphicsPipeline()
{
    auto device = m_ctx->device();
    VkFormat imgFormat = m_ctx->swapchainFormat();

    // ----------------- pipeline creation -----------------

    std::filesystem::path vertPath = m_shaderFolderPath;
    vertPath += "/shader.vert.spv";

    auto readVertexShader = VkEngine::readShader(vertPath);
    if (!readVertexShader.has_value()) {
        throw std::runtime_error(
            "[Renderer] Failed to read vertex shader code, error code : " + std::to_string(readVertexShader.error())
        );
    }

    VkEngine::ShaderCodeData vertCode = readVertexShader.value();

    std::filesystem::path fragPath = m_shaderFolderPath;
    fragPath += "/shader.frag.spv";

    auto readFragShader = VkEngine::readShader(fragPath);
    if (!readFragShader.has_value()) {
        throw std::runtime_error(
            "[Renderer] Failed to read frag shader code, error code : " + std::to_string(readFragShader.error())
        );
    }

    VkEngine::ShaderCodeData fragCode = readFragShader.value();

    VkShaderModuleCreateInfo vertexShaderModuleInfo =
        VkEngine::shaderModuleCreateInfo(vertCode.codeSize, vertCode.codeData.data());
    VkShaderModuleCreateInfo fragShaderModuleInfo =
        VkEngine::shaderModuleCreateInfo(fragCode.codeSize, fragCode.codeData.data());

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0] = VkEngine::shaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, &vertexShaderModuleInfo);
    shaderStages[1] = VkEngine::shaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, &fragShaderModuleInfo);

    VkPipelineVertexInputStateCreateInfo vertexStageInfo = VkEngine::vertexInputStateCreateInfo();

    VkPipelineInputAssemblyStateCreateInfo assemblyInfo =
        VkEngine::inputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    VkPipelineViewportStateCreateInfo viewportStateInfo = VkEngine::viewportStateCreateInfo();

    VkPipelineRasterizationStateCreateInfo rasterInfo = VkEngine::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

    VkPipelineMultisampleStateCreateInfo msSampleStateCreateInfo = VkEngine::multisampleStateCreateInfo();

    VkPipelineColorBlendAttachmentState colorBlendState = VkEngine::colorBlendAttachmentState();
    VkPipelineColorBlendStateCreateInfo colorBlendInfo = VkEngine::colorBlendStateCreateInfo(&colorBlendState);

    constexpr int dynamicStatesCount = 2;
    VkDynamicState dynamicStates[dynamicStatesCount] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateInfo =
        VkEngine::dynamicStateCreateInfo(dynamicStatesCount, dynamicStates);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkEngine::pipelineLayoutCreateInfo();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(float);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo = VkEngine::pipelineRenderingCreateInfo(&imgFormat);

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_graphicsPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = VkEngine::graphicsPipelineCreateInfo(
        2, shaderStages, &vertexStageInfo, &assemblyInfo, &viewportStateInfo, &rasterInfo, &msSampleStateCreateInfo,
        &colorBlendInfo, &dynamicStateInfo, m_graphicsPipelineLayout, &pipelineRenderingInfo
    );

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &m_graphicsPipeline) !=
        VK_SUCCESS)
        throw std::runtime_error("[Renderer] Failed to create graphics pipeline");
}

void Renderer::createRenderSems()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = VkEngine::semaphoreCreateInfo();

    // one render semaphore per img in swapchain
    int imgCount = static_cast<int>(m_ctx->swapchainImgs().size());
    m_renderSems.resize(imgCount);

    for (int ISwapChainImg = 0; ISwapChainImg < imgCount; ++ISwapChainImg) {
        if (vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_renderSems[ISwapChainImg]) !=
            VK_SUCCESS) {
            throw std::runtime_error(
                "[Renderer] Failed to create swapchain semaphore " + std::to_string(ISwapChainImg)
            );
        }
    }
}

VkEngine::FrameData& Renderer::getCurrentFrame()
{
    return m_frames[m_frameNb % FRAMES_IN_FLIGHT];
}

void Renderer::cleanup()
{
    if (!m_ctx)
        return; // in case after init ctx = nullptr

    auto device = m_ctx->device();

    vkDeviceWaitIdle(device);

    m_resourceDestroyer.flush(device);

    cleanupRenderSems();

    vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
    vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
}

void Renderer::cleanupRenderSems()
{
    auto device = m_ctx->device();

    int semCount = static_cast<int>(m_renderSems.size());
    for (int IRenderSem = 0; IRenderSem < semCount; ++IRenderSem) {
        vkDestroySemaphore(device, m_renderSems[IRenderSem], nullptr);
    }
    m_renderSems.clear();
}

/// @brief Recreating the swapchain, recreate the render semaphores
void Renderer::recreateSwapchain()
{
    vkDeviceWaitIdle(m_ctx->device());

    m_ctx->createSwapchain();
    cleanupRenderSems();
    createRenderSems();
}

// no early return between acquire and submit
// the swapchain semaphore would be left signaled and next acquire would fail
void Renderer::drawFrame()
{
    if (m_swapchainDirty) {
        recreateSwapchain();
        m_swapchainDirty = false;
    }

    auto& frameData = getCurrentFrame();
    auto device = m_ctx->device();
    VkSwapchainKHR swapChain = m_ctx->swapchain();

    vkWaitForFences(device, 1, &frameData.renderFence, VK_TRUE, UINT64_MAX);

    uint32_t imgId;
    auto acquireImg =
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, frameData.swapchainSem, VK_NULL_HANDLE, &imgId);

    if (acquireImg == VK_ERROR_OUT_OF_DATE_KHR) {
        m_swapchainDirty = true;
        return;
    } else if (acquireImg == VK_SUBOPTIMAL_KHR) {
        m_swapchainDirty = true;
    } else if (acquireImg != VK_SUCCESS) {
        throw std::runtime_error("[Renderer] Failed to acquire swapchain image");
    }

    VkCommandPool cmdPool = frameData.commandPool;
    VkCommandBuffer cmdBuff = frameData.commandBuffer;

    if (vkResetCommandPool(device, cmdPool, 0) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] Failed to reset command pool");

    VkCommandBufferBeginInfo beginInfo = VkEngine::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    if (vkBeginCommandBuffer(cmdBuff, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] failed to begin cmd buffer");

    VkImage img = m_ctx->swapchainImgs()[imgId];
    VkImageView imgView = m_ctx->swapchainViews()[imgId];
    VkSemaphore& renderSemaphore = m_renderSems[imgId];

    VkImageSubresourceRange imgSubresourceRange = VkEngine::imgSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    // only the stage writing the swapchain image waits for the acquire
    // earlier stages can start before the image is available
    // wait semaphore stage should be the same as barrier srcStage
    VkSemaphoreSubmitInfo waitSemInfo =
        VkEngine::semSubmitInfo(frameData.swapchainSem, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // ----------------- barrier to render -----------------
    // layout will be updated after the wait semaphore as the wait semaphore blocks the gpu at the stage
    // COLOR_ATTACHMENT_OUTPUT
    VkImageMemoryBarrier2 undefinedToColorAttachment = VkEngine::imageMemoryBarrier(
        img, imgSubresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
    );

    VkDependencyInfo dependencyInfo = VkEngine::dependencyInfo(&undefinedToColorAttachment);

    vkCmdPipelineBarrier2(cmdBuff, &dependencyInfo);

    VkClearValue clearValue{};
    clearValue.color = m_clearColorValue;

    VkRenderingAttachmentInfo colorAttachmentInfo = VkEngine::renderingAttachmentInfo(
        imgView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, clearValue, VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE
    );

    VkExtent2D swapchainExtent = m_ctx->swapchainExtent();

    VkViewport viewport{};
    viewport.maxDepth = 1.0f;
    viewport.minDepth = 0.0f;
    viewport.height = -static_cast<float>(swapchainExtent.height); // ccw triangles, flip y axis to keep them visible
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.x = 0;
    viewport.y = static_cast<float>(swapchainExtent.height);

    VkRect2D scissor{};
    scissor.extent = swapchainExtent;
    scissor.offset = {0, 0};

    VkRenderingInfo rederingInfo = VkEngine::renderingInfo(&colorAttachmentInfo, scissor);

    // ----------------- rendering -----------------
    vkCmdBeginRendering(cmdBuff, &rederingInfo);

    vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    vkCmdSetViewport(cmdBuff, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

    float constant = static_cast<float>(sin(static_cast<double>(m_frameNb) * 0.01));

    vkCmdPushConstants(cmdBuff, m_graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float), &constant);

    vkCmdDraw(cmdBuff, 3, 1, 0, 0);

    vkCmdEndRendering(cmdBuff);

    // ----------------- barrier to present -----------------
    VkImageMemoryBarrier2 colorToPresentBarrier = VkEngine::imageMemoryBarrier(
        img, imgSubresourceRange, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0
    );

    VkDependencyInfo dependencyInfo2 = VkEngine::dependencyInfo(&colorToPresentBarrier);

    vkCmdPipelineBarrier2(cmdBuff, &dependencyInfo2);

    // signaled once all commands are done -> image safe to present
    VkSemaphoreSubmitInfo signalSemInfo =
        VkEngine::semSubmitInfo(renderSemaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

    if (vkEndCommandBuffer(cmdBuff) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] failed to end cmd buffer");

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo = VkEngine::cmdBuffSubmitInfo(cmdBuff);

    // reset here so if an early return happens before sumbit, on the next iteration renderFence would be left waiting
    // because never signaled
    if (vkResetFences(device, 1, &frameData.renderFence) != VK_SUCCESS)
        throw std::runtime_error("[Renderer] Failed to reset fence");

    // submit to graphics queue and signal fence for next iteration that uses this frameData
    VkSubmitInfo2 submitInfo = VkEngine::submitInfo(&cmdBufferSubmitInfo, &waitSemInfo, &signalSemInfo);
    auto resSumbit = vkQueueSubmit2(m_ctx->graphicsQueue(), 1, &submitInfo, frameData.renderFence);
    if (resSumbit != VK_SUCCESS)
        throw std::runtime_error("[Renderer] submit failed");

    // present after render semaphore signaled
    VkPresentInfoKHR presentInfo = VkEngine::presentInfo(&swapChain, &imgId, &renderSemaphore);
    auto resPresent = vkQueuePresentKHR(m_ctx->graphicsQueue(), &presentInfo);
    if (resPresent == VK_ERROR_OUT_OF_DATE_KHR || resPresent == VK_SUBOPTIMAL_KHR) {
        m_swapchainDirty = true;
    } else if (resPresent != VK_SUCCESS)
        throw std::runtime_error("[Renderer] present failed");

    ++m_frameNb;
}
