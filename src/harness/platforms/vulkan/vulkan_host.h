#ifndef HARNESS_VULKAN_HOST_H
#define HARNESS_VULKAN_HOST_H

#include <stdint.h>
#include <vulkan/vulkan.h>

typedef int (*Vulkan_HostLoadLibraryFn)(const char* path);
typedef void (*Vulkan_HostUnloadLibraryFn)(void);
typedef void* (*Vulkan_HostGetVkGetInstanceProcAddrFn)(void);
typedef const char* (*Vulkan_HostGetErrorFn)(void);
typedef int (*Vulkan_HostGetInstanceExtensionsFn)(void* window, uint32_t* count, const char** names, uint32_t max_names);
typedef int (*Vulkan_HostCreateSurfaceFn)(void* window, VkInstance instance, VkSurfaceKHR* surface);

typedef struct tVulkan_host {
    Vulkan_HostLoadLibraryFn load_library;
    Vulkan_HostUnloadLibraryFn unload_library;
    Vulkan_HostGetVkGetInstanceProcAddrFn get_vk_get_instance_proc_addr;
    Vulkan_HostGetErrorFn get_error;
    Vulkan_HostGetInstanceExtensionsFn get_instance_extensions;
    Vulkan_HostCreateSurfaceFn create_surface;
    void* window;
} tVulkan_host;

#endif
