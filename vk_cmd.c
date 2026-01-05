
#include "vk_cmd.h"
#include <string.h>

static VkCommandBufferLevel vk_cmd_level(bool primary)
{
    return primary ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
}

void vk_cmd_create_pool(VkDevice device, uint32_t queue_family_index, bool transient, bool resettable, VkCommandPool* out_pool)
{
    uint32_t flags = 0;

    if(transient)
        flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    if(resettable)
        flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPoolCreateInfo ci = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext            = NULL,
        .flags            = flags,
        .queueFamilyIndex = queue_family_index,
    };

    VK_CHECK(vkCreateCommandPool(device, &ci, NULL, out_pool));
}

void vk_cmd_destroy_pool(VkDevice device, VkCommandPool pool)
{
    if(pool)
        vkDestroyCommandPool(device, pool, NULL);
}

void vk_cmd_create_many_pools(VkDevice device, uint32_t queue_family_index, bool transient, bool resettable, uint32_t count, VkCommandPool* out_pools)
{
    for(uint32_t i = 0; i < count; i++)
    {
        vk_cmd_create_pool(device, queue_family_index, transient, resettable, &out_pools[i]);
    }
}

void vk_cmd_destroy_many_pools(VkDevice device, uint32_t count, VkCommandPool* pools)
{
    for(uint32_t i = 0; i < count; i++)
        vk_cmd_destroy_pool(device, pools[i]);
}

void vk_cmd_alloc(VkDevice device, VkCommandPool pool, bool primary, VkCommandBuffer* out_cmd)
{
    VkCommandBufferAllocateInfo ci = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = pool,
        .level              = vk_cmd_level(primary),
        .commandBufferCount = 1,
    };

    VK_CHECK(vkAllocateCommandBuffers(device, &ci, out_cmd));
}


void vk_cmd_begin(VkCommandBuffer cmd, bool one_time)
{
    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = one_time ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0u,
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
}

void vk_cmd_end(VkCommandBuffer cmd)
{
    VK_CHECK(vkEndCommandBuffer(cmd));
}

void vk_cmd_submit_once(VkDevice device, VkQueue queue, VkCommandBuffer cmd)
{
    VkSubmitInfo submit = {
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd,
    };

    VkFenceCreateInfo fc = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };

    VkFence fence = VK_NULL_HANDLE;

    VK_CHECK(vkCreateFence(device, &fc, NULL, &fence));
    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(device, fence, NULL);
}

void vk_cmd_reset(VkCommandBuffer cmd)
{
    VK_CHECK(vkResetCommandBuffer(cmd, 0));
}

void vk_cmd_reset_pool(VkDevice device, VkCommandPool pool)
{
    VK_CHECK(vkResetCommandPool(device, pool, 0));
}
