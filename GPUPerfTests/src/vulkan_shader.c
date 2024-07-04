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
#include "resources.h"

#ifdef VULKAN_SHADER_TRACE
#define TRACE_SHADER(format, ...)   TRACE("[SHADER] " format, __VA_ARGS__)
#else
#define TRACE_SHADER(format, ...)
#endif

test_status VulkanShaderInitializeFromFile(vulkan_device *device, const char *filepath, VkShaderStageFlags shader_stages, vulkan_shader *shader_handle) {
    TRACE_SHADER("Loading shader source file \"%s\"\n", filepath);
    if (filepath == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    char *shader_file = NULL;
    size_t shader_file_size;
    test_status status = ResourcesGetShaderFile(filepath, (const char **)&shader_file, &shader_file_size);
    TEST_RETFAIL(status);
    status = VulkanShaderInitialize(device, shader_file, shader_file_size, shader_stages, shader_handle);
    return status;
}

test_status VulkanShaderInitialize(vulkan_device *device, const char *shader_source, size_t shader_source_size, VkShaderStageFlags shader_stages, vulkan_shader *shader_handle) {
    TRACE_SHADER("Initializing shader 0x%p (source at 0x%p, size: %lu, stages: %08x)\n", shader_handle, shader_source, shader_source_size, shader_stages);
    if (shader_handle == NULL || device == NULL || shader_source == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    shader_handle->device = device;
    shader_handle->shader_stages = shader_stages;
    shader_handle->shader_module = VK_NULL_HANDLE;
    shader_handle->pipeline_layout = VK_NULL_HANDLE;
    shader_handle->descriptor_set_layouts = NULL;
    test_status status = HelperArrayListInitialize(&(shader_handle->descriptor_set_array), sizeof(vulkan_shader_descriptor_set));
    TEST_RETFAIL(status);

    VkShaderModuleCreateInfo shader_module_create_info = {0};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.pNext = NULL;
    shader_module_create_info.pCode = (const uint32_t *)shader_source;
    shader_module_create_info.codeSize = shader_source_size;

    VkResult res = vkCreateShaderModule(device->device, &shader_module_create_info, NULL, &(shader_handle->shader_module));
    if (!VULKAN_SUCCESS(res)) {
        HelperArrayListClean(&(shader_handle->descriptor_set_array));
        return TEST_VK_SHADER_MODULE_CREATION_ERROR;
    }
    return TEST_OK;
}

test_status VulkanShaderCleanUp(vulkan_shader *shader_handle) {
    TRACE_SHADER("Cleaning up shader 0x%p\n", shader_handle);
    if (shader_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (shader_handle->pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(shader_handle->device->device, shader_handle->pipeline_layout, NULL);
    }
    size_t set_array_size = HelperArrayListSize(&(shader_handle->descriptor_set_array));
    for (size_t i = 0; i < set_array_size; i++) {
        vulkan_shader_descriptor_set *descriptor_set = (vulkan_shader_descriptor_set *)HelperArrayListGet(&(shader_handle->descriptor_set_array), i);
        if (descriptor_set != NULL) {
            if (descriptor_set->descriptor_set_layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(shader_handle->device->device, descriptor_set->descriptor_set_layout, NULL);
            }
            HelperArrayListClean(&(descriptor_set->descriptor_array));
        }
    }
    if (shader_handle->shader_module != VK_NULL_HANDLE) {
        vkDestroyShaderModule(shader_handle->device->device, shader_handle->shader_module, NULL);
    }
    free(shader_handle->descriptor_set_layouts);
    return HelperArrayListClean(&(shader_handle->descriptor_set_array));
}

test_status VulkanShaderAddDescriptor(vulkan_shader *shader_handle, const char *descriptor_name, uint32_t descriptor_type, uint32_t descriptor_set_index, uint32_t descriptor_index) {
    return VulkanShaderAddFixedSizeDescriptor(shader_handle, 0, descriptor_name, descriptor_type, descriptor_set_index, descriptor_index);
}

test_status VulkanShaderAddFixedSizeDescriptor(vulkan_shader *shader_handle, size_t size, const char *descriptor_name, uint32_t descriptor_type, uint32_t descriptor_set_index, uint32_t descriptor_index) {
    TRACE_SHADER("Adding descriptor to shader 0x%p (name: \"%s\", size: %lu, type: %lu, set: %lu, index: %lu)\n", shader_handle, descriptor_name, size, descriptor_type, descriptor_set_index, descriptor_index);
    if (shader_handle == NULL || descriptor_name == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (shader_handle->pipeline_layout != VK_NULL_HANDLE) {
        return TEST_VK_SHADER_DESCRIPTORS_ALREADY_CREATED;
    }
    size_t name_length = strlen(descriptor_name);
    if (name_length == 0 || name_length >= VULKAN_DESCRIPTOR_MAX_NAME_LENGTH) {
        return TEST_INVALID_PARAMETER;
    }
    vulkan_shader_descriptor descriptor;
    strcpy(descriptor.name, descriptor_name);
    descriptor.region_size = size;
    descriptor.index = descriptor_index;

    switch (descriptor_type) {
    case VULKAN_BINDING_STORAGE:
        descriptor.descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        break;
    case VULKAN_BINDING_UNIFORM:
        descriptor.descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;
    case VULKAN_BINDING_SAMPLER:
        descriptor.descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;
    default:
        return TEST_INVALID_PARAMETER;
    }
    vulkan_shader_descriptor_set *found_set = NULL;
    size_t set_array_size = HelperArrayListSize(&(shader_handle->descriptor_set_array));
    for (size_t i = 0; i < set_array_size; i++) {
        vulkan_shader_descriptor_set *descriptor_set = (vulkan_shader_descriptor_set *)HelperArrayListGet(&(shader_handle->descriptor_set_array), i);
        if (descriptor_set != NULL && descriptor_set->index == descriptor_set_index) {
            found_set = descriptor_set;
        }
    }
    if (found_set == NULL) {
        vulkan_shader_descriptor_set new_set;
        new_set.index = descriptor_set_index;
        new_set.descriptor_set_layout = VK_NULL_HANDLE;
        test_status status = HelperArrayListInitialize(&(new_set.descriptor_array), sizeof(vulkan_shader_descriptor));
        TEST_RETFAIL(status);
        size_t new_set_array_index = 0;
        status = HelperArrayListAdd(&(shader_handle->descriptor_set_array), &new_set, sizeof(new_set), &new_set_array_index);
        if (!TEST_SUCCESS(status)) {
            HelperArrayListClean(&(new_set.descriptor_array));
            return status;
        }
        found_set = HelperArrayListGet(&(shader_handle->descriptor_set_array), new_set_array_index);
    }

    return HelperArrayListAdd(&(found_set->descriptor_array), &descriptor, sizeof(descriptor), NULL);
}

test_status VulkanShaderCreateDescriptorSets(vulkan_shader *shader_handle) {
    TRACE_SHADER("Creating descriptor sets of shader 0x%p\n", shader_handle);
    if (shader_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (shader_handle->pipeline_layout != VK_NULL_HANDLE) {
        return TEST_VK_SHADER_DESCRIPTORS_ALREADY_CREATED;
    }
    VkResult res = VK_SUCCESS;
    size_t set_count = HelperArrayListSize(&(shader_handle->descriptor_set_array));

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {0};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.pNext = NULL;
    pipeline_layout_create_info.pushConstantRangeCount = 0;
    pipeline_layout_create_info.pPushConstantRanges = NULL;
    pipeline_layout_create_info.setLayoutCount = (uint32_t)set_count;

    if (set_count > 0) {
        TRACE_SHADER("Creating pipeline layout with %lu sets\n", set_count);
        shader_handle->descriptor_set_layouts = (VkDescriptorSetLayout *)malloc(set_count * sizeof(VkDescriptorSetLayout));
        if (shader_handle->descriptor_set_layouts == NULL) {
            return TEST_OUT_OF_MEMORY;
        }

        for (size_t i = 0; i < set_count; i++) {
            vulkan_shader_descriptor_set *descriptor_set = (vulkan_shader_descriptor_set *)HelperArrayListGet(&(shader_handle->descriptor_set_array), i);
            size_t descriptor_count = HelperArrayListSize(&(descriptor_set->descriptor_array));
            if (descriptor_count == 0) {
                return TEST_VK_SHADER_ZERO_SIZED_DESCRIPTOR_SET;
            }
            VkDescriptorSetLayoutBinding *descriptor_set_layout_bindings = malloc(descriptor_count * sizeof(VkDescriptorSetLayoutBinding));
            if (descriptor_set_layout_bindings == NULL) {
                return TEST_OUT_OF_MEMORY;
            }
            memset(descriptor_set_layout_bindings, 0, descriptor_count * sizeof(VkDescriptorSetLayoutBinding));
            for (size_t j = 0; j < descriptor_count; j++) {
                vulkan_shader_descriptor *descriptor = (vulkan_shader_descriptor *)HelperArrayListGet(&(descriptor_set->descriptor_array), j);

                descriptor_set_layout_bindings[j].binding = descriptor->index;
                descriptor_set_layout_bindings[j].descriptorCount = 1;
                descriptor_set_layout_bindings[j].descriptorType = descriptor->descriptor_type;
                descriptor_set_layout_bindings[j].pImmutableSamplers = NULL;
                descriptor_set_layout_bindings[j].stageFlags = shader_handle->shader_stages;
            }

            VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {0};
            descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptor_set_layout_create_info.pNext = NULL;
            descriptor_set_layout_create_info.bindingCount = (uint32_t)descriptor_count;
            descriptor_set_layout_create_info.pBindings = descriptor_set_layout_bindings;

            TRACE_SHADER("Creating descriptor set layout (set: %lu, descriptors: %lu)\n", i, descriptor_count);
            res = vkCreateDescriptorSetLayout(shader_handle->device->device, &descriptor_set_layout_create_info, NULL, &(descriptor_set->descriptor_set_layout));
            free(descriptor_set_layout_bindings);

            if (!VULKAN_SUCCESS(res)) {
                return TEST_VK_DESCRIPTOR_SET_LAYOUT_CREATION_ERROR;
            }
            shader_handle->descriptor_set_layouts[i] = descriptor_set->descriptor_set_layout;
        }
        pipeline_layout_create_info.pSetLayouts = shader_handle->descriptor_set_layouts;
    } else {
        TRACE_SHADER("Creating empty pipeline layout\n");
        shader_handle->descriptor_set_layouts = NULL;
        pipeline_layout_create_info.pSetLayouts = NULL;
    }

    res = vkCreatePipelineLayout(shader_handle->device->device, &pipeline_layout_create_info, NULL, &(shader_handle->pipeline_layout));
    VULKAN_RETFAIL(res, TEST_VK_PIPELINE_LAYOUT_CREATION_ERROR);
    return TEST_OK;
}

vulkan_shader_descriptor *VulkanShaderGetDescriptor(vulkan_shader *shader_handle, const char *descriptor_name, vulkan_shader_descriptor_set **descriptor_set) {
    if (shader_handle == NULL || descriptor_name == NULL) {
        return NULL;
    }
    size_t set_count = HelperArrayListSize(&(shader_handle->descriptor_set_array));
    for (size_t i = 0; i < set_count; i++) {
        vulkan_shader_descriptor_set *parent_descriptor_set = (vulkan_shader_descriptor_set *)HelperArrayListGet(&(shader_handle->descriptor_set_array), i);

        size_t descriptor_count = HelperArrayListSize(&(parent_descriptor_set->descriptor_array));
        for (size_t j = 0; j < descriptor_count; j++) {
            vulkan_shader_descriptor *descriptor = (vulkan_shader_descriptor *)HelperArrayListGet(&(parent_descriptor_set->descriptor_array), j);

            if (strcmp(descriptor->name, descriptor_name) == 0) {
                if (descriptor_set != NULL) {
                    *descriptor_set = parent_descriptor_set;
                }
                return descriptor;
            }
        }
    }
    return NULL;
}