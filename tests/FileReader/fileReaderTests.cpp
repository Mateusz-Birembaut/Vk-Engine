#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <iterator>

#include "VkEngine/FileReader.h"

TEST_CASE("Read empty file spv", "[FileReader]")
{
    auto resultEmpty = VkEngine::readShader(std::filesystem::path{FILEREADER_TEST_DATA_DIR} / "empty.spv");
    REQUIRE(resultEmpty.error() == VkEngine::ShaderFileError::invalidData);
}

TEST_CASE("Read frag shader spv", "[FileReader]")
{
    VkEngine::ShaderCodeData data{};
    data.codeSize = std::filesystem::file_size(std::filesystem::path{FILEREADER_TEST_DATA_DIR} / "shader.frag.spv");
    auto resultFragShader = VkEngine::readShader(std::filesystem::path{FILEREADER_TEST_DATA_DIR} / "shader.frag.spv");
    REQUIRE((data.codeSize / 4) == resultFragShader.value().codeData.size());
    REQUIRE(resultFragShader.value().codeData[0] == 0x07230203u); // magic number SPIR-V
}

TEST_CASE("Read frag shader spv, full content", "[FileReader]")
{
    const auto path = std::filesystem::path{FILEREADER_TEST_DATA_DIR} / "shader.frag.spv";

    std::ifstream raw{path, std::ios::binary};
    REQUIRE(raw.is_open());
    std::vector<char> bytes{std::istreambuf_iterator<char>{raw}, std::istreambuf_iterator<char>{}};
    REQUIRE(bytes.size() % 4 == 0);

    std::vector<uint32_t> expected(bytes.size() / 4);
    std::memcpy(expected.data(), bytes.data(), bytes.size());

    auto result = VkEngine::readShader(path);
    REQUIRE(result.has_value());
    REQUIRE(result.value().codeData == expected);
}

TEST_CASE("Read on non existing file", "[FileReader]")
{
    auto resultFragShader =
        VkEngine::readShader(std::filesystem::path{FILEREADER_TEST_DATA_DIR} / "dazdazdazd.frag.spv");
    REQUIRE(resultFragShader.error() == VkEngine::ShaderFileError::fileNotFound);
}
