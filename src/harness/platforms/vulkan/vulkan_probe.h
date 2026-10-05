#ifndef HARNESS_VULKAN_PROBE_H
#define HARNESS_VULKAN_PROBE_H

#include "platforms/vulkan/vulkan_host.h"
#include <stdint.h>
#include <vulkan/vulkan.h>

typedef struct tVulkan_probe_result {
    int available;
    uint32_t api_version;
    char device_name[256];
    char reason[256];
} tVulkan_probe_result;

extern tVulkan_probe_result gVulkan_probe;

void Vulkan_Probe_SetSkipped(const char* reason);

int Vulkan_Probe(Vulkan_HostLoadLibraryFn load_library,
    Vulkan_HostUnloadLibraryFn unload_library,
    Vulkan_HostGetVkGetInstanceProcAddrFn get_vk_get_instance_proc_addr,
    Vulkan_HostGetErrorFn get_error);

#endif
