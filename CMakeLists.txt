cmake_minimum_required(VERSION 3.24)

enable_testing() 

project(VkEngine VERSION 0.1.0 LANGUAGES CXX)

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


include(FetchContent)
FetchContent_Declare(
    fetch_vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG        v1.4.313
)
FetchContent_MakeAvailable(fetch_vk_bootstrap)

include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

add_executable(${PROJECT_NAME})


add_executable(engine_tests tests/FileReader/fileReaderTests.cpp)
target_link_libraries(engine_tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(engine_tests PRIVATE include)
set_property(TARGET engine_tests PROPERTY CXX_STANDARD 23)

# -Wno-missing-field-initializers : l'init partielle par designated initializers
# (champs restants à zéro) est l'idiome standard pour les structs Vulkan
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wconversion -Wno-missing-field-initializers)
target_compile_options(engine_tests PRIVATE -Wall -Wextra -Wconversion -Wno-missing-field-initializers)

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

	PUBLIC
		FILE_SET HEADERS
		BASE_DIRS
			include
		FILES
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)

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



