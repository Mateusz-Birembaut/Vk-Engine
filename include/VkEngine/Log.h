#pragma once

#include <memory>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace vke {

namespace log {

inline std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink = nullptr;

inline std::shared_ptr<spdlog::logger> gltf{nullptr};
inline std::shared_ptr<spdlog::logger> vulkan{nullptr};

inline void initLoggers()
{
    console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::trace);
    console_sink->set_pattern("[%H:%M:%S:%e] [Console] [%n] [%^%l%$] %v");

    gltf = std::make_shared<spdlog::logger>("gltf", console_sink);
    gltf->set_level(spdlog::level::info);

    /*     vulkan = std::make_shared<spdlog::logger>("vulkan", console_sink);
        vulkan->set_level(spdlog::level::info); */
}

} // namespace log

} // namespace vke
