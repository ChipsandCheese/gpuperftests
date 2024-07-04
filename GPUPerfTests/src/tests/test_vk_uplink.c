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
#include "latency_helper.h"
#include "tests/test_vk_uplink.h"

#define VULKAN_UPLINK_TEST_TYPE_READ            0
#define VULKAN_UPLINK_TEST_TYPE_WRITE           1
#define VULKAN_UPLINK_TEST_TYPE_LATENCY_SHORT   2
#define VULKAN_UPLINK_TEST_TYPE_LATENCY_LONG    3
#define VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ    4
#define VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE   5
#define VULKAN_UPLINK_TEST_TYPE_MEMCPY_READ     6
#define VULKAN_UPLINK_TEST_TYPE_MEMCPY_WRITE    7

#define VULKAN_UPLINK_TARGET_TIME_US                (1000000)       // 1 second
#define VULKAN_UPLINK_TIME_CUTOFF_US                (500000)        // 0.5 seconds
#define VULKAN_UPLINK_LATENCY_SHORT_US              (10000000)      // 10 seconds
#define VULKAN_UPLINK_LATENCY_LONG_US               (120000000)     // 2 minutes
#define VULKAN_UPLINK_RNG_SEED                      (5487352346)
#define VULKAN_UPLINK_HOP_STRIDE_BYTES              (4096)
#define VULKAN_UPLINK_HOP_TIME_CHECK                (10000)
#define VULKAN_UPLINK_COMPUTE_STARTING_WORKGROUPS   (128)
#define VULKAN_UPLINK_COMPUTE_WORKGROUP_SIZE        (256)
#define VULKAN_UPLINK_COMPUTE_BYTES_PER_WORKGROUP   (16 * VULKAN_UPLINK_COMPUTE_WORKGROUP_SIZE)
#define VULKAN_UPLINK_MINIMUM_COPY_SIZE             (512)

typedef struct vulkan_uplink_uniform_buffer_t {
    uint32_t region_size;
} vulkan_uplink_uniform_buffer;

typedef struct vulkan_uplink_thread_data_t {
    uint32_t *source_memory;
    uint32_t *destination_memory;
    uint32_t block_size;
    uint32_t cycles;
} vulkan_uplink_thread_data;

static helper_atomic_bool memcpy_thread_run_flag;
static uint32_t memcpy_thread_count;
static helper_thread *memcpy_threads;
static vulkan_uplink_thread_data **memcpy_thread_data;

static test_status _VulkanUplinkEntry(vulkan_physical_device *device, void *config_data);
static test_status _VulkanUplinkInitializeMemcpyThreads(uint32_t *source, uint32_t *destination, uint64_t size);
static void _VulkanUplinkCleanUpMemcpyThreads();
static uint64_t _VulkanUplinkMemcpy();
static void _VulkanUplinkMemcpyThreadFunc(uint32_t thread_id, void *data);

test_status TestsVulkanUplinkRegister() {
    test_status status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_READ, TESTS_VULKAN_UPLINK_CPU_READ_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_WRITE, TESTS_VULKAN_UPLINK_CPU_WRITE_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_LATENCY_SHORT, TESTS_VULKAN_UPLINK_CPU_LATENCY_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_LATENCY_LONG, TESTS_VULKAN_UPLINK_CPU_LATENCY_LONG_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ, TESTS_VULKAN_UPLINK_GPU_READ_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE, TESTS_VULKAN_UPLINK_GPU_WRITE_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_MEMCPY_READ, TESTS_VULKAN_UPLINK_MAP_READ_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTest(&_VulkanUplinkEntry, (void *)VULKAN_UPLINK_TEST_TYPE_MEMCPY_WRITE, TESTS_VULKAN_UPLINK_MAP_WRITE_NAME, TESTS_VULKAN_UPLINK_VERSION, false);
    TEST_RETFAIL(status);
    return status;
}

static test_status _VulkanUplinkEntry(vulkan_physical_device *physical_device, void *config_data) {
    test_status status = TEST_OK;
    VkResult res = VK_SUCCESS;
    uint32_t test_type = (uint32_t)((uint64_t)config_data);

    INFO("Device Name: %s\n", physical_device->physical_properties.properties.deviceName);

    latency_helper_lru lru;
    status = LatencyHelperLRUInitialize(&lru, VULKAN_UPLINK_HOP_STRIDE_BYTES);
    if (!TEST_SUCCESS(status)) {
        goto error;
    }

    VkQueueFamilyProperties *queue_family_properties = NULL;
    uint32_t queue_family_property_count = 0;
    status = VulkanGetPhysicalQueueFamilyProperties(physical_device, &queue_family_properties, &queue_family_property_count);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_lru;
    }
    uint32_t *queue_family_indices = 0;
    uint32_t queue_family_count = 0;
    VkQueueFlags desired_queue_flags = VK_QUEUE_TRANSFER_BIT;
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        desired_queue_flags = VK_QUEUE_COMPUTE_BIT;
    }
    status = VulkanSelectQueueFamilyMultiple(&queue_family_indices, &queue_family_count, queue_family_properties, queue_family_property_count, desired_queue_flags);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_lru;
    }
    INFO("Found %lu suitable queue %s:\n", queue_family_count, (queue_family_count == 1) ? "family" : "families");
    for (uint32_t i = 0; i < queue_family_count; i++) {
        uint32_t actual_index = queue_family_indices[i];
        VkQueueFlags flags = queue_family_properties[actual_index].queueFlags;
        char flags_string[6];
        flags_string[0] = ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) ? 'G' : '-';
        flags_string[1] = ((flags & VK_QUEUE_COMPUTE_BIT) != 0) ? 'C' : '-';
        flags_string[2] = ((flags & VK_QUEUE_TRANSFER_BIT) != 0) ? 'T' : '-';
        flags_string[3] = ((flags & VK_QUEUE_SPARSE_BINDING_BIT) != 0) ? 'B' : '-';
        flags_string[4] = ((flags & VK_QUEUE_PROTECTED_BIT) != 0) ? 'P' : '-';
        flags_string[5] = '\0';
        INFO("Queue family %lu: index %lu queues %lu flags %s\n", i, actual_index, queue_family_properties[actual_index].queueCount, flags_string);
    }
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        // Just use one as we don't need concurrent queue submit for these test
        queue_family_count = 1;
    }
    vulkan_device device;
    status = VulkanCreateDeviceWithQueues(physical_device, queue_family_properties, queue_family_indices, NULL, queue_family_count, &device, NULL);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_queue_properties;
    }
    vulkan_shader compute_shader;
    vulkan_compute_pipeline compute_pipeline;
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        bool read_test = test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ;
        status = VulkanShaderInitializeFromFile(&device, "vulkan_uplink_gpu.spv", VK_SHADER_STAGE_COMPUTE_BIT, &compute_shader);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_device;
        }
        status = VulkanShaderAddDescriptor(&compute_shader, "device buffer", VULKAN_BINDING_STORAGE, 0, read_test ? 0 : 1);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_shader;
        }
        status = VulkanShaderAddDescriptor(&compute_shader, "host buffer", VULKAN_BINDING_STORAGE, 0, read_test ? 1 : 0);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_shader;
        }
        status = VulkanShaderAddFixedSizeDescriptor(&compute_shader, sizeof(vulkan_uplink_uniform_buffer), "uniform buffer", VULKAN_BINDING_UNIFORM, 0, 2);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_shader;
        }
        status = VulkanShaderCreateDescriptorSets(&compute_shader);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_shader;
        }
        status = VulkanComputePipelineInitialize(&compute_shader, "main", &compute_pipeline);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_shader;
        }
    }
    INFO("Total available parallel queues: %lu\n", device.total_queue_count);
    vulkan_memory uniform_memory;
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        status = VulkanMemoryInitialize(&device, VULKAN_MEMORY_VISIBLE | VULKAN_MEMORY_HOST_LOCAL, &uniform_memory);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_pipeline;
        }
        status = VulkanMemoryAddRegion(&uniform_memory, sizeof(vulkan_uplink_uniform_buffer), "uniform buffer", VULKAN_REGION_UNIFORM, VULKAN_REGION_NORMAL);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_uniform_memory;
        }
        status = VulkanMemoryAllocateBacking(&uniform_memory);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_uniform_memory;
        }
    }
    uint32_t device_memory_flags = 0;
    if (test_type == VULKAN_UPLINK_TEST_TYPE_MEMCPY_READ || test_type == VULKAN_UPLINK_TEST_TYPE_MEMCPY_WRITE || test_type == VULKAN_UPLINK_TEST_TYPE_LATENCY_SHORT || test_type == VULKAN_UPLINK_TEST_TYPE_LATENCY_LONG) {
        device_memory_flags |= VULKAN_MEMORY_VISIBLE;
        if (!VulkanMemoryIsMemoryTypePresent(physical_device, device_memory_flags)) {
            status = TEST_VK_FEATURE_UNSUPPORTED;
            goto cleanup_uniform_memory;
        }
    }
    uint32_t host_memory_flags = VULKAN_MEMORY_VISIBLE | VULKAN_MEMORY_HOST_LOCAL;
    vulkan_memory device_memory;
    status = VulkanMemoryInitializeForQueues(&device, queue_family_indices, queue_family_count, device_memory_flags, &device_memory);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_uniform_memory;
    }
    vulkan_memory host_memory;
    status = VulkanMemoryInitializeForQueues(&device, queue_family_indices, queue_family_count, host_memory_flags, &host_memory);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_device_memory;
    }
    helper_unit_pair unit_conversion;
    uint64_t allocation_size = 0;
    uint64_t device_capacity = VulkanMemoryGetPhysicalPoolSize(&device_memory);
    uint64_t host_capacity = VulkanMemoryGetPhysicalPoolSize(&host_memory);

    HelperConvertUnitsBytes1024(device_capacity, &unit_conversion);
    INFO("Device memory: %.3f %s\n", unit_conversion.value, unit_conversion.units);
    HelperConvertUnitsBytes1024(host_capacity, &unit_conversion);
    INFO("Host memory: %.3f %s\n", unit_conversion.value, unit_conversion.units);
    // "device_capacity - 1" is a hack for Radeon GPUs since DEVICE_LOCAL | HOST_VISIBLE = full BAR size, eg. 256 MiB
    // If you try to allocate those 256 MiB, the memory actually stays cached on the host and latency measurements
    // end up measuring system memory latency... Thanks AMD!
    allocation_size = min(HelperFindLargestPowerOfTwo(device_capacity - 1), HelperFindLargestPowerOfTwo(host_capacity));
    // Don't overallocate memory
    allocation_size = min(allocation_size, device.physical_device->physical_maintenance_properties_3.maxMemoryAllocationSize);
    // And also don't overallocate storage buffers if this is a compute read/write test
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        allocation_size = min(allocation_size, device.physical_device->physical_properties.properties.limits.maxStorageBufferRange);
    }
    //WORKAROUND-43
    if (device.physical_device->physical_properties.properties.vendorID == 0x8086 && allocation_size > 3221225472) {
        // Allocating the maximum 4GiB on Intel Arc (and maybe other Intel GPUs) currently throws an access violation exception
        INFO("Detected an Intel GPU, limiting allocation size to 3GiB (driver bug workaround)\n");
        allocation_size = 3221225472;
    }
    allocation_size = HelperFindLargestPowerOfTwo(allocation_size);
    bool failed = false;
    while (!failed) {
        status = VulkanMemoryAddRegion(&device_memory, allocation_size, "device buffer", VULKAN_REGION_STORAGE, VULKAN_REGION_TRANSFER_DESTINATION | VULKAN_REGION_TRANSFER_SOURCE);
        if (!TEST_SUCCESS(status)) {
            failed = true;
        }
        if (!failed) {
            status = VulkanMemoryAddRegion(&host_memory, allocation_size, "host buffer", VULKAN_REGION_STORAGE, VULKAN_REGION_TRANSFER_DESTINATION | VULKAN_REGION_TRANSFER_SOURCE);
            if (!TEST_SUCCESS(status)) {
                failed = true;
            }
        }
        if (!failed) {
            status = VulkanMemoryAllocateBacking(&device_memory);
            if (!TEST_SUCCESS(status)) {
                failed = true;
            }
        }
        if (!failed) {
            status = VulkanMemoryAllocateBacking(&host_memory);
            if (!TEST_SUCCESS(status)) {
                failed = true;
            }
        }
        if (failed) {
            INFO("Vulkan error encountered, lowering memory allocation. This is expected to happen.\n");
            if (VulkanMemoryIsAllocated(&device_memory)) {
                status = VulkanMemoryFreeBuffersAndBacking(&device_memory);
                if (!TEST_SUCCESS(status)) {
                    goto cleanup_host_memory;
                }
            }
            status = VulkanMemoryCleanUp(&device_memory);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
            status = VulkanMemoryInitializeForQueues(&device, queue_family_indices, queue_family_count, device_memory_flags, &device_memory);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
            if (VulkanMemoryIsAllocated(&host_memory)) {
                status = VulkanMemoryFreeBuffersAndBacking(&host_memory);
                if (!TEST_SUCCESS(status)) {
                    goto cleanup_host_memory;
                }
            }
            status = VulkanMemoryCleanUp(&host_memory);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
            status = VulkanMemoryInitializeForQueues(&device, queue_family_indices, queue_family_count, host_memory_flags, &host_memory);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
            allocation_size /= 2;
            if (allocation_size >= 1024 * 1024) {
                failed = false;
            }
        } else {
            break;
        }
    }
    if (failed) {
        FATAL("Failed to allocate sufficient amount of memory.\n");
        goto cleanup_host_memory;
    }
    HelperConvertUnitsBytes1024(allocation_size, &unit_conversion);
    INFO("Allocated memory: %.3f %s\n", unit_conversion.value, unit_conversion.units);

    vulkan_region *device_region = VulkanMemoryGetRegion(&device_memory, "device buffer");
    vulkan_region *host_region = VulkanMemoryGetRegion(&host_memory, "host buffer");
    if (device_region == NULL || host_region == NULL) {
        status = TEST_VK_MEMORY_MAPPING_ERROR;
        goto cleanup_host_memory;
    }
    if (test_type == VULKAN_UPLINK_TEST_TYPE_LATENCY_SHORT || test_type == VULKAN_UPLINK_TEST_TYPE_LATENCY_LONG) {
        INFO("Filling memory with pointer chains...\n");
        status = LatencyHelperLRUFillSubregion(&lru, device_region, 1024*1024);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_host_memory;
        }
        volatile uint32_t *device_memory = VulkanMemoryMap(device_region);
        if (device_memory == NULL) {
            goto cleanup_host_memory;
        }
        INFO("Measuring uplink latency...\n");
        uint64_t current_runtime = 0;
        uint64_t executed_cycles = 0;
        uint32_t pointer_index = 0;
        uint32_t runtime = (test_type == VULKAN_UPLINK_TEST_TYPE_LATENCY_SHORT) ? VULKAN_UPLINK_LATENCY_SHORT_US : VULKAN_UPLINK_LATENCY_LONG_US;
        uint32_t print_state = 0;

        while (current_runtime < runtime) {
            HelperResetTimestamp();
            for (uint32_t i = 0; i < VULKAN_UPLINK_HOP_TIME_CHECK; i++) {
                pointer_index = device_memory[pointer_index];
            }
            current_runtime += HelperGetTimestamp();
            if (test_type == VULKAN_UPLINK_TEST_TYPE_LATENCY_LONG) {
                uint32_t progress = (uint32_t)(current_runtime / (VULKAN_UPLINK_LATENCY_LONG_US / 10));
                if (print_state != progress) {
                    print_state = progress;
                    switch (print_state) {
                    case 1:
                        INFO("~ I feel this will take a while...\n");
                        break;
                    case 4:
                        INFO("~ Yep, still going!\n");
                        break;
                    case 7:
                        INFO("~ %lu minutes is a long time, isn't it?\n", VULKAN_UPLINK_LATENCY_LONG_US / 60000000);
                        break;
                    case 9:
                        INFO("~ Almost there...!\n");
                        break;
                    case 10:
                        INFO("~ Thank you for your patience, have a cookie!\n");
                        break;
                    default:
                        break;
                    }
                }
            }
            executed_cycles++;
        }
        INFO("Executed %llu access cycles\n", executed_cycles *VULKAN_UPLINK_HOP_TIME_CHECK);
        uint64_t result_latency_ps = (current_runtime * 1000000) / (executed_cycles * VULKAN_UPLINK_HOP_TIME_CHECK);
        INFO("Final results for %s:\n", physical_device->physical_properties.properties.deviceName);
        if (MainGetTestResultFormat() == test_result_csv) {
            LOG_PLAIN("%s\n", physical_device->physical_properties.properties.deviceName);
            LOG_PLAIN("Latency (ns)\n");
            LOG_PLAIN("%.3f\n", result_latency_ps / 1000.0f);
        } else if (MainGetTestResultFormat() == test_result_raw) {
            LOG_RESULT(0, "%s", "%llu", "latency", result_latency_ps);
        } else {
            INFO("Average latency: %.3fns\n", result_latency_ps / 1000.0f);
        }

        VulkanMemoryUnmap(device_region);
    } else {
        bool write_test = (test_type == VULKAN_UPLINK_TEST_TYPE_WRITE || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE || test_type == VULKAN_UPLINK_TEST_TYPE_MEMCPY_WRITE);
        bool compute_test = (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE);
        bool mapped_test = (test_type == VULKAN_UPLINK_TEST_TYPE_MEMCPY_READ || test_type == VULKAN_UPLINK_TEST_TYPE_MEMCPY_WRITE);
        vulkan_region *uniform_region = NULL;
        if (compute_test) {
            uniform_region = VulkanMemoryGetRegion(&uniform_memory, "uniform buffer");
            if (uniform_region == NULL) {
                goto cleanup_host_memory;
            }
        }
        INFO("Filling memory with random numbers...\n");
        status = BufferFillerRandomIntegers(write_test ? host_region : device_region, VULKAN_UPLINK_RNG_SEED);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_host_memory;
        }
        vulkan_region *source_region = write_test ? host_region : device_region;
        vulkan_region *destination_region = write_test ? device_region : host_region;
        vulkan_command_buffer *command_buffers = malloc(queue_family_count * sizeof(vulkan_command_buffer));
        if (command_buffers == NULL) {
            status = TEST_OUT_OF_MEMORY;
            goto cleanup_host_memory;
        }
        if (compute_test) {
            status = VulkanComputePipelineBind(&compute_pipeline, &device_memory, "device buffer");
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
            status = VulkanComputePipelineBind(&compute_pipeline, &host_memory, "host buffer");
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
            status = VulkanComputePipelineBind(&compute_pipeline, &uniform_memory, "uniform buffer");
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
        }
        vulkan_command_sequence *command_sequences = malloc(queue_family_count * sizeof(vulkan_command_sequence));
        if (command_sequences == NULL) {
            status = TEST_OUT_OF_MEMORY;
            goto free_command_buffers;
        }
        for (uint32_t i = 0; i < queue_family_count; i++) {
            status = VulkanCommandBufferInitializeOnQueue(&device, i, 1, &(command_buffers[i]));
            if (!TEST_SUCCESS(status)) {
                goto free_command_sequences;
            }
            status = VulkanCommandSequenceInitialize(&(command_buffers[i]), VULKAN_COMMAND_SEQUENCE_MULTI_QUEUE_USE, &(command_sequences[i]));
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_buffer;
            }
        }
        if (mapped_test) {
            uint32_t *source_memory = VulkanMemoryMap(source_region);
            uint32_t *destination_memory = VulkanMemoryMap(destination_region);
            if (source_memory == NULL || destination_memory == NULL) {
                status = TEST_VK_MEMORY_MAPPING_ERROR;
                goto cleanup_host_memory;
            }
            status = _VulkanUplinkInitializeMemcpyThreads(source_memory, destination_memory, allocation_size);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_host_memory;
            }
        }
        INFO("Measuring uplink bandwidth...\n");

        uint64_t result_speed = 0;
        uint32_t current_batch_size = compute_test ? VULKAN_UPLINK_COMPUTE_STARTING_WORKGROUPS : 1;
        bool first_run = true;
        bool keep_running = true;

        while (keep_running) {
            if (!first_run) {
                if (!mapped_test) {
                    for (uint32_t i = 0; i < queue_family_count; i++) {
                        VulkanCommandBufferReset(&(command_sequences[i]));
                    }
                }
            } else {
                first_run = false;
            }
            if (!mapped_test) {
                for (uint32_t i = 0; i < queue_family_count; i++) {
                    status = VulkanCommandBufferStart(&(command_sequences[i]));
                    if (!TEST_SUCCESS(status)) {
                        goto cleanup_command_buffer;
                    }
                    if (compute_test) {
                        volatile vulkan_uplink_uniform_buffer *uniform_buffer_memory = VulkanMemoryMap(uniform_region);
                        if (uniform_buffer_memory == NULL) {
                            goto cleanup_host_memory;
                        }
                        uniform_buffer_memory->region_size = (uint32_t)(allocation_size / VULKAN_UPLINK_COMPUTE_BYTES_PER_WORKGROUP); // In workgroups
                        status = VulkanCommandBufferBindComputePipeline(&(command_sequences[i]), &compute_pipeline);
                        if (!TEST_SUCCESS(status)) {
                            goto cleanup_command_sequence;
                        }
                        uint32_t max_x_dispatch = (uint32_t)HelperFindLargestPowerOfTwo(device.physical_device->physical_properties.properties.limits.maxComputeWorkGroupCount[0]);
                        uint32_t x = current_batch_size;
                        uint32_t y = 1;
                        if (current_batch_size > max_x_dispatch) {
                            y = current_batch_size / max_x_dispatch;
                            x = max_x_dispatch;
                        }
                        status = VulkanCommandBufferDispatch(&(command_sequences[i]), x, y, 1);
                        if (status == TEST_VK_DISPATCH_TOO_LARGE) {
                            keep_running = false;
                            status = TEST_OK;
                            break;
                        } else if (!TEST_SUCCESS(status)) {
                            goto cleanup_command_sequence;
                        }
                        VulkanMemoryUnmap(uniform_region);
                    } else {
                        for (uint32_t j = 0; j < current_batch_size; j++) {
                            status = VulkanCommandBufferCopyRegion(&(command_sequences[i]), write_test ? host_region : device_region, write_test ? device_region : host_region);
                            if (!TEST_SUCCESS(status)) {
                                goto cleanup_command_sequence;
                            }
                        }
                    }
                    status = VulkanCommandBufferEnd(&(command_sequences[i]));
                    if (!TEST_SUCCESS(status)) {
                        goto cleanup_command_sequence;
                    }
                }
            }
            if (keep_running) {
                HelperResetTimestamp();
                bool first_cycle = true;
                uint64_t transfer_cycles = 0;
                uint64_t memcpy_total_data = 0;
                while (true) {
                    if (!mapped_test) {
                        for (uint32_t i = 0; i < queue_family_count; i++) {
                            status = VulkanCommandBufferSubmitOnQueue(&(command_sequences[i]), compute_test ? 0 : VULKAN_QUEUES_ALL);
                            if (!TEST_SUCCESS(status)) {
                                goto cleanup_command_sequence;
                            }
                        }
                        for (uint32_t i = 0; i < queue_family_count; i++) {
                            status = VulkanCommandBufferWaitOnQueue(&(command_sequences[i]), VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE, compute_test ? 0 : VULKAN_QUEUES_ALL);
                            if (!TEST_SUCCESS(status)) {
                                goto cleanup_command_sequence;
                            }
                        }
                        transfer_cycles++;
                    } else {
                        //memcpy(destination_memory, source_memory, allocation_size);
                        memcpy_total_data = _VulkanUplinkMemcpy();
                    }
                    uint64_t current_runtime = HelperGetTimestamp();
                    if ((first_cycle || mapped_test) && current_runtime >= VULKAN_UPLINK_TIME_CUTOFF_US) {
                        keep_running = false;
                    }
                    if (current_runtime > VULKAN_UPLINK_TARGET_TIME_US) {
                        uint64_t total_data = 0;
                        if (compute_test) {
                            total_data = transfer_cycles * current_batch_size * VULKAN_UPLINK_COMPUTE_BYTES_PER_WORKGROUP;
                        } else if (mapped_test) {
                            total_data = memcpy_total_data;
                        } else {
                            total_data = transfer_cycles * current_batch_size * device.total_queue_count * allocation_size;
                        }
                        uint64_t data_rate = (total_data * 1000000) / current_runtime;
                        HelperConvertUnitsBytes1024(data_rate, &unit_conversion);
                        INFO("Batch size %lu bandwidth: %.3f %s/s\n", current_batch_size, unit_conversion.value, unit_conversion.units);
                        if (data_rate > result_speed) {
                            result_speed = data_rate;
                        }
                        current_batch_size *= 2;
                        break;
                    }
                    first_cycle = false;
                }
            }
        }

        const char *test_key = (test_type == VULKAN_UPLINK_TEST_TYPE_WRITE) ? "write" : "read";
        INFO("Final results for %s:\n", physical_device->physical_properties.properties.deviceName);
        if (MainGetTestResultFormat() == test_result_csv) {
            LOG_PLAIN("%s\n", physical_device->physical_properties.properties.deviceName);
            LOG_PLAIN("Bandwidth (GiB/s)\n");
            LOG_PLAIN("%f\n", (float)(result_speed / 1000000) / 1000.0f);
        } else if (MainGetTestResultFormat() == test_result_raw) {
            LOG_RESULT(0, "%s", "%llu", test_key, result_speed);
        } else {
            HelperConvertUnitsBytes1024(result_speed, &unit_conversion);
            INFO("Maximum %s bandwidth: %.3f %s/s\n", test_key, unit_conversion.value, unit_conversion.units);
        }

        VulkanMemoryUnmap(device_region);
        VulkanMemoryUnmap(host_region);
        if (mapped_test) {
            _VulkanUplinkCleanUpMemcpyThreads();
        }
    cleanup_command_sequence:
        for (uint32_t i = 0; i < queue_family_count; i++) {
            VulkanCommandBufferReset(&(command_sequences[i]));
        }
    cleanup_command_buffer:
        for (uint32_t i = 0; i < queue_family_count; i++) {
            VulkanCommandBufferCleanUp(&(command_buffers[i]));
        }
    free_command_sequences:
        free(command_sequences);
    free_command_buffers:
        free(command_buffers);
    }
cleanup_host_memory:
    VulkanMemoryFreeBuffersAndBacking(&host_memory);
    VulkanMemoryCleanUp(&host_memory);
cleanup_device_memory:
    VulkanMemoryFreeBuffersAndBacking(&device_memory);
    VulkanMemoryCleanUp(&device_memory);
cleanup_uniform_memory:
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        VulkanMemoryFreeBuffersAndBacking(&uniform_memory);
        VulkanMemoryCleanUp(&uniform_memory);
    }
cleanup_pipeline:
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        VulkanComputePipelineCleanUp(&compute_pipeline);
    }
cleanup_shader:
    if (test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_READ || test_type == VULKAN_UPLINK_TEST_TYPE_COMPUTE_WRITE) {
        VulkanShaderCleanUp(&compute_shader);
    }
cleanup_device:
    VulkanDestroyDevice(&device);
cleanup_queue_properties:
    free(queue_family_properties);
cleanup_lru:
    LatencyHelperLRUCleanUp(&lru);
error:
    return status;
}

static test_status _VulkanUplinkInitializeMemcpyThreads(uint32_t *source, uint32_t *destination, uint64_t size) {
    memcpy_thread_count = HelperGetProcessorCount();
    INFO("Detected %lu CPU threads\n", memcpy_thread_count);
    memcpy_thread_data = malloc(sizeof(vulkan_uplink_thread_data*) * memcpy_thread_count);
    if (memcpy_thread_data == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    uint32_t block_size = (uint32_t)(size / memcpy_thread_count);
    if (block_size < VULKAN_UPLINK_MINIMUM_COPY_SIZE) {
        block_size = VULKAN_UPLINK_MINIMUM_COPY_SIZE;
    }
    uint64_t current_offset = 0;
    for (uint32_t i = 0; i < memcpy_thread_count; i++) {
        memcpy_thread_data[i] = malloc(sizeof(vulkan_uplink_thread_data));
        if (memcpy_thread_data[i] == NULL) {
            _VulkanUplinkCleanUpMemcpyThreads();
            return TEST_OUT_OF_MEMORY;
        }
        memcpy_thread_data[i]->source_memory = (uint32_t *)((size_t)source + current_offset);
        memcpy_thread_data[i]->destination_memory = (uint32_t *)((size_t)destination + current_offset);
        uint64_t memory_start = current_offset;
        current_offset += block_size;
        if (current_offset > size || (i + 1 == memcpy_thread_count)) {
            current_offset = size;
        }
        uint64_t memory_end = current_offset;
        memcpy_thread_data[i]->block_size = (uint32_t)(memory_end - memory_start);
        memcpy_thread_data[i]->cycles = 0;
    }
    return TEST_OK;
}

static void _VulkanUplinkCleanUpMemcpyThreads() {
    if (memcpy_thread_data != NULL) {
        for (uint32_t i = 0; i < memcpy_thread_count; i++) {
            if (memcpy_thread_data[i] != NULL) {
                free(memcpy_thread_data[i]);
            }
        }
        free(memcpy_thread_data);
    }
}

static uint64_t _VulkanUplinkMemcpy() {
    HelperAtomicBoolSet(&memcpy_thread_run_flag);
    memcpy_threads = HelperCreateThreads(memcpy_thread_count, _VulkanUplinkMemcpyThreadFunc, memcpy_thread_data);
    if (memcpy_threads == NULL) {
        return 0;
    }
    HelperSleep(VULKAN_UPLINK_TARGET_TIME_US / 1000);
    HelperAtomicBoolClear(&memcpy_thread_run_flag);
    HelperWaitForThreads(memcpy_threads, memcpy_thread_count);
    HelperCleanUpThreads(memcpy_threads, memcpy_thread_count);

    uint64_t total_data = 0;
    for (uint32_t i = 0; i < memcpy_thread_count; i++) {
        if (memcpy_thread_data[i] != NULL) {
            total_data += (uint64_t)memcpy_thread_data[i]->cycles * (uint64_t)memcpy_thread_data[i]->block_size;
        }
    }
    return total_data;
}

static void _VulkanUplinkMemcpyThreadFunc(uint32_t thread_id, void *data) {
    vulkan_uplink_thread_data *thread_data = (vulkan_uplink_thread_data *)data;

    if (thread_data->block_size >= VULKAN_UPLINK_MINIMUM_COPY_SIZE) {
        while (HelperAtomicBoolRead(&memcpy_thread_run_flag)) {
            memcpy(thread_data->destination_memory, thread_data->source_memory, thread_data->block_size);
            thread_data->cycles++;
        }
    }
}