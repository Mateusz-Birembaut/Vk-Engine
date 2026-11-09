#pragma once

#include <filesystem>
#include <memory>
#include <tiny_gltf_v3.h>

#include "VkEngine/Types.h"

namespace vke {

template <typename SRC, size_t Count>
std::vector<glm::vec<Count, float>> readPrimitive(const tg3_model& model, const tg3_accessor& acc)
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

std::vector<Mesh> loadGltf(const std::filesystem::path& path);

} // namespace vke
