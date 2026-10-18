#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(push_constant) uniform WorldPushConstants {
    mat4 model;
    vec4 tint;
} push;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 o_color;

void main() {
    vec4 color = texture(tex, v_uv) * push.tint;
    float ndl = max(dot(normalize(v_normal), normalize(vec3(0.35, 0.8, 0.2))), 0.0);
    color.rgb *= 0.55 + 0.45 * ndl;
    o_color = color;
}
