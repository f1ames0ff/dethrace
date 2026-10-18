#version 450

layout(set = 0, binding = 0) readonly buffer OverlayPixels {
    uint pixels[];
} overlay;

layout(push_constant) uniform OverlayPushConstants {
    uvec2 size;
} push;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 o_color;

void main() {
    ivec2 coord = ivec2(v_uv * vec2(push.size));
    coord = clamp(coord, ivec2(0), ivec2(push.size) - ivec2(1));
    uint p = overlay.pixels[coord.y * int(push.size.x) + coord.x];
    vec4 color = vec4(
        float(p & 255u),
        float((p >> 8) & 255u),
        float((p >> 16) & 255u),
        float((p >> 24) & 255u)) / 255.0;
    if (color.a < 0.5) {
        discard;
    }
    o_color = color;
}
