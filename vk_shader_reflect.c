#include "vk_shader_reflect.h"

// Convert SpvReflectDescriptorType to VkDescriptorType
static VkDescriptorType spv_to_vk_descriptor_type(SpvReflectDescriptorType spv_type)
{
    switch(spv_type)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                    return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:     return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:              return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:              return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:       return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:       return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:             return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:             return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:     return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:     return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:           return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        default:                                                      return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

// Convert SpvReflectShaderStageFlagBits to VkShaderStageFlagBits
static VkShaderStageFlagBits spv_to_vk_shader_stage(SpvReflectShaderStageFlagBits spv_stage)
{
    switch(spv_stage)
    {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:                  return VK_SHADER_STAGE_VERTEX_BIT;
        case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:    return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:                return VK_SHADER_STAGE_GEOMETRY_BIT;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:                return VK_SHADER_STAGE_FRAGMENT_BIT;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:                 return VK_SHADER_STAGE_COMPUTE_BIT;
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT:                return VK_SHADER_STAGE_TASK_BIT_EXT;
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT:                return VK_SHADER_STAGE_MESH_BIT_EXT;
        case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:              return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:             return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:         return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:                return VK_SHADER_STAGE_MISS_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:            return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        default:                                                    return 0;
    }
}

// Convert SpvReflectFormat to VkFormat
static VkFormat spv_to_vk_format(SpvReflectFormat spv_format)
{
    // SpvReflectFormat values match VkFormat values directly
    return (VkFormat)spv_format;
}


bool shader_reflect_create(ShaderReflection* reflection,
                           const void*       spirv_code,
                           size_t            spirv_size)
{
    memset(reflection, 0, sizeof(*reflection));

    SpvReflectResult result = spvReflectCreateShaderModule(spirv_size, spirv_code, &reflection->module);
    if(result != SPV_REFLECT_RESULT_SUCCESS)
    {
        log_error("Failed to create shader reflection module: %d", result);
        return false;
    }

    reflection->stage       = spv_to_vk_shader_stage(reflection->module.shader_stage);
    reflection->entry_point = reflection->module.entry_point_name;

    // Get compute shader local size
    if(reflection->stage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        const SpvReflectEntryPoint* entry = spvReflectGetEntryPoint(&reflection->module, reflection->entry_point);
        if(entry)
        {
            reflection->local_size_x = entry->local_size.x;
            reflection->local_size_y = entry->local_size.y;
            reflection->local_size_z = entry->local_size.z;
        }
    }

    // Enumerate descriptor sets
    uint32_t set_count = 0;
    result = spvReflectEnumerateDescriptorSets(&reflection->module, &set_count, NULL);
    if(result != SPV_REFLECT_RESULT_SUCCESS)
    {
        log_error("Failed to enumerate descriptor sets: %d", result);
        spvReflectDestroyShaderModule(&reflection->module);
        return false;
    }

    if(set_count > 0)
    {
        SpvReflectDescriptorSet* sets[SHADER_REFLECT_MAX_SETS];
        set_count = MIN(set_count, SHADER_REFLECT_MAX_SETS);

        result = spvReflectEnumerateDescriptorSets(&reflection->module, &set_count, sets);
        if(result != SPV_REFLECT_RESULT_SUCCESS)
        {
            log_error("Failed to get descriptor sets: %d", result);
            spvReflectDestroyShaderModule(&reflection->module);
            return false;
        }

        reflection->set_count = set_count;

        for(uint32_t i = 0; i < set_count; i++)
        {
            const SpvReflectDescriptorSet* spv_set = sets[i];
            ReflectedDescriptorSet*        ref_set = &reflection->sets[i];

            ref_set->set_index     = spv_set->set;
            ref_set->binding_count = MIN(spv_set->binding_count, SHADER_REFLECT_MAX_BINDINGS);

            for(uint32_t j = 0; j < ref_set->binding_count; j++)
            {
                const SpvReflectDescriptorBinding* spv_binding = spv_set->bindings[j];
                ReflectedBinding*                  ref_binding = &ref_set->bindings[j];

                ref_binding->binding          = spv_binding->binding;
                ref_binding->descriptor_type  = spv_to_vk_descriptor_type(spv_binding->descriptor_type);
                ref_binding->descriptor_count = spv_binding->count;
                ref_binding->stage_flags      = reflection->stage;
                ref_binding->name             = spv_binding->name;

                // Handle arrays
                if(spv_binding->count == 0)
                    ref_binding->descriptor_count = 1;
            }
        }
    }

    // Enumerate push constants
    uint32_t push_count = 0;
    result = spvReflectEnumeratePushConstantBlocks(&reflection->module, &push_count, NULL);
    if(result == SPV_REFLECT_RESULT_SUCCESS && push_count > 0)
    {
        SpvReflectBlockVariable* push_blocks[SHADER_REFLECT_MAX_PUSH];
        push_count = MIN(push_count, SHADER_REFLECT_MAX_PUSH);

        result = spvReflectEnumeratePushConstantBlocks(&reflection->module, &push_count, push_blocks);
        if(result == SPV_REFLECT_RESULT_SUCCESS)
        {
            reflection->push_constant_count = push_count;

            for(uint32_t i = 0; i < push_count; i++)
            {
                const SpvReflectBlockVariable* spv_push = push_blocks[i];
                ReflectedPushConstant*         ref_push = &reflection->push_constants[i];

                ref_push->offset      = spv_push->offset;
                ref_push->size        = spv_push->size;
                ref_push->stage_flags = reflection->stage;
                ref_push->name        = spv_push->name;
            }
        }
    }

    // Enumerate vertex inputs (only for vertex shaders)
    if(reflection->stage == VK_SHADER_STAGE_VERTEX_BIT)
    {
        uint32_t input_count = 0;
        result = spvReflectEnumerateInputVariables(&reflection->module, &input_count, NULL);
        if(result == SPV_REFLECT_RESULT_SUCCESS && input_count > 0)
        {
            SpvReflectInterfaceVariable* inputs[SHADER_REFLECT_MAX_INPUTS];
            input_count = MIN(input_count, SHADER_REFLECT_MAX_INPUTS);

            result = spvReflectEnumerateInputVariables(&reflection->module, &input_count, inputs);
            if(result == SPV_REFLECT_RESULT_SUCCESS)
            {
                uint32_t valid_count = 0;
                for(uint32_t i = 0; i < input_count; i++)
                {
                    const SpvReflectInterfaceVariable* spv_input = inputs[i];

                    // Skip built-in variables
                    if(spv_input->built_in >= 0)
                        continue;

                    ReflectedVertexInput* ref_input = &reflection->vertex_inputs[valid_count];
                    ref_input->location = spv_input->location;
                    ref_input->format   = spv_to_vk_format(spv_input->format);
                    ref_input->name     = spv_input->name;
                    ref_input->offset   = 0; // Will be calculated during vertex layout creation

                    valid_count++;
                }
                reflection->vertex_input_count = valid_count;
            }
        }
    }

    return true;
}


void shader_reflect_destroy(ShaderReflection* reflection)
{
    spvReflectDestroyShaderModule(&reflection->module);
    memset(reflection, 0, sizeof(*reflection));
}


void shader_reflect_merge(MergedReflection*       merged,
                          const ShaderReflection* reflections,
                          uint32_t                reflection_count)
{
    memset(merged, 0, sizeof(*merged));

    // Track the highest set index we've seen
    uint32_t max_set = 0;

    // First pass: find max set index
    for(uint32_t r = 0; r < reflection_count; r++)
    {
        const ShaderReflection* ref = &reflections[r];
        for(uint32_t s = 0; s < ref->set_count; s++)
        {
            if(ref->sets[s].set_index >= max_set)
                max_set = ref->sets[s].set_index + 1;
        }
    }

    merged->set_count = MIN(max_set, SHADER_REFLECT_MAX_SETS);

    // Merge descriptor bindings from all shaders
    for(uint32_t r = 0; r < reflection_count; r++)
    {
        const ShaderReflection* ref = &reflections[r];

        for(uint32_t s = 0; s < ref->set_count; s++)
        {
            const ReflectedDescriptorSet* src_set = &ref->sets[s];
            uint32_t set_index = src_set->set_index;

            if(set_index >= SHADER_REFLECT_MAX_SETS)
                continue;

            ReflectedDescriptorSet* dst_set = &merged->sets[set_index];
            dst_set->set_index = set_index;

            for(uint32_t b = 0; b < src_set->binding_count; b++)
            {
                const ReflectedBinding* src_binding = &src_set->bindings[b];

                // Check if binding already exists
                bool found = false;
                for(uint32_t db = 0; db < dst_set->binding_count; db++)
                {
                    if(dst_set->bindings[db].binding == src_binding->binding)
                    {
                        // Merge stage flags
                        dst_set->bindings[db].stage_flags |= src_binding->stage_flags;
                        found = true;
                        break;
                    }
                }

                if(!found && dst_set->binding_count < SHADER_REFLECT_MAX_BINDINGS)
                {
                    dst_set->bindings[dst_set->binding_count] = *src_binding;
                    dst_set->binding_count++;
                }
            }
        }
    }

    // Merge push constants
    for(uint32_t r = 0; r < reflection_count; r++)
    {
        const ShaderReflection* ref = &reflections[r];

        for(uint32_t p = 0; p < ref->push_constant_count; p++)
        {
            const ReflectedPushConstant* src_push = &ref->push_constants[p];

            // Check for overlapping push constant ranges
            bool found = false;
            for(uint32_t dp = 0; dp < merged->push_constant_count; dp++)
            {
                VkPushConstantRange* dst = &merged->push_constants[dp];

                // Check if ranges overlap
                if(dst->offset == src_push->offset && dst->size == src_push->size)
                {
                    dst->stageFlags |= src_push->stage_flags;
                    found = true;
                    break;
                }
            }

            if(!found && merged->push_constant_count < SHADER_REFLECT_MAX_PUSH)
            {
                VkPushConstantRange* dst = &merged->push_constants[merged->push_constant_count];
                dst->offset      = src_push->offset;
                dst->size        = src_push->size;
                dst->stageFlags  = src_push->stage_flags;
                merged->push_constant_count++;
            }
        }
    }
}


uint32_t shader_reflect_get_set_layout_bindings(const ReflectedDescriptorSet*    set,
                                                 VkDescriptorSetLayoutBinding*   out_bindings,
                                                 uint32_t                        max_bindings)
{
    uint32_t count = MIN(set->binding_count, max_bindings);

    for(uint32_t i = 0; i < count; i++)
    {
        const ReflectedBinding* src = &set->bindings[i];

        out_bindings[i] = (VkDescriptorSetLayoutBinding){
            .binding            = src->binding,
            .descriptorType     = src->descriptor_type,
            .descriptorCount    = src->descriptor_count,
            .stageFlags         = src->stage_flags,
            .pImmutableSamplers = NULL
        };
    }

    return count;
}


void shader_reflect_create_set_layouts(VkDevice                device,
                                       DescriptorLayoutCache*  cache,
                                       const MergedReflection* merged,
                                       VkDescriptorSetLayout*  out_layouts,
                                       uint32_t*               out_layout_count)
{
    *out_layout_count = merged->set_count;

    for(uint32_t i = 0; i < merged->set_count; i++)
    {
        const ReflectedDescriptorSet* set = &merged->sets[i];

        if(set->binding_count == 0)
        {
            // Empty set - create empty layout
            VkDescriptorSetLayoutCreateInfo info = {
                .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 0,
                .pBindings    = NULL
            };
            out_layouts[i] = descriptor_layout_cache_get(device, cache, &info);
        }
        else
        {
            VkDescriptorSetLayoutBinding bindings[SHADER_REFLECT_MAX_BINDINGS];
            uint32_t binding_count = shader_reflect_get_set_layout_bindings(set, bindings, SHADER_REFLECT_MAX_BINDINGS);

            VkDescriptorSetLayoutCreateInfo info = {
                .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = binding_count,
                .pBindings    = bindings
            };

            out_layouts[i] = descriptor_layout_cache_get(device, cache, &info);
        }
    }
}


VkPipelineLayout shader_reflect_create_pipeline_layout(VkDevice                device,
                                                       DescriptorLayoutCache*  desc_cache,
                                                       PipelineLayoutCache*    pipe_cache,
                                                       const MergedReflection* merged)
{
    VkDescriptorSetLayout set_layouts[SHADER_REFLECT_MAX_SETS];
    uint32_t              set_layout_count = 0;

    shader_reflect_create_set_layouts(device, desc_cache, merged, set_layouts, &set_layout_count);

    return pipeline_layout_cache_get(device,
                                     pipe_cache,
                                     set_layouts,
                                     set_layout_count,
                                     merged->push_constants,
                                     merged->push_constant_count);
}


VkPipelineLayout shader_reflect_build_pipeline_layout(VkDevice               device,
                                                      DescriptorLayoutCache* desc_cache,
                                                      PipelineLayoutCache*   pipe_cache,
                                                      const void* const*     spirv_codes,
                                                      const size_t*          spirv_sizes,
                                                      uint32_t               shader_count)
{
    ShaderReflection reflections[8];
    uint32_t         valid_count = 0;

    for(uint32_t i = 0; i < shader_count && i < 8; i++)
    {
        if(shader_reflect_create(&reflections[valid_count], spirv_codes[i], spirv_sizes[i]))
        {
            valid_count++;
        }
    }

    if(valid_count == 0)
    {
        log_error("No valid shader reflections created");
        return VK_NULL_HANDLE;
    }

    MergedReflection merged;
    shader_reflect_merge(&merged, reflections, valid_count);

    VkPipelineLayout layout = shader_reflect_create_pipeline_layout(device, desc_cache, pipe_cache, &merged);

    // Cleanup
    for(uint32_t i = 0; i < valid_count; i++)
    {
        shader_reflect_destroy(&reflections[i]);
    }

    return layout;
}


uint32_t shader_reflect_get_vertex_attributes(const ShaderReflection*             reflection,
                                               VkVertexInputAttributeDescription* out_attrs,
                                               uint32_t                           max_attrs,
                                               uint32_t                           binding)
{
    uint32_t count  = MIN(reflection->vertex_input_count, max_attrs);
    uint32_t offset = 0;

    // Sort by location first (simple bubble sort)
    ReflectedVertexInput sorted[SHADER_REFLECT_MAX_INPUTS];
    memcpy(sorted, reflection->vertex_inputs, count * sizeof(ReflectedVertexInput));

    for(uint32_t i = 0; i < count; i++)
    {
        for(uint32_t j = i + 1; j < count; j++)
        {
            if(sorted[j].location < sorted[i].location)
            {
                ReflectedVertexInput tmp = sorted[i];
                sorted[i]                = sorted[j];
                sorted[j]                = tmp;
            }
        }
    }

    // Calculate offsets and fill output
    for(uint32_t i = 0; i < count; i++)
    {
        out_attrs[i] = (VkVertexInputAttributeDescription){
            .location = sorted[i].location,
            .binding  = binding,
            .format   = sorted[i].format,
            .offset   = offset
        };

        // Calculate size based on format
        uint32_t size = 0;
        switch(sorted[i].format)
        {
            case VK_FORMAT_R32_SFLOAT:          size = 4;  break;
            case VK_FORMAT_R32G32_SFLOAT:       size = 8;  break;
            case VK_FORMAT_R32G32B32_SFLOAT:    size = 12; break;
            case VK_FORMAT_R32G32B32A32_SFLOAT: size = 16; break;
            case VK_FORMAT_R32_SINT:            size = 4;  break;
            case VK_FORMAT_R32G32_SINT:         size = 8;  break;
            case VK_FORMAT_R32G32B32_SINT:      size = 12; break;
            case VK_FORMAT_R32G32B32A32_SINT:   size = 16; break;
            case VK_FORMAT_R32_UINT:            size = 4;  break;
            case VK_FORMAT_R32G32_UINT:         size = 8;  break;
            case VK_FORMAT_R32G32B32_UINT:      size = 12; break;
            case VK_FORMAT_R32G32B32A32_UINT:   size = 16; break;
            default:                             size = 4;  break;
        }

        offset += size;
    }

    return count;
}


void shader_reflect_print(const ShaderReflection* reflection)
{
    log_info("=== Shader Reflection ===");
    log_info("Stage: 0x%x", reflection->stage);
    log_info("Entry Point: %s", reflection->entry_point ? reflection->entry_point : "(null)");

    if(reflection->stage == VK_SHADER_STAGE_COMPUTE_BIT)
    {
        log_info("Local Size: %u x %u x %u",
                 reflection->local_size_x,
                 reflection->local_size_y,
                 reflection->local_size_z);
    }

    log_info("Descriptor Sets: %u", reflection->set_count);
    for(uint32_t s = 0; s < reflection->set_count; s++)
    {
        const ReflectedDescriptorSet* set = &reflection->sets[s];
        log_info("  Set %u: %u bindings", set->set_index, set->binding_count);

        for(uint32_t b = 0; b < set->binding_count; b++)
        {
            const ReflectedBinding* binding = &set->bindings[b];
            log_info("    Binding %u: type=%u count=%u stages=0x%x name=%s",
                     binding->binding,
                     binding->descriptor_type,
                     binding->descriptor_count,
                     binding->stage_flags,
                     binding->name ? binding->name : "(null)");
        }
    }

    log_info("Push Constants: %u", reflection->push_constant_count);
    for(uint32_t p = 0; p < reflection->push_constant_count; p++)
    {
        const ReflectedPushConstant* push = &reflection->push_constants[p];
        log_info("  Push %u: offset=%u size=%u stages=0x%x name=%s",
                 p,
                 push->offset,
                 push->size,
                 push->stage_flags,
                 push->name ? push->name : "(null)");
    }

    if(reflection->vertex_input_count > 0)
    {
        log_info("Vertex Inputs: %u", reflection->vertex_input_count);
        for(uint32_t i = 0; i < reflection->vertex_input_count; i++)
        {
            const ReflectedVertexInput* input = &reflection->vertex_inputs[i];
            log_info("  Location %u: format=%u name=%s",
                     input->location,
                     input->format,
                     input->name ? input->name : "(null)");
        }
    }

    log_info("=========================");
}
