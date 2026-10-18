#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "rvk_internal.h"

#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

void rvk_log(RvkDevice *device, int level, const char *format, ...) {
    char message[1024];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (device != NULL && device->info.log != NULL) {
        device->info.log(device->info.log_user, level, message);
    } else {
        fprintf(stderr, "render_vk: %s\n", message);
    }
}

uint64_t rvk_now_ticks(void) {
#ifdef _WIN32
    LARGE_INTEGER counter;

    QueryPerformanceCounter(&counter);
    return (uint64_t)counter.QuadPart;
#else
    struct timespec value;

    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        return 0;
    }
    return (uint64_t)value.tv_sec * 1000000000ULL + (uint64_t)value.tv_nsec;
#endif
}

double rvk_timer_frequency(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;

    QueryPerformanceFrequency(&frequency);
    return (double)frequency.QuadPart;
#else
    return 1000000000.0;
#endif
}

double rvk_ticks_to_ms(const RvkDevice *device, uint64_t ticks) {
    double frequency = device != NULL ? device->timer_frequency : 0.0;

    if (frequency <= 0.0) {
        frequency = rvk_timer_frequency();
    }
    if (frequency <= 0.0) {
        return 0.0;
    }
    return ((double)ticks * 1000.0) / frequency;
}

const char *rvk_result_string(RvkResult result) {
    switch (result) {
    case RVK_OK:
        return "ok";
    case RVK_ERROR_ARGUMENT:
        return "invalid argument";
    case RVK_ERROR_VULKAN:
        return "Vulkan error";
    case RVK_ERROR_NO_DEVICE:
        return "no suitable Vulkan device";
    case RVK_ERROR_UNSUPPORTED:
        return "unsupported configuration";
    case RVK_ERROR_OUT_OF_DATE:
        return "swapchain out of date";
    case RVK_ERROR_DEVICE_LOST:
        return "device lost";
    default:
        return "unknown error";
    }
}

const RvkStats *rvk_get_stats(const RvkDevice *device) {
    if (device == NULL) {
        return NULL;
    }
    return &device->stats;
}

const char *rvk_get_adapter_name(const RvkDevice *device) {
    if (device == NULL) {
        return "";
    }
    return device->adapter_name;
}

void rvk_wait_idle(RvkDevice *device) {
    if (device == NULL || device->device == VK_NULL_HANDLE || device->vk.DeviceWaitIdle == NULL) {
        return;
    }
    device->vk.DeviceWaitIdle(device->device);
}
