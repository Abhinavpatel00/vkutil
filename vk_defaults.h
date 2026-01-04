#pragma once
#include "tinytypes.h"
#include "external/xxHash/xxhash.h"

typedef uint32_t Hash32;
typedef uint64_t Hash64;

uint32_t hash32_bytes(const void* data, size_t size);
uint64_t hash64_bytes(const void* data, size_t size);

uint32_t hash32_bytes(const void* data, size_t size)
{
    return (uint32_t)XXH32(data, size, 0);
}

uint64_t hash64_bytes(const void* data, size_t size)
{
    return (uint64_t)XXH64(data, size, 0);
}
#define VK_IMAGE_VIEW_DEFAULT(img, fmt)                                                                                \
    (VkImageViewCreateInfo)                                                                                            \
    {                                                                                                                  \
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = NULL, .flags = 0, .image = (img),                  \
        .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = (fmt),                                                            \
        .components =                                                                                                  \
            {                                                                                                          \
                VK_COMPONENT_SWIZZLE_IDENTITY,                                                                         \
                VK_COMPONENT_SWIZZLE_IDENTITY,                                                                         \
                VK_COMPONENT_SWIZZLE_IDENTITY,                                                                         \
                VK_COMPONENT_SWIZZLE_IDENTITY,                                                                         \
            },                                                                                                         \
        .subresourceRange = {                                                                                          \
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,                                                               \
            .baseMipLevel   = 0,                                                                                       \
            .levelCount     = 1,                                                                                       \
            .baseArrayLayer = 0,                                                                                       \
            .layerCount     = 1,                                                                                       \
        },                                                                                                             \
    }
