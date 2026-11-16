#include "VkEngine/GltfLoader.h"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <stb_image.h>
#include <tiny_gltf_v3.h>
#include <utility>

#include "VkEngine/FileReader.h"

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
    tg3_error_stack_init(errors.get());

    tg3_error_code err = tg3_parse_file(model.get(), errors.get(), pathCstr, pathSize, &opts);
    if (err != TG3_OK) {
        for (uint32_t IError = 0; IError < errors.count(); IError++) {
            const tg3_error_entry* errEntry = errors.entry(IError);
            fprintf(stderr, "[%d] %s\n", (int) errEntry->severity, errEntry->message ? errEntry->message : "(null)");
        }
        return std::nullopt;
    }

    return model;
}

void fillMeshesData(const tg3_model& model, std::vector<gltf::Mesh>& impSceneMeshes)
{
    uint32_t meshCount = model.meshes_count;

    impSceneMeshes.reserve(meshCount);

    for (uint32_t IMesh = 0; IMesh < meshCount; ++IMesh) {
        vke::gltf::Mesh mesh{};
        const tg3_mesh& tgMesh = model.meshes[IMesh];

        uint32_t primitivesCount = model.meshes[IMesh].primitives_count;
        mesh.primitives.reserve(primitivesCount);

        for (uint32_t IPrim = 0; IPrim < primitivesCount; ++IPrim) {
            vke::gltf::Primitive primitive{};

            const tg3_primitive& tgPrim = tgMesh.primitives[IPrim];

            uint32_t attributeCount = tgPrim.attributes_count;
            int32_t indicesPrim = tgPrim.indices;
            primitive.material = tgPrim.material;

            int32_t positionPrim = -1;
            int32_t normalPrim = -1;
            int32_t uvPrim = -1;
            int32_t colorPrim = -1;

            // get indices of attributes
            for (uint32_t IAttr = 0; IAttr < attributeCount; IAttr++) {
                auto& attribute = tgPrim.attributes[IAttr];

                if (strcmp(attribute.key.data, "POSITION") == 0) {
                    positionPrim = attribute.value;
                } else if (strcmp(attribute.key.data, "NORMAL") == 0) {
                    normalPrim = attribute.value;
                } else if (strcmp(attribute.key.data, "TEXCOORD_0") == 0) {
                    uvPrim = attribute.value;
                } else if (strcmp(attribute.key.data, "COLOR_0") == 0) {
                    colorPrim = attribute.value;
                }
            }

            const auto& vertexAcc = model.accessors[positionPrim];

            std::vector<glm::vec3> positions = readPrimitiveAttribute<float, 3>(model, vertexAcc);
            std::vector<glm::vec2> uvs;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec4> colors;

            if (normalPrim != -1) {
                const auto& normalAcc = model.accessors[normalPrim];
                normals = readPrimitiveAttribute<float, 3>(model, normalAcc);
            }

            if (uvPrim != -1) {
                const auto& uvAcc = model.accessors[uvPrim];
                switch (uvAcc.component_type) {
                    case TG3_COMPONENT_TYPE_FLOAT:
                        uvs = readPrimitiveAttribute<float, 2>(model, uvAcc);
                        break;
                    case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
                        uvs = readPrimitiveAttribute<uint8_t, 2>(model, uvAcc);
                        break;
                    case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
                        uvs = readPrimitiveAttribute<uint16_t, 2>(model, uvAcc);
                        break;
                }
            }

            if (colorPrim != -1) {
                const auto& colorAcc = model.accessors[colorPrim];

                colors = readColors(model, colorAcc);
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
                primitive.vertices[IVertex].position = positions[IVertex];
                if (normalPrim != -1) {
                    primitive.vertices[IVertex].normal = normals[IVertex];
                }
                if (uvPrim != -1) {
                    primitive.vertices[IVertex].uv_x = uvs[IVertex].x;
                    primitive.vertices[IVertex].uv_y = uvs[IVertex].y;
                }
                if (colorPrim != -1) {
                    primitive.vertices[IVertex].color = colors[IVertex];
                }
            }

            // add primitive to mesh
            mesh.primitives.push_back(std::move(primitive));
        }

        impSceneMeshes.push_back(std::move(mesh));
    }
}

void fillNodesData(const tg3_model& model, std::vector<gltf::Node>& impSceneNodes)
{
    impSceneNodes.reserve(model.nodes_count);

    // comply with gltf 2.1 => no more than 1 scene wil be loaded, skip if 0 scenes
    if (model.scenes_count == 0) {
        std::cerr << "[GltfLoader] No scenes found inside gltf file";
        return;
    }

    const auto& scene = (model.default_scene != -1) ? model.scenes[model.default_scene] : model.scenes[0];

    // iterating through rootNodes (listed in gltf -> scenes -> nodes)
    for (size_t IRootNode = 0; IRootNode < scene.nodes_count; IRootNode++) {
        const auto& rootNode = model.nodes[scene.nodes[IRootNode]];

        traverseNodes(model, rootNode, glm::mat4(1.0f), impSceneNodes); // recursive dfs
    }
}

void fillImagesData(const tg3_model& model, const std::filesystem::path& path, std::vector<gltf::Image>& impSceneImgs)
{
    impSceneImgs.reserve(model.images_count);

    for (uint32_t IImage = 0; IImage < model.images_count; IImage++) {
        const auto& image = model.images[IImage];
        if (image.uri.len != 0) {
            std::filesystem::path pathImage = path / image.uri.data;

            auto imgData = readFile(pathImage);
            if (!imgData.has_value()) {
                // for now error TODO : add fallback texture here ?
                throw std::runtime_error("[GltfLoader] Couldn't read image bytes");
            }

            auto& img = impSceneImgs.emplace_back();
            img.data = std::move(imgData.value());

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
        // if source -1 still added, default sampler will be used
        impSceneTextures.push_back(gltf::Texture{.imgIndex = texture.source, .samplerIndex = texture.sampler});
    }
}

void fillMaterialData(
    const tg3_model& model, std::vector<gltf::Material>& impSceneMaterials, std::vector<gltf::Image>& impSceneImgs
)
{
    impSceneMaterials.reserve(model.materials_count);

    for (uint32_t IMaterial = 0; IMaterial < model.materials_count; IMaterial++) {
        const auto& tgMaterial = model.materials[IMaterial];

        // if image is used in metallic_roughness_texture then update its color space
        if (tgMaterial.pbr_metallic_roughness.metallic_roughness_texture.index != -1) {
            uint32_t metallicTextureIndex = tgMaterial.pbr_metallic_roughness.metallic_roughness_texture.index;
            uint32_t metallicImageIndex = model.textures[metallicTextureIndex].source;

            if (metallicImageIndex < impSceneImgs.size()) {
                impSceneImgs[metallicImageIndex].colorSpace = ColorSpace::LINEAR;
            }
        }

        impSceneMaterials.push_back(
            gltf::Material{
                .baseColorFactor = glm::vec4(glm::make_vec4(tgMaterial.pbr_metallic_roughness.base_color_factor)),
                .baseColorTextureIndex = tgMaterial.pbr_metallic_roughness.base_color_texture.index,
                .metallicFactor = static_cast<float>(tgMaterial.pbr_metallic_roughness.metallic_factor),
                .roughnessFactor = static_cast<float>(tgMaterial.pbr_metallic_roughness.roughness_factor),
                .metallicRoughnessTextureIndex = tgMaterial.pbr_metallic_roughness.metallic_roughness_texture.index,
                .normalTextureIndex = tgMaterial.normal_texture.index,
                .occlusionTexture = tgMaterial.occlusion_texture.index,
                .emissiveTexture = tgMaterial.emissive_texture.index,
            }
        );
    }
}

void traverseNodes(
    const tg3_model& model, const tg3_node& tgNode, const glm::mat4& parentTransform,
    std::vector<gltf::Node>& impSceneNodes
)
{
    glm::mat4 localTransform{1.0f};

    if (tgNode.has_matrix) {
        // create matrix from stored matrix
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
        traverseNodes(model, model.nodes[tgNode.children[IChild]], localTransform, impSceneNodes);
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
        case TG3_TEXTURE_FILTER_NEAREST | TG3_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST |
            TG3_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
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

gltf::ImportedScene vke::loadGltf(const std::filesystem::path& path)
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