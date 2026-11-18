#pragma once

#include <glm/mat4x4.hpp>

#include <vector>

#include "VkEngine/Types/FileData.h"
#include "VkEngine/Types/Texturing.h"
#include "VkEngine/Types/Vertex.h"

namespace vke {

namespace gltf {

struct Primitive {
    std::vector<Vertex> vertices{};
    std::vector<uint32_t> indices{};
    int32_t material{-1}; // -1 absent like in gltf
};

struct Mesh {
    std::vector<Primitive> primitives;
};

struct Node {
    int32_t meshIndex{-1}; // -1 absent like in gltf
    glm::mat4 transform{1.0f};
};

struct Image {
    FileData data; // empty if failed to read image
    ColorSpace colorSpace{ColorSpace::LINEAR};
};

struct Texture {
    int32_t imgIndex{0};
    int32_t samplerIndex{0};
};

struct TextureInfo {
    glm::mat3x2 transform{1.0f};
    int32_t texIndex{-1};
    uint32_t texCoord{0};
};

struct Material {
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    TextureInfo baseColorTexture{};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    TextureInfo metallicRoughnessTexture{};
    TextureInfo normalTexture{};
    TextureInfo occlusionTexture{};
    TextureInfo emissiveTexture{};
    glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
};

struct ImportedScene {
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    std::vector<Image> images;
    std::vector<Sampler> samplers;
    std::vector<Texture> textures;
    std::vector<Material> materials;
};

}; // namespace gltf

} // namespace vke