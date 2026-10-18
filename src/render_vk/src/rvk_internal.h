#ifndef RVK_INTERNAL_H
#define RVK_INTERNAL_H

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rvk.h"

#define RVK_MAX_INSTANCE_EXTENSIONS 16
#define RVK_MAX_DEVICE_EXTENSIONS 8
#define RVK_MAX_QUEUE_FAMILIES 8

typedef struct RvkDispatch {
    PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion;
    PFN_vkEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
    PFN_vkEnumerateInstanceLayerProperties EnumerateInstanceLayerProperties;
    PFN_vkCreateInstance CreateInstance;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR GetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR GetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR GetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkDestroySurfaceKHR DestroySurfaceKHR;
    PFN_vkCreateDevice CreateDevice;
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkCreateSwapchainKHR CreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR DestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR AcquireNextImageKHR;
    PFN_vkQueuePresentKHR QueuePresentKHR;
    PFN_vkCreateRenderPass CreateRenderPass;
    PFN_vkDestroyRenderPass DestroyRenderPass;
    PFN_vkCreateImageView CreateImageView;
    PFN_vkDestroyImageView DestroyImageView;
    PFN_vkCreateFramebuffer CreateFramebuffer;
    PFN_vkDestroyFramebuffer DestroyFramebuffer;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkResetCommandBuffer ResetCommandBuffer;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass;
    PFN_vkCmdEndRenderPass CmdEndRenderPass;
    PFN_vkCreateSemaphore CreateSemaphore;
    PFN_vkDestroySemaphore DestroySemaphore;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkResetFences ResetFences;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkQueueWaitIdle QueueWaitIdle;
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkCreateImage CreateImage;
    PFN_vkDestroyImage DestroyImage;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
    PFN_vkBindImageMemory BindImageMemory;
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkCreateSampler CreateSampler;
    PFN_vkDestroySampler DestroySampler;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout;
    PFN_vkCreateDescriptorPool CreateDescriptorPool;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets;
    PFN_vkFreeDescriptorSets FreeDescriptorSets;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets;
    PFN_vkCreatePipelineLayout CreatePipelineLayout;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout;
    PFN_vkCreateShaderModule CreateShaderModule;
    PFN_vkDestroyShaderModule DestroyShaderModule;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines;
    PFN_vkDestroyPipeline DestroyPipeline;
    PFN_vkCmdBindPipeline CmdBindPipeline;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer;
    PFN_vkCmdPushConstants CmdPushConstants;
    PFN_vkCmdDraw CmdDraw;
    PFN_vkCmdDrawIndexed CmdDrawIndexed;
    PFN_vkCmdSetViewport CmdSetViewport;
    PFN_vkCmdSetScissor CmdSetScissor;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
} RvkDispatch;

typedef struct RvkTextureEntry {
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    VkDescriptorSet descriptor;
    uint32_t width;
    uint32_t height;
    int used;
} RvkTextureEntry;

struct RvkDevice {
    RvkInitInfo info;
    RvkDispatch vk;
    PFN_vkGetInstanceProcAddr get_instance_proc_addr;
    VkInstance instance;
    VkDebugUtilsMessengerEXT messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    uint32_t graphics_family;
    uint32_t present_family;
    VkQueue graphics_queue;
    VkQueue present_queue;
    VkSwapchainKHR swapchain;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkExtent2D extent;
    uint32_t image_count;
    VkImage *images;
    VkImageView *views;
    VkFramebuffer *framebuffers;
    VkRenderPass render_pass;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkCommandBuffer upload_command_buffer;
    VkFence upload_fence;
    VkSemaphore image_available;
    VkSemaphore *render_finished;
    VkFence in_flight;
    uint32_t image_index;
    int frame_open;
    int pass_open;
    int pending_resize;
    uint32_t pending_width;
    uint32_t pending_height;
    int device_lost;
    int validation_enabled;
    int debug_utils_enabled;
    double timer_frequency;
    uint64_t frame_start_ticks;
    uint64_t wait_start_ticks;
    RvkStats stats;
    char adapter_name[256];
    VkPhysicalDeviceMemoryProperties memory_properties;
    VkSampler sampler;
    VkDescriptorSetLayout ubo_set_layout;
    VkDescriptorSetLayout texture_set_layout;
    VkDescriptorSetLayout overlay_set_layout;
    VkDescriptorPool descriptor_pool;
    VkBuffer ubo_buffer;
    VkDeviceMemory ubo_memory;
    void *ubo_mapped;
    VkDescriptorSet ubo_descriptor;
    VkPipelineLayout world_pipeline_layout;
    VkPipelineLayout overlay_pipeline_layout;
    VkPipeline world_pipeline;
    VkPipeline world_sky_pipeline;
    VkPipeline overlay_load_pipeline;
    VkPipeline overlay_clear_pipeline;
    VkRenderPass world_render_pass;
    VkRenderPass overlay_load_render_pass;
    VkRenderPass overlay_clear_render_pass;
    VkFramebuffer *world_framebuffers;
    VkFramebuffer *overlay_framebuffers;
    VkFramebuffer *overlay_clear_framebuffers;
    VkFormat depth_format;
    VkImage depth_image;
    VkDeviceMemory depth_memory;
    VkImageView depth_view;
    VkBuffer world_vertex_buffer;
    VkDeviceMemory world_vertex_memory;
    void *world_vertex_mapped;
    VkDeviceSize world_vertex_capacity;
    VkBuffer world_index_buffer;
    VkDeviceMemory world_index_memory;
    void *world_index_mapped;
    VkDeviceSize world_index_capacity;
    VkBuffer overlay_buffer;
    VkDeviceMemory overlay_memory;
    void *overlay_mapped;
    VkDeviceSize overlay_capacity;
    VkDescriptorSet overlay_descriptor;
    RvkTextureEntry *textures;
    uint32_t texture_count;
    uint32_t texture_capacity;
    RvkTexture white_texture;
    int world_rendered;
};

void rvk_log(RvkDevice *device, int level, const char *format, ...);
uint64_t rvk_now_ticks(void);
double rvk_timer_frequency(void);
double rvk_ticks_to_ms(const RvkDevice *device, uint64_t ticks);
RvkResult rvk_check(RvkDevice *device, VkResult result, const char *operation);
RvkResult rvk_create_swapchain(RvkDevice *device);
void rvk_destroy_swapchain(RvkDevice *device);
RvkResult rvk_create_render_pass(RvkDevice *device);
void rvk_destroy_render_pass(RvkDevice *device);
RvkResult rvk_create_framebuffers(RvkDevice *device);
void rvk_destroy_framebuffers(RvkDevice *device);
RvkResult rvk_recreate_swapchain(RvkDevice *device);
RvkResult rvk_create_depth_resources(RvkDevice *device);
void rvk_destroy_depth_resources(RvkDevice *device);
RvkResult rvk_create_render_passes(RvkDevice *device);
void rvk_destroy_render_passes(RvkDevice *device);
RvkResult rvk_create_gpu_resources(RvkDevice *device);
void rvk_destroy_gpu_resources(RvkDevice *device);
RvkResult rvk_create_pipelines(RvkDevice *device);
void rvk_destroy_pipelines(RvkDevice *device);
uint32_t rvk_find_memory_type(RvkDevice *device, uint32_t type_bits, VkMemoryPropertyFlags properties);
VkResult rvk_create_buffer(RvkDevice *device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *memory, void **mapped);
void rvk_destroy_buffer(RvkDevice *device, VkBuffer *buffer, VkDeviceMemory *memory, void **mapped);
VkResult rvk_create_image(RvkDevice *device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage *image, VkDeviceMemory *memory);
void rvk_transition_image(RvkDevice *device, VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout);
RvkResult rvk_upload_texture(RvkDevice *device, VkImage image, const uint8_t *rgba8, uint32_t width, uint32_t height);
RvkResult rvk_create_overlay_buffer(RvkDevice *device, VkDeviceSize size);

#endif
