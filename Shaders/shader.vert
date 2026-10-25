#version 460 core

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 uv;

vec2 positions[3] = vec2[](
    vec2(-0.5, -0.5),
    vec2(0.5, -0.5),
    vec2(0.0, +0.5)
);

vec2 uvs[3] = vec2[](
    vec2(0, 0),
    vec2(0, 1),
    vec2(1, 1)
);


void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
    uv = uvs[gl_VertexIndex];
}   