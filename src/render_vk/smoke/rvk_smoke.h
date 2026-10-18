#ifndef RVK_SMOKE_H
#define RVK_SMOKE_H

#include <stdint.h>

#include "rvk.h"

typedef struct RvkSmokeHost {
    void *user;
    int (*init)(void *user, uint32_t width, uint32_t height, const char *title);
    void (*shutdown)(void *user);
    int (*poll)(void *user, int *quit, int *resize, uint32_t *width, uint32_t *height);
    void (*sleep_ms)(void *user, uint32_t milliseconds);
    void (*resize)(void *user, uint32_t width, uint32_t height);
    RvkPlatform (*platform)(void *user);
} RvkSmokeHost;

int rvk_smoke_run(int argc, char **argv, const RvkSmokeHost *host);

#endif
