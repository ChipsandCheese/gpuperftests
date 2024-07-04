// Copyright (c) 2021 - 2023, Nemes <nemes@nemez.net>
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
#include "runner.h"
#include "tests/test_vk_list.h"

#ifdef _WIN32
#include "sanitize_windows_h.h"
#include <Windows.h>
#include "windows_helper.h"
#endif

static test_status _VulkanListEntry(int32_t device_id, void *config_data) {
    TEST_UNUSED(device_id);
    TEST_UNUSED(config_data);
    uint32_t vulkan_version = VK_MAKE_VERSION(TEST_VER_MAJOR(TESTS_VULKAN_LIST_VERSION), TEST_VER_MINOR(TESTS_VULKAN_LIST_VERSION), TEST_VER_PATCH(TESTS_VULKAN_LIST_VERSION));

    vulkan_instance instance;
    test_status status = VulkanCreateInstance(false, TESTS_VULKAN_LIST_NAME, vulkan_version, &instance);
    TEST_RETFAIL(status);
    vulkan_physical_device *devices = NULL;
    uint32_t device_count = 0;
    status = VulkanGetPhysicalDevices(&instance, &devices, &device_count);
    if (!TEST_SUCCESS(status)) {
        VulkanDestroyInstance(&instance);
        return status;
    }
#ifdef _WIN32
    helper_arraylist win_devices;
    uint32_t win_device_count = 0;
    status = WindowsHelperInitializeGPUs(&win_devices, &win_device_count);
#endif
    for (uint32_t i = 0; i < device_count; i++) {
        size_t device_local_heap_size = 0;
        for (uint32_t j = 0; j < devices[i].physical_memory_properties.memoryProperties.memoryHeapCount; j++) {
            VkMemoryHeap heap = devices[i].physical_memory_properties.memoryProperties.memoryHeaps[j];

            if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                device_local_heap_size += heap.size;
            }
        }
#ifdef _WIN32
        bool is_rdp_gpu = false;
        uint64_t vulkan_luid = 0;
        status = HelperConvertByteArrayUint64(devices[i].physical_ID_properties.deviceLUID, VK_LUID_SIZE, &vulkan_luid);
        for (size_t k = 0; k < win_device_count; k++) {
            windows_helper_gpu* win_device = (windows_helper_gpu*)HelperArrayListGet(&win_devices, k);
            uint64_t windows_luid = WindowsHelperGPUGetLUID(win_device);
            if (vulkan_luid == windows_luid && !(strcmp(win_device->name, "Microsoft Remote Display Adapter"))) {
                is_rdp_gpu = true;
            } else {
                continue;
            }
        }
        if (is_rdp_gpu) {
            INFO("Detected Windows Virtual RDP GPU \"%s\", ignoring.\n", devices[i].physical_properties.properties.deviceName);
            continue;
        }
#endif
        LOG_RESULT(devices[i].device_index, "%s", "%llu", devices[i].physical_properties.properties.deviceName, device_local_heap_size);
    }
#ifdef _WIN32
    status = WindowsHelperCleanGPUs(&win_devices);
#endif 
    test_status cleanup_status = VulkanDestroyInstance(&instance);
    return TEST_SUCCESS(status) ? cleanup_status : status;
}

test_status TestsVulkanListRegister() {
    return RunnerRegisterTest(&_VulkanListEntry, NULL, TESTS_VULKAN_LIST_NAME, TESTS_VULKAN_LIST_VERSION);
}