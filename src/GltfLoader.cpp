#include "VkEngine/GltfLoader.h"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <tiny_gltf_v3.h>
#include <utility>

#include "VkEngine/Types.h"

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
        for (uint32_t i = 0; i < errors.count(); i++) {
            const tg3_error_entry* errEntry = errors.entry(i);
            fprintf(stderr, "[%d] %s\n", (int) errEntry->severity, errEntry->message ? errEntry->message : "(null)");
        }
        return std::nullopt;
    }

    return model;
}

void fillMeshesData(const tg3_model& model, std::vector<Mesh>& impSceneMeshes)
{
    uint32_t meshesCount = model.meshes_count;

    impSceneMeshes.reserve(meshesCount);

    for (uint32_t IMesh = 0; IMesh < meshesCount; ++IMesh) {
        vke::Mesh mesh{};
        const tg3_mesh& tgMesh = model.meshes[IMesh];

        uint32_t primitivesCount = model.meshes[IMesh].primitives_count;
        mesh.primitives.reserve(primitivesCount);

        for (uint32_t IPrim = 0; IPrim < primitivesCount; ++IPrim) {
            vke::Primitive primitive{};

            const tg3_primitive& tgPrim = tgMesh.primitives[IPrim];

            uint32_t attributeCount = tgPrim.attributes_count;
            int32_t indicesPrim = tgPrim.indices;

            int32_t positionPrim = -1;
            int32_t normalPrim = -1;
            int32_t uvPrim = -1;

            // get indices of attributes
            for (uint32_t IAttr = 0; IAttr < attributeCount; IAttr++) {
                auto& attribute = tgPrim.attributes[IAttr];

                if (strcmp(attribute.key.data, "POSITION") == 0) {
                    positionPrim = attribute.value;
                } else if (strcmp(attribute.key.data, "NORMAL") == 0) {
                    normalPrim = attribute.value;
                } else if (strcmp(attribute.key.data, "TEXCOORD_0") == 0) {
                    uvPrim = attribute.value;
                }
            }

            const auto& vertexAcc = model.accessors[positionPrim];

            std::vector<glm::vec3> positions = readPrimitiveAttribute<float, 3>(model, vertexAcc);
            std::vector<glm::vec2> uvs;
            std::vector<glm::vec3> normals;

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
            for (size_t i = 0; i < positions.size(); i++) {
                primitive.vertices[i].position = positions[i];
                if (normalPrim != -1) {
                    primitive.vertices[i].normal = normals[i];
                }
                if (uvPrim != -1) {
                    primitive.vertices[i].uv_x = uvs[i].x;
                    primitive.vertices[i].uv_y = uvs[i].y;
                }
            }

            // add primitive to mesh
            mesh.primitives.push_back(std::move(primitive)); // prevent copy with move
        }

        impSceneMeshes.push_back(std::move(mesh)); // add mesh to scene meshes, prevent copy with move
    }
}

void fillNodesData(const tg3_model& model, std::vector<Node>& impSceneNodes)
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

void traverseNodes(
    const tg3_model& model, const tg3_node& tgNode, const glm::mat4& parentTransform, std::vector<Node>& impSceneNodes
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

    for (uint32_t i = 0; i < tgNode.children_count; i++) {
        traverseNodes(model, model.nodes[tgNode.children[i]], localTransform, impSceneNodes);
    }
}

ImportedScene vke::loadGltf(const std::filesystem::path& path)
{
    ImportedScene impScene{};

    // load tiny gltf 3 model from path
    auto modelLoaded = loadGltfModel(path);
    if (!modelLoaded.has_value()) {
        return {};
    }
    tinygltf3::Model model = std::move(modelLoaded).value();

    fillMeshesData(*model.get(), impScene.meshes);

    fillNodesData(*model.get(), impScene.nodes);

    return impScene;
}

} // namespace vke