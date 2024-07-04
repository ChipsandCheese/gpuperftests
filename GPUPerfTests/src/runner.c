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
#include "tests/test_vk_list.h"

static helper_arraylist test_list;

test_status RunnerRegisterTest(test_main *test_entry, void *config_data, const char *const test_name, uint32_t test_version) {
    if (test_entry == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    runner_test entry;
    entry.entry = test_entry;
    entry.config_data = config_data;
    entry.name = test_name;
    entry.version = test_version;
    return HelperArrayListAdd(&test_list, &entry, sizeof(runner_test), NULL);
}

test_status RunnerExecuteTests(const char *test_name, int32_t device_id) {
    size_t size = HelperArrayListSize(&test_list);
    for (uint32_t i = 0; i < size; i++) {
        runner_test *test_entry = (runner_test *)HelperArrayListGet(&test_list, i);
        if (strcmp(test_name, test_entry->name) == 0) {
            LOG("TESTV", "%s %u.%u.%u\n", test_entry->name, TEST_VER_MAJOR(test_entry->version), TEST_VER_MINOR(test_entry->version), TEST_VER_PATCH(test_entry->version));
            SEPARATOR();
            return test_entry->entry(device_id, test_entry->config_data);
        }
    }
    return TEST_UNKNOWN_TESTCASE;
}

test_status RunnerRegisterTests() {
    memset(&test_list, 0, sizeof(test_list));
    test_status status = TEST_OK;
    status = TestsVulkanListRegister();
    TEST_RETFAIL(status);
    status = VulkanRunnerRegisterTests();
    TEST_RETFAIL(status);
    return status;
}

test_status RunnerCleanUp() {
    return HelperArrayListClean(&test_list);
}

test_status RunnerPrintTests() {
    size_t size = HelperArrayListSize(&test_list);
    for (uint32_t i = 0; i < size; i++) {
        runner_test *test_entry = (runner_test *)HelperArrayListGet(&test_list, i);
        INFO("    %s (Version %u.%u.%u)\n", test_entry->name, TEST_VER_MAJOR(test_entry->version), TEST_VER_MINOR(test_entry->version), TEST_VER_PATCH(test_entry->version));
    }
    return TEST_OK;
}