#include "VkEngine/GltfLoader.h"

#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <tiny_gltf_v3.h>
#include <utility>

#include "VkEngine/FileReader.h"
#include "VkEngine/Log.h"

namespace vke {

std::optional<tinygltf3::Model> loadGltfModel(const std::filesystem::path& path)
{
    tg3_parse_options opts;
    tinygltf3::ErrorStack errors;
    tinygltf3::Model model;

    std::string pathStr = path.string();
    const char* pathCstr = pathStr.c_str();
    const uint32_t pathSize = static_cast<uint32_t>(pathStr.size());

    tg3_parse_options_init(&opts);
    tg3_error_code err = tg3_parse_file(model.get(), errors.get(), pathCstr, pathSize, &opts);

    bool success = (err == TG3_OK);

    for (uint32_t IError = 0; IError < errors.count(); IError++) {
        const tg3_error_entry* errEntry = errors.entry(IError);

        std::string errMsg = (errEntry->message) ? errEntry->message : "(null)";

        switch (errEntry->severity) {
            case TG3_SEVERITY_INFO:
                log::gltf->info("{}", errMsg);
                break;
            case TG3_SEVERITY_WARNING:
                log::gltf->warn("{}", errMsg);
                break;
            case TG3_SEVERITY_ERROR:
            default:
                log::gltf->error("{}", errMsg);
        }
    }

    if (!success) {
        log::gltf->info("Failed to parse file {}", path);
        return std::nullopt;
    }

    return model;
}

std::optional<gltf::Primitive>
readPrimitiveData(const tg3_model& model, std::string_view meshName, uint32_t PrimIndex, const tg3_primitive& tgPrim)
{
    if (tgPrim.mode != TG3_MODE_TRIANGLES) {
        log::gltf->warn(
            "Mesh {} : Prim {}, Wrong primitive mode, primitive will be skipped, primitive mode must be Triangles (4)",
            meshName, PrimIndex
        );
        return std::nullopt;
    };

    gltf::Primitive primitive{
        .material = tgPrim.material,
    };

    uint32_t attributeCount = tgPrim.attributes_count;
    int32_t indicesPrim = tgPrim.indices;

    int32_t positionPrim = -1;
    int32_t normalPrim = -1;
    int32_t uvPrim0 = -1;
    int32_t uvPrim1 = -1;
    int32_t colorPrim = -1;
    int32_t tangPrim = -1;

    // get indices of attributes
    for (uint32_t IAttr = 0; IAttr < attributeCount; IAttr++) {
        auto& attribute = tgPrim.attributes[IAttr];

        if (strcmp(attribute.key.data, "POSITION") == 0) {
            positionPrim = attribute.value;
        } else if (strcmp(attribute.key.data, "NORMAL") == 0) {
            normalPrim = attribute.value;
        } else if (strcmp(attribute.key.data, "TEXCOORD_0") == 0) {
            uvPrim0 = attribute.value;
        } else if (strcmp(attribute.key.data, "TEXCOORD_1") == 0) {
            uvPrim1 = attribute.value;
        } else if (strcmp(attribute.key.data, "COLOR_0") == 0) {
            colorPrim = attribute.value;
        } else if (strcmp(attribute.key.data, "TANGENT") == 0) {
            tangPrim = attribute.value;
        }
    }

    if (positionPrim == -1) {
        log::gltf->warn("Mesh {} : Prim {}, No position attribute found, primitive skipped", meshName, PrimIndex);
        return std::nullopt;
    }

    const auto& vertexAcc = model.accessors[positionPrim];

    std::vector<glm::vec3> positions = readPrimitiveAttribute<float, 3>(model, vertexAcc);
    std::vector<glm::vec2> uvs0;
    std::vector<glm::vec2> uvs1;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec4> colors;
    std::vector<glm::vec4> tangs;

    // TODO : compute them if absent
    if (normalPrim != -1) {
        const auto& normalAcc = model.accessors[normalPrim];
        normals = readPrimitiveAttribute<float, 3>(model, normalAcc);
    } else {
        log::gltf->warn(
            "Mesh {} : Prim {}, No normal attribute found, normals and tangents must be computed", meshName, PrimIndex
        );
    }

    if (uvPrim0 != -1) {
        const auto& uvAcc = model.accessors[uvPrim0];
        switch (uvAcc.component_type) {
            case TG3_COMPONENT_TYPE_FLOAT:
                uvs0 = readPrimitiveAttribute<float, 2>(model, uvAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                uvs0 = readPrimitiveAttribute<uint8_t, 2>(model, uvAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                uvs0 = readPrimitiveAttribute<uint16_t, 2>(model, uvAcc);
                break;
        }
    } else {
        log::gltf->warn("No texCoord attribute found");
    }

    if (uvPrim1 != -1) {
        const auto& uvAcc = model.accessors[uvPrim1];
        switch (uvAcc.component_type) {
            case TG3_COMPONENT_TYPE_FLOAT:
                uvs1 = readPrimitiveAttribute<float, 2>(model, uvAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                uvs1 = readPrimitiveAttribute<uint8_t, 2>(model, uvAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                uvs1 = readPrimitiveAttribute<uint16_t, 2>(model, uvAcc);
                break;
        }
    }

    if (colorPrim != -1) {
        const auto& colorAcc = model.accessors[colorPrim];

        colors = readColors(model, colorAcc);
    }

    // TODO : compute them if absent
    if (tangPrim != -1) {
        const auto& tangAcc = model.accessors[tangPrim];

        tangs = readPrimitiveAttribute<float, 4>(model, tangAcc);
    } else {
        log::gltf->warn(
            "Mesh {} : Prim {}, No tangent attribute found, tangents must be computed", meshName, PrimIndex
        );
    }

    const auto& indexAcc = model.accessors[indicesPrim];

    switch (indexAcc.component_type) {
        case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
            primitive.indices = readIndices<uint8_t>(model, indexAcc);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
            primitive.indices = readIndices<uint16_t>(model, indexAcc);
            break;
        case TG3_COMPONENT_TYPE_UNSIGNED_INT:
            primitive.indices = readIndices<uint32_t>(model, indexAcc);
            break;
    }

    primitive.vertices.resize(positions.size());

    // fill vertices data
    for (size_t IVertex = 0; IVertex < positions.size(); IVertex++) {
        auto& vertex = primitive.vertices[IVertex];

        vertex.position = positions[IVertex];
        if (normalPrim != -1) {
            vertex.normal = normals[IVertex];
        }
        if (uvPrim0 != -1) {
            vertex.texCoord0 = uvs0[IVertex];

            // fill text coord 1 with values of uv0
            vertex.texCoord1 = uvs0[IVertex];
        }
        if (uvPrim1 != -1) {
            // if text coord 1 set, fill with values of uv1
            vertex.texCoord1 = uvs1[IVertex];
        }
        if (colorPrim != -1) {
            vertex.color = colors[IVertex];
        }
        if (tangPrim != -1) {
            vertex.tangent = tangs[IVertex];
        }
    }

    return primitive;
}

void fillMeshesData(const tg3_model& model, std::vector<gltf::Mesh>& impSceneMeshes)
{
    uint32_t meshCount = model.meshes_count;

    impSceneMeshes.reserve(meshCount);

    for (uint32_t IMesh = 0; IMesh < meshCount; ++IMesh) {
        gltf::Mesh mesh{};
        const tg3_mesh& tgMesh = model.meshes[IMesh];

        uint32_t primitivesCount = tgMesh.primitives_count;
        mesh.primitives.reserve(primitivesCount);

        for (uint32_t IPrim = 0; IPrim < primitivesCount; ++IPrim) {
            const auto& tgPrim = tgMesh.primitives[IPrim];

            const auto primRet =
                readPrimitiveData(model, std::string_view{tgMesh.name.data, tgMesh.name.len}, IPrim, tgPrim);

            if (primRet.has_value()) {
                mesh.primitives.push_back(std::move(primRet.value()));
            }
        }

        impSceneMeshes.push_back(std::move(mesh));
    }
}

void fillNodesData(const tg3_model& model, std::vector<gltf::Node>& impSceneNodes)
{
    impSceneNodes.reserve(model.nodes_count);

    // comply with gltf 2.1 => no more than 1 scene wil be loaded, skip if 0 scenes
    if (model.scenes_count == 0) {
        log::gltf->warn("No scenes found inside gltf file");
        return;
    }

    const auto& scene = (model.default_scene != -1) ? model.scenes[model.default_scene] : model.scenes[0];

    if (scene.nodes_count == 0) {
        log::gltf->warn(
            "Scene {} : No root nodes found inside gltf file", std::string_view{scene.name.data, scene.name.len}
        );
        return;
    }

    // iterating through rootNodes (listed in gltf -> scenes -> nodes)
    for (size_t IRootNode = 0; IRootNode < scene.nodes_count; IRootNode++) {
        const auto& rootNode = model.nodes[scene.nodes[IRootNode]];

        traverseNodes(model, rootNode, glm::mat4(1.0f), impSceneNodes); // recursive dfs
    }
}

void fillImagesData(const tg3_model& model, const std::filesystem::path& path, std::vector<gltf::Image>& impSceneImgs)
{
    uint32_t imageCount = model.images_count;
    impSceneImgs.reserve(imageCount);

    for (uint32_t IImage = 0; IImage < imageCount; IImage++) {
        const auto& image = model.images[IImage];

        if (image.uri.len != 0) {
            std::filesystem::path pathImage = path / image.uri.data;

            auto& img = impSceneImgs.emplace_back();
            auto imgData = readFile(pathImage);

            if (!imgData.has_value()) {
                log::gltf->warn(
                    "Image {} uri : {}, Failed to load image, fallback data will be used", IImage,
                    std::string_view{image.uri.data, image.uri.len}
                );
                // data will be empty, need to be checked when creating textures
            } else {
                img.data = std::move(imgData.value());
            }

        } else {
            const auto& bufferView = model.buffer_views[image.buffer_view];
            const auto& buffer = model.buffers[bufferView.buffer];

            auto& img = impSceneImgs.emplace_back();

            const auto* begin = &buffer.data.data[bufferView.byte_offset];
            img.data.bytes.assign(begin, begin + bufferView.byte_length);
        }
    }
}

void fillSamplerData(const tg3_model& model, std::vector<Sampler>& impSceneSampler)
{
    impSceneSampler.reserve(model.samplers_count);

    for (uint32_t ISampler = 0; ISampler < model.samplers_count; ISampler++) {
        const auto& tgSampler = model.samplers[ISampler];

        impSceneSampler.emplace_back(
            filterModeFrom(tgSampler.min_filter), filterModeFrom(tgSampler.mag_filter), wrapModeFrom(tgSampler.wrap_s),
            wrapModeFrom(tgSampler.wrap_t)
        );
    }
}

void fillTexturesData(const tg3_model& model, std::vector<gltf::Texture>& impSceneTextures)
{
    impSceneTextures.reserve(model.textures_count);

    for (uint32_t ITexture = 0; ITexture < model.textures_count; ITexture++) {
        const auto& texture = model.textures[ITexture];

        impSceneTextures.push_back(gltf::Texture{.imgIndex = texture.source, .samplerIndex = texture.sampler});
    }
}

void fillMaterialData(
    const tg3_model& model, std::vector<gltf::Material>& impSceneMaterials, std::vector<gltf::Image>& impSceneImgs
)
{
    impSceneMaterials.reserve(model.materials_count);

    const auto updateToSRGB = [&](uint32_t tgTextureIndex) -> void {
        if (tgTextureIndex != -1) {
            uint32_t imgIndex = model.textures[tgTextureIndex].source;

            if (imgIndex < impSceneImgs.size()) {
                impSceneImgs[imgIndex].colorSpace = ColorSpace::SRGB;
            }
        }
    };

    for (uint32_t IMaterial = 0; IMaterial < model.materials_count; IMaterial++) {
        const auto& tgMaterial = model.materials[IMaterial];

        gltf::Material material{
            .baseColorFactor = glm::vec4(glm::make_vec4(tgMaterial.pbr_metallic_roughness.base_color_factor)),

            .baseColorTexture{
                .texIndex = tgMaterial.pbr_metallic_roughness.base_color_texture.index,
                .texCoord = static_cast<uint32_t>(tgMaterial.pbr_metallic_roughness.base_color_texture.tex_coord),
            },

            .metallicFactor = static_cast<float>(tgMaterial.pbr_metallic_roughness.metallic_factor),

            .roughnessFactor = static_cast<float>(tgMaterial.pbr_metallic_roughness.roughness_factor),

            .metallicRoughnessTexture{
                .texIndex = tgMaterial.pbr_metallic_roughness.metallic_roughness_texture.index,
                .texCoord =
                    static_cast<uint32_t>(tgMaterial.pbr_metallic_roughness.metallic_roughness_texture.tex_coord),
            },

            .normalTexture{
                .texIndex = tgMaterial.normal_texture.index,
                .texCoord = static_cast<uint32_t>(tgMaterial.normal_texture.tex_coord),
            },

            .occlusionTexture{
                .texIndex = tgMaterial.occlusion_texture.index,
                .texCoord = static_cast<uint32_t>(tgMaterial.occlusion_texture.tex_coord),
            },

            .emissiveTexture{
                .texIndex = tgMaterial.emissive_texture.index,
                .texCoord = static_cast<uint32_t>(tgMaterial.emissive_texture.tex_coord),
            },

            .emissiveFactor = glm::vec3(glm::make_vec3(tgMaterial.emissive_factor)),
        };

        updateToSRGB(tgMaterial.pbr_metallic_roughness.base_color_texture.index);
        fillTextureRefExtData(material.baseColorTexture, tgMaterial.pbr_metallic_roughness.base_color_texture.ext);

        fillTextureRefExtData(
            material.metallicRoughnessTexture, tgMaterial.pbr_metallic_roughness.metallic_roughness_texture.ext
        );

        fillTextureRefExtData(material.normalTexture, tgMaterial.normal_texture.ext);

        fillTextureRefExtData(material.emissiveTexture, tgMaterial.emissive_texture.ext);

        fillTextureRefExtData(material.occlusionTexture, tgMaterial.occlusion_texture.ext);

        impSceneMaterials.push_back(std::move(material));
    }
}

void traverseNodes(
    const tg3_model& model, const tg3_node& tgNode, const glm::mat4& parentTransform,
    std::vector<gltf::Node>& impSceneNodes
)
{
    glm::mat4 localTransform{1.0f};

    if (tgNode.has_matrix) {
        localTransform = glm::mat4(glm::make_mat4(tgNode.matrix));
    } else {
        // create matrix from T R S
        glm::vec3 translation(glm::make_vec3(tgNode.translation));
        glm::quat rotation(glm::make_quat(tgNode.rotation));
        glm::vec3 scale(glm::make_vec3(tgNode.scale));

        glm::mat4 matTranslate = glm::translate(glm::mat4(1.0f), translation);
        glm::mat4 matRotate = glm::mat4_cast(rotation);
        glm::mat4 matScale = glm::scale(glm::mat4(1.0f), scale);

        localTransform = matTranslate * matRotate * matScale;
    }

    localTransform = parentTransform * localTransform;

    if (tgNode.mesh != -1) {
        impSceneNodes.emplace_back(tgNode.mesh, localTransform);
    }

    for (uint32_t IChild = 0; IChild < tgNode.children_count; IChild++) {
        uint32_t childNodeIndex = tgNode.children[IChild];

        if (childNodeIndex < model.nodes_count) {
            traverseNodes(model, model.nodes[tgNode.children[IChild]], localTransform, impSceneNodes);
        }
    }
}

WrapMode wrapModeFrom(int32_t value)
{
    switch (value) {
        case TG3_TEXTURE_WRAP_CLAMP_TO_EDGE:
            return WrapMode::CLAMP_TO_EDGE;
        case TG3_TEXTURE_WRAP_MIRRORED_REPEAT:
            return WrapMode::MIRRORED_REPEAT;
        default:
            return WrapMode::REPEAT;
    }
}

FilterMode filterModeFrom(int32_t value)
{
    switch (value) {
        // TODO : later split this when mipmap is in
        case TG3_TEXTURE_FILTER_NEAREST:
        case TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
        case TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            return FilterMode::NEAREST;
        default:
            return FilterMode::LINEAR;
    }
}

std::vector<glm::vec4> readColors(const tg3_model& model, const tg3_accessor& colorAcc)
{
    std::vector<glm::vec4> colors;

    if (colorAcc.type == TG3_TYPE_VEC4) {
        switch (colorAcc.component_type) {
            case TG3_COMPONENT_TYPE_FLOAT:
                colors = readPrimitiveAttribute<float, 4>(model, colorAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                colors = readPrimitiveAttribute<uint8_t, 4>(model, colorAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                colors = readPrimitiveAttribute<uint16_t, 4>(model, colorAcc);
                break;
        }
    } else if (colorAcc.type == TG3_TYPE_VEC3) {
        std::vector<glm::vec3> colorsV3;

        switch (colorAcc.component_type) {
            case TG3_COMPONENT_TYPE_FLOAT:
                colorsV3 = readPrimitiveAttribute<float, 3>(model, colorAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                colorsV3 = readPrimitiveAttribute<uint8_t, 3>(model, colorAcc);
                break;
            case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                colorsV3 = readPrimitiveAttribute<uint16_t, 3>(model, colorAcc);
                break;
        }

        colors.reserve(colorsV3.size());
        for (const auto& colorV3 : colorsV3) {
            colors.emplace_back(colorV3, 1.0f);
        }
    }

    return colors;
}

void fillTextureRefExtData(gltf::TextureInfo& textInfo, const tg3_extras_ext& extensionInfo)
{
    if (textInfo.texIndex < 0) {
        return;
    }

    for (uint32_t IExtension = 0; IExtension < extensionInfo.extensions_count; IExtension++) {
        const auto& ext = extensionInfo.extensions[IExtension];

        if (strcmp(ext.name.data, "KHR_texture_transform") == 0) {
            textInfo.transform = readTextureTransform(ext);
        }
    }
}

glm::mat3x2 readTextureTransform(const tg3_extension& ext)
{
    float rotationRad{0.0f};
    glm::vec2 scale{1.0f, 1.0f};
    glm::vec2 offset{0.0f, 0.0f};

    // TODO : cleanup later
    for (uint32_t IObject = 0; IObject < ext.value.object_count; IObject++) {
        const auto& obj = ext.value.object_data[IObject];

        if (strcmp(obj.key.data, "scale") == 0) {
            uint32_t scaleSize = std::min(obj.value.array_count, static_cast<uint32_t>(2));

            for (uint32_t IScaleScalar = 0; IScaleScalar < scaleSize; IScaleScalar++) {
                const auto& scalar = obj.value.array_data[IScaleScalar];

                if (scalar.type == TG3_VALUE_INT) {
                    scale[IScaleScalar] = (static_cast<float>(scalar.int_val));
                } else if (scalar.type == TG3_VALUE_REAL) {
                    scale[IScaleScalar] = (static_cast<float>(scalar.real_val));
                }
            }
        }

        if (strcmp(obj.key.data, "offset") == 0) {
            uint32_t offsetSize = std::min(obj.value.array_count, uint32_t{2});

            for (uint32_t IOffsetScalar = 0; IOffsetScalar < offsetSize; IOffsetScalar++) {
                const auto& scalar = obj.value.array_data[IOffsetScalar];

                if (scalar.type == TG3_VALUE_INT) {
                    offset[IOffsetScalar] = (static_cast<float>(scalar.int_val));
                } else if (scalar.type == TG3_VALUE_REAL) {
                    offset[IOffsetScalar] = (static_cast<float>(scalar.real_val));
                }
            }
        }
        if (strcmp(obj.key.data, "rotation") == 0) {
            if (obj.value.type == TG3_VALUE_INT) {
                rotationRad = (static_cast<float>(obj.value.int_val));
            } else if (obj.value.type == TG3_VALUE_REAL) {
                rotationRad = (static_cast<float>(obj.value.real_val));
            }
        }
    }

    // clang-format off
    glm::mat3 trMat = glm::mat3{
        1.0f,     0.0f,     0.0f, 
        0.0f,     1.0f,     0.0f,  
        offset.x, offset.y, 1.0f  
    };

    float cosRot = std::cos(rotationRad);
    float sinRot = std::sin(rotationRad);

    glm::mat3 rMat = glm::mat3{
        cosRot,  sinRot, 0.0f,
        sinRot,  cosRot, 0.0f,
        0.0f  ,  0.0f  , 1.0f  
    };

    glm::mat3 sMat = glm::mat3{
        scale.x, 0.0f,    0.0f, 
        0.0f,    scale.y, 0.0f,
        0.0f,    0.0f,    1.0f  
    };
    // clang-format on

    glm::mat3 uvTransform = trMat * rMat * sMat;

    return glm::mat3x2{
        glm::vec2{uvTransform[0][0], uvTransform[0][1]}, glm::vec2{uvTransform[1][0], uvTransform[1][1]},
        glm::vec2{uvTransform[2][0], uvTransform[2][1]}
    };
}

std::optional<gltf::ImportedScene> loadGltf(const std::filesystem::path& path)
{
    gltf::ImportedScene impScene{};

    // load tiny gltf 3 model from path, RAII wrapper
    auto modelLoaded = loadGltfModel(path);
    if (!modelLoaded.has_value()) {
        return {};
    }
    tinygltf3::Model model = std::move(modelLoaded).value();

    fillMeshesData(*model.get(), impScene.meshes);

    fillNodesData(*model.get(), impScene.nodes);

    fillImagesData(*model.get(), path.parent_path(), impScene.images);

    fillSamplerData(*model.get(), impScene.samplers);

    fillTexturesData(*model.get(), impScene.textures);

    fillMaterialData(*model.get(), impScene.materials, impScene.images);

    return impScene;
}

} // namespace vke