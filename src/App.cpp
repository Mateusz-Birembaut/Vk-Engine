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
#include "VkEngine/Renderer.h"
#include "VkEngine/VkEngineInfo.h"
#include "VkEngine/VulkanCtx.h"
#include "VkEngine/Window.h"

App::App() = default;

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
    const auto modelPath = exeDir / "Resources" / "Models" / "CarConcept.glb";
    // const auto modelPath = exeDir / "Resources" / "Models" / "CarConcept" / "CarConcept.gltf";
    //  const auto modelPath = exeDir / "Resources" / "Models" / "john_face" / "scene.gltf";
    //  const auto modelPath = exeDir / "Resources" / "Models" / "london_loop.glb";
    //  const auto modelPath = exeDir / "Resources" / "Models" / "city.glb";
    //  const auto modelPath = exeDir / "Resources" / "Models" / "Cartoon_City_Free.glb";
    const auto impScene = vke::loadGltf(modelPath);

    // stores for each mesh the handles of its primitives
    std::vector<std::vector<Handle<vke::GPUMesh>>> meshHandles(impScene.meshes.size());

    // stores each image handles
    std::vector<Handle<vke::GPUImage>> imageHandles(impScene.images.size());

    // stores each material handles
    std::vector<Handle<vke::GPUMaterial>> matHandles(impScene.materials.size());

    // upload each primitive for each mesh
    for (uint32_t IMesh = 0; IMesh < impScene.meshes.size(); IMesh++) {
        const vke::Mesh& mesh = impScene.meshes[IMesh];

        for (uint32_t IPrim = 0; IPrim < mesh.primitives.size(); IPrim++) {
            const vke::Primitive& prim = mesh.primitives[IPrim];

            meshHandles[IMesh].push_back(m_renderer->createMesh(prim.vertices, prim.indices));
        }
    }

    // upload each image
    for (uint32_t IImage = 0; IImage < impScene.images.size(); IImage++) {
        const auto& imageData = impScene.images[IImage];

        vke::CPUImage imgData{};

        // TODO do not use prefered channels
        imgData.data = stbi_load_from_memory(
            imageData.bytes.data(), static_cast<int>(imageData.bytes.size()), &imgData.width, &imgData.height,
            &imgData.channels, 4
        );

        if (imgData.data) {
            imgData.channels = 4;
            imageHandles[IImage] = m_renderer->createTexture(imgData);
            stbi_image_free(imgData.data);
        } else {
            throw std::runtime_error("[APP] Couldn't read image data");
        }
    }

    // upload each materials
    for (uint32_t IMaterial = 0; IMaterial < impScene.materials.size(); IMaterial++) {
        const auto& gltfMaterial = impScene.materials[IMaterial];

        vke::GPUMaterialInfo materialCreateInfo{
            .baseColorFactor = gltfMaterial.baseColorFactor,
            .metallicFactor = gltfMaterial.metallicFactor,
            .roughnessFactor = gltfMaterial.roughnessFactor,
        };

        if (gltfMaterial.baseColorTextureIndex == -1) {
            materialCreateInfo.baseColorTexture = m_renderer->fallbackTexture();
        } else {
            int32_t imageIndex = impScene.textures[gltfMaterial.baseColorTextureIndex].imgIndex;
            // int32_t samplerIndex = impScene.textures[gltfMaterial.baseColorTextureIndex].samplerIndex;
            if (imageIndex >= 0) {
                materialCreateInfo.baseColorTexture = {
                    .imageHandle = imageHandles[imageIndex], .samplerIndex = 0 /*TODO : upload samplers*/
                };
            } else {
                materialCreateInfo.baseColorTexture = m_renderer->fallbackTexture();
            }
        }

        if (gltfMaterial.metallicRoughnessTextureIndex == -1) {
            materialCreateInfo.metallicRoughnessTexture = m_renderer->fallbackTexture();
        } else {
            int32_t imageIndex = impScene.textures[gltfMaterial.metallicRoughnessTextureIndex].imgIndex;
            // int32_t samplerIndex = impScene.textures[gltfMaterial.metallicRoughnessTextureIndex].samplerIndex;
            if (imageIndex >= 0) {
                materialCreateInfo.metallicRoughnessTexture = {
                    .imageHandle = imageHandles[imageIndex], .samplerIndex = 0 /*TODO : upload samplers*/
                };
            } else {
                materialCreateInfo.metallicRoughnessTexture = m_renderer->fallbackTexture();
            }
        }

        matHandles[IMaterial] = m_renderer->createMaterial(materialCreateInfo);
    }

    std::vector<vke::RenderItem> renderItems;
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
}