// Copyright (c) 2021 - 2022, Nemes <nemes@nemez.net>
// SPDX-License-Identifier: MIT
// 
// MIT License
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software andassociated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, andto permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice andthis permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "vulkan_helper.h"
#include "logger.h"
#include "vulkan_shader.h"
#include "vulkan_memory.h"
#include "vulkan_compute_pipeline.h"

#ifdef VULKAN_COMPUTE_PIPELINE_TRACE
#define TRACE_COMPUTE(format, ...)   TRACE("[COMPUTE] " format, __VA_ARGS__)
#else
#define TRACE_COMPUTE(format, ...)
#endif

test_status VulkanComputePipelineInitialize(vulkan_shader *compute_shader, const char *entrypoint, vulkan_compute_pipeline *pipeline_handle) {
    TRACE_COMPUTE("Initializing compute pipeline 0x%p (compute shader: 0x%p, entrypoint: \"%s\")\n", pipeline_handle, compute_shader, entrypoint);
    if (compute_shader == NULL || pipeline_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (compute_shader->pipeline_layout == VK_NULL_HANDLE) {
        return TEST_VK_SHADER_DESCRIPTORS_NOT_CREATED;
    }
    if ((compute_shader->shader_stages & VK_SHADER_STAGE_COMPUTE_BIT) == 0) {
        return TEST_VK_COMPUTE_PIPELINE_SHADER_STAGE_MISMATCH;
    }
    pipeline_handle->pipeline = VK_NULL_HANDLE;
    pipeline_handle->descriptor_pool = VK_NULL_HANDLE;

    VkComputePipelineCreateInfo compute_pipeline_create_info = {0};
    compute_pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_create_info.pNext = NULL;
    compute_pipeline_create_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_create_info.stage.pName = NULL;
    compute_pipeline_create_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_create_info.stage.module = compute_shader->shader_module;
    compute_pipeline_create_info.stage.pName = entrypoint;
    compute_pipeline_create_info.layout = compute_shader->pipeline_layout;

    VkResult res = vkCreateComputePipelines(compute_shader->device->device, VK_NULL_HANDLE, 1, &compute_pipeline_create_info, NULL, &(pipeline_handle->pipeline));
    VULKAN_RETFAIL(res, TEST_VK_COMPUTE_PIPELINE_CREATION_ERROR);

    helper_arraylist descriptor_pool_sizes;
    test_status status = HelperArrayListInitialize(&descriptor_pool_sizes, sizeof(VkDescriptorPoolSize));
    if (!TEST_SUCCESS(status)) {
        goto cleanup_pipeline;
    }
    size_t set_count = HelperArrayListSize(&(compute_shader->descriptor_set_array));
    for (size_t i = 0; i < set_count; i++) {
        vulkan_shader_descriptor_set *descriptor_set = (vulkan_shader_descriptor_set *)HelperArrayListGet(&(compute_shader->descriptor_set_array), i);
        size_t descriptor_count = HelperArrayListSize(&(descriptor_set->descriptor_array));

        for (size_t j = 0; j < descriptor_count; j++) {
            vulkan_shader_descriptor *descriptor = (vulkan_shader_descriptor *)HelperArrayListGet(&(descriptor_set->descriptor_array), j);
            bool found_pool_size = false;

            size_t collected_count = HelperArrayListSize(&descriptor_pool_sizes);
            for (size_t k = 0; k < collected_count; k++) {
                VkDescriptorPoolSize *pool_size = (VkDescriptorPoolSize *)HelperArrayListGet(&descriptor_pool_sizes, k);

                if (pool_size->type == descriptor->descriptor_type) {
                    found_pool_size = true;
                    pool_size->descriptorCount++;
                }
            }
            if (!found_pool_size) {
                VkDescriptorPoolSize pool_size = {0};
                pool_size.type = descriptor->descriptor_type;
                pool_size.descriptorCount = 1;

                status = HelperArrayListAdd(&descriptor_pool_sizes, &pool_size, sizeof(pool_size), NULL);
                if (!TEST_SUCCESS(status)) {
                    HelperArrayListClean(&descriptor_pool_sizes);
                    goto cleanup_pipeline;
                }
            }
        }
    }

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {0};
    descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_create_info.pNext = NULL;
    descriptor_pool_create_info.poolSizeCount = (uint32_t)HelperArrayListSize(&descriptor_pool_sizes);
    descriptor_pool_create_info.pPoolSizes = (VkDescriptorPoolSize *)HelperArrayListRawData(&descriptor_pool_sizes);
    descriptor_pool_create_info.maxSets = (uint32_t)set_count;

    res = vkCreateDescriptorPool(compute_shader->device->device, &descriptor_pool_create_info, NULL, &(pipeline_handle->descriptor_pool));
    HelperArrayListClean(&descriptor_pool_sizes);
    if (!VULKAN_SUCCESS(res)) {
        status = TEST_VK_DESCRIPTOR_POOL_CREATION_ERROR;
        goto cleanup_pipeline;
    }

    pipeline_handle->descriptor_sets = malloc(set_count * sizeof(VkDescriptorSet));
    if (pipeline_handle->descriptor_sets == NULL) {
        status = TEST_OUT_OF_MEMORY;
        goto cleanup_descriptor_pool;
    }
    pipeline_handle->descriptor_set_indices = malloc(set_count * sizeof(uint32_t));
    if (pipeline_handle->descriptor_set_indices == NULL) {
        status = TEST_OUT_OF_MEMORY;
        goto free_descriptor_sets;
    }
    memset(pipeline_handle->descriptor_sets, 0, set_count * sizeof(VkDescriptorSet));
    memset(pipeline_handle->descriptor_set_indices, 0, set_count * sizeof(uint32_t));

    for (size_t i = 0; i < set_count; i++) {
        vulkan_shader_descriptor_set *descriptor_set = (vulkan_shader_descriptor_set *)HelperArrayListGet(&(compute_shader->descriptor_set_array), i);

        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {0};
        descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptor_set_allocate_info.pNext = NULL;
        descriptor_set_allocate_info.descriptorPool = pipeline_handle->descriptor_pool;
        descriptor_set_allocate_info.descriptorSetCount = 1;
        descriptor_set_allocate_info.pSetLayouts = &(descriptor_set->descriptor_set_layout);

        res = vkAllocateDescriptorSets(compute_shader->device->device, &descriptor_set_allocate_info, &(pipeline_handle->descriptor_sets[i]));
        if (!VULKAN_SUCCESS(res)) {
            status = TEST_VK_DESCRIPTOR_SET_ALLOCATION_ERROR;
            goto free_descriptor_set_indices;
        }
        pipeline_handle->descriptor_set_indices[i] = descriptor_set->index;
    }

    pipeline_handle->device = compute_shader->device;
    pipeline_handle->shader = compute_shader;
    return TEST_OK;
free_descriptor_set_indices:
    free(pipeline_handle->descriptor_set_indices);
    pipeline_handle->descriptor_set_indices = NULL;
free_descriptor_sets:
    free(pipeline_handle->descriptor_sets);
    pipeline_handle->descriptor_sets = NULL;
cleanup_descriptor_pool:
    vkDestroyDescriptorPool(pipeline_handle->device->device, pipeline_handle->descriptor_pool, NULL);
cleanup_pipeline:
    vkDestroyPipeline(pipeline_handle->device->device, pipeline_handle->pipeline, NULL);
    return status;
}

test_status VulkanComputePipelineCleanUp(vulkan_compute_pipeline *pipeline_handle) {
    TRACE_COMPUTE("Cleaning up compute pipeline 0x%p\n", pipeline_handle);
    if (pipeline_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (pipeline_handle->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(pipeline_handle->device->device, pipeline_handle->pipeline, NULL);
    }
    if (pipeline_handle->descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(pipeline_handle->device->device, pipeline_handle->descriptor_pool, NULL);
    }
    if (pipeline_handle->descriptor_set_indices != NULL) {
        free(pipeline_handle->descriptor_set_indices);
        pipeline_handle->descriptor_set_indices = NULL;
    }
    if (pipeline_handle->descriptor_sets != NULL) {
        free(pipeline_handle->descriptor_sets);
        pipeline_handle->descriptor_sets = NULL;
    }
    return TEST_OK;
}

test_status VulkanComputePipelineBind(vulkan_compute_pipeline *pipeline_handle, vulkan_memory *memory_handle, const char *binding_name) {
    TRACE_COMPUTE("Binding \"%s\" (compute pipeline: 0x%p, memory: 0x%p)\n", binding_name, pipeline_handle, memory_handle);
    if (pipeline_handle == NULL || memory_handle == NULL || binding_name == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    vulkan_shader_descriptor_set *descriptor_set = NULL;
    vulkan_shader_descriptor *descriptor = VulkanShaderGetDescriptor(pipeline_handle->shader, binding_name, &descriptor_set);
    if (descriptor == NULL) {
        return TEST_VK_BINDING_UNKNOWN_DESCRIPTOR;
    }

    VkDescriptorSet vk_descriptor_set = VK_NULL_HANDLE;
    size_t set_count = HelperArrayListSize(&(pipeline_handle->shader->descriptor_set_array));
    for (size_t i = 0; i < set_count; i++) {
        if (pipeline_handle->descriptor_set_indices[i] == descriptor_set->index) {
            vk_descriptor_set = pipeline_handle->descriptor_sets[i];
            break;
        }
    }
    if (vk_descriptor_set == VK_NULL_HANDLE) {
        return TEST_VK_BINDING_UNKNOWN_DESCRIPTOR;
    }
    
    VkWriteDescriptorSet write_descriptor_set;
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.pNext = NULL;
    write_descriptor_set.dstSet = vk_descriptor_set;
    write_descriptor_set.dstBinding = descriptor->index;
    write_descriptor_set.dstArrayElement = 0;
    write_descriptor_set.descriptorType = descriptor->descriptor_type;
    write_descriptor_set.descriptorCount = 1;

    if (descriptor->descriptor_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        vulkan_texture *texture = VulkanMemoryGetTexture(memory_handle, binding_name);
        if (texture == NULL) {
            return TEST_VK_BINDING_UNKNOWN_TEXTURE;
        }

        VkDescriptorImageInfo descriptor_image_info;
        descriptor_image_info.imageLayout = texture->current_layout;
        descriptor_image_info.imageView = texture->image_view;
        descriptor_image_info.sampler = texture->image_sampler;

        write_descriptor_set.pBufferInfo = NULL;
        write_descriptor_set.pImageInfo = &descriptor_image_info;
        write_descriptor_set.pTexelBufferView = NULL;

        vkUpdateDescriptorSets(pipeline_handle->device->device, 1, &write_descriptor_set, 0, NULL);
    } else {
        vulkan_region *region = VulkanMemoryGetRegion(memory_handle, binding_name);
        if (region == NULL) {
            return TEST_VK_BINDING_UNKNOWN_MEMORY_REGION;
        }

        VkDescriptorBufferInfo descriptor_buffer_info;
        descriptor_buffer_info.buffer = region->backing_buffer;
        descriptor_buffer_info.offset = region->offset;
        descriptor_buffer_info.range = region->size;

        write_descriptor_set.pBufferInfo = &descriptor_buffer_info;
        write_descriptor_set.pImageInfo = NULL;
        write_descriptor_set.pTexelBufferView = NULL;

        vkUpdateDescriptorSets(pipeline_handle->device->device, 1, &write_descriptor_set, 0, NULL);
    }

    return TEST_OK;
}