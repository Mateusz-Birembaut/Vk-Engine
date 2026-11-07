cmake_minimum_required(VERSION 3.24)
project(VkEngine VERSION 0.1.0 LANGUAGES CXX)

option(VKENGINE_VALIDATION "Enable Vulkan validation layers" ON)

# ------------------- Dependencies -------------------
# SDL3   : found via the SDL3_ROOT environment variable
# Vulkan : found via the VULKAN_SDK environment variable
find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-shared)
find_package(Vulkan REQUIRED COMPONENTS glslc)

include(FetchContent)
FetchContent_Declare(vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v1.4.364
    GIT_SHALLOW    TRUE)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
    GIT_SHALLOW    TRUE)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(vk_bootstrap glm Catch2)

# ------------------- Helpers -------------------
function(vke_setup_target target)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_include_directories(${target} PRIVATE include)
    target_include_directories(${target} SYSTEM PRIVATE external)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wconversion -Wno-missing-field-initializers)
    endif()
endfunction()

# ------------------- Engine -------------------
configure_file(cmake/VkEngineInfo.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/VkEngine/VkEngineInfo.h @ONLY)

add_executable(VkEngine
    src/main.cpp
    src/App.cpp
    src/Window.cpp
    src/VulkanCtx.cpp
    src/Renderer.cpp
    src/External.cpp
    src/GPUResources.cpp
    src/Camera.cpp
    src/CameraController.cpp)

vke_setup_target(VkEngine)
target_include_directories(VkEngine PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(VkEngine PRIVATE SDL3::SDL3 Vulkan::Vulkan vk-bootstrap::vk-bootstrap glm::glm)
target_compile_definitions(VkEngine PRIVATE $<$<BOOL:${VKENGINE_VALIDATION}>:VKENGINE_VALIDATION>)
set_target_properties(VkEngine PROPERTIES COMPILE_WARNING_AS_ERROR ON)

# Copy runtime DLLs (SDL3.dll) next to the exe
if(WIN32)
    add_custom_command(TARGET VkEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:VkEngine> $<TARGET_FILE_DIR:VkEngine>
        COMMAND_EXPAND_LISTS)
endif()

# ------------------- Shaders (GLSL -> SPIR-V) -------------------
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/Resources/Shaders)
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    Resources/Shaders/*.vert Resources/Shaders/*.frag Resources/Shaders/*.comp)

foreach(shader IN LISTS SHADER_SOURCES)
    get_filename_component(name ${shader} NAME)
    set(spv ${SHADER_BINARY_DIR}/${name}.spv)
    add_custom_command(
        OUTPUT  ${spv}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
        COMMAND Vulkan::glslc ${shader} -o ${spv}
        DEPENDS ${shader}
        COMMENT "Compiling shader ${name}"
        VERBATIM)
    list(APPEND SPV_FILES ${spv})
endforeach()

add_custom_target(Shaders DEPENDS ${SPV_FILES})
add_dependencies(VkEngine Shaders)

# ------------------- Models -------------------
add_custom_command(TARGET VkEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Models"
        "$<TARGET_FILE_DIR:VkEngine>/Resources/Models"
    COMMENT "Copying model resources"
    VERBATIM)


# ------------------- Tests -------------------
enable_testing()

add_executable(engine_tests
    tests/FileReader/fileReaderTests.cpp
    tests/Pool/PoolTests.cpp)

vke_setup_target(engine_tests)
target_link_libraries(engine_tests PRIVATE Catch2::Catch2WithMain Vulkan::Vulkan)
target_compile_definitions(engine_tests PRIVATE
    FILEREADER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/FileReader/data")

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(engine_tests)
