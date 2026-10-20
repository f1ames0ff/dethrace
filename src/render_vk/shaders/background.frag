#version 450

layout(set = 0, binding = 0) uniform sampler2D tex;

layout(push_constant) uniform BackgroundPush {
    float src_x;
    float top_y;
    float texture_width;
    float texture_height;
    float scale_y;
} background;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

void main() {
    int width = max(int(background.texture_width), 1);
    int height = max(int(background.texture_height), 1);
    int screen_y = int(floor(gl_FragCoord.y));
    int top = int(floor(background.top_y));
    int scale = max(int(background.scale_y + 0.5), 1);
    int u;
    int v;

    if (screen_y < top) {
        u = 0;
        v = 0;
    } else {
        int scaled_y = (screen_y - top) / scale;
        if (scaled_y >= height) {
            u = 0;
            v = height - 1;
        } else {
            u = int(mod(floor(gl_FragCoord.x + background.src_x), float(width)));
            v = scaled_y;
        }
    }
    o_color = vec4(texelFetch(tex, ivec2(clamp(u, 0, width - 1), clamp(v, 0, height - 1)), 0).rgb, 1.0);
}
