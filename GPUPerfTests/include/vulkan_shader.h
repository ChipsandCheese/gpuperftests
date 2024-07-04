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

#ifndef VULKAN_SHADER_H
#define VULKAN_SHADER_H

#ifdef __cplusplus
extern "C" {
#endif

#define VULKAN_BINDING_STORAGE              (0)
#define VULKAN_BINDING_UNIFORM              (1)
#define VULKAN_BINDING_SAMPLER              (2)

#define VULKAN_DESCRIPTOR_MAX_NAME_LENGTH   (32)

typedef struct vulkan_shader_t {
    vulkan_device *device;
    VkShaderModule shader_module;
    VkShaderStageFlags shader_stages;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSetLayout *descriptor_set_layouts;
    helper_arraylist descriptor_set_array;
} vulkan_shader;

typedef struct vulkan_shader_descriptor_set_t {
    VkDescriptorSetLayout descriptor_set_layout;
    uint32_t index;
    helper_arraylist descriptor_array;
} vulkan_shader_descriptor_set;

typedef struct vulkan_shader_descriptor_t {
    VkDescriptorType descriptor_type;
    size_t region_size;
    uint32_t index;
    char name[VULKAN_DESCRIPTOR_MAX_NAME_LENGTH];
} vulkan_shader_descriptor;

test_status VulkanShaderInitializeFromFile(vulkan_device *device, const char *filepath, VkShaderStageFlags shader_stages, vulkan_shader *shader_handle);
test_status VulkanShaderInitialize(vulkan_device *device, const char *shader_source, size_t shader_source_size, VkShaderStageFlags shader_stages, vulkan_shader *shader_handle);
test_status VulkanShaderCleanUp(vulkan_shader *shader_handle);
test_status VulkanShaderAddDescriptor(vulkan_shader *shader_handle, const char *descriptor_name, uint32_t descriptor_type, uint32_t descriptor_set_index, uint32_t descriptor_index);
test_status VulkanShaderAddFixedSizeDescriptor(vulkan_shader *shader_handle, size_t size, const char *descriptor_name, uint32_t descriptor_type, uint32_t descriptor_set_index, uint32_t descriptor_index);
test_status VulkanShaderCreateDescriptorSets(vulkan_shader *shader_handle);
vulkan_shader_descriptor *VulkanShaderGetDescriptor(vulkan_shader *shader_handle, const char *descriptor_name, vulkan_shader_descriptor_set **descriptor_set);

#ifdef __cplusplus
}
#endif
#endif