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

float NDF_GGXTR(vec3 N, vec3 H, float roughness){
    float a = roughness * roughness; 
    float a2 = a * a; 
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH) * (a2 - 1) + 1;

    return a2 / (PI * ( denom * denom));
};

float GF_GGXSB(vec3 N, vec3 V, float k){
    
    float NdotV = max(dot(N,V), 0.0);

    return NdotV / (NdotV * (1-k) + k);
};

float G_S(vec3 N,vec3 V, vec3 L, float roughness){
    float k = ((roughness + 1) * (roughness + 1)) / 8;

    return GF_GGXSB(N, V, k) * GF_GGXSB(N, L, k);
};

vec3 F_S(vec3 H, vec3 V, vec3 F0){
    return F0 + (1 - F0) * pow(1 - (max(dot(H, V), 0.0)), 5);
};

void main() {
    SceneData sData = SceneData(pc.sceneDataBufferAddr);
    MaterialBuffer mbuff = MaterialBuffer(sData.matBuffAddress);

    Material mat = mbuff.materials[pc.materialIndex];

    vec4 baseColor = texture(
        sampler2D(
            textures[nonuniformEXT(mat.baseColorTextureIndex)],
            samplers[nonuniformEXT(mat.baseColorTextureSampler)])
        , uv);

  
    vec4 metallicRoughness = texture(
        sampler2D(
            textures[nonuniformEXT(mat.metallicRoughnessTextureIndex)],
            samplers[nonuniformEXT(mat.metallicRoughnessTextureSampler)])
        , uv);

    float metalness = metallicRoughness.b * mat.metallicFactor;
    float roughness = metallicRoughness.g * mat.roughnessFactor;

    vec4 surfaceColor = vColor * baseColor * mat.baseColorFactor;  

    float ambientFactor = 0.03; // later use a cubemap for now is ok 
    vec3 ambient = surfaceColor.xyz * ambientFactor;

    vec3 N = normalize(N); 
    vec3 V = normalize(sData.cameraPos - wPos);

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
    float D = NDF_GGXTR(N, H, roughness);
    vec3 F = F_S(H, V, F0);
    float G = G_S(N, V, L, roughness);

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

    fragColor = vec4(Lo + ambient, surfaceColor.a);

    //fragColor = vec4(normal * 0.5 + 0.5, 1.0f); 
}
