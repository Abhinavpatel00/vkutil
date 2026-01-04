#include "vk_pipelines.h"
#include <vulkan/vulkan_core.h>

#include <errno.h>
#include <stdio.h>

static VkShaderModule create_shader_module_from_spirv(VkDevice device, const void* spirv, size_t spirv_size)
{
    VkShaderModuleCreateInfo ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv_size,
        .pCode    = (const uint32_t*)spirv,
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &ci, NULL, &module));
    return module;
}

static bool read_entire_file(const char* path, void** out_data, size_t* out_size)
{
    *out_data = NULL;
    *out_size = 0;

    FILE* f = fopen(path, "rb");
    if(!f)
    {
        log_error("Failed to open file '%s' (errno=%d)", path, errno);
        return false;
    }

    if(fseek(f, 0, SEEK_END) != 0)
    {
        log_error("Failed to seek file '%s'", path);
        fclose(f);
        return false;
    }

    long size = ftell(f);
    if(size <= 0)
    {
        log_error("Invalid file size for '%s'", path);
        fclose(f);
        return false;
    }
    rewind(f);

    void* data = malloc((size_t)size);
    if(!data)
    {
        log_error("Out of memory reading '%s'", path);
        fclose(f);
        return false;
    }

    size_t got = fread(data, 1, (size_t)size, f);
    fclose(f);

    if(got != (size_t)size)
    {
        log_error("Short read for '%s' (%zu/%zu)", path, got, (size_t)size);
        free(data);
        return false;
    }

    *out_data = data;
    *out_size = (size_t)size;
    return true;
}

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


VkPipeline create_graphics_pipeline_reflected(VkDevice                device,
                                              VkPipelineCache         pipelinecache,
                                              DescriptorLayoutCache*  dcache,
                                              PipelineLayoutCache*    plcache,
                                              const void*             vert_spirv,
                                              size_t                  vert_spirv_size,
                                              const void*             frag_spirv,
                                              size_t                  frag_spirv_size,
                                              const char*             vert_entry,
                                              const char*             frag_entry,
                                              GraphicsPipelineState*  state,
                                              VkPipelineLayout*       out_pipeline_layout)
{
    if(vert_entry == NULL)
        vert_entry = "main";
    if(frag_entry == NULL)
        frag_entry = "main";

    VkShaderModule vert_module = create_shader_module_from_spirv(device, vert_spirv, vert_spirv_size);
    VkShaderModule frag_module = create_shader_module_from_spirv(device, frag_spirv, frag_spirv_size);

    const void*  spirvs[2] = {vert_spirv, frag_spirv};
    const size_t sizes[2]  = {vert_spirv_size, frag_spirv_size};

    VkPipelineLayout pipeline_layout = shader_reflect_build_pipeline_layout(device, dcache, plcache, spirvs, sizes, 2);
    if(out_pipeline_layout)
        *out_pipeline_layout = pipeline_layout;

    // Patch state temporarily with the created modules
    VkShaderModule prev_vert = state->vert_shader;
    VkShaderModule prev_frag = state->frag_shader;
    state->vert_shader       = vert_module;
    state->frag_shader       = frag_module;

    if(state->depth_test_enable)
    {
        assert(state->depth_format != VK_FORMAT_UNDEFINED);
    }
    if(state->primitive_restart_enable)
    {
        assert(state->topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || state->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
               || state->topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
    }

    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = state->vert_shader,
            .pName  = vert_entry,
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = state->frag_shader,
            .pName  = frag_entry,
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexState = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = state->vertex_binding_description_count,
        .pVertexBindingDescriptions      = state->vertex_binding_descriptions,
        .vertexAttributeDescriptionCount = state->vertex_attribute_description_count,
        .pVertexAttributeDescriptions    = state->vertex_attribute_descriptions,
    };

    VkPipelineRenderingCreateInfo dynrenderinginfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
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
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode             = state->polygon_mode,
        .cullMode                = state->cull_mode,
        .frontFace               = state->front_face,
        .rasterizerDiscardEnable = VK_FALSE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0,
        .depthBiasClamp          = 0,
        .depthBiasSlopeFactor    = 0,
        .lineWidth               = 1,
    };

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
                .blendEnable    = VK_FALSE,
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
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = state->depth_test_enable,
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
        .renderPass          = VK_NULL_HANDLE,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device, pipelinecache, 1, &ci, NULL, &pipeline));

    if(blend_attachments != small)
        free(blend_attachments);

    // Restore state and cleanup temporary modules
    state->vert_shader = prev_vert;
    state->frag_shader = prev_frag;
    vkDestroyShaderModule(device, vert_module, NULL);
    vkDestroyShaderModule(device, frag_module, NULL);

    return pipeline;
}


VkPipeline create_compute_pipeline_reflected(VkDevice               device,
                                             VkPipelineCache        pipelinecache,
                                             DescriptorLayoutCache* dcache,
                                             PipelineLayoutCache*   plcache,
                                             const void*            comp_spirv,
                                             size_t                 comp_spirv_size,
                                             const char*            comp_entry,
                                             VkPipelineLayout*      out_pipeline_layout)
{
    if(comp_entry == NULL)
        comp_entry = "main";

    VkShaderModule comp_module = create_shader_module_from_spirv(device, comp_spirv, comp_spirv_size);

    const void*  spirvs[1] = {comp_spirv};
    const size_t sizes[1]  = {comp_spirv_size};

    VkPipelineLayout pipeline_layout = shader_reflect_build_pipeline_layout(device, dcache, plcache, spirvs, sizes, 1);
    if(out_pipeline_layout)
        *out_pipeline_layout = pipeline_layout;

    VkPipelineShaderStageCreateInfo stage = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = comp_module,
        .pName  = comp_entry,
    };

    VkComputePipelineCreateInfo ci = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = pipeline_layout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(device, pipelinecache, 1, &ci, NULL, &pipeline));

    vkDestroyShaderModule(device, comp_module, NULL);
    return pipeline;
}


VkPipeline create_graphics_pipeline_reflected_from_file(VkDevice               device,
                                                        VkPipelineCache        pipelinecache,
                                                        DescriptorLayoutCache* dcache,
                                                        PipelineLayoutCache*   plcache,
                                                        const char*            vert_spirv_path,
                                                        const char*            frag_spirv_path,
                                                        const char*            vert_entry,
                                                        const char*            frag_entry,
                                                        GraphicsPipelineState* state,
                                                        VkPipelineLayout*      out_pipeline_layout)
{
    void*  vert_data = NULL;
    size_t vert_size = 0;
    void*  frag_data = NULL;
    size_t frag_size = 0;

    if(!read_entire_file(vert_spirv_path, &vert_data, &vert_size))
        return VK_NULL_HANDLE;
    if(!read_entire_file(frag_spirv_path, &frag_data, &frag_size))
    {
        free(vert_data);
        return VK_NULL_HANDLE;
    }

    VkPipeline p = create_graphics_pipeline_reflected(device,
                                                      pipelinecache,
                                                      dcache,
                                                      plcache,
                                                      vert_data,
                                                      vert_size,
                                                      frag_data,
                                                      frag_size,
                                                      vert_entry,
                                                      frag_entry,
                                                      state,
                                                      out_pipeline_layout);

    free(vert_data);
    free(frag_data);
    return p;
}


VkPipeline create_compute_pipeline_reflected_from_file(VkDevice               device,
                                                       VkPipelineCache        pipelinecache,
                                                       DescriptorLayoutCache* dcache,
                                                       PipelineLayoutCache*   plcache,
                                                       const char*            comp_spirv_path,
                                                       const char*            comp_entry,
                                                       VkPipelineLayout*      out_pipeline_layout)
{
    void*  comp_data = NULL;
    size_t comp_size = 0;

    if(!read_entire_file(comp_spirv_path, &comp_data, &comp_size))
        return VK_NULL_HANDLE;

    VkPipeline p = create_compute_pipeline_reflected(device,
                                                     pipelinecache,
                                                     dcache,
                                                     plcache,
                                                     comp_data,
                                                     comp_size,
                                                     comp_entry,
                                                     out_pipeline_layout);
    free(comp_data);
    return p;
}
