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

#include "main.h"
#include "logger.h"
#include "helper.h"
#include "vulkan_helper.h"
#include "vulkan_memory.h"
#include "buffer_filler.h"
#include "latency_helper.h"

typedef struct latency_helper_buffer_filler_lru_t {
    latency_helper_lru *lru;
    uint64_t capacity;
} latency_helper_buffer_filler_lru;

static test_status _LatencyHelperLRUBufferFillerFunc(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);

test_status LatencyHelperLRUInitialize(latency_helper_lru *lru, uint32_t stride_bytes) {
    if (lru == NULL || (stride_bytes % sizeof(uint32_t)) != 0) {
        return TEST_INVALID_PARAMETER;
    }
    uint32_t slots = stride_bytes / sizeof(uint32_t);
    lru->stride = stride_bytes;

    uint32_t memory_required = slots - 1;
    bool *data = malloc(memory_required * sizeof(bool));
    if (data == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    lru->lru_table = malloc(slots * sizeof(uint32_t));
    if (lru->lru_table == NULL) {
        free(data);
        return TEST_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < memory_required; i++) {
        data[i] = true;
    }
    uint32_t last_number = 0;
    for (uint32_t i = 0; i < slots; i++) {
        uint32_t depth = slots / 2;
        uint32_t flag_offset = 0;
        uint32_t number = 0;
        while (flag_offset < memory_required) {
            bool flag = data[flag_offset];
            data[flag_offset] = !flag;
            if (flag) {
                flag_offset += depth;
                number += depth;
            } else {
                flag_offset++;
            }
            if (depth == 1) {
                lru->lru_table[last_number] = number;
                last_number = number;
                break;
            } else {
                depth /= 2;
            }
        }
    }
    /*
    for (uint32_t i = 0; i < slots; i++) {
        INFO("LRU #%llu: %lu\n", i, (*order_list)[i]);
    }*/

    free(data);
    return TEST_OK;
}

test_status LatencyHelperLRUCleanUp(latency_helper_lru *lru) {
    if (lru == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    free(lru->lru_table);
    return TEST_OK;
}

test_status LatencyHelperLRUFillSubregion(latency_helper_lru *lru, vulkan_region *region, size_t size) {
    if (lru == NULL || region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    latency_helper_buffer_filler_lru filler_data;
    filler_data.lru = lru;
    filler_data.capacity = size;

    return BufferFillerGenericOffset(region, size, 0, _LatencyHelperLRUBufferFillerFunc, sizeof(uint32_t), &filler_data, NULL, NULL);
}

test_status LatencyHelperLRUFillRegion(latency_helper_lru *lru, vulkan_region *region) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    return LatencyHelperLRUFillSubregion(lru, region, region->size);
}

uint64_t LatencyHelperLRUGetHopCount(latency_helper_lru *lru, vulkan_region *region) {
    if (lru == NULL || region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    return region->size / sizeof(uint32_t);
}

uint32_t LatencyHelperLRUGetStartingOffset(latency_helper_lru *lru, vulkan_region *region, uint32_t desired_index) {
    if (lru == NULL || region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    uint32_t stride_size = (uint32_t)(lru->stride / sizeof(uint32_t));
    uint32_t region_strides = (uint32_t)(region->size / stride_size);
    uint32_t loops = desired_index / region_strides;
    uint32_t remainder_offset = desired_index % region_strides;
    uint32_t current_stride_offset = 0;

    for (uint32_t i = 0; i < loops; i++) {
        current_stride_offset = lru->lru_table[current_stride_offset];
    }

    return remainder_offset * stride_size + current_stride_offset;
}

static test_status _LatencyHelperLRUBufferFillerFunc(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_index);
    latency_helper_buffer_filler_lru *filler_data = (latency_helper_buffer_filler_lru *)custom_data;
    uint32_t *pointers = (uint32_t *)block_data;
    size_t pointer_count = block_size / sizeof(uint32_t);
    size_t absolute_block_offset = block_offset / sizeof(uint32_t);
    size_t total_size = filler_data->capacity / sizeof(uint32_t);
    size_t stride_size = filler_data->lru->stride / sizeof(uint32_t);

    for (size_t i = 0; i < pointer_count; i++) {
        size_t pointer_offset = absolute_block_offset + i;
        size_t new_pointer_offset = pointer_offset + stride_size;
        if (new_pointer_offset >= total_size) {
            size_t position_in_stride = pointer_offset % stride_size;
            new_pointer_offset = filler_data->lru->lru_table[position_in_stride];
            //INFO("PTR %llu (%llu): %llu\n", i, pointer_offset % stride_size, position_in_stride);
        }
        pointers[i] = (uint32_t)new_pointer_offset;
    }

    return TEST_OK;
}