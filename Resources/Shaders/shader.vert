#version 460 core
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct Vertex {
    vec4 color;
    vec4 tangent;
    vec2 texCoord0;
    vec2 texCoord1;
    vec3 position;
    vec3 normal;

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

layout (location = 1) out vec4 vColor;
layout (location = 2) out vec2 vUv0;
layout (location = 3) out vec2 vUv1;
layout (location = 4) out vec3 vWPos;
layout (location = 5) out vec3 vN;           // normal in world space
layout (location = 6) out vec3 vTangent;     // tangent in world space
layout (location = 7) out float vTangentDir; // sign D of tangent space

void main() {
    SceneData sData = SceneData(pc.sceneDataBufferAddr);
    VertexBuffer vbuff = VertexBuffer(sData.vertBuffAddress);

    Vertex vert = vbuff.vertices[gl_VertexIndex];

    vec4 finalPosition = sData.proj * sData.view * pc.model * vec4(vert.position, 1.0);
    gl_Position = finalPosition;

    mat4 normalMatrix = transpose(inverse(pc.model));

    vUv0 = vert.texCoord0;
    vUv1 = vert.texCoord1;
    vN = normalize((normalMatrix * vec4(vert.normal, 0.0)).xyz);  // for now compute normal matrix  in shader, TODO : if support of non uniform scales is maintained, move this to CPU on a per draw basis
    vColor = vert.color;
    vWPos = vec3(pc.model * vec4(vert.position, 1.0));
    vTangent = normalize((normalMatrix * vec4(vert.tangent.xyz, 0.0)).xyz);
    vTangentDir = vert.tangent.w;
}   