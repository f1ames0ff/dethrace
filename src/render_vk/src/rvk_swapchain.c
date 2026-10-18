#include "rvk_internal.h"

static int rvk_format_is_usable(RvkDevice *device, VkFormat format) {
    VkFormatProperties properties;

    memset(&properties, 0, sizeof(properties));
    device->vk.GetPhysicalDeviceFormatProperties(device->physical_device, format, &properties);
    return (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
}

static VkSurfaceFormatKHR rvk_choose_surface_format(RvkDevice *device, const VkSurfaceFormatKHR *formats, uint32_t count) {
    static const VkFormat preferred[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB
    };
    uint32_t i;
    uint32_t p;

    for (p = 0; p < sizeof(preferred) / sizeof(preferred[0]); p++) {
        for (i = 0; i < count; i++) {
            if (formats[i].format == preferred[p] && rvk_format_is_usable(device, formats[i].format)) {
                return formats[i];
            }
        }
    }
    for (i = 0; i < count; i++) {
        if (rvk_format_is_usable(device, formats[i].format)) {
            return formats[i];
        }
    }
    return formats[0];
}

static int rvk_present_mode_available(const VkPresentModeKHR *modes, uint32_t count, VkPresentModeKHR mode) {
    uint32_t i;

    for (i = 0; i < count; i++) {
        if (modes[i] == mode) {
            return 1;
        }
    }
    return 0;
}

static VkCompositeAlphaFlagBitsKHR rvk_choose_composite_alpha(VkCompositeAlphaFlagsKHR supported) {
    if ((supported & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0) {
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    if ((supported & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) != 0) {
        return VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }
    if ((supported & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) != 0) {
        return VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }
    return VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
}

RvkResult rvk_create_swapchain(RvkDevice *device) {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR *formats = NULL;
    VkPresentModeKHR *modes = NULL;
    VkSurfaceFormatKHR surface_format;
    VkPresentModeKHR present_mode;
    VkSwapchainCreateInfoKHR create_info;
    VkResult result;
    uint32_t format_count = 0;
    uint32_t mode_count = 0;
    uint32_t image_count;
    VkExtent2D extent;

    result = device->vk.GetPhysicalDeviceSurfaceCapabilitiesKHR(device->physical_device, device->surface, &capabilities);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    }
    if (device->vk.GetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, device->surface, &format_count, NULL) != VK_SUCCESS || format_count == 0) {
        rvk_log(device, 2, "surface has no formats");
        return RVK_ERROR_UNSUPPORTED;
    }
    formats = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    if (formats == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (device->vk.GetPhysicalDeviceSurfaceFormatsKHR(device->physical_device, device->surface, &format_count, formats) != VK_SUCCESS) {
        free(formats);
        return RVK_ERROR_VULKAN;
    }
    if (device->vk.GetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, device->surface, &mode_count, NULL) != VK_SUCCESS || mode_count == 0) {
        free(formats);
        rvk_log(device, 2, "surface has no present modes");
        return RVK_ERROR_UNSUPPORTED;
    }
    modes = (VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR) * mode_count);
    if (modes == NULL) {
        free(formats);
        return RVK_ERROR_ARGUMENT;
    }
    if (device->vk.GetPhysicalDeviceSurfacePresentModesKHR(device->physical_device, device->surface, &mode_count, modes) != VK_SUCCESS) {
        free(formats);
        free(modes);
        return RVK_ERROR_VULKAN;
    }

    surface_format = rvk_choose_surface_format(device, formats, format_count);
    free(formats);

    if (device->info.present_mode == RVK_PRESENT_IMMEDIATE) {
        if (!rvk_present_mode_available(modes, mode_count, VK_PRESENT_MODE_IMMEDIATE_KHR)) {
            free(modes);
            rvk_log(device, 2, "VK_PRESENT_MODE_IMMEDIATE_KHR is not available");
            return RVK_ERROR_UNSUPPORTED;
        }
        present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    } else {
        present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }
    free(modes);

    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = device->info.width;
        extent.height = device->info.height;
        if (extent.width < capabilities.minImageExtent.width) {
            extent.width = capabilities.minImageExtent.width;
        }
        if (extent.width > capabilities.maxImageExtent.width) {
            extent.width = capabilities.maxImageExtent.width;
        }
        if (extent.height < capabilities.minImageExtent.height) {
            extent.height = capabilities.minImageExtent.height;
        }
        if (extent.height > capabilities.maxImageExtent.height) {
            extent.height = capabilities.maxImageExtent.height;
        }
    }
    if (extent.width == 0 || extent.height == 0) {
        rvk_log(device, 2, "surface extent is empty");
        return RVK_ERROR_OUT_OF_DATE;
    }

    image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && image_count > capabilities.maxImageCount) {
        image_count = capabilities.maxImageCount;
    }

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = device->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = rvk_choose_composite_alpha(capabilities.supportedCompositeAlpha);
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (device->graphics_family != device->present_family) {
        uint32_t families[2];

        families[0] = device->graphics_family;
        families[1] = device->present_family;
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = families;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    result = device->vk.CreateSwapchainKHR(device->device, &create_info, NULL, &device->swapchain);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkCreateSwapchainKHR");
    }

    result = device->vk.GetSwapchainImagesKHR(device->device, device->swapchain, &image_count, NULL);
    if (result != VK_SUCCESS || image_count == 0) {
        return rvk_check(device, result, "vkGetSwapchainImagesKHR(count)");
    }
    device->images = (VkImage *)calloc(image_count, sizeof(VkImage));
    if (device->images == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    result = device->vk.GetSwapchainImagesKHR(device->device, device->swapchain, &image_count, device->images);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkGetSwapchainImagesKHR(images)");
    }

    device->format = surface_format.format;
    device->color_space = surface_format.colorSpace;
    device->extent = extent;
    device->image_count = image_count;
    device->render_finished = (VkSemaphore *)calloc(image_count, sizeof(VkSemaphore));
    if (device->render_finished == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    {
        VkSemaphoreCreateInfo semaphore_info;
        uint32_t image;

        memset(&semaphore_info, 0, sizeof(semaphore_info));
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (image = 0; image < image_count; image++) {
            result = device->vk.CreateSemaphore(device->device, &semaphore_info, NULL, &device->render_finished[image]);
            if (result != VK_SUCCESS) {
                return rvk_check(device, result, "vkCreateSemaphore(render_finished)");
            }
        }
    }
    device->stats.width = extent.width;
    device->stats.height = extent.height;
    device->stats.swapchain_images = image_count;
    device->stats.present_mode = (uint32_t)present_mode;
    rvk_log(device, 0, "swapchain: %ux%u, %u images, format %d, present mode %d", extent.width, extent.height, image_count, (int)surface_format.format, (int)present_mode);
    return RVK_OK;
}

void rvk_destroy_swapchain(RvkDevice *device) {
    if (device == NULL) {
        return;
    }
    if (device->swapchain != VK_NULL_HANDLE && device->vk.DestroySwapchainKHR != NULL) {
        device->vk.DestroySwapchainKHR(device->device, device->swapchain, NULL);
    }
    device->swapchain = VK_NULL_HANDLE;
    if (device->render_finished != NULL && device->vk.DestroySemaphore != NULL) {
        uint32_t image;

        for (image = 0; image < device->image_count; image++) {
            if (device->render_finished[image] != VK_NULL_HANDLE) {
                device->vk.DestroySemaphore(device->device, device->render_finished[image], NULL);
            }
        }
    }
    free(device->render_finished);
    device->render_finished = NULL;
    free(device->images);
    device->images = NULL;
    device->image_count = 0;
}

RvkResult rvk_create_render_pass(RvkDevice *device) {
    VkAttachmentDescription color_attachment;
    VkAttachmentReference color_reference;
    VkSubpassDescription subpass;
    VkSubpassDependency dependency;
    VkRenderPassCreateInfo create_info;
    VkResult result;

    memset(&color_attachment, 0, sizeof(color_attachment));
    color_attachment.format = device->format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    memset(&color_reference, 0, sizeof(color_reference));
    color_reference.attachment = 0;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_reference;

    memset(&dependency, 0, sizeof(dependency));
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    result = device->vk.CreateRenderPass(device->device, &create_info, NULL, &device->render_pass);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkCreateRenderPass");
    }
    return RVK_OK;
}

void rvk_destroy_render_pass(RvkDevice *device) {
    if (device == NULL) {
        return;
    }
    if (device->render_pass != VK_NULL_HANDLE && device->vk.DestroyRenderPass != NULL) {
        device->vk.DestroyRenderPass(device->device, device->render_pass, NULL);
    }
    device->render_pass = VK_NULL_HANDLE;
}

RvkResult rvk_create_framebuffers(RvkDevice *device) {
    uint32_t i;
    VkResult result;

    device->views = (VkImageView *)calloc(device->image_count, sizeof(VkImageView));
    device->framebuffers = (VkFramebuffer *)calloc(device->image_count, sizeof(VkFramebuffer));
    if (device->views == NULL || device->framebuffers == NULL) {
        return RVK_ERROR_ARGUMENT;
    }

    for (i = 0; i < device->image_count; i++) {
        VkImageViewCreateInfo view_info;
        VkFramebufferCreateInfo framebuffer_info;

        memset(&view_info, 0, sizeof(view_info));
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = device->images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = device->format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        result = device->vk.CreateImageView(device->device, &view_info, NULL, &device->views[i]);
        if (result != VK_SUCCESS) {
            return rvk_check(device, result, "vkCreateImageView");
        }

        memset(&framebuffer_info, 0, sizeof(framebuffer_info));
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = device->render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = &device->views[i];
        framebuffer_info.width = device->extent.width;
        framebuffer_info.height = device->extent.height;
        framebuffer_info.layers = 1;
        result = device->vk.CreateFramebuffer(device->device, &framebuffer_info, NULL, &device->framebuffers[i]);
        if (result != VK_SUCCESS) {
            return rvk_check(device, result, "vkCreateFramebuffer");
        }
    }
    return RVK_OK;
}

void rvk_destroy_framebuffers(RvkDevice *device) {
    uint32_t i;

    if (device == NULL) {
        return;
    }
    if (device->framebuffers != NULL) {
        for (i = 0; i < device->image_count; i++) {
            if (device->framebuffers[i] != VK_NULL_HANDLE && device->vk.DestroyFramebuffer != NULL) {
                device->vk.DestroyFramebuffer(device->device, device->framebuffers[i], NULL);
            }
        }
    }
    if (device->views != NULL) {
        for (i = 0; i < device->image_count; i++) {
            if (device->views[i] != VK_NULL_HANDLE && device->vk.DestroyImageView != NULL) {
                device->vk.DestroyImageView(device->device, device->views[i], NULL);
            }
        }
    }
    free(device->framebuffers);
    free(device->views);
    device->framebuffers = NULL;
    device->views = NULL;
}

RvkResult rvk_recreate_swapchain(RvkDevice *device) {
    RvkResult result;

    if (device == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (device->device != VK_NULL_HANDLE && device->vk.DeviceWaitIdle != NULL) {
        device->vk.DeviceWaitIdle(device->device);
    }
    rvk_destroy_framebuffers(device);
    rvk_destroy_render_pass(device);
    rvk_destroy_swapchain(device);

    result = rvk_create_swapchain(device);
    if (result != RVK_OK) {
        return result;
    }
    result = rvk_create_render_pass(device);
    if (result != RVK_OK) {
        return result;
    }
    result = rvk_create_framebuffers(device);
    if (result != RVK_OK) {
        return result;
    }
    return RVK_OK;
}
