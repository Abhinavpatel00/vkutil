#include "vk_defaults.h"


#ifndef VK_DESCRIPTOR_H_
#define VK_DESCRIPTOR_H_

typedef struct DescriptorPoolChunk
{
    VkDescriptorPool pool;
    float            scale;
} DescriptorPoolChunk;

typedef struct DescriptorAllocator
{
    VkDevice             device;
    DescriptorPoolChunk* pools;
} DescriptorAllocator;

typedef struct DescriptorLayoutKey
{
    uint32_t                     binding_count;
    VkDescriptorSetLayoutBinding bindings[16];
    uint32_t                     hash;
} DescriptorLayoutKey;

// cache entry
typedef struct DescriptorLayoutEntry
{
    DescriptorLayoutKey   key;
    VkDescriptorSetLayout layout;
} DescriptorLayoutEntry;

typedef struct DescriptorLayoutCache
{
    DescriptorLayoutEntry* entries;
} DescriptorLayoutCache;

// manager bucket
typedef struct DescriptorAllocatorBucket
{
    DescriptorLayoutKey key;
    DescriptorAllocator alloc;
} DescriptorAllocatorBucket;

// master manager
typedef struct DescriptorAllocatorManager
{
    VkDevice                   device;
    DescriptorAllocatorBucket* buckets;
} DescriptorAllocatorManager;

// allocator API
void     descriptor_allocator_init(DescriptorAllocator* alloc, VkDevice device);
void     descriptor_allocator_destroy(DescriptorAllocator* alloc);
void     descriptor_allocator_reset(DescriptorAllocator* alloc);
VkResult descriptor_allocator_allocate(DescriptorAllocator* alloc, VkDescriptorSetLayout layout, VkDescriptorSet* out);

// layout cache API
void descriptor_layout_cache_init(DescriptorLayoutCache* cache);
void descriptor_layout_cache_destroy(VkDevice device, DescriptorLayoutCache* cache);
VkDescriptorSetLayout descriptor_layout_cache_get(VkDevice device, DescriptorLayoutCache* cache, const VkDescriptorSetLayoutCreateInfo* info);

// manager API
void     descriptor_allocator_manager_init(DescriptorAllocatorManager* m, VkDevice device);
void     descriptor_allocator_manager_destroy(DescriptorAllocatorManager* m);
VkResult descriptor_manager_allocate(DescriptorAllocatorManager*            m,
                                     DescriptorLayoutCache*                 cache,
                                     const VkDescriptorSetLayoutCreateInfo* info,
                                     VkDescriptorSet*                       out);

// helpers
VkDescriptorSetLayout get_or_create_set_layout(VkDevice                            device,
                                               DescriptorLayoutCache*              cache,
                                               const VkDescriptorSetLayoutBinding* bindings,
                                               uint32_t                            binding_count);


#endif  // VK_DESCRIPTOR_H_
