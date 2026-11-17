#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <span>

#include "VkEngine/Types/Handle.h"
#include "VkEngine/Types/Texturing.h"

namespace vke {

struct GPUMesh;
struct GPUImage;
struct GPUMaterial;

struct ImageDesc {
    int width{0};
    int height{0};
    int channels{4};
    unsigned char* data{nullptr};
    ColorSpace colorSpace{ColorSpace::SRGB};

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

struct Texture {
    Handle<GPUImage> imageHandle{};
    uint32_t samplerIndex{0};
};

struct TextureRef {
    glm::mat3x2 transform{1.0f};
    Texture texture{};
    uint32_t texCoord{0};
};

struct MaterialDesc {
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    TextureRef baseColorTexture{};
    TextureRef metallicRoughnessTexture{};
    TextureRef normalTexture{};
    TextureRef occlusionTexture{};
    TextureRef emissiveTexture{};
    glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
};

struct RenderView {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::vec3 pos{};
};

struct RenderItem {
    glm::mat4 model{1.0f};
    Handle<GPUMesh> meshHandle{};
    Handle<GPUMaterial> materialHandle{};
};

} // namespace vke