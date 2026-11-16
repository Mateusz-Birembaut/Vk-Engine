#version 460 core
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer { Vertex vertices[]; };

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

layout (location = 0) out vec2 vUv;
layout (location = 1) out vec3 vN;
layout (location = 2) out vec4 vColor;
layout (location = 3) out vec3 vWPos;

void main() {
    SceneData sData = SceneData(pc.sceneDataBufferAddr);
    VertexBuffer vbuff = VertexBuffer(sData.vertBuffAddress);

    Vertex vert = vbuff.vertices[gl_VertexIndex];

    vec4 finalPosition = sData.proj * sData.view * pc.model * vec4(vert.position, 1.0);
    gl_Position = finalPosition;

    mat4 normalMatrix = transpose(inverse(pc.model));

    vUv = vec2(vert.uv_x, vert.uv_y);
    // for now compute normal matrix  in shader, TODO : if support of non uniform scales is maintained, move this to CPU on a per draw basis
    vN = normalize((normalMatrix * vec4(vert.normal, 0.0)).xyz); 
    vColor = vert.color;
    vWPos = vec3(pc.model * vec4(vert.position, 1.0));
}   