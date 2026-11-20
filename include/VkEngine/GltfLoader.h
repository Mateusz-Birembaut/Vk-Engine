#pragma once

#include "VkEngine/Types/ImportedScene.h"

#include <glm/mat4x4.hpp>
#include <tiny_gltf_v3.h>

#include <filesystem>
#include <memory>
#include <optional>

namespace vke
{

struct PrimitiveAttributeIndices
{
    int32_t pos{-1};
    int32_t normal{-1};
    int32_t uv0{-1};
    int32_t uv1{-1};
    int32_t color{-1};
    int32_t tang{-1};
    int32_t indices{-1};
};

enum materialTextureFlagBits : uint32_t
{
    VKE_MAT_BASE_COLOR_TEXTURE = 1u << 0,
    VKE_MAT_METALLIC_ROUGHNESS_TEXTURE = 1u << 1,
    VKE_MAT_NORMAL_TEXTURE = 1u << 2,
    VKE_MAT_OCCLUSION_TEXTURE = 1u << 3,
    VKE_MAT_EMISSIVE_TEXTURE = 1u << 4,
    VKE_MAT_ANY_TEXTURE = 0xFFFFFFFF,
};
typedef uint32_t materialTextureFlags;

/**
 * Loads in RAM a scene from a gltf file
 *
 * @param path Path to .gltf or .glb file
 *
 */
std::optional<gltf::ImportedScene> loadGltf(const std::filesystem::path& path);

std::optional<tinygltf3::Model> loadGltfModel(const std::filesystem::path& path);

/**
 * Fills a structure containing all possible attrbitue indices for a primitive, -1 if absent
 */
PrimitiveAttributeIndices readPrimitiveAttributesIndices(const tg3_primitive& tgPrim);

std::vector<glm::vec2> readPrimitiveUVAttribute(const tg3_model& model, int32_t uvAttributeIndex);

/**
 * Checks whether the material have a set of textures, by default checks if the material have at least 1 texture
 *
 * @param textureFlags Default = VKE_MAT_ANY_TEXTURE, change this to check
 */
bool MaterialHaveTexture(
    const tg3_model& model, int32_t materialIndex, materialTextureFlags textureFlags = VKE_MAT_ANY_TEXTURE
);

std::optional<gltf::Primitive>
readPrimitiveData(const tg3_model& model, std::string_view meshName, uint32_t primIndex, const tg3_primitive& tgPrim);

void fillMeshesData(const tg3_model& model, std::vector<gltf::Mesh>& impSceneMeshes);

void fillNodesData(const tg3_model& model, std::vector<gltf::Node>& impSceneNodes);

std::vector<unsigned char> readBufferImageBytes(const tg3_model& model, uint32_t IImage, const tg3_image& tgImage);
void fillImagesData(const tg3_model& model, const std::filesystem::path& path, std::vector<gltf::Image>& impSceneImgs);

void fillSamplerData(const tg3_model& model, std::vector<Sampler>& impSceneSampler);

void fillTexturesData(const tg3_model& model, std::vector<gltf::Texture>& impSceneTextures);

void fillTextureRefExtData(vke::gltf::TextureInfo& textInfo, const tg3_extras_ext& extensionInfo);

glm::mat3x2 readTextureTransform(const tg3_extension& ext);
void fillMaterialData(
    const tg3_model& model, std::vector<gltf::Material>& impSceneMaterials, std::vector<gltf::Image>& impSceneImgs
);

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

    for (size_t IElement = 0; IElement < elementCount; IElement++)
    {
        std::memcpy(&value, &buffer.data.data[byteOffset + IElement * byteStride], bytesSrc);

        data.push_back(static_cast<glm::vec<Count, float>>(value));

        if (acc.normalized)
        {
            if constexpr (std::is_signed_v<SRC>)
            {
                for (glm::length_t IValue = 0; IValue < Count; IValue++)
                {
                    data[IElement][IValue] = std::max(data[IElement][IValue] / std::numeric_limits<SRC>::max(), -1.0f);
                }
            }
            else
            {
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
    for (size_t IIndex = 0; IIndex < indexCount; IIndex++)
    {
        std::memcpy(&value, &buffer.data.data[byteOffset + IIndex * bytePerIndex], bytePerIndex);
        indices.push_back(static_cast<uint32_t>(value));
    }
    return indices;
}

std::vector<glm::vec4> readColors(const tg3_model& model, const tg3_accessor& acc);

} // namespace vke
