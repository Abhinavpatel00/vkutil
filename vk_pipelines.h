#include "vk_defaults.h"

#include "vk_pipeline_layout.h"
#include "vk_descriptor.h"

// Optional helper for automatic pipeline layout/descriptor set layouts
#include "vk_shader_reflect.h"

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

// --- Reflected pipeline creation (auto descriptor/pipeline layouts) ---

// Builds descriptor set layouts + pipeline layout from SPIR-V (via SPIRV-Reflect)
// and creates a graphics pipeline. Shader modules are created and destroyed internally.
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
                                              VkPipelineLayout*       out_pipeline_layout);

// Builds descriptor set layouts + pipeline layout from SPIR-V (via SPIRV-Reflect)
// and creates a compute pipeline. Shader module is created and destroyed internally.
VkPipeline create_compute_pipeline_reflected(VkDevice               device,
                                             VkPipelineCache        pipelinecache,
                                             DescriptorLayoutCache* dcache,
                                             PipelineLayoutCache*   plcache,
                                             const void*            comp_spirv,
                                             size_t                 comp_spirv_size,
                                             const char*            comp_entry,
                                             VkPipelineLayout*      out_pipeline_layout);

// Same as above, but loads SPIR-V from file paths.
VkPipeline create_graphics_pipeline_reflected_from_file(VkDevice               device,
                                                        VkPipelineCache        pipelinecache,
                                                        DescriptorLayoutCache* dcache,
                                                        PipelineLayoutCache*   plcache,
                                                        const char*            vert_spirv_path,
                                                        const char*            frag_spirv_path,
                                                        const char*            vert_entry,
                                                        const char*            frag_entry,
                                                        GraphicsPipelineState* state,
                                                        VkPipelineLayout*      out_pipeline_layout);

VkPipeline create_compute_pipeline_reflected_from_file(VkDevice               device,
                                                       VkPipelineCache        pipelinecache,
                                                       DescriptorLayoutCache* dcache,
                                                       PipelineLayoutCache*   plcache,
                                                       const char*            comp_spirv_path,
                                                       const char*            comp_entry,
                                                       VkPipelineLayout*      out_pipeline_layout);
