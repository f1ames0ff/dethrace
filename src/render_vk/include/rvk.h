#ifndef RVK_H
#define RVK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RvkDevice RvkDevice;

typedef enum RvkResult {
    RVK_OK = 0,
    RVK_ERROR_ARGUMENT,
    RVK_ERROR_VULKAN,
    RVK_ERROR_NO_DEVICE,
    RVK_ERROR_UNSUPPORTED,
    RVK_ERROR_OUT_OF_DATE,
    RVK_ERROR_DEVICE_LOST
} RvkResult;

typedef enum RvkPresentMode {
    RVK_PRESENT_FIFO = 0,
    RVK_PRESENT_IMMEDIATE = 1
} RvkPresentMode;

typedef enum RvkPass {
    RVK_PASS_MAIN = 0
} RvkPass;

typedef void (*RvkLogFn)(void *user, int level, const char *message);
typedef void (*RvkProc)(void);
typedef RvkProc (*RvkGetInstanceProcAddrFn)(void);

typedef struct RvkPlatform {
    void *user;
    int (*load_vulkan)(void *user);
    void (*unload_vulkan)(void *user);
    RvkGetInstanceProcAddrFn (*get_instance_proc_addr)(void *user);
    int (*get_instance_extensions)(void *user, uint32_t *count, const char **names, uint32_t max_names);
    int (*create_surface)(void *user, uint64_t instance, uint64_t *surface);
    const char *(*get_error)(void *user);
} RvkPlatform;

typedef struct RvkInitInfo {
    uint32_t struct_size;
    const char *app_name;
    uint32_t width;
    uint32_t height;
    RvkPlatform platform;
    int enable_validation;
    RvkPresentMode present_mode;
    RvkLogFn log;
    void *log_user;
} RvkInitInfo;

typedef struct RvkPassInfo {
    uint32_t struct_size;
    RvkPass pass;
    int clear;
    float clear_color[4];
} RvkPassInfo;

typedef struct RvkStats {
    uint64_t frame_index;
    float cpu_frame_ms;
    float cpu_wait_ms;
    uint32_t width;
    uint32_t height;
    uint32_t swapchain_images;
    uint32_t present_mode;
    uint32_t pass_count;
    uint32_t validation_messages;
} RvkStats;

RvkResult rvk_create(const RvkInitInfo *info, RvkDevice **out_device);
void rvk_destroy(RvkDevice *device);
RvkResult rvk_resize(RvkDevice *device, uint32_t width, uint32_t height);
RvkResult rvk_begin_frame(RvkDevice *device);
RvkResult rvk_begin_pass(RvkDevice *device, const RvkPassInfo *info);
void rvk_end_pass(RvkDevice *device);
RvkResult rvk_end_frame(RvkDevice *device);
void rvk_wait_idle(RvkDevice *device);
const RvkStats *rvk_get_stats(const RvkDevice *device);
const char *rvk_get_adapter_name(const RvkDevice *device);
const char *rvk_result_string(RvkResult result);

#ifdef __cplusplus
}
#endif

#endif
