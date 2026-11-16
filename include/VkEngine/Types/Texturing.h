#pragma once

namespace vke {

enum class ColorSpace {
    SRGB,
    LINEAR,
};

enum class FilterMode {
    NEAREST,
    LINEAR,
};

enum class WrapMode {
    REPEAT,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
};

struct Sampler {
    FilterMode minFilter{FilterMode::LINEAR};
    FilterMode magFilter{FilterMode::LINEAR};
    WrapMode wrapS{WrapMode::REPEAT};
    WrapMode wrapT{WrapMode::REPEAT};

    bool operator==(const Sampler&) const = default;
};

} // namespace vke
