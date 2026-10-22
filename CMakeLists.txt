cmake_minimum_required(VERSION 3.24)

project(VkEngine VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

option(VkEngine_VENDORED "Use vendored libraries" OFF)
option(VKENGINE_VALIDATION "Enable Vulkan validation layers" ON)

# -------------------SDL----------------------------
if(VkEngine_VENDORED)
    # This assumes you have added SDL as a submodule in vendored/SDL
    add_subdirectory(vendored/SDL EXCLUDE_FROM_ALL)
else()
    find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
endif()

# -------------------VULKAN----------------------------
if(VkEngine_VENDORED)
    add_subdirectory(vendored/Vulkan EXCLUDE_FROM_ALL)
else()
    find_package(Vulkan REQUIRED)
endif()


#-------------------VK BOOTSTRAP----------------------------
include(FetchContent)
FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v1.4.364
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

#-------------------Catch2----------------------------
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

#-------------------GLM----------------------------
include(FetchContent)
FetchContent_Declare(
	glm
	GIT_REPOSITORY	https://github.com/g-truc/glm.git
	GIT_TAG 1.0.3
)
FetchContent_MakeAvailable(glm)

add_executable(${PROJECT_NAME})

add_executable(engine_tests tests/FileReader/fileReaderTests.cpp
    tests/Pool/PoolTests.cpp)
target_link_libraries(engine_tests PRIVATE Catch2::Catch2WithMain Vulkan::Vulkan)
target_include_directories(engine_tests PRIVATE include)
target_include_directories(engine_tests PRIVATE external/)
set_property(TARGET engine_tests PROPERTY CXX_STANDARD 23)

# register each Catch2 TEST_CASE with CTest (VS Code Testing panel)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(engine_tests)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
    target_compile_options(engine_tests PRIVATE /W4)
else()
    # Partial designated initializers are the usual Vulkan struct initialization idiom.
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wconversion -Wno-missing-field-initializers)
    target_compile_options(engine_tests PRIVATE -Wall -Wextra -Wconversion -Wno-missing-field-initializers)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_WARNING_AS_ERROR ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)

# -------------------VkEngineInfo----------------------------
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VkEngineInfo.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/VkEngine/VkEngineInfo.h
    @ONLY
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

# -------------------Src/Headers---------------------------
target_sources( ${PROJECT_NAME}
	PRIVATE
		src/main.cpp
        src/App.cpp
        src/Window.cpp
        src/VulkanCtx.cpp
        src/Renderer.cpp
        src/External.cpp
        src/GPUResources.cpp

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
			include
		FILES
)

target_include_directories(${PROJECT_NAME} PRIVATE external/)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        VERBATIM
    )
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<BOOL:${VKENGINE_VALIDATION}>:VKENGINE_VALIDATION>)
target_compile_definitions(engine_tests PRIVATE FILEREADER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/FileReader/data")


# --- Shaders GLSL to SPIR-V ---
find_program(GLSLC glslc REQUIRED)

set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shaders)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/Shaders)

file(GLOB SHADER_SOURCES
    ${SHADER_SOURCE_DIR}/*.vert
    ${SHADER_SOURCE_DIR}/*.frag
    ${SHADER_SOURCE_DIR}/*.comp)

foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_FILE ${SHADER_BINARY_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
        COMMAND ${GLSLC} ${SHADER} -o ${SPV_FILE}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}")

    list(APPEND SPV_FILES ${SPV_FILE})
endforeach()

add_custom_target(Shaders DEPENDS ${SPV_FILES})
add_dependencies(${PROJECT_NAME} Shaders) 



