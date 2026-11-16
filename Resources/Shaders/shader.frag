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
    uint normalTextureIndex;
    uint normalTextureSampler;
    uint occlusionTextureIndex;
    uint occlusionTextureSampler;
    uint emissiveTextureIndex;
    uint emissiveTextureSampler;
    float metallicFactor;
    float roughnessFactor;
    vec3 emissiveFactor;
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

layout (location = 1) in vec4 vColor;
layout (location = 2) in vec2 vUv0;
layout (location = 3) in vec2 vUv1;
layout (location = 4) in vec3 vWPos;
layout (location = 5) in vec3 vN;

layout(location = 0) out vec4 fragColor;

float DistributionGGX(vec3 N, vec3 H, float roughness){

    float a = roughness * roughness; 
    float a2 =  a * a; 
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH) * (a2 - 1) + 1;
    denom = max(PI * ( denom * denom), 0.0001);  // prevent division by 0

    return a2 / denom;
};

float GeometrySchlickGGX(vec3 N, vec3 V, float k){
    
    float NdotV = max(dot(N,V), 0.0);
    float denom = max(NdotV * (1-k) + k, 0.0001); // prevent division by 0

    return NdotV / denom;
};

float GeometrySmith(vec3 N,vec3 V, vec3 L, float roughness){

    float k = ((roughness + 1) * (roughness + 1)) / 8;

    return GeometrySchlickGGX(N, V, k) * GeometrySchlickGGX(N, L, k);
};

vec3 fresnelSchlick(vec3 H, vec3 V, vec3 F0){

    return F0 + (1 - F0) * pow(1 - (max(dot(H, V), 0.0)), 5);

};

vec3 reinhardToneMapping(vec3 v){
    return v / (1.0f + v);
}

void main() {
    SceneData sData = SceneData(pc.sceneDataBufferAddr);
    MaterialBuffer mbuff = MaterialBuffer(sData.matBuffAddress);

    Material mat = mbuff.materials[pc.materialIndex];

    vec4 baseColor = vec4(1.0f);
    
    if(mat.baseColorTextureIndex != ~0){

        baseColor =
            texture(sampler2D(
                textures[nonuniformEXT(mat.baseColorTextureIndex)],
                samplers[nonuniformEXT(mat.baseColorTextureSampler)])
            , vUv0);
    }

    float metalness = mat.metallicFactor;
    float roughness = mat.roughnessFactor;

    if(mat.metallicRoughnessTextureIndex != ~0){

        vec4 metallicRoughness = 
            texture(sampler2D(
                textures[nonuniformEXT(mat.metallicRoughnessTextureIndex)],
                samplers[nonuniformEXT(mat.metallicRoughnessTextureSampler)])
            , vUv0);

        metalness *= metallicRoughness.b;
        roughness *= metallicRoughness.g;
    }

    vec4 surfaceColor = vColor * baseColor * mat.baseColorFactor;  

    float ambientFactor = 0.03; // later use a cubemap for now is ok 
    vec3 ambient = surfaceColor.xyz * ambientFactor;

    vec3 N = normalize(vN); 
    vec3 V = normalize(sData.cameraPos - vWPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, surfaceColor.rgb, metalness);

    // data for each light 

    vec3 L = normalize(-sData.sunLightDir); // in this case sun light direction
    vec3 H = normalize(L + V);

    float distance = length(1.0); // no attenuation for sun light so setting 1
    float attenuation = 1.0 / (distance * distance); 

    vec3 radiance = sData.sunLightColor * sData.sunIntensity * attenuation;

    // compute cook torrance BRDF
    // specular part
    float D = DistributionGGX(N, H, roughness);
    vec3 F = fresnelSchlick(H, V, F0);
    float G = GeometrySmith(N, V, L, roughness);

    vec3 num = D * F * G;
    float denom = 4.0 * max(dot(V, N), 0.0) * max(dot(L, N), 0.0) + 0.0001; // prevent division by 0 

    vec3 specular = num / denom;
    vec3 kS = F;

    //diffuse part
    vec3 diffuse = surfaceColor.xyz / PI; 
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metalness; // if material is fully metallic, no diffuse color	

    vec3 brdf = kD * diffuse + specular;

    // refletance
    float cosTheta = max(dot(N, L), 0.0);
    vec3 Lo = brdf * radiance * cosTheta;

    fragColor = vec4(reinhardToneMapping(Lo + ambient), surfaceColor.a);

    //fragColor = vec4(normal * 0.5 + 0.5, 1.0f); 
}
