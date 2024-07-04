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
#include "tests/test_vk_rate.h"

#define VULKAN_RATE_PARALLEL_OPS                (16)    /* 4 4D vectors for each thread */
#define VULKAN_RATE_WORKGROUP_SIZE              (256)
#define VULKAN_RATE_STARTING_WORKGROUP_COUNT    (16)
#define VULKAN_RATE_STARTING_LOOP_COUNT         (1024)
#define VULKAN_RATE_TARGET_TIME_US              (250000)

#define VULKAN_RATE_OP_TYPE_OP                  (0)
#define VULKAN_RATE_OP_TYPE_FLOP                (1)
#define VULKAN_RATE_OP_TYPE_FLOPX2              (2)
#define VULKAN_RATE_OP_TYPE_IOP                 (3)
#define VULKAN_RATE_OP_TYPE_IOPX2               (4)

#define VULKAN_RATE_REGISTER_SUBTEST(type, op, size, ops_per_cycle, op_type) \
{\
    test_status status = _VulkanRateRegisterSubtest(type, op, TESTS_VULKAN_RATE_NAME_PREFIX type TESTS_VULKAN_RATE_NAME_SEPARATOR op, size, ops_per_cycle, op_type);\
    if (!TEST_SUCCESS(status)) {\
        return status;\
    }\
}
#define VULKAN_RATE_REGISTER_SUBTEST_GROUP(type, size, ops_per_cycle, op_type, op_type2x) \
VULKAN_RATE_REGISTER_SUBTEST(type, TESTS_VULKAN_RATE_OP_ADD, size, ops_per_cycle, op_type) \
VULKAN_RATE_REGISTER_SUBTEST(type, TESTS_VULKAN_RATE_OP_SUB, size, ops_per_cycle, op_type) \
VULKAN_RATE_REGISTER_SUBTEST(type, TESTS_VULKAN_RATE_OP_MUL, size, ops_per_cycle, op_type) \
VULKAN_RATE_REGISTER_SUBTEST(type, TESTS_VULKAN_RATE_OP_MAC, size, ops_per_cycle, op_type2x) \
VULKAN_RATE_REGISTER_SUBTEST(type, TESTS_VULKAN_RATE_OP_DIV, size, ops_per_cycle, op_type) \
VULKAN_RATE_REGISTER_SUBTEST(type, TESTS_VULKAN_RATE_OP_REM, size, ops_per_cycle, op_type)

typedef struct vulkan_rate_uniform_buffer_t {
    uint32_t loop_count;
} vulkan_rate_uniform_buffer;

static const char *_vulkan_rate_type_map[] = {
    TESTS_VULKAN_RATE_TYPE_FP16,
    TESTS_VULKAN_RATE_TYPE_FP32,
    TESTS_VULKAN_RATE_TYPE_FP64,
    TESTS_VULKAN_RATE_TYPE_INT8,
    TESTS_VULKAN_RATE_TYPE_INT16,
    TESTS_VULKAN_RATE_TYPE_INT32,
    TESTS_VULKAN_RATE_TYPE_INT64
};

static const char *_vulkan_rate_op_map[] = {
    TESTS_VULKAN_RATE_OP_MAC,
    TESTS_VULKAN_RATE_OP_ADD,
    TESTS_VULKAN_RATE_OP_SUB,
    TESTS_VULKAN_RATE_OP_MUL,
    TESTS_VULKAN_RATE_OP_DIV,
    TESTS_VULKAN_RATE_OP_REM,
    TESTS_VULKAN_RATE_OP_RCP,
    TESTS_VULKAN_RATE_OP_ISQRT
};

static test_status _VulkanRateEntry(vulkan_physical_device *device, void *config_data);
static test_status _VulkanRateExecuteKernel(uint64_t workgroup_count, uint32_t loop_count, uint32_t ops_per_cycle, uint64_t *result, uint64_t *time_taken, vulkan_device *device, vulkan_region *uniform_region, vulkan_command_sequence *command_sequence, vulkan_compute_pipeline *pipeline);
static int32_t _VulkanRateGetIndexOfType(const char *type);
static int32_t _VulkanRateGetIndexOfOp(const char *op);
static const char *_VulkanRateGetTypeFromIndex(int32_t index);
static const char *_VulkanRateGetOpFromIndex(int32_t index);
static test_status _VulkanRateRegisterSubtest(const char *type, const char *op, const char *test_name, size_t datatype_size, uint32_t ops_per_cycle, uint32_t op_type);

test_status TestsVulkanRateRegister() {
    VULKAN_RATE_REGISTER_SUBTEST(TESTS_VULKAN_RATE_TYPE_FP16, TESTS_VULKAN_RATE_OP_ISQRT, sizeof(float), 2, VULKAN_RATE_OP_TYPE_OP);
    VULKAN_RATE_REGISTER_SUBTEST(TESTS_VULKAN_RATE_TYPE_FP32, TESTS_VULKAN_RATE_OP_ISQRT, sizeof(float), 1, VULKAN_RATE_OP_TYPE_OP);
    VULKAN_RATE_REGISTER_SUBTEST(TESTS_VULKAN_RATE_TYPE_FP64, TESTS_VULKAN_RATE_OP_ISQRT, sizeof(double), 1, VULKAN_RATE_OP_TYPE_OP);
    VULKAN_RATE_REGISTER_SUBTEST(TESTS_VULKAN_RATE_TYPE_FP16, TESTS_VULKAN_RATE_OP_RCP, sizeof(float), 2, VULKAN_RATE_OP_TYPE_FLOP);
    VULKAN_RATE_REGISTER_SUBTEST(TESTS_VULKAN_RATE_TYPE_FP32, TESTS_VULKAN_RATE_OP_RCP, sizeof(float), 1, VULKAN_RATE_OP_TYPE_FLOP);
    VULKAN_RATE_REGISTER_SUBTEST(TESTS_VULKAN_RATE_TYPE_FP64, TESTS_VULKAN_RATE_OP_RCP, sizeof(double), 1, VULKAN_RATE_OP_TYPE_FLOP);
    VULKAN_RATE_REGISTER_SUBTEST_GROUP(TESTS_VULKAN_RATE_TYPE_FP16, sizeof(float), 2, VULKAN_RATE_OP_TYPE_FLOP, VULKAN_RATE_OP_TYPE_FLOPX2);
    VULKAN_RATE_REGISTER_SUBTEST_GROUP(TESTS_VULKAN_RATE_TYPE_FP32, sizeof(float), 1, VULKAN_RATE_OP_TYPE_FLOP, VULKAN_RATE_OP_TYPE_FLOPX2);
    VULKAN_RATE_REGISTER_SUBTEST_GROUP(TESTS_VULKAN_RATE_TYPE_FP64, sizeof(double), 1, VULKAN_RATE_OP_TYPE_FLOP, VULKAN_RATE_OP_TYPE_FLOPX2);
    VULKAN_RATE_REGISTER_SUBTEST_GROUP(TESTS_VULKAN_RATE_TYPE_INT8, sizeof(int8_t), 4, VULKAN_RATE_OP_TYPE_IOP, VULKAN_RATE_OP_TYPE_IOPX2);
    VULKAN_RATE_REGISTER_SUBTEST_GROUP(TESTS_VULKAN_RATE_TYPE_INT16, sizeof(int16_t), 2, VULKAN_RATE_OP_TYPE_IOP, VULKAN_RATE_OP_TYPE_IOPX2);
    VULKAN_RATE_REGISTER_SUBTEST_GROUP(TESTS_VULKAN_RATE_TYPE_INT32, sizeof(int32_t), 1, VULKAN_RATE_OP_TYPE_IOP, VULKAN_RATE_OP_TYPE_IOPX2);
    VULKAN_RATE_REGISTER_SUBTEST_GROUP(TESTS_VULKAN_RATE_TYPE_INT64, sizeof(int64_t), 1, VULKAN_RATE_OP_TYPE_IOP, VULKAN_RATE_OP_TYPE_IOPX2);
    return TEST_OK;
}

static test_status _VulkanRateEntry(vulkan_physical_device *physical_device, void *config_data) {
    test_status status = TEST_OK;
    VkResult res = VK_SUCCESS;

    int32_t test_type_index = (int32_t)((((uint64_t)config_data) >> 16) & 0xFFFF);
    int32_t test_op_index = (int32_t)(((uint64_t)config_data) & 0xFFFF);
    size_t test_datatype_size = (size_t)((((uint64_t)config_data) >> 32) & 0xFFFF);
    uint32_t test_ops_per_cycle = (uint32_t)((((uint64_t)config_data) >> 48) & 0xFF);
    uint32_t test_op_type = (uint32_t)((((uint64_t)config_data) >> 56) & 0xFF);
    const char *test_datatype_string = _VulkanRateGetTypeFromIndex(test_type_index);
    const char *test_op_string = _VulkanRateGetOpFromIndex(test_op_index);

    if (test_datatype_string == NULL || test_op_string == NULL || test_datatype_size == 0) {
        return TEST_PROGRAMMING_ERROR;
    }
    INFO("Device Name: %s\n", physical_device->physical_properties.properties.deviceName);

    VkQueueFamilyProperties *queue_family_properties = NULL;
    uint32_t queue_family_count = 0;
    status = VulkanGetPhysicalQueueFamilyProperties(physical_device, &queue_family_properties, &queue_family_count);
    if (!TEST_SUCCESS(status)) {
        goto error;
    }

    VkPhysicalDeviceVulkan12Features enabled_features_vk12 = {0};
    enabled_features_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enabled_features_vk12.pNext = NULL;
    VkPhysicalDeviceVulkan11Features enabled_features_vk11 = {0};
    enabled_features_vk11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    enabled_features_vk11.pNext = &enabled_features_vk12;
    VkPhysicalDeviceFeatures2 enabled_features = {0};
    enabled_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    enabled_features.pNext = &enabled_features_vk11;

    if (test_type_index == _VulkanRateGetIndexOfType(TESTS_VULKAN_RATE_TYPE_FP64)) {
        enabled_features.features.shaderFloat64 = VK_TRUE;
    } else if (test_type_index == _VulkanRateGetIndexOfType(TESTS_VULKAN_RATE_TYPE_FP16)) {
        enabled_features_vk12.shaderFloat16 = VK_TRUE;
        enabled_features_vk11.storageBuffer16BitAccess = VK_TRUE;
    } else if (test_type_index == _VulkanRateGetIndexOfType(TESTS_VULKAN_RATE_TYPE_INT64)) {
        enabled_features.features.shaderInt64 = VK_TRUE;
    } else if (test_type_index == _VulkanRateGetIndexOfType(TESTS_VULKAN_RATE_TYPE_INT16)) {
        enabled_features.features.shaderInt16 = VK_TRUE;
        enabled_features_vk11.storageBuffer16BitAccess = VK_TRUE;
    } else if (test_type_index == _VulkanRateGetIndexOfType(TESTS_VULKAN_RATE_TYPE_INT8)) {
        enabled_features_vk12.shaderInt8 = VK_TRUE;
        enabled_features_vk12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    }

    vulkan_device device;
    status = VulkanCreateDevice(physical_device, queue_family_properties, queue_family_count, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, &device, &enabled_features);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_queue_properties;
    }

    vulkan_shader shader;
    const char *shader_name = NULL;
    status = HelperPrintToBuffer(&shader_name, NULL, "vulkan_rate_%s_%s.spv", test_datatype_string, test_op_string);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_device;
    }
    status = VulkanShaderInitializeFromFile(&device, shader_name, VK_SHADER_STAGE_COMPUTE_BIT, &shader);
    free((void*)shader_name);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_device;
    }
    status = VulkanShaderAddDescriptor(&shader, "dummy inputs", VULKAN_BINDING_STORAGE, 0, 0);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    status = VulkanShaderAddDescriptor(&shader, "dummy outputs", VULKAN_BINDING_STORAGE, 0, 1);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_shader;
    }
    status = VulkanShaderAddFixedSizeDescriptor(&shader, sizeof(vulkan_rate_uniform_buffer), "uniform buffer", VULKAN_BINDING_UNIFORM, 0, 2);
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
    uint64_t dummy_region_size = VULKAN_RATE_WORKGROUP_SIZE * VULKAN_RATE_PARALLEL_OPS * test_datatype_size * 2;
    INFO("Allocating %llu bytes of dummy data\n", dummy_region_size);
    status = VulkanMemoryAddRegion(&memory, dummy_region_size, "dummy inputs", VULKAN_REGION_STORAGE, VULKAN_REGION_TRANSFER_DESTINATION);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory;
    }
    status = VulkanMemoryAddRegion(&memory, dummy_region_size, "dummy outputs", VULKAN_REGION_STORAGE, VULKAN_REGION_TRANSFER_DESTINATION);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory;
    }
    status = VulkanMemoryAllocateBacking(&memory);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory;
    }

    bool uniform_memory_is_visible = false;
    vulkan_memory uniform_memory;
    status = VulkanMemoryInitialize(&device, VULKAN_MEMORY_VISIBLE | VULKAN_MEMORY_HOST_LOCAL, &uniform_memory);
    if (!TEST_SUCCESS(status)) {
        goto free_memory;
    }
    status = VulkanMemoryAddRegion(&uniform_memory, sizeof(vulkan_rate_uniform_buffer), "uniform buffer", VULKAN_REGION_UNIFORM, VULKAN_REGION_NORMAL);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory2;
    }
    status = VulkanMemoryAllocateBacking(&uniform_memory);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_memory2;
    }
    vulkan_region *data_region_1 = VulkanMemoryGetRegion(&memory, "dummy inputs");
    status = BufferFillerValueFloat(data_region_1, 1.0f);
    if (!TEST_SUCCESS(status)) {
        goto free_memory2;
    }
    status = VulkanComputePipelineBind(&pipeline, &memory, "dummy inputs");
    if (!TEST_SUCCESS(status)) {
        goto free_memory2;
    }
    status = VulkanComputePipelineBind(&pipeline, &memory, "dummy outputs");
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
    vulkan_region *uniform_region = VulkanMemoryGetRegion(&uniform_memory, "uniform buffer");

    /* Warmup */
    INFO("Warming up...\n");
    for (int i = 0; i < 10; i++) {
        status = _VulkanRateExecuteKernel(VULKAN_RATE_STARTING_WORKGROUP_COUNT, VULKAN_RATE_STARTING_WORKGROUP_COUNT, test_ops_per_cycle, NULL, NULL, &device, uniform_region, &command_sequence, &pipeline);
        if (!TEST_SUCCESS(status)) {
            goto cleanup_command_buffer;
        }
    }
    INFO("Warmup finished\n");

    uint64_t top_result = 0;
    uint64_t top_loops = 0;
    uint64_t top_workgroups = 0;
    uint64_t workgroup_count = VULKAN_RATE_STARTING_WORKGROUP_COUNT;

    while (true) {
        uint64_t result = 0;
        uint64_t time_taken = 0;
        uint32_t loop_count = VULKAN_RATE_STARTING_LOOP_COUNT;

        while (time_taken < VULKAN_RATE_TARGET_TIME_US) {
            status = _VulkanRateExecuteKernel(workgroup_count, loop_count, test_ops_per_cycle, &result, &time_taken, &device, uniform_region, &command_sequence, &pipeline);
            if (!TEST_SUCCESS(status)) {
                goto cleanup_command_buffer;
            }
            if (result > top_result) {
                top_result = result;
                top_loops = loop_count;
                top_workgroups = workgroup_count;
            }
            loop_count *= 2;
        }
        if (loop_count == VULKAN_RATE_STARTING_LOOP_COUNT * 2) {
            /* We exited after the first run */
            break;
        }
        workgroup_count *= 2;
    }
    if (test_op_type == VULKAN_RATE_OP_TYPE_FLOPX2 || test_op_type == VULKAN_RATE_OP_TYPE_IOPX2) {
        top_result *= 2;
    }
    INFO("Final results for %s:\n", physical_device->physical_properties.properties.deviceName);
    if (MainGetTestResultFormat() == test_result_csv) {
        LOG_PLAIN("%s,\n", physical_device->physical_properties.properties.deviceName);
        LOG_PLAIN("Datatype,Operations (GFLOPS/GIOPS/GOPS)\n");
        LOG_PLAIN("%s,%f\n", test_datatype_string, (float)(top_result / 1000000) / 1000.0f);
    } else if (MainGetTestResultFormat() == test_result_raw) {
        LOG_RESULT(0, "%s", "%llu", test_datatype_string, top_result);
    } else {
        helper_unit_pair ops_conversion;
        HelperConvertUnitsPlain1000(top_result, &ops_conversion);
        const char *op_type_string = "OPS";
        switch (test_op_type) {
        case VULKAN_RATE_OP_TYPE_FLOP:
        case VULKAN_RATE_OP_TYPE_FLOPX2:
            op_type_string = "FLOPS";
            break;
        case VULKAN_RATE_OP_TYPE_IOP:
        case VULKAN_RATE_OP_TYPE_IOPX2:
            op_type_string = "IOPS";
            break;
        default:
            break;
        }
        INFO("Rate for %s %s: %.3f %s%s\n", test_datatype_string, test_op_string, ops_conversion.value, ops_conversion.units, op_type_string);
    }
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

static test_status _VulkanRateExecuteKernel(uint64_t workgroup_count, uint32_t loop_count, uint32_t ops_per_cycle, uint64_t *result, uint64_t *time_taken, vulkan_device *device, vulkan_region *uniform_region, vulkan_command_sequence *command_sequence, vulkan_compute_pipeline *pipeline) {
    helper_unit_pair unit_conversion;
    test_status status = TEST_OK;
    volatile vulkan_rate_uniform_buffer *uniform_buffer_memory = VulkanMemoryMap(uniform_region);
    if (uniform_buffer_memory == NULL) {
        status = TEST_INVALID_PARAMETER;
        goto error;
    }
    uniform_buffer_memory->loop_count = loop_count;
    VulkanMemoryUnmap(uniform_region);

    status = VulkanCommandBufferStart(command_sequence);
    if (!TEST_SUCCESS(status)) {
        goto error;
    }
    status = VulkanCommandBufferBindComputePipeline(command_sequence, pipeline);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_command_sequence;
    }
    uint32_t groups_x = 0;
    uint32_t groups_y = 0;
    uint32_t groups_z = 0;
    status = VulkanCalculateWorkgroupDispatch(device, workgroup_count, &groups_x, &groups_y, &groups_z);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_command_sequence;
    }
    status = VulkanCommandBufferDispatch(command_sequence, groups_x, groups_y, groups_z);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_command_sequence;
    }
    status = VulkanCommandBufferEnd(command_sequence);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_command_sequence;
    }
    HelperResetTimestamp();
    status = VulkanCommandBufferSubmit(command_sequence);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_command_sequence;
    }
    status = VulkanCommandBufferWait(command_sequence, VULKAN_COMMAND_SEQUENCE_WAIT_INFINITE);
    if (!TEST_SUCCESS(status)) {
        goto cleanup_command_sequence;
    }
    uint64_t time = HelperMarkTimestamp();
    if (time_taken != NULL) {
        *time_taken = time;
    }
    if (time == 0) {
        INFO("Loop count %lu workgroup count %llu took %.3fms (rate: N/A)\n", loop_count, workgroup_count, time / 1000.0f);
        if (result != NULL) {
            *result = 0;
        }
    } else {
        uint64_t total_operations = (uint64_t)groups_x * (uint64_t)groups_y * (uint64_t)groups_z * VULKAN_RATE_WORKGROUP_SIZE * VULKAN_RATE_PARALLEL_OPS * loop_count * ops_per_cycle;
        uint64_t throughput_per_second = (total_operations * 1000000) / time;
        HelperConvertUnitsPlain1000(throughput_per_second, &unit_conversion);
        INFO("Loop count %lu workgroup count %llu took %.3fms (rate: %.3f %sOPS/s)\n", loop_count, workgroup_count, time / 1000.0f, unit_conversion.value, unit_conversion.units);
        if (result != NULL) {
            *result = throughput_per_second;
        }
    }
cleanup_command_sequence:
    VulkanCommandBufferReset(command_sequence);
error:
    return status;
}

static int32_t _VulkanRateGetIndexOfType(const char *type) {
    size_t count = sizeof(_vulkan_rate_type_map) / sizeof(const char *);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(_vulkan_rate_type_map[i], type) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int32_t _VulkanRateGetIndexOfOp(const char *op) {
    size_t count = sizeof(_vulkan_rate_op_map) / sizeof(const char *);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(_vulkan_rate_op_map[i], op) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

static const char *_VulkanRateGetTypeFromIndex(int32_t index) {
    size_t count = sizeof(_vulkan_rate_type_map) / sizeof(const char *);
    if (index < 0 || (size_t)index >= count) {
        return NULL;
    }
    return _vulkan_rate_type_map[index];
}

static const char *_VulkanRateGetOpFromIndex(int32_t index) {
    size_t count = sizeof(_vulkan_rate_op_map) / sizeof(const char *);
    if (index < 0 || (size_t)index >= count) {
        return NULL;
    }
    return _vulkan_rate_op_map[index];
}

static test_status _VulkanRateRegisterSubtest(const char *type, const char *op, const char *test_name, size_t datatype_size, uint32_t ops_per_cycle, uint32_t op_type) {
    int32_t type_index = _VulkanRateGetIndexOfType(type);
    int32_t op_index = _VulkanRateGetIndexOfOp(op);
    if (type_index < 0 || op_index < 0) {
        return TEST_PROGRAMMING_ERROR;
    }
    uint64_t config = (((uint64_t)op_type) << 56) | (((uint64_t)ops_per_cycle) << 48) | (((uint64_t)datatype_size) << 32) | (((uint64_t)type_index) << 16) | ((uint64_t)op_index);
    return VulkanRunnerRegisterTest(&_VulkanRateEntry, (void *)config, test_name, TESTS_VULKAN_RATE_VERSION, false);
}