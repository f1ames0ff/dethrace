#include "vk_present.h"
#include "vk_shaders.h"

#include "harness/trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VKP_MAX_INSTANCE_EXTENSIONS 8

static PFN_vkGetInstanceProcAddr gpa;

static PFN_vkCreateInstance pCreateInstance;
static PFN_vkDestroyInstance pDestroyInstance;
static PFN_vkEnumerateInstanceExtensionProperties pEnumerateInstanceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices pEnumeratePhysicalDevices;
static PFN_vkEnumerateDeviceExtensionProperties pEnumerateDeviceExtensionProperties;
static PFN_vkGetPhysicalDeviceProperties pGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties pGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties pGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR pGetPhysicalDeviceSurfaceSupportKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR pGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkCreateDevice pCreateDevice;
static PFN_vkDestroyDevice pDestroyDevice;
static PFN_vkGetDeviceQueue pGetDeviceQueue;
static PFN_vkDeviceWaitIdle pDeviceWaitIdle;
static PFN_vkDestroySurfaceKHR pDestroySurfaceKHR;
static PFN_vkCreateSwapchainKHR pCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR pDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR pGetSwapchainImagesKHR;
static PFN_vkAcquireNextImageKHR pAcquireNextImageKHR;
static PFN_vkQueuePresentKHR pQueuePresentKHR;
static PFN_vkQueueSubmit pQueueSubmit;
static PFN_vkCreateCommandPool pCreateCommandPool;
static PFN_vkDestroyCommandPool pDestroyCommandPool;
static PFN_vkAllocateCommandBuffers pAllocateCommandBuffers;
static PFN_vkFreeCommandBuffers pFreeCommandBuffers;
static PFN_vkBeginCommandBuffer pBeginCommandBuffer;
static PFN_vkEndCommandBuffer pEndCommandBuffer;
static PFN_vkResetCommandBuffer pResetCommandBuffer;
static PFN_vkCmdPipelineBarrier pCmdPipelineBarrier;
static PFN_vkCmdCopyBufferToImage pCmdCopyBufferToImage;
static PFN_vkCmdBeginRenderPass pCmdBeginRenderPass;
static PFN_vkCmdEndRenderPass pCmdEndRenderPass;
static PFN_vkCmdBindPipeline pCmdBindPipeline;
static PFN_vkCmdBindDescriptorSets pCmdBindDescriptorSets;
static PFN_vkCmdSetViewport pCmdSetViewport;
static PFN_vkCmdSetScissor pCmdSetScissor;
static PFN_vkCmdDraw pCmdDraw;
static PFN_vkCreateImageView pCreateImageView;
static PFN_vkDestroyImageView pDestroyImageView;
static PFN_vkCreateImage pCreateImage;
static PFN_vkDestroyImage pDestroyImage;
static PFN_vkGetImageMemoryRequirements pGetImageMemoryRequirements;
static PFN_vkCreateBuffer pCreateBuffer;
static PFN_vkDestroyBuffer pDestroyBuffer;
static PFN_vkGetBufferMemoryRequirements pGetBufferMemoryRequirements;
static PFN_vkAllocateMemory pAllocateMemory;
static PFN_vkFreeMemory pFreeMemory;
static PFN_vkBindImageMemory pBindImageMemory;
static PFN_vkBindBufferMemory pBindBufferMemory;
static PFN_vkMapMemory pMapMemory;
static PFN_vkUnmapMemory pUnmapMemory;
static PFN_vkCreateRenderPass pCreateRenderPass;
static PFN_vkDestroyRenderPass pDestroyRenderPass;
static PFN_vkCreateFramebuffer pCreateFramebuffer;
static PFN_vkDestroyFramebuffer pDestroyFramebuffer;
static PFN_vkCreateShaderModule pCreateShaderModule;
static PFN_vkDestroyShaderModule pDestroyShaderModule;
static PFN_vkCreateDescriptorSetLayout pCreateDescriptorSetLayout;
static PFN_vkDestroyDescriptorSetLayout pDestroyDescriptorSetLayout;
static PFN_vkCreateDescriptorPool pCreateDescriptorPool;
static PFN_vkDestroyDescriptorPool pDestroyDescriptorPool;
static PFN_vkAllocateDescriptorSets pAllocateDescriptorSets;
static PFN_vkUpdateDescriptorSets pUpdateDescriptorSets;
static PFN_vkCreatePipelineLayout pCreatePipelineLayout;
static PFN_vkDestroyPipelineLayout pDestroyPipelineLayout;
static PFN_vkCreateGraphicsPipelines pCreateGraphicsPipelines;
static PFN_vkDestroyPipeline pDestroyPipeline;
static PFN_vkCreateSampler pCreateSampler;
static PFN_vkDestroySampler pDestroySampler;
static PFN_vkCreateFence pCreateFence;
static PFN_vkDestroyFence pDestroyFence;
static PFN_vkWaitForFences pWaitForFences;
static PFN_vkResetFences pResetFences;
static PFN_vkCreateSemaphore pCreateSemaphore;
static PFN_vkDestroySemaphore pDestroySemaphore;

typedef struct tVkPresent {
    int active;
    int recovery_attempted;
    tVulkan_host host;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue queue;
    uint32_t queue_family;

    VkSwapchainKHR swapchain;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkExtent2D extent;
    uint32_t image_count;
    VkImage* images;
    VkImageView* views;
    VkFramebuffer* framebuffers;

    VkRenderPass render_pass;
    VkDescriptorSetLayout set_layout;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_set;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkSampler sampler;

    VkBuffer pixel_buffer;
    VkDeviceMemory pixel_memory;
    void* pixel_mapped;
    VkDeviceSize pixel_buffer_size;

    VkImage index_image;
    VkDeviceMemory index_memory;
    VkImageView index_view;
    uint32_t index_width;
    uint32_t index_height;
    int index_initialised;

    VkBuffer palette_buffer;
    VkDeviceMemory palette_memory;
    void* palette_mapped;
    VkImage palette_image;
    VkDeviceMemory palette_image_memory;
    VkImageView palette_view;
    int palette_initialised;
    int palette_dirty;
    br_uint_32 palette[256];

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkFence fence;
    VkSemaphore acquire_semaphore;
    VkSemaphore* render_semaphores;

    br_pixelmap* last_source;
    int has_frame;

    tHarness_viewport viewport;
    int window_width;
    int window_height;
} tVkPresent;

static tVkPresent g;

static int vk_fail(const char* stage, VkResult result) {
    fprintf(stderr, "Vulkan presenter: %s failed (VkResult %d)\n", stage, (int)result);
    return -1;
}

static PFN_vkVoidFunction get_proc(VkInstance instance, const char* name) {
    return gpa(instance, name);
}

static int load_instance_functions(VkInstance instance) {
#define VKP_LOAD(type, name)                                    \
    do {                                                        \
        p##name = (type)get_proc(instance, "vk" #name);         \
        if (p##name == NULL) {                                  \
            fprintf(stderr, "Vulkan presenter: missing vk%s\n", #name); \
            return -1;                                          \
        }                                                       \
    } while (0)

    VKP_LOAD(PFN_vkDestroyInstance, DestroyInstance);
    VKP_LOAD(PFN_vkEnumeratePhysicalDevices, EnumeratePhysicalDevices);
    VKP_LOAD(PFN_vkEnumerateDeviceExtensionProperties, EnumerateDeviceExtensionProperties);
    VKP_LOAD(PFN_vkGetPhysicalDeviceProperties, GetPhysicalDeviceProperties);
    VKP_LOAD(PFN_vkGetPhysicalDeviceQueueFamilyProperties, GetPhysicalDeviceQueueFamilyProperties);
    VKP_LOAD(PFN_vkGetPhysicalDeviceMemoryProperties, GetPhysicalDeviceMemoryProperties);
    VKP_LOAD(PFN_vkGetPhysicalDeviceSurfaceSupportKHR, GetPhysicalDeviceSurfaceSupportKHR);
    VKP_LOAD(PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    VKP_LOAD(PFN_vkGetPhysicalDeviceSurfaceFormatsKHR, GetPhysicalDeviceSurfaceFormatsKHR);
    VKP_LOAD(PFN_vkGetPhysicalDeviceSurfacePresentModesKHR, GetPhysicalDeviceSurfacePresentModesKHR);
    VKP_LOAD(PFN_vkCreateDevice, CreateDevice);
    VKP_LOAD(PFN_vkDestroyDevice, DestroyDevice);
    VKP_LOAD(PFN_vkGetDeviceQueue, GetDeviceQueue);
    VKP_LOAD(PFN_vkDeviceWaitIdle, DeviceWaitIdle);
    VKP_LOAD(PFN_vkDestroySurfaceKHR, DestroySurfaceKHR);
    VKP_LOAD(PFN_vkCreateSwapchainKHR, CreateSwapchainKHR);
    VKP_LOAD(PFN_vkDestroySwapchainKHR, DestroySwapchainKHR);
    VKP_LOAD(PFN_vkGetSwapchainImagesKHR, GetSwapchainImagesKHR);
    VKP_LOAD(PFN_vkAcquireNextImageKHR, AcquireNextImageKHR);
    VKP_LOAD(PFN_vkQueuePresentKHR, QueuePresentKHR);
    VKP_LOAD(PFN_vkQueueSubmit, QueueSubmit);
    VKP_LOAD(PFN_vkCreateCommandPool, CreateCommandPool);
    VKP_LOAD(PFN_vkDestroyCommandPool, DestroyCommandPool);
    VKP_LOAD(PFN_vkAllocateCommandBuffers, AllocateCommandBuffers);
    VKP_LOAD(PFN_vkFreeCommandBuffers, FreeCommandBuffers);
    VKP_LOAD(PFN_vkBeginCommandBuffer, BeginCommandBuffer);
    VKP_LOAD(PFN_vkEndCommandBuffer, EndCommandBuffer);
    VKP_LOAD(PFN_vkResetCommandBuffer, ResetCommandBuffer);
    VKP_LOAD(PFN_vkCmdPipelineBarrier, CmdPipelineBarrier);
    VKP_LOAD(PFN_vkCmdCopyBufferToImage, CmdCopyBufferToImage);
    VKP_LOAD(PFN_vkCmdBeginRenderPass, CmdBeginRenderPass);
    VKP_LOAD(PFN_vkCmdEndRenderPass, CmdEndRenderPass);
    VKP_LOAD(PFN_vkCmdBindPipeline, CmdBindPipeline);
    VKP_LOAD(PFN_vkCmdBindDescriptorSets, CmdBindDescriptorSets);
    VKP_LOAD(PFN_vkCmdSetViewport, CmdSetViewport);
    VKP_LOAD(PFN_vkCmdSetScissor, CmdSetScissor);
    VKP_LOAD(PFN_vkCmdDraw, CmdDraw);
    VKP_LOAD(PFN_vkCreateImageView, CreateImageView);
    VKP_LOAD(PFN_vkDestroyImageView, DestroyImageView);
    VKP_LOAD(PFN_vkCreateImage, CreateImage);
    VKP_LOAD(PFN_vkDestroyImage, DestroyImage);
    VKP_LOAD(PFN_vkGetImageMemoryRequirements, GetImageMemoryRequirements);
    VKP_LOAD(PFN_vkCreateBuffer, CreateBuffer);
    VKP_LOAD(PFN_vkDestroyBuffer, DestroyBuffer);
    VKP_LOAD(PFN_vkGetBufferMemoryRequirements, GetBufferMemoryRequirements);
    VKP_LOAD(PFN_vkAllocateMemory, AllocateMemory);
    VKP_LOAD(PFN_vkFreeMemory, FreeMemory);
    VKP_LOAD(PFN_vkBindImageMemory, BindImageMemory);
    VKP_LOAD(PFN_vkBindBufferMemory, BindBufferMemory);
    VKP_LOAD(PFN_vkMapMemory, MapMemory);
    VKP_LOAD(PFN_vkUnmapMemory, UnmapMemory);
    VKP_LOAD(PFN_vkCreateRenderPass, CreateRenderPass);
    VKP_LOAD(PFN_vkDestroyRenderPass, DestroyRenderPass);
    VKP_LOAD(PFN_vkCreateFramebuffer, CreateFramebuffer);
    VKP_LOAD(PFN_vkDestroyFramebuffer, DestroyFramebuffer);
    VKP_LOAD(PFN_vkCreateShaderModule, CreateShaderModule);
    VKP_LOAD(PFN_vkDestroyShaderModule, DestroyShaderModule);
    VKP_LOAD(PFN_vkCreateDescriptorSetLayout, CreateDescriptorSetLayout);
    VKP_LOAD(PFN_vkDestroyDescriptorSetLayout, DestroyDescriptorSetLayout);
    VKP_LOAD(PFN_vkCreateDescriptorPool, CreateDescriptorPool);
    VKP_LOAD(PFN_vkDestroyDescriptorPool, DestroyDescriptorPool);
    VKP_LOAD(PFN_vkAllocateDescriptorSets, AllocateDescriptorSets);
    VKP_LOAD(PFN_vkUpdateDescriptorSets, UpdateDescriptorSets);
    VKP_LOAD(PFN_vkCreatePipelineLayout, CreatePipelineLayout);
    VKP_LOAD(PFN_vkDestroyPipelineLayout, DestroyPipelineLayout);
    VKP_LOAD(PFN_vkCreateGraphicsPipelines, CreateGraphicsPipelines);
    VKP_LOAD(PFN_vkDestroyPipeline, DestroyPipeline);
    VKP_LOAD(PFN_vkCreateSampler, CreateSampler);
    VKP_LOAD(PFN_vkDestroySampler, DestroySampler);
    VKP_LOAD(PFN_vkCreateFence, CreateFence);
    VKP_LOAD(PFN_vkDestroyFence, DestroyFence);
    VKP_LOAD(PFN_vkWaitForFences, WaitForFences);
    VKP_LOAD(PFN_vkResetFences, ResetFences);
    VKP_LOAD(PFN_vkCreateSemaphore, CreateSemaphore);
    VKP_LOAD(PFN_vkDestroySemaphore, DestroySemaphore);
#undef VKP_LOAD
    return 0;
}

static int find_memory_type(uint32_t type_bits, VkMemoryPropertyFlags properties, uint32_t* index) {
    VkPhysicalDeviceMemoryProperties memory_properties;
    uint32_t i;

    pGetPhysicalDeviceMemoryProperties(g.physical_device, &memory_properties);

    for (i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            *index = i;
            return 0;
        }
    }
    return -1;
}

static int create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer* buffer, VkDeviceMemory* memory, void** mapped) {
    VkBufferCreateInfo buffer_info;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo allocate_info;
    VkResult result;
    uint32_t memory_type;

    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    result = pCreateBuffer(g.device, &buffer_info, NULL, buffer);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateBuffer", result);
    }
    pGetBufferMemoryRequirements(g.device, *buffer, &requirements);
    if (find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memory_type) != 0) {
        fprintf(stderr, "Vulkan presenter: no host visible memory type for buffer\n");
        return -1;
    }

    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = memory_type;
    result = pAllocateMemory(g.device, &allocate_info, NULL, memory);
    if (result != VK_SUCCESS) {
        return vk_fail("vkAllocateMemory(buffer)", result);
    }
    if (pBindBufferMemory(g.device, *buffer, *memory, 0) != VK_SUCCESS) {
        fprintf(stderr, "Vulkan presenter: vkBindBufferMemory failed\n");
        return -1;
    }
    if (mapped != NULL) {
        result = pMapMemory(g.device, *memory, 0, VK_WHOLE_SIZE, 0, mapped);
        if (result != VK_SUCCESS) {
            return vk_fail("vkMapMemory", result);
        }
    }
    return 0;
}

static int create_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* memory, VkImageView* view, int* initialised) {
    VkImageCreateInfo image_info;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo allocate_info;
    VkImageViewCreateInfo view_info;
    VkResult result;
    uint32_t memory_type;

    memset(&image_info, 0, sizeof(image_info));
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent.width = width;
    image_info.extent.height = height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = usage;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    result = pCreateImage(g.device, &image_info, NULL, image);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateImage", result);
    }
    pGetImageMemoryRequirements(g.device, *image, &requirements);
    if (find_memory_type(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory_type) != 0) {
        fprintf(stderr, "Vulkan presenter: no device local memory type\n");
        return -1;
    }

    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = requirements.size;
    allocate_info.memoryTypeIndex = memory_type;
    result = pAllocateMemory(g.device, &allocate_info, NULL, memory);
    if (result != VK_SUCCESS) {
        return vk_fail("vkAllocateMemory(image)", result);
    }
    if (pBindImageMemory(g.device, *image, *memory, 0) != VK_SUCCESS) {
        fprintf(stderr, "Vulkan presenter: vkBindImageMemory failed\n");
        return -1;
    }

    memset(&view_info, 0, sizeof(view_info));
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = *image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    result = pCreateImageView(g.device, &view_info, NULL, view);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateImageView", result);
    }
    if (initialised != NULL) {
        *initialised = 0;
    }
    return 0;
}

static void destroy_buffer(VkBuffer* buffer, VkDeviceMemory* memory) {
    if (*buffer != VK_NULL_HANDLE) {
        pDestroyBuffer(g.device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
    }
    if (*memory != VK_NULL_HANDLE) {
        pFreeMemory(g.device, *memory, NULL);
        *memory = VK_NULL_HANDLE;
    }
}

static void destroy_image(VkImage* image, VkDeviceMemory* memory, VkImageView* view) {
    if (*view != VK_NULL_HANDLE) {
        pDestroyImageView(g.device, *view, NULL);
        *view = VK_NULL_HANDLE;
    }
    if (*image != VK_NULL_HANDLE) {
        pDestroyImage(g.device, *image, NULL);
        *image = VK_NULL_HANDLE;
    }
    if (*memory != VK_NULL_HANDLE) {
        pFreeMemory(g.device, *memory, NULL);
        *memory = VK_NULL_HANDLE;
    }
}

static int create_instance(void) {
    const char* extensions[VKP_MAX_INSTANCE_EXTENSIONS];
    uint32_t extension_count = 0;
    uint32_t i;
    VkApplicationInfo app_info;
    VkInstanceCreateInfo create_info;
    VkResult result;
    VkExtensionProperties* available = NULL;
    uint32_t available_count = 0;
    int portability = 0;

    pCreateInstance = (PFN_vkCreateInstance)gpa(NULL, "vkCreateInstance");
    pEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)gpa(NULL, "vkEnumerateInstanceExtensionProperties");
    if (pCreateInstance == NULL || pEnumerateInstanceExtensionProperties == NULL) {
        fprintf(stderr, "Vulkan presenter: loader is missing core instance entry points\n");
        return -1;
    }

    if (g.host.get_instance_extensions(g.host.window, &extension_count, extensions, VKP_MAX_INSTANCE_EXTENSIONS) != 0) {
        fprintf(stderr, "Vulkan presenter: could not get SDL Vulkan instance extensions\n");
        return -1;
    }

    result = pEnumerateInstanceExtensionProperties(NULL, &available_count, NULL);
    if (result == VK_SUCCESS && available_count > 0) {
        available = malloc(sizeof(VkExtensionProperties) * available_count);
        if (available != NULL && pEnumerateInstanceExtensionProperties(NULL, &available_count, available) == VK_SUCCESS) {
            for (i = 0; i < available_count; i++) {
                if (strcmp(available[i].extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                    portability = 1;
                    break;
                }
            }
        }
        free(available);
    }

    if (portability && extension_count < VKP_MAX_INSTANCE_EXTENSIONS) {
        extensions[extension_count++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    }

    memset(&app_info, 0, sizeof(app_info));
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "dethrace";
    app_info.pEngineName = "dethrace";
    app_info.apiVersion = VK_API_VERSION_1_0;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = extension_count;
    create_info.ppEnabledExtensionNames = extension_count > 0 ? extensions : NULL;
    if (portability) {
        create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    result = pCreateInstance(&create_info, NULL, &g.instance);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateInstance", result);
    }
    if (load_instance_functions(g.instance) != 0) {
        return -1;
    }
    if (g.host.create_surface(g.host.window, g.instance, &g.surface) != 0) {
        fprintf(stderr, "Vulkan presenter: SDL_Vulkan_CreateSurface failed\n");
        return -1;
    }
    return 0;
}

static int pick_physical_device(void) {
    uint32_t device_count = 0;
    VkPhysicalDevice* devices;
    uint32_t i;
    VkResult result;

    result = pEnumeratePhysicalDevices(g.instance, &device_count, NULL);
    if (result != VK_SUCCESS || device_count == 0) {
        fprintf(stderr, "Vulkan presenter: no physical devices\n");
        return -1;
    }
    devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    if (devices == NULL) {
        return -1;
    }
    result = pEnumeratePhysicalDevices(g.instance, &device_count, devices);
    if (result != VK_SUCCESS) {
        free(devices);
        return vk_fail("vkEnumeratePhysicalDevices", result);
    }

    for (i = 0; i < device_count; i++) {
        uint32_t queue_family_count = 0;
        VkQueueFamilyProperties* queue_families;
        uint32_t j;
        int graphics_family = -1;
        int present = 0;
        uint32_t extension_count = 0;
        VkExtensionProperties* extensions;
        int has_swapchain = 0;
        VkBool32 supports_present = VK_FALSE;

        pGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, NULL);
        queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
        if (queue_families == NULL) {
            continue;
        }
        pGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_family_count, queue_families);
        for (j = 0; j < queue_family_count; j++) {
            if ((queue_families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                graphics_family = (int)j;
                pGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, g.surface, &supports_present);
                present = supports_present != VK_FALSE;
                break;
            }
        }
        free(queue_families);
        if (graphics_family < 0 || !present) {
            continue;
        }

        if (pEnumerateDeviceExtensionProperties(devices[i], VK_NULL_HANDLE, &extension_count, NULL) != VK_SUCCESS || extension_count == 0) {
            continue;
        }
        extensions = malloc(sizeof(VkExtensionProperties) * extension_count);
        if (extensions == NULL) {
            continue;
        }
        if (pEnumerateDeviceExtensionProperties(devices[i], VK_NULL_HANDLE, &extension_count, extensions) == VK_SUCCESS) {
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

        g.physical_device = devices[i];
        g.queue_family = (uint32_t)graphics_family;
        break;
    }
    free(devices);

    if (g.physical_device == VK_NULL_HANDLE) {
        fprintf(stderr, "Vulkan presenter: no device with graphics/present queues and swapchain support\n");
        return -1;
    }
    return 0;
}

static int create_device(void) {
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info;
    VkDeviceCreateInfo device_info;
    const char* extensions[1];
    VkResult result;

    memset(&queue_info, 0, sizeof(queue_info));
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = g.queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;

    extensions[0] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    memset(&device_info, 0, sizeof(device_info));
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = extensions;

    result = pCreateDevice(g.physical_device, &device_info, NULL, &g.device);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateDevice", result);
    }
    pGetDeviceQueue(g.device, g.queue_family, 0, &g.queue);
    return 0;
}

static int choose_surface_format(void) {
    uint32_t count = 0;
    VkSurfaceFormatKHR* formats;
    uint32_t i;
    VkResult result;
    int found = 0;

    result = pGetPhysicalDeviceSurfaceFormatsKHR(g.physical_device, g.surface, &count, NULL);
    if (result != VK_SUCCESS || count == 0) {
        return -1;
    }
    formats = malloc(sizeof(VkSurfaceFormatKHR) * count);
    if (formats == NULL) {
        return -1;
    }
    if (pGetPhysicalDeviceSurfaceFormatsKHR(g.physical_device, g.surface, &count, formats) != VK_SUCCESS) {
        free(formats);
        return -1;
    }
    g.format = formats[0].format;
    g.color_space = formats[0].colorSpace;
    for (i = 0; i < count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            g.format = formats[i].format;
            g.color_space = formats[i].colorSpace;
            found = 1;
            break;
        }
    }
    if (!found) {
        for (i = 0; i < count; i++) {
            if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
                g.format = formats[i].format;
                g.color_space = formats[i].colorSpace;
                break;
            }
        }
    }
    free(formats);
    return 0;
}

static int get_extent(VkExtent2D* extent) {
    VkSurfaceCapabilitiesKHR capabilities;
    VkResult result;

    result = pGetPhysicalDeviceSurfaceCapabilitiesKHR(g.physical_device, g.surface, &capabilities);
    if (result != VK_SUCCESS) {
        return vk_fail("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
    }
    if (capabilities.currentExtent.width != 0xFFFFFFFFu) {
        *extent = capabilities.currentExtent;
        return 0;
    }
    extent->width = (uint32_t)g.window_width;
    extent->height = (uint32_t)g.window_height;
    if (extent->width < capabilities.minImageExtent.width) {
        extent->width = capabilities.minImageExtent.width;
    }
    if (extent->width > capabilities.maxImageExtent.width) {
        extent->width = capabilities.maxImageExtent.width;
    }
    if (extent->height < capabilities.minImageExtent.height) {
        extent->height = capabilities.minImageExtent.height;
    }
    if (extent->height > capabilities.maxImageExtent.height) {
        extent->height = capabilities.maxImageExtent.height;
    }
    return 0;
}

static void destroy_framebuffers_and_views(void) {
    uint32_t i;

    if (g.framebuffers != NULL) {
        for (i = 0; i < g.image_count; i++) {
            if (g.framebuffers[i] != VK_NULL_HANDLE) {
                pDestroyFramebuffer(g.device, g.framebuffers[i], NULL);
            }
        }
        free(g.framebuffers);
        g.framebuffers = NULL;
    }
    if (g.views != NULL) {
        for (i = 0; i < g.image_count; i++) {
            if (g.views[i] != VK_NULL_HANDLE) {
                pDestroyImageView(g.device, g.views[i], NULL);
            }
        }
        free(g.views);
        g.views = NULL;
    }
    free(g.images);
    g.images = NULL;
    g.image_count = 0;
}

static int create_swapchain(VkSwapchainKHR old_swapchain) {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSwapchainCreateInfoKHR create_info;
    VkExtent2D extent;
    VkPresentModeKHR* present_modes;
    uint32_t present_mode_count = 0;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t i;
    uint32_t image_count;
    VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkResult result;

    if (get_extent(&extent) != 0) {
        return -1;
    }
    if (extent.width == 0 || extent.height == 0) {
        fprintf(stderr, "Vulkan presenter: zero-sized surface\n");
        return -1;
    }
    if (pGetPhysicalDeviceSurfaceCapabilitiesKHR(g.physical_device, g.surface, &capabilities) != VK_SUCCESS) {
        return -1;
    }
    if ((capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0) {
        composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }

    present_modes = NULL;
    if (pGetPhysicalDeviceSurfacePresentModesKHR(g.physical_device, g.surface, &present_mode_count, NULL) == VK_SUCCESS && present_mode_count > 0) {
        present_modes = malloc(sizeof(VkPresentModeKHR) * present_mode_count);
        if (present_modes != NULL) {
            if (pGetPhysicalDeviceSurfacePresentModesKHR(g.physical_device, g.surface, &present_mode_count, present_modes) == VK_SUCCESS) {
                present_mode = present_modes[0];
                for (i = 0; i < present_mode_count; i++) {
                    if (present_modes[i] == VK_PRESENT_MODE_FIFO_KHR) {
                        present_mode = VK_PRESENT_MODE_FIFO_KHR;
                        break;
                    }
                }
            }
            free(present_modes);
        }
    }

    image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = g.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = g.format;
    create_info.imageColorSpace = g.color_space;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = composite;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = old_swapchain;

    result = pCreateSwapchainKHR(g.device, &create_info, NULL, &g.swapchain);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateSwapchainKHR", result);
    }
    g.extent = extent;

    result = pGetSwapchainImagesKHR(g.device, g.swapchain, &image_count, NULL);
    if (result != VK_SUCCESS || image_count == 0) {
        return vk_fail("vkGetSwapchainImagesKHR", result);
    }
    g.image_count = image_count;
    g.images = malloc(sizeof(VkImage) * g.image_count);
    g.views = calloc(g.image_count, sizeof(VkImageView));
    g.framebuffers = calloc(g.image_count, sizeof(VkFramebuffer));
    if (g.images == NULL || g.views == NULL || g.framebuffers == NULL) {
        return -1;
    }
    result = pGetSwapchainImagesKHR(g.device, g.swapchain, &image_count, g.images);
    if (result != VK_SUCCESS || image_count > g.image_count) {
        return vk_fail("vkGetSwapchainImagesKHR", result);
    }
    for (i = 0; i < g.image_count; i++) {
        VkImageViewCreateInfo view_info;
        memset(&view_info, 0, sizeof(view_info));
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = g.images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = g.format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.layerCount = 1;
        result = pCreateImageView(g.device, &view_info, NULL, &g.views[i]);
        if (result != VK_SUCCESS) {
            return vk_fail("vkCreateImageView(swapchain)", result);
        }
    }
    for (i = 0; i < g.image_count; i++) {
        VkFramebufferCreateInfo framebuffer_info;
        memset(&framebuffer_info, 0, sizeof(framebuffer_info));
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = g.render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &g.views[i];
        framebuffer_info.width = g.extent.width;
        framebuffer_info.height = g.extent.height;
        framebuffer_info.layers = 1;
        result = pCreateFramebuffer(g.device, &framebuffer_info, NULL, &g.framebuffers[i]);
        if (result != VK_SUCCESS) {
            return vk_fail("vkCreateFramebuffer", result);
        }
    }

    g.render_semaphores = calloc(g.image_count, sizeof(VkSemaphore));
    if (g.render_semaphores == NULL) {
        return -1;
    }
    for (i = 0; i < g.image_count; i++) {
        VkSemaphoreCreateInfo semaphore_info;
        memset(&semaphore_info, 0, sizeof(semaphore_info));
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        result = pCreateSemaphore(g.device, &semaphore_info, NULL, &g.render_semaphores[i]);
        if (result != VK_SUCCESS) {
            return vk_fail("vkCreateSemaphore", result);
        }
    }
    return 0;
}

static void destroy_swapchain_objects(void) {
    uint32_t i;

    if (g.render_semaphores != NULL) {
        for (i = 0; i < g.image_count; i++) {
            if (g.render_semaphores[i] != VK_NULL_HANDLE) {
                pDestroySemaphore(g.device, g.render_semaphores[i], NULL);
            }
        }
        free(g.render_semaphores);
        g.render_semaphores = NULL;
    }
    destroy_framebuffers_and_views();
}

static void destroy_swapchain(void) {
    destroy_swapchain_objects();
    if (g.swapchain != VK_NULL_HANDLE) {
        pDestroySwapchainKHR(g.device, g.swapchain, NULL);
        g.swapchain = VK_NULL_HANDLE;
    }
}

static int create_render_pass(void) {
    VkAttachmentDescription attachment;
    VkAttachmentReference color_reference;
    VkSubpassDescription subpass;
    VkRenderPassCreateInfo render_pass_info;
    VkResult result;

    memset(&attachment, 0, sizeof(attachment));
    attachment.format = g.format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    memset(&color_reference, 0, sizeof(color_reference));
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;

    memset(&render_pass_info, 0, sizeof(render_pass_info));
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    result = pCreateRenderPass(g.device, &render_pass_info, NULL, &g.render_pass);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateRenderPass", result);
    }
    return 0;
}

static int create_shader_module(const uint32_t* code, size_t size, VkShaderModule* module) {
    VkShaderModuleCreateInfo module_info;
    VkResult result;

    memset(&module_info, 0, sizeof(module_info));
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = size;
    module_info.pCode = code;
    result = pCreateShaderModule(g.device, &module_info, NULL, module);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateShaderModule", result);
    }
    return 0;
}

static int create_pipeline(void) {
    VkShaderModule vertex_module;
    VkShaderModule fragment_module;
    VkPipelineShaderStageCreateInfo stages[2];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState blend_attachment;
    VkPipelineColorBlendStateCreateInfo blend;
    VkDynamicState dynamic_states[2];
    VkPipelineDynamicStateCreateInfo dynamic;
    VkPipelineLayoutCreateInfo layout_info;
    VkGraphicsPipelineCreateInfo pipeline_info;
    VkResult result;

    if (create_shader_module(PRESENT_VERT_SPV, sizeof(PRESENT_VERT_SPV), &vertex_module) != 0) {
        return -1;
    }
    if (create_shader_module(PRESENT_FRAG_SPV, sizeof(PRESENT_FRAG_SPV), &fragment_module) != 0) {
        pDestroyShaderModule(g.device, vertex_module, NULL);
        return -1;
    }

    memset(stages, 0, sizeof(stages));
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex_module;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment_module;
    stages[1].pName = "main";

    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    memset(&input_assembly, 0, sizeof(input_assembly));
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    memset(&dynamic, 0, sizeof(dynamic));
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamic_states;

    memset(&viewport_state, 0, sizeof(viewport_state));
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    memset(&rasterization, 0, sizeof(rasterization));
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    memset(&multisample, 0, sizeof(multisample));
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    memset(&blend_attachment, 0, sizeof(blend_attachment));
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = VK_FALSE;

    memset(&blend, 0, sizeof(blend));
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;

    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &g.set_layout;
    result = pCreatePipelineLayout(g.device, &layout_info, NULL, &g.pipeline_layout);
    if (result != VK_SUCCESS) {
        pDestroyShaderModule(g.device, fragment_module, NULL);
        pDestroyShaderModule(g.device, vertex_module, NULL);
        return vk_fail("vkCreatePipelineLayout", result);
    }

    memset(&pipeline_info, 0, sizeof(pipeline_info));
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pColorBlendState = &blend;
    pipeline_info.pDynamicState = &dynamic;
    pipeline_info.layout = g.pipeline_layout;
    pipeline_info.renderPass = g.render_pass;
    pipeline_info.subpass = 0;

    result = pCreateGraphicsPipelines(g.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &g.pipeline);
    if (result != VK_SUCCESS) {
        pDestroyShaderModule(g.device, fragment_module, NULL);
        pDestroyShaderModule(g.device, vertex_module, NULL);
        return vk_fail("vkCreateGraphicsPipelines", result);
    }

    pDestroyShaderModule(g.device, fragment_module, NULL);
    pDestroyShaderModule(g.device, vertex_module, NULL);
    return 0;
}

static int create_descriptors(void) {
    VkDescriptorSetLayoutBinding bindings[2];
    VkDescriptorSetLayoutCreateInfo layout_info;
    VkDescriptorPoolSize pool_size;
    VkDescriptorPoolCreateInfo pool_info;
    VkDescriptorSetAllocateInfo allocate_info;
    VkSamplerCreateInfo sampler_info;
    VkResult result;

    memset(bindings, 0, sizeof(bindings));
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;
    result = pCreateDescriptorSetLayout(g.device, &layout_info, NULL, &g.set_layout);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateDescriptorSetLayout", result);
    }

    memset(&pool_size, 0, sizeof(pool_size));
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 2;

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    result = pCreateDescriptorPool(g.device, &pool_info, NULL, &g.descriptor_pool);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateDescriptorPool", result);
    }

    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = g.descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &g.set_layout;
    result = pAllocateDescriptorSets(g.device, &allocate_info, &g.descriptor_set);
    if (result != VK_SUCCESS) {
        return vk_fail("vkAllocateDescriptorSets", result);
    }

    memset(&sampler_info, 0, sizeof(sampler_info));
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.maxLod = 0.0f;
    result = pCreateSampler(g.device, &sampler_info, NULL, &g.sampler);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateSampler", result);
    }
    return 0;
}

static int create_palette_image(void) {
    if (create_image(256, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &g.palette_image, &g.palette_image_memory, &g.palette_view, &g.palette_initialised) != 0) {
        return -1;
    }
    if (create_buffer(256 * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &g.palette_buffer, &g.palette_memory, &g.palette_mapped) != 0) {
        return -1;
    }
    return 0;
}

static int update_descriptors(void) {
    VkDescriptorImageInfo index_info;
    VkDescriptorImageInfo palette_info;
    VkWriteDescriptorSet writes[2];

    memset(&index_info, 0, sizeof(index_info));
    index_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    index_info.imageView = g.index_view;
    index_info.sampler = g.sampler;

    memset(&palette_info, 0, sizeof(palette_info));
    palette_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    palette_info.imageView = g.palette_view;
    palette_info.sampler = g.sampler;

    memset(writes, 0, sizeof(writes));
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = g.descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].pImageInfo = &index_info;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = g.descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &palette_info;

    pUpdateDescriptorSets(g.device, 2, writes, 0, NULL);
    return 0;
}

static int create_sync_objects(void) {
    VkCommandPoolCreateInfo pool_info;
    VkCommandBufferAllocateInfo allocate_info;
    VkFenceCreateInfo fence_info;
    VkSemaphoreCreateInfo semaphore_info;
    VkResult result;

    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = g.queue_family;
    result = pCreateCommandPool(g.device, &pool_info, NULL, &g.command_pool);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateCommandPool", result);
    }

    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.commandPool = g.command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;
    result = pAllocateCommandBuffers(g.device, &allocate_info, &g.command_buffer);
    if (result != VK_SUCCESS) {
        return vk_fail("vkAllocateCommandBuffers", result);
    }

    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = pCreateFence(g.device, &fence_info, NULL, &g.fence);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateFence", result);
    }

    memset(&semaphore_info, 0, sizeof(semaphore_info));
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = pCreateSemaphore(g.device, &semaphore_info, NULL, &g.acquire_semaphore);
    if (result != VK_SUCCESS) {
        return vk_fail("vkCreateSemaphore", result);
    }
    return 0;
}

static int ensure_index_resources(uint32_t width, uint32_t height) {
    if (g.index_image != VK_NULL_HANDLE && g.index_width == width && g.index_height == height) {
        return 0;
    }
    pDeviceWaitIdle(g.device);
    destroy_image(&g.index_image, &g.index_memory, &g.index_view);
    destroy_buffer(&g.pixel_buffer, &g.pixel_memory);
    g.pixel_mapped = NULL;
    g.pixel_buffer_size = 0;

    if (create_image(width, height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &g.index_image, &g.index_memory, &g.index_view, &g.index_initialised) != 0) {
        return -1;
    }
    if (create_buffer((VkDeviceSize)width * height, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &g.pixel_buffer, &g.pixel_memory, &g.pixel_mapped) != 0) {
        return -1;
    }
    g.index_width = width;
    g.index_height = height;
    g.pixel_buffer_size = (VkDeviceSize)width * height;
    return update_descriptors();
}

static int recreate_swapchain(void) {
    VkExtent2D extent;
    VkSwapchainKHR old_swapchain;
    int result;

    if (get_extent(&extent) != 0 || extent.width == 0 || extent.height == 0) {
        return 0;
    }
    pDeviceWaitIdle(g.device);
    old_swapchain = g.swapchain;
    destroy_swapchain_objects();
    g.swapchain = VK_NULL_HANDLE;
    result = create_swapchain(old_swapchain);
    if (old_swapchain != VK_NULL_HANDLE) {
        pDestroySwapchainKHR(g.device, old_swapchain, NULL);
    }
    return result;
}

static void recover_device_lost(void) {
    tVulkan_host host = g.host;
    int window_width = g.window_width;
    int window_height = g.window_height;
    int previous_attempts = g.recovery_attempted;

    if (previous_attempts > 0) {
        fprintf(stderr, "Vulkan presenter: device lost again after recovery, giving up\n");
        g.active = 0;
        return;
    }
    LOG_WARN("Vulkan device lost, reinitialising the presenter");
    VkPresent_Shutdown();
    if (VkPresent_Init(&host, window_width, window_height) != 0) {
        fprintf(stderr, "Vulkan presenter: reinitialisation after device loss failed\n");
        return;
    }
    g.recovery_attempted = previous_attempts + 1;
}

static int presenter_present(br_pixelmap* src, int upload) {
    VkResult result;
    uint32_t image_index = 0;
    VkCommandBufferBeginInfo begin_info;
    VkImageMemoryBarrier barriers[2];
    uint32_t barrier_count;
    VkBufferMemoryBarrier buffer_barrier;
    VkRenderPassBeginInfo render_pass_info;
    VkClearValue clear_value;
    VkViewport viewport;
    VkRect2D scissor;
    VkSubmitInfo submit_info;
    VkPresentInfoKHR present_info;
    VkPipelineStageFlags wait_stage;
    int need_pixel_buffer_barrier = 0;
    int need_palette_barrier = 0;

    if (g.window_width <= 0 || g.window_height <= 0) {
        return 0;
    }

    if (upload) {
        if (ensure_index_resources(src->width, src->height) != 0) {
            return -1;
        }
    }
    if (g.index_image == VK_NULL_HANDLE) {
        return 0;
    }

    result = pWaitForFences(g.device, 1, &g.fence, VK_TRUE, UINT64_MAX);
    if (result == VK_ERROR_DEVICE_LOST) {
        recover_device_lost();
        return 0;
    }
    if (result != VK_SUCCESS) {
        return vk_fail("vkWaitForFences", result);
    }

    if (upload) {
        uint32_t row;
        const uint8_t* source = src->pixels;
        uint8_t* destination = g.pixel_mapped;
        for (row = 0; row < src->height; row++) {
            memcpy(destination + (size_t)row * src->width, source + (size_t)row * src->row_bytes, src->width);
        }
        need_pixel_buffer_barrier = 1;
    }
    if (g.palette_dirty) {
        memcpy(g.palette_mapped, g.palette, sizeof(g.palette));
        need_palette_barrier = 1;
    }

    result = pAcquireNextImageKHR(g.device, g.swapchain, UINT64_MAX, g.acquire_semaphore, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (recreate_swapchain() != 0) {
            g.active = 0;
        }
        return 0;
    }
    if (result == VK_ERROR_DEVICE_LOST) {
        recover_device_lost();
        return 0;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return vk_fail("vkAcquireNextImageKHR", result);
    }

    pResetFences(g.device, 1, &g.fence);

    pResetCommandBuffer(g.command_buffer, 0);
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = pBeginCommandBuffer(g.command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return vk_fail("vkBeginCommandBuffer", result);
    }

    if (need_pixel_buffer_barrier || need_palette_barrier) {
        VkBufferMemoryBarrier buffer_barriers[2];
        uint32_t buffer_barrier_count = 0;
        memset(&buffer_barrier, 0, sizeof(buffer_barrier));
        buffer_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        buffer_barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        buffer_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        buffer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        buffer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        if (need_pixel_buffer_barrier) {
            buffer_barrier.buffer = g.pixel_buffer;
            buffer_barrier.offset = 0;
            buffer_barrier.size = VK_WHOLE_SIZE;
            buffer_barriers[buffer_barrier_count++] = buffer_barrier;
        }
        if (need_palette_barrier) {
            buffer_barrier.buffer = g.palette_buffer;
            buffer_barrier.offset = 0;
            buffer_barrier.size = VK_WHOLE_SIZE;
            buffer_barriers[buffer_barrier_count++] = buffer_barrier;
        }
        pCmdPipelineBarrier(g.command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, NULL, buffer_barrier_count, buffer_barriers, 0, NULL);
    }

    barrier_count = 0;
    if (upload) {
        VkImageMemoryBarrier* barrier = &barriers[barrier_count++];
        memset(barrier, 0, sizeof(*barrier));
        barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier->oldLayout = g.index_initialised ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier->newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->image = g.index_image;
        barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier->subresourceRange.levelCount = 1;
        barrier->subresourceRange.layerCount = 1;
        if (g.index_initialised) {
            barrier->srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        } else {
            barrier->srcAccessMask = 0;
            barrier->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
    }
    if (g.palette_dirty) {
        VkImageMemoryBarrier* barrier = &barriers[barrier_count++];
        memset(barrier, 0, sizeof(*barrier));
        barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier->oldLayout = g.palette_initialised ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier->newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->image = g.palette_image;
        barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier->subresourceRange.levelCount = 1;
        barrier->subresourceRange.layerCount = 1;
        if (g.palette_initialised) {
            barrier->srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        } else {
            barrier->srcAccessMask = 0;
            barrier->dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        }
    }
    if (barrier_count > 0) {
        pCmdPipelineBarrier(g.command_buffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, NULL, 0, NULL, barrier_count, barriers);
    }

    if (upload) {
        VkBufferImageCopy region;
        memset(&region, 0, sizeof(region));
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = g.index_width;
        region.imageExtent.height = g.index_height;
        region.imageExtent.depth = 1;
        pCmdCopyBufferToImage(g.command_buffer, g.pixel_buffer, g.index_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        g.index_initialised = 1;
    }
    if (g.palette_dirty) {
        VkBufferImageCopy region;
        memset(&region, 0, sizeof(region));
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width = 256;
        region.imageExtent.height = 1;
        region.imageExtent.depth = 1;
        pCmdCopyBufferToImage(g.command_buffer, g.palette_buffer, g.palette_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        g.palette_initialised = 1;
    }

    barrier_count = 0;
    if (upload) {
        VkImageMemoryBarrier* barrier = &barriers[barrier_count++];
        memset(barrier, 0, sizeof(*barrier));
        barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier->newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->image = g.index_image;
        barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier->subresourceRange.levelCount = 1;
        barrier->subresourceRange.layerCount = 1;
    }
    if (g.palette_dirty) {
        VkImageMemoryBarrier* barrier = &barriers[barrier_count++];
        memset(barrier, 0, sizeof(*barrier));
        barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier->newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->image = g.palette_image;
        barrier->subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier->subresourceRange.levelCount = 1;
        barrier->subresourceRange.layerCount = 1;
    }
    if (barrier_count > 0) {
        pCmdPipelineBarrier(g.command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, NULL, 0, NULL, barrier_count, barriers);
    }

    memset(&clear_value, 0, sizeof(clear_value));
    clear_value.color.float32[3] = 1.0f;

    memset(&render_pass_info, 0, sizeof(render_pass_info));
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = g.render_pass;
    render_pass_info.framebuffer = g.framebuffers[image_index];
    render_pass_info.renderArea.extent = g.extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;
    pCmdBeginRenderPass(g.command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    viewport.x = (float)g.viewport.x;
    viewport.y = (float)g.viewport.y;
    viewport.width = (float)(g.viewport.width > 0 ? g.viewport.width : (int)g.extent.width);
    viewport.height = (float)(g.viewport.height > 0 ? g.viewport.height : (int)g.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    pCmdSetViewport(g.command_buffer, 0, 1, &viewport);

    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = g.extent;
    pCmdSetScissor(g.command_buffer, 0, 1, &scissor);

    pCmdBindPipeline(g.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipeline);
    pCmdBindDescriptorSets(g.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g.pipeline_layout, 0, 1, &g.descriptor_set, 0, NULL);
    pCmdDraw(g.command_buffer, 3, 1, 0, 0);
    pCmdEndRenderPass(g.command_buffer);

    result = pEndCommandBuffer(g.command_buffer);
    if (result != VK_SUCCESS) {
        return vk_fail("vkEndCommandBuffer", result);
    }

    wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &g.acquire_semaphore;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &g.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &g.render_semaphores[image_index];
    result = pQueueSubmit(g.queue, 1, &submit_info, g.fence);
    if (result == VK_ERROR_DEVICE_LOST) {
        recover_device_lost();
        return 0;
    }
    if (result != VK_SUCCESS) {
        return vk_fail("vkQueueSubmit", result);
    }

    memset(&present_info, 0, sizeof(present_info));
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &g.render_semaphores[image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &g.swapchain;
    present_info.pImageIndices = &image_index;
    result = pQueuePresentKHR(g.queue, &present_info);
    if (result == VK_ERROR_DEVICE_LOST) {
        recover_device_lost();
        return 0;
    }
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        if (recreate_swapchain() != 0) {
            g.active = 0;
        }
    } else if (result != VK_SUCCESS) {
        return vk_fail("vkQueuePresentKHR", result);
    }

    g.palette_dirty = 0;
    g.last_source = src;
    g.has_frame = 1;
    return 0;
}

static void destroy_all(void) {
    if (g.device != VK_NULL_HANDLE) {
        pDeviceWaitIdle(g.device);
    }
    destroy_swapchain();
    destroy_image(&g.index_image, &g.index_memory, &g.index_view);
    destroy_buffer(&g.pixel_buffer, &g.pixel_memory);
    destroy_image(&g.palette_image, &g.palette_image_memory, &g.palette_view);
    destroy_buffer(&g.palette_buffer, &g.palette_memory);
    g.pixel_mapped = NULL;
    g.palette_mapped = NULL;

    if (g.device != VK_NULL_HANDLE) {
        if (g.fence != VK_NULL_HANDLE) {
            pDestroyFence(g.device, g.fence, NULL);
            g.fence = VK_NULL_HANDLE;
        }
        if (g.acquire_semaphore != VK_NULL_HANDLE) {
            pDestroySemaphore(g.device, g.acquire_semaphore, NULL);
            g.acquire_semaphore = VK_NULL_HANDLE;
        }
        if (g.command_pool != VK_NULL_HANDLE) {
            pDestroyCommandPool(g.device, g.command_pool, NULL);
            g.command_pool = VK_NULL_HANDLE;
        }
        if (g.sampler != VK_NULL_HANDLE) {
            pDestroySampler(g.device, g.sampler, NULL);
            g.sampler = VK_NULL_HANDLE;
        }
        if (g.pipeline != VK_NULL_HANDLE) {
            pDestroyPipeline(g.device, g.pipeline, NULL);
            g.pipeline = VK_NULL_HANDLE;
        }
        if (g.pipeline_layout != VK_NULL_HANDLE) {
            pDestroyPipelineLayout(g.device, g.pipeline_layout, NULL);
            g.pipeline_layout = VK_NULL_HANDLE;
        }
        if (g.descriptor_pool != VK_NULL_HANDLE) {
            pDestroyDescriptorPool(g.device, g.descriptor_pool, NULL);
            g.descriptor_pool = VK_NULL_HANDLE;
        }
        if (g.set_layout != VK_NULL_HANDLE) {
            pDestroyDescriptorSetLayout(g.device, g.set_layout, NULL);
            g.set_layout = VK_NULL_HANDLE;
        }
        if (g.render_pass != VK_NULL_HANDLE) {
            pDestroyRenderPass(g.device, g.render_pass, NULL);
            g.render_pass = VK_NULL_HANDLE;
        }
        pDestroyDevice(g.device, NULL);
        g.device = VK_NULL_HANDLE;
    }
    if (g.surface != VK_NULL_HANDLE && pDestroySurfaceKHR != NULL) {
        pDestroySurfaceKHR(g.instance, g.surface, NULL);
        g.surface = VK_NULL_HANDLE;
    }
    if (g.instance != VK_NULL_HANDLE && pDestroyInstance != NULL) {
        pDestroyInstance(g.instance, NULL);
        g.instance = VK_NULL_HANDLE;
    }
    if (g.host.unload_library != NULL) {
        g.host.unload_library();
    }
}

int VkPresent_Init(const tVulkan_host* host, int window_width, int window_height) {
    memset(&g, 0, sizeof(g));
    g.host = *host;
    g.window_width = window_width;
    g.window_height = window_height;
    g.viewport.scale_x = 1.0f;
    g.viewport.scale_y = 1.0f;

    if (g.host.load_library(NULL) != 0) {
        fprintf(stderr, "Vulkan presenter: could not load a Vulkan loader\n");
        return -1;
    }
    gpa = (PFN_vkGetInstanceProcAddr)g.host.get_vk_get_instance_proc_addr();
    if (gpa == NULL) {
        fprintf(stderr, "Vulkan presenter: no vkGetInstanceProcAddr\n");
        g.host.unload_library();
        memset(&g, 0, sizeof(g));
        return -1;
    }

    if (create_instance() != 0
        || pick_physical_device() != 0
        || create_device() != 0
        || choose_surface_format() != 0
        || create_render_pass() != 0
        || create_descriptors() != 0
        || create_pipeline() != 0
        || create_palette_image() != 0
        || create_sync_objects() != 0
        || create_swapchain(VK_NULL_HANDLE) != 0) {
        destroy_all();
        memset(&g, 0, sizeof(g));
        return -1;
    }

    g.active = 1;
    g.palette_dirty = 1;
    return 0;
}

void VkPresent_Shutdown(void) {
    if (g.device == VK_NULL_HANDLE && g.instance == VK_NULL_HANDLE) {
        return;
    }
    g.active = 0;
    destroy_all();
    memset(&g, 0, sizeof(g));
}

int VkPresent_IsActive(void) {
    return g.active;
}

void VkPresent_SetViewport(const tHarness_viewport* viewport) {
    g.viewport = *viewport;
}

void VkPresent_OnResize(int window_width, int window_height) {
    g.window_width = window_width;
    g.window_height = window_height;
}

void VkPresent_SetPalette(br_colour entries[256]) {
    int i;
    for (i = 0; i < 256; i++) {
        g.palette[i] = (0xffu << 24 | BR_RED(entries[i]) << 16 | BR_GRN(entries[i]) << 8 | BR_BLU(entries[i]));
    }
    g.palette_dirty = 1;
}

void VkPresent_Frame(br_pixelmap* src) {
    if (!g.active || src == NULL) {
        return;
    }
    if (presenter_present(src, 1) != 0) {
        g.active = 0;
    }
}

void VkPresent_Represent(void) {
    if (!g.active || !g.has_frame || g.last_source == NULL) {
        return;
    }
    if (presenter_present(g.last_source, 0) != 0) {
        g.active = 0;
    }
}
