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
#include "tests/test_vk_info.h"

static test_status _VulkanInfoEntry(vulkan_physical_device *device, void *config_data) {
    test_status status = TEST_OK;

    INFO("Device %u:\n", device->device_index);
        
    uint32_t api_version = device->physical_properties.properties.apiVersion;
    INFO("    Device Name: %s\n", device->physical_properties.properties.deviceName);
    INFO("    API Version: %u.%u.%u\n", VK_VERSION_MAJOR(api_version), VK_VERSION_MINOR(api_version), VK_VERSION_PATCH(api_version));
    INFO("    Driver Version: %s\n", device->physical_properties_vk12.driverInfo);
    INFO("    Driver ID: 0x%08x (%s)\n", device->physical_properties_vk12.driverID, VulkanTranslateDriverID(device->physical_properties_vk12.driverID));
    INFO("    Vendor ID: 0x%08x (%s)\n", device->physical_properties.properties.vendorID, VulkanTranslateVendorID(device->physical_properties.properties.vendorID));
    INFO("    Device ID: 0x%08x\n", device->physical_properties.properties.deviceID);
    INFO("    Device Type: 0x%08x (%s)\n", device->physical_properties.properties.deviceType, VulkanTranslateDeviceType(device->physical_properties.properties.deviceType));

    INFO("    Memory Types:\n");
    for (uint32_t j = 0; j < device->physical_memory_properties.memoryProperties.memoryTypeCount; j++) {
        VkMemoryType type = device->physical_memory_properties.memoryProperties.memoryTypes[j];

        char flags_string[7];
        flags_string[0] = ((type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) ? 'D' : '-';
        flags_string[1] = ((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) ? 'V' : '-';
        flags_string[2] = ((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) ? 'C' : '-';
        flags_string[3] = ((type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0) ? 'K' : '-';
        flags_string[4] = ((type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0) ? 'L' : '-';
        flags_string[5] = ((type.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0) ? 'P' : '-';
        flags_string[6] = '\0';
        INFO("        %u: Heap %4u %s\n", j, type.heapIndex, flags_string);
    }
    INFO("    Memory Heaps:\n");
    for (uint32_t j = 0; j < device->physical_memory_properties.memoryProperties.memoryHeapCount; j++) {
        VkMemoryHeap heap = device->physical_memory_properties.memoryProperties.memoryHeaps[j];

        helper_unit_pair heap_size;
        HelperConvertUnitsBytes1024(heap.size, &heap_size);
        char flags_string[3];
        flags_string[0] = ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) ? 'D' : '-';
        flags_string[1] = ((heap.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) != 0) ? 'I' : '-';
        flags_string[2] = '\0';
        INFO("        %u: %8.3f %s %s\n", j, heap_size.value, heap_size.units, flags_string);
    }

    VkQueueFamilyProperties *queue_family_properties = NULL;
    uint32_t queue_family_count = 0;
    status = VulkanGetPhysicalQueueFamilyProperties(device, &queue_family_properties, &queue_family_count);
    if (!TEST_SUCCESS(status)) {
        FATAL("    Failed To Get Queue Families Info\n");
        return status;
    }
    INFO("    Queue Families:\n");
    for (uint32_t i = 0; i < queue_family_count; i++) {
        VkQueueFlags flags = queue_family_properties[i].queueFlags;
        char flags_string[6];
        flags_string[0] = ((flags & VK_QUEUE_GRAPHICS_BIT) != 0) ? 'G' : '-';
        flags_string[1] = ((flags & VK_QUEUE_COMPUTE_BIT) != 0) ? 'C' : '-';
        flags_string[2] = ((flags & VK_QUEUE_TRANSFER_BIT) != 0) ? 'T' : '-';
        flags_string[3] = ((flags & VK_QUEUE_SPARSE_BINDING_BIT) != 0) ? 'B' : '-';
        flags_string[4] = ((flags & VK_QUEUE_PROTECTED_BIT) != 0) ? 'P' : '-';
        flags_string[5] = '\0';
        INFO("        %u: %4ux %s\n", i, queue_family_properties[i].queueCount, flags_string);
    }
    free(queue_family_properties);
    uint32_t layer_count = 0;
    VkLayerProperties *layers = NULL;
    status = VulkanGetSupportedLayers(device, &layers, &layer_count);
    if (!TEST_SUCCESS(status)) {
        FATAL("    Failed To Get Layers Info\n");
        return status;
    }
    INFO("    Supported Layers:\n");
    for (uint32_t i = 0; i < layer_count; i++) {
        INFO("        %s\n", layers[i].layerName);
    }
    free(layers);

    uint32_t extension_count = 0;
    VkExtensionProperties *extensions = NULL;
    status = VulkanGetSupportedExtensions(device, &extensions, &extension_count);
    if (!TEST_SUCCESS(status)) {
        FATAL("    Failed To Get Extensions Info\n");
        return status;
    }
    INFO("    Supported Extensions:\n");
    for (uint32_t i = 0; i < extension_count; i++) {
        INFO("        %s\n", extensions[i].extensionName);
    }
    free(extensions);

    return TEST_OK;
}

test_status TestsVulkanInfoRegister() {
    return VulkanRunnerRegisterTest(&_VulkanInfoEntry, NULL, TESTS_VULKAN_INFO_NAME, TESTS_VULKAN_INFO_VERSION, false);
}