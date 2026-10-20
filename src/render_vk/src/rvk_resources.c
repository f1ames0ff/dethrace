#include "rvk_internal.h"

uint32_t rvk_find_memory_type(RvkDevice *device, uint32_t type_bits, VkMemoryPropertyFlags properties) {
    uint32_t i;

    for (i = 0; i < device->memory_properties.memoryTypeCount; i++) {
        if ((type_bits & (1u << i)) != 0
            && (device->memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

VkResult rvk_create_buffer(RvkDevice *device, VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *memory, void **mapped) {
    VkBufferCreateInfo buffer_info;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_info;
    VkResult result;
    uint32_t memory_type;

    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    result = device->vk.CreateBuffer(device->device, &buffer_info, NULL, buffer);
    if (result != VK_SUCCESS) {
        return result;
    }
    device->vk.GetBufferMemoryRequirements(device->device, *buffer, &requirements);
    memory_type = rvk_find_memory_type(device, requirements.memoryTypeBits, properties);
    if (memory_type == UINT32_MAX) {
        device->vk.DestroyBuffer(device->device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    memset(&memory_info, 0, sizeof(memory_info));
    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.allocationSize = requirements.size;
    memory_info.memoryTypeIndex = memory_type;
    result = device->vk.AllocateMemory(device->device, &memory_info, NULL, memory);
    if (result != VK_SUCCESS) {
        device->vk.DestroyBuffer(device->device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        return result;
    }
    result = device->vk.BindBufferMemory(device->device, *buffer, *memory, 0);
    if (result != VK_SUCCESS) {
        device->vk.FreeMemory(device->device, *memory, NULL);
        device->vk.DestroyBuffer(device->device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        *memory = VK_NULL_HANDLE;
        return result;
    }
    if (mapped != NULL) {
        *mapped = NULL;
        result = device->vk.MapMemory(device->device, *memory, 0, size, 0, mapped);
        if (result != VK_SUCCESS) {
            device->vk.FreeMemory(device->device, *memory, NULL);
            device->vk.DestroyBuffer(device->device, *buffer, NULL);
            *buffer = VK_NULL_HANDLE;
            *memory = VK_NULL_HANDLE;
            return result;
        }
    }
    return VK_SUCCESS;
}

void rvk_destroy_buffer(RvkDevice *device, VkBuffer *buffer, VkDeviceMemory *memory, void **mapped) {
    if (mapped != NULL && *mapped != NULL && device->vk.UnmapMemory != NULL) {
        device->vk.UnmapMemory(device->device, *memory);
        *mapped = NULL;
    }
    if (*buffer != VK_NULL_HANDLE && device->vk.DestroyBuffer != NULL) {
        device->vk.DestroyBuffer(device->device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
    }
    if (*memory != VK_NULL_HANDLE && device->vk.FreeMemory != NULL) {
        device->vk.FreeMemory(device->device, *memory, NULL);
        *memory = VK_NULL_HANDLE;
    }
}

VkResult rvk_create_image(RvkDevice *device, uint32_t width, uint32_t height, VkFormat format,
    VkImageUsageFlags usage, VkImage *image, VkDeviceMemory *memory) {
    VkImageCreateInfo image_info;
    VkMemoryRequirements requirements;
    VkMemoryAllocateInfo memory_info;
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

    result = device->vk.CreateImage(device->device, &image_info, NULL, image);
    if (result != VK_SUCCESS) {
        return result;
    }
    device->vk.GetImageMemoryRequirements(device->device, *image, &requirements);
    memory_type = rvk_find_memory_type(device, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type == UINT32_MAX) {
        device->vk.DestroyImage(device->device, *image, NULL);
        *image = VK_NULL_HANDLE;
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }
    memset(&memory_info, 0, sizeof(memory_info));
    memory_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_info.allocationSize = requirements.size;
    memory_info.memoryTypeIndex = memory_type;
    result = device->vk.AllocateMemory(device->device, &memory_info, NULL, memory);
    if (result != VK_SUCCESS) {
        device->vk.DestroyImage(device->device, *image, NULL);
        *image = VK_NULL_HANDLE;
        return result;
    }
    result = device->vk.BindImageMemory(device->device, *image, *memory, 0);
    if (result != VK_SUCCESS) {
        device->vk.FreeMemory(device->device, *memory, NULL);
        device->vk.DestroyImage(device->device, *image, NULL);
        *image = VK_NULL_HANDLE;
        *memory = VK_NULL_HANDLE;
        return result;
    }
    return VK_SUCCESS;
}

void rvk_transition_image(RvkDevice *device, VkCommandBuffer command_buffer, VkImage image,
    VkImageLayout old_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier barrier;
    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        destination_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    device->vk.CmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

RvkResult rvk_upload_texture(RvkDevice *device, VkImage image, const uint8_t *rgba8, uint32_t width, uint32_t height) {
    VkDeviceSize size = (VkDeviceSize)width * height * 4;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory staging_memory = VK_NULL_HANDLE;
    void *mapped = NULL;
    VkCommandBufferBeginInfo begin_info;
    VkBufferImageCopy copy;
    VkSubmitInfo submit_info;
    VkResult result;

    result = rvk_create_buffer(device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging, &staging_memory, &mapped);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create texture staging buffer");
    }
    memcpy(mapped, rgba8, (size_t)size);
    device->vk.UnmapMemory(device->device, staging_memory);
    mapped = NULL;

    result = device->vk.WaitForFences(device->device, 1, &device->upload_fence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        rvk_destroy_buffer(device, &staging, &staging_memory, &mapped);
        return rvk_check(device, result, "wait texture upload fence");
    }
    result = device->vk.ResetFences(device->device, 1, &device->upload_fence);
    if (result != VK_SUCCESS) {
        rvk_destroy_buffer(device, &staging, &staging_memory, &mapped);
        return rvk_check(device, result, "reset texture upload fence");
    }
    result = device->vk.ResetCommandBuffer(device->upload_command_buffer, 0);
    if (result != VK_SUCCESS) {
        rvk_destroy_buffer(device, &staging, &staging_memory, &mapped);
        return rvk_check(device, result, "reset texture upload command buffer");
    }
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = device->vk.BeginCommandBuffer(device->upload_command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        rvk_destroy_buffer(device, &staging, &staging_memory, &mapped);
        return rvk_check(device, result, "begin texture upload command buffer");
    }

    rvk_transition_image(device, device->upload_command_buffer, image,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    memset(&copy, 0, sizeof(copy));
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent.width = width;
    copy.imageExtent.height = height;
    copy.imageExtent.depth = 1;
    device->vk.CmdCopyBufferToImage(device->upload_command_buffer, staging, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    rvk_transition_image(device, device->upload_command_buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    result = device->vk.EndCommandBuffer(device->upload_command_buffer);
    if (result != VK_SUCCESS) {
        rvk_destroy_buffer(device, &staging, &staging_memory, &mapped);
        return rvk_check(device, result, "end texture upload command buffer");
    }

    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &device->upload_command_buffer;
    result = device->vk.QueueSubmit(device->graphics_queue, 1, &submit_info, device->upload_fence);
    if (result != VK_SUCCESS) {
        rvk_destroy_buffer(device, &staging, &staging_memory, &mapped);
        return rvk_check(device, result, "submit texture upload");
    }
    result = device->vk.WaitForFences(device->device, 1, &device->upload_fence, VK_TRUE, UINT64_MAX);
    rvk_destroy_buffer(device, &staging, &staging_memory, &mapped);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "wait texture upload");
    }
    return RVK_OK;
}

static VkResult rvk_create_sampler(RvkDevice *device) {
    VkSamplerCreateInfo sampler_info;

    memset(&sampler_info, 0, sizeof(sampler_info));
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;
    sampler_info.maxAnisotropy = 1.0f;
    return device->vk.CreateSampler(device->device, &sampler_info, NULL, &device->sampler);
}

static VkResult rvk_create_descriptor_layouts(RvkDevice *device) {
    VkDescriptorSetLayoutBinding binding;
    VkDescriptorSetLayoutCreateInfo layout_info;
    VkResult result;

    memset(&binding, 0, sizeof(binding));
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &binding;
    result = device->vk.CreateDescriptorSetLayout(device->device, &layout_info, NULL, &device->ubo_set_layout);
    if (result != VK_SUCCESS) {
        return result;
    }

    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    result = device->vk.CreateDescriptorSetLayout(device->device, &layout_info, NULL, &device->texture_set_layout);
    if (result != VK_SUCCESS) {
        return result;
    }

    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    result = device->vk.CreateDescriptorSetLayout(device->device, &layout_info, NULL, &device->overlay_set_layout);
    return result;
}

static VkResult rvk_create_descriptor_pool(RvkDevice *device) {
    VkDescriptorPoolSize sizes[3];
    VkDescriptorPoolCreateInfo pool_info;

    memset(sizes, 0, sizeof(sizes));
    sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[0].descriptorCount = 2048;
    sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[1].descriptorCount = 8;
    sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[2].descriptorCount = 8;
    memset(&pool_info, 0, sizeof(pool_info));
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 2064;
    pool_info.poolSizeCount = 3;
    pool_info.pPoolSizes = sizes;
    return device->vk.CreateDescriptorPool(device->device, &pool_info, NULL, &device->descriptor_pool);
}

static RvkResult rvk_create_ubo(RvkDevice *device) {
    VkDescriptorSetAllocateInfo allocate_info;
    VkDescriptorBufferInfo buffer_info;
    VkWriteDescriptorSet write;
    VkResult result;

    result = rvk_create_buffer(device, 64, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &device->ubo_buffer, &device->ubo_memory, &device->ubo_mapped);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create world uniform buffer");
    }
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = device->descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &device->ubo_set_layout;
    result = device->vk.AllocateDescriptorSets(device->device, &allocate_info, &device->ubo_descriptor);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "allocate world uniform descriptor");
    }
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.buffer = device->ubo_buffer;
    buffer_info.offset = 0;
    buffer_info.range = 64;
    memset(&write, 0, sizeof(write));
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = device->ubo_descriptor;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &buffer_info;
    device->vk.UpdateDescriptorSets(device->device, 1, &write, 0, NULL);
    return RVK_OK;
}

RvkResult rvk_create_overlay_buffer(RvkDevice *device, VkDeviceSize size) {
    VkDescriptorSetAllocateInfo allocate_info;
    VkDescriptorBufferInfo buffer_info;
    VkWriteDescriptorSet write;
    VkResult result;

    if (device->overlay_buffer != VK_NULL_HANDLE) {
        rvk_destroy_buffer(device, &device->overlay_buffer, &device->overlay_memory, &device->overlay_mapped);
    }
    result = rvk_create_buffer(device, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &device->overlay_buffer, &device->overlay_memory, &device->overlay_mapped);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create overlay buffer");
    }
    device->overlay_capacity = size;
    if (device->overlay_descriptor == VK_NULL_HANDLE) {
        memset(&allocate_info, 0, sizeof(allocate_info));
        allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate_info.descriptorPool = device->descriptor_pool;
        allocate_info.descriptorSetCount = 1;
        allocate_info.pSetLayouts = &device->overlay_set_layout;
        result = device->vk.AllocateDescriptorSets(device->device, &allocate_info, &device->overlay_descriptor);
        if (result != VK_SUCCESS) {
            return rvk_check(device, result, "allocate overlay descriptor");
        }
    }
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.buffer = device->overlay_buffer;
    buffer_info.offset = 0;
    buffer_info.range = size;
    memset(&write, 0, sizeof(write));
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = device->overlay_descriptor;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &buffer_info;
    device->vk.UpdateDescriptorSets(device->device, 1, &write, 0, NULL);
    return RVK_OK;
}

RvkResult rvk_create_texture(RvkDevice *device, const uint8_t *rgba8, uint32_t width, uint32_t height, RvkTexture *out_texture) {
    RvkTextureEntry *entry;
    VkImageViewCreateInfo view_info;
    VkDescriptorSetAllocateInfo allocate_info;
    VkDescriptorImageInfo image_info;
    VkWriteDescriptorSet write;
    VkResult result;
    uint32_t index;

    if (device == NULL || rgba8 == NULL || width == 0 || height == 0 || out_texture == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (device->texture_count >= device->texture_capacity) {
        uint32_t new_capacity = device->texture_capacity == 0 ? 64 : device->texture_capacity * 2;
        RvkTextureEntry *new_entries = (RvkTextureEntry *)realloc(device->textures, sizeof(RvkTextureEntry) * new_capacity);

        if (new_entries == NULL) {
            return RVK_ERROR_ARGUMENT;
        }
        memset(new_entries + device->texture_capacity, 0,
            sizeof(RvkTextureEntry) * (new_capacity - device->texture_capacity));
        device->textures = new_entries;
        device->texture_capacity = new_capacity;
    }
    index = device->texture_count;
    entry = &device->textures[index];
    memset(entry, 0, sizeof(*entry));
    result = rvk_create_image(device, width, height, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, &entry->image, &entry->memory);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create texture image");
    }
    {
        RvkResult upload_result = rvk_upload_texture(device, entry->image, rgba8, width, height);

        if (upload_result != RVK_OK) {
            device->vk.DestroyImage(device->device, entry->image, NULL);
            device->vk.FreeMemory(device->device, entry->memory, NULL);
            memset(entry, 0, sizeof(*entry));
            return upload_result;
        }
    }
    memset(&view_info, 0, sizeof(view_info));
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = entry->image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    result = device->vk.CreateImageView(device->device, &view_info, NULL, &entry->view);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create texture view");
    }
    memset(&allocate_info, 0, sizeof(allocate_info));
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.descriptorPool = device->descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &device->texture_set_layout;
    result = device->vk.AllocateDescriptorSets(device->device, &allocate_info, &entry->descriptor);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "allocate texture descriptor");
    }
    memset(&image_info, 0, sizeof(image_info));
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = entry->view;
    image_info.sampler = device->sampler;
    memset(&write, 0, sizeof(write));
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = entry->descriptor;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;
    device->vk.UpdateDescriptorSets(device->device, 1, &write, 0, NULL);
    entry->width = width;
    entry->height = height;
    entry->used = 1;
    device->texture_count++;
    *out_texture = index + 1;
    return RVK_OK;
}

void rvk_destroy_texture(RvkDevice *device, RvkTexture texture) {
    RvkTextureEntry *entry;

    if (device == NULL || texture == 0 || texture > device->texture_count) {
        return;
    }
    entry = &device->textures[texture - 1];
    if (!entry->used) {
        return;
    }
    if (entry->descriptor != VK_NULL_HANDLE && device->vk.FreeDescriptorSets != NULL) {
        device->vk.FreeDescriptorSets(device->device, device->descriptor_pool, 1, &entry->descriptor);
        entry->descriptor = VK_NULL_HANDLE;
    }
    if (entry->view != VK_NULL_HANDLE && device->vk.DestroyImageView != NULL) {
        device->vk.DestroyImageView(device->device, entry->view, NULL);
    }
    if (entry->image != VK_NULL_HANDLE && device->vk.DestroyImage != NULL) {
        device->vk.DestroyImage(device->device, entry->image, NULL);
    }
    if (entry->memory != VK_NULL_HANDLE && device->vk.FreeMemory != NULL) {
        device->vk.FreeMemory(device->device, entry->memory, NULL);
    }
    entry->used = 0;
}

RvkResult rvk_create_gpu_resources(RvkDevice *device) {
    VkResult result;
    uint8_t white[4] = { 255, 255, 255, 255 };

    device->vk.GetPhysicalDeviceMemoryProperties(device->physical_device, &device->memory_properties);
    result = rvk_create_sampler(device);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create sampler");
    }
    result = rvk_create_descriptor_layouts(device);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create descriptor layouts");
    }
    result = rvk_create_descriptor_pool(device);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create descriptor pool");
    }
    if (rvk_create_ubo(device) != RVK_OK) {
        return RVK_ERROR_VULKAN;
    }
    if (rvk_create_overlay_buffer(device, 640u * 480u * 4u) != RVK_OK) {
        return RVK_ERROR_VULKAN;
    }
    {
        VkBuffer vertex_buffer = VK_NULL_HANDLE;
        VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
        void *vertex_mapped = NULL;
        VkBuffer index_buffer = VK_NULL_HANDLE;
        VkDeviceMemory index_memory = VK_NULL_HANDLE;
        void *index_mapped = NULL;

        result = rvk_create_buffer(device, 64u * 1024u * 1024u,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &vertex_buffer, &vertex_memory, &vertex_mapped);
        if (result != VK_SUCCESS) {
            return rvk_check(device, result, "create world vertex buffer");
        }
        result = rvk_create_buffer(device, 32u * 1024u * 1024u,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &index_buffer, &index_memory, &index_mapped);
        if (result != VK_SUCCESS) {
            rvk_destroy_buffer(device, &vertex_buffer, &vertex_memory, &vertex_mapped);
            return rvk_check(device, result, "create world index buffer");
        }
        device->world_vertex_buffer = vertex_buffer;
        device->world_vertex_memory = vertex_memory;
        device->world_vertex_mapped = vertex_mapped;
        device->world_vertex_capacity = 64u * 1024u * 1024u;
        device->world_index_buffer = index_buffer;
        device->world_index_memory = index_memory;
        device->world_index_mapped = index_mapped;
        device->world_index_capacity = 32u * 1024u * 1024u;
    }
    {
        RvkResult texture_result = rvk_create_texture(device, white, 1, 1, &device->white_texture);

        if (texture_result != RVK_OK) {
            return texture_result;
        }
    }
    return RVK_OK;
}

void rvk_destroy_gpu_resources(RvkDevice *device) {
    uint32_t i;

    if (device == NULL) {
        return;
    }
    if (device->textures != NULL) {
        for (i = 0; i < device->texture_count; i++) {
            rvk_destroy_texture(device, i + 1);
        }
        free(device->textures);
        device->textures = NULL;
    }
    rvk_destroy_buffer(device, &device->overlay_buffer, &device->overlay_memory, &device->overlay_mapped);
    rvk_destroy_buffer(device, &device->world_index_buffer, &device->world_index_memory, &device->world_index_mapped);
    rvk_destroy_buffer(device, &device->world_vertex_buffer, &device->world_vertex_memory, &device->world_vertex_mapped);
    rvk_destroy_buffer(device, &device->ubo_buffer, &device->ubo_memory, &device->ubo_mapped);
    if (device->sampler != VK_NULL_HANDLE && device->vk.DestroySampler != NULL) {
        device->vk.DestroySampler(device->device, device->sampler, NULL);
    }
    if (device->descriptor_pool != VK_NULL_HANDLE && device->vk.DestroyDescriptorPool != NULL) {
        device->vk.DestroyDescriptorPool(device->device, device->descriptor_pool, NULL);
    }
    if (device->overlay_set_layout != VK_NULL_HANDLE && device->vk.DestroyDescriptorSetLayout != NULL) {
        device->vk.DestroyDescriptorSetLayout(device->device, device->overlay_set_layout, NULL);
    }
    if (device->texture_set_layout != VK_NULL_HANDLE && device->vk.DestroyDescriptorSetLayout != NULL) {
        device->vk.DestroyDescriptorSetLayout(device->device, device->texture_set_layout, NULL);
    }
    if (device->ubo_set_layout != VK_NULL_HANDLE && device->vk.DestroyDescriptorSetLayout != NULL) {
        device->vk.DestroyDescriptorSetLayout(device->device, device->ubo_set_layout, NULL);
    }
}
