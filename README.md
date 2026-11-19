# VkEngine

For now it's only a small Vulkan 1.4 renderer displaying a **glTF 2.0** file using PBR. Written in C++23, built solo as a learning project (no AI-written code).
The long-term goal is a data-driven engine built around a custom ECS.

## Current state

Need to clean up some code, for example, I've rushed a bit when adding glTF loading (seeing meshes displayed was really cool, I went a bit too fast, need to split some methods into multiple methods to keep things tidy)

### Renderer
- **Vulkan 1.4** core features : dynamic rendering, synchronization2, buffer device address, descriptor indexing, scalar block layout, maintenance5
- **Bindless** : a single descriptor set holding arrays of textures and samplers, bound once per frame
- **Vertex pulling** : vertices read from one large buffer through its device address. Materials and scene data are also accessed by address
- 2 frames in flight, depth buffer, swapchain recreation on resize
- GPU memory allocation through VMA, instance/device/swapchain setup through vk-bootstrap

### Shading
- Cook-Torrance PBR, metallic-roughness workflow (GGX distribution, Smith geometry, Schlick Fresnel), Reinhard tone mapping
- One directional light and a constant ambient term
- Normal mapping (tangents provided by the glTF file, not computed if missing)

### glTF 2.0 loading (`.gltf` and `.glb`)
- Only 1 scene loaded
- Meshes and primitives: `POSITION`, `NORMAL`, `TANGENT`, `TEXCOORD_0` / `TEXCOORD_1`, `COLOR_0`
- Non-indexed primitives (indices are generated)
- Node hierarchy of the default scene, flattened to world matrices when loading the file
- Materials: base color, metallic-roughness and normal textures, with their factors
- Samplers (filter and wrap modes)
- Extension: `KHR_texture_transform`
- (partial) Error handling when loading: index and range checks, invalid primitives skipped, missing images replaced by a fallback texture, every issue reported through named `spdlog` loggers

### Debugging
- Validation layers in the Debug preset, including synchronization validation and GPU-assisted validation
- Shaders compiled with debug info (`-g`) in Debug

### Not implemented yet
- ECS and runtime transform hierarchy
- Image based lighting, shadows, multiple lights
- Mipmaps and anisotropic filtering
- Occlusion and emissive textures, `normalTexture.scale`
- Computing normals and tangents when the file does not provide them
- Primitive modes other than triangles, other extensions
- Debug UI, debug object names

## Building

### Requirements

(What I am working with, not tested on other OS)

- Windows with MSVC, CMake 3.24+ and Ninja
- [Vulkan SDK](https://vulkan.lunarg.com/) 1.4 (provides `glslc`), with the `VULKAN_SDK` environment variable set
- [SDL3](https://github.com/libsdl-org/SDL), found through the `SDL3_ROOT` environment variable
- A GPU and driver supporting Vulkan 1.4

Fetched automatically by CMake: vk-bootstrap, GLM, spdlog, Catch2.
Bundled in `external/`: tinygltf v3, VMA, stb_image.

### Build and run

The `debug` and `release` presets use Ninja and MSVC, which need the Visual Studio build environment:

- **VS Code** with the CMake Tools extension: *CMake: Select Configure Preset* → then *CMake: Build*, then *CMake: Run Without Debugging* (or *CMake: Debug*)

- **Command line**: use an *x64 Native Tools Command Prompt for VS* (a regular terminal won't find Ninja and `cl`), then:
```sh
cmake --preset debug          # or: release (validation layers off)
cmake --build --preset debug
```

The executable is written to `build/debug/`.

### Models

Models are not versioned. Put your glTF files in `Resources/Models/`.
The model to load is currently hardcoded in `src/App.cpp`.

## Controls

| Input | Action |
|---|---|
| `Enter` | Capture the mouse |
| `Esc` | Release the mouse |
| Mouse | Look around (while captured) |
| `W` `A` `S` `D` | Move |

## Project layout

```
include/VkEngine/   public headers
src/                engine sources
Resources/Shaders/  GLSL shaders (compiled to SPIR-V at build time)
Resources/Models/   glTF models (not versioned)
external/           bundled third-party headers
tests/              Catch2 unit tests
```

## Roadmap

- [x] Vulkan setup, dynamic rendering, triangle
- [x] Resource management with handles, VMA, vertex pulling
- [x] glTF loading, bindless textures and samplers
- [x] PBR shading and normal mapping
- [ ] Custom ECS
- [ ] Runtime transform hierarchy
- [ ] Support more glTF extensions, handle missing attributes
- [ ] Debug UI (ImGui?)
- [ ] Lighting: multiple lights, IBL, shadows
- [ ] GPU-driven rendering (want to explore GPU culling)
