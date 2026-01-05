#include "vk_defaults.h"
uint32_t hash32_bytes(const void* data, size_t size)
{
    return (uint32_t)XXH32(data, size, 0);
}

uint64_t hash64_bytes(const void* data, size_t size)
{
    return (uint64_t)XXH64(data, size, 0);
}

uint32_t round_up(uint32_t a, uint32_t b)
{
    return (a + b - 1) & ~(b - 1);
}
uint64_t round_up_64(uint64_t a, uint64_t b)
{
    return (a + b - 1) & ~(b - 1);
}
