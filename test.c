#include "tinytypes.h"
#include "vk_startup.h"
#include "vk_swapchain.h"
#include "vk_queue.h"
#include "vk_sync.h"
#include "vk_barrier.h"
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stdbool.h>
#include <vulkan/vulkan_core.h>
typedef struct
{
    VkSemaphore image_available_semaphore;
    VkFence     in_flight_fence;
} FrameSync;

int main()
{
    volkInitialize();
    if(!is_instance_extension_supported("VK_KHR_wayland_surface"))
    {

        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    }
    else
    {

        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    }
    assert(glfwInit());
    const char* layers[2];
    uint32_t    layer_count = 0;
    const char* dev_exts[]  = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    u32          glfw_ext_count = 0;
    const char** glfw_exts      = glfwGetRequiredInstanceExtensions(&glfw_ext_count);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow*           window = glfwCreateWindow(800, 600, "Vulkan", NULL, NULL);
    renderer_context_desc desc   = {
          .app_name = "My Renderer",

          .instance_layers     = NULL,
          .instance_extensions = glfw_exts,
          .device_extensions   = dev_exts,

          .instance_layer_count        = 0,
          .instance_extension_count    = glfw_ext_count,
          .device_extension_count      = 1,
          .enable_gpu_based_validation = false,
          .enable_validation           = 1,

          .validation_severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
                               | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
          .validation_types = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
          .use_custom_features = false  // IMPORTANT
    };


    renderer_context ctx = {0};
    vk_create_instance(&ctx, &desc);

    volkLoadInstanceOnly(ctx.instance);
    setup_debug_messenger(&ctx, &desc);


    VkSurfaceKHR surface;

    VK_CHECK(glfwCreateWindowSurface(ctx.instance, window, NULL, &surface));

    VkPhysicalDevice gpu = pick_physical_device(ctx.instance, surface, &desc);


    VkDevice device = VK_NULL_HANDLE;

    queue_families qf;
    find_queue_families(gpu, surface, &qf);  // however you already find queues

    create_device(gpu, surface, &desc, qf, &device);
    volkLoadDevice(device);
    init_device_queues(device, &qf);

    FlowSwapchain swap = {0};

    int fb_w = 0, fb_h = 0;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);

    FlowSwapchainCreateInfo sci = {.surface                = surface,
                                   .width                  = fb_w,
                                   .height                 = fb_h,
                                   .min_image_count        = 3,
                                   .preferred_present_mode = vk_swapchain_select_present_mode(gpu, surface, false),
                                   .preferred_format       = VK_FORMAT_B8G8R8A8_UNORM,
                                   .preferred_color_space  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                                   .extra_usage            = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                   .old_swapchain          = VK_NULL_HANDLE};

    vk_create_swapchain(device, gpu, &swap, &sci);

    u32             current_frame = 0;
    u32             image_index   = 0;
    FrameSync       frame_sync[MAX_FRAME_IN_FLIGHT];
    VkCommandPool   cmd_pools[MAX_FRAME_IN_FLIGHT];
    VkCommandBuffer cmd_buffers[MAX_FRAME_IN_FLIGHT];

    vk_create_semaphores(device, MAX_FRAME_IN_FLIGHT, &frame_sync->image_available_semaphore);
    vk_create_fences(device, MAX_FRAME_IN_FLIGHT, true, &frame_sync->in_flight_fence);
    forEach(i, MAX_FRAME_IN_FLIGHT)
    {
        VkCommandPoolCreateInfo pool_info = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = qf.graphics_family,
        };

        VK_CHECK(vkCreateCommandPool(device, &pool_info, NULL, &cmd_pools[i]));
    }
    forEach(i, MAX_FRAME_IN_FLIGHT)
    {
        VkCommandBufferAllocateInfo alloc = {.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                             .commandPool        = cmd_pools[i],
                                             .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                             .commandBufferCount = 1};

        VK_CHECK(vkAllocateCommandBuffers(device, &alloc, &cmd_buffers[i]));
    }


    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        if(w == 0 || h == 0)
        {
            continue;
        }

        bool recreate = false;
        vkWaitForFences(device, 1, &frame_sync[current_frame].in_flight_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &frame_sync[current_frame].in_flight_fence);
        /* reset EVERYTHING allocated for this frame */
        vkResetCommandPool(device, cmd_pools[current_frame], 0);
        // Acquire image
        if(!vk_swapchain_acquire(device, &swap, frame_sync[current_frame].image_available_semaphore, VK_NULL_HANDLE,
                                 UINT64_MAX, &recreate))
        {
            if(recreate)
            {
                vk_swapchain_recreate(device, gpu, &swap, w, h);
                continue;
            }
        }
        image_index = swap.current_image;
        // -------------------------------------------------------------
        // RENDER HERE using swap.images[image_index] via your FB/pipeline
        // -------------------------------------------------------------
        VkCommandBuffer cmd = cmd_buffers[current_frame];

        VkCommandBufferBeginInfo begin = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

        vkBeginCommandBuffer(cmd, &begin);


        // Transition: UNDEFINED -> TRANSFER_DST
        IMAGE_BARRIER_IMMEDIATE(cmd, swap.images[image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        VkClearColorValue color = {.float32 = {0.1f, 0.2f, 0.4f, 1.0f}};

        VkImageSubresourceRange range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};

        vkCmdClearColorImage(cmd, swap.images[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);

        // Transition: TRANSFER_DST -> PRESENT

        IMAGE_BARRIER_IMMEDIATE(cmd, swap.images[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkEndCommandBuffer(cmd);

        VkSemaphore          wait_semaphores[]   = {frame_sync[current_frame].image_available_semaphore};
        VkPipelineStageFlags wait_stage          = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSemaphore          signal_semaphores[] = {swap.render_finished[image_index]};

        VkSubmitInfo submit = {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount   = 1,
            .pWaitSemaphores      = wait_semaphores,
            .pWaitDstStageMask    = &wait_stage,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &cmd_buffers[current_frame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores    = signal_semaphores,
        };

        vkQueueSubmit(qf.graphics_queue, 1, &submit, frame_sync[current_frame].in_flight_fence);
        // Present
        if(!vk_swapchain_present(qf.present_queue, &swap, &swap.render_finished[swap.current_image], 1, &recreate))
        {
            if(recreate)
            {
                vk_swapchain_recreate(device, gpu, &swap, w, h);
                continue;
            }
        }
    }

    vkDeviceWaitIdle(device);
    for(u32 i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
    {
        vkDestroyCommandPool(device, cmd_pools[i], NULL);
    }

    vk_swapchain_destroy(device, &swap);
    vkDestroySurfaceKHR(ctx.instance, surface, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(ctx.instance, NULL);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
