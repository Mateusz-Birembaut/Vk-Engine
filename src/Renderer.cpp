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

    initPerFrameData();
    createRenderSems();
    initGraphicsPipeline();

    m_clearColorValue.float32[0] = 0.0f; // R
    m_clearColorValue.float32[1] = 0.2f; // G
    m_clearColorValue.float32[2] = 0.4f; // B
    m_clearColorValue.float32[3] = 1.0f; // A

    // for now load fallback texture here
    uint32_t whitePixelData = 0xFFFFFFFF; // RGBA
    vke::CPUImage whitePixel{
        .width = 1, .height = 1, .channels = 4, .data = reinterpret_cast<unsigned char*>(&whitePixelData)
    };

    Handle<vke::GPUImage> whitePixelImgHandle = m_gpuResources.createTexture(whitePixel);
    uint32_t whitePixelSamplerId = m_gpuResources.getOrCreateSampler(); // for now create a basic sampler
    m_fallbackTexture = {whitePixelImgHandle, whitePixelSamplerId};
}

void Renderer::initPerFrameData()
{
    VkCommandPoolCreateInfo cmdPoolInfo = vke::cmdPoolCreateInfo(m_ctx->graphicsQueueFamily(), 0);
    VkFenceCreateInfo fenceCreateInfo = vke::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vke::semaphoreCreateInfo();

    for (int IFrame = 0; IFrame < FRAMES_IN_FLIGHT; ++IFrame) {
        // cmd pool and buffer
        vke::check(
            vkCreateCommandPool(m_ctx->device(), &cmdPoolInfo, nullptr, &m_frames[IFrame].commandPool),
            "[Renderer] Failed to create command pool"
        );
        m_resourceDestroyer.push(m_frames[IFrame].commandPool);

        VkCommandBufferAllocateInfo cmdBuffInfo =
            vke::cmdBufferAllocInfo(m_frames[IFrame].commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

        vke::check(
            vkAllocateCommandBuffers(m_ctx->device(), &cmdBuffInfo, &m_frames[IFrame].commandBuffer),
            "[Renderer] Failed to create command buffer"
        ); // will be destroyed with its command pool

        // sync objects
        vke::check(
            vkCreateFence(m_ctx->device(), &fenceCreateInfo, nullptr, &m_frames[IFrame].renderFence),
            "[Renderer] Failed to create render fence"
        );
        m_resourceDestroyer.push(m_frames[IFrame].renderFence);

        vke::check(
            vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_frames[IFrame].swapchainSem),
            "[Renderer] Failed to create swapchain semaphore"
        );
        m_resourceDestroyer.push(m_frames[IFrame].swapchainSem);

        // scene data buffer
        m_frames[IFrame].sceneDataBuffHandle = m_gpuResources.createMappedStorageBuffer(sizeof(vke::SceneData));
    }
}

void Renderer::initGraphicsPipeline()
{
    auto device = m_ctx->device();
    VkFormat imgFormat = m_ctx->swapchainFormat();

    // ----------------- pipeline creation -----------------

    std::filesystem::path vertPath = m_shaderFolderPath;
    vertPath += "/shader.vert.spv";

    auto readVertexShader = vke::readShader(vertPath);
    if (!readVertexShader.has_value()) {
        throw std::runtime_error(
            "[Renderer] Failed to read vertex shader code, error code : " + std::to_string(readVertexShader.error())
        );
    }

    vke::ShaderCodeData vertCode = readVertexShader.value();

    std::filesystem::path fragPath = m_shaderFolderPath;
    fragPath += "/shader.frag.spv";

    auto readFragShader = vke::readShader(fragPath);
    if (!readFragShader.has_value()) {
        throw std::runtime_error(
            "[Renderer] Failed to read frag shader code, error code : " + std::to_string(readFragShader.error())
        );
    }

    vke::ShaderCodeData fragCode = readFragShader.value();

    VkShaderModuleCreateInfo vertexShaderModuleInfo =
        vke::shaderModuleCreateInfo(vertCode.codeData.size() * 4, vertCode.codeData.data());
    VkShaderModuleCreateInfo fragShaderModuleInfo =
        vke::shaderModuleCreateInfo(fragCode.codeData.size() * 4, fragCode.codeData.data());

    VkPipelineShaderStageCreateInfo shaderStages[2];
    shaderStages[0] = vke::shaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, &vertexShaderModuleInfo);
    shaderStages[1] = vke::shaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, &fragShaderModuleInfo);

    VkPipelineVertexInputStateCreateInfo vertexStageInfo = vke::vertexInputStateCreateInfo();

    VkPipelineInputAssemblyStateCreateInfo assemblyInfo =
        vke::inputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    VkPipelineViewportStateCreateInfo viewportStateInfo = vke::viewportStateCreateInfo();

    VkPipelineRasterizationStateCreateInfo rasterInfo = vke::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

    VkPipelineMultisampleStateCreateInfo msSampleStateCreateInfo = vke::multisampleStateCreateInfo();

    VkPipelineDepthStencilStateCreateInfo depthStateCreateInfo = vke::depthStateCreateInfo();

    VkPipelineColorBlendAttachmentState colorBlendState = vke::colorBlendAttachmentState();
    VkPipelineColorBlendStateCreateInfo colorBlendInfo = vke::colorBlendStateCreateInfo(&colorBlendState);

    constexpr int dynamicStatesCount = 2;
    VkDynamicState dynamicStates[dynamicStatesCount] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateInfo = vke::dynamicStateCreateInfo(dynamicStatesCount, dynamicStates);

    VkDescriptorSetLayout descriptorSetLayout = m_gpuResources.descSetLayout();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vke::pipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPushConstantRange fragShaderPushConstants{};
    fragShaderPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderPushConstants.offset = 0;
    fragShaderPushConstants.size = sizeof(vke::PushConstants);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &fragShaderPushConstants;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo =
        vke::pipelineRenderingCreateInfo(&imgFormat, VK_FORMAT_D32_SFLOAT); // TODO ask for best format

    vke::check(
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_graphicsPipelineLayout),
        "[Renderer] Failed to create pipeline layout"
    );

    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = vke::graphicsPipelineCreateInfo(
        2, shaderStages, &vertexStageInfo, &assemblyInfo, &viewportStateInfo, &rasterInfo, &msSampleStateCreateInfo,
        &depthStateCreateInfo, &colorBlendInfo, &dynamicStateInfo, m_graphicsPipelineLayout, &pipelineRenderingInfo
    );

    vke::check(
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &m_graphicsPipeline),
        "[Renderer] Failed to create graphics pipeline"
    );
}

void Renderer::createRenderSems()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = vke::semaphoreCreateInfo();

    // one render semaphore per img in swapchain
    int imgCount = static_cast<int>(m_ctx->swapchainImgs().size());
    m_renderSems.resize(imgCount);

    for (int ISwapChainImg = 0; ISwapChainImg < imgCount; ++ISwapChainImg) {
        vke::check(
            vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_renderSems[ISwapChainImg]),
            "[Renderer] Failed to create swapchain semaphore"
        );
    }
}

vke::FrameData& Renderer::getCurrentFrame()
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

    m_gpuResources.recreateDepthBuffer();
}

// no early return between acquire and submit
// the swapchain semaphore would be left signaled and next acquire would fail
void Renderer::drawFrame(const std::span<vke::RenderItem>& renderItems, const vke::RenderView& RenderView)
{
    if (m_swapchainDirty) {
        recreateSwapchain();
        m_swapchainDirty = false;
    }

    auto& frameData = getCurrentFrame();
    auto device = m_ctx->device();
    VkSwapchainKHR swapChain = m_ctx->swapchain();
    auto* sceneBuffer = m_gpuResources.getBuffer(frameData.sceneDataBuffHandle);

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
        vke::check(acquireImg, "[Renderer] Failed to acquire swapchain image");
    }

    // copy scene data to buffer
    vke::SceneData sceneData{
        .view = RenderView.view, .proj = RenderView.proj, .vertBuffAddress = m_gpuResources.vertBuffAddr()
    };

    memcpy(sceneBuffer->pMappedData, &sceneData, sizeof(vke::SceneData));

    VkCommandPool cmdPool = frameData.commandPool;
    VkCommandBuffer cmdBuff = frameData.commandBuffer;

    vke::check(vkResetCommandPool(device, cmdPool, 0), "[Renderer] Failed to reset command pool");

    VkCommandBufferBeginInfo beginInfo = vke::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    vke::check(vkBeginCommandBuffer(cmdBuff, &beginInfo), "[Renderer] failed to begin cmd buffer");

    VkImage img = m_ctx->swapchainImgs()[imgId];
    VkImageView imgView = m_ctx->swapchainViews()[imgId];
    VkSemaphore& renderSemaphore = m_renderSems[imgId];

    const vke::GPUImage* depthBuffer = m_gpuResources.getDepthBuffer();

    VkImageSubresourceRange imgSubresourceRange = vke::imgSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceRange depthImgSubresourceRange = vke::imgSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT);

    // only the stage writing the swapchain image waits for the acquire
    // earlier stages can start before the image is available
    // wait semaphore stage should be the same as barrier srcStage
    VkSemaphoreSubmitInfo waitSemInfo =
        vke::semSubmitInfo(frameData.swapchainSem, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // ----------------- barrier to render -----------------
    // layout will be updated after the wait semaphore as the wait semaphore blocks the gpu at the stage
    // COLOR_ATTACHMENT_OUTPUT

    std::array<VkImageMemoryBarrier2, 2> imgBarriers{
        vke::imageMemoryBarrier(
            img, imgSubresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        ),

        vke::imageMemoryBarrier(
            depthBuffer->vkImage, depthImgSubresourceRange, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        ),
    };

    VkDependencyInfo dependencyInfo =
        vke::dependencyInfo(static_cast<uint32_t>(imgBarriers.size()), imgBarriers.data());

    vkCmdPipelineBarrier2(cmdBuff, &dependencyInfo);

    VkClearValue colorClearValue{};
    colorClearValue.color = m_clearColorValue;

    VkClearValue depthClearValue{};
    depthClearValue.depthStencil = m_clearDepthValue;

    VkRenderingAttachmentInfo colorAttachmentInfo = vke::renderingAttachmentInfo(
        imgView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorClearValue, VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE
    );

    VkRenderingAttachmentInfo depthAttachmentInfo = vke::renderingAttachmentInfo(
        depthBuffer->vkImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depthClearValue,
        VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE
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

    VkRenderingInfo rederingInfo = vke::renderingInfo(&colorAttachmentInfo, &depthAttachmentInfo, scissor);

    // ----------------- rendering -----------------
    vkCmdBeginRendering(cmdBuff, &rederingInfo);

    vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    VkDescriptorSet descSet = m_gpuResources.descSet();
    vkCmdBindDescriptorSets(
        cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &descSet, 0, nullptr
    );

    vkCmdSetViewport(cmdBuff, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

    vke::PushConstants pushConstants{
        .sceneDataBufferAddr = sceneBuffer->deviceAddress,
        .samplerIndex = m_fallbackTexture.samplerIndex,
        .baseColorTextureIndex = m_fallbackTexture.imageHandle.index,
    };

    for (const auto& renderItem : renderItems) {
        pushConstants.baseColorTextureIndex = renderItem.baseColorHandle.index;
        pushConstants.baseColorFactor = renderItem.baseColorFactor;
        pushConstants.model = renderItem.model;

        vkCmdPushConstants(
            cmdBuff, m_graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
            sizeof(vke::PushConstants), &pushConstants
        );

        vkCmdBindIndexBuffer(cmdBuff, m_gpuResources.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        const vke::GPUMesh* mesh = m_gpuResources.getMesh(renderItem.meshHandle);
        if (mesh) {
            vkCmdDrawIndexed(cmdBuff, mesh->indexCount, 1, mesh->firstIndex, mesh->baseVertex, 0);
        }
    }

    vkCmdEndRendering(cmdBuff);

    // ----------------- barrier to present -----------------
    VkImageMemoryBarrier2 colorToPresentBarrier = vke::imageMemoryBarrier(
        img, imgSubresourceRange, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0
    );

    VkDependencyInfo dependencyInfo2 = vke::dependencyInfo(1, &colorToPresentBarrier);

    vkCmdPipelineBarrier2(cmdBuff, &dependencyInfo2);

    // signaled once all commands are done -> image safe to present
    VkSemaphoreSubmitInfo signalSemInfo = vke::semSubmitInfo(renderSemaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

    vke::check(vkEndCommandBuffer(cmdBuff), "[Renderer] failed to end cmd buffer");

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo = vke::cmdBuffSubmitInfo(cmdBuff);

    // reset here so if an early return happens before sumbit, on the next iteration renderFence would be left waiting
    // because never signaled
    vke::check(vkResetFences(device, 1, &frameData.renderFence), "[Renderer] Failed to reset fence");

    // submit to graphics queue and signal fence for next iteration that uses this frameData
    VkSubmitInfo2 submitInfo = vke::submitInfo(&cmdBufferSubmitInfo, &waitSemInfo, &signalSemInfo);
    vke::check(
        vkQueueSubmit2(m_ctx->graphicsQueue(), 1, &submitInfo, frameData.renderFence), "[Renderer] submit failed"
    );

    // present after render semaphore signaled
    VkPresentInfoKHR presentInfo = vke::presentInfo(&swapChain, &imgId, &renderSemaphore);
    auto resPresent = vkQueuePresentKHR(m_ctx->graphicsQueue(), &presentInfo);
    if (resPresent == VK_ERROR_OUT_OF_DATE_KHR || resPresent == VK_SUBOPTIMAL_KHR) {
        m_swapchainDirty = true;
    } else if (resPresent != VK_SUCCESS)
        throw std::runtime_error("[Renderer] present failed");

    ++m_frameNb;
}

Handle<vke::GPUMesh> Renderer::createMesh(std::span<const vke::Vertex> vertices, std::span<const uint32_t> indices)
{
    return m_gpuResources.createGpuMesh(vertices, indices);
}

Handle<vke::GPUImage> Renderer::createTexture(const vke::CPUImage& cpuImg)
{
    return m_gpuResources.createTexture(cpuImg);
}