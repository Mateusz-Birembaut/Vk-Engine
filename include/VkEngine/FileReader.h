#pragma once

#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace vke {

enum FileError { invalidPath, invalidData, readError, failOpen, fileNotFound, unknownError };

struct ShaderCodeData {
    std::vector<uint32_t> codeData;
};

struct FileData {
    std::vector<unsigned char> bytes;
};

inline std::expected<FileData, FileError> readFile(const std::filesystem::path& path)
{
    try {
        FileData fileData{};

        if (!std::filesystem::exists(path)) {
            return std::unexpected(FileError::fileNotFound);
        }

        std::ifstream fileReader{path, std::ifstream::binary};

        size_t size = static_cast<size_t>(std::filesystem::file_size(path));

        if (size == 0) {
            return std::unexpected(FileError::invalidData);
        }

        if (!fileReader.is_open())
            return std::unexpected(FileError::failOpen);

        fileData.bytes.resize(size);
        fileReader.read(reinterpret_cast<char*>(fileData.bytes.data()), size);

        if (!fileReader || fileReader.gcount() != static_cast<std::streamsize>(size))
            return std::unexpected(FileError::readError);

        return fileData;

    } catch (const std::exception&) {
        return std::unexpected(FileError::unknownError);
    }
}

inline std::expected<ShaderCodeData, FileError> readShader(const std::filesystem::path& path)
{
    try {
        if (path.extension() != ".spv") {
            return std::unexpected(FileError::invalidPath);
        }

        auto fileDataRet = readFile(path);

        if (!fileDataRet.has_value()) {
            return std::unexpected(fileDataRet.error());
        }

        FileData& fileData = fileDataRet.value();

        if (fileData.bytes.size() == 0 || fileData.bytes.size() % 4 != 0) {
            return std::unexpected(FileError::invalidData);
        }

        ShaderCodeData shaderData{};
        size_t byteLength = fileData.bytes.size();
        shaderData.codeData.resize(byteLength / 4);
        std::memcpy(shaderData.codeData.data(), fileData.bytes.data(), byteLength);

        return shaderData;

    } catch (const std::exception&) {
        return std::unexpected(FileError::unknownError);
    }
}

} // namespace vke
