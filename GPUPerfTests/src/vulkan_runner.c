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
#include "runner.h"
#include "vulkan_helper.h"
#include "vulkan_runner.h"
#include "tests/test_vk_info.h"
#include "tests/test_vk_bandwidth.h"
#include "tests/test_vk_latency.h"
#include "tests/test_vk_rate.h"
#include "tests/test_vk_uplink.h"

static test_status _VulkanRunnerEntry(int32_t device_id, void *config_data);

test_status VulkanRunnerRegisterTests() {
    test_status status = TEST_OK;
    status = TestsVulkanInfoRegister();
    TEST_RETFAIL(status);
    status = TestsVulkanBandwidthRegister();
    TEST_RETFAIL(status);
    status = TestsVulkanLatencyRegister();
    TEST_RETFAIL(status);
    status = TestsVulkanRateRegister();
    TEST_RETFAIL(status);
    status = TestsVulkanUplinkRegister();
    TEST_RETFAIL(status);
    return status;
}

test_status VulkanRunnerRegisterTest(vulkan_test_main *entrypoint, void *config_data, const char *const test_name, uint32_t test_version, bool graphical_context) {
    vulkan_runner_context *context = malloc(sizeof(vulkan_runner_context));
    if (context == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    memset(context, 0, sizeof(vulkan_runner_context));
    context->entrypoint = entrypoint;
    context->config_data = config_data;
    context->name = test_name;
    context->version = test_version;
    context->graphical = graphical_context;

    return RunnerRegisterTest(&_VulkanRunnerEntry, (void *)context, test_name, test_version);
}

static test_status _VulkanRunnerEntry(int32_t device_id, void *config_data) {
    vulkan_runner_context *context = (vulkan_runner_context *)config_data;
    uint32_t vulkan_version = VK_MAKE_VERSION(TEST_VER_MAJOR(context->version), TEST_VER_MINOR(context->version), TEST_VER_PATCH(context->version));

    vulkan_instance instance;
    test_status status = VulkanCreateInstance(context->graphical, context->name, vulkan_version, &instance);
    TEST_RETFAIL(status);
    vulkan_physical_device *devices = NULL;
    uint32_t device_count = 0;
    status = VulkanGetPhysicalDevices(&instance, &devices, &device_count);
    if (!TEST_SUCCESS(status)) {
        VulkanDestroyInstance(&instance);
        return status;
    }
    for (uint32_t i = 0; i < device_count; i++) {
        if (device_id == -1 || (uint32_t)device_id == i) {
            status = context->entrypoint(&(devices[i]), context->config_data);
            if (!TEST_SUCCESS(status)) {
                break;
            }
        }
    }
    test_status cleanup_status = VulkanDestroyInstance(&instance);
    return TEST_SUCCESS(status) ? cleanup_status : status;
}