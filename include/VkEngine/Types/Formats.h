#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace vke {

struct Vertex {
    glm::vec3 position{};
    float uv_x{0.0f};
    glm::vec3 normal{};
    float uv_y{0.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

enum ColorSpace { SRGB, LINEAR };

} // namespace vke
