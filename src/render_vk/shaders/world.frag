#version 450

layout(set = 1, binding = 0) uniform sampler2D tex;

layout(push_constant) uniform WorldPushConstants {
    mat4 model;
    vec4 tint;
    vec4 fog;
    uint flags;
} push;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in float v_depth;

layout(location = 0) out vec4 o_color;

void main() {
    vec4 color = texture(tex, v_uv);
    if ((push.flags & 1u) != 0u && dot(color.rgb, vec3(0.299, 0.587, 0.114)) < 0.0039) {
        discard;
    }
    color *= push.tint;
    if (color.a < 0.01) {
        discard;
    }
    if ((push.flags & 8u) == 0u) {
        float ndl = max(dot(normalize(v_normal), normalize(vec3(0.35, 0.8, 0.2))), 0.0);
        color.rgb *= 0.55 + 0.45 * ndl;
    }
    if (push.fog.w > 0.0 && push.fog.y > push.fog.x) {
        float fog_amount = clamp((v_depth - push.fog.x) / (push.fog.y - push.fog.x), 0.0, 1.0);
        color.rgb = mix(color.rgb, vec3(0.02), fog_amount * push.fog.w);
    }
    o_color = color;
}
