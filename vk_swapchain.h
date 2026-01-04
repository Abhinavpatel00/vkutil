#include "vk_defaults.h"
#include "vk_sync.h"
#include <vulkan/vulkan_core.h>

#define MAX_SWAPCHAIN_IMAGES 8


typedef struct ALIGNAS(64) FlowSwapchain
{
    VkSwapchainKHR   swapchain;
    VkSurfaceKHR     surface;
    VkFormat         format;
    VkColorSpaceKHR  color_space;
    VkPresentModeKHR present_mode;
    VkExtent2D       extent;

    uint32_t    image_count;
    VkImage     images[MAX_SWAPCHAIN_IMAGES];
    VkImageView image_views[MAX_SWAPCHAIN_IMAGES];

    VkSemaphore render_finished[MAX_SWAPCHAIN_IMAGES];

    VkImageUsageFlags image_usage;

    uint32_t current_image;
    bool     vsync;

} FlowSwapchain;

typedef struct FlowSwapchainCreateInfo
{
    VkSurfaceKHR      surface;
    uint32_t          width;
    uint32_t          height;
    uint32_t          min_image_count;
    VkPresentModeKHR  preferred_present_mode;
    VkFormat          preferred_format;
    VkColorSpaceKHR   preferred_color_space; /* VK_COLOR_SPACE_SRGB_NONLINEAR_KHR default */
    VkImageUsageFlags extra_usage;           /* Additional usage flags */
    VkSwapchainKHR    old_swapchain;         /* For recreation */
} FlowSwapchainCreateInfo;

void vk_create_swapchain(VkDevice device, VkPhysicalDevice gpu, FlowSwapchain* swapchain, const FlowSwapchainCreateInfo* info);
void vk_swapchain_destroy(VkDevice device, FlowSwapchain* swapchain);
void vk_swapchain_acquire(VkDevice device, FlowSwapchain* swapchain, VkSemaphore image_available_semaphore, VkFence fence, uint64_t timeout);
void vk_swapchain_present(VkQueue present_queue, FlowSwapchain* swapchain, const VkSemaphore* wait_semaphores, uint32_t wait_count);
void vk_swapchain_recreate(VkDevice device, VkPhysicalDevice gpu, FlowSwapchain* sc, uint32_t new_w, uint32_t new_h);
VkPresentModeKHR vk_swapchain_select_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, bool vsync);


