#include "VkEngine/App.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_timer.h>

#include <glm/gtc/matrix_transform.hpp>

#if defined(__linux__)
#include <SDL3/SDL_main.h>
#endif

#include <filesystem>
#include <iostream>
#include <numbers>
#include <stb_image.h>
#include <stdexcept>

#include "VkEngine/CameraController.h"
#include "VkEngine/FileReader.h"
#include "VkEngine/GltfLoader.h"
#include "VkEngine/Log.h"
#include "VkEngine/Renderer.h"
#include "VkEngine/VkEngineInfo.h"
#include "VkEngine/VulkanCtx.h"
#include "VkEngine/Window.h"

namespace vke {

App::App()
{
    log::initLoggers();
}

App::~App()
{
    SDL_Quit();
}

void App::init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        throw std::runtime_error(std::string("[SDL] Couldn't init  : ") + SDL_GetError());

    if (!SDL_SetAppMetadata(app::NAME.data(), app::VERSION.data(), "vkeID"))
        SDL_Log("%s", "Could not set app meta data");

    m_window = std::make_unique<Window>();

    m_ctx = std::make_unique<VulkanCtx>();
    m_ctx->init(m_window.get());

    std::filesystem::path exeDir = SDL_GetBasePath();
    const auto shadePath = exeDir / "Resources" / "Shaders";

    m_renderer = std::make_unique<Renderer>();
    m_renderer->init(m_ctx.get(), shadePath);

    m_cameraController = std::make_unique<CameraController>();
}

void App::run()
{
    init();

    std::filesystem::path exeDir = SDL_GetBasePath();
    // const auto modelPath = exeDir / "Resources" / "Models" / "Box" / "Box.gltf";
    // const auto modelPath = exeDir / "Resources" / "Models" / "AntiqueCamera.glb";
    // const auto modelPath = exeDir / "Resources" / "Models" / "steampunk_underwater_explorer" / "scene.gltf";
    // const auto modelPath = exeDir / "Resources" / "Models" / "CarConcept" / "CarConcept.gltf";
    // const auto modelPath = exeDir / "Resources" / "Models" / "CarConcept.glb";
    const auto modelPath = exeDir / "Resources" / "Models" / "john_face" / "scene.gltf";
    // const auto modelPath = exeDir / "Resources" / "Models" / "microphone_gxl_066_bafhcteks.glb";
    // const auto modelPath = exeDir / "Resources" / "Models" / "free_1975_porsche_911_930_turbo.glb";
    const auto impSceneRet = loadGltf(modelPath);

    if (!impSceneRet.has_value()) {
        return;
    }

    const auto impScene = std::move(impSceneRet.value());

    // stores for each mesh the handles of its primitives
    std::vector<std::vector<Handle<GPUMesh>>> meshHandles(impScene.meshes.size());

    // stores each image handles
    std::vector<Handle<GPUImage>> imageHandles(impScene.images.size());

    // std::vector of index of
    std::vector<uint32_t> samplerIndices(impScene.samplers.size());

    // stores each material handles
    std::vector<Handle<GPUMaterial>> matHandles(impScene.materials.size());

    // store
    std::vector<Texture> textures(impScene.textures.size());

    const auto& fallbackTexture = m_renderer->fallbackTexture();

    // upload each primitive for each mesh
    for (uint32_t IMesh = 0; IMesh < impScene.meshes.size(); IMesh++) {
        const gltf::Mesh& mesh = impScene.meshes[IMesh];

        for (uint32_t IPrim = 0; IPrim < mesh.primitives.size(); IPrim++) {
            const gltf::Primitive& prim = mesh.primitives[IPrim];

            meshHandles[IMesh].push_back(m_renderer->createMesh(prim.vertices, prim.indices));
        }
    }

    // upload each image
    for (uint32_t IImage = 0; IImage < impScene.images.size(); IImage++) {
        const auto& img = impScene.images[IImage];

        ImageDesc cpuImage{.colorSpace = img.colorSpace};

        // TODO do not use prefered channels
        cpuImage.data = stbi_load_from_memory(
            img.data.bytes.data(), static_cast<int>(img.data.bytes.size()), &cpuImage.width, &cpuImage.height,
            &cpuImage.channels, 4
        );

        if (cpuImage.data) {
            cpuImage.channels = 4;
            imageHandles[IImage] = m_renderer->createTexture(cpuImage);
            stbi_image_free(cpuImage.data);
        } else {
            throw std::runtime_error("[APP] Couldn't read image data");
        }
    }

    // upload or get samplers
    for (uint32_t ISampler = 0; ISampler < impScene.samplers.size(); ISampler++) {
        const auto& sampler = impScene.samplers[ISampler];
        samplerIndices[ISampler] = m_renderer->getOrCreateSampler(sampler);
    }

    // create Textures
    for (uint32_t ITexture = 0; ITexture < impScene.textures.size(); ITexture++) {
        const auto& importedTexture = impScene.textures[ITexture];

        // check
        Texture texture{
            .imageHandle = (importedTexture.imgIndex >= 0 && importedTexture.imgIndex < imageHandles.size())
                               ? imageHandles[importedTexture.imgIndex]
                               : fallbackTexture.imageHandle,
            .samplerIndex = (importedTexture.samplerIndex >= 0 && importedTexture.samplerIndex < samplerIndices.size())
                                ? samplerIndices[importedTexture.samplerIndex]
                                : fallbackTexture.samplerIndex,
        };

        textures[ITexture] = (std::move(texture));
    }

    // upload each materials
    for (uint32_t IMaterial = 0; IMaterial < impScene.materials.size(); IMaterial++) {
        const auto& gltfMaterial = impScene.materials[IMaterial];

        MaterialDesc materialDesc{
            .baseColorFactor = gltfMaterial.baseColorFactor,

            .baseColorTexture{
                .transform = gltfMaterial.baseColorTexture.transform,
                .texCoord = gltfMaterial.baseColorTexture.texCoord,
            },

            .metallicRoughnessTexture{
                .transform = gltfMaterial.metallicRoughnessTexture.transform,
                .texCoord = gltfMaterial.metallicRoughnessTexture.texCoord,
            },

            .normalTexture{
                .transform = gltfMaterial.normalTexture.transform,
                .texCoord = gltfMaterial.normalTexture.texCoord,
            },

            .occlusionTexture{
                .transform = gltfMaterial.occlusionTexture.transform,
                .texCoord = gltfMaterial.occlusionTexture.texCoord,
            },

            .emissiveTexture{
                .transform = gltfMaterial.emissiveTexture.transform,
                .texCoord = gltfMaterial.emissiveTexture.texCoord,
            },

            .emissiveFactor = gltfMaterial.emissiveFactor,

            .metallicFactor = gltfMaterial.metallicFactor,

            .roughnessFactor = gltfMaterial.roughnessFactor,
        };

        if (gltfMaterial.baseColorTexture.texIndex >= 0) {
            materialDesc.baseColorTexture.texture = textures[gltfMaterial.baseColorTexture.texIndex];
        } else {
            materialDesc.baseColorTexture.texture = fallbackTexture;
        }

        if (gltfMaterial.metallicRoughnessTexture.texIndex >= 0) {
            materialDesc.metallicRoughnessTexture.texture = textures[gltfMaterial.metallicRoughnessTexture.texIndex];
        }

        if (gltfMaterial.normalTexture.texIndex >= 0) {
            materialDesc.normalTexture.texture = textures[gltfMaterial.normalTexture.texIndex];
        }

        if (gltfMaterial.emissiveTexture.texIndex >= 0) {
            materialDesc.emissiveTexture.texture = textures[gltfMaterial.emissiveTexture.texIndex];
        }

        if (gltfMaterial.occlusionTexture.texIndex >= 0) {
            materialDesc.occlusionTexture.texture = textures[gltfMaterial.occlusionTexture.texIndex];
        }

        matHandles[IMaterial] = m_renderer->createMaterial(materialDesc);
    }

    std::vector<RenderItem> renderItems;
    renderItems.reserve(impScene.nodes.size());

    for (uint32_t INode = 0; INode < impScene.nodes.size(); INode++) {
        const auto& node = impScene.nodes[INode];

        if (node.meshIndex == -1 || node.meshIndex >= impScene.meshes.size()) {
            continue;
        }

        const auto& mesh = impScene.meshes[node.meshIndex];
        const auto& meshHandlesPrims = meshHandles[node.meshIndex];

        for (uint32_t IPrimHandles = 0; IPrimHandles < meshHandlesPrims.size(); IPrimHandles++) {
            // there is as mush primiive handles as mesh primitives so should be ok
            const auto& prim = mesh.primitives[IPrimHandles];

            if (prim.material != -1) {
                renderItems.emplace_back(node.transform, meshHandlesPrims[IPrimHandles], matHandles[prim.material]);

            } else {
                renderItems.emplace_back(
                    node.transform, meshHandlesPrims[IPrimHandles], m_renderer->fallbackMaterial()
                );
            }

            // 0 equals to fallback texture
        }
    }

    uint64_t previousTime = SDL_GetTicksNS();
    VkExtent2D previousExtent{};

    while (m_running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                    m_window->setMouseCapture(false);

                } else if (e.key.scancode == SDL_SCANCODE_RETURN) {
                    m_window->setMouseCapture(true);
                }
            } else if (e.type == SDL_EVENT_MOUSE_MOTION && m_window->isCapturingMouse()) {
                m_cameraController->handleMouseMotion(e.motion.xrel, e.motion.yrel);

            } else if (e.type == SDL_EVENT_WINDOW_MINIMIZED) {
                m_window->setMinimized(true);

            } else if (e.type == SDL_EVENT_WINDOW_RESTORED) {
                m_window->setMinimized(false);
            } else if (e.type == SDL_EVENT_QUIT) {
                m_running = false;
            }
        }

        uint64_t currentTime = SDL_GetTicksNS();
        // delta time in s, dt < 0.1s to limit range of motion after alt tabing
        float dt = std::min(static_cast<float>(currentTime - previousTime) / 1e9f, 0.1f);
        previousTime = currentTime;

        m_cameraController->handleKeyboard(dt);

        if (!m_running)
            break;

        // update game state, draw the current frame
        if (!m_window->isMinimized()) {
            m_renderer->drawFrame(renderItems, m_cameraController->renderView());

            // update after swapchain recreation
            VkExtent2D currentExtent = m_ctx->swapchainExtent();
            if (currentExtent.width != 0 && currentExtent.height != 0 &&
                (currentExtent.height != previousExtent.height || currentExtent.width != previousExtent.width)) {
                previousExtent = currentExtent;
                m_cameraController->updateAspectRatio(
                    static_cast<float>(currentExtent.width) / static_cast<float>(currentExtent.height)
                );
            }
        }
    }
} // namespace vke

} // namespace vke