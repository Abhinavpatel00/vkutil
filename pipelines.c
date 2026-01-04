#include "vk_pipelines.h"
#include <vulkan/vulkan_core.h>

VkPipeline create_graphics_pipeline(VkDevice                               device,
                                    VkPipelineCache                        pipelinecache,
                                    DescriptorLayoutCache*                 dcache,
                                    PipelineLayoutCache*                   plcache,
                                    const VkDescriptorSetLayoutCreateInfo* set_infos,
                                    uint32_t                               set_count,
                                    const VkPushConstantRange*             push_ranges,
                                    uint32_t                               push_count,
                                    GraphicsPipelineState*                 state)
{

    if(state->depth_test_enable)
    {
        assert(state->depth_format != VK_FORMAT_UNDEFINED);
    }
    if(state->primitive_restart_enable)
    {
        assert(state->topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || state->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
               || state->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
    }

    VkPipelineShaderStageCreateInfo      stages[2]   = {{
                                                            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                                                            .module = state->vert_shader,
                                                            .pName  = "main",
                                                 },
                                                        {
                                                            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                            .module = state->frag_shader,
                                                            .pName  = "main",
                                                 }};
    VkPipelineVertexInputStateCreateInfo vertexState = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = state->vertex_binding_description_count,
        .pVertexBindingDescriptions      = state->vertex_binding_descriptions,
        .vertexAttributeDescriptionCount = state->vertex_attribute_description_count,
        .pVertexAttributeDescriptions    = state->vertex_attribute_descriptions,
    };

    VkPipelineRenderingCreateInfo dynrenderinginfo       = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                            //  Multiview Rendering is a technique that was Invented for Rendering
                                                            //  the two eyes in VR in a single drawcall.
                                                            // https://www.reddit.com/r/GraphicsProgramming/comments/ngx7xr/single_pass_multiview_rendering_for_splitscreen/
                                                            //.viewMask =   https://doc.qt.io/qt-6/qt3dxr-multiview.html
                                                            .colorAttachmentCount    = state->color_attachment_count,
                                                            .pColorAttachmentFormats = state->color_formats,
                                                            .depthAttachmentFormat   = state->depth_format,
                                                            .stencilAttachmentFormat = state->stencil_format};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = state->topology,
        .primitiveRestartEnable = state->primitive_restart_enable,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        // think if pNext will be set to `rasterizationLineState`
        .polygonMode = state->polygon_mode,
        .cullMode    = state->cull_mode,
        .frontFace   = state->front_face,
        //	https://stackoverflow.com/questions/42470669/when-does-it-make-sense-to-turn-off-the-rasterization-step
        .rasterizerDiscardEnable = VK_FALSE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0,
        .depthBiasClamp          = 0,
        .depthBiasSlopeFactor    = 0,
        .lineWidth               = 1,
    };

    // another way is vkdynamicstate
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };
    uint32_t color_attachment_count = state->color_attachment_count;


    VkPipelineColorBlendAttachmentState  small[4];
    VkPipelineColorBlendAttachmentState* blend_attachments = small;

    if(color_attachment_count > 4)
    {
        blend_attachments = (VkPipelineColorBlendAttachmentState*)malloc(sizeof(*blend_attachments) * color_attachment_count);
    }

    if(color_attachment_count > 0)
    {

        for(uint32_t i = 0; i < color_attachment_count; i++)
        {
            blend_attachments[i] = (VkPipelineColorBlendAttachmentState){
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            };
        }
    }

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .attachmentCount = state->color_attachment_count,
        .pAttachments    = blend_attachments,
    };
    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = state->depth_test_enable,

        .depthWriteEnable      = state->depth_test_enable ? VK_TRUE : VK_FALSE,
        .depthCompareOp        = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable     = VK_FALSE,
        .front                 = {},
        .back                  = {},
        .minDepthBounds        = 0.0f,
        .maxDepthBounds        = 1.0f,

    };

    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable  = VK_FALSE,
    };

    VkDynamicState dynamics[2] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates    = dynamics,
    };

    assert(set_count <= 8);

    VkDescriptorSetLayout set_layouts[8];
    for(uint32_t i = 0; i < set_count; i++)
        set_layouts[i] = descriptor_layout_cache_get(device, dcache, &set_infos[i]);

    VkPipelineLayout pipeline_layout = pipeline_layout_cache_get(device, plcache, set_layouts, set_count, push_ranges, push_count);
    VkGraphicsPipelineCreateInfo ci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &dynrenderinginfo,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vertexState,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisample,
        .pDepthStencilState  = &depthStencil,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dynamic,
        .layout              = pipeline_layout,
        .renderPass          = VK_NULL_HANDLE,  // not needed anymore
                                                // .basePipelineHandle  useless bcz
        // no vedor recommends it
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(device, pipelinecache, 1, &ci, NULL, &pipeline));

    free(blend_attachments);
    return pipeline;
}
