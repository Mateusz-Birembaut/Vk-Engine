#pragma once

#include <glm/mat4x4.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <tiny_gltf_v3.h>

#include "VkEngine/Types/ImportedScene.h"

namespace vke {

gltf::ImportedScene loadGltf(const std::filesystem::path& path);
std::optional<gltf::ImportedScene> loadGltf(const std::filesystem::path& path);

std::optional<tinygltf3::Model> loadGltfModel(const std::filesystem::path& path);

void fillMeshesData(const tg3_model& model, std::vector<gltf::Mesh>& impSceneMeshes);
void fillNodesData(const tg3_model& model, std::vector<gltf::Node>& impSceneNodes);
void fillImagesData(const tg3_model& model, const std::filesystem::path& path, std::vector<gltf::Image>& impSceneImgs);
void fillSamplerData(const tg3_model& model, std::vector<Sampler>& impSceneSampler);
void fillTexturesData(const tg3_model& model, std::vector<gltf::Texture>& impSceneTextures);
void fillMaterialData(
    const tg3_model& model, std::vector<gltf::Material>& impSceneMaterials, std::vector<gltf::Image>& impSceneImgs
);

void fillTextureRefExtData(vke::gltf::TextureInfo& textInfo, const tg3_extras_ext& extensionInfo);

void traverseNodes(
    const tg3_model& model, const tg3_node& node, const glm::mat4& parentTransform,
    std::vector<gltf::Node>& impSceneNodes
);

WrapMode wrapModeFrom(int32_t value);
FilterMode filterModeFrom(int32_t value);

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

    for (size_t IElement = 0; IElement < elementCount; IElement++) {
        std::memcpy(&value, &buffer.data.data[byteOffset + IElement * byteStride], bytesSrc);

        data.push_back(static_cast<glm::vec<Count, float>>(value));

        if (acc.normalized) {
            if constexpr (std::is_signed_v<SRC>) {
                for (glm::length_t IValue = 0; IValue < Count; IValue++) {
                    data[IElement][IValue] = std::max(data[IElement][IValue] / std::numeric_limits<SRC>::max(), -1.0f);
                }
            } else {
                data[IElement] /= static_cast<float>(std::numeric_limits<SRC>::max());
            }
        }
    }
    return data;
}

template <typename SRC> std::vector<uint32_t> readIndices(const tg3_model& model, const tg3_accessor& acc)
{
    auto bufferView = model.buffer_views[acc.buffer_view];
    auto buffer = model.buffers[bufferView.buffer];

    size_t indexCount = static_cast<size_t>(acc.count);

    std::vector<uint32_t> indices;
    indices.reserve(indexCount);

    size_t byteOffset = static_cast<size_t>(bufferView.byte_offset + acc.byte_offset);
    size_t bytePerIndex = sizeof(SRC);

    SRC value{};
    for (size_t IIndex = 0; IIndex < indexCount; IIndex++) {
        std::memcpy(&value, &buffer.data.data[byteOffset + IIndex * bytePerIndex], bytePerIndex);
        indices.push_back(static_cast<uint32_t>(value));
    }
    return indices;
}

std::vector<glm::vec4> readColors(const tg3_model& model, const tg3_accessor& acc);

glm::mat3x2 readTextureTransform(const tg3_extension& ext);

} // namespace vke
