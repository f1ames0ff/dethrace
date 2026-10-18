#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(set = 0, binding = 0) uniform WorldUniforms {
    mat4 view_proj;
} world;

layout(push_constant) uniform WorldPushConstants {
    mat4 model;
    vec4 tint;
} push;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;

void main() {
    vec4 clip = world.view_proj * push.model * vec4(position, 1.0);
    clip.z = (clip.w - clip.z) * 0.5;
    clip.y = -clip.y;
    gl_Position = clip;
    v_normal = mat3(push.model) * normal;
    v_uv = uv;
}
