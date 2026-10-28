#version 460 core
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

layout (location = 0) out vec2 uv;

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
    uint64_t vertBuffAddr;
    uint samplerId;
    uint textureId;
} pc;


void main() {
    VertexBuffer vbuff = VertexBuffer(pc.vertBuffAddr);

    Vertex vert = vbuff.vertices[gl_VertexIndex];

    gl_Position = vec4(vert.position, 1.0);

    uv = vec2(vert.uv_x, vert.uv_y);
}   