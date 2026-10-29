#pragma once

#include <vulkan/vulkan.h>

#include <stdexcept>

namespace vke {

inline void check(VkResult result, const char* msg)
{
    if (result != VK_SUCCESS) {
        throw std::runtime_error(msg);
    }
}

} // namespace vke
