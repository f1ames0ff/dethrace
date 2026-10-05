#version 450

layout(set = 0, binding = 0) uniform sampler2D uIndex;
layout(set = 0, binding = 1) uniform sampler2D uPalette;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 oColor;

void main() {
    float index = texture(uIndex, vUV).r;
    int i = int(index * 255.0 + 0.5);
    oColor = texelFetch(uPalette, ivec2(i, 0), 0);
}
