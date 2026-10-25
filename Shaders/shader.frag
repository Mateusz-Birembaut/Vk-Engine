#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout ( push_constant ) uniform constants
{
    uint samplerId;
    uint textureId;
} PushConstants;

layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

layout (location = 1) in vec2 uv;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 color = texture(
        sampler2D(
            textures[nonuniformEXT(PushConstants.textureId)],
            samplers[nonuniformEXT(PushConstants.samplerId)])
        , uv);

    fragColor = color;
}
