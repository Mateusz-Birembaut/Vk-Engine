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
    // const auto modelPath = exeDir / "Resources" / "Models" / "CarConcept.glb";
    // const auto modelPath = exeDir / "Resources" / "Models" / "CarConcept" / "CarConcept.gltf";
    // const auto modelPath = exeDir / "Resources" / "Models" / "john_face" / "scene.gltf";
    // const auto modelPath = exeDir / "Resources" / "Models" / "london_loop.glb";
    // const auto modelPath = exeDir / "Resources" / "Models" / "city.glb";
    const auto modelPath = exeDir / "Resources" / "Models" / "Cartoon_City_Free.glb";
    const auto impScene = vke::loadGltf(modelPath);

    // stores for each mesh the handles of its primitives
    std::vector<std::vector<Handle<vke::GPUMesh>>> meshHandles(impScene.meshes.size());

    // stores each image handles
    std::vector<Handle<vke::GPUImage>> imageHandles(impScene.images.size());

    // upload each primitive for each mesh
    for (uint32_t IMesh = 0; IMesh < impScene.meshes.size(); IMesh++) {
        const vke::Mesh& mesh = impScene.meshes[IMesh];

        for (uint32_t IPrim = 0; IPrim < mesh.primitives.size(); IPrim++) {
            const vke::Primitive& prim = mesh.primitives[IPrim];

            meshHandles[IMesh].push_back(m_renderer->createMesh(prim.vertices, prim.indices));
        }
    }

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
                const auto& primMaterial = impScene.materials[prim.material];

                if (primMaterial.baseColorTextureIndex == -1) {
                    renderItems.emplace_back(
                        node.transform, primMaterial.baseColorFactor, meshHandlesPrims[IPrimHandles],
                        Handle<vke::GPUImage>{}
                    );
                } else {
                    uint32_t baseColorTextureIndex = static_cast<uint32_t>(primMaterial.baseColorTextureIndex);
                    uint32_t imageIndex = impScene.textures[baseColorTextureIndex].imgIndex;

                    renderItems.emplace_back(
                        node.transform, primMaterial.baseColorFactor, meshHandlesPrims[IPrimHandles],
                        imageHandles[imageIndex]
                    );
                }
            } else {
                renderItems.emplace_back(
                    node.transform, glm::vec4{1.0f, 1.0f, 1.0f, 1.0f}, meshHandlesPrims[IPrimHandles],
                    Handle<vke::GPUImage>{}
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