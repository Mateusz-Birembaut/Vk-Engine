#pragma once

#include <cstdint>
#include <vector>

namespace vke
{

enum FileError
{
    invalidPath,
    invalidData,
    readError,
    failOpen,
    fileNotFound,
    unknownError
};

struct ShaderCodeData
{
    std::vector<uint32_t> codeData;
};

struct FileData
{
    std::vector<unsigned char> bytes;
};

} // namespace vke
