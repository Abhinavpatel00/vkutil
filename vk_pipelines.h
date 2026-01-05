#ifndef VK_PIPELINES_H_
#define VK_PIPELINES_H_

#include "vk_defaults.h"
#include "vk_pipeline_layout.h"
#include "vk_descriptor.h"
#include "vk_shader_reflect.h"

// ============================================================================
// Graphics Pipeline Config - minimal, no shader module fields
// ============================================================================

typedef struct GraphicsPipelineConfig
{
    // Vertex input (optional - can be NULL for vertex-pulling)
    uint32_t                                 vertex_binding_count;
    const VkVertexInputBindingDescription*   vertex_bindings;
    uint32_t                                 vertex_attribute_count;
    const VkVertexInputAttributeDescription* vertex_attributes;

    // Rasterization
    VkCullModeFlags  cull_mode;
    VkFrontFace      front_face;
    VkPolygonMode    polygon_mode;

    // Input assembly
    VkPrimitiveTopology topology;

    // Depth/stencil
    VkBool32 depth_test_enable;
    VkBool32 depth_write_enable;

    // Attachments (dynamic rendering)
    uint32_t        color_attachment_count;
    const VkFormat* color_formats;
    VkFormat        depth_format;
    VkFormat        stencil_format;

} GraphicsPipelineConfig;

// ============================================================================
// Pipeline Creation API - simple, file-path based
// ============================================================================

// Creates a graphics pipeline from SPIR-V file paths.
// Loads shaders, reflects descriptor/push-constant layout, creates pipeline.
// Returns pipeline handle; optionally outputs the created pipeline layout.
VkPipeline create_graphics_pipeline(VkDevice                device,
                                    VkPipelineCache         cache,
                                    DescriptorLayoutCache*  desc_cache,
                                    PipelineLayoutCache*    pipe_cache,
                                    const char*             vert_shader_path,
                                    const char*             frag_shader_path,
                                    GraphicsPipelineConfig* config,
                                    VkPipelineLayout*       out_layout);

// Creates a compute pipeline from a SPIR-V file path.
// Loads shader, reflects descriptor/push-constant layout, creates pipeline.
// Returns pipeline handle; optionally outputs the created pipeline layout.
VkPipeline create_compute_pipeline(VkDevice               device,
                                   VkPipelineCache        cache,
                                   DescriptorLayoutCache* desc_cache,
                                   PipelineLayoutCache*   pipe_cache,
                                   const char*            comp_shader_path,
                                   VkPipelineLayout*      out_layout);

// ============================================================================
// Default config helper
// ============================================================================

static inline GraphicsPipelineConfig graphics_pipeline_config_default(void)
{
    return (GraphicsPipelineConfig){
        .vertex_binding_count   = 0,
        .vertex_bindings        = NULL,
        .vertex_attribute_count = 0,
        .vertex_attributes      = NULL,
        .cull_mode              = VK_CULL_MODE_BACK_BIT,
        .front_face             = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .polygon_mode           = VK_POLYGON_MODE_FILL,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .depth_test_enable      = VK_FALSE,
        .depth_write_enable     = VK_FALSE,
        .color_attachment_count = 0,
        .color_formats          = NULL,
        .depth_format           = VK_FORMAT_UNDEFINED,
        .stencil_format         = VK_FORMAT_UNDEFINED,
    };
}

#endif // VK_PIPELINES_H_
