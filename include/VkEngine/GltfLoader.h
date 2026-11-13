#pragma once

#include <glm/mat4x4.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <tiny_gltf_v3.h>

#include "VkEngine/Types.h"

namespace vke {

struct FileData;

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

struct Sampler {
    int32_t minFilter;
    int32_t magFilter;
    int32_t wrapS;
    int32_t wrapT;
};

struct GltfTexture {
    int32_t imgIndex{0};
    int32_t samplerIndex{0};
};

struct Material {
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    int32_t baseColorTextureIndex{-1};
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    int32_t metallicRoughnessTextureIndex{-1};
};

struct ImportedScene {
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    std::vector<FileData> images;
    std::vector<Sampler> samplers;
    std::vector<GltfTexture> textures;
    std::vector<Material> materials;
};

ImportedScene loadGltf(const std::filesystem::path& path);

std::optional<tinygltf3::Model> loadGltfModel(const std::filesystem::path& path);

void fillMeshesData(const tg3_model& model, std::vector<Mesh>& impSceneMeshes);
void fillNodesData(const tg3_model& model, std::vector<Node>& impSceneNodes);
void fillImagesData(const tg3_model& model, const std::filesystem::path& path, std::vector<FileData>& impSceneImgs);
void fillSamplerData(const tg3_model& model, std::vector<Sampler>& impSceneSampler);
void fillTexturesData(const tg3_model& model, std::vector<GltfTexture>& impSceneTextures);
void fillMaterialData(const tg3_model& model, std::vector<Material>& impSceneMaterials);

void traverseNodes(
    const tg3_model& model, const tg3_node& node, const glm::mat4& parentTransform, std::vector<Node>& impSceneNodes
);

template <typename SRC, size_t Count>
std::vector<glm::vec<Count, float>> readPrimitiveAttribute(const tg3_model& model, const tg3_accessor& acc)
{
    auto bufferView = model.buffer_views[acc.buffer_view];
    auto buffer = model.buffers[bufferView.buffer];

    size_t elementCount = static_cast<size_t>(acc.count);

    std::vector<glm::vec<Count, float>> data;
    data.reserve(elementCount);

    size_t byteOffset = static_cast<size_t>(bufferView.byte_offset + acc.byte_offset);
    int32_t byteStrideSigned = tg3_accessor_byte_stride(&acc, &bufferView);
    assert(byteStrideSigned != -1);
    size_t byteStride = static_cast<size_t>(byteStrideSigned);
    size_t bytesSrc = sizeof(SRC) * Count;

    glm::vec<Count, SRC> value{};

    for (size_t i = 0; i < elementCount; i++) {
        std::memcpy(&value, &buffer.data.data[byteOffset + i * byteStride], bytesSrc);

        data.push_back(static_cast<glm::vec<Count, float>>(value));

        if (acc.normalized) {
            if constexpr (std::is_signed_v<SRC>) {
                for (glm::length_t j = 0; j < Count; j++) {
                    data[i][j] = std::max(data[i][j] / std::numeric_limits<SRC>::max(), -1.0f);
                }
            } else {
                data[i] /= static_cast<float>(std::numeric_limits<SRC>::max());
            }
        }
    }
    return data;
}

template <typename SRC> std::vector<uint32_t> readIndices(const tg3_model& model, const tg3_accessor& acc)
{
    auto bufferView = model.buffer_views[acc.buffer_view];
    auto buffer = model.buffers[bufferView.buffer];

    size_t elementCount = static_cast<size_t>(acc.count);

    std::vector<uint32_t> indices;
    indices.reserve(elementCount);

    size_t byteOffset = static_cast<size_t>(bufferView.byte_offset + acc.byte_offset);
    size_t bytePerIndex = sizeof(SRC);

    SRC value{};
    for (size_t i = 0; i < elementCount; i++) {
        std::memcpy(&value, &buffer.data.data[byteOffset + i * bytePerIndex], bytePerIndex);
        indices.push_back(static_cast<uint32_t>(value));
    }
    return indices;
}

} // namespace vke
