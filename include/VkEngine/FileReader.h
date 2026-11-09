#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <tiny_gltf_v3.h>
#include <vector>

#include "VkEngine/GltfLoader.h"
#include "VkEngine/Types.h"

namespace vke {

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

    } catch (const std::exception&) {
        return std::unexpected(ShaderFileError::unknownError);
    }
}

inline std::vector<vke::Mesh> testGltf(const std::filesystem::path& path)
{
    tg3_parse_options opts;
    tg3_error_stack errors;
    tg3_model model;

    std::string pathStr = path.string();
    const char* pathCstr = pathStr.c_str();
    const uint32_t pathSize = static_cast<uint32_t>(pathStr.size());

    tg3_parse_options_init(&opts);
    tg3_error_stack_init(&errors);

    tg3_error_code err = tg3_parse_file(&model, &errors, pathCstr, pathSize, &opts);
    if (err != TG3_OK) {
        for (uint32_t i = 0; i < errors.count; i++) {
            fprintf(
                stderr, "[%d] %s\n", (int) errors.entries[i].severity,
                errors.entries[i].message ? errors.entries[i].message : "(null)"
            );
        }
    }

    std::vector<vke::Mesh> meshes(model.meshes_count); // for now only 1 supported

    for (size_t idMesh = 0; idMesh < static_cast<size_t>(model.meshes_count); ++idMesh) {
        int32_t positionPrim = -1;
        int32_t indicesPrim = -1;
        int32_t normalPrim = -1;
        int32_t uvPrim = -1;

        const tg3_mesh& mesh = model.meshes[idMesh];
        for (uint32_t i = 0; i < mesh.primitives->attributes_count; i++) {
            auto& attribute = mesh.primitives->attributes[i];

            if (strcmp(attribute.key.data, "POSITION") == 0) {
                positionPrim = attribute.value;
            } else if (strcmp(attribute.key.data, "NORMAL") == 0) {
                normalPrim = attribute.value;
            } else if (strcmp(attribute.key.data, "TEXCOORD_0") == 0) {
                uvPrim = attribute.value;
            }
            indicesPrim = mesh.primitives->indices;
        }

        const auto& vertexAcc = model.accessors[positionPrim];

        std::vector<glm::vec3> positions = readPrimitive<float, 3>(model, vertexAcc);
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;

        meshes[idMesh].vertices.resize(positions.size());

        if (normalPrim != -1) {
            const auto& normalAcc = model.accessors[normalPrim];
            normals = readPrimitive<float, 3>(model, normalAcc);
        }

        if (uvPrim != -1) {
            const auto& uvAcc = model.accessors[uvPrim];
            switch (uvAcc.component_type) {
                case TG3_COMPONENT_TYPE_FLOAT:
                    uvs = readPrimitive<float, 2>(model, uvAcc);
                    break;
                case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                    uvs = readPrimitive<uint8_t, 2>(model, uvAcc);
                    break;
                case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                    uvs = readPrimitive<uint16_t, 2>(model, uvAcc);
                    break;
            }
        }

        const auto& indexAcc = model.accessors[indicesPrim];

        switch (indexAcc.component_type) {
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                meshes[idMesh].indices = readIndices<int8_t>(model, indexAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                meshes[idMesh].indices = readIndices<uint16_t>(model, indexAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_INT:
                meshes[idMesh].indices = readIndices<uint32_t>(model, indexAcc);
                break;
        }

        for (size_t i = 0; i < positions.size(); i++) {
            meshes[idMesh].vertices[i].position = positions[i];
            if (normalPrim != -1) {
                meshes[idMesh].vertices[i].normal = normals[i];
            }
            if (uvPrim != -1) {
                meshes[idMesh].vertices[i].uv_x = uvs[i].x;
                meshes[idMesh].vertices[i].uv_y = uvs[i].y;
            }
        }
    }

    // ... use model ...
    tg3_model_free(&model);
    tg3_error_stack_free(&errors);

    return meshes;
}

} // namespace vke
