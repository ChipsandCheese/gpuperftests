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

#ifndef VULKAN_HELPER_H
#define VULKAN_HELPER_H

#include "main.h"
#include <vulkan/vulkan.h>
#include "helper.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VULKAN_SUCCESS(status)              ((status) == VK_SUCCESS)
#define VULKAN_RETFAIL(status, retstatus)   if (!VULKAN_SUCCESS((status))) { WARNING("Encountered a Vulkan error: %li\n", status); return (retstatus); }

// Helper value for VulkanCreateDeviceWithQueue and VulkanCommandBuffer*
#define VULKAN_QUEUES_ALL                   (0xFFFFFFFFUL)

typedef struct vulkan_queue_family_t {
    uint32_t family_index;
    uint32_t queue_offset;
    uint32_t queue_count;
} vulkan_queue_family;

typedef struct vulkan_physical_device_t {
    VkPhysicalDevice physical_device;
    VkPhysicalDeviceProperties2 physical_properties;
    VkPhysicalDeviceIDProperties physical_ID_properties;
    VkPhysicalDeviceFeatures2 physical_features;
    VkPhysicalDeviceVulkan12Properties physical_properties_vk12;
    VkPhysicalDeviceVulkan12Features physical_features_vk12;
    VkPhysicalDeviceMemoryProperties2 physical_memory_properties;
    VkPhysicalDeviceMaintenance3Properties physical_maintenance_properties_3;
    uint32_t device_index;
} vulkan_physical_device;

typedef struct vulkan_device_t {
    vulkan_physical_device *physical_device;
    VkDevice device;
    vulkan_queue_family *queue_families;
    uint32_t queue_family_count;
    uint32_t total_queue_count;
    VkQueue *queues;
} vulkan_device;

typedef struct vulkan_instance_t {
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_utils_messenger;
    PFN_vkCreateDebugUtilsMessengerEXT debug_utils_vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT debug_utils_vkDestroyDebugUtilsMessengerEXT;
    PFN_vkSubmitDebugUtilsMessageEXT debug_utils_vkSubmitDebugUtilsMessageEXT;
    bool debug_mode;
} vulkan_instance;

test_status VulkanCreateInstance(bool graphical, const char *test_name, uint32_t test_version, vulkan_instance *instance);
test_status VulkanDestroyInstance(vulkan_instance *instance);
test_status VulkanGetSupportedExtensions(vulkan_physical_device *physical_device, VkExtensionProperties **extensions, uint32_t *extension_count);
bool VulkanIsExtensionSupported(const char *extension_name, VkExtensionProperties *extensions, uint32_t extension_count);
test_status VulkanGetSupportedLayers(vulkan_physical_device *physical_device, VkLayerProperties **layers, uint32_t *layer_count);
bool VulkanIsLayerSupported(const char *layer_name, VkLayerProperties *layers, uint32_t layer_count);
test_status VulkanGetPhysicalDevices(vulkan_instance *instance, vulkan_physical_device **devices, uint32_t *device_count);
const char *VulkanTranslateVendorID(uint32_t vendor_id);
const char *VulkanTranslateDriverID(uint32_t driver_id);
const char *VulkanTranslateDeviceType(VkPhysicalDeviceType device_type);
test_status VulkanGetPhysicalQueueFamilyProperties(vulkan_physical_device *device, VkQueueFamilyProperties **queue_family_properties, uint32_t *queue_family_count);
test_status VulkanSelectQueueFamilyExact(uint32_t *queue_index, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags, uint32_t match_mask);
test_status VulkanSelectQueueFamily(uint32_t *queue_index, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags);
test_status VulkanSelectQueueFamilyMultipleExact(uint32_t **queue_indices, uint32_t *selected_queue_count, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags, uint32_t match_mask);
test_status VulkanSelectQueueFamilyMultiple(uint32_t **queue_indices, uint32_t *selected_queue_count, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags);
test_status VulkanCreateDeviceWithQueues(vulkan_physical_device *physical_device, VkQueueFamilyProperties *queue_family_properties, uint32_t *queue_family_indices, uint32_t *queue_counts, uint32_t queue_family_count, vulkan_device *device, const void *pNext);
test_status VulkanCreateDeviceWithQueue(vulkan_physical_device *physical_device, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_index, uint32_t queue_count, vulkan_device *device, const void *pNext);
test_status VulkanCreateDevice(vulkan_physical_device *physical_device, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags, vulkan_device *device, const void *pNext);
test_status VulkanDestroyDevice(vulkan_device *device);
test_status VulkanCalculateWorkgroupDispatch(vulkan_device *device, uint64_t total_workgroups, uint32_t *x, uint32_t *y, uint32_t *z);

#ifdef __cplusplus
}
#endif
#endif
