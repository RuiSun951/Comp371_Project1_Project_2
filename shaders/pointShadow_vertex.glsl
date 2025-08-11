#version 330 core
layout(location=0) in vec3 aPos;

uniform mat4 model;
uniform mat4 vp;      // per-cube-face (proj * view)

out vec3 WorldPos;

void main() {
    vec4 wp = model * vec4(aPos, 1.0);
    WorldPos = wp.xyz;
    gl_Position = vp * wp;
}
