#include "tinytypes.h"
#include "vk_startup.h"
#include "vk_swapchain.h"
#include "vk_queue.h"
#include "vk_sync.h"
#include "vk_barrier.h"
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>
#include "vk_cmd.h"
#include "vk_pipelines.h"
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
          .enable_gpu_based_validation = true,
          .enable_validation           = true,

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
    forEach(i, MAX_FRAME_IN_FLIGHT)
    {
        vk_create_semaphore(device, &frame_sync[i].image_available_semaphore);
        vk_create_fence(device, true, &frame_sync[i].in_flight_fence);
    };
    vk_cmd_create_many_pools(device, qf.graphics_family, true, false, MAX_FRAME_IN_FLIGHT, cmd_pools);
    forEach(i, MAX_FRAME_IN_FLIGHT)
    {
        vk_cmd_alloc(device, cmd_pools[i], true, &cmd_buffers[i]);
    }
    DescriptorLayoutCache desc_cache = {0};
    PipelineLayoutCache   pipe_cache = {0};

    descriptor_layout_cache_init(&desc_cache);
    pipeline_layout_cache_init(&pipe_cache);

    GraphicsPipelineConfig cfg = graphics_pipeline_config_default();
    cfg.color_attachment_count = 1;
    cfg.color_formats          = &swap.format;
    cfg.depth_format           = VK_FORMAT_UNDEFINED;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    VkPipeline pipeline = create_graphics_pipeline(device, VK_NULL_HANDLE, &desc_cache, &pipe_cache, "compiledshaders/tri.vert.spv",
                                                   "compiledshaders/tri.frag.spv", &cfg, &pipeline_layout);

    VkImageLayout swap_image_layouts[MAX_SWAPCHAIN_IMAGES] = {0};


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

        vk_cmd_begin(cmd, true);


        /* transition for rendering target */
        IMAGE_BARRIER_IMMEDIATE(cmd, swap.images[image_index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo color_attach = {.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                                  .imageView   = swap.image_views[image_index],
                                                  .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                  .loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                  .storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
                                                  .clearValue  = {.color = {.float32 = {0.05f, 0.05f, 0.08f, 1.0f}}}};

        VkRenderingInfo rendering = {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea           = {.offset = {0, 0}, .extent = {swap.extent.width, swap.extent.height}},
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &color_attach,
        };

        vkCmdBeginRendering(cmd, &rendering);
        vk_cmd_set_viewport_scissor(cmd, swap.extent);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        /* no vertex buffers, use gl_VertexIndex */
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);

        /* transition for presentation */
        IMAGE_BARRIER_IMMEDIATE(cmd, swap.images[image_index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);


        vk_cmd_end(cmd);
        VkSemaphoreSubmitInfo wait_info   = {.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                             .semaphore = frame_sync[current_frame].image_available_semaphore,
                                             .value     = 0,
                                             .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphoreSubmitInfo signal_info = {
            .sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = swap.render_finished[image_index],
            .value     = 0,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
        };

        VkCommandBufferSubmitInfo cmdInfo = {.sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR,
                                             .pNext         = NULL,
                                             .commandBuffer = cmd_buffers[current_frame],
                                             .deviceMask    = 0};

        VkSubmitInfo2 submit = {.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                .waitSemaphoreInfoCount   = 1,
                                .pWaitSemaphoreInfos      = &wait_info,
                                .commandBufferInfoCount   = 1,
                                .pCommandBufferInfos      = &cmdInfo,
                                .signalSemaphoreInfoCount = 1,
                                .pSignalSemaphoreInfos    = &signal_info


        };

        VK_CHECK(vkQueueSubmit2(qf.graphics_queue, 1, &submit, frame_sync[current_frame].in_flight_fence));
        // Present
        if(!vk_swapchain_present(qf.present_queue, &swap, &swap.render_finished[swap.current_image], 1, &recreate))
        {
            if(recreate)
            {
                vk_swapchain_recreate(device, gpu, &swap, w, h);
                continue;
            }
        }

        current_frame = (current_frame + 1) % MAX_FRAME_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);
    for(u32 i = 0; i < MAX_FRAME_IN_FLIGHT; i++)
    {
        vkDestroyCommandPool(device, cmd_pools[i], NULL);
    }
    vkDestroyPipeline(device, pipeline, NULL);
    vkDestroyPipelineLayout(device, pipeline_layout, NULL);

    //    pipeline_layout_cache_destroy(device, &pipe_cache);
    descriptor_layout_cache_destroy(device, &desc_cache);

    vk_swapchain_destroy(device, &swap);
    vkDestroySurfaceKHR(ctx.instance, surface, NULL);
    vkDestroyDevice(device, NULL);
    vkDestroyInstance(ctx.instance, NULL);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
