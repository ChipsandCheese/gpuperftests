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

#ifndef VULKAN_COMMAND_BUFFER_H
#define VULKAN_COMMAND_BUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#define VULKAN_COMMAND_SEQUENCE_NORMAL              (0)
#define VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION (1 << 0)
#define VULKAN_COMMAND_SEQUENCE_MULTI_QUEUE_USE     (1 << 1)

#define VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE       (0xFFFFFFFFFFFFFFFFULL)

#define VULKAN_COMMAND_COPY_SUBREGION_WHOLE_REGION  (0)

typedef struct vulkan_command_buffer_t {
    vulkan_device *device;
    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;
    uint64_t *command_buffer_use_bitmask;
    uint32_t command_buffer_count;
    vulkan_queue_family *queue_family;
} vulkan_command_buffer;

typedef struct vulkan_command_sequence_t {
    vulkan_command_buffer *command_pool;
    VkCommandBuffer command_buffer;
    VkFence *wait_fences;
    uint32_t command_buffer_index;
    uint32_t flags;
    uint32_t queue_count;
} vulkan_command_sequence;

typedef struct vulkan_command_buffer_singlerun_t {
    vulkan_command_buffer command_buffer;
    vulkan_command_sequence sequence;
} vulkan_command_buffer_singlerun;

test_status VulkanCommandBufferInitializeOnQueue(vulkan_device *device, uint32_t queue_family_index, uint32_t command_buffer_count, vulkan_command_buffer *command_handle);
test_status VulkanCommandBufferInitialize(vulkan_device *device, uint32_t command_buffer_count, vulkan_command_buffer *command_handle);
test_status VulkanCommandBufferCleanUp(vulkan_command_buffer *command_handle);
test_status VulkanCommandSequenceInitialize(vulkan_command_buffer *command_handle, uint32_t recording_flags, vulkan_command_sequence *sequence_handle);
test_status VulkanCommandBufferStart(vulkan_command_sequence *sequence_handle);
test_status VulkanCommandBufferEnd(vulkan_command_sequence *sequence_handle);
test_status VulkanCommandBufferReset(vulkan_command_sequence *sequence_handle);
test_status VulkanCommandBufferSubmitOnQueues(vulkan_command_sequence *sequence_handle, uint32_t queue_offset, uint32_t queue_count);
test_status VulkanCommandBufferSubmitOnQueue(vulkan_command_sequence *sequence_handle, uint32_t queue_index);
test_status VulkanCommandBufferSubmit(vulkan_command_sequence *sequence_handle);
test_status VulkanCommandBufferWaitOnQueues(vulkan_command_sequence *sequence_handle, size_t max_wait_nanoseconds, uint32_t queue_offset, uint32_t queue_count);
test_status VulkanCommandBufferWaitOnQueue(vulkan_command_sequence *sequence_handle, size_t max_wait_nanoseconds, uint32_t queue_index);
test_status VulkanCommandBufferWait(vulkan_command_sequence *sequence_handle, size_t max_wait_nanoseconds);
test_status VulkanCommandBufferStartSingleOnQueue(vulkan_device *device, uint32_t queue_family_index, vulkan_command_buffer_singlerun *command_buffer_singlerun_handle);
test_status VulkanCommandBufferStartSingle(vulkan_device *device, vulkan_command_buffer_singlerun *command_buffer_singlerun_handle);
test_status VulkanCommandBufferFinishSingle(vulkan_command_buffer_singlerun *command_buffer_singlerun_handle, uint64_t timeout);
test_status VulkanCommandBufferAbortSingle(vulkan_command_buffer_singlerun *command_buffer_singlerun_handle);
test_status VulkanCommandBufferBindComputePipeline(vulkan_command_sequence *sequence_handle, vulkan_compute_pipeline *pipeline_handle);
test_status VulkanCommandBufferDispatch(vulkan_command_sequence *sequence_handle, uint32_t x, uint32_t y, uint32_t z);
test_status VulkanCommandBufferCopySubregion(vulkan_command_sequence *sequence_handle, vulkan_region *source, size_t source_offset, vulkan_region *destination, size_t destination_offset, size_t copy_size);
test_status VulkanCommandBufferCopyRegion(vulkan_command_sequence *sequence_handle, vulkan_region *source, vulkan_region *destination);
test_status VulkanCommandBufferTransitionImageLayout(vulkan_command_sequence *sequence_handle, vulkan_texture *texture_handle, VkImageLayout new_layout);
test_status VulkanCommandBufferCopyBufferSubimage(vulkan_command_sequence *sequence_handle, vulkan_region *source, size_t source_offset, vulkan_texture *destination, int32_t destination_offset_x, int32_t destination_offset_y, int32_t destination_offset_z, uint32_t destination_width, uint32_t destination_height, uint32_t destination_depth, uint32_t mip_level);
test_status VulkanCommandBufferCopyBufferImage(vulkan_command_sequence *sequence_handle, vulkan_region *source, vulkan_texture *destination, uint32_t mip_level);

#ifdef __cplusplus
}
#endif
#endif