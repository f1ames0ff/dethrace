#include "rvk_internal.h"
#include "rvk_shaders.h"

static VkResult rvk_create_shader_module(RvkDevice *device, const uint32_t *code, size_t code_size, VkShaderModule *module) {
    VkShaderModuleCreateInfo create_info;

    memset(&create_info, 0, sizeof(create_info));
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code_size;
    create_info.pCode = code;
    return device->vk.CreateShaderModule(device->device, &create_info, NULL, module);
}

static RvkResult rvk_create_world_pipeline(RvkDevice *device, VkBool32 depth_test, VkBool32 depth_write, VkPipeline *out_pipeline) {
    VkShaderModule vertex_module = VK_NULL_HANDLE;
    VkShaderModule fragment_module = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2];
    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attributes[3];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color_blend;
    VkDynamicState dynamic_states[2];
    VkPipelineDynamicStateCreateInfo dynamic_state;
    VkPushConstantRange push_range;
    VkDescriptorSetLayout set_layouts[2];
    VkPipelineLayoutCreateInfo layout_info;
    VkGraphicsPipelineCreateInfo pipeline_info;
    VkResult result;

    result = rvk_create_shader_module(device, rvk_world_vert_spv, sizeof(rvk_world_vert_spv), &vertex_module);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create world vertex shader");
    }
    result = rvk_create_shader_module(device, rvk_world_frag_spv, sizeof(rvk_world_frag_spv), &fragment_module);
    if (result != VK_SUCCESS) {
        device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
        return rvk_check(device, result, "create world fragment shader");
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

    memset(&binding, 0, sizeof(binding));
    binding.binding = 0;
    binding.stride = sizeof(RvkVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    memset(attributes, 0, sizeof(attributes));
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(RvkVertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(RvkVertex, normal);
    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = offsetof(RvkVertex, uv);
    memset(&vertex_input, 0, sizeof(vertex_input));
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attributes;

    memset(&input_assembly, 0, sizeof(input_assembly));
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

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

    memset(&depth_stencil, 0, sizeof(depth_stencil));
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = depth_test;
    depth_stencil.depthWriteEnable = depth_write;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    memset(&color_attachment, 0, sizeof(color_attachment));
    color_attachment.blendEnable = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    memset(&color_blend, 0, sizeof(color_blend));
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_attachment;

    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    memset(&dynamic_state, 0, sizeof(dynamic_state));
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    set_layouts[0] = device->ubo_set_layout;
    set_layouts[1] = device->texture_set_layout;
    memset(&push_range, 0, sizeof(push_range));
    push_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(float) * 25;
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    result = device->vk.CreatePipelineLayout(device->device, &layout_info, NULL, &device->world_pipeline_layout);
    if (result != VK_SUCCESS) {
        device->vk.DestroyShaderModule(device->device, fragment_module, NULL);
        device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
        return rvk_check(device, result, "create world pipeline layout");
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
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = device->world_pipeline_layout;
    pipeline_info.renderPass = device->world_render_pass;
    pipeline_info.subpass = 0;
    result = device->vk.CreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, out_pipeline);
    device->vk.DestroyShaderModule(device->device, fragment_module, NULL);
    device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create world pipeline");
    }
    return RVK_OK;
}

static RvkResult rvk_create_overlay_pipeline(RvkDevice *device, VkRenderPass render_pass, VkPipeline *out_pipeline) {
    VkShaderModule vertex_module = VK_NULL_HANDLE;
    VkShaderModule fragment_module = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color_blend;
    VkDynamicState dynamic_states[2];
    VkPipelineDynamicStateCreateInfo dynamic_state;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkGraphicsPipelineCreateInfo pipeline_info;
    VkResult result;

    result = rvk_create_shader_module(device, rvk_overlay_vert_spv, sizeof(rvk_overlay_vert_spv), &vertex_module);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create overlay vertex shader");
    }
    result = rvk_create_shader_module(device, rvk_overlay_frag_spv, sizeof(rvk_overlay_frag_spv), &fragment_module);
    if (result != VK_SUCCESS) {
        device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
        return rvk_check(device, result, "create overlay fragment shader");
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

    memset(&color_attachment, 0, sizeof(color_attachment));
    color_attachment.blendEnable = VK_TRUE;
    color_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    memset(&color_blend, 0, sizeof(color_blend));
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_attachment;

    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    memset(&dynamic_state, 0, sizeof(dynamic_state));
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    if (device->overlay_pipeline_layout == VK_NULL_HANDLE) {
        memset(&push_range, 0, sizeof(push_range));
        push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        push_range.offset = 0;
        push_range.size = sizeof(uint32_t) * 2;
        memset(&layout_info, 0, sizeof(layout_info));
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &device->overlay_set_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push_range;
        result = device->vk.CreatePipelineLayout(device->device, &layout_info, NULL, &device->overlay_pipeline_layout);
        if (result != VK_SUCCESS) {
            device->vk.DestroyShaderModule(device->device, fragment_module, NULL);
            device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
            return rvk_check(device, result, "create overlay pipeline layout");
        }
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
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = device->overlay_pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    result = device->vk.CreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, out_pipeline);
    device->vk.DestroyShaderModule(device->device, fragment_module, NULL);
    device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create overlay pipeline");
    }
    return RVK_OK;
}

static RvkResult rvk_create_background_pipeline(RvkDevice *device) {
    VkShaderModule vertex_module = VK_NULL_HANDLE;
    VkShaderModule fragment_module = VK_NULL_HANDLE;
    VkPipelineShaderStageCreateInfo stages[2];
    VkPipelineVertexInputStateCreateInfo vertex_input;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineViewportStateCreateInfo viewport_state;
    VkPipelineRasterizationStateCreateInfo rasterization;
    VkPipelineMultisampleStateCreateInfo multisample;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineColorBlendAttachmentState color_attachment;
    VkPipelineColorBlendStateCreateInfo color_blend;
    VkDynamicState dynamic_states[2];
    VkPipelineDynamicStateCreateInfo dynamic_state;
    VkPushConstantRange push_range;
    VkPipelineLayoutCreateInfo layout_info;
    VkGraphicsPipelineCreateInfo pipeline_info;
    VkResult result;

    result = rvk_create_shader_module(device, rvk_background_vert_spv, sizeof(rvk_background_vert_spv), &vertex_module);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create background vertex shader");
    }
    result = rvk_create_shader_module(device, rvk_background_frag_spv, sizeof(rvk_background_frag_spv), &fragment_module);
    if (result != VK_SUCCESS) {
        device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
        return rvk_check(device, result, "create background fragment shader");
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
    memset(&depth_stencil, 0, sizeof(depth_stencil));
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_FALSE;
    depth_stencil.depthWriteEnable = VK_FALSE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    memset(&color_attachment, 0, sizeof(color_attachment));
    color_attachment.blendEnable = VK_FALSE;
    color_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    memset(&color_blend, 0, sizeof(color_blend));
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.attachmentCount = 1;
    color_blend.pAttachments = &color_attachment;

    dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
    memset(&dynamic_state, 0, sizeof(dynamic_state));
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    memset(&push_range, 0, sizeof(push_range));
    push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_range.offset = 0;
    push_range.size = sizeof(float) * 5;
    memset(&layout_info, 0, sizeof(layout_info));
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &device->texture_set_layout;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_range;
    result = device->vk.CreatePipelineLayout(device->device, &layout_info, NULL, &device->background_pipeline_layout);
    if (result != VK_SUCCESS) {
        device->vk.DestroyShaderModule(device->device, fragment_module, NULL);
        device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
        return rvk_check(device, result, "create background pipeline layout");
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
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = device->background_pipeline_layout;
    pipeline_info.renderPass = device->world_render_pass;
    pipeline_info.subpass = 0;
    result = device->vk.CreateGraphicsPipelines(device->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &device->background_pipeline);
    device->vk.DestroyShaderModule(device->device, fragment_module, NULL);
    device->vk.DestroyShaderModule(device->device, vertex_module, NULL);
    if (result != VK_SUCCESS) {
        return rvk_check(device, result, "create background pipeline");
    }
    return RVK_OK;
}

RvkResult rvk_create_pipelines(RvkDevice *device) {
    RvkResult result;

    result = rvk_create_world_pipeline(device, VK_TRUE, VK_TRUE, &device->world_pipeline);
    if (result != RVK_OK) {
        return result;
    }
    result = rvk_create_world_pipeline(device, VK_TRUE, VK_FALSE, &device->world_blend_pipeline);
    if (result != RVK_OK) {
        return result;
    }
    result = rvk_create_world_pipeline(device, VK_FALSE, VK_FALSE, &device->world_sky_pipeline);
    if (result != RVK_OK) {
        return result;
    }
    result = rvk_create_background_pipeline(device);
    if (result != RVK_OK) {
        return result;
    }
    result = rvk_create_overlay_pipeline(device, device->overlay_load_render_pass, &device->overlay_load_pipeline);
    if (result != RVK_OK) {
        return result;
    }
    return rvk_create_overlay_pipeline(device, device->overlay_clear_render_pass, &device->overlay_clear_pipeline);
}

void rvk_destroy_pipelines(RvkDevice *device) {
    if (device == NULL) {
        return;
    }
    if (device->overlay_clear_pipeline != VK_NULL_HANDLE && device->vk.DestroyPipeline != NULL) {
        device->vk.DestroyPipeline(device->device, device->overlay_clear_pipeline, NULL);
    }
    if (device->overlay_load_pipeline != VK_NULL_HANDLE && device->vk.DestroyPipeline != NULL) {
        device->vk.DestroyPipeline(device->device, device->overlay_load_pipeline, NULL);
    }
    if (device->world_pipeline != VK_NULL_HANDLE && device->vk.DestroyPipeline != NULL) {
        device->vk.DestroyPipeline(device->device, device->world_pipeline, NULL);
    }
    if (device->world_blend_pipeline != VK_NULL_HANDLE && device->vk.DestroyPipeline != NULL) {
        device->vk.DestroyPipeline(device->device, device->world_blend_pipeline, NULL);
        device->world_blend_pipeline = VK_NULL_HANDLE;
    }
    if (device->world_sky_pipeline != VK_NULL_HANDLE && device->vk.DestroyPipeline != NULL) {
        device->vk.DestroyPipeline(device->device, device->world_sky_pipeline, NULL);
    }
    if (device->background_pipeline != VK_NULL_HANDLE && device->vk.DestroyPipeline != NULL) {
        device->vk.DestroyPipeline(device->device, device->background_pipeline, NULL);
    }
    if (device->background_pipeline_layout != VK_NULL_HANDLE && device->vk.DestroyPipelineLayout != NULL) {
        device->vk.DestroyPipelineLayout(device->device, device->background_pipeline_layout, NULL);
    }
    if (device->overlay_pipeline_layout != VK_NULL_HANDLE && device->vk.DestroyPipelineLayout != NULL) {
        device->vk.DestroyPipelineLayout(device->device, device->overlay_pipeline_layout, NULL);
    }
    if (device->world_pipeline_layout != VK_NULL_HANDLE && device->vk.DestroyPipelineLayout != NULL) {
        device->vk.DestroyPipelineLayout(device->device, device->world_pipeline_layout, NULL);
    }
    device->overlay_clear_pipeline = VK_NULL_HANDLE;
    device->overlay_load_pipeline = VK_NULL_HANDLE;
    device->world_pipeline = VK_NULL_HANDLE;
    device->world_sky_pipeline = VK_NULL_HANDLE;
    device->background_pipeline = VK_NULL_HANDLE;
    device->background_pipeline_layout = VK_NULL_HANDLE;
    device->overlay_pipeline_layout = VK_NULL_HANDLE;
    device->world_pipeline_layout = VK_NULL_HANDLE;
}
