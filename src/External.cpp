#include <cstdio>

#define TINYGLTF3_IMPLEMENTATION
#define TINYGLTF3_ENABLE_FS
#define TINYGLTF3_ENABLE_STB_IMAGE
#include <tiny_gltf_v3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define VMA_LEAK_LOG_FORMAT(format, ...)                                                                               \
    do {                                                                                                               \
        printf((format), __VA_ARGS__);                                                                                 \
        printf("\n");                                                                                                  \
    } while (false)

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
