#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require

#define PI 3.14159265359

struct Material{
    vec4 baseColorFactor;
    uint baseColorTextureIndex;
    uint baseColorTextureSampler;
    uint metallicRoughnessTextureIndex;
    uint metallicRoughnessTextureSampler;
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
    vec3 cameraPos;
    vec3 sunLightDir;
    vec3 sunLightColor;
    float sunIntensity;
    uint64_t vertBuffAddress;
    uint64_t matBuffAddress;
};

layout(set = 0, binding = 0) uniform sampler samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 N;
layout (location = 2) in vec4 vColor;
layout (location = 3) in vec3 wPos;

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


    vec4 surfaceColor = vColor * baseColor * mat.baseColorFactor;  
   
    float kS = 0.0; // for now no specular until => TODO : need to fix format of metallicRoughnessTexture
    float kD = 1.0 - kS;

    vec3 f_lambert = surfaceColor.xyz / PI;
    vec3 f_cookTerrance = vec3(0.0); // when metallicRoughnessTexture is ok change that 

    vec3 cookTerrance_brdf = kD * f_lambert + kS * f_cookTerrance; 

    // radiance for directionnal light like the sun
    vec3 wi = -sData.sunLightDir;
    float attenuation = 1.0f; // no attenuation for sun
    vec3 radiance = sData.sunLightColor * sData.sunIntensity * attenuation;

    float cosTheta = max(dot(N, wi), 0.0);

    vec3 Lo = cookTerrance_brdf * radiance * cosTheta;

    float ambientFactor = 0.13; // later use a cubemap for now is ok 
    vec3 ambient = surfaceColor.xyz * ambientFactor;

    fragColor = vec4(Lo + ambient, surfaceColor.a);

/*     
    vec4 metallicRoughness = texture(
        sampler2D(
            textures[nonuniformEXT(mat.metallicRoughnessTextureIndex)],
            samplers[nonuniformEXT(mat.metallicRoughnessTextureSampler)])
        , uv);

    float metalness = metallicRoughness.b;
    float roughness = metallicRoughness.g;

    fragColor = vec4(0.0f, metalness * mat.metallicFactor, roughness * mat.roughnessFactor, 1.0);
 */
    //fragColor = vec4(normal * 0.5 + 0.5, 1.0f); 
}
