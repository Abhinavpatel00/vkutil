#include "vk_swapchain.h"
#include "vk_sync.h"
VkSurfaceCapabilities2KHR query_surface_capabilities(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    VkPhysicalDeviceSurfaceInfo2KHR info = {
        .sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
        .surface = surface,
    };

    VkSurfaceCapabilities2KHR caps = {
        .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
    };

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(gpu, &info, &caps));
    return caps;
}


VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR* caps, uint32_t desired_w, uint32_t desired_h)
{
    if(caps->currentExtent.width != 0xFFFFFFFF)
        return caps->currentExtent;
    VkExtent2D extent = {.width = desired_w, .height = desired_h};
    extent.width      = CLAMP(extent.width, caps->minImageExtent.width, caps->maxImageExtent.width);
    extent.height     = CLAMP(extent.height, caps->minImageExtent.height, caps->maxImageExtent.height);
    return extent;
}
void vk_create_swapchain(VkDevice device, VkPhysicalDevice gpu, FlowSwapchain* out_swapchain, const FlowSwapchainCreateInfo* info)
{
    VkSurfaceCapabilities2KHR caps = query_surface_capabilities(gpu, info->surface);

    VkExtent2D               extent = choose_extent(&caps.surfaceCapabilities, info->width, info->height);
    VkImageUsageFlags        usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | info->extra_usage;
    VkSwapchainCreateInfoKHR ci     = {.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                       .surface          = info->surface,
                                       .minImageCount    = info->min_image_count,
                                       .imageFormat      = info->preferred_format,
                                       .imageColorSpace  = info->preferred_color_space,
                                       .imageExtent      = extent,
                                       .imageArrayLayers = 1,
                                       .imageUsage       = usage,
                                       .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                       .preTransform     = caps.surfaceCapabilities.currentTransform,
                                       .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                       .presentMode      = info->preferred_present_mode,
                                       .clipped          = VK_TRUE,
                                       .oldSwapchain     = info->old_swapchain};

    VK_CHECK(vkCreateSwapchainKHR(device, &ci, NULL, &out_swapchain->swapchain));

    out_swapchain->surface       = info->surface;
    out_swapchain->extent        = extent;
    out_swapchain->format        = info->preferred_format;
    out_swapchain->color_space   = info->preferred_color_space;
    out_swapchain->present_mode  = info->preferred_present_mode;
    out_swapchain->current_image = 0;
    out_swapchain->image_usage   = usage;
    // Query swapchain images
    VK_CHECK(vkGetSwapchainImagesKHR(device, out_swapchain->swapchain, &out_swapchain->image_count, NULL));

    if(out_swapchain->image_count > MAX_SWAPCHAIN_IMAGES)
        out_swapchain->image_count = MAX_SWAPCHAIN_IMAGES;  // don’t blow the stack

    VK_CHECK(vkGetSwapchainImagesKHR(device, out_swapchain->swapchain, &out_swapchain->image_count, out_swapchain->images));

    // Create image views
    forEach(i, out_swapchain->image_count)
    {
        VkImageViewCreateInfo view_ci = VK_IMAGE_VIEW_DEFAULT(out_swapchain->images[i], out_swapchain->format);
        VK_CHECK(vkCreateImageView(device, &view_ci, NULL, &out_swapchain->image_views[i]));
    }

    vk_create_semaphores(device, out_swapchain->image_count, out_swapchain->render_finished);
}


void vk_swapchain_destroy(VkDevice device, FlowSwapchain* swapchain)
{
    if(!swapchain)
        return;

    forEach(i, swapchain->image_count)
    {
        if(swapchain->image_views[i] != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, swapchain->image_views[i], NULL);
        }
    }

    vk_destroy_semaphores(device, swapchain->image_count, swapchain->render_finished);
    if(swapchain->swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain->swapchain, NULL);
    }

    memset(swapchain, 0, sizeof(*swapchain));
}


void vk_swapchain_acquire(VkDevice device, FlowSwapchain* swapchain, VkSemaphore image_available_semaphore, VkFence fence, uint64_t timeout)
{
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain->swapchain, timeout, image_available_semaphore, fence, &swapchain->current_image));
}


void vk_swapchain_present(VkQueue present_queue, FlowSwapchain* swapchain, const VkSemaphore* wait_semaphores, uint32_t wait_count)
{
    VkPresentInfoKHR present_info = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext              = NULL,
        .waitSemaphoreCount = wait_count,
        .pWaitSemaphores    = wait_semaphores,
        .swapchainCount     = 1u,
        .pSwapchains        = &swapchain->swapchain,
        .pImageIndices      = &swapchain->current_image,
        .pResults           = NULL,
    };

    vkQueuePresentKHR(present_queue, &present_info);
}


VkPresentModeKHR vk_swapchain_select_present_mode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, bool vsync)
{
    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, NULL);

    VkPresentModeKHR modes[16];
    if(mode_count > 16)
        mode_count = 16;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &mode_count, modes);

    if(vsync)
    {
        /* Prefer FIFO (always available) */
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /* Prefer mailbox for low-latency without tearing */
    for(uint32_t i = 0; i < mode_count; i++)
    {
        if(modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }

    /* Fall back to immediate */
    for(uint32_t i = 0; i < mode_count; i++)
    {
        if(modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

void vk_swapchain_recreate(VkDevice device, VkPhysicalDevice gpu, FlowSwapchain* sc, uint32_t new_w, uint32_t new_h)
{
    vkDeviceWaitIdle(device);


    forEach(i, sc->image_count)
    {
        if(sc->image_views[i])
            vkDestroyImageView(device, sc->image_views[i], NULL);

    }

    vk_destroy_semaphores(device, sc->image_count, sc->render_finished);
    FlowSwapchainCreateInfo info = {0};
    info.surface                 = sc->surface;
    info.width                   = new_w;
    info.height                  = new_h;
    info.min_image_count         = MAX(3u, sc->image_count);
    info.preferred_format        = sc->format;
    info.preferred_color_space   = sc->color_space;
    info.preferred_present_mode  = sc->present_mode;
    info.extra_usage             = sc->image_usage & ~VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.old_swapchain           = sc->swapchain;

    VkSwapchainKHR old = sc->swapchain;

    vk_create_swapchain(device, gpu, sc, &info);

    if(old)
        vkDestroySwapchainKHR(device, old, NULL);
}
