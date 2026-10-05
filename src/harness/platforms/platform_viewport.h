#ifndef HARNESS_PLATFORM_VIEWPORT_H
#define HARNESS_PLATFORM_VIEWPORT_H

typedef struct tHarness_viewport {
    int x, y;
    int width, height;
    float scale_x, scale_y;
} tHarness_viewport;

void Harness_CalculateViewport(int window_width, int window_height, int frame_width, int frame_height, tHarness_viewport* viewport);

#endif
