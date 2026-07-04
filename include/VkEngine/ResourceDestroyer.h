#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class ResourceDestroyer {
public:
    ResourceDestroyer() = default;
    ~ResourceDestroyer() = default;

    void flush(VkDevice device)
    {
        for (auto&& cmdPool : m_cmdPools) {
            vkDestroyCommandPool(device, cmdPool, nullptr);
        }
        for (auto&& fence : m_fences) {
            vkDestroyFence(device, fence, nullptr);
        }
        for (auto&& sem : m_sems) {
            vkDestroySemaphore(device, sem, nullptr);
        }
    }

    void push(VkCommandPool cmdPool)
    {
        m_cmdPools.push_back(cmdPool);
    }

    void push(VkFence fence)
    {
        m_fences.push_back(fence);
    }

    void push(VkSemaphore sem)
    {
        m_sems.push_back(sem);
    }

private:
    std::vector<VkCommandPool> m_cmdPools;
    std::vector<VkFence> m_fences;
    std::vector<VkSemaphore> m_sems;
    std::vector<VkBuffer> m_buffs;
};
