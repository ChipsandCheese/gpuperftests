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
#include "vulkan_memory.h"
#include "vulkan_shader.h"
#include "vulkan_compute_pipeline.h"
#include "vulkan_command_buffer.h"

#ifdef VULKAN_COMMAND_BUFFER_TRACE
#define TRACE_COMMAND(format, ...)   TRACE("[COMMAND] " format, __VA_ARGS__)
#else
#define TRACE_COMMAND(format, ...)
#endif

#define GET_BIT(bit_array, index)   ((bit_array[(index) / 64] >> ((index) % 64)) & 1ULL)
#define SET_BIT(bit_array, index)   bit_array[(index) / 64] |= 1ULL << ((index) % 64)
#define CLEAR_BIT(bit_array, index) bit_array[(index) / 64] &= ~(1ULL << ((index) % 64))

test_status VulkanCommandBufferInitializeOnQueue(vulkan_device *device, uint32_t queue_family_index, uint32_t command_buffer_count, vulkan_command_buffer *command_handle) {
    TRACE_COMMAND("Initializing command buffer 0x%p (device: 0x%p, buffer count: %lu)\n", command_handle, device, command_buffer_count);
    if (command_handle == NULL || command_buffer_count == 0) {
        return TEST_INVALID_PARAMETER;
    }
    if (queue_family_index >= device->queue_family_count) {
        return TEST_INVALID_PARAMETER;
    }
    test_status status = TEST_OK;
    command_handle->device = device;
    command_handle->command_pool = VK_NULL_HANDLE;
    command_handle->command_buffers = NULL;
    command_handle->command_buffer_count = command_buffer_count;
    command_handle->queue_family = &(device->queue_families[queue_family_index]);

    VkCommandPoolCreateInfo command_pool_create_info = {0};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.pNext = NULL;
    command_pool_create_info.queueFamilyIndex = command_handle->queue_family->family_index;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult res = vkCreateCommandPool(device->device, &command_pool_create_info, NULL, &(command_handle->command_pool));
    if (!VULKAN_SUCCESS(res)) {
        status = TEST_VK_COMMAND_POOL_CREATION_ERROR;
        goto error;
    }
    command_handle->command_buffers = malloc(command_buffer_count * sizeof(VkCommandBuffer));
    if (command_handle->command_buffers == NULL) {
        status = TEST_OUT_OF_MEMORY;
        goto cleanup_command_pool;
    }
    uint32_t bitmask_entries = (command_buffer_count / 64) + 1;
    command_handle->command_buffer_use_bitmask = malloc(bitmask_entries * sizeof(uint64_t));
    if (command_handle->command_buffer_use_bitmask == NULL) {
        status = TEST_OUT_OF_MEMORY;
        goto free_command_buffers;
    }
    memset(command_handle->command_buffers, 0, command_buffer_count * sizeof(VkCommandBuffer));
    memset(command_handle->command_buffer_use_bitmask, 0, bitmask_entries * sizeof(uint64_t));

    VkCommandBufferAllocateInfo command_buffer_allocate_info = {0};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.pNext = NULL;
    command_buffer_allocate_info.commandPool = command_handle->command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = command_buffer_count;

    res = vkAllocateCommandBuffers(device->device, &command_buffer_allocate_info, command_handle->command_buffers);
    if (!VULKAN_SUCCESS(res)) {
        status = TEST_VK_COMMAND_BUFFER_ALLOCATION_ERROR;
        goto free_bitmasks;
    }
    return TEST_OK;
free_bitmasks:
    free(command_handle->command_buffer_use_bitmask);
    command_handle->command_buffer_use_bitmask = NULL;
free_command_buffers:
    free(command_handle->command_buffers);
    command_handle->command_buffers = NULL;
cleanup_command_pool:
    vkDestroyCommandPool(device->device, command_handle->command_pool, NULL);
error:
    return status;
}

test_status VulkanCommandBufferInitialize(vulkan_device *device, uint32_t command_buffer_count, vulkan_command_buffer *command_handle) {
    return VulkanCommandBufferInitializeOnQueue(device, 0, command_buffer_count, command_handle);
}

test_status VulkanCommandBufferCleanUp(vulkan_command_buffer *command_handle) {
    TRACE_COMMAND("Cleaning up command buffer 0x%p\n", command_handle);
    if (command_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    for (uint32_t i = 0; i < command_handle->command_buffer_count; i++) {
        if (GET_BIT(command_handle->command_buffer_use_bitmask, i) == 0) {
            SET_BIT(command_handle->command_buffer_use_bitmask, i);
            vkResetCommandBuffer(command_handle->command_buffers[i], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        }
    }
    if (command_handle->command_buffer_use_bitmask != NULL) {
        free(command_handle->command_buffer_use_bitmask);
        command_handle->command_buffer_use_bitmask = NULL;
    }
    if (command_handle->command_buffers != NULL) {
        vkFreeCommandBuffers(command_handle->device->device, command_handle->command_pool, command_handle->command_buffer_count, command_handle->command_buffers);
        free(command_handle->command_buffers);
        command_handle->command_buffers = NULL;
    }
    if (command_handle->command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(command_handle->device->device, command_handle->command_pool, NULL);
    }
    return TEST_OK;
}

test_status VulkanCommandSequenceInitializeMultiQueue(vulkan_command_buffer *command_handle, uint32_t recording_flags, uint32_t queue_count, vulkan_command_sequence *sequence_handle) {
    TRACE_COMMAND("Initializing command sequence 0x%p (command buffer: 0x%p, flags: %08x)\n", sequence_handle, command_handle, recording_flags);
    if (command_handle == NULL || sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (((recording_flags & (VULKAN_COMMAND_SEQUENCE_MULTI_QUEUE_USE)) == 0) && queue_count > 1) {
        return TEST_VK_COMMAND_SEQUENCE_NOT_MULTI_QUEUE;
    }
    sequence_handle->command_pool = command_handle;
    sequence_handle->command_buffer = VK_NULL_HANDLE;
    sequence_handle->command_buffer_index = 0;
    sequence_handle->flags = recording_flags;
    sequence_handle->queue_count = queue_count;
    sequence_handle->wait_fences = malloc(queue_count * sizeof(VkFence));
    if (sequence_handle->wait_fences == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    return TEST_OK;
}

test_status VulkanCommandSequenceInitialize(vulkan_command_buffer *command_handle, uint32_t recording_flags, vulkan_command_sequence *sequence_handle) {
    if (command_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    uint32_t queue_count = 1;
    if ((recording_flags & (VULKAN_COMMAND_SEQUENCE_MULTI_QUEUE_USE)) != 0) {
        queue_count = command_handle->queue_family->queue_count;
    }
    return VulkanCommandSequenceInitializeMultiQueue(command_handle, recording_flags, queue_count, sequence_handle);
}

test_status VulkanCommandBufferStart(vulkan_command_sequence *sequence_handle) {
    TRACE_COMMAND("Starting command sequence 0x%p\n", sequence_handle);
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (sequence_handle->command_buffer != VK_NULL_HANDLE) {
        return TEST_VK_COMMAND_SEQUENCE_ALREADY_STARTED;
    }
    for (uint32_t i = 0; i < sequence_handle->command_pool->command_buffer_count; i++) {
        if (GET_BIT(sequence_handle->command_pool->command_buffer_use_bitmask, i) == 0) {
            SET_BIT(sequence_handle->command_pool->command_buffer_use_bitmask, i);
            sequence_handle->command_buffer = sequence_handle->command_pool->command_buffers[i];
            sequence_handle->command_buffer_index = i;
            break;
        }
    }
    if (sequence_handle->command_buffer == VK_NULL_HANDLE) {
        return TEST_VK_OUT_OF_COMMAND_BUFFERS;
    }
    VkFenceCreateInfo fence_create_info = {0};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.pNext = NULL;
    fence_create_info.flags = 0;

    VkResult res;
    for (uint32_t i = 0; i < sequence_handle->queue_count; i++) {
        res = vkCreateFence(sequence_handle->command_pool->device->device, &fence_create_info, NULL, &(sequence_handle->wait_fences[i]));
        if (!VULKAN_SUCCESS(res)) {
            CLEAR_BIT(sequence_handle->command_pool->command_buffer_use_bitmask, sequence_handle->command_buffer_index);
            return TEST_VK_FENCE_CREATION_ERROR;
        }
    }
    VkCommandBufferUsageFlags flags = 0;
    if ((sequence_handle->flags & VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION) != 0) {
        flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    }
    if ((sequence_handle->flags & VULKAN_COMMAND_SEQUENCE_MULTI_QUEUE_USE) != 0) {
        flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    }
    VkCommandBufferBeginInfo command_buffer_begin_info = {0};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.pNext = NULL;
    command_buffer_begin_info.pInheritanceInfo = NULL;
    command_buffer_begin_info.flags = flags;

    res = vkBeginCommandBuffer(sequence_handle->command_buffer, &command_buffer_begin_info);
    if (!VULKAN_SUCCESS(res)) {
        CLEAR_BIT(sequence_handle->command_pool->command_buffer_use_bitmask, sequence_handle->command_buffer_index);
        for (uint32_t i = 0; i < sequence_handle->queue_count; i++) {
            vkDestroyFence(sequence_handle->command_pool->device->device, sequence_handle->wait_fences[i], NULL);
        }
        free(sequence_handle->wait_fences);
        sequence_handle->wait_fences = NULL;
        return TEST_VK_COMMAND_BUFFER_BEGIN_ERROR;
    }
    return TEST_OK;
}

test_status VulkanCommandBufferEnd(vulkan_command_sequence *sequence_handle) {
    TRACE_COMMAND("Ending command sequence 0x%p\n", sequence_handle);
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (sequence_handle->command_buffer == VK_NULL_HANDLE) {
        return TEST_VK_COMMAND_SEQUENCE_NOT_STARTED;
    }
    VkResult res = vkEndCommandBuffer(sequence_handle->command_buffer);
    VULKAN_RETFAIL(res, TEST_VK_COMMAND_BUFFER_END_ERROR);
    return TEST_OK;
}

test_status VulkanCommandBufferReset(vulkan_command_sequence *sequence_handle) {
    TRACE_COMMAND("Resetting command sequence 0x%p\n", sequence_handle);
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (sequence_handle->command_buffer == VK_NULL_HANDLE) {
        /* For convenience: Don't return an error if this is a self-resetting command buffer */
        return ((sequence_handle->flags & VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION) != 0) ? TEST_OK : TEST_VK_COMMAND_SEQUENCE_NOT_STARTED;
    }
    VkResult res = vkResetCommandBuffer(sequence_handle->command_buffer, 0);
    VULKAN_RETFAIL(res, TEST_VK_COMMAND_BUFFER_RESET_ERROR);

    for (uint32_t i = 0; i < sequence_handle->queue_count; i++) {
        vkDestroyFence(sequence_handle->command_pool->device->device, sequence_handle->wait_fences[i], NULL);
        sequence_handle->wait_fences[i] = VK_NULL_HANDLE;
    }
    CLEAR_BIT(sequence_handle->command_pool->command_buffer_use_bitmask, sequence_handle->command_buffer_index);
    sequence_handle->command_buffer = VK_NULL_HANDLE;
    return TEST_OK;
}

test_status VulkanCommandBufferSubmitOnQueues(vulkan_command_sequence *sequence_handle, uint32_t queue_offset, uint32_t queue_count) {
    TRACE_COMMAND("Submitting command sequence 0x%p on queues %lu-%lu\n", sequence_handle, queue_offset, queue_offset + queue_count - 1);
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (sequence_handle->command_buffer == VK_NULL_HANDLE) {
        return TEST_VK_COMMAND_SEQUENCE_NOT_STARTED;
    }
    if (queue_count > sequence_handle->queue_count) {
        return TEST_VK_INVALID_QUEUE_INDEX;
    }
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &(sequence_handle->command_buffer);

    for (uint32_t i = 0; i < queue_count; i++) {
        VkResult res = vkQueueSubmit(sequence_handle->command_pool->device->queues[sequence_handle->command_pool->queue_family->queue_offset + queue_offset + i], 1, &submit_info, sequence_handle->wait_fences[i]);
        VULKAN_RETFAIL(res, TEST_VK_QUEUE_SUBMIT_ERROR);
    }
    return TEST_OK;
}
test_status VulkanCommandBufferSubmitOnQueue(vulkan_command_sequence *sequence_handle, uint32_t queue_index) {
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    uint32_t queue_count;
    uint32_t queue_offset;
    if (queue_index == VULKAN_QUEUES_ALL) {
        if ((sequence_handle->flags & VULKAN_COMMAND_SEQUENCE_MULTI_QUEUE_USE) == 0) {
            return TEST_VK_COMMAND_SEQUENCE_NOT_MULTI_QUEUE;
        }
        queue_count = sequence_handle->command_pool->queue_family->queue_count;
        queue_offset = 0;
    } else {
        queue_count = 1;
        queue_offset = queue_index;
    }
    return VulkanCommandBufferSubmitOnQueues(sequence_handle, queue_offset, queue_count);
}

test_status VulkanCommandBufferSubmit(vulkan_command_sequence *sequence_handle) {
    return VulkanCommandBufferSubmitOnQueue(sequence_handle, 0);
}

test_status VulkanCommandBufferWaitOnQueues(vulkan_command_sequence *sequence_handle, size_t max_wait_nanoseconds, uint32_t queue_offset, uint32_t queue_count) {
    TEST_UNUSED(queue_offset);
#ifdef VULKAN_COMMAND_BUFFER_TRACE
    if (max_wait_nanoseconds == VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE) {
        TRACE_COMMAND("Waiting for command sequence 0x%p on queues %lu-%lu (timeout: INFINITE)\n", sequence_handle, queue_offset, queue_offset + queue_count - 1);
    } else {
        TRACE_COMMAND("Waiting for command sequence 0x%p on queues %lu-%lu (timeout: %lluns = %llums)\n", sequence_handle, queue_offset, queue_offset + queue_count - 1, max_wait_nanoseconds, max_wait_nanoseconds / 1000000ULL);
    }
#endif
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (sequence_handle->command_buffer == VK_NULL_HANDLE) {
        return TEST_VK_COMMAND_SEQUENCE_NOT_STARTED;
    }
    if (queue_count > sequence_handle->queue_count) {
        return TEST_VK_INVALID_QUEUE_INDEX;
    }
    while (true) {
        VkResult res;
        if (max_wait_nanoseconds == 0) {
            res = vkGetFenceStatus(sequence_handle->command_pool->device->device, sequence_handle->wait_fences[0]);
        } else {
            res = vkWaitForFences(sequence_handle->command_pool->device->device, queue_count, sequence_handle->wait_fences, VK_TRUE, (max_wait_nanoseconds == VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE) ? 1000000ULL : max_wait_nanoseconds);
        }
        if (!VULKAN_SUCCESS(res)) {
            if (res == VK_TIMEOUT) {
                if (max_wait_nanoseconds != VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE) {
                    return TEST_VK_WAIT_FOR_FENCES_TIMEOUT;
                }
            } else if (res == VK_NOT_READY) {
                return TEST_VK_WAIT_FOR_FENCES_NOT_READY;
            } else {
                return TEST_VK_WAIT_FOR_FENCES_ERROR;
            }
        } else {
            break;
        }
    }
    if ((sequence_handle->flags & VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION) != 0) {
        return VulkanCommandBufferReset(sequence_handle);
    } else {
        vkResetFences(sequence_handle->command_pool->device->device, queue_count, sequence_handle->wait_fences);
        return TEST_OK;
    }
}

test_status VulkanCommandBufferWaitOnQueue(vulkan_command_sequence *sequence_handle, size_t max_wait_nanoseconds, uint32_t queue_index) {
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    uint32_t queue_count;
    uint32_t queue_offset;
    if (queue_index == VULKAN_QUEUES_ALL) {
        queue_count = sequence_handle->command_pool->queue_family->queue_count;
        queue_offset = 0;
    } else {
        queue_count = 1;
        queue_offset = queue_index;
    }
    return VulkanCommandBufferWaitOnQueues(sequence_handle, max_wait_nanoseconds, queue_offset, queue_count);
}

test_status VulkanCommandBufferWait(vulkan_command_sequence *sequence_handle, size_t max_wait_nanoseconds) {
    return VulkanCommandBufferWaitOnQueue(sequence_handle, max_wait_nanoseconds, 0);
}

test_status VulkanCommandBufferStartSingleOnQueue(vulkan_device *device, uint32_t queue_family_index, vulkan_command_buffer_singlerun *command_buffer_singlerun_handle) {
    if (command_buffer_singlerun_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    test_status status = VulkanCommandBufferInitializeOnQueue(device, queue_family_index, 1, &(command_buffer_singlerun_handle->command_buffer));
    TEST_RETFAIL(status);
    status = VulkanCommandSequenceInitialize(&(command_buffer_singlerun_handle->command_buffer), VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION, &(command_buffer_singlerun_handle->sequence));
    if (!TEST_SUCCESS(status)) {
        VulkanCommandBufferCleanUp(&(command_buffer_singlerun_handle->command_buffer));
        return status;
    }
    status = VulkanCommandBufferStart(&(command_buffer_singlerun_handle->sequence));
    if (!TEST_SUCCESS(status)) {
        VulkanCommandBufferCleanUp(&(command_buffer_singlerun_handle->command_buffer));
    }
    return status;
}

test_status VulkanCommandBufferStartSingle(vulkan_device *device, vulkan_command_buffer_singlerun *command_buffer_singlerun_handle) {
    return VulkanCommandBufferStartSingleOnQueue(device, 0, command_buffer_singlerun_handle);
}

test_status VulkanCommandBufferFinishSingle(vulkan_command_buffer_singlerun *command_buffer_singlerun_handle, uint64_t timeout) {
    if (command_buffer_singlerun_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    test_status status = VulkanCommandBufferEnd(&(command_buffer_singlerun_handle->sequence));
    if (!TEST_SUCCESS(status)) {
        goto reset_command_sequence;
    }
    status = VulkanCommandBufferSubmit(&(command_buffer_singlerun_handle->sequence));
    if (!TEST_SUCCESS(status)) {
        goto reset_command_sequence;
    }
    status = VulkanCommandBufferWait(&(command_buffer_singlerun_handle->sequence), timeout);
    if (!TEST_SUCCESS(status)) {
        goto reset_command_sequence;
    }
reset_command_sequence:
    VulkanCommandBufferReset(&(command_buffer_singlerun_handle->sequence));
    VulkanCommandBufferCleanUp(&(command_buffer_singlerun_handle->command_buffer));
    return status;
}

test_status VulkanCommandBufferAbortSingle(vulkan_command_buffer_singlerun *command_buffer_singlerun_handle) {
    if (command_buffer_singlerun_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VulkanCommandBufferEnd(&(command_buffer_singlerun_handle->sequence));
    VulkanCommandBufferReset(&(command_buffer_singlerun_handle->sequence));
    VulkanCommandBufferCleanUp(&(command_buffer_singlerun_handle->command_buffer));
    return TEST_OK;
}

test_status VulkanCommandBufferBindComputePipeline(vulkan_command_sequence *sequence_handle, vulkan_compute_pipeline *pipeline_handle) {
    TRACE_COMMAND("Binding compute pipeline to command sequence 0x%p (compute pipeline: 0x%p)\n", sequence_handle, pipeline_handle);
    if (sequence_handle == NULL || pipeline_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (sequence_handle->command_buffer == VK_NULL_HANDLE) {
        return TEST_VK_COMMAND_SEQUENCE_NOT_STARTED;
    }

    uint32_t set_count = (uint32_t)HelperArrayListSize(&(pipeline_handle->shader->descriptor_set_array));
    vkCmdBindPipeline(sequence_handle->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_handle->pipeline);
    vkCmdBindDescriptorSets(sequence_handle->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_handle->shader->pipeline_layout, 0, set_count, pipeline_handle->descriptor_sets, 0, NULL);
    return TEST_OK;
}

test_status VulkanCommandBufferDispatch(vulkan_command_sequence *sequence_handle, uint32_t x, uint32_t y, uint32_t z) {
    TRACE_COMMAND("Dispatching command sequence 0x%p (type: 3D, x: %lu, y: %lu, z: %lu)\n", sequence_handle, x, y, z);
    if (sequence_handle == NULL || (x * y * z) == 0) {
        return TEST_INVALID_PARAMETER;
    }
    if (sequence_handle->command_buffer == VK_NULL_HANDLE) {
        return TEST_VK_COMMAND_SEQUENCE_NOT_STARTED;
    }
    uint32_t max_x = sequence_handle->command_pool->device->physical_device->physical_properties.properties.limits.maxComputeWorkGroupCount[0];
    uint32_t max_y = sequence_handle->command_pool->device->physical_device->physical_properties.properties.limits.maxComputeWorkGroupCount[1];
    uint32_t max_z = sequence_handle->command_pool->device->physical_device->physical_properties.properties.limits.maxComputeWorkGroupCount[2];
    if (x > max_x || y > max_y || z > max_z) {
        return TEST_VK_DISPATCH_TOO_LARGE;
    }
    vkCmdDispatch(sequence_handle->command_buffer, x, y, z);
    return TEST_OK;
}

test_status VulkanCommandBufferCopySubregion(vulkan_command_sequence *sequence_handle, vulkan_region *source, size_t source_offset, vulkan_region *destination, size_t destination_offset, size_t copy_size) {
    TRACE_COMMAND("Adding CopySubregion to command sequence 0x%p (source: 0x%p, source offset: %llu, destination: 0x%p, destination offset: %llu, copy size: %llu)\n", sequence_handle, source, source_offset, destination, destination_offset, copy_size);
    if (sequence_handle == NULL || source == NULL || destination == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VkDeviceSize common_size = min(source->size - source_offset, destination->size - destination_offset);
    if (common_size < copy_size) {
        return TEST_INVALID_PARAMETER;
    }

    VkBufferCopy buffer_copy = {0};
    buffer_copy.size = copy_size == VULKAN_COMMAND_COPY_SUBREGION_WHOLE_REGION ? common_size : copy_size;
    buffer_copy.srcOffset = source->offset + source_offset;
    buffer_copy.dstOffset = destination->offset + destination_offset;

    vkCmdCopyBuffer(sequence_handle->command_buffer, source->backing_buffer, destination->backing_buffer, 1, &buffer_copy);
    return TEST_OK;
}

test_status VulkanCommandBufferCopyRegion(vulkan_command_sequence *sequence_handle, vulkan_region *source, vulkan_region *destination) {
    return VulkanCommandBufferCopySubregion(sequence_handle, source, 0, destination, 0, VULKAN_COMMAND_COPY_SUBREGION_WHOLE_REGION);
}

test_status VulkanCommandBufferTransitionImageLayout(vulkan_command_sequence *sequence_handle, vulkan_texture *texture_handle, VkImageLayout new_layout) {
    if (sequence_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VkPipelineStageFlags src_stage;
    VkPipelineStageFlags dst_stage;

    VkImageMemoryBarrier image_memory_barrier = {0};
    image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    image_memory_barrier.pNext = NULL;
    image_memory_barrier.oldLayout = texture_handle->current_layout;
    image_memory_barrier.newLayout = new_layout;
    image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    image_memory_barrier.image = texture_handle->backing_image;
    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_memory_barrier.subresourceRange.baseMipLevel = 0;
    image_memory_barrier.subresourceRange.levelCount = texture_handle->image_mips;
    image_memory_barrier.subresourceRange.baseArrayLayer = 0;
    image_memory_barrier.subresourceRange.layerCount = 1;
    if (texture_handle->current_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        image_memory_barrier.srcAccessMask = 0;
        image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (texture_handle->current_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        return TEST_VK_UNSUPPORTED_IMAGE_LAYOUT_TRANSITION;
    }

    vkCmdPipelineBarrier(sequence_handle->command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
    return TEST_OK;
}

test_status VulkanCommandBufferCopyBufferSubimage(vulkan_command_sequence *sequence_handle, vulkan_region *source, size_t source_offset, vulkan_texture *destination, int32_t destination_offset_x, int32_t destination_offset_y, int32_t destination_offset_z, uint32_t destination_width, uint32_t destination_height, uint32_t destination_depth, uint32_t mip_level) {
    if (sequence_handle == NULL || source == NULL || destination == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VkBufferImageCopy buffer_image_copy = {0};
    buffer_image_copy.bufferOffset = source->offset + source_offset;
    buffer_image_copy.bufferRowLength = 0;
    buffer_image_copy.bufferImageHeight = 0;
    buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffer_image_copy.imageSubresource.mipLevel = mip_level;
    buffer_image_copy.imageSubresource.baseArrayLayer = 0;
    buffer_image_copy.imageSubresource.layerCount = 1;
    buffer_image_copy.imageOffset.x = destination_offset_x;
    buffer_image_copy.imageOffset.y = destination_offset_y;
    buffer_image_copy.imageOffset.z = destination_offset_z;
    buffer_image_copy.imageExtent.width = destination_width;
    buffer_image_copy.imageExtent.height = destination_height;
    buffer_image_copy.imageExtent.depth = destination_depth;

    vkCmdCopyBufferToImage(sequence_handle->command_buffer, source->backing_buffer, destination->backing_image, destination->current_layout, 1, &buffer_image_copy);
    return TEST_OK;
}

test_status VulkanCommandBufferCopyBufferImage(vulkan_command_sequence *sequence_handle, vulkan_region *source, vulkan_texture *destination, uint32_t mip_level) {
    return VulkanCommandBufferCopyBufferSubimage(sequence_handle, source, 0, destination, 0, 0, 0, destination->image_width, destination->image_height, destination->image_depth, mip_level);
}