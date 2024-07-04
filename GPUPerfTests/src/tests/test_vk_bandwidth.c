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
#include "vulkan_texture.h"
#include "buffer_filler.h"
#include "vulkan_staging.h"
#include "tests/test_vk_bandwidth.h"

#define VULKAN_BANDWIDTH_BYTES_PER_FETCH            (16)
#define VULKAN_BANDWIDTH_FETCHES_PER_CYCLE          (4)
#define VULKAN_BANDWIDTH_WORKGROUP_SIZE             (256)
#define VULKAN_BANDWIDTH_TARGET_TIME_US             (250000)
#define VULKAN_BANDWIDTH_STARTING_LOOP_COUNT        (4)
#define VULKAN_BANDWIDTH_RNG_SEED                   (332487265)

typedef struct vulkan_bandwidth_uniform_buffer_t {
    uint32_t loop_count;
    uint32_t region_size;
    uint32_t skip_amount;
    uint32_t texture_width;
    uint32_t texture_height;
} vulkan_bandwidth_uniform_buffer;

/* Check sizes of:
 * 4K, 8K, 12K, 16K, 20K, 24K, 28K, 32K, 40K, 48K, 56K, 64K, 80K, 96K, 112K, 128K,
 * 192K, 256K, 384K, 448K, 512K, 768K, 1M, 1.5M, 2M, 3M, 4M, 6M, 8M, 12M, 16M,
 * 24M, 32M, 40M, 48M, 56M, 64M, 96M, 128M, 192M, 256M, 384M, 512M, 768M,
 * 1G, 1.5G, 2G, 3G, 4G
 *
 * NOTE: Current WG size of 256 means we need to go in 4KB increments (VULKAN_BANDWIDTH_WORKGROUP_SIZE * VULKAN_BANDWIDTH_FETCHES_PER_CYCLE * VULKAN_BANDWIDTH_BYTES_PER_FETCH)
 */
const uint64_t vulkan_bandwidth_region_sizes[] = {
    4096, 8192, 12288, 16384, 20480, 24576, 28672, 32768, 40960, 49152, 57344, 65536, 81920, 98304, 114688, 131072,
    196608, 262144, 393216, 458752, 524288, 786432, 1048576, 1572864, 2097152, 3145728, 4194304, 6291456, 8388608, 12582912, 16777216,
    25165824, 33554432, 41943040, 50331648, 58720256, 67108864, 100663296, 134217728, 201326592, 268435456, 402653184, 536870912, 805306368,
    1073741824, 1610612736, 2147483648, 3221225472, 4294967296
};
const uint32_t vulkan_bandwidth_region_count = (uint32_t)(sizeof(vulkan_bandwidth_region_sizes) / sizeof(vulkan_bandwidth_region_sizes[0]));

static test_status _VulkanBandwidthEntry(vulkan_physical_device *device, void *config_data);

test_status TestsVulkanBandwidthRegister() {
    return VulkanRunnerRegisterTest(&_VulkanBandwidthEntry, NULL, TESTS_VULKAN_BANDWIDTH_NAME, TESTS_VULKAN_BANDWIDTH_VERSION, false);
}

static test_status _VulkanBandwidthEntry(vulkan_physical_device *physical_device, void *config_data) {
    test_status status = TEST_OK;
    VkResult res = VK_SUCCESS;

    bool use_texture = false;

    INFO("Device Name: %s\n", physical_device->physical_properties.properties.deviceName);

    VkQueueFamilyProperties *queue_family_properties = NULL;
    uint32_t queue_family_count = 0;
    status = VulkanGetPhysicalQueueFamilyProperties(physical_device, &queue_family_properties, &queue_family_count);
    if (!TEST_SUCCESS(status)) {
        goto error;
    }

    vulkan_device device;
    status = VulkanCreateDevice(physical_device, queue_family_properties, queue_family_count, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, &device, NULL);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_queue_properties;
    }

    vulkan_shader shader;
    status = VulkanShaderInitializeFromFile(&device, use_texture ? "vulkan_bandwidth_texture.spv" : "vulkan_bandwidth.spv", VK_SHADER_STAGE_COMPUTE_BIT, &shader);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_device;
    }
    if (use_texture) {
        status = VulkanShaderAddDescriptor(&shader, "data texture 1", VULKAN_BINDING_SAMPLER, 0, 0);
    } else {
        status = VulkanShaderAddDescriptor(&shader, "data buffer 1", VULKAN_BINDING_STORAGE, 0, 0);
    }
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    status = VulkanShaderAddDescriptor(&shader, "data buffer 2", VULKAN_BINDING_STORAGE, 0, 1);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    status = VulkanShaderAddFixedSizeDescriptor(&shader, sizeof(vulkan_bandwidth_uniform_buffer), "uniform buffer", VULKAN_BINDING_UNIFORM, 0, 2);
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
    uint64_t maximum_allocation;
    uint32_t maximum_texture_size = device.physical_device->physical_properties.properties.limits.maxImageDimension2D;
    if (use_texture) {
        INFO("Maximum texture size: %lux%lu\n", maximum_texture_size, maximum_texture_size);
        maximum_allocation = (size_t)maximum_texture_size * (size_t)maximum_texture_size * VULKAN_BANDWIDTH_BYTES_PER_FETCH;
    } else {
        maximum_allocation = min(device.physical_device->physical_properties.properties.limits.maxStorageBufferRange, device.physical_device->physical_maintenance_properties_3.maxMemoryAllocationSize);
    }
    HelperConvertUnitsBytes1024(maximum_allocation, &unit_conversion);
    INFO("Maximum allocation size: %.3f %s\n", unit_conversion.value, unit_conversion.units);
    uint64_t vram_capacity = VulkanMemoryGetPhysicalPoolSize(&memory);
    HelperConvertUnitsBytes1024(vram_capacity, &unit_conversion);
    INFO("Device local memory capacity: %.3f %s\n", unit_conversion.value, unit_conversion.units);
    uint64_t maximum_region_size = min(maximum_allocation, vram_capacity);
    uint32_t max_usable_region_size = 0;
    if (vulkan_bandwidth_region_sizes[vulkan_bandwidth_region_count - 1] <= maximum_region_size) {
        max_usable_region_size = vulkan_bandwidth_region_count - 1;
    } else {
        for (uint32_t i = 1; i < vulkan_bandwidth_region_count; i++) {
            uint64_t region_size = vulkan_bandwidth_region_sizes[i];
            if (region_size > maximum_region_size) {
                max_usable_region_size = i - 1;
                break;
            }
        }
    }
    maximum_region_size = vulkan_bandwidth_region_sizes[max_usable_region_size];
    HelperConvertUnitsBytes1024(maximum_region_size, &unit_conversion);
    INFO("Maximum region size: %.0f%s\n", unit_conversion.value, unit_conversion.units);

    uint32_t final_texture_width = 0;
    uint32_t final_texture_height = 0;

    while (true) {
        bool failure = false;
        uint32_t width = 0;
        uint32_t height = 0;
        if (use_texture) {
            size_t texels = maximum_region_size / VULKAN_BANDWIDTH_BYTES_PER_FETCH;
            width = (maximum_texture_size / (VULKAN_BANDWIDTH_FETCHES_PER_CYCLE * VULKAN_BANDWIDTH_WORKGROUP_SIZE)) * (VULKAN_BANDWIDTH_FETCHES_PER_CYCLE * VULKAN_BANDWIDTH_WORKGROUP_SIZE);
            height = (uint32_t)(texels / width);
            if (height == 0) {
                width = (uint32_t)texels;
            } else if (((size_t)height * width) != texels) {
                height++;
                width = (uint32_t)(texels / (size_t)height);
            }
            status = VulkanMemoryAddTexture2D(&memory, width, height, VK_FORMAT_R32G32B32A32_SFLOAT, 1, "data texture 1");
        } else {
            status = VulkanMemoryAddRegion(&memory, maximum_region_size, "data buffer 1", VULKAN_REGION_STORAGE, VULKAN_REGION_TRANSFER_DESTINATION);
        }
        if (!TEST_SUCCESS(status)) {
            failure = true;
        }
        if (!failure) {
            status = VulkanMemoryAddRegion(&memory, VULKAN_BANDWIDTH_WORKGROUP_SIZE * VULKAN_BANDWIDTH_BYTES_PER_FETCH, "data buffer 2", VULKAN_REGION_STORAGE, VULKAN_REGION_NORMAL);
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
            maximum_region_size = vulkan_bandwidth_region_sizes[max_usable_region_size];
        } else {
            final_texture_width = width;
            final_texture_height = height;
            break;
        }
    }
    HelperConvertUnitsBytes1024(maximum_region_size, &unit_conversion);
    if (use_texture) {
        INFO("Final region size: %.0f%s (%lux%lu texels)\n", unit_conversion.value, unit_conversion.units, final_texture_width, final_texture_height);
    } else {
        INFO("Final region size: %.0f%s\n", unit_conversion.value, unit_conversion.units);
    }

    uint64_t total_groups = maximum_region_size / (VULKAN_BANDWIDTH_BYTES_PER_FETCH * VULKAN_BANDWIDTH_WORKGROUP_SIZE);
    INFO("Ideal workgroup count: %llu\n", total_groups);
    uint32_t *group_size_limits = device.physical_device->physical_properties.properties.limits.maxComputeWorkGroupCount;
    INFO("Workgroup dispatch limits: %lux%lux%lu\n", group_size_limits[0], group_size_limits[1], group_size_limits[2]);
    uint64_t groups_max_x = HelperFindLargestPowerOfTwo(group_size_limits[0]);
    uint64_t groups_max_y = HelperFindLargestPowerOfTwo(group_size_limits[1]);
    uint64_t groups_max_z = HelperFindLargestPowerOfTwo(group_size_limits[2]);
    uint32_t groups_x = 1;
    uint32_t groups_y = 1;
    uint32_t groups_z = 1;
    if (total_groups > groups_max_x) {
        groups_x = (uint32_t)groups_max_x;
        total_groups /= groups_max_x;
        if (total_groups > groups_max_y) {
            groups_y = (uint32_t)groups_max_y;
            total_groups /= groups_max_y;
            if (total_groups > groups_max_z) {
                groups_z = (uint32_t)groups_max_z;
            } else {
                groups_z = (uint32_t)total_groups;
            }
        } else {
            groups_y = (uint32_t)total_groups;
        }
    } else {
        groups_x = (uint32_t)total_groups;
    }
    INFO("Chosen workgroup dispatch: %lux%lux%lu (total: %llu)\n", groups_x, groups_y, groups_z, (uint64_t)groups_x * (uint64_t)groups_y * (uint64_t)groups_z);

    bool uniform_memory_is_visible = false;
    vulkan_memory uniform_memory;
    status = VulkanMemoryInitialize(&device, VULKAN_MEMORY_VISIBLE | VULKAN_MEMORY_HOST_LOCAL, &uniform_memory);
    if (!TEST_SUCCESS(status)) {
        goto free_memory;
    }
    status = VulkanMemoryAddRegion(&uniform_memory, sizeof(vulkan_bandwidth_uniform_buffer), "uniform buffer", VULKAN_REGION_UNIFORM, VULKAN_REGION_NORMAL);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory2;
    }
    status = VulkanMemoryAllocateBacking(&uniform_memory);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory2;
    }
    if (use_texture) {
        vulkan_texture *data_texture_1 = VulkanMemoryGetTexture(&memory, "data texture 1");
        status = VulkanTexturePrepareForCopy(data_texture_1);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_memory2;
        }
        /* TODO: Fill the texture with RNG noise */
        /*vulkan_staging staging;
        VulkanStagingInitializeImage(data_texture_1, maximum_region_size, &staging);
        void *staging_buffer = VulkanStagingGetBuffer(&staging);
        memset(staging_buffer, 0, maximum_region_size);
        VulkanStagingTransferImage(&staging, 0);
        VulkanStagingCleanUp(&staging);*/
        status = VulkanTexturePrepareForRender(data_texture_1);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_memory2;
        }
    } else {
        vulkan_region *data_region_1 = VulkanMemoryGetRegion(&memory, "data buffer 1");
        status = BufferFillerRandomFloats(data_region_1, VULKAN_BANDWIDTH_RNG_SEED);
        if (!TEST_SUCCESS(status)) {
            goto free_memory2;
        }
    }
    if (use_texture) {
        status = VulkanComputePipelineBind(&pipeline, &memory, "data texture 1");
    } else {
        status = VulkanComputePipelineBind(&pipeline, &memory, "data buffer 1");
    }
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
    bool warmup = true;

    uint32_t region_size_index = 0;
    while (region_size_index <= max_usable_region_size) {
        uint64_t region_size = vulkan_bandwidth_region_sizes[region_size_index];
        uint32_t loop_count = VULKAN_BANDWIDTH_STARTING_LOOP_COUNT;
        if (warmup) {
            INFO("Warming up...\n");
        }
        while (true) {
            volatile vulkan_bandwidth_uniform_buffer *uniform_buffer_memory = VulkanMemoryMap(uniform_region);
            if (uniform_buffer_memory == NULL) {
                goto free_results;
            }
            uint32_t current_region_steps = (uint32_t)(region_size / (VULKAN_BANDWIDTH_WORKGROUP_SIZE * VULKAN_BANDWIDTH_BYTES_PER_FETCH));
            uniform_buffer_memory->loop_count = loop_count;
            uniform_buffer_memory->region_size = (uint32_t)(region_size / VULKAN_BANDWIDTH_BYTES_PER_FETCH);
            uniform_buffer_memory->skip_amount = (uint32_t)((loop_count + current_region_steps + 1) * VULKAN_BANDWIDTH_WORKGROUP_SIZE * VULKAN_BANDWIDTH_FETCHES_PER_CYCLE);
            if (use_texture) {
                size_t texels = region_size / VULKAN_BANDWIDTH_BYTES_PER_FETCH;
                uniform_buffer_memory->texture_width = (final_texture_width / (VULKAN_BANDWIDTH_FETCHES_PER_CYCLE * VULKAN_BANDWIDTH_WORKGROUP_SIZE)) * (VULKAN_BANDWIDTH_FETCHES_PER_CYCLE * VULKAN_BANDWIDTH_WORKGROUP_SIZE);
                uniform_buffer_memory->texture_height = (uint32_t)(texels / uniform_buffer_memory->texture_width);
                if (uniform_buffer_memory->texture_height == 0) {
                    uniform_buffer_memory->texture_width = (uint32_t)texels;
                    uniform_buffer_memory->texture_height = 1;
                } else if (((size_t)uniform_buffer_memory->texture_height * uniform_buffer_memory->texture_width) != texels) {
                    uniform_buffer_memory->texture_height++;
                    uniform_buffer_memory->texture_width = (uint32_t)(texels / (size_t)uniform_buffer_memory->texture_height);
                }
            } else {
                uniform_buffer_memory->texture_width = 0;
                uniform_buffer_memory->texture_height = 0;
            }

            VulkanMemoryUnmap(uniform_region);

            status = VulkanCommandBufferStart(&command_sequence);
            if (!TEST_SUCCESS(status)) {
                goto free_results;
            }
            status = VulkanCommandBufferBindComputePipeline(&command_sequence, &pipeline);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_sequence;
            }
            status = VulkanCommandBufferDispatch(&command_sequence, groups_x, groups_y, groups_z);
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
            if (!warmup) {
                if (time == 0) {
                    INFO("Loop count %lu took %.3fms (bandwidth: N/A)\n", loop_count, time / 1000.0f);
                } else {
                    uint64_t total_data_read = (uint64_t)groups_x * (uint64_t)groups_y * (uint64_t)groups_z * VULKAN_BANDWIDTH_WORKGROUP_SIZE * loop_count * VULKAN_BANDWIDTH_FETCHES_PER_CYCLE * VULKAN_BANDWIDTH_BYTES_PER_FETCH;
                    uint64_t throughput_per_second = (total_data_read * 1000000) / time;
                    HelperConvertUnitsBytes1024(throughput_per_second, &unit_conversion);
                    INFO("Loop count %lu took %.3fms (bandwidth: %.3f %s/s)\n", loop_count, time / 1000.0f, unit_conversion.value, unit_conversion.units);
                    results[region_size_index] = throughput_per_second;
                }
            }

            loop_count *= 2;
            if (time >= VULKAN_BANDWIDTH_TARGET_TIME_US) {
                if (warmup) {
                    INFO("Warmup finished\n");
                } else {
                    helper_unit_pair region_conversion;
                    HelperConvertUnitsBytes1024(region_size, &region_conversion);
                    if (use_texture) {
                        INFO("%.1f %s (%lux%lu) bandwidth: %.3f %s/s\n", region_conversion.value, region_conversion.units, uniform_buffer_memory->texture_width, uniform_buffer_memory->texture_height, unit_conversion.value, unit_conversion.units);
                    } else {
                        INFO("%.1f %s bandwidth: %.3f %s/s\n", region_conversion.value, region_conversion.units, unit_conversion.value, unit_conversion.units);
                    }
                }
                break;
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
        LOG_PLAIN("Region size,Bandwidth (GiB/s)\n");
    }
    for (uint32_t i = 0; i <= max_usable_region_size; i++) {
        uint64_t region_size = vulkan_bandwidth_region_sizes[i];
        uint64_t result = results[i];

        helper_unit_pair region_conversion;
        HelperConvertUnitsBytes1024(region_size, &region_conversion);

        if (MainGetTestResultFormat() == test_result_csv) {
            LOG_PLAIN("%.1f%s,%.3f\n", region_conversion.value, region_conversion.units, (float)(result / (1024*1024)) / 1024.0f);
        } else if (MainGetTestResultFormat() == test_result_raw) {
            LOG_RESULT(i, "%llu", "%llu", region_size, result);
        } else {
            HelperConvertUnitsBytes1024(result, &unit_conversion);
            INFO("Bandwidth for %.1f %s: %.3f %s/s\n", region_conversion.value, region_conversion.units, unit_conversion.value, unit_conversion.units);
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
error:
    return status;
}

const uint64_t *VulkanBandwidthGetRegionSizes() {
    return vulkan_bandwidth_region_sizes;
}

size_t VulkanBandwidthGetRegionCount() {
    return vulkan_bandwidth_region_count;
}