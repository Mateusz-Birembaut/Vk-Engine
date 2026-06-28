cmake_minimum_required(VERSION 3.24)


project(VkEngine VERSION 0.1.0 LANGUAGES CXX)

option(VkEngine_VENDORED "Use vendored libraries" OFF)

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


add_executable(${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY COMPILE_WARNING_AS_ERROR ON)

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
        src/VkEngine.cpp
        src/Window.cpp

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