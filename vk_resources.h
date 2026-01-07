#include "tinytypes.h"
#include "vk_defaults.h"
// buffer is a region of memory used to store vertex data, index data, uniform data, and other types of data.
typedef struct Buffer
{
    VkBuffer        buffer;  // vulkan buffer
    VkDeviceSize    buffer_size;
    VkDeviceAddress address;     // addr of the buffer in the shader
    uint8_t*        mapping;     //this is a CPU pointer directly into GPU-visible memory.
    VmaAllocation   allocation;  // Memory associated with the buffer
} Buffer;


// the buffer exists logically as one big thing.
//
// physically, it is backed by many allocations
//
//so
//
// struct largeBuffer
// {
// 	           *VmaAllocation // many alloc
// };
//
//

typedef struct Image
{
    VkImage               image;
    VkExtent3D            extent;
    uint32_t              mipLevels;
    uint32_t              arrayLayers;
    VkFormat              format;
    VmaAllocation         allocation;
    VkDescriptorImageInfo descriptor;
} Image;


typedef struct ResourceAllocator
{
    VkDevice     device;
    VmaAllocator allocator;

    uint64_t     leak_id;
    uint64_t     allocation_counter;
    VkDeviceSize max_alloc_size;

} ResourceAllocator;


void res_init(VkInstance instance, VkDevice device, VkPhysicalDevice physical_device, ResourceAllocator* ra, VmaAllocatorCreateInfo info);
void res_deinit(ResourceAllocator* ra);
void vk_create_buffer(ResourceAllocator*             ra,
                      const VkBufferCreateInfo*      bufferInfo,
                      const VmaAllocationCreateInfo* allocInfo,
                      VkDeviceSize                   minalignment,
                      Buffer*                        outbuffer);


void res_create_buffer(ResourceAllocator*       ra,
                       VkDevice                 device,
                       VkDeviceSize             size,
                       VkBufferUsageFlags2KHR   usageflags,
                       VmaMemoryUsage           memory_usage,
                       VmaAllocationCreateFlags flags,
                       VkDeviceSize             min_alignment,
                       Buffer*                  out);


void res_destroy_buffer(ResourceAllocator* ra, Buffer* buf);
