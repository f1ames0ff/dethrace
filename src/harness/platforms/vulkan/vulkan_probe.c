#include "vulkan_probe.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

tVulkan_probe_result gVulkan_probe;

static void set_unavailable(const char* fmt, ...) {
    va_list args;

    gVulkan_probe.available = 0;
    gVulkan_probe.api_version = 0;
    gVulkan_probe.device_name[0] = '\0';
    va_start(args, fmt);
    vsnprintf(gVulkan_probe.reason, sizeof(gVulkan_probe.reason), fmt, args);
    va_end(args);
}

void Vulkan_Probe_SetSkipped(const char* reason) {
    memset(&gVulkan_probe, 0, sizeof(gVulkan_probe));
    set_unavailable("%s", reason);
}

int Vulkan_Probe(Vulkan_HostLoadLibraryFn load_library,
    Vulkan_HostUnloadLibraryFn unload_library,
    Vulkan_HostGetVkGetInstanceProcAddrFn get_vk_get_instance_proc_addr,
    Vulkan_HostGetErrorFn get_error) {

    PFN_vkGetInstanceProcAddr get_instance_proc_addr;
    PFN_vkCreateInstance create_instance;
    PFN_vkDestroyInstance destroy_instance;
    PFN_vkEnumerateInstanceVersion enumerate_instance_version;
    PFN_vkEnumerateInstanceExtensionProperties enumerate_instance_extensions;
    PFN_vkEnumeratePhysicalDevices enumerate_physical_devices;
    PFN_vkGetPhysicalDeviceProperties get_physical_device_properties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queue_family_properties;
    PFN_vkEnumerateDeviceExtensionProperties enumerate_device_extensions;

    static const char* const portability_extension[] = { VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME };
    const char* error;

    VkApplicationInfo app_info;
    VkInstanceCreateInfo create_info;
    VkInstance instance;
    VkPhysicalDevice* devices;
    uint32_t device_count;
    uint32_t i;
    VkResult res;
    int result;

    memset(&gVulkan_probe, 0, sizeof(gVulkan_probe));
    gVulkan_probe.api_version = VK_API_VERSION_1_0;

    if (load_library(NULL) != 0) {
        error = get_error != NULL ? get_error() : NULL;
        set_unavailable("SDL could not load a Vulkan loader or driver%s%s",
            (error != NULL && error[0] != '\0') ? ": " : "",
            (error != NULL && error[0] != '\0') ? error : "");
        return 1;
    }

    get_instance_proc_addr = (PFN_vkGetInstanceProcAddr)get_vk_get_instance_proc_addr();
    if (get_instance_proc_addr == NULL) {
        set_unavailable("SDL did not provide vkGetInstanceProcAddr");
        unload_library();
        return 1;
    }

    enumerate_instance_version = (PFN_vkEnumerateInstanceVersion)get_instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    if (enumerate_instance_version != NULL) {
        uint32_t version = 0;
        if (enumerate_instance_version(&version) == VK_SUCCESS) {
            gVulkan_probe.api_version = version;
        }
    }

    create_instance = (PFN_vkCreateInstance)get_instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance");
    if (create_instance == NULL) {
        set_unavailable("Vulkan loader does not expose vkCreateInstance");
        unload_library();
        return 1;
    }

    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "dethrace";
    app_info.pEngineName = "dethrace";
    app_info.apiVersion = VK_API_VERSION_1_0;

    enumerate_instance_extensions = (PFN_vkEnumerateInstanceExtensionProperties)get_instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;

    if (enumerate_instance_extensions != NULL) {
        uint32_t extension_count = 0;
        if (enumerate_instance_extensions(NULL, &extension_count, NULL) == VK_SUCCESS && extension_count > 0) {
            VkExtensionProperties* extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
            if (extensions != NULL) {
                if (enumerate_instance_extensions(NULL, &extension_count, extensions) == VK_SUCCESS) {
                    uint32_t j;
                    for (j = 0; j < extension_count; j++) {
                        if (strcmp(extensions[j].extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                            create_info.enabledExtensionCount = 1;
                            create_info.ppEnabledExtensionNames = portability_extension;
                            break;
                        }
                    }
                }
                free(extensions);
            }
        }
    }

    instance = VK_NULL_HANDLE;
    res = create_instance(&create_info, NULL, &instance);
    if (res != VK_SUCCESS) {
        set_unavailable("vkCreateInstance failed (VkResult %d)", (int)res);
        unload_library();
        return 1;
    }

    enumerate_physical_devices = (PFN_vkEnumeratePhysicalDevices)get_instance_proc_addr(instance, "vkEnumeratePhysicalDevices");
    get_physical_device_properties = (PFN_vkGetPhysicalDeviceProperties)get_instance_proc_addr(instance, "vkGetPhysicalDeviceProperties");
    get_queue_family_properties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)get_instance_proc_addr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    enumerate_device_extensions = (PFN_vkEnumerateDeviceExtensionProperties)get_instance_proc_addr(instance, "vkEnumerateDeviceExtensionProperties");
    destroy_instance = (PFN_vkDestroyInstance)get_instance_proc_addr(instance, "vkDestroyInstance");

    if (enumerate_physical_devices == NULL || get_physical_device_properties == NULL || get_queue_family_properties == NULL || enumerate_device_extensions == NULL || destroy_instance == NULL) {
        set_unavailable("Vulkan loader does not expose the required entry points");
        if (destroy_instance != NULL) {
            destroy_instance(instance, NULL);
        }
        unload_library();
        return 1;
    }

    devices = NULL;
    result = 1;

    res = enumerate_physical_devices(instance, &device_count, NULL);
    if (res != VK_SUCCESS || device_count == 0) {
        set_unavailable("no Vulkan physical devices found");
        goto cleanup;
    }

    devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    if (devices == NULL) {
        set_unavailable("out of memory while enumerating Vulkan devices");
        goto cleanup;
    }

    res = enumerate_physical_devices(instance, &device_count, devices);
    if (res != VK_SUCCESS) {
        set_unavailable("vkEnumeratePhysicalDevices failed (VkResult %d)", (int)res);
        goto cleanup;
    }

    for (i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties properties;
        VkQueueFamilyProperties* queue_families;
        VkExtensionProperties* extensions;
        uint32_t queue_family_count = 0;
        uint32_t extension_count = 0;
        uint32_t j;
        int has_graphics_queue = 0;
        int has_swapchain = 0;

        get_physical_device_properties(devices[i], &properties);
        get_queue_family_properties(devices[i], &queue_family_count, NULL);
        queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        if (queue_families == NULL) {
            continue;
        }
        get_queue_family_properties(devices[i], &queue_family_count, queue_families);
        for (j = 0; j < queue_family_count; j++) {
            if ((queue_families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                has_graphics_queue = 1;
                break;
            }
        }
        free(queue_families);
        if (!has_graphics_queue) {
            continue;
        }

        if (enumerate_device_extensions(devices[i], NULL, &extension_count, NULL) != VK_SUCCESS || extension_count == 0) {
            continue;
        }
        extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
        if (extensions == NULL) {
            continue;
        }
        if (enumerate_device_extensions(devices[i], NULL, &extension_count, extensions) == VK_SUCCESS) {
            for (j = 0; j < extension_count; j++) {
                if (strcmp(extensions[j].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                    has_swapchain = 1;
                    break;
                }
            }
        }
        free(extensions);
        if (!has_swapchain) {
            continue;
        }

        gVulkan_probe.available = 1;
        gVulkan_probe.api_version = properties.apiVersion;
        snprintf(gVulkan_probe.device_name, sizeof(gVulkan_probe.device_name), "%s", properties.deviceName);
        snprintf(gVulkan_probe.reason, sizeof(gVulkan_probe.reason), "Vulkan device available");
        result = 0;
        break;
    }

    if (!gVulkan_probe.available) {
        set_unavailable("no Vulkan device with a graphics queue and swapchain support");
    }

cleanup:
    free(devices);
    destroy_instance(instance, NULL);
    unload_library();
    return result;
}
