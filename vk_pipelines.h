#include "vk_defaults.h"

#include "vk_pipeline_layout.h"
#include "vk_descriptor.h"

typedef struct
{

    VkShaderModule vert_shader;
    VkShaderModule frag_shader;

    uint32_t                               vertex_binding_description_count;
    const VkVertexInputBindingDescription* vertex_binding_descriptions;

    uint32_t                                 vertex_attribute_description_count;
    const VkVertexInputAttributeDescription* vertex_attribute_descriptions;
    VkCullModeFlags                          cull_mode;
    VkFrontFace                              front_face;
    VkPolygonMode                            polygon_mode;
    // It lets you use a special “restart” index inside an index buffer to break
    // one primitive and start a new one without another draw call
    VkBool32            depth_test_enable;
    VkBool32            primitive_restart_enable;
    VkPrimitiveTopology topology;

    uint32_t        color_attachment_count;
    const VkFormat* color_formats;

    VkFormat               depth_format;
    VkFormat               stencil_format;
} GraphicsPipelineState;

VkPipeline create_graphics_pipeline(VkDevice                               device,
                                    VkPipelineCache                        pipelinecache,
                                    DescriptorLayoutCache*                 dcache,
                                    PipelineLayoutCache*                   plcache,
                                    const VkDescriptorSetLayoutCreateInfo* set_infos,
                                    uint32_t                               set_count,
                                    const VkPushConstantRange*             push_ranges,
                                    uint32_t                               push_count,
                                    GraphicsPipelineState*                 state);
