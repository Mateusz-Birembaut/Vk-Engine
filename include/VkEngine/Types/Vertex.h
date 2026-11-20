#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace vke
{

// TODO : use an array for each group of data => vertices in one buffer, uv normal
struct Vertex
{
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 tangent{};
    glm::vec2 texCoord0{};
    glm::vec2 texCoord1{};
    glm::vec3 position{};
    glm::vec3 normal{};
};

} // namespace vke
