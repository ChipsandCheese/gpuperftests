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
#include "vulkan_helper.h"
#include "vulkan_runner.h"
#include "vulkan_memory.h"
#include "vulkan_shader.h"
#include "vulkan_compute_pipeline.h"
#include "vulkan_command_buffer.h"
#include "buffer_filler.h"
#include "latency_helper.h"
#include "tests/test_vk_latency.h"

#define VULKAN_LATENCY_TARGET_TIME_US               (250000)                                /* Target execution time to get accurate results */
#define VULKAN_LATENCY_HOP_STRIDE_BYTES             (512)                                   /* Maximum cache line size that can be tricked (region must be a multiple of this) - must match value in shader */
#define VULKAN_LATENCY_HOPS_PER_CYCLE               (64)                                    /* Pointer fetches per loop cycle to minimize loop logic overhead */
#define VULKAN_LATENCY_STARTING_HOPS                (1024 / VULKAN_LATENCY_HOPS_PER_CYCLE)  /* Minimum amount of fetches to execute */
#define VULKAN_LATENCY_POINTER_SIZE                 (sizeof(uint32_t))                      /* Size of our beloved pointer - must match value in shader */
#define VULKAN_LATENCY_BACKOFF_THRESHOLD            (1.2f)
#define VULKAN_LATENCY_COVERAGE_MULTIPLE            (2)

#define VULKAN_LATENCY_TEST_TYPE_VECTOR             (0)
#define VULKAN_LATENCY_TEST_TYPE_SCALAR             (1)

typedef struct vulkan_latency_uniform_buffer_t {
    uint32_t hop_count;
    uint32_t region_size;
    uint32_t per_wg_offset;
    uint32_t lru[VULKAN_LATENCY_HOP_STRIDE_BYTES / VULKAN_LATENCY_POINTER_SIZE];
} vulkan_latency_uniform_buffer;

/* Check sizes of:
 * 4K, 8K, 12K, 16K, 20K, 24K, 28K, 32K, 40K, 48K, 56K, 64K, 80K, 96K, 112K, 128K,
 * 192K, 256K, 384K, 448K, 512K, 768K, 1M, 1.5M, 2M, 3M, 4M, 6M, 8M, 12M, 16M,
 * 24M, 32M, 40M, 48M, 56M, 64M, 96M, 128M, 192M, 256M, 384M, 512M, 768M,
 * 1G, 1.5G, 2G, 3G, 4G
 *
 * NOTE: Current stride size of 512 means we need to go in 512B increments (VULKAN_LATENCY_HOP_STRIDE_BYTES)
 */
const uint64_t vulkan_latency_region_sizes[] = {
    4096, 8192, 12288, 16384, 20480, 24576, 28672, 32768, 40960, 49152, 57344, 65536, 81920, 98304, 114688, 131072,
    196608, 262144, 393216, 458752, 524288, 786432, 1048576, 1572864, 2097152, 3145728, 4194304, 6291456, 8388608, 12582912, 16777216,
    25165824, 33554432, 41943040, 50331648, 58720256, 67108864, 100663296, 134217728, 201326592, 268435456, 402653184, 536870912, 805306368,
    1073741824, 1610612736, 2147483648, 3221225472, 4294967296
};
const uint32_t vulkan_latency_region_count = (uint32_t)(sizeof(vulkan_latency_region_sizes) / sizeof(vulkan_latency_region_sizes[0]));

static test_status _VulkanLatencyEntry(vulkan_physical_device *device, void *config_data);

test_status TestsVulkanLatencyRegister() {
    test_status status = VulkanRunnerRegisterTest(&_VulkanLatencyEntry, (void*)(uint64_t)VULKAN_LATENCY_TEST_TYPE_SCALAR, TESTS_VULKAN_LATENCY_SCLR_NAME, TESTS_VULKAN_LATENCY_VERSION, false);
    TEST_RETFAIL(status);
    return VulkanRunnerRegisterTest(&_VulkanLatencyEntry, (void*)(uint64_t)VULKAN_LATENCY_TEST_TYPE_VECTOR, TESTS_VULKAN_LATENCY_VEC_NAME, TESTS_VULKAN_LATENCY_VERSION, false);
}

static test_status _VulkanLatencyEntry(vulkan_physical_device *physical_device, void *config_data) {
    test_status status = TEST_OK;
    VkResult res = VK_SUCCESS;
    bool scalar_test = ((uint64_t)config_data) == VULKAN_LATENCY_TEST_TYPE_SCALAR;

    latency_helper_lru lru;
    status = LatencyHelperLRUInitialize(&lru, VULKAN_LATENCY_HOP_STRIDE_BYTES);
    if (!TEST_SUCCESS(status)) {
        goto error;
    }

    INFO("Device Name: %s\n", physical_device->physical_properties.properties.deviceName);

    VkQueueFamilyProperties *queue_family_properties = NULL;
    uint32_t queue_family_count = 0;
    status = VulkanGetPhysicalQueueFamilyProperties(physical_device, &queue_family_properties, &queue_family_count);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_lru;
    }

    vulkan_device device;
    status = VulkanCreateDevice(physical_device, queue_family_properties, queue_family_count, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, &device, NULL);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_queue_properties;
    }

    vulkan_shader shader;
    status = VulkanShaderInitializeFromFile(&device, scalar_test ? "vulkan_latency_scalar.spv" : "vulkan_latency_vector.spv", VK_SHADER_STAGE_COMPUTE_BIT, &shader);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_device;
    }
    status = VulkanShaderAddDescriptor(&shader, "data buffer 1", VULKAN_BINDING_STORAGE, 0, 0);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    status = VulkanShaderAddDescriptor(&shader, "data buffer 2", VULKAN_BINDING_STORAGE, 0, 1);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    status = VulkanShaderAddFixedSizeDescriptor(&shader, sizeof(vulkan_latency_uniform_buffer), "uniform buffer", VULKAN_BINDING_UNIFORM, 0, 2);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    status = VulkanShaderCreateDescriptorSets(&shader);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    vulkan_compute_pipeline pipeline;
    status = VulkanComputePipelineInitialize(&shader, "main", &pipeline);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    vulkan_memory memory;
    status = VulkanMemoryInitialize(&device, VULKAN_MEMORY_NORMAL, &memory);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_pipeline;
    }
    helper_unit_pair unit_conversion;
    uint64_t maximum_allocation = min(device.physical_device->physical_properties.properties.limits.maxStorageBufferRange, device.physical_device->physical_maintenance_properties_3.maxMemoryAllocationSize);
    HelperConvertUnitsBytes1024(maximum_allocation, &unit_conversion);
    INFO("Maximum allocation size: %.3f %s\n", unit_conversion.value, unit_conversion.units);
    uint64_t vram_capacity = VulkanMemoryGetPhysicalPoolSize(&memory);
    HelperConvertUnitsBytes1024(vram_capacity, &unit_conversion);
    INFO("Device local memory capacity: %.3f %s\n", unit_conversion.value, unit_conversion.units);
    uint64_t maximum_region_size = min(maximum_allocation, vram_capacity);
    uint32_t max_usable_region_size = 0;
    if (vulkan_latency_region_sizes[vulkan_latency_region_count - 1] <= maximum_region_size) {
        max_usable_region_size = vulkan_latency_region_count - 1;
    } else {
        for (uint32_t i = 1; i < vulkan_latency_region_count; i++) {
            uint64_t region_size = vulkan_latency_region_sizes[i];
            if (region_size > maximum_region_size) {
                max_usable_region_size = i - 1;
                break;
            }
        }
    }
    maximum_region_size = vulkan_latency_region_sizes[max_usable_region_size];
    HelperConvertUnitsBytes1024(maximum_region_size, &unit_conversion);
    INFO("Maximum region size: %.0f%s\n", unit_conversion.value, unit_conversion.units);

    while (true) {
        bool failure = false;
        status = VulkanMemoryAddRegion(&memory, maximum_region_size, "data buffer 1", VULKAN_REGION_STORAGE, VULKAN_REGION_TRANSFER_DESTINATION);
        if (!TEST_SUCCESS(status)) {
            failure = true;
        }
        if (!failure) {
            status = VulkanMemoryAddRegion(&memory, VULKAN_LATENCY_POINTER_SIZE, "data buffer 2", VULKAN_REGION_STORAGE, VULKAN_REGION_NORMAL);
            if (!TEST_SUCCESS(status)) {
                failure = true;
            }
        }
        if (!failure) {
            status = VulkanMemoryAllocateBacking(&memory);
            if (!TEST_SUCCESS(status)) {
                failure = true;
            }
        }
        if (failure) {
            INFO("Vulkan error encountered, lowering memory allocation. This is expected to happen.\n");
            status = VulkanMemoryCleanUp(&memory);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_pipeline;
            }
            status = VulkanMemoryInitialize(&device, VULKAN_MEMORY_NORMAL, &memory);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_pipeline;
            }
            max_usable_region_size--;
            if (max_usable_region_size == 0) {
                FATAL("Failed to allocate memory!\n");
                goto cleanup_memory;
            }
            maximum_region_size = vulkan_latency_region_sizes[max_usable_region_size];
        } else {
            break;
        }
    }
    HelperConvertUnitsBytes1024(maximum_region_size, &unit_conversion);
    INFO("Final region size: %.0f%s\n", unit_conversion.value, unit_conversion.units);

    bool uniform_memory_is_visible = false;
    vulkan_memory uniform_memory;
    status = VulkanMemoryInitialize(&device, VULKAN_MEMORY_VISIBLE | VULKAN_MEMORY_HOST_LOCAL, &uniform_memory);
    if (!TEST_SUCCESS(status)) {
        goto free_memory;
    }
    status = VulkanMemoryAddRegion(&uniform_memory, sizeof(vulkan_latency_uniform_buffer), "uniform buffer", VULKAN_REGION_UNIFORM, VULKAN_REGION_NORMAL);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory2;
    }
    status = VulkanMemoryAllocateBacking(&uniform_memory);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory2;
    }
    status = VulkanComputePipelineBind(&pipeline, &memory, "data buffer 1");
    if (!TEST_SUCCESS(status)) {
        goto free_memory2;
    }
    status = VulkanComputePipelineBind(&pipeline, &memory, "data buffer 2");
    if (!TEST_SUCCESS(status)) {
        goto free_memory2;
    }
    status = VulkanComputePipelineBind(&pipeline, &uniform_memory, "uniform buffer");
    if (!TEST_SUCCESS(status)) {
        goto free_memory2;
    }
    vulkan_command_buffer command_buffer;
    status = VulkanCommandBufferInitialize(&device, 1, &command_buffer);
    if (!TEST_SUCCESS(status)) {
        goto free_memory2;
    }
    vulkan_command_sequence command_sequence;
    status = VulkanCommandSequenceInitialize(&command_buffer, VULKAN_COMMAND_SEQUENCE_RESET_ON_COMPLETION, &command_sequence);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_command_buffer;
    }
    uint64_t *results = malloc((max_usable_region_size + 1) * sizeof(uint64_t));
    if (results == NULL) {
        status = TEST_OUT_OF_MEMORY;
        goto cleanup_command_buffer;
    }
    vulkan_region *uniform_region = VulkanMemoryGetRegion(&uniform_memory, "uniform buffer");
    vulkan_region *data_region_1 = VulkanMemoryGetRegion(&memory, "data buffer 1");
    bool warmup = true;

    uint32_t region_size_index = 0;
    while (region_size_index <= max_usable_region_size) {
        uint64_t region_size = vulkan_latency_region_sizes[region_size_index];
        uint32_t hop_count = VULKAN_LATENCY_STARTING_HOPS;
        if (warmup) {
            INFO("Warming up...\n");
        }
        DEBUG("Filling memory with pointer chains...\n");
        status = LatencyHelperLRUFillSubregion(&lru, data_region_1, region_size);
        if (!TEST_SUCCESS(status)) {
            goto free_results;
        }
        bool last_was_wg_increase = false;
        uint64_t hops_needed_per_full_pass = region_size / VULKAN_LATENCY_POINTER_SIZE;
        uint32_t workgroups = 1;

        while (true) {
            bool too_many_workgroups = false;
            volatile vulkan_latency_uniform_buffer *uniform_buffer_memory = VulkanMemoryMap(uniform_region);
            if (uniform_buffer_memory == NULL) {
                goto free_results;
            }
            uniform_buffer_memory->hop_count = hop_count;
            uniform_buffer_memory->region_size = (uint32_t)(hops_needed_per_full_pass);
            uniform_buffer_memory->per_wg_offset = uniform_buffer_memory->region_size / workgroups;
            memcpy((void*)uniform_buffer_memory->lru, lru.lru_table, sizeof(uniform_buffer_memory->lru));

            VulkanMemoryUnmap(uniform_region);

            status = VulkanCommandBufferStart(&command_sequence);
            if (!TEST_SUCCESS(status)) {
                goto free_results;
            }
            status = VulkanCommandBufferBindComputePipeline(&command_sequence, &pipeline);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_sequence;
            }
            status = VulkanCommandBufferDispatch(&command_sequence, workgroups, 1, 1);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_sequence;
            }
            status = VulkanCommandBufferEnd(&command_sequence);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_sequence;
            }
            HelperResetTimestamp();
            status = VulkanCommandBufferSubmit(&command_sequence);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_sequence;
            }
            status = VulkanCommandBufferWait(&command_sequence, VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_sequence;
            }
            uint64_t time = HelperMarkTimestamp();
            float time_per_hop_ns = 0;
            if (!warmup) {
                if (time == 0) {
                    INFO("Hop count %lu, WG count %lu took %.3fms (latency: N/A, total hops: %llu)\n", hop_count * VULKAN_LATENCY_HOPS_PER_CYCLE, workgroups, time / 1000.0f, (uint64_t)hop_count *VULKAN_LATENCY_HOPS_PER_CYCLE * (uint64_t)workgroups);
                } else {
                    uint64_t total_hops = (uint64_t)hop_count * VULKAN_LATENCY_HOPS_PER_CYCLE;
                    uint64_t total_time_100ns = time * 100000;
                    uint64_t time_per_hop_100ns = total_time_100ns / total_hops;
                    time_per_hop_ns = (float)((double)time_per_hop_100ns / 100.0);
                    if (last_was_wg_increase && (results[region_size_index] * VULKAN_LATENCY_BACKOFF_THRESHOLD) < time_per_hop_100ns) {
                        INFO("Hop count %lu, WG count %lu took %.3fms (latency: %.3fns INVALID (WG overload), total hops: %llu)\n", hop_count * VULKAN_LATENCY_HOPS_PER_CYCLE, workgroups, time / 1000.0f, time_per_hop_ns, (uint64_t)hop_count * VULKAN_LATENCY_HOPS_PER_CYCLE * (uint64_t)workgroups);
                        too_many_workgroups = true;
                        time_per_hop_ns = (float)((double)results[region_size_index] / 100.0);
                    } else {
                        INFO("Hop count %lu, WG count %lu took %.3fms (latency: %.3fns, total hops: %llu)\n", hop_count * VULKAN_LATENCY_HOPS_PER_CYCLE, workgroups, time / 1000.0f, time_per_hop_ns, (uint64_t)hop_count * VULKAN_LATENCY_HOPS_PER_CYCLE * (uint64_t)workgroups);
                        results[region_size_index] = time_per_hop_100ns;
                    }
                }
            }

            if (too_many_workgroups || time >= VULKAN_LATENCY_TARGET_TIME_US) {
                if (warmup) {
                    INFO("Warmup finished\n");
                    break;
                } else {
                    if (too_many_workgroups || ((uint64_t)hop_count * VULKAN_LATENCY_HOPS_PER_CYCLE * (uint64_t)workgroups) >= (hops_needed_per_full_pass * VULKAN_LATENCY_COVERAGE_MULTIPLE)) {
                        helper_unit_pair region_conversion;
                        HelperConvertUnitsBytes1024(region_size, &region_conversion);
                        INFO("%.1f %s latency: %.3fns\n", region_conversion.value, region_conversion.units, time_per_hop_ns);
                        break;
                    } else {
                        workgroups *= 2;
                        hop_count /= 2;
                        last_was_wg_increase = true;
                    }
                }
            } else {
                hop_count *= 2;
                last_was_wg_increase = false;
            }
        }
        if (warmup) {
            warmup = false;
        } else {
            region_size_index++;
        }
    }

    INFO("Final results for %s:\n", physical_device->physical_properties.properties.deviceName);
    if (MainGetTestResultFormat() == test_result_csv) {
        LOG_PLAIN("%s,\n", physical_device->physical_properties.properties.deviceName);
        LOG_PLAIN("Region size,Latency (ns)\n");
    }
    for (uint32_t i = 0; i <= max_usable_region_size; i++) {
        uint64_t region_size = vulkan_latency_region_sizes[i];
        uint64_t result = results[i];
        float result_ns = (float)((double)result / 100.0);

        helper_unit_pair region_conversion;
        HelperConvertUnitsBytes1024(region_size, &region_conversion);

        if (MainGetTestResultFormat() == test_result_csv) {
            LOG_PLAIN("%.1f%s,%.3f\n", region_conversion.value, region_conversion.units, result_ns);
        } else if(MainGetTestResultFormat() == test_result_raw) {
            LOG_RESULT(i, "%llu", "%llu", region_size, result * 10);
        } else {
            INFO("Latency for %.1f %s: %.3fns\n", region_conversion.value, region_conversion.units, result_ns);
        }
    }

cleanup_command_sequence:
    VulkanCommandBufferReset(&command_sequence);
free_results:
    free(results);
cleanup_command_buffer:
    VulkanCommandBufferCleanUp(&command_buffer);
free_memory2:
    VulkanMemoryFreeBuffersAndBacking(&uniform_memory);
cleanup_memory2:
    VulkanMemoryCleanUp(&uniform_memory);
free_memory:
    VulkanMemoryFreeBuffersAndBacking(&memory);
cleanup_memory:
    VulkanMemoryCleanUp(&memory);
cleanup_pipeline:
    VulkanComputePipelineCleanUp(&pipeline);
cleanup_shader:
    VulkanShaderCleanUp(&shader);
cleanup_device:
    VulkanDestroyDevice(&device);
cleanup_queue_properties:
    free(queue_family_properties);
cleanup_lru:
    LatencyHelperLRUCleanUp(&lru);
error:
    return status;
}

const uint64_t *VulkanLatencyGetRegionSizes() {
    return vulkan_latency_region_sizes;
}

size_t VulkanLatencyGetRegionCount() {
    return vulkan_latency_region_count;
}