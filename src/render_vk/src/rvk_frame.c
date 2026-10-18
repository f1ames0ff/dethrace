#include "rvk_internal.h"

RvkResult rvk_begin_frame(RvkDevice *device) {
    RvkResult recreate_result;
    VkResult result;
    uint64_t start_ticks;

    if (device == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (device->device_lost) {
        return RVK_ERROR_DEVICE_LOST;
    }
    if (device->frame_open) {
        return RVK_ERROR_ARGUMENT;
    }
    if (device->pending_resize) {
        if (device->pending_width == 0 || device->pending_height == 0) {
            return RVK_ERROR_OUT_OF_DATE;
        }
        device->info.width = device->pending_width;
        device->info.height = device->pending_height;
        recreate_result = rvk_recreate_swapchain(device);
        if (recreate_result != RVK_OK) {
            return recreate_result;
        }
        device->pending_resize = 0;
    }

    start_ticks = rvk_now_ticks();
    device->wait_start_ticks = start_ticks;
    result = device->vk.WaitForFences(device->device, 1, &device->in_flight, VK_TRUE, UINT64_MAX);
    device->stats.cpu_wait_ms = (float)rvk_ticks_to_ms(device, rvk_now_ticks() - device->wait_start_ticks);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkWaitForFences");
    }

    result = device->vk.AcquireNextImageKHR(device->device, device->swapchain, UINT64_MAX, device->image_available, VK_NULL_HANDLE, &device->image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        device->pending_resize = 1;
        device->pending_width = device->extent.width;
        device->pending_height = device->extent.height;
        return RVK_ERROR_OUT_OF_DATE;
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        device->pending_resize = 1;
        device->pending_width = device->extent.width;
        device->pending_height = device->extent.height;
    } else if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkAcquireNextImageKHR");
    }

    result = device->vk.ResetCommandBuffer(device->command_buffer, 0);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkResetCommandBuffer");
    }
    {
        VkCommandBufferBeginInfo begin_info;

        memset(&begin_info, 0, sizeof(begin_info));
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = device->vk.BeginCommandBuffer(device->command_buffer, &begin_info);
        if (result != VK_SUCCESS) {
            return rvk_check(device, result, "vkBeginCommandBuffer");
        }
    }

    device->frame_start_ticks = start_ticks;
    device->frame_open = 1;
    device->pass_open = 0;
    device->stats.pass_count = 0;
    return RVK_OK;
}

RvkResult rvk_begin_pass(RvkDevice *device, const RvkPassInfo *info) {
    VkRenderPassBeginInfo begin_info;
    VkClearValue clear_value;

    if (device == NULL || info == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (info->struct_size != 0 && info->struct_size != sizeof(RvkPassInfo)) {
        return RVK_ERROR_ARGUMENT;
    }
    if (info->pass != RVK_PASS_MAIN) {
        return RVK_ERROR_UNSUPPORTED;
    }
    if (!device->frame_open || device->pass_open) {
        return RVK_ERROR_ARGUMENT;
    }

    memset(&clear_value, 0, sizeof(clear_value));
    clear_value.color.float32[0] = info->clear_color[0];
    clear_value.color.float32[1] = info->clear_color[1];
    clear_value.color.float32[2] = info->clear_color[2];
    clear_value.color.float32[3] = info->clear_color[3];

    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = device->render_pass;
    begin_info.framebuffer = device->framebuffers[device->image_index];
    begin_info.renderArea.offset.x = 0;
    begin_info.renderArea.offset.y = 0;
    begin_info.renderArea.extent = device->extent;
    begin_info.clearValueCount = 1;
    begin_info.pClearValues = &clear_value;

    device->vk.CmdBeginRenderPass(device->command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    device->pass_open = 1;
    device->stats.pass_count++;
    return RVK_OK;
}

void rvk_end_pass(RvkDevice *device) {
    if (device == NULL || !device->pass_open) {
        return;
    }
    device->vk.CmdEndRenderPass(device->command_buffer);
    device->pass_open = 0;
}

RvkResult rvk_end_frame(RvkDevice *device) {
    VkResult result;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info;
    VkPresentInfoKHR present_info;
    uint64_t end_ticks;

    if (device == NULL) {
        return RVK_ERROR_ARGUMENT;
    }
    if (!device->frame_open || device->pass_open) {
        return RVK_ERROR_ARGUMENT;
    }

    result = device->vk.EndCommandBuffer(device->command_buffer);
    if (result != VK_SUCCESS) {
        device->frame_open = 0;
        return rvk_check(device, result, "vkEndCommandBuffer");
    }

    memset(&submit_info, 0, sizeof(submit_info));
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &device->image_available;
    submit_info.pWaitDstStageMask = &wait_stage;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &device->command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &device->render_finished[device->image_index];

    result = device->vk.ResetFences(device->device, 1, &device->in_flight);
    if (result != VK_SUCCESS) {
        device->device_lost = 1;
        device->frame_open = 0;
        return rvk_check(device, result, "vkResetFences");
    }
    result = device->vk.QueueSubmit(device->graphics_queue, 1, &submit_info, device->in_flight);
    if (result != VK_SUCCESS) {
        device->device_lost = 1;
        device->frame_open = 0;
        return rvk_check(device, result, "vkQueueSubmit");
    }

    memset(&present_info, 0, sizeof(present_info));
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &device->render_finished[device->image_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &device->swapchain;
    present_info.pImageIndices = &device->image_index;

    result = device->vk.QueuePresentKHR(device->present_queue, &present_info);
    end_ticks = rvk_now_ticks();
    device->stats.cpu_frame_ms = (float)rvk_ticks_to_ms(device, end_ticks - device->frame_start_ticks);
    device->stats.frame_index++;
    device->frame_open = 0;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        device->pending_resize = 1;
        device->pending_width = device->extent.width;
        device->pending_height = device->extent.height;
        return RVK_ERROR_OUT_OF_DATE;
    }
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "vkQueuePresentKHR");
    }
    return RVK_OK;
}
