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

layout(buffer_reference, scalar) readonly buffer SceneData { 
    mat4 view;
    mat4 proj;
    uint64_t vertBuffAddress;
};

layout(buffer_reference, scalar) readonly buffer VertexBuffer { Vertex vertices[]; };

layout ( push_constant ) uniform constants
{
    uint64_t sceneDataBufferAddr;
    uint samplerId;
    uint textureId;
    mat4 model;
} pc;

layout (location = 0) out vec2 uv;
layout (location = 1) out vec3 normal;

void main() {
    SceneData sData = SceneData(pc.sceneDataBufferAddr);
    VertexBuffer vbuff = VertexBuffer(sData.vertBuffAddress);

    Vertex vert = vbuff.vertices[gl_VertexIndex];

    vec4 finalPosition = sData.proj * sData.view * pc.model * vec4(vert.position, 1.0);
    gl_Position = finalPosition;

    normal = vert.normal;
    uv = vec2(vert.uv_x, vert.uv_y);
}   