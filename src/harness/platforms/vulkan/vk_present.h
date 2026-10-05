#ifndef HARNESS_VK_PRESENT_H
#define HARNESS_VK_PRESENT_H

#include "brender.h"
#include "platforms/platform_viewport.h"
#include "platforms/vulkan/vulkan_host.h"

int VkPresent_Init(const tVulkan_host* host, int window_width, int window_height);
void VkPresent_Shutdown(void);
int VkPresent_IsActive(void);
void VkPresent_Frame(br_pixelmap* src);
void VkPresent_Represent(void);
void VkPresent_SetPalette(br_colour entries[256]);
void VkPresent_OnResize(int window_width, int window_height);
void VkPresent_SetViewport(const tHarness_viewport* viewport);

#endif
