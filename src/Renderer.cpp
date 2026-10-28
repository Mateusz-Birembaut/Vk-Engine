#include "VkEngine/Renderer.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <time.h>

#include "VkEngine/FileReader.h"
#include "VkEngine/Helpers.h"
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

    m_gpuResources.init(pVulkanCtx);

    initCommands();
    initSyncStructs();
    initGraphicsPipeline();

    m_clearColorValue.float32[0] = 0.0f; // R
    m_clearColorValue.float32[1] = 0.2f; // G
    m_clearColorValue.float32[2] = 0.4f; // B
    m_clearColorValue.float32[3] = 1.0f; // A

    // for now load fallback texture here
    uint32_t whitePixelData = 0xFFFFFFFF; // RGBA
    VkEngine::CPUImage whitePixel{
        .width = 1, .height = 1, .channels = 4, .data = reinterpret_cast<unsigned char*>(&whitePixelData)
    };

    Handle<VkEngine::GPUImage> whitePixelImgHandle = m_gpuResources.createGpuImage(whitePixel);
    uint32_t whitePixelSamplerId = m_gpuResources.getOrCreateSampler(); // for now create a basic sampler

    std::vector<VkEngine::Vertex> vertices{
        VkEngine::Vertex{.position = {-0.5f, -0.5f, 0.0f}},
        VkEngine::Vertex{.position = {0.5f, -0.5f, 0.0f}},
        VkEngine::Vertex{.position = {0.0f, 0.5f, 0.0f}},
    };
    std::vector<uint32_t> indices{0, 1, 2};

    m_testMesh = m_gpuResources.createGpuMesh(vertices, indices);
    m_fallbackTexture = {whitePixelImgHandle, whitePixelSamplerId};

    std::vector<VkEngine::Vertex> vertices2{
        VkEngine::Vertex{.position = {0.0f, 0.8f, 0.0f}},       VkEngine::Vertex{.position = {0.283f, 0.206f, 0.0f}},
        VkEngine::Vertex{.position = {0.761f, 0.247f, 0.0f}},   VkEngine::Vertex{.position = {0.333f, -0.108f, 0.0f}},
        VkEngine::Vertex{.position = {0.470f, -0.647f, 0.0f}},  VkEngine::Vertex{.position = {0.0f, -0.35f, 0.0f}},
        VkEngine::Vertex{.position = {-0.470f, -0.647f, 0.0f}}, VkEngine::Vertex{.position = {-0.333f, -0.108f, 0.0f}},
        VkEngine::Vertex{.position = {-0.761f, 0.247f, 0.0f}},  VkEngine::Vertex{.position = {-0.283f, 0.206f, 0.0f}},
        VkEngine::Vertex{.position = {0.0f, 0.0f, 0.0f}},
    };
    std::vector<uint32_t> indices2{
        10, 1, 0, 10, 2, 1, 10, 3, 2, 10, 4, 3, 10, 5, 4, 10, 6, 5, 10, 7, 6, 10, 8, 7, 10, 9, 8, 10, 0, 9,
    };

    m_testMesh2 = m_gpuResources.createGpuMesh(vertices2, indices2);
}

void Renderer::initCommands()
{
    VkCommandPoolCreateInfo cmdPoolInfo = VkEngine::cmdPoolCreateInfo(m_ctx->graphicsQueueFamily(), 0);

    for (int Iframe = 0; Iframe < FRAMES_IN_FLIGHT; ++Iframe) {
        VkEngine::check(
            vkCreateCommandPool(m_ctx->device(), &cmdPoolInfo, nullptr, &m_frames[Iframe].commandPool),
            "[Renderer] Failed to create command pool"
        );
        m_resourceDestroyer.push(m_frames[Iframe].commandPool);

        VkCommandBufferAllocateInfo cmdBuffInfo =
            VkEngine::cmdBufferAllocInfo(m_frames[Iframe].commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

        VkEngine::check(
            vkAllocateCommandBuffers(m_ctx->device(), &cmdBuffInfo, &m_frames[Iframe].commandBuffer),
            "[Renderer] Failed to create command buffer"
        );
        // will be destroyed with its command pool
    }
}

void Renderer::initSyncStructs()
{
    VkFenceCreateInfo fenceCreateInfo = VkEngine::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = VkEngine::semaphoreCreateInfo();

    // one fence and swapchain semaphore per frame in flight
    for (int IFrame = 0; IFrame < FRAMES_IN_FLIGHT; ++IFrame) {
        VkEngine::check(
            vkCreateFence(m_ctx->device(), &fenceCreateInfo, nullptr, &m_frames[IFrame].renderFence),
            "[Renderer] Failed to create render fence"
        );
        m_resourceDestroyer.push(m_frames[IFrame].renderFence);

        VkEngine::check(
            vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_frames[IFrame].swapchainSem),
            "[Renderer] Failed to create swapchain semaphore"
        );
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

    VkDescriptorSetLayout descriptorSetLayout = m_gpuResources.descSetLayout();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = VkEngine::pipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPushConstantRange fragShaderPushConstants{};
    fragShaderPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderPushConstants.offset = 0;
    fragShaderPushConstants.size = sizeof(VkEngine::PushConstants);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &fragShaderPushConstants;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo = VkEngine::pipelineRenderingCreateInfo(&imgFormat);

    VkEngine::check(
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_graphicsPipelineLayout),
        "[Renderer] Failed to create pipeline layout"
    );

    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = VkEngine::graphicsPipelineCreateInfo(
        2, shaderStages, &vertexStageInfo, &assemblyInfo, &viewportStateInfo, &rasterInfo, &msSampleStateCreateInfo,
        &colorBlendInfo, &dynamicStateInfo, m_graphicsPipelineLayout, &pipelineRenderingInfo
    );

    VkEngine::check(
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &m_graphicsPipeline),
        "[Renderer] Failed to create graphics pipeline"
    );
}

void Renderer::createRenderSems()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = VkEngine::semaphoreCreateInfo();

    // one render semaphore per img in swapchain
    int imgCount = static_cast<int>(m_ctx->swapchainImgs().size());
    m_renderSems.resize(imgCount);

    for (int ISwapChainImg = 0; ISwapChainImg < imgCount; ++ISwapChainImg) {
        VkEngine::check(
            vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_renderSems[ISwapChainImg]),
            "[Renderer] Failed to create swapchain semaphore"
        );
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

    m_gpuResources.cleanup();
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
    }

    if (acquireImg == VK_SUBOPTIMAL_KHR) {
        m_swapchainDirty = true;
    } else {
        VkEngine::check(acquireImg, "[Renderer] Failed to acquire swapchain image");
    }

    VkCommandPool cmdPool = frameData.commandPool;
    VkCommandBuffer cmdBuff = frameData.commandBuffer;

    VkEngine::check(vkResetCommandPool(device, cmdPool, 0), "[Renderer] Failed to reset command pool");

    VkCommandBufferBeginInfo beginInfo = VkEngine::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkEngine::check(vkBeginCommandBuffer(cmdBuff, &beginInfo), "[Renderer] failed to begin cmd buffer");

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

    VkDescriptorSet descSet = m_gpuResources.descSet();
    vkCmdBindDescriptorSets(
        cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &descSet, 0, nullptr
    );

    vkCmdSetViewport(cmdBuff, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

    VkEngine::PushConstants pushConstants{
        .vertBuffAddress = m_gpuResources.vertBuffAddr(),
        .samplerIndex = m_fallbackTexture.samplerIndex,
        .textureIndex = m_fallbackTexture.imageHandle.index
    };

    vkCmdPushConstants(
        cmdBuff, m_graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(VkEngine::PushConstants), &pushConstants
    );

    vkCmdBindIndexBuffer(cmdBuff, m_gpuResources.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const VkEngine::GPUMesh* mesh = m_gpuResources.getMesh(m_testMesh);
    if (mesh) {
        vkCmdDrawIndexed(cmdBuff, mesh->indexCount, 1, mesh->firstIndex, mesh->baseVertex, 0);
    }

    const VkEngine::GPUMesh* mesh2 = m_gpuResources.getMesh(m_testMesh2);
    if (mesh2) {
        vkCmdDrawIndexed(cmdBuff, mesh2->indexCount, 1, mesh2->firstIndex, mesh2->baseVertex, 0);
    }

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

    VkEngine::check(vkEndCommandBuffer(cmdBuff), "[Renderer] failed to end cmd buffer");

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo = VkEngine::cmdBuffSubmitInfo(cmdBuff);

    // reset here so if an early return happens before sumbit, on the next iteration renderFence would be left waiting
    // because never signaled
    VkEngine::check(vkResetFences(device, 1, &frameData.renderFence), "[Renderer] Failed to reset fence");

    // submit to graphics queue and signal fence for next iteration that uses this frameData
    VkSubmitInfo2 submitInfo = VkEngine::submitInfo(&cmdBufferSubmitInfo, &waitSemInfo, &signalSemInfo);
    VkEngine::check(
        vkQueueSubmit2(m_ctx->graphicsQueue(), 1, &submitInfo, frameData.renderFence), "[Renderer] submit failed"
    );

    // present after render semaphore signaled
    VkPresentInfoKHR presentInfo = VkEngine::presentInfo(&swapChain, &imgId, &renderSemaphore);
    auto resPresent = vkQueuePresentKHR(m_ctx->graphicsQueue(), &presentInfo);
    if (resPresent == VK_ERROR_OUT_OF_DATE_KHR || resPresent == VK_SUBOPTIMAL_KHR) {
        m_swapchainDirty = true;
    } else if (resPresent != VK_SUCCESS)
        throw std::runtime_error("[Renderer] present failed");

    ++m_frameNb;
}
