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

#ifndef VULKAN_COMPUTE_PIPELINE_H
#define VULKAN_COMPUTE_PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vulkan_compute_pipeline_t {
    vulkan_device *device;
    vulkan_shader *shader;
    VkPipeline pipeline;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSet *descriptor_sets;
    uint32_t *descriptor_set_indices;
} vulkan_compute_pipeline;

test_status VulkanComputePipelineInitialize(vulkan_shader *compute_shader, const char *entrypoint, vulkan_compute_pipeline *pipeline_handle);
test_status VulkanComputePipelineCleanUp(vulkan_compute_pipeline *pipeline_handle);
test_status VulkanComputePipelineBind(vulkan_compute_pipeline *pipeline_handle, vulkan_memory *memory_handle, const char *binding_name);

#ifdef __cplusplus
}
#endif
#endif