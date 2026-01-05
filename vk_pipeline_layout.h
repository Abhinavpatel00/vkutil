#ifndef VK_PIPELINE_LAYOUT_H_
#define VK_PIPELINE_LAYOUT_H_

#include "vk_defaults.h"
#include "vk_descriptor.h"


typedef struct PipelineLayoutKey
{
    VkDescriptorSetLayout set_layouts[8];
    uint32_t              set_layout_count;

    VkPushConstantRange push_constants[4];
    uint32_t            push_constant_count;

    Hash64 hash;

} PipelineLayoutKey;

typedef struct PipelineLayoutEntry
{
    PipelineLayoutKey key;
    VkPipelineLayout  layout;

} PipelineLayoutEntry;

typedef struct PipelineLayoutCache
{
    PipelineLayoutEntry* entries;  // stretchy buffer
} PipelineLayoutCache;

void pipeline_layout_cache_init(PipelineLayoutCache* cache);

VkPipelineLayout pipeline_layout_cache_get(VkDevice                     device,
                                           PipelineLayoutCache*         cache,
                                           const VkDescriptorSetLayout* set_layouts,
                                           uint32_t                     set_layout_count,
                                           const VkPushConstantRange*   push_ranges,
                                           uint32_t                     push_range_count);

void pipeline_layout_cache_destroy(VkDevice device, PipelineLayoutCache* cache);


VkPipelineLayout pipeline_layout_cache_build(VkDevice                                   device,
                                             DescriptorLayoutCache*                     desc_cache,
                                             PipelineLayoutCache*                       pipe_cache,
                                             const VkDescriptorSetLayoutBinding* const* set_bindings,
                                             const uint32_t*                            binding_counts,
                                             uint32_t                                   set_count,
                                             const VkPushConstantRange*                 push_ranges,
                                             uint32_t                                   push_count);

#endif // VK_PIPELINE_LAYOUT_H_
