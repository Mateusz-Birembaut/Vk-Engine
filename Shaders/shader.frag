#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_gpu_shader_int64 : require

layout ( push_constant ) uniform constants
{
    uint64_t vertBuffAddr;
    uint samplerId;
    uint textureId;
} pc;

layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 color = texture(
        sampler2D(
            textures[nonuniformEXT(pc.textureId)],
            samplers[nonuniformEXT(pc.samplerId)])
        , uv);

    fragColor = color;
}
