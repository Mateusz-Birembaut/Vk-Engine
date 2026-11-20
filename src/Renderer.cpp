#include "VkEngine/Renderer.h"

#include "VkEngine/FileReader.h"
#include "VkEngine/Helpers.h"
#include "VkEngine/VkInits.h"
#include "VkEngine/VulkanCtx.h"

#include <array>
#include <cmath>
#include <stdexcept>

namespace vke
{

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

    // for now load fallback texture here
    uint32_t whitePixelData = 0xFFFFFFFF; // RGBA
    ImageDesc whitePixel{
        .width = 1, .height = 1, .channels = 4, .data = reinterpret_cast<unsigned char*>(&whitePixelData)
    };

    Handle<GPUImage> fallBackImageHandle = m_gpuResources.createGpuImage(whitePixel);
    uint32_t fallBackSamplerIndex = m_gpuResources.getOrCreateSampler({}); // for now create a basic sampler

    m_fallbackTexture = Texture{.imageHandle = fallBackImageHandle, .samplerIndex = fallBackSamplerIndex};

    m_fallbackMaterial = m_gpuResources.createMaterial(
        {.baseColorTexture{
             .texture = m_fallbackTexture,
         },
         .metallicRoughnessTexture{.texture = m_fallbackTexture}}
    );
}

void Renderer::initPerFrameData()
{
    VkCommandPoolCreateInfo cmdPoolInfo = cmdPoolCreateInfo(m_ctx->graphicsQueueFamily(), 0);
    VkFenceCreateInfo fenceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };
    VkSemaphoreCreateInfo semaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    for (auto& m_frame : m_frames)
    {
        // cmd pool and buffer
        check(
            vkCreateCommandPool(m_ctx->device(), &cmdPoolInfo, nullptr, &m_frame.commandPool),
            "[Renderer] Failed to create command pool"
        );
        m_resourceDestroyer.push(m_frame.commandPool);

        VkCommandBufferAllocateInfo cmdBuffInfo =
            cmdBufferAllocInfo(m_frame.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

        check(
            vkAllocateCommandBuffers(m_ctx->device(), &cmdBuffInfo, &m_frame.commandBuffer),
            "[Renderer] Failed to create command buffer"
        ); // will be destroyed with its command pool

        // sync objects
        check(
            vkCreateFence(m_ctx->device(), &fenceCreateInfo, nullptr, &m_frame.renderFence),
            "[Renderer] Failed to create render fence"
        );
        m_resourceDestroyer.push(m_frame.renderFence);

        check(
            vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_frame.swapchainSem),
            "[Renderer] Failed to create swapchain semaphore"
        );
        m_resourceDestroyer.push(m_frame.swapchainSem);

        // scene data buffer
        m_frame.sceneDataBuffHandle = m_gpuResources.createMappedBuffer(sizeof(SceneData));
    }
}

void Renderer::initGraphicsPipeline()
{
    auto device = m_ctx->device();
    VkFormat imgFormat = m_ctx->swapchainFormat();

    // ----------------- pipeline creation -----------------

    std::filesystem::path vertPath = m_shaderFolderPath;
    vertPath += "/shader.vert.spv";

    auto readVertexShader = readShader(vertPath);
    if (!readVertexShader.has_value())
    {
        throw std::runtime_error(
            "[Renderer] Failed to read vertex shader code, error code : " + std::to_string(readVertexShader.error())
        );
    }

    const ShaderCodeData& vertCode = *readVertexShader;

    std::filesystem::path fragPath = m_shaderFolderPath;
    fragPath += "/shader.frag.spv";

    auto readFragShader = readShader(fragPath);
    if (!readFragShader.has_value())
    {
        throw std::runtime_error(
            "[Renderer] Failed to read frag shader code, error code : " + std::to_string(readFragShader.error())
        );
    }

    const ShaderCodeData& fragCode = *readFragShader;

    VkShaderModuleCreateInfo vertexShaderModuleInfo =
        shaderModuleCreateInfo(vertCode.codeData.size() * 4, vertCode.codeData.data());
    VkShaderModuleCreateInfo fragShaderModuleInfo =
        shaderModuleCreateInfo(fragCode.codeData.size() * 4, fragCode.codeData.data());

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    shaderStages[0] = shaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, &vertexShaderModuleInfo);
    shaderStages[1] = shaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, &fragShaderModuleInfo);

    VkPipelineVertexInputStateCreateInfo vertexStageInfo = vertexInputStateCreateInfo();

    VkPipelineInputAssemblyStateCreateInfo assemblyInfo =
        inputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    VkPipelineViewportStateCreateInfo viewportStateInfo = viewportStateCreateInfo();

    VkPipelineRasterizationStateCreateInfo rasterInfo = rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);

    VkPipelineMultisampleStateCreateInfo msSampleStateCreateInfo = multisampleStateCreateInfo();

    VkPipelineDepthStencilStateCreateInfo depthStateCreateInfo = vke::depthStateCreateInfo();

    VkPipelineColorBlendAttachmentState colorBlendState = colorBlendAttachmentState();
    VkPipelineColorBlendStateCreateInfo colorBlendInfo = colorBlendStateCreateInfo(&colorBlendState);

    std::array<VkDynamicState, 2> dynamicStates;
    dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;

    VkPipelineDynamicStateCreateInfo dynamicStateInfo =
        dynamicStateCreateInfo(dynamicStates.size(), dynamicStates.data());

    VkDescriptorSetLayout descriptorSetLayout = m_gpuResources.descSetLayout();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = pipelineLayoutCreateInfo();
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkPushConstantRange fragShaderPushConstants{};
    fragShaderPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderPushConstants.offset = 0;
    fragShaderPushConstants.size = sizeof(PushConstants);

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &fragShaderPushConstants;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo =
        pipelineRenderingCreateInfo(&imgFormat, VK_FORMAT_D32_SFLOAT); // TODO ask for best format

    check(
        vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_graphicsPipelineLayout),
        "[Renderer] Failed to create pipeline layout"
    );

    VkGraphicsPipelineCreateInfo graphicsPipelineInfo = graphicsPipelineCreateInfo(
        2, shaderStages.data(), &vertexStageInfo, &assemblyInfo, &viewportStateInfo, &rasterInfo,
        &msSampleStateCreateInfo, &depthStateCreateInfo, &colorBlendInfo, &dynamicStateInfo, m_graphicsPipelineLayout,
        &pipelineRenderingInfo
    );

    check(
        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphicsPipelineInfo, nullptr, &m_graphicsPipeline),
        "[Renderer] Failed to create graphics pipeline"
    );
}

void Renderer::createRenderSems()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    // one render semaphore per img in swapchain
    int imgCount = static_cast<int>(m_ctx->swapchainImgs().size());
    m_renderSems.resize(imgCount);

    for (int ISwapChainImg = 0; ISwapChainImg < imgCount; ++ISwapChainImg)
    {
        check(
            vkCreateSemaphore(m_ctx->device(), &semaphoreCreateInfo, nullptr, &m_renderSems[ISwapChainImg]),
            "[Renderer] Failed to create swapchain semaphore"
        );
    }
}

FrameData& Renderer::getCurrentFrame()
{
    return m_frames[m_frameNb % FRAMES_IN_FLIGHT];
}

Texture Renderer::fallbackTexture() const
{
    return m_fallbackTexture;
}

Handle<GPUMaterial> Renderer::fallbackMaterial() const
{
    return m_fallbackMaterial;
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
    for (int IRenderSem = 0; IRenderSem < semCount; ++IRenderSem)
    {
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
void Renderer::drawFrame(const std::span<RenderItem>& renderItems, const RenderView& RenderView)
{
    if (m_swapchainDirty)
    {
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

    if (acquireImg == VK_ERROR_OUT_OF_DATE_KHR)
    {
        m_swapchainDirty = true;
        return;
    }

    if (acquireImg == VK_SUBOPTIMAL_KHR)
    {
        m_swapchainDirty = true;
    }
    else
    {
        check(acquireImg, "[Renderer] Failed to acquire swapchain image");
    }

    // copy scene data to buffer
    SceneData sceneData{
        .view = RenderView.view,
        .proj = RenderView.proj,
        .cameraPos = RenderView.pos,
        .vertBuffAddress = m_gpuResources.vertBuffAddr(),
        .materialBuffAdress = m_gpuResources.matBuffAddr(),
    };

    memcpy(sceneBuffer->pMappedData, &sceneData, sizeof(SceneData));

    VkCommandPool cmdPool = frameData.commandPool;
    VkCommandBuffer cmdBuff = frameData.commandBuffer;

    check(vkResetCommandPool(device, cmdPool, 0), "[Renderer] Failed to reset command pool");

    VkCommandBufferBeginInfo beginInfo = commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    check(vkBeginCommandBuffer(cmdBuff, &beginInfo), "[Renderer] failed to begin cmd buffer");

    VkImage img = m_ctx->swapchainImgs()[imgId];
    VkImageView imgView = m_ctx->swapchainViews()[imgId];
    VkSemaphore& renderSemaphore = m_renderSems[imgId];

    const GPUImage* depthBuffer = m_gpuResources.getDepthBuffer();

    VkImageSubresourceRange imgSubresourceRange = vke::imgSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    VkImageSubresourceRange depthImgSubresourceRange = vke::imgSubresourceRange(VK_IMAGE_ASPECT_DEPTH_BIT);

    // only the stage writing the swapchain image waits for the acquire
    // earlier stages can start before the image is available
    // wait semaphore stage should be the same as barrier srcStage
    VkSemaphoreSubmitInfo waitSemInfo =
        semSubmitInfo(frameData.swapchainSem, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    // ----------------- barrier to render -----------------
    // layout will be updated after the wait semaphore as the wait semaphore blocks the gpu at the stage
    // COLOR_ATTACHMENT_OUTPUT

    std::array<VkImageMemoryBarrier2, 2> imgBarriers{
        imageMemoryBarrier(
            img, imgSubresourceRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT
        ),

        imageMemoryBarrier(
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

    VkRenderingAttachmentInfo colorAttachmentInfo = renderingAttachmentInfo(
        imgView, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, colorClearValue, VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE
    );

    VkRenderingAttachmentInfo depthAttachmentInfo = renderingAttachmentInfo(
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
    scissor.offset = {.x = 0, .y = 0};

    VkRenderingInfo rederingInfo = renderingInfo(&colorAttachmentInfo, &depthAttachmentInfo, scissor);

    // ----------------- rendering -----------------
    vkCmdBeginRendering(cmdBuff, &rederingInfo);

    vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    VkDescriptorSet descSet = m_gpuResources.descSet();
    vkCmdBindDescriptorSets(
        cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &descSet, 0, nullptr
    );

    vkCmdSetViewport(cmdBuff, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuff, 0, 1, &scissor);

    PushConstants pushConstants{
        .sceneDataBufferAddr = sceneBuffer->deviceAddress,
    };

    for (const auto& renderItem : renderItems)
    {
        pushConstants.materialIndex = renderItem.materialHandle.index; // TODO check for validity before this
        pushConstants.model = renderItem.model;

        vkCmdPushConstants(
            cmdBuff, m_graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
            sizeof(PushConstants), &pushConstants
        );

        vkCmdBindIndexBuffer(cmdBuff, m_gpuResources.indexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        const GPUMesh* mesh = m_gpuResources.getMesh(renderItem.meshHandle);
        if (mesh)
        {
            vkCmdDrawIndexed(cmdBuff, mesh->indexCount, 1, mesh->firstIndex, mesh->baseVertex, 0);
        }
    }

    vkCmdEndRendering(cmdBuff);

    // ----------------- barrier to present -----------------
    VkImageMemoryBarrier2 colorToPresentBarrier = imageMemoryBarrier(
        img, imgSubresourceRange, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_NONE, 0
    );

    VkDependencyInfo dependencyInfo2 = vke::dependencyInfo(1, &colorToPresentBarrier);

    vkCmdPipelineBarrier2(cmdBuff, &dependencyInfo2);

    // signaled once all commands are done -> image safe to present
    VkSemaphoreSubmitInfo signalSemInfo = semSubmitInfo(renderSemaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);

    check(vkEndCommandBuffer(cmdBuff), "[Renderer] failed to end cmd buffer");

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo = cmdBuffSubmitInfo(cmdBuff);

    // reset here so if an early return happens before sumbit, on the next iteration renderFence would be left
    // waiting because never signaled
    check(vkResetFences(device, 1, &frameData.renderFence), "[Renderer] Failed to reset fence");

    // submit to graphics queue and signal fence for next iteration that uses this frameData
    VkSubmitInfo2 submitInfo = vke::submitInfo(&cmdBufferSubmitInfo, &waitSemInfo, &signalSemInfo);
    check(vkQueueSubmit2(m_ctx->graphicsQueue(), 1, &submitInfo, frameData.renderFence), "[Renderer] submit failed");

    // present after render semaphore signaled
    VkPresentInfoKHR presentInfo = vke::presentInfo(&swapChain, &imgId, &renderSemaphore);
    auto resPresent = vkQueuePresentKHR(m_ctx->graphicsQueue(), &presentInfo);
    if (resPresent == VK_ERROR_OUT_OF_DATE_KHR || resPresent == VK_SUBOPTIMAL_KHR)
    {
        m_swapchainDirty = true;
    }
    else if (resPresent != VK_SUCCESS)
        throw std::runtime_error("[Renderer] present failed");

    ++m_frameNb;
}

Handle<GPUMesh> Renderer::createMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices)
{
    return m_gpuResources.createGpuMesh(vertices, indices);
}

Handle<GPUImage> Renderer::createGpuImage(const ImageDesc& imgDesc)
{
    return m_gpuResources.createGpuImage(imgDesc);
}

Handle<GPUMaterial> Renderer::createMaterial(const MaterialDesc& material)
{
    return m_gpuResources.createMaterial(material);
}

uint32_t Renderer::getOrCreateSampler(const Sampler& samplerDesc)
{
    return m_gpuResources.getOrCreateSampler(samplerDesc);
}

} // namespace vke