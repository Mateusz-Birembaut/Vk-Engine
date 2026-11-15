#pragma once

#include <cstdint>

namespace vke {

template <typename T> struct Handle {
    uint32_t index{0};
    uint32_t gen{0};
};

} // namespace vke
