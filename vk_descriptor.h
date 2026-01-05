#include "vk_defaults.h"


#ifndef VK_DESCRIPTOR_H_
#define VK_DESCRIPTOR_H_

typedef struct DescriptorPoolChunk
{
    VkDescriptorPool pool;
} DescriptorPoolChunk;

typedef struct DescriptorAllocator
{
    VkDevice             device;
    DescriptorPoolChunk* pools;
} DescriptorAllocator;

void descriptor_allocator_init(DescriptorAllocator* alloc, VkDevice device);
VkResult descriptor_allocator_allocate(DescriptorAllocator* alloc, VkDescriptorSetLayout layout, VkDescriptorSet* out_set);
void descriptor_allocator_reset(DescriptorAllocator* alloc);
void descriptor_allocator_destroy(DescriptorAllocator* alloc);


// -------- Layout cache --------

typedef struct DescriptorLayoutKey
{
    VkDescriptorSetLayoutBinding bindings[16];
    uint32_t                     binding_count;
    uint32_t                     hash;
} DescriptorLayoutKey;

typedef struct DescriptorLayoutEntry
{
    DescriptorLayoutKey   key;
    VkDescriptorSetLayout layout;
} DescriptorLayoutEntry;

typedef struct DescriptorLayoutCache
{
    DescriptorLayoutEntry* entries;
} DescriptorLayoutCache;

void descriptor_layout_cache_init(DescriptorLayoutCache* cache);
VkDescriptorSetLayout descriptor_layout_cache_get(VkDevice device, DescriptorLayoutCache* cache, const VkDescriptorSetLayoutCreateInfo* info);

void descriptor_layout_cache_destroy(VkDevice device, DescriptorLayoutCache* cache);


VkDescriptorSetLayout get_or_create_set_layout(VkDevice                            device,
                                               DescriptorLayoutCache*              cache,
                                               const VkDescriptorSetLayoutBinding* bindings,
                                               uint32_t                            binding_count);

#endif // VK_DESCRIPTOR_H_
