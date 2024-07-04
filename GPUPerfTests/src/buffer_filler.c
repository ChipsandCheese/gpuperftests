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
#include "vulkan_helper.h"
#include "logger.h"
#include "vulkan_memory.h"
#include "vulkan_shader.h"
#include "vulkan_compute_pipeline.h"
#include "vulkan_command_buffer.h"
#include "vulkan_staging.h"
#include "buffer_filler.h"

#define BUFFER_FILLER_MINIMUM_BLOCK_SIZE            (1024 * 1024)
#define BUFFER_FILLER_MAXIMUM_BLOCK_SIZE            (16 * 1024 * 1024)

typedef struct buffer_filler_thread_t {
    void *block_data;
    size_t block_offset;
    size_t block_size;
    void *custom_data;
    uint32_t block_index;
    buffer_filler_block_func *entrypoint;
    test_status exit_code;
} buffer_filler_thread;

typedef struct buffer_filler_value_t {
    union {
        int8_t int8;
        int16_t int16;
        int32_t int32;
        int64_t int64;
        uint8_t uint8;
        uint16_t uint16;
        uint32_t uint32;
        uint64_t uint64;
        float float32;
        double float64;
    };
} buffer_filler_value;

typedef struct buffer_filler_rng_t {
    uint64_t global_seed;
    uint64_t *block_seeds;
} buffer_filler_rng;

static void _BufferFillerThreadFunc(uint32_t thread_id, void *data);

static test_status _BufferFillerValueI8(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueI16(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueI32(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueI64(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueU8(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueU16(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueU32(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueU64(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueF32(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerValueF64(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerRandomIntegers(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);
static test_status _BufferFillerRandomFloats(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);

static test_status _BufferFillerRandomPerBlockInitialize(uint32_t block_count, void *custom_data);
static test_status _BufferFillerRandomPerBlockCleanUp(uint32_t block_count, void *custom_data);

test_status BufferFillerZero(vulkan_region *region) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Clearing region 0x%p of size %llu bytes\n", region, region->size);
    buffer_filler_value val;
    val.int8 = 0;
    return BufferFillerGeneric(region, _BufferFillerValueI8, sizeof(int8_t), &val, NULL, NULL);
}

test_status BufferFillerValueByte(vulkan_region *region, int8_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with int8 value %i\n", region, region->size, value);
    buffer_filler_value val;
    val.int8 = value;
    return BufferFillerGeneric(region, _BufferFillerValueI8, sizeof(int8_t), &val, NULL, NULL);
}

test_status BufferFillerValueShort(vulkan_region *region, int16_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with int16 value %i\n", region, region->size, value);
    buffer_filler_value val;
    val.int16 = value;
    return BufferFillerGeneric(region, _BufferFillerValueI16, sizeof(int16_t), &val, NULL, NULL);
}

test_status BufferFillerValueInt(vulkan_region *region, int32_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with int32 value %li\n", region, region->size, value);
    buffer_filler_value val;
    val.int32 = value;
    return BufferFillerGeneric(region, _BufferFillerValueI32, sizeof(int32_t), &val, NULL, NULL);
}

test_status BufferFillerValueLong(vulkan_region *region, int64_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with int64 value %lli\n", region, region->size, value);
    buffer_filler_value val;
    val.int64 = value;
    return BufferFillerGeneric(region, _BufferFillerValueI64, sizeof(int64_t), &val, NULL, NULL);
}

test_status BufferFillerValueUByte(vulkan_region *region, uint8_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with uint8 value %u\n", region, region->size, value);
    buffer_filler_value val;
    val.uint8 = value;
    return BufferFillerGeneric(region, _BufferFillerValueU8, sizeof(uint8_t), &val, NULL, NULL);
}

test_status BufferFillerValueUShort(vulkan_region *region, uint16_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with uint16 value %u\n", region, region->size, value);
    buffer_filler_value val;
    val.uint16 = value;
    return BufferFillerGeneric(region, _BufferFillerValueU16, sizeof(uint16_t), &val, NULL, NULL);
}

test_status BufferFillerValueUInt(vulkan_region *region, uint32_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with uint32 value %lu\n", region, region->size, value);
    buffer_filler_value val;
    val.uint32 = value;
    return BufferFillerGeneric(region, _BufferFillerValueU32, sizeof(uint32_t), &val, NULL, NULL);
}

test_status BufferFillerValueULong(vulkan_region *region, uint64_t value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with uint64 value %llu\n", region, region->size, value);
    buffer_filler_value val;
    val.uint64 = value;
    return BufferFillerGeneric(region, _BufferFillerValueU64, sizeof(uint64_t), &val, NULL, NULL);
}

test_status BufferFillerValueFloat(vulkan_region *region, float value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with float32 value %f\n", region, region->size, value);
    buffer_filler_value val;
    val.float32 = value;
    return BufferFillerGeneric(region, _BufferFillerValueF32, sizeof(float), &val, NULL, NULL);
}

test_status BufferFillerValueDouble(vulkan_region *region, double value) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with float64 value %f\n", region, region->size, value);
    buffer_filler_value val;
    val.float64 = value;
    return BufferFillerGeneric(region, _BufferFillerValueF64, sizeof(double), &val, NULL, NULL);
}

test_status BufferFillerRandomIntegers(vulkan_region *region, uint64_t seed) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with random uint32s (seed: %llu)\n", region, region->size, seed);
    buffer_filler_rng data;
    data.global_seed = seed;
    data.block_seeds = NULL;
    return BufferFillerGeneric(region, _BufferFillerRandomIntegers, sizeof(uint32_t), &data, _BufferFillerRandomPerBlockInitialize, _BufferFillerRandomPerBlockCleanUp);
}

test_status BufferFillerRandomFloats(vulkan_region *region, uint64_t seed) {
    if (region == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    DEBUG("Filling region 0x%p of size %llu bytes with random float32s (seed: %llu)\n", region, region->size, seed);
    buffer_filler_rng data;
    data.global_seed = seed;
    data.block_seeds = NULL;
    return BufferFillerGeneric(region, _BufferFillerRandomFloats, sizeof(float), &data, _BufferFillerRandomPerBlockInitialize, _BufferFillerRandomPerBlockCleanUp);
}

test_status BufferFillerGeneric(vulkan_region *region, buffer_filler_block_func *block_function, size_t data_unit_size, void *custom_data, buffer_filler_prep_func *per_block_initialize, buffer_filler_prep_func *per_block_cleanup) {
    return BufferFillerGenericOffset(region, region->size, 0, block_function, data_unit_size, custom_data, per_block_initialize, per_block_cleanup);
}

test_status BufferFillerGenericOffset(vulkan_region * region, size_t region_size_override, size_t offset_override, buffer_filler_block_func * block_function, size_t data_unit_size, void *custom_data, buffer_filler_prep_func *per_block_initialize, buffer_filler_prep_func *per_block_cleanup) {
    if ((region_size_override + offset_override) > region->size && offset_override < region_size_override) {
        return TEST_INVALID_PARAMETER;
    }
    /* Check whether the region size fits data_unit_size sized blocks properly */
    if ((region_size_override / data_unit_size) * data_unit_size != region_size_override) {
        return TEST_INVALID_PARAMETER;
    }
    test_status status = TEST_OK;
    size_t available_memory = HelperGetFreeSystemMemory();

    if (available_memory == 0) {
        available_memory = BUFFER_FILLER_MINIMUM_BLOCK_SIZE;
    }
    available_memory = (available_memory / data_unit_size) * data_unit_size;
    size_t subregion_size = min(available_memory, (region_size_override - offset_override));
    size_t destination_size = region_size_override - offset_override;

    /* Try to allocate the biggest staging buffer we can */
    vulkan_staging staging;
    bool subregion_was_small = subregion_size <= BUFFER_FILLER_MINIMUM_BLOCK_SIZE;
    while (subregion_size > BUFFER_FILLER_MINIMUM_BLOCK_SIZE) {
        status = VulkanStagingInitializeSubrange(region, subregion_size, &staging);
        if (!TEST_SUCCESS(status)) {
            subregion_size /= 2;
        } else {
            break;
        }
    }
    /* All allocations until subregion_size > BUFFER_FILLER_MINIMUM_BLOCK_SIZE failed, try to allocate the current small subregion_size */
    if (!TEST_SUCCESS(status) || subregion_was_small) {
        status = VulkanStagingInitializeSubrange(region, subregion_size, &staging);
        if (!TEST_SUCCESS(status)) {
            return status;
        }
    }

    bool use_fallback_singlethreaded = false;
    size_t maximum_block_size = (BUFFER_FILLER_MAXIMUM_BLOCK_SIZE / data_unit_size) * data_unit_size;
    size_t max_blocks_per_pass = subregion_size / maximum_block_size;
    if ((max_blocks_per_pass * maximum_block_size) != subregion_size) {
        max_blocks_per_pass++;
    }
    size_t total_block_count = destination_size / maximum_block_size;
    if ((total_block_count * maximum_block_size) != destination_size) {
        total_block_count++;
    }
    if (per_block_initialize != NULL) {
        status = per_block_initialize((uint32_t)total_block_count, custom_data);
        if (!TEST_SUCCESS(status)) {
            VulkanStagingCleanUp(&staging);
            return status;
        }
    }
    
    helper_thread *thread_handles = NULL;
    buffer_filler_thread *thread_parameters = NULL;
    if (max_blocks_per_pass > 1) {
        thread_handles = malloc(max_blocks_per_pass * sizeof(helper_thread));
        if (thread_handles == NULL) {
            use_fallback_singlethreaded = true;
        } else {
            thread_parameters = malloc(max_blocks_per_pass * sizeof(buffer_filler_thread));
            if (thread_parameters == NULL) {
                free(thread_handles);
                use_fallback_singlethreaded = true;
            }
        }
    }

    uint32_t block_index = 0;
    for (size_t current_offset = 0; current_offset < destination_size; current_offset += subregion_size) {
        void *staging_buffer = VulkanStagingGetBuffer(&staging);
        size_t current_size = min(destination_size - current_offset, subregion_size);

        size_t block_offset = 0;
        uint32_t populated_blocks_this_cycle = 0;
        for (size_t i = 0; i < max_blocks_per_pass && block_offset < current_size; i++) {
            size_t block_size = min(current_size - block_offset, maximum_block_size);
            void *block_data = (void *)((size_t)staging_buffer + block_offset + offset_override);

            if (max_blocks_per_pass == 1 || use_fallback_singlethreaded) {
                status = block_function(block_data, current_offset + block_offset, block_size, block_index, custom_data);
                if (!TEST_SUCCESS(status)) {
                    goto error;
                }
            } else {
                thread_parameters[i].block_data = block_data;
                thread_parameters[i].block_offset = current_offset + block_offset;
                thread_parameters[i].block_size = block_size;
                thread_parameters[i].custom_data = custom_data;
                thread_parameters[i].entrypoint = block_function;
                thread_parameters[i].block_index = block_index;
                thread_parameters[i].exit_code = TEST_OK;
                thread_handles[i] = HelperCreateThread(_BufferFillerThreadFunc, &(thread_parameters[i]));
            }

            block_offset += maximum_block_size;
            populated_blocks_this_cycle++;
            block_index++;
        }
        if (max_blocks_per_pass > 1 && !use_fallback_singlethreaded) {
            HelperWaitForThreads(thread_handles, populated_blocks_this_cycle);

            for (size_t i = 0; i < populated_blocks_this_cycle; i++) {
                if (thread_handles[i] != 0) {
                    HelperCleanUpThread(thread_handles[i]);
                }
                if (!TEST_SUCCESS(thread_parameters[i].exit_code)) {
                    status = thread_parameters[i].exit_code;
                    goto error;
                }
            }
        }

        status = VulkanStagingTransferSubrange(&staging, current_offset + offset_override);
        if (!TEST_SUCCESS(status)) {
            break;
        }
    }
error:
    if (max_blocks_per_pass > 1 && !use_fallback_singlethreaded) {
        free(thread_parameters);
        free(thread_handles);
    }
    VulkanStagingCleanUp(&staging);
    if (per_block_cleanup != NULL) {
        test_status cleanup_status = per_block_cleanup((uint32_t)total_block_count, custom_data);
        if (!TEST_SUCCESS(status)) {
            return status;
        } else {
            return cleanup_status;
        }
    } else {
        return TEST_OK;
    }
}

static void _BufferFillerThreadFunc(uint32_t thread_id, void *data) {
    buffer_filler_thread *thread_data = (buffer_filler_thread *)data;
    thread_data->exit_code = thread_data->entrypoint(thread_data->block_data, thread_data->block_offset, thread_data->block_size, thread_data->block_index, thread_data->custom_data);
}

static test_status _BufferFillerValueI8(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;
    //DEBUG("Block Index: %lu\n", block_index);
    memset(block_data, value->int8, block_size);
    return TEST_OK;
}

static test_status _BufferFillerValueU8(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    uint8_t *data = (uint8_t *)block_data;
    uint8_t *end_data = (uint8_t *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->uint8;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueI16(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    int16_t *data = (int16_t *)block_data;
    int16_t *end_data = (int16_t *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->int16;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueU16(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    uint16_t *data = (uint16_t *)block_data;
    uint16_t *end_data = (uint16_t *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->uint16;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueI32(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    int32_t *data = (int32_t *)block_data;
    int32_t *end_data = (int32_t *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->int32;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueU32(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    uint32_t *data = (uint32_t *)block_data;
    uint32_t *end_data = (uint32_t *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->uint32;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueI64(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    int64_t *data = (int64_t *)block_data;
    int64_t *end_data = (int64_t *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->int64;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueU64(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    uint64_t *data = (uint64_t *)block_data;
    uint64_t *end_data = (uint64_t *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->uint64;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueF32(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    float *data = (float *)block_data;
    float *end_data = (float *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->float32;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerValueF64(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    TEST_UNUSED(block_index);
    double *data = (double *)block_data;
    double *end_data = (double *)((size_t)block_data + block_size);
    buffer_filler_value *value = (buffer_filler_value *)custom_data;

    while (data < end_data) {
        *data = value->float64;
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerRandomIntegers(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    buffer_filler_rng *rng_data = (buffer_filler_rng *)custom_data;
    uint64_t rng_state;
    HelperSeedRandom(&rng_state, rng_data->block_seeds[block_index]);

    uint32_t *data = (uint32_t *)block_data;
    uint32_t *end_data = (uint32_t *)((size_t)block_data + block_size);

    while (data < end_data) {
        *data = (uint32_t)HelperGenerateRandom(&rng_state);
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerRandomFloats(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data) {
    TEST_UNUSED(block_offset);
    buffer_filler_rng *rng_data = (buffer_filler_rng *)custom_data;
    uint64_t rng_state;
    HelperSeedRandom(&rng_state, rng_data->block_seeds[block_index]);

    float *data = (float *)block_data;
    float *end_data = (float *)((size_t)block_data + block_size);

    while (data < end_data) {
        *data = HelperGenerateRandomFloat(&rng_state);
        data++;
    }
    return TEST_OK;
}

static test_status _BufferFillerRandomPerBlockInitialize(uint32_t block_count, void *custom_data) {
    DEBUG("RNG Initializing per-block seeds (%lu blocks)\n", block_count);
    buffer_filler_rng *rng_data = (buffer_filler_rng *)custom_data;
    rng_data->block_seeds = malloc(block_count * sizeof(uint64_t));
    if (rng_data->block_seeds == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    uint64_t rng_state;
    HelperSeedRandom(&rng_state, rng_data->global_seed);
    for (uint32_t i = 0; i < block_count; i++) {
        rng_data->block_seeds[i] = HelperGenerateRandom(&rng_state);
    }
    return TEST_OK;
}

static test_status _BufferFillerRandomPerBlockCleanUp(uint32_t block_count, void *custom_data) {
    DEBUG("RNG Cleaning up per-block seeds (%lu blocks)\n", block_count);
    buffer_filler_rng *rng_data = (buffer_filler_rng *)custom_data;
    free(rng_data->block_seeds);
    return TEST_OK;
}