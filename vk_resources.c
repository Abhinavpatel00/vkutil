#include "vk_resources.h"
#include "external/logger-c/logger/logger.h"
void res_init(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device, ResourceAllocator* ra, VmaAllocatorCreateInfo info)
{

    ra->device = device;
    info.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    info.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;
    info.flags |= VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;


    VkPhysicalDeviceVulkan11Properties props11 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};

    VkPhysicalDeviceProperties2 props = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &props11};

    vkGetPhysicalDeviceProperties2(info.physicalDevice, &props);
    ra->max_alloc_size = props11.maxMemoryAllocationSize;
    //  use VMA_DYNAMIC_VULKAN_FUNCTIONS
    VmaVulkanFunctions vulkanFunctions = {
        .vkGetInstanceProcAddr                   = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr                     = vkGetDeviceProcAddr,
        .vkGetPhysicalDeviceProperties           = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties     = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory                        = vkAllocateMemory,
        .vkFreeMemory                            = vkFreeMemory,
        .vkMapMemory                             = vkMapMemory,
        .vkUnmapMemory                           = vkUnmapMemory,
        .vkFlushMappedMemoryRanges               = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges          = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory                      = vkBindBufferMemory,
        .vkBindImageMemory                       = vkBindImageMemory,
        .vkGetBufferMemoryRequirements           = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements            = vkGetImageMemoryRequirements,
        .vkCreateBuffer                          = vkCreateBuffer,
        .vkDestroyBuffer                         = vkDestroyBuffer,
        .vkCreateImage                           = vkCreateImage,
        .vkDestroyImage                          = vkDestroyImage,
        .vkCmdCopyBuffer                         = vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR       = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR        = vkGetImageMemoryRequirements2,
        .vkBindBufferMemory2KHR                  = vkBindBufferMemory2,
        .vkBindImageMemory2KHR                   = vkBindImageMemory2,
        .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
        .vkGetDeviceBufferMemoryRequirements     = vkGetDeviceBufferMemoryRequirements,
        .vkGetDeviceImageMemoryRequirements      = vkGetDeviceImageMemoryRequirements,
    };
    info.pVulkanFunctions = &vulkanFunctions;

    VK_CHECK(vmaCreateAllocator(&info, &ra->allocator));
}
void res_deinit(ResourceAllocator* ra)
{

    if(!ra->allocator)
        return;

    vmaDestroyAllocator(ra->allocator);
}


static void res_add_leak_detection(ResourceAllocator* ra, VmaAllocation allocation)
{
    if(!ra)
        return;

    if(ra->leak_id == ra->allocation_counter)
    {
#if defined(_WIN32)
        if(IsDebuggerPresent())
            DebugBreak();
#elif defined(__unix__)
//	log_info(leak at )
#endif
    }

    char name[64];
    snprintf(name, sizeof(name), "alloc_%llu", (unsigned long long)ra->allocation_counter++);

    vmaSetAllocationName(ra->allocator, allocation, name);
}

void vk_create_buffer(ResourceAllocator*             ra,
                      const VkBufferCreateInfo*      bufferInfo,
                      const VmaAllocationCreateInfo* allocInfo,
                      VkDeviceSize                   minalignment,
                      Buffer*                        outbuffer)
{
    VmaAllocationInfo outinfo = {0};
    VK_CHECK(vmaCreateBufferWithAlignment(ra->allocator, bufferInfo, allocInfo, minalignment, &outbuffer->buffer,
                                          &outbuffer->allocation, &outinfo));

    outbuffer->buffer_size = bufferInfo->size;
    outbuffer->mapping     = (uint8_t*)outinfo.pMappedData;


    //  NEED the device to fetch device address
    VkBufferDeviceAddressInfo addrInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = outbuffer->buffer};


    outbuffer->address = vkGetBufferDeviceAddress(ra->device, &addrInfo);

    res_add_leak_detection(ra, outbuffer->allocation);
}


void res_create_buffer(ResourceAllocator*       ra,
                       VkDevice                 device,
                       VkDeviceSize             size,
                       VkBufferUsageFlags2KHR   usageflags,
                       VmaMemoryUsage           memory_usage,
                       VmaAllocationCreateFlags flags,
                       VkDeviceSize             min_alignment,
                       Buffer*                  out)
{
    VkBufferUsageFlags2CreateInfo usage2 = {.sType = VK_STRUCTURE_TYPE_BUFFER_USAGE_FLAGS_2_CREATE_INFO,
                                            .usage = usageflags | VK_BUFFER_USAGE_2_SHADER_DEVICE_ADDRESS_BIT
                                                     | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT};

    VkBufferCreateInfo buffer_info = {.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                      .pNext                 = &usage2,
                                      .size                  = size,
                                      .usage                 = 0,
                                      .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                                      .queueFamilyIndexCount = 0,
                                      .pQueueFamilyIndices   = NULL};

    VmaAllocationCreateInfo alloc_info = {
        .flags = flags,
        .usage = memory_usage,

    };

    vk_create_buffer(ra, &buffer_info, &alloc_info, min_alignment, out);
}


void res_destroy_buffer(ResourceAllocator* ra, Buffer* buf)
{
    if(!ra || !buf)
        return;

    if(buf->buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(ra->allocator, buf->buffer, buf->allocation);
    }

    buf->buffer      = VK_NULL_HANDLE;
    buf->allocation  = VK_NULL_HANDLE;
    buf->mapping     = NULL;
    buf->address     = 0;
    buf->buffer_size = 0;
}
