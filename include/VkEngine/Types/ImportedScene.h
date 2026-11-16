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
    FileData data;
    ColorSpace colorSpace{ColorSpace::SRGB};
};

struct Texture {
    int32_t imgIndex{0};
    int32_t samplerIndex{0};
};

struct Material {
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    int32_t baseColorTextureIndex{-1};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    int32_t metallicRoughnessTextureIndex{-1};
    int32_t normalTextureIndex{-1};
    int32_t occlusionTexture{-1};
    int32_t emissiveTexture{-1};
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