#include "rvk_internal.h"

typedef struct RvkWorldPush {
    float model[16];
    float tint[4];
} RvkWorldPush;

static VkDescriptorSet rvk_texture_descriptor(RvkDevice *device, RvkTexture texture) {
    if (texture != 0 && texture <= device->texture_count) {
        RvkTextureEntry *entry = &device->textures[texture - 1];

        if (entry->used && entry->descriptor != VK_NULL_HANDLE) {
            return entry->descriptor;
        }
    }
    if (device->white_texture != 0 && device->white_texture <= device->texture_count) {
        return device->textures[device->white_texture - 1].descriptor;
    }
    return VK_NULL_HANDLE;
}

RvkResult rvk_draw_world(RvkDevice *device, const float view_proj[16],
    const RvkVertex *vertices, uint32_t vertex_count,
    const uint32_t *indices, uint32_t index_count,
    const RvkWorldDraw *draws, uint32_t draw_count,
    const float clear_color[4]) {
    VkDeviceSize vertex_bytes;
    VkDeviceSize index_bytes;
    VkRenderPassBeginInfo begin_info;
    VkClearValue clear_values[2];
    VkViewport viewport;
    VkRect2D scissor;
    VkDeviceSize vertex_offset = 0;
    uint32_t i;

    if (device == NULL || view_proj == NULL || vertices == NULL || indices == NULL || draws == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (!device->frame_open || device->pass_open) {
        return RVK_ERROR_ARGUMENT;
    }
    if (device->world_rendered) {
        return RVK_ERROR_ARGUMENT;
    }
    vertex_bytes = (VkDeviceSize)vertex_count * sizeof(RvkVertex);
    index_bytes = (VkDeviceSize)index_count * sizeof(uint32_t);
    if (vertex_bytes > device->world_vertex_capacity || index_bytes > device->world_index_capacity) {
        return RVK_ERROR_UNSUPPORTED;
    }
    memcpy(device->world_vertex_mapped, vertices, (size_t)vertex_bytes);
    memcpy(device->world_index_mapped, indices, (size_t)index_bytes);
    memcpy(device->ubo_mapped, view_proj, 16 * sizeof(float));

    memset(clear_values, 0, sizeof(clear_values));
    clear_values[0].color.float32[0] = clear_color[0];
    clear_values[0].color.float32[1] = clear_color[1];
    clear_values[0].color.float32[2] = clear_color[2];
    clear_values[0].color.float32[3] = clear_color[3];
    clear_values[1].depthStencil.depth = 1.0f;
    clear_values[1].depthStencil.stencil = 0;

    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = device->world_render_pass;
    begin_info.framebuffer = device->world_framebuffers[device->image_index];
    begin_info.renderArea.offset.x = 0;
    begin_info.renderArea.offset.y = 0;
    begin_info.renderArea.extent = device->extent;
    begin_info.clearValueCount = 2;
    begin_info.pClearValues = clear_values;
    device->vk.CmdBeginRenderPass(device->command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)device->extent.width;
    viewport.height = (float)device->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = device->extent;
    device->vk.CmdSetViewport(device->command_buffer, 0, 1, &viewport);
    device->vk.CmdSetScissor(device->command_buffer, 0, 1, &scissor);
    device->vk.CmdBindPipeline(device->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, device->world_pipeline);
    device->vk.CmdBindDescriptorSets(device->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        device->world_pipeline_layout, 0, 1, &device->ubo_descriptor, 0, NULL);
    device->vk.CmdBindVertexBuffers(device->command_buffer, 0, 1, &device->world_vertex_buffer, &vertex_offset);
    device->vk.CmdBindIndexBuffer(device->command_buffer, device->world_index_buffer, 0, VK_INDEX_TYPE_UINT32);

    for (i = 0; i < draw_count; i++) {
        const RvkWorldDraw *draw = &draws[i];
        RvkWorldPush push;
        VkDescriptorSet texture = rvk_texture_descriptor(device, draw->texture);

        if (texture == VK_NULL_HANDLE) {
            continue;
        }
        if (draw->first_index + draw->index_count > index_count) {
            continue;
        }
        memcpy(push.model, draw->model, sizeof(push.model));
        memcpy(push.tint, draw->tint, sizeof(push.tint));
        device->vk.CmdBindDescriptorSets(device->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            device->world_pipeline_layout, 1, 1, &texture, 0, NULL);
        device->vk.CmdPushConstants(device->command_buffer, device->world_pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
        device->vk.CmdDrawIndexed(device->command_buffer, draw->index_count, 1, draw->first_index, 0, 0);
    }
    device->vk.CmdEndRenderPass(device->command_buffer);
    device->world_rendered = 1;
    return RVK_OK;
}

RvkResult rvk_draw_overlay(RvkDevice *device, const uint32_t *pixels, uint32_t width, uint32_t height) {
    VkDeviceSize size;
    VkRenderPassBeginInfo begin_info;
    VkClearValue clear_value;
    VkViewport viewport;
    VkRect2D scissor;
    uint32_t size_values[2];

    if (device == NULL || pixels == NULL || width == 0 || height == 0) {
        return RVK_ERROR_ARGUMENT;
    }
    if (!device->frame_open || device->pass_open) {
        return RVK_ERROR_ARGUMENT;
    }
    size = (VkDeviceSize)width * height * 4;
    if (size > device->overlay_capacity) {
        if (rvk_create_overlay_buffer(device, size) != RVK_OK) {
            return RVK_ERROR_VULKAN;
        }
    }
    memcpy(device->overlay_mapped, pixels, (size_t)size);

    memset(&clear_value, 0, sizeof(clear_value));
    clear_value.color.float32[0] = 0.35f;
    clear_value.color.float32[1] = 0.55f;
    clear_value.color.float32[2] = 0.85f;
    clear_value.color.float32[3] = 1.0f;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = device->world_rendered ? device->overlay_load_render_pass : device->overlay_clear_render_pass;
    begin_info.framebuffer = device->world_rendered
        ? device->overlay_framebuffers[device->image_index]
        : device->overlay_clear_framebuffers[device->image_index];
    begin_info.renderArea.offset.x = 0;
    begin_info.renderArea.offset.y = 0;
    begin_info.renderArea.extent = device->extent;
    begin_info.clearValueCount = device->world_rendered ? 0 : 1;
    begin_info.pClearValues = &clear_value;
    device->vk.CmdBeginRenderPass(device->command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)device->extent.width;
    viewport.height = (float)device->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = device->extent;
    device->vk.CmdSetViewport(device->command_buffer, 0, 1, &viewport);
    device->vk.CmdSetScissor(device->command_buffer, 0, 1, &scissor);
    device->vk.CmdBindPipeline(device->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        device->world_rendered ? device->overlay_load_pipeline : device->overlay_clear_pipeline);
    device->vk.CmdBindDescriptorSets(device->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        device->overlay_pipeline_layout, 0, 1, &device->overlay_descriptor, 0, NULL);
    size_values[0] = width;
    size_values[1] = height;
    device->vk.CmdPushConstants(device->command_buffer, device->overlay_pipeline_layout,
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(size_values), size_values);
    device->vk.CmdDraw(device->command_buffer, 3, 1, 0, 0);
    device->vk.CmdEndRenderPass(device->command_buffer);
    return RVK_OK;
}
