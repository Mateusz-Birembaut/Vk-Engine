#pragma once

#include <vulkan/vulkan_core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <vk_mem_alloc.h>

#include "VkEngine/Types/Handle.h"

namespace vke {

struct GPUBuffer {
    VkBuffer vkbuffer{VK_NULL_HANDLE};
    uint64_t deviceAddress{0};
    VmaAllocation allocation{VK_NULL_HANDLE};
    void* pMappedData{nullptr};
};

struct GPUImage {
    VkImage vkImage{VK_NULL_HANDLE};
    VkImageView vkImageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
};

struct GPUMesh {
    uint32_t firstIndex{0}; // first index inside index buffer
    uint32_t indexCount{0};
    int32_t baseVertex{0}; // offset index to read vertex data inside the vertex buffer
    VmaVirtualAllocation vAllocVertices{VK_NULL_HANDLE};
    VmaVirtualAllocation vAllocIndices{VK_NULL_HANDLE};
};

struct GPUMegaBuffer {
    uint64_t capacity{0};
    VmaVirtualBlock block{VK_NULL_HANDLE};
    GPUBuffer gpuBuffer{};
};

struct GPUMaterial {
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t baseColorTextureIndex{};
    uint32_t baseColorTextureSampler{};
    uint32_t metallicRoughnessTextureIndex{};
    uint32_t metallicRoughnessTextureSampler{};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
};

struct PushConstants {
    glm::mat4 model{1.0f};
    uint64_t sceneDataBufferAddr{0};
    uint32_t materialIndex{0};
};

struct SceneData {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::vec3 cameraPos{};
    glm::vec3 sunLightDir{-0.5f, -1.0f, -0.3f};
    // glm::vec3 sunLightColor{1.0f, 0.95f, 0.8f};
    glm::vec3 sunLightColor{1.0f, 1.0f, 1.0f};
    float sunIntensiy{3.0f};
    uint64_t vertBuffAddress{0};
    uint64_t materialBuffAdress{0};
};

struct FrameData {
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkFence renderFence{VK_NULL_HANDLE};
    VkSemaphore swapchainSem{VK_NULL_HANDLE};
    Handle<GPUBuffer> sceneDataBuffHandle{};
};

} // namespace vke