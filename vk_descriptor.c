
#include "vk_descriptor.h"

static VkDescriptorPool create_pool(VkDevice device)
{
    VkDescriptorPoolSize sizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 32},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},
    };

    VkDescriptorPoolCreateInfo info = {.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                       .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                       .maxSets       = 128,
                                       .poolSizeCount = (uint32_t)(sizeof sizes / sizeof sizes[0]),
                                       .pPoolSizes    = sizes};

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device, &info, NULL, &pool));
    return pool;
}

void descriptor_allocator_init(DescriptorAllocator* alloc, VkDevice device)
{
    alloc->device = device;
    alloc->pools  = NULL;
}

static VkDescriptorPool current_pool(DescriptorAllocator* a)
{
    if(arrlen(a->pools) == 0)
    {
        DescriptorPoolChunk chunk = {create_pool(a->device)};
        arrpush(a->pools, chunk);
    }

    return a->pools[arrlen(a->pools) - 1].pool;
}

VkResult descriptor_allocator_allocate(DescriptorAllocator* alloc, VkDescriptorSetLayout layout, VkDescriptorSet* out_set)
{
    VkDescriptorSetAllocateInfo info = {.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                        .descriptorPool     = current_pool(alloc),
                                        .descriptorSetCount = 1,
                                        .pSetLayouts        = &layout};

    VkResult r = vkAllocateDescriptorSets(alloc->device, &info, out_set);
    if(r == VK_SUCCESS)
        return VK_SUCCESS;

    if(r == VK_ERROR_OUT_OF_POOL_MEMORY || r == VK_ERROR_FRAGMENTED_POOL)
    {
        DescriptorPoolChunk chunk = {create_pool(alloc->device)};
        arrpush(alloc->pools, chunk);

        info.descriptorPool = chunk.pool;
        return vkAllocateDescriptorSets(alloc->device, &info, out_set);
    }

    return r;
}

void descriptor_allocator_reset(DescriptorAllocator* alloc)
{
    for(int i = 0; i < arrlen(alloc->pools); i++)
        vkResetDescriptorPool(alloc->device, alloc->pools[i].pool, 0);
}

void descriptor_allocator_destroy(DescriptorAllocator* alloc)
{
    for(int i = 0; i < arrlen(alloc->pools); i++)
        vkDestroyDescriptorPool(alloc->device, alloc->pools[i].pool, NULL);

    arrfree(alloc->pools);
}


// -------- Layout cache --------

void descriptor_layout_cache_init(DescriptorLayoutCache* cache)
{
    cache->entries = NULL;
}

static uint32_t hash_layout_key(const DescriptorLayoutKey* k)
{
    return hash32_bytes(k->bindings, k->binding_count * sizeof(VkDescriptorSetLayoutBinding)) ^ k->binding_count;
}

VkDescriptorSetLayout descriptor_layout_cache_get(VkDevice device, DescriptorLayoutCache* cache, const VkDescriptorSetLayoutCreateInfo* info)
{
    DescriptorLayoutKey key = {0};

    key.binding_count = info->bindingCount;
    memcpy(key.bindings, info->pBindings, info->bindingCount * sizeof(VkDescriptorSetLayoutBinding));

    key.hash = hash_layout_key(&key);

    for(int i = 0; i < arrlen(cache->entries); i++)
    {
        DescriptorLayoutEntry* e = &cache->entries[i];

        if(e->key.hash == key.hash && e->key.binding_count == key.binding_count
           && memcmp(e->key.bindings, key.bindings, key.binding_count * sizeof(VkDescriptorSetLayoutBinding)) == 0)
        {
            return e->layout;
        }
    }

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, info, NULL, &layout));

    DescriptorLayoutEntry entry = {key, layout};
    arrpush(cache->entries, entry);

    return layout;
}

void descriptor_layout_cache_destroy(VkDevice device, DescriptorLayoutCache* cache)
{
    for(int i = 0; i < arrlen(cache->entries); i++)
        vkDestroyDescriptorSetLayout(device, cache->entries[i].layout, NULL);

    arrfree(cache->entries);
}


VkDescriptorSetLayout get_or_create_set_layout(VkDevice                            device,
                                               DescriptorLayoutCache*              cache,
                                               const VkDescriptorSetLayoutBinding* bindings,
                                               uint32_t                            binding_count)
{
    VkDescriptorSetLayoutCreateInfo info = {.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                            .bindingCount = binding_count,
                                            .pBindings    = bindings};

    return descriptor_layout_cache_get(device, cache, &info);
}
