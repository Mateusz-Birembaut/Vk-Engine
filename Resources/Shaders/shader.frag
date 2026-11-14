#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require

struct Material{
    vec4 baseColorFactor;
    uint baseColorTextureIndex;
    uint baseColorTextureSampler;
    uint metallicRoughnessTextureIndex;
    uint metallicRoughnessTextureSample;
    float metallicFactor;
    float roughnessFactor;
};

layout(buffer_reference, scalar) readonly buffer MaterialBuffer { Material materials[]; };

layout ( push_constant ) uniform constants
{
    mat4 model;
    uint64_t sceneDataBufferAddr;
    uint materialIndex;
} pc;

layout(buffer_reference, scalar) readonly buffer SceneData { 
    mat4 view;
    mat4 proj;
    uint64_t vertBuffAddress;
    uint64_t matBuffAddress;
};

layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 vertColor;

layout(location = 0) out vec4 fragColor;

void main() {
    SceneData sData = SceneData(pc.sceneDataBufferAddr);

    MaterialBuffer mbuff = MaterialBuffer(sData.matBuffAddress);

    Material mat = mbuff.materials[pc.materialIndex];

    vec4 baseColor = texture(
        sampler2D(
            textures[nonuniformEXT(mat.baseColorTextureIndex)],
            samplers[nonuniformEXT(mat.baseColorTextureSampler)])
        , uv);

    fragColor = vertColor * baseColor * mat.baseColorFactor; 


    //fragColor = vec4(normal + 0.5, 1.0f); 
}
