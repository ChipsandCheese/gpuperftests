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
#include "vulkan_staging.h"

// 512MiB means we need a 256MiB/s link to the GPU in order to TDR, should be safe for anything that can actually run Vulkan
#define VULKAN_STAGING_MAXIMUM_TRANSFER_SIZE    (256*1024*1024)

#ifdef VULKAN_STAGING_TRACE
#define TRACE_STAGING(format, ...)   TRACE("[STAGING] " format, __VA_ARGS__)
#else
#define TRACE_STAGING(format, ...)
#endif

static test_status _VulkanStagingInitializeGeneric(void *destination_object, bool is_texture, size_t data_size, uint32_t subregion_width, uint32_t subregion_height, uint32_t subregion_depth, vulkan_staging *staging_handle);

test_status VulkanStagingInitializeRange(vulkan_region *destination_region, vulkan_staging *staging_handle) {
    if (destination_region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    return VulkanStagingInitializeSubrange(destination_region, destination_region->size, staging_handle);
}

test_status VulkanStagingInitializeSubrange(vulkan_region *destination_region, size_t subregion_size, vulkan_staging *staging_handle) {
    return _VulkanStagingInitializeGeneric((void *)destination_region, false, subregion_size, 0, 0, 0, staging_handle);
}

test_status VulkanStagingInitializeImage(vulkan_texture *destination_texture, size_t source_data_size, vulkan_staging *staging_handle) {
    if (destination_texture == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    return VulkanStagingInitializeSubimage(destination_texture, source_data_size, destination_texture->image_width, destination_texture->image_height, destination_texture->image_depth, staging_handle);
}

test_status VulkanStagingInitializeSubimage(vulkan_texture *destination_texture, size_t source_data_size, uint32_t width, uint32_t height, uint32_t depth, vulkan_staging *staging_handle) {
    return _VulkanStagingInitializeGeneric((void *)destination_texture, true, source_data_size, width, height, depth, staging_handle);
}

test_status VulkanStagingCleanUp(vulkan_staging *staging_handle) {
    TRACE_STAGING("Cleaning up staging helper 0x%p\n", staging_handle);
    if (staging_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (staging_handle->staging_buffer_base != NULL) {
        vulkan_region *staging_region = VulkanMemoryGetRegion(&(staging_handle->staging_memory), "staging buffer");
        VulkanMemoryUnmap(staging_region);
        staging_handle->staging_buffer_base = NULL;
    }
    VulkanMemoryFreeBuffersAndBacking(&(staging_handle->staging_memory));
    VulkanMemoryCleanUp(&(staging_handle->staging_memory));
    VulkanCommandBufferCleanUp(&(staging_handle->command_buffer));
    return TEST_OK;
}

test_status VulkanStagingTransferSubrange(vulkan_staging *staging_handle, size_t buffer_offset) {
    TRACE_STAGING("Transfering staging buffer data of 0x%p (offset: %llu)\n", staging_handle, buffer_offset);
    if (staging_handle == NULL || staging_handle->staging_buffer_base == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    vulkan_region *staging_region = VulkanMemoryGetRegion(&(staging_handle->staging_memory), "staging buffer");
    VulkanMemoryUnmap(staging_region);
    staging_handle->staging_buffer_base = NULL;

    test_status status = TEST_OK;
    vulkan_command_sequence command_sequence;
    // Chunk up the transfer so we don't TDR during big transfers or on slow uplink systems.
    size_t remaining_size = staging_region->size;
    size_t added_offset = 0;
    while (remaining_size > 0) {
        status = VulkanCommandSequenceInitialize(&(staging_handle->command_buffer), VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION, &command_sequence);
        TEST_RETFAIL(status);
        status = VulkanCommandBufferStart(&command_sequence);
        TEST_RETFAIL(status);
        size_t current_size = remaining_size >= VULKAN_STAGING_MAXIMUM_TRANSFER_SIZE ? VULKAN_STAGING_MAXIMUM_TRANSFER_SIZE : remaining_size;
        status = VulkanCommandBufferCopySubregion(&command_sequence, staging_region, added_offset, staging_handle->destination_region, buffer_offset + added_offset, current_size);
        if (!TEST_SUCCESS(status)) {
            goto reset_command_sequence;
        }
        status = VulkanCommandBufferEnd(&command_sequence);
        if (!TEST_SUCCESS(status)) {
            goto reset_command_sequence;
        }
        status = VulkanCommandBufferSubmit(&command_sequence);
        if (!TEST_SUCCESS(status)) {
            goto reset_command_sequence;
        }
        status = VulkanCommandBufferWait(&command_sequence, VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE);
        if (!TEST_SUCCESS(status)) {
            goto reset_command_sequence;
        }
        remaining_size -= current_size;
        added_offset += current_size;
    }
    staging_handle->staging_buffer_base = VulkanMemoryMap(staging_region);
    if (staging_handle->staging_buffer_base == NULL) {
        return TEST_VK_MEMORY_MAPPING_ERROR;
    }
    return TEST_OK;
reset_command_sequence:
    VulkanCommandBufferReset(&command_sequence);
    return status;
}

test_status VulkanStagingTransferRange(vulkan_staging *staging_handle) {
    return VulkanStagingTransferSubrange(staging_handle, 0);
}

test_status VulkanStagingTransferSubimage(vulkan_staging *staging_handle, int32_t texture_offset_x, int32_t texture_offset_y, int32_t texture_offset_z, uint32_t mip_level) {
    TRACE_STAGING("Transfering staging texture data of 0x%p (offset: [%li, %li, %li])\n", staging_handle, texture_offset_x, texture_offset_y, texture_offset_z);
    if (staging_handle == NULL || staging_handle->staging_buffer_base == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    vulkan_region *staging_region = VulkanMemoryGetRegion(&(staging_handle->staging_memory), "staging buffer");
    VulkanMemoryUnmap(staging_region);
    staging_handle->staging_buffer_base = NULL;

    vulkan_command_sequence command_sequence;
    test_status status = VulkanCommandSequenceInitialize(&(staging_handle->command_buffer), VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION, &command_sequence);
    TEST_RETFAIL(status);
    status = VulkanCommandBufferStart(&command_sequence);
    TEST_RETFAIL(status);
    status = VulkanCommandBufferCopyBufferSubimage(&command_sequence, staging_region, 0, staging_handle->destination_texture, texture_offset_x, texture_offset_y, texture_offset_z, staging_handle->subregion_width, staging_handle->subregion_height, staging_handle->subregion_depth, mip_level);
    if (!TEST_SUCCESS(status)) {
        goto reset_command_sequence;
    }
    status = VulkanCommandBufferEnd(&command_sequence);
    if (!TEST_SUCCESS(status)) {
        goto reset_command_sequence;
    }
    status = VulkanCommandBufferSubmit(&command_sequence);
    if (!TEST_SUCCESS(status)) {
        goto reset_command_sequence;
    }
    status = VulkanCommandBufferWait(&command_sequence, VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE);
    if (!TEST_SUCCESS(status)) {
        goto reset_command_sequence;
    }
    staging_handle->staging_buffer_base = VulkanMemoryMap(staging_region);
    if (staging_handle->staging_buffer_base == NULL) {
        return TEST_VK_MEMORY_MAPPING_ERROR;
    }
    return TEST_OK;
reset_command_sequence:
    VulkanCommandBufferReset(&command_sequence);
    return status;
}

test_status VulkanStagingTransferImage(vulkan_staging *staging_handle, uint32_t mip_level) {
    return VulkanStagingTransferSubimage(staging_handle, 0, 0, 0, mip_level);
}

void *VulkanStagingGetBuffer(vulkan_staging *staging_handle) {
    if (staging_handle == NULL) {
        return NULL;
    }
    return staging_handle->staging_buffer_base;
}

static test_status _VulkanStagingInitializeGeneric(void *destination_object, bool is_texture, size_t data_size, uint32_t subregion_width, uint32_t subregion_height, uint32_t subregion_depth, vulkan_staging *staging_handle) {
#ifdef VULKAN_STAGING_TRACE
    if (is_texture) {
        TRACE_STAGING("Initializing staging helper 0x%p (texture: 0x%p, size: %llux%llux%llu)\n", staging_handle, destination_object, subregion_width, subregion_height, subregion_depth);
    } else {
        TRACE_STAGING("Initializing staging helper 0x%p (buffer: 0x%p, size: %llu)\n", staging_handle, destination_object, subregion_width);
    }
#endif
    if (destination_object == NULL || staging_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    vulkan_device *device = is_texture ? (((vulkan_texture *)destination_object)->memory_pool->device) : (((vulkan_region *)destination_object)->memory_pool->device);

    test_status status = VulkanMemoryInitialize(device, VULKAN_MEMORY_VISIBLE | VULKAN_MEMORY_HOST_LOCAL, &(staging_handle->staging_memory));
    TEST_RETFAIL(status);
    status = VulkanMemoryAddRegion(&(staging_handle->staging_memory), data_size, "staging buffer", VULKAN_REGION_STORAGE, VULKAN_REGION_TRANSFER_SOURCE);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory;
    }
    status = VulkanMemoryAllocateBacking(&(staging_handle->staging_memory));
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory;
    }
    vulkan_region *staging_region = VulkanMemoryGetRegion(&(staging_handle->staging_memory), "staging buffer");
    staging_handle->staging_buffer_base = VulkanMemoryMap(staging_region);
    if (staging_handle->staging_buffer_base == NULL) {
        goto free_memory;
    }
    status = VulkanCommandBufferInitialize(device, 1, &(staging_handle->command_buffer));
    if (!TEST_SUCCESS(status)) {
        goto unmap_memory;
    }
    if (is_texture) {
        staging_handle->destination_region = NULL;
        staging_handle->destination_texture = (vulkan_texture *)destination_object;
        staging_handle->subregion_width = (uint32_t)subregion_width;
        staging_handle->subregion_height = subregion_height;
        staging_handle->subregion_depth = subregion_depth;
    } else {
        staging_handle->destination_region = (vulkan_region *)destination_object;
        staging_handle->destination_texture = NULL;
        staging_handle->subregion_width = 0;
        staging_handle->subregion_height = 0;
        staging_handle->subregion_depth = 0;
    }
    return TEST_OK;
unmap_memory:
    VulkanMemoryUnmap(staging_region);
    staging_handle->staging_buffer_base = NULL;
free_memory:
    VulkanMemoryFreeBuffersAndBacking(&(staging_handle->staging_memory));
cleanup_memory:
    VulkanMemoryCleanUp(&(staging_handle->staging_memory));
    return status;
}