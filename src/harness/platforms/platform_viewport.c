#include "platforms/platform_viewport.h"

void Harness_CalculateViewport(int window_width, int window_height, int frame_width, int frame_height, tHarness_viewport* viewport) {
    int vp_width, vp_height;
    float target_aspect_ratio;
    float aspect_ratio;

    viewport->x = 0;
    viewport->y = 0;
    viewport->width = 0;
    viewport->height = 0;
    viewport->scale_x = 1.0f;
    viewport->scale_y = 1.0f;

    if (window_width <= 0 || window_height <= 0 || frame_width <= 0 || frame_height <= 0) {
        return;
    }

    aspect_ratio = (float)window_width / (float)window_height;
    target_aspect_ratio = (float)frame_width / (float)frame_height;

    vp_width = window_width;
    vp_height = window_height;
    if (aspect_ratio != target_aspect_ratio) {
        if (aspect_ratio > target_aspect_ratio) {
            vp_width = (int)(window_height * target_aspect_ratio + .5f);
        } else {
            vp_height = (int)(window_width / target_aspect_ratio + .5f);
        }
    }
    viewport->x = (window_width - vp_width) / 2;
    viewport->y = (window_height - vp_height) / 2;
    viewport->width = vp_width;
    viewport->height = vp_height;
    viewport->scale_x = (float)vp_width / (float)frame_width;
    viewport->scale_y = (float)vp_height / (float)frame_height;
}
