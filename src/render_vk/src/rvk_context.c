#include "rvk_internal.h"

static VKAPI_ATTR VkBool32 VKAPI_CALL rvk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
    RvkDevice *device = (RvkDevice *)user_data;
    int level = severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ? 2 : 1;
    int reportable = (types & (VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)) != 0;

    if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        return VK_FALSE;
    }
    if (device != NULL && reportable && severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        device->stats.validation_messages++;
    }
    if (reportable) {
        rvk_log(device, level, "%s", callback_data != NULL && callback_data->pMessage != NULL ? callback_data->pMessage : "validation message");
    }
    return VK_FALSE;
}

static int rvk_has_instance_extension(RvkDevice *device, const char *name) {
    uint32_t count = 0;
    VkExtensionProperties *properties;
    uint32_t i;
    int found = 0;

    if (device->vk.EnumerateInstanceExtensionProperties == NULL) {
        return 0;
    }
    if (device->vk.EnumerateInstanceExtensionProperties(NULL, &count, NULL) != VK_SUCCESS || count == 0) {
        return 0;
    }
    properties = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * count);
    if (properties == NULL) {
        return 0;
    }
    if (device->vk.EnumerateInstanceExtensionProperties(NULL, &count, properties) == VK_SUCCESS) {
        for (i = 0; i < count; i++) {
            if (strcmp(properties[i].extensionName, name) == 0) {
                found = 1;
                break;
            }
        }
    }
    free(properties);
    return found;
}

static int rvk_has_instance_layer(RvkDevice *device, const char *name) {
    uint32_t count = 0;
    VkLayerProperties *properties;
    uint32_t i;
    int found = 0;

    if (device->vk.EnumerateInstanceLayerProperties == NULL) {
        return 0;
    }
    if (device->vk.EnumerateInstanceLayerProperties(&count, NULL) != VK_SUCCESS || count == 0) {
        return 0;
    }
    properties = (VkLayerProperties *)malloc(sizeof(VkLayerProperties) * count);
    if (properties == NULL) {
        return 0;
    }
    if (device->vk.EnumerateInstanceLayerProperties(&count, properties) == VK_SUCCESS) {
        for (i = 0; i < count; i++) {
            if (strcmp(properties[i].layerName, name) == 0) {
                found = 1;
                break;
            }
        }
    }
    free(properties);
    return found;
}

#define RVK_LOAD(name)                                                                            \
    do {                                                                                          \
        device->vk.name = (PFN_vk##name)device->get_instance_proc_addr(device->instance, "vk" #name); \
        if (device->vk.name == NULL) {                                                            \
            rvk_log(device, 2, "missing Vulkan entry point vk%s", #name);                         \
            return RVK_ERROR_VULKAN;                                                              \
        }                                                                                         \
    } while (0)

static RvkResult rvk_load_instance_functions(RvkDevice *device) {
    RVK_LOAD(EnumeratePhysicalDevices);
    RVK_LOAD(EnumerateDeviceExtensionProperties);
    RVK_LOAD(GetPhysicalDeviceProperties);
    RVK_LOAD(GetPhysicalDeviceFormatProperties);
    RVK_LOAD(GetPhysicalDeviceMemoryProperties);
    RVK_LOAD(GetPhysicalDeviceQueueFamilyProperties);
    RVK_LOAD(GetPhysicalDeviceSurfaceSupportKHR);
    RVK_LOAD(GetPhysicalDeviceSurfaceCapabilitiesKHR);
    RVK_LOAD(GetPhysicalDeviceSurfaceFormatsKHR);
    RVK_LOAD(GetPhysicalDeviceSurfacePresentModesKHR);
    RVK_LOAD(DestroySurfaceKHR);
    RVK_LOAD(CreateDevice);
    RVK_LOAD(DestroyDevice);
    RVK_LOAD(GetDeviceQueue);
    RVK_LOAD(DeviceWaitIdle);
    RVK_LOAD(CreateSwapchainKHR);
    RVK_LOAD(DestroySwapchainKHR);
    RVK_LOAD(GetSwapchainImagesKHR);
    RVK_LOAD(AcquireNextImageKHR);
    RVK_LOAD(QueuePresentKHR);
    RVK_LOAD(CreateRenderPass);
    RVK_LOAD(DestroyRenderPass);
    RVK_LOAD(CreateImageView);
    RVK_LOAD(DestroyImageView);
    RVK_LOAD(CreateFramebuffer);
    RVK_LOAD(DestroyFramebuffer);
    RVK_LOAD(CreateCommandPool);
    RVK_LOAD(DestroyCommandPool);
    RVK_LOAD(AllocateCommandBuffers);
    RVK_LOAD(BeginCommandBuffer);
    RVK_LOAD(EndCommandBuffer);
    RVK_LOAD(ResetCommandBuffer);
    RVK_LOAD(CmdBeginRenderPass);
    RVK_LOAD(CmdEndRenderPass);
    RVK_LOAD(CreateSemaphore);
    RVK_LOAD(DestroySemaphore);
    RVK_LOAD(CreateFence);
    RVK_LOAD(DestroyFence);
    RVK_LOAD(WaitForFences);
    RVK_LOAD(ResetFences);
    RVK_LOAD(QueueSubmit);
    RVK_LOAD(QueueWaitIdle);
    RVK_LOAD(CreateBuffer);
    RVK_LOAD(DestroyBuffer);
    RVK_LOAD(GetBufferMemoryRequirements);
    RVK_LOAD(BindBufferMemory);
    RVK_LOAD(CreateImage);
    RVK_LOAD(DestroyImage);
    RVK_LOAD(GetImageMemoryRequirements);
    RVK_LOAD(BindImageMemory);
    RVK_LOAD(AllocateMemory);
    RVK_LOAD(FreeMemory);
    RVK_LOAD(MapMemory);
    RVK_LOAD(UnmapMemory);
    RVK_LOAD(CreateSampler);
    RVK_LOAD(DestroySampler);
    RVK_LOAD(CreateDescriptorSetLayout);
    RVK_LOAD(DestroyDescriptorSetLayout);
    RVK_LOAD(CreateDescriptorPool);
    RVK_LOAD(DestroyDescriptorPool);
    RVK_LOAD(AllocateDescriptorSets);
    RVK_LOAD(FreeDescriptorSets);
    RVK_LOAD(UpdateDescriptorSets);
    RVK_LOAD(CreatePipelineLayout);
    RVK_LOAD(DestroyPipelineLayout);
    RVK_LOAD(CreateShaderModule);
    RVK_LOAD(DestroyShaderModule);
    RVK_LOAD(CreateGraphicsPipelines);
    RVK_LOAD(DestroyPipeline);
    RVK_LOAD(CmdBindPipeline);
    RVK_LOAD(CmdBindDescriptorSets);
    RVK_LOAD(CmdBindVertexBuffers);
    RVK_LOAD(CmdBindIndexBuffer);
    RVK_LOAD(CmdPushConstants);
    RVK_LOAD(CmdDraw);
    RVK_LOAD(CmdDrawIndexed);
    RVK_LOAD(CmdSetViewport);
    RVK_LOAD(CmdSetScissor);
    RVK_LOAD(CmdCopyBufferToImage);
    RVK_LOAD(CmdPipelineBarrier);

    device->vk.CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)device->get_instance_proc_addr(device->instance, "vkCreateDebugUtilsMessengerEXT");
    device->vk.DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)device->get_instance_proc_addr(device->instance, "vkDestroyDebugUtilsMessengerEXT");
    return RVK_OK;
}

static RvkResult rvk_create_instance(RvkDevice *device) {
    const char *extensions[RVK_MAX_INSTANCE_EXTENSIONS];
    uint32_t extension_count = 0;
    const char *layers[1];
    uint32_t layer_count = 0;
    VkApplicationInfo application_info;
    VkInstanceCreateInfo create_info;
    VkDebugUtilsMessengerCreateInfoEXT debug_info;
    VkResult result;

    device->get_instance_proc_addr = (PFN_vkGetInstanceProcAddr)device->info.platform.get_instance_proc_addr(device->info.platform.user);
    if (device->get_instance_proc_addr == NULL) {
        rvk_log(device, 2, "platform did not provide vkGetInstanceProcAddr");
        return RVK_ERROR_NO_DEVICE;
    }

    device->vk.EnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)device->get_instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    device->vk.EnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)device->get_instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    device->vk.EnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)device->get_instance_proc_addr(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties");
    device->vk.CreateInstance = (PFN_vkCreateInstance)device->get_instance_proc_addr(VK_NULL_HANDLE, "vkCreateInstance");
    if (device->vk.CreateInstance == NULL) {
        rvk_log(device, 2, "Vulkan loader did not provide vkCreateInstance");
        return RVK_ERROR_NO_DEVICE;
    }

    if (device->info.enable_validation) {
        if (!rvk_has_instance_layer(device, "VK_LAYER_KHRONOS_validation")) {
            rvk_log(device, 2, "VK_LAYER_KHRONOS_validation is not available");
            return RVK_ERROR_UNSUPPORTED;
        }
        if (!rvk_has_instance_extension(device, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
            rvk_log(device, 2, "VK_EXT_debug_utils is not available");
            return RVK_ERROR_UNSUPPORTED;
        }
        layers[layer_count++] = "VK_LAYER_KHRONOS_validation";
        device->validation_enabled = 1;
    }

    if (device->info.platform.get_instance_extensions(device->info.platform.user, &extension_count, extensions, RVK_MAX_INSTANCE_EXTENSIONS) != 0) {
        rvk_log(device, 2, "platform could not enumerate instance extensions");
        return RVK_ERROR_NO_DEVICE;
    }
    if (extension_count > RVK_MAX_INSTANCE_EXTENSIONS) {
        rvk_log(device, 2, "platform returned too many instance extensions");
        return RVK_ERROR_NO_DEVICE;
    }
    if (device->info.enable_validation) {
        uint32_t i;
        int present = 0;

        for (i = 0; i < extension_count; i++) {
            if (strcmp(extensions[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
                present = 1;
                break;
            }
        }
        if (!present) {
            if (extension_count >= RVK_MAX_INSTANCE_EXTENSIONS) {
                rvk_log(device, 2, "no room for VK_EXT_debug_utils");
                return RVK_ERROR_NO_DEVICE;
            }
            extensions[extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        }
    }

    memset(&application_info, 0, sizeof(application_info));
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = device->info.app_name != NULL ? device->info.app_name : "dethrace";
    application_info.applicationVersion = 1;
    application_info.pEngineName = "render_vk";
    application_info.engineVersion = 1;
    application_info.apiVersion = VK_API_VERSION_1_0;

    memset(&debug_info, 0, sizeof(debug_info));
    if (device->info.enable_validation) {
        debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        debug_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_info.pfnUserCallback = rvk_debug_callback;
        debug_info.pUserData = device;
    }

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = device->info.enable_validation ? &debug_info : NULL;
    create_info.pApplicationInfo = &application_info;
    create_info.enabledLayerCount = layer_count;
    create_info.ppEnabledLayerNames = layer_count > 0 ? layers : NULL;
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extension_count > 0 ? extensions : NULL;

    result = device->vk.CreateInstance(&create_info, NULL, &device->instance);
    if (result != VK_SUCCESS) {
        rvk_log(device, 2, "vkCreateInstance failed with %d", (int)result);
        return RVK_ERROR_NO_DEVICE;
    }

    device->vk.DestroyInstance = (PFN_vkDestroyInstance)device->get_instance_proc_addr(device->instance, "vkDestroyInstance");
    if (device->vk.DestroyInstance == NULL) {
        rvk_log(device, 2, "Vulkan loader did not provide vkDestroyInstance");
        return RVK_ERROR_NO_DEVICE;
    }

    if (rvk_load_instance_functions(device) != RVK_OK) {
        return RVK_ERROR_VULKAN;
    }

    if (device->info.enable_validation) {
        VkDebugUtilsMessengerCreateInfoEXT messenger_info;

        if (device->vk.CreateDebugUtilsMessengerEXT == NULL) {
            rvk_log(device, 2, "VK_EXT_debug_utils entry points are not available");
            return RVK_ERROR_UNSUPPORTED;
        }
        memset(&messenger_info, 0, sizeof(messenger_info));
        messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messenger_info.messageSeverity = debug_info.messageSeverity;
        messenger_info.messageType = debug_info.messageType;
        messenger_info.pfnUserCallback = rvk_debug_callback;
        messenger_info.pUserData = device;
        result = device->vk.CreateDebugUtilsMessengerEXT(device->instance, &messenger_info, NULL, &device->messenger);
        if (result != VK_SUCCESS) {
            rvk_log(device, 2, "vkCreateDebugUtilsMessengerEXT failed with %d", (int)result);
            return RVK_ERROR_VULKAN;
        }
        device->debug_utils_enabled = 1;
    }
    return RVK_OK;
}

static RvkResult rvk_create_surface(RvkDevice *device) {
    uint64_t surface = 0;

    if (device->info.platform.create_surface(device->info.platform.user, (uint64_t)device->instance, &surface) != 0 || surface == 0) {
        const char *error = device->info.platform.get_error != NULL ? device->info.platform.get_error(device->info.platform.user) : NULL;
        rvk_log(device, 2, "platform could not create a Vulkan surface%s%s", error != NULL ? ": " : "", error != NULL ? error : "");
        return RVK_ERROR_NO_DEVICE;
    }
    device->surface = (VkSurfaceKHR)surface;
    return RVK_OK;
}

static int rvk_device_has_swapchain(RvkDevice *device, VkPhysicalDevice physical_device) {
    uint32_t count = 0;
    VkExtensionProperties *properties;
    uint32_t i;
    int found = 0;

    if (device->vk.EnumerateDeviceExtensionProperties == NULL) {
        return 0;
    }
    if (device->vk.EnumerateDeviceExtensionProperties(physical_device, NULL, &count, NULL) != VK_SUCCESS || count == 0) {
        return 0;
    }
    properties = (VkExtensionProperties *)malloc(sizeof(VkExtensionProperties) * count);
    if (properties == NULL) {
        return 0;
    }
    if (device->vk.EnumerateDeviceExtensionProperties(physical_device, NULL, &count, properties) == VK_SUCCESS) {
        for (i = 0; i < count; i++) {
            if (strcmp(properties[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                found = 1;
                break;
            }
        }
    }
    free(properties);
    return found;
}

static int rvk_find_queue_families(RvkDevice *device, VkPhysicalDevice physical_device, uint32_t *graphics, uint32_t *present) {
    VkQueueFamilyProperties families[RVK_MAX_QUEUE_FAMILIES];
    uint32_t count = RVK_MAX_QUEUE_FAMILIES;
    uint32_t i;
    int graphics_index = -1;
    int present_index = -1;

    device->vk.GetPhysicalDeviceQueueFamilyProperties(physical_device, &count, families);
    if (count > RVK_MAX_QUEUE_FAMILIES) {
        count = RVK_MAX_QUEUE_FAMILIES;
    }
    for (i = 0; i < count; i++) {
        VkBool32 supports_present = VK_FALSE;

        device->vk.GetPhysicalDeviceSurfaceSupportKHR(physical_device, i, device->surface, &supports_present);
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && supports_present) {
            *graphics = i;
            *present = i;
            return 1;
        }
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && graphics_index < 0) {
            graphics_index = (int)i;
        }
        if (supports_present && present_index < 0) {
            present_index = (int)i;
        }
    }
    if (graphics_index < 0 || present_index < 0) {
        return 0;
    }
    *graphics = (uint32_t)graphics_index;
    *present = (uint32_t)present_index;
    return 1;
}

static RvkResult rvk_pick_physical_device(RvkDevice *device) {
    uint32_t count = 0;
    VkPhysicalDevice *devices;
    VkPhysicalDevice best = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties best_properties;
    uint32_t best_graphics = 0;
    uint32_t best_present = 0;
    uint32_t i;

    if (device->vk.EnumeratePhysicalDevices(device->instance, &count, NULL) != VK_SUCCESS || count == 0) {
        rvk_log(device, 2, "no Vulkan physical devices");
        return RVK_ERROR_NO_DEVICE;
    }
    devices = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * count);
    if (devices == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (device->vk.EnumeratePhysicalDevices(device->instance, &count, devices) != VK_SUCCESS) {
        free(devices);
        rvk_log(device, 2, "could not enumerate Vulkan physical devices");
        return RVK_ERROR_NO_DEVICE;
    }

    memset(&best_properties, 0, sizeof(best_properties));
    for (i = 0; i < count; i++) {
        VkPhysicalDeviceProperties properties;
        uint32_t graphics = 0;
        uint32_t present = 0;

        if (!rvk_device_has_swapchain(device, devices[i])) {
            continue;
        }
        if (!rvk_find_queue_families(device, devices[i], &graphics, &present)) {
            continue;
        }
        device->vk.GetPhysicalDeviceProperties(devices[i], &properties);
        if (best == VK_NULL_HANDLE || properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            best = devices[i];
            best_properties = properties;
            best_graphics = graphics;
            best_present = present;
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                break;
            }
        }
    }
    free(devices);

    if (best == VK_NULL_HANDLE) {
        rvk_log(device, 2, "no Vulkan device supports graphics, presentation and VK_KHR_swapchain");
        return RVK_ERROR_NO_DEVICE;
    }

    device->physical_device = best;
    device->graphics_family = best_graphics;
    device->present_family = best_present;
    snprintf(device->adapter_name, sizeof(device->adapter_name), "%s", best_properties.deviceName);
    rvk_log(device, 0, "Vulkan device: %s (type %d, API %u.%u.%u)", device->adapter_name, (int)best_properties.deviceType,
        VK_API_VERSION_MAJOR(best_properties.apiVersion), VK_API_VERSION_MINOR(best_properties.apiVersion), VK_API_VERSION_PATCH(best_properties.apiVersion));
    return RVK_OK;
}

static RvkResult rvk_create_device(RvkDevice *device) {
    VkDeviceQueueCreateInfo queue_infos[2];
    float priority = 1.0f;
    uint32_t queue_count = 0;
    const char *extensions[1];
    VkDeviceCreateInfo create_info;
    VkResult result;

    if (rvk_pick_physical_device(device) != RVK_OK) {
        return RVK_ERROR_NO_DEVICE;
    }

    memset(queue_infos, 0, sizeof(queue_infos));
    queue_infos[queue_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[queue_count].queueFamilyIndex = device->graphics_family;
    queue_infos[queue_count].queueCount = 1;
    queue_infos[queue_count].pQueuePriorities = &priority;
    queue_count++;
    if (device->present_family != device->graphics_family) {
        queue_infos[queue_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[queue_count].queueFamilyIndex = device->present_family;
        queue_infos[queue_count].queueCount = 1;
        queue_infos[queue_count].pQueuePriorities = &priority;
        queue_count++;
    }

    extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = queue_count;
    create_info.pQueueCreateInfos = queue_infos;
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = extensions;

    result = device->vk.CreateDevice(device->physical_device, &create_info, NULL, &device->device);
    if (result != VK_SUCCESS) {
        rvk_log(device, 2, "vkCreateDevice failed with %d", (int)result);
        return RVK_ERROR_NO_DEVICE;
    }
    device->vk.GetDeviceQueue(device->device, device->graphics_family, 0, &device->graphics_queue);
    device->vk.GetDeviceQueue(device->device, device->present_family, 0, &device->present_queue);
    device->vk.GetPhysicalDeviceMemoryProperties(device->physical_device, &device->memory_properties);
    return RVK_OK;
}

static RvkResult rvk_create_commands(RvkDevice *device) {
    VkCommandPoolCreateInfo pool_info;
    VkCommandBufferAllocateInfo buffer_info;
    VkSemaphoreCreateInfo semaphore_info;
    VkFenceCreateInfo fence_info;
    VkResult result;

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = device->graphics_family;
    result = device->vk.CreateCommandPool(device->device, &pool_info, NULL, &device->command_pool);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkCreateCommandPool");
    }

    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buffer_info.commandPool = device->command_pool;
    buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buffer_info.commandBufferCount = 2;
    {
        VkCommandBuffer command_buffers[2];

        result = device->vk.AllocateCommandBuffers(device->device, &buffer_info, command_buffers);
        if (result != VK_SUCCESS) {
            return rvk_check(device, result, "vkAllocateCommandBuffers");
        }
        device->command_buffer = command_buffers[0];
        device->upload_command_buffer = command_buffers[1];
    }

    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = device->vk.CreateSemaphore(device->device, &semaphore_info, NULL, &device->image_available);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkCreateSemaphore(image_available)");
    }

    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = device->vk.CreateFence(device->device, &fence_info, NULL, &device->in_flight);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkCreateFence");
    }
    result = device->vk.CreateFence(device->device, &fence_info, NULL, &device->upload_fence);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkCreateFence(upload)");
    }
    return RVK_OK;
}

RvkResult rvk_check(RvkDevice *device, VkResult result, const char *operation) {
    if (result == VK_SUCCESS) {
        return RVK_OK;
    }
    if (result == VK_ERROR_DEVICE_LOST) {
        device->device_lost = 1;
        rvk_log(device, 2, "%s failed: device lost", operation);
        return RVK_ERROR_DEVICE_LOST;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        rvk_log(device, 1, "%s returned %d", operation, (int)result);
        return RVK_ERROR_OUT_OF_DATE;
    }
    rvk_log(device, 2, "%s failed with %d", operation, (int)result);
    return RVK_ERROR_VULKAN;
}

RvkResult rvk_create(const RvkInitInfo *info, RvkDevice **out_device) {
    RvkDevice *device;
    RvkResult result;

    if (out_device != NULL) {
        *out_device = NULL;
    }
    if (info == NULL || out_device == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (info->struct_size != 0 && info->struct_size != sizeof(RvkInitInfo)) {
        return RVK_ERROR_ARGUMENT;
    }
    if (info->platform.load_vulkan == NULL || info->platform.unload_vulkan == NULL
        || info->platform.get_instance_proc_addr == NULL
        || info->platform.get_instance_extensions == NULL
        || info->platform.create_surface == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (info->present_mode != RVK_PRESENT_FIFO && info->present_mode != RVK_PRESENT_IMMEDIATE) {
        return RVK_ERROR_ARGUMENT;
    }

    device = (RvkDevice *)calloc(1, sizeof(RvkDevice));
    if (device == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    device->info = *info;
    if (device->info.width == 0) {
        device->info.width = 640;
    }
    if (device->info.height == 0) {
        device->info.height = 480;
    }
    device->timer_frequency = rvk_timer_frequency();

    if (info->platform.load_vulkan(info->platform.user) != 0) {
        const char *error = info->platform.get_error != NULL ? info->platform.get_error(info->platform.user) : NULL;
        rvk_log(device, 2, "platform could not load Vulkan%s%s", error != NULL ? ": " : "", error != NULL ? error : "");
        rvk_destroy(device);
        return RVK_ERROR_NO_DEVICE;
    }

    result = rvk_create_instance(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_surface(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_device(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_commands(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_swapchain(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_depth_resources(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_render_passes(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_framebuffers(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_gpu_resources(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    result = rvk_create_pipelines(device);
    if (result != RVK_OK) {
        rvk_destroy(device);
        return result;
    }
    rvk_log(device, 0, "render_vk initialized: %ux%u, %u swapchain images, present mode %d", device->extent.width, device->extent.height, device->image_count, (int)info->present_mode);
    *out_device = device;
    return RVK_OK;
}

void rvk_destroy(RvkDevice *device) {
    if (device == NULL) {
        return;
    }
    if (device->device != VK_NULL_HANDLE && device->vk.DeviceWaitIdle != NULL) {
        device->vk.DeviceWaitIdle(device->device);
    }
    rvk_destroy_pipelines(device);
    rvk_destroy_gpu_resources(device);
    rvk_destroy_framebuffers(device);
    rvk_destroy_render_passes(device);
    rvk_destroy_depth_resources(device);
    rvk_destroy_swapchain(device);

    if (device->upload_fence != VK_NULL_HANDLE && device->vk.DestroyFence != NULL) {
        device->vk.DestroyFence(device->device, device->upload_fence, NULL);
    }
    if (device->in_flight != VK_NULL_HANDLE && device->vk.DestroyFence != NULL) {
        device->vk.DestroyFence(device->device, device->in_flight, NULL);
    }
    if (device->image_available != VK_NULL_HANDLE && device->vk.DestroySemaphore != NULL) {
        device->vk.DestroySemaphore(device->device, device->image_available, NULL);
    }
    if (device->command_pool != VK_NULL_HANDLE && device->vk.DestroyCommandPool != NULL) {
        device->vk.DestroyCommandPool(device->device, device->command_pool, NULL);
    }
    if (device->device != VK_NULL_HANDLE && device->vk.DestroyDevice != NULL) {
        device->vk.DestroyDevice(device->device, NULL);
    }
    if (device->surface != VK_NULL_HANDLE && device->vk.DestroySurfaceKHR != NULL) {
        device->vk.DestroySurfaceKHR(device->instance, device->surface, NULL);
    }
    if (device->messenger != VK_NULL_HANDLE && device->vk.DestroyDebugUtilsMessengerEXT != NULL) {
        device->vk.DestroyDebugUtilsMessengerEXT(device->instance, device->messenger, NULL);
    }
    if (device->instance != VK_NULL_HANDLE && device->vk.DestroyInstance != NULL) {
        device->vk.DestroyInstance(device->instance, NULL);
    }
    if (device->info.platform.unload_vulkan != NULL) {
        device->info.platform.unload_vulkan(device->info.platform.user);
    }
    free(device->images);
    free(device->views);
    free(device->framebuffers);
    free(device);
}

RvkResult rvk_resize(RvkDevice *device, uint32_t width, uint32_t height) {
    if (device == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    device->pending_resize = 1;
    device->pending_width = width;
    device->pending_height = height;
    return RVK_OK;
}
