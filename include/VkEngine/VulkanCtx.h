#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class Window;

class VulkanCtx {
public:
    VulkanCtx() = default;
    ~VulkanCtx();

    void init(Window* pWindow);
    void createSwapchain();
    void cleanupSwapchain();

    VkDevice device() const
    {
        return m_device;
    }
    VkQueue graphicsQueue() const
    {
        return m_graphicsQueue;
    }
    uint32_t graphicsQueueFamily() const
    {
        return m_graphicsQueueFamily;
    }
    VkExtent2D swapchainExtent() const
    {
        return m_swapchainExtent;
    }
    VkFormat swapchainFormat() const
    {
        return m_swapchainImgFormat;
    }
    const std::vector<VkImageView>& swapchainViews() const
    {
        return m_swapchainImgViews;
    }

    const std::vector<VkImage>& swapchainImgs() const
    {
        return m_swapchainImgs;
    }

    VkSwapchainKHR swapchain() const
    {
        return m_swapchain;
    }

private:
    Window* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_gpu = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkExtent2D m_swapchainExtent{};
    VkFormat m_swapchainImgFormat{};
    std::vector<VkImage> m_swapchainImgs;
    std::vector<VkImageView> m_swapchainImgViews;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily{};
};
