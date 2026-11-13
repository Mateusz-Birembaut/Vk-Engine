#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_gpu_shader_int64 : require

layout ( push_constant ) uniform constants
{
    uint64_t sceneDataBufferAddr;
    uint samplerId;
    uint baseColorTextureIndex;
    vec4 baseColorFactor;
    mat4 model;
} pc;

layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 vertColor;

layout(location = 0) out vec4 fragColor;

void main() {
    vec4 baseColor = texture(
        sampler2D(
            textures[nonuniformEXT(pc.baseColorTextureIndex)],
            samplers[nonuniformEXT(pc.samplerId)])
        , uv);

    fragColor = vertColor * baseColor * pc.baseColorFactor; 
    //fragColor = vec4(normal + 0.5, 1.0f); 
}
