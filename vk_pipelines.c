#include "vk_pipelines.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Internal helpers
// ============================================================================

static bool read_file(const char* path, void** out_data, size_t* out_size)
{
    *out_data = NULL;
    *out_size = 0;

    FILE* f = fopen(path, "rb");
    if(!f)
    {
        log_error("Failed to open '%s' (errno=%d)", path, errno);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    if(len <= 0)
    {
        log_error("Invalid size for '%s'", path);
        fclose(f);
        return false;
    }

    void* data = malloc((size_t)len);
    if(!data)
    {
        log_error("Out of memory reading '%s'", path);
        fclose(f);
        return false;
    }

    if(fread(data, 1, (size_t)len, f) != (size_t)len)
    {
        log_error("Short read for '%s'", path);
        free(data);
        fclose(f);
        return false;
    }

    fclose(f);
    *out_data = data;
    *out_size = (size_t)len;
    return true;
}

static VkShaderModule create_shader_module(VkDevice device, const void* code, size_t size)
{
    VkShaderModuleCreateInfo ci = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode    = (const uint32_t*)code,
    };
    VkShaderModule mod = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &ci, NULL, &mod));
    return mod;
}

// ============================================================================
// Graphics Pipeline
// ============================================================================

VkPipeline create_graphics_pipeline(VkDevice                device,
                                    VkPipelineCache         cache,
                                    DescriptorLayoutCache*  desc_cache,
                                    PipelineLayoutCache*    pipe_cache,
                                    const char*             vert_path,
                                    const char*             frag_path,
                                    GraphicsPipelineConfig* cfg,
                                    VkPipelineLayout*       out_layout)
{
    // Load SPIR-V
    void*  vert_code = NULL;
    size_t vert_size = 0;
    void*  frag_code = NULL;
    size_t frag_size = 0;

    if(!read_file(vert_path, &vert_code, &vert_size))
        return VK_NULL_HANDLE;
    if(!read_file(frag_path, &frag_code, &frag_size))
    {
        free(vert_code);
        return VK_NULL_HANDLE;
    }

    // Create shader modules
    VkShaderModule vert_mod = create_shader_module(device, vert_code, vert_size);
    VkShaderModule frag_mod = create_shader_module(device, frag_code, frag_size);

    // Reflect and build pipeline layout
    const void*      spirvs[2] = {vert_code, frag_code};
    const size_t     sizes[2]  = {vert_size, frag_size};
    VkPipelineLayout layout    = shader_reflect_build_pipeline_layout(device, desc_cache, pipe_cache, spirvs, sizes, 2);
    if(out_layout)
        *out_layout = layout;

    // Shader stages
    VkPipelineShaderStageCreateInfo stages[2] = {
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_mod,
            .pName  = "main",
        },
        {
            .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_mod,
            .pName  = "main",
        },
    };

    ShaderReflection vert_reflect;
    shader_reflect_create(&vert_reflect, vert_code, vert_size);

    cfg->vertex_attribute_count = shader_reflect_get_vertex_attributes(&vert_reflect, cfg->vertex_attributes, 16,
                                                                       0  // binding index
    );


    uint32_t stride = 0;

    for(uint32_t i = 0; i < cfg->vertex_attribute_count; i++)
    {
        uint32_t end = cfg->vertex_attributes[i].offset;

        switch(cfg->vertex_attributes[i].format)
        {
            case VK_FORMAT_R32_SFLOAT:
                end += 4;
                break;
            case VK_FORMAT_R32G32_SFLOAT:
                end += 8;
                break;
            case VK_FORMAT_R32G32B32_SFLOAT:
                end += 12;
                break;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                end += 16;
                break;
            default:
                end += 4;
                break;
        }

        if(end > stride)
            stride = end;
    }


    // Vertex input
    cfg->vertex_binding_count = 1;
    cfg->vertex_bindings[0] =
        (VkVertexInputBindingDescription){.binding = 0, .stride = stride, .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    VkPipelineVertexInputStateCreateInfo vertex_input = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = cfg->vertex_binding_count,
        .pVertexBindingDescriptions      = cfg->vertex_bindings,
        .vertexAttributeDescriptionCount = cfg->vertex_attribute_count,
        .pVertexAttributeDescriptions    = cfg->vertex_attributes,
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = cfg->topology,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Viewport (dynamic)
    VkPipelineViewportStateCreateInfo viewport = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount  = 1,
    };

    // Rasterization
    VkPipelineRasterizationStateCreateInfo raster = {
        .sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = cfg->polygon_mode,
        .cullMode    = cfg->cull_mode,
        .frontFace   = cfg->front_face,
        .lineWidth   = 1.0f,
    };

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisample = {
        .sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Depth/stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable  = cfg->depth_test_enable,
        .depthWriteEnable = cfg->depth_write_enable,
        .depthCompareOp   = VK_COMPARE_OP_LESS,
    };

    // Color blend attachments
    VkPipelineColorBlendAttachmentState blend_atts[8];
    for(uint32_t i = 0; i < cfg->color_attachment_count && i < 8; i++)
    {
        blend_atts[i] = (VkPipelineColorBlendAttachmentState){
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        };
    }

    VkPipelineColorBlendStateCreateInfo blend = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = cfg->color_attachment_count,
        .pAttachments    = blend_atts,
    };

    // Dynamic state
    VkDynamicState                   dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic      = {
             .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
             .dynamicStateCount = 2,
             .pDynamicStates    = dyn_states,
    };

    // Dynamic rendering
    VkPipelineRenderingCreateInfo rendering = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount    = cfg->color_attachment_count,
        .pColorAttachmentFormats = cfg->color_formats,
        .depthAttachmentFormat   = cfg->depth_format,
        .stencilAttachmentFormat = cfg->stencil_format,
    };

    // Create pipeline
    VkGraphicsPipelineCreateInfo ci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext               = &rendering,
        .stageCount          = 2,
        .pStages             = stages,
        .pVertexInputState   = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState      = &viewport,
        .pRasterizationState = &raster,
        .pMultisampleState   = &multisample,
        .pDepthStencilState  = &depth_stencil,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dynamic,
        .layout              = layout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device, cache, 1, &ci, NULL, &pipeline));

    // Cleanup
    vkDestroyShaderModule(device, vert_mod, NULL);
    vkDestroyShaderModule(device, frag_mod, NULL);
    free(vert_code);
    free(frag_code);

    return pipeline;
}
void vk_cmd_set_viewport_scissor(VkCommandBuffer cmd, VkExtent2D extent)
{
    VkViewport vp = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float)extent.width,
        .height   = (float)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D sc = {
        .offset = {0, 0},
        .extent = extent,
    };

    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &sc);
}

// ============================================================================
// Compute Pipeline
// ============================================================================

VkPipeline create_compute_pipeline(VkDevice               device,
                                   VkPipelineCache        cache,
                                   DescriptorLayoutCache* desc_cache,
                                   PipelineLayoutCache*   pipe_cache,
                                   const char*            comp_path,
                                   VkPipelineLayout*      out_layout)
{
    // Load SPIR-V
    void*  comp_code = NULL;
    size_t comp_size = 0;
    if(!read_file(comp_path, &comp_code, &comp_size))
        return VK_NULL_HANDLE;

    // Create shader module
    VkShaderModule comp_mod = create_shader_module(device, comp_code, comp_size);

    // Reflect and build pipeline layout
    const void*      spirvs[1] = {comp_code};
    const size_t     sizes[1]  = {comp_size};
    VkPipelineLayout layout    = shader_reflect_build_pipeline_layout(device, desc_cache, pipe_cache, spirvs, sizes, 1);
    if(out_layout)
        *out_layout = layout;

    // Create pipeline
    VkPipelineShaderStageCreateInfo stage = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = comp_mod,
        .pName  = "main",
    };

    VkComputePipelineCreateInfo ci = {
        .sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage  = stage,
        .layout = layout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(device, cache, 1, &ci, NULL, &pipeline));

    // Cleanup
    vkDestroyShaderModule(device, comp_mod, NULL);
    free(comp_code);

    return pipeline;
}
