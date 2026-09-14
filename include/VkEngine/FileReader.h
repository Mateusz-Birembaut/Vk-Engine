#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <vector>

#include "VkEngine/Types.h"

namespace VkEngine {

enum ShaderFileError { invalidPath, invalidData, readError, failOpen, fileNotFound, unknownError };

inline std::expected<ShaderCodeData, ShaderFileError> readShader(const std::filesystem::path& path)
{
    try {
        ShaderCodeData shaderData{};

        if (path.extension() != ".spv") {
            return std::unexpected(ShaderFileError::invalidPath);
        }

        if (!std::filesystem::exists(path)) {
            return std::unexpected(ShaderFileError::fileNotFound);
        }

        std::ifstream fileReader{path, std::ifstream::binary};

        size_t size = static_cast<size_t>(std::filesystem::file_size(path));
        if (size == 0 || size % 4 != 0)
            return std::unexpected(ShaderFileError::invalidData);

        shaderData.codeSize = size;

        if (!fileReader.is_open())
            return std::unexpected(ShaderFileError::failOpen);

        shaderData.codeData.resize(size / 4);
        fileReader.read(reinterpret_cast<char*>(shaderData.codeData.data()), size);

        if (!fileReader || fileReader.gcount() != static_cast<std::streamsize>(size))
            return std::unexpected(ShaderFileError::readError);

        return shaderData;

    } catch (const std::exception& e) {
        return std::unexpected(ShaderFileError::unknownError);
    }
}

} // namespace VkEngine
