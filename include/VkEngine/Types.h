#pragma once

#include <vulkan/vulkan.h>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>
#include <vk_mem_alloc.h>

#include "VkEngine/Pool.h"

namespace VkEngine {

struct FrameData {
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkFence renderFence{VK_NULL_HANDLE};
    VkSemaphore swapchainSem{VK_NULL_HANDLE};
};

struct ShaderCodeData {
    size_t codeSize{0}; // must be a multiple of 4
    std::vector<uint32_t> codeData;
};

struct CPUImage {
    int width{0};
    int height{0};
    int channels{4};
    unsigned char* data{nullptr};
};

struct GPUImage {
    VkImage vkImage{VK_NULL_HANDLE};
    VkImageView vkImageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
};

struct GPUBuffer {
    VkBuffer vkbuffer{VK_NULL_HANDLE};
    uint64_t deviceAddress{0};
    VmaAllocation allocation{VK_NULL_HANDLE};
};

struct Vertex {
    glm::vec3 position{};
    float uv_x{0.0f};
    glm::vec3 normal{};
    float uv_y{0.0f};
    glm::vec4 color{};
};

struct Texture {
    Handle<VkEngine::GPUImage> imageHandle{};
    uint32_t samplerIndex{0};
};

/* struct SamplerDesc{

}
 */
} // namespace VkEngine
