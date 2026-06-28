#include "VkEngine/Renderer.h"

#include <stdexcept>

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
}

void Renderer::initCommands()
{
    VkCommandPoolCreateInfo cmdPoolInfo{};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = m_ctx->graphicsQueueFamily();
    cmdPoolInfo.pNext = nullptr;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    for (int Iframe = 0; Iframe < FRAMES_IN_FLIGHT; ++Iframe) {
        if (vkCreateCommandPool(m_ctx->device(), &cmdPoolInfo, nullptr, &m_frames[Iframe].commandPool) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create command pool " + std::to_string(Iframe));
        }

        VkCommandBufferAllocateInfo cmdBuffInfo{};
        cmdBuffInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBuffInfo.pNext = nullptr;
        cmdBuffInfo.commandPool = m_frames[Iframe].commandPool;
        cmdBuffInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBuffInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(m_ctx->device(), &cmdBuffInfo, &m_frames[Iframe].commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("[Renderer] Failed to create command buffer " + std::to_string(Iframe));
        }
    }
}

FrameData& Renderer::getCurrentFrame()
{
    return m_frames[m_frameNb % FRAMES_IN_FLIGHT];
}

void Renderer::cleanup()
{
    for (int Iframe = 0; Iframe < FRAMES_IN_FLIGHT; ++Iframe) {
        vkDestroyCommandPool(m_ctx->device(), m_frames[Iframe].commandPool, nullptr);
        m_frames[Iframe].commandPool = VK_NULL_HANDLE;
        m_frames[Iframe].commandBuffer = VK_NULL_HANDLE;
    }
}