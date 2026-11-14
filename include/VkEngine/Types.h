#pragma once

#include <vulkan/vulkan.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <span>
#include <vector>
#include <vk_mem_alloc.h>

#include "VkEngine/Pool.h"

namespace vke {

struct GPUBuffer {
    VkBuffer vkbuffer{VK_NULL_HANDLE};
    uint64_t deviceAddress{0};
    VmaAllocation allocation{VK_NULL_HANDLE};
    void* pMappedData{nullptr};
};

struct FrameData {
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkFence renderFence{VK_NULL_HANDLE};
    VkSemaphore swapchainSem{VK_NULL_HANDLE};
    Handle<GPUBuffer> sceneDataBuffHandle{};
};

struct CPUImage {
    int width{0};
    int height{0};
    int channels{4};
    unsigned char* data{nullptr};

    /// @return Size of image in bytes
    size_t sizeBytes() const
    {
        return static_cast<size_t>(width) * height * channels;
    }

    /// @return Read only bytes of the image
    std::span<const std::byte> bytes() const
    {
        return std::as_bytes(std::span(data, sizeBytes()));
    };
};

struct GPUImage {
    VkImage vkImage{VK_NULL_HANDLE};
    VkImageView vkImageView{VK_NULL_HANDLE};
    VmaAllocation allocation{VK_NULL_HANDLE};
};

struct Vertex {
    glm::vec3 position{};
    float uv_x{0.0f};
    glm::vec3 normal{};
    float uv_y{0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct Texture {
    Handle<vke::GPUImage> imageHandle{};
    uint32_t samplerIndex{0};
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

struct GPUMaterialInfo {
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    Texture baseColorTexture{};
    Texture metallicRoughnessTexture{};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
};

struct PushConstants {
    glm::mat4 model{1.0f};
    uint64_t sceneDataBufferAddr{0};
    uint32_t materialIndex{0};
};

struct RenderView {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
};

struct SceneData {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    uint64_t vertBuffAddress{0};
    uint64_t materialBuffAdress{0};
};

struct RenderItem {
    glm::mat4 model{1.0f};
    Handle<GPUMesh> meshHandle{};
    Handle<GPUMaterial> materialHandle{};
};

} // namespace vke