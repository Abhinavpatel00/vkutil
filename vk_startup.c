#include "vk_startup.h"
#include "external/logger-c/logger/logger.h"
#include "tinytypes.h"
void query_device_features(VkPhysicalDevice gpu, VkFeatureChain* out)
{
    memset(out, 0, sizeof(*out));

    out->core.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    out->v12.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    out->v13.sType  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    out->core.pNext = &out->v12;
    out->v12.pNext  = &out->v13;

    vkGetPhysicalDeviceFeatures2(gpu, &out->core);
}
bool is_instance_extension_supported(const char* extension_name)
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    VkExtensionProperties* extensions = malloc(extensionCount * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions);

    for(uint32_t i = 0; i < extensionCount; i++)
    {
        if(strcmp(extension_name, extensions[i].extensionName) == 0)
        {
            free(extensions);
            return true;
        }
    }

    free(extensions);
    return false;
}

RendererCaps default_caps(void)
{
    return (RendererCaps){
        .dynamic_rendering         = true,
        .sync2                     = true,
        .descriptor_indexing       = true,
        .timeline_semaphores       = true,
        .multi_draw_indirect       = true,
        .multi_draw_indirect_count = true,
        .buffer_device_address     = true,
        .maintenance4              = true,
    };
}
static void apply_caps(VkFeatureChain* f, const RendererCaps* caps)
{
#define TRY_ENABLE(flag, supported, name)                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if((caps->flag) && (supported))                                                                                \
        {                                                                                                              \
            (supported) = VK_TRUE;                                                                                     \
            log_info("[features] enabled: %s", name);                                                                  \
        }                                                                                                              \
        else if(caps->flag)                                                                                            \
        {                                                                                                              \
            log_info("[features] unavailable: %s", name);                                                              \
        }                                                                                                              \
    } while(0)

    TRY_ENABLE(dynamic_rendering, f->v13.dynamicRendering, "dynamic rendering");
    TRY_ENABLE(sync2, f->v13.synchronization2, "synchronization2");
    TRY_ENABLE(descriptor_indexing, f->v12.descriptorIndexing, "descriptor indexing");
    TRY_ENABLE(timeline_semaphores, f->v12.timelineSemaphore, "timeline semaphores");
    TRY_ENABLE(multi_draw_indirect, f->core.features.multiDrawIndirect, "multi-draw indirect");
    TRY_ENABLE(multi_draw_indirect_count, f->v13.maintenance4, /* counted indirect depends on maintenance 4 */
               "multi-draw indirect count");
    TRY_ENABLE(buffer_device_address, f->v12.bufferDeviceAddress, "buffer device address");
    TRY_ENABLE(maintenance4, f->v13.maintenance4, "maintenance4");

#undef TRY_ENABLE
}
bool device_supports_extensions(VkPhysicalDevice gpu, const char** req, uint32_t req_count)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(gpu, NULL, &count, NULL);

    VkExtensionProperties* props = malloc(sizeof(*props) * count);
    vkEnumerateDeviceExtensionProperties(gpu, NULL, &count, props);

    for(uint32_t i = 0; i < req_count; i++)
    {
        bool found = false;

        for(uint32_t j = 0; j < count; j++)
        {
            if(strcmp(req[i], props[j].extensionName) == 0)
            {
                found = true;
                break;
            }
        }

        if(!found)
        {
            log_error("[extensions] missing: %s", req[i]);
            free(props);
            return false;
        }

        log_info("[extensions] enabled: %s", req[i]);
    }

    free(props);
    return true;
}

void vk_create_instance(renderer_context* ctx, const renderer_context_desc* desc)
{
    memset(ctx, 0, sizeof(*ctx));

    const char* base_exts[8];
    uint32_t    ext_count = 0;

    // always needed
    base_exts[ext_count++] = VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME;

    if(desc->enable_validation)
    {
        base_exts[ext_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }
    // merge user extensions
    const uint32_t total = ext_count + desc->instance_extension_count;

    const char** merged = (const char**)malloc(sizeof(char*) * total);

    memcpy(merged, base_exts, sizeof(char*) * ext_count);

    if(desc->instance_extension_count)
    {
        memcpy(merged + ext_count, desc->instance_extensions, sizeof(char*) * desc->instance_extension_count);
    }

    VkApplicationInfo app = {.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                             .pApplicationName   = desc->app_name,
                             .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                             .pEngineName        = "vkutil",
                             .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
                             .apiVersion         = VK_API_VERSION_1_3};


    const char* layers[8];
    uint32_t    layer_count = desc->instance_layer_count;

    if(layer_count)
    {
        memcpy(layers, desc->instance_layers, sizeof(char*) * layer_count);
    }
    if(desc->enable_validation)
    {
        layers[layer_count++] = "VK_LAYER_KHRONOS_validation";
    }

    VkValidationFeaturesEXT validation_features;
    memset(&validation_features, 0, sizeof(validation_features));
    validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

    static const VkValidationFeatureEnableEXT enabled_features[] = {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT,
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT, VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT};

    VkInstanceCreateInfo info = {.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                 .pApplicationInfo        = &app,
                                 .enabledExtensionCount   = total,
                                 .ppEnabledExtensionNames = merged,
                                 .enabledLayerCount       = desc->instance_layer_count,
                                 .ppEnabledLayerNames     = desc->instance_layers,
                                 .pNext                   = NULL};

    if(desc->enable_validation && desc->enable_gpu_based_validation)
    {
        validation_features.enabledValidationFeatureCount = (uint32_t)(sizeof(enabled_features) / sizeof(enabled_features[0]));
        validation_features.pEnabledValidationFeatures = enabled_features;

        validation_features.pNext = info.pNext;
        info.pNext                = &validation_features;
    }

    VK_CHECK(vkCreateInstance(&info, NULL, &ctx->instance));

    free(merged);
    ctx->debug_utils_enabled = desc->enable_validation;
}


typedef struct GpuScore
{
    VkPhysicalDevice device;
    uint32_t         score;
} GpuScore;

static uint32_t score_physical_device(VkPhysicalDevice gpu, VkSurfaceKHR surface, const char** required_exts, uint32_t required_ext_count)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(gpu, &props);

    fprintf(stderr, "[GPU] Evaluating: %s\n", props.deviceName);

    if(!device_supports_extensions(gpu, required_exts, required_ext_count))
    {
        fprintf(stderr, "  -> rejected: missing required extensions\n");
        return 0;
    }

    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, NULL);

    VkQueueFamilyProperties qprops[32];
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, qprops);

    VkBool32 can_present = VK_FALSE;
    for(uint32_t i = 0; i < queue_count; i++)
    {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &present);
        if(present)
        {
            can_present = VK_TRUE;
            break;
        }
    }

    if(!can_present)
    {
        fprintf(stderr, "  -> rejected: cannot present to surface\n");
        return 0;
    }

    uint32_t score = 0;

    switch(props.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score += 1000;
            fprintf(stderr, "  + discrete bonus: 1000\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score += 600;
            fprintf(stderr, "  + integrated bonus: 600\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score += 300;
            fprintf(stderr, "  + virtual bonus: 300\n");
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            score += 50;
            fprintf(stderr, "  + cpu fallback: 50\n");
            break;
        default:
            fprintf(stderr, "  + unknown type: 0\n");
            break;
    }

    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(gpu, &mem);

    for(uint32_t i = 0; i < mem.memoryHeapCount; i++)
    {
        if(mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            uint32_t add = (uint32_t)(mem.memoryHeaps[i].size / (1024 * 1024 * 64));
            score += add;
            fprintf(stderr, "  + VRAM factor: %u\n", add);
        }
    }

    if(score == 0)
        score = 1;

    fprintf(stderr, "  -> final score: %u\n\n", score);
    return score;
}

VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface, renderer_context_desc* rcd)
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, NULL);
    if(count == 0)
    {
        fprintf(stderr, "[GPU] No Vulkan devices found. Tragic.\n");
        return VK_NULL_HANDLE;
    }

    VkPhysicalDevice devices[16];
    vkEnumeratePhysicalDevices(instance, &count, devices);

    GpuScore best = {0};

    fprintf(stderr, "[GPU] Found %u device(s). Scoring...\n\n", count);

    for(uint32_t i = 0; i < count; i++)
    {
        uint32_t score = score_physical_device(devices[i], surface, rcd->device_extensions, rcd->device_extension_count);

        if(score > best.score)
        {
            best.device = devices[i];
            best.score  = score;
        }
    }

    if(best.device == VK_NULL_HANDLE)
    {
        fprintf(stderr, "[GPU] No suitable device found. Time to rethink life choices.\n");
    }
    else
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(best.device, &props);
        fprintf(stderr, "[GPU] Selected device: %s (score %u)\n", props.deviceName, best.score);
    }

    return best.device;
}

void create_device(VkPhysicalDevice physical, VkSurfaceKHR surface, renderer_context_desc* renderer_ctx_desc, const queue_families q, VkDevice* out_device)
{
    if(!device_supports_extensions(physical, renderer_ctx_desc->device_extensions, renderer_ctx_desc->device_extension_count))
    {
        log_error("[device] cannot continue: missing required extensions");
        return;
    }

    float priority = 1.0f;

    uint32_t unique_families[4];
    uint32_t uf_count = 0;

    unique_families[uf_count++] = q.graphics_family;
    if(q.present_family != q.graphics_family)
        unique_families[uf_count++] = q.present_family;
    if(q.has_compute && q.compute_family != q.graphics_family)
        unique_families[uf_count++] = q.compute_family;
    if(q.has_transfer && q.transfer_family != q.graphics_family)
        unique_families[uf_count++] = q.transfer_family;

    VkDeviceQueueCreateInfo* queue_infos = malloc(sizeof(VkDeviceQueueCreateInfo) * uf_count);

    for(uint32_t i = 0; i < uf_count; i++)
    {
        queue_infos[i] = (VkDeviceQueueCreateInfo){.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                   .queueFamilyIndex = unique_families[i],
                                                   .queueCount       = 1,
                                                   .pQueuePriorities = &priority};
    }

    VkFeatureChain features;

    if(renderer_ctx_desc->use_custom_features)
    {
        log_info("[features] using custom chain from user");
        features = renderer_ctx_desc->custom_features;
    }
    else
    {
        query_device_features(physical, &features);

        RendererCaps caps = default_caps();  // user may override separately
        apply_caps(&features, &caps);
    }

    VkDeviceCreateInfo info = {.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                               .pNext                   = &features.core,
                               .queueCreateInfoCount    = uf_count,
                               .pQueueCreateInfos       = queue_infos,
                               .enabledExtensionCount   = renderer_ctx_desc->device_extension_count,
                               .ppEnabledExtensionNames = renderer_ctx_desc->device_extensions,
                               .pEnabledFeatures        = NULL};

    VK_CHECK(vkCreateDevice(physical, &info, NULL, out_device));

    free(queue_infos);
}
