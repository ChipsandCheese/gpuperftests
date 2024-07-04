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

#include "vulkan_helper.h"
#include "logger.h"

static const char *_vulkan_validation_layers[] = {
    "VK_LAYER_KHRONOS_validation"
};

static const char *_vulkan_debug_extensions[] = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME
};

static VKAPI_ATTR VkBool32 VKAPI_CALL _VulkanDebugReportEXTCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user);
static test_status _VulkanGetSupportedLayersGlobal(VkLayerProperties **layers, uint32_t *layer_count);

test_status VulkanCreateInstance(bool graphical, const char *test_name, uint32_t test_version, vulkan_instance *instance) {
    bool debug_mode = false; // in case I ever decide to turn this into an input arg
#ifdef _DEBUG
    debug_mode = true;
#endif
    VkResult res;
    test_status status = TEST_OK;
    /* PREPARE BASIC STRUCTURES */
    VkApplicationInfo application_info = {0};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = test_name;
    application_info.applicationVersion = test_version;
    application_info.pEngineName = GPUPERF_APP_NAME;
    application_info.engineVersion = VK_MAKE_VERSION(GPUPERF_VERSION_MAJOR, GPUPERF_VERSION_MINOR, GPUPERF_VERSION_PATCH);
    application_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_create_info = {0};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &application_info;

    /* GET REQUIRED EXTENSIONS */
    helper_arraylist required_extensions = {0};
    status = HelperArrayListInitialize(&required_extensions, sizeof(const char *));
    TEST_RETFAIL(status);
    if (graphical) {
        /* Get GLFW extensions */
    }
    if (debug_mode) {
        for (uint32_t i = 0; i < (sizeof(_vulkan_debug_extensions) / sizeof(const char *)); i++) {
            status = HelperArrayListAdd(&required_extensions, &(_vulkan_debug_extensions[i]), sizeof(_vulkan_debug_extensions[i]), NULL);
            if (!TEST_SUCCESS(status)) {
                HelperArrayListClean(&required_extensions);
                return status;
            }
        }
    }
    instance_create_info.enabledExtensionCount = (uint32_t)HelperArrayListSize(&required_extensions);
    instance_create_info.ppEnabledExtensionNames = (const char *const *)HelperArrayListRawData(&required_extensions);

    /* GET REQUIRED LAYERS */
    if (debug_mode) {
        VkLayerProperties *layers = NULL;
        uint32_t layer_count = 0;
        status = _VulkanGetSupportedLayersGlobal(&layers, &layer_count);
        TEST_RETFAIL(status);
        for (uint32_t i = 0; i < (sizeof(_vulkan_validation_layers) / sizeof(const char *)); i++) {
            if (!VulkanIsLayerSupported(_vulkan_validation_layers[i], layers, layer_count)) {
                free(layers);
                return TEST_VK_LAYER_UNSUPPORTED;
            }
        }
        free(layers);
        instance_create_info.enabledLayerCount = sizeof(_vulkan_validation_layers) / sizeof(const char *);
        instance_create_info.ppEnabledLayerNames = _vulkan_validation_layers;
    } else {
        instance_create_info.enabledLayerCount = 0;
        instance_create_info.ppEnabledLayerNames = NULL;
    }

    /* CREATE INSTANCE */
    res = vkCreateInstance(&instance_create_info, NULL, &(instance->instance));
    HelperArrayListClean(&required_extensions);
    VULKAN_RETFAIL(res, TEST_VK_CREATE_INSTANCE_ERROR);

    /* INITIALIZE DEBUG LOGGING EXTENSION */
    if (debug_mode) {
        instance->debug_utils_vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance->instance, "vkCreateDebugUtilsMessengerEXT");
        instance->debug_utils_vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance->instance, "vkDestroyDebugUtilsMessengerEXT");
        instance->debug_utils_vkSubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)vkGetInstanceProcAddr(instance->instance, "vkSubmitDebugUtilsMessageEXT");

        if (instance->debug_utils_vkCreateDebugUtilsMessengerEXT == NULL || instance->debug_utils_vkDestroyDebugUtilsMessengerEXT == NULL || instance->debug_utils_vkSubmitDebugUtilsMessageEXT == NULL) {
            vkDestroyInstance(instance->instance, NULL);
            return TEST_VK_DEBUG_UTILS_PROC_ADDR_ERROR;
        }
        VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info_ext = {0};
        debug_utils_messenger_create_info_ext.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_utils_messenger_create_info_ext.pfnUserCallback = _VulkanDebugReportEXTCallback;
        debug_utils_messenger_create_info_ext.pUserData = (void *)instance;
        debug_utils_messenger_create_info_ext.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_utils_messenger_create_info_ext.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        res = instance->debug_utils_vkCreateDebugUtilsMessengerEXT(instance->instance, &debug_utils_messenger_create_info_ext, NULL, &(instance->debug_utils_messenger));
        if (!VULKAN_SUCCESS(res)) {
            vkDestroyInstance(instance->instance, NULL);
            return TEST_VK_DEBUG_MESSENGER_CREATE_ERROR;
        }
        VkDebugUtilsMessengerCallbackDataEXT debug_utils_messenger_callback_data_ext = {0};
        debug_utils_messenger_callback_data_ext.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
        debug_utils_messenger_callback_data_ext.pMessage = GPUPERF_APP_NAME " is running in debug mode!";
        instance->debug_utils_vkSubmitDebugUtilsMessageEXT(instance->instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &debug_utils_messenger_callback_data_ext);
    }
    instance->debug_mode = debug_mode;
    return TEST_OK;
}

test_status VulkanDestroyInstance(vulkan_instance *instance) {
    if (instance->debug_mode) {
        instance->debug_utils_vkDestroyDebugUtilsMessengerEXT(instance->instance, instance->debug_utils_messenger, NULL);
    }
    vkDestroyInstance(instance->instance, NULL);
    return TEST_OK;
}

test_status VulkanGetSupportedExtensions(vulkan_physical_device *physical_device, VkExtensionProperties **extensions, uint32_t *extension_count) {
    if (extensions == NULL || extension_count == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VkResult res = vkEnumerateDeviceExtensionProperties(physical_device->physical_device, NULL, extension_count, NULL);
    VULKAN_RETFAIL(res, TEST_VK_EXTENSION_ENUMERATION_ERROR);
    *extensions = malloc((*extension_count) * sizeof(VkExtensionProperties));
    if (*extensions == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    memset(*extensions, 0, (*extension_count) * sizeof(VkExtensionProperties));
    res = vkEnumerateDeviceExtensionProperties(physical_device->physical_device, NULL, extension_count, *extensions);
    VULKAN_RETFAIL(res, TEST_VK_EXTENSION_ENUMERATION_ERROR);
    return TEST_OK;
}

bool VulkanIsExtensionSupported(const char *extension_name, VkExtensionProperties *extensions, uint32_t extension_count) {
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(extension_name, extensions[i].extensionName) == 0) {
            return true;
        }
    }
    return false;
}

test_status VulkanGetSupportedLayers(vulkan_physical_device *physical_device, VkLayerProperties **layers, uint32_t *layer_count) {
    if (layers == NULL || layer_count == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VkResult res = vkEnumerateDeviceLayerProperties(physical_device->physical_device, layer_count, NULL);
    VULKAN_RETFAIL(res, TEST_VK_LAYER_ENUMERATION_ERROR);
    *layers = malloc((*layer_count) * sizeof(VkLayerProperties));
    if (*layers == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    memset(*layers, 0, (*layer_count) * sizeof(VkLayerProperties));
    res = vkEnumerateDeviceLayerProperties(physical_device->physical_device, layer_count, *layers);
    VULKAN_RETFAIL(res, TEST_VK_LAYER_ENUMERATION_ERROR);
    return TEST_OK;
}

bool VulkanIsLayerSupported(const char *layer_name, VkLayerProperties *layers, uint32_t layer_count) {
    for (uint32_t i = 0; i < layer_count; i++) {
        if (strcmp(layer_name, layers[i].layerName) == 0) {
            return true;
        }
    }
    return false;
}

test_status VulkanGetPhysicalDevices(vulkan_instance *instance, vulkan_physical_device **devices, uint32_t *device_count) {
    if (devices == NULL || device_count == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VkResult res = vkEnumeratePhysicalDevices(instance->instance, device_count, NULL);
    VULKAN_RETFAIL(res, TEST_VK_DEVICE_ENUMERATION_ERROR);
    VkPhysicalDevice *physical_devices = malloc((*device_count) * sizeof(VkPhysicalDevice));
    if (physical_devices == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    res = vkEnumeratePhysicalDevices(instance->instance, device_count, physical_devices);
    if (!VULKAN_SUCCESS(res)) {
        free(physical_devices);
        return TEST_VK_DEVICE_ENUMERATION_ERROR;
    }
    *devices = malloc((*device_count) * sizeof(vulkan_physical_device));
    if (*devices == NULL) {
        free(physical_devices);
        return TEST_OUT_OF_MEMORY;
    }
    memset(*devices, 0, (*device_count) * sizeof(vulkan_physical_device));
    for (uint32_t i = 0; i < *device_count; i++) {
        vulkan_physical_device *device = &((*devices)[i]);
        VkPhysicalDevice *physical_device = &(physical_devices[i]);
        memcpy(&(device->physical_device), physical_device, sizeof(VkPhysicalDevice));
        device->physical_ID_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        device->physical_ID_properties.pNext = NULL;
        device->physical_maintenance_properties_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
        device->physical_maintenance_properties_3.pNext = &(device->physical_ID_properties);
        device->physical_properties_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
        device->physical_properties_vk12.pNext = &(device->physical_maintenance_properties_3);
        device->physical_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        device->physical_properties.pNext = &(device->physical_properties_vk12);
        vkGetPhysicalDeviceProperties2(*physical_device, &(device->physical_properties));
        device->physical_features_vk12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        device->physical_features_vk12.pNext = NULL;
        device->physical_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        device->physical_features.pNext = &(device->physical_features_vk12);
        vkGetPhysicalDeviceFeatures2(*physical_device, &(device->physical_features));
        device->physical_memory_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        device->physical_memory_properties.pNext = NULL;
        vkGetPhysicalDeviceMemoryProperties2(*physical_device, &(device->physical_memory_properties));
        device->device_index = i;
    }
    free(physical_devices);
    return TEST_OK;
}

const char *VulkanTranslateVendorID(uint32_t vendor_id) {
    switch (vendor_id) {
    case 0x1002:
        return "Advanced Micro Devices, Inc.";
    case 0x10DE:
        return "NVIDIA Corporation";
    case 0x8086:
        return "Intel Corporation";
    case 0x13B5:
        return "Arm Limited";
    case 0x1010:
        return "Imagination Technologies";
    case 0x5143:
        return "Qualcomm Technologies, Inc.";
    case 0x14E4:
        return "Broadcom Inc.";
    case VK_VENDOR_ID_VIV:
        return "Vivante";
    case VK_VENDOR_ID_VSI:
        return "VeriSilicon";
    case VK_VENDOR_ID_KAZAN:
        return "Kazan Software Renderer";
    case VK_VENDOR_ID_CODEPLAY:
        return "Codeplay Software Ltd.";
    case VK_VENDOR_ID_MESA:
        return "Mesa";
    case 0x10006:
        return "PoCL";
    default:
        return "Unknown Vendor";
    }
}

const char *VulkanTranslateDriverID(uint32_t driver_id) {
    switch (driver_id) {
    case VK_DRIVER_ID_AMD_PROPRIETARY:
        return "AMD Proprietary";
    case VK_DRIVER_ID_AMD_OPEN_SOURCE:
        return "AMD Open Source";
    case VK_DRIVER_ID_MESA_RADV:
        return "Mesa RADV";
    case VK_DRIVER_ID_NVIDIA_PROPRIETARY:
        return "NVIDIA Proprietary";
    case VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS:
        return "Intel Proprietary";
    case VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA:
        return "Intel Open Source";
    case VK_DRIVER_ID_IMAGINATION_PROPRIETARY:
        return "Imagination Proprietary";
    case VK_DRIVER_ID_QUALCOMM_PROPRIETARY:
        return "Qualcomm Proprietary";
    case VK_DRIVER_ID_ARM_PROPRIETARY:
        return "ARM Proprietary";
    case VK_DRIVER_ID_GOOGLE_SWIFTSHADER:
        return "Google SwiftShader";
    case VK_DRIVER_ID_GGP_PROPRIETARY:
        return "Google Games Platform Proprietary";
    case VK_DRIVER_ID_BROADCOM_PROPRIETARY:
        return "Broadcom Proprietary";
    case VK_DRIVER_ID_MESA_LLVMPIPE:
        return "Mesa LLVMpipe";
    case VK_DRIVER_ID_MOLTENVK:
        return "MoltenVK";
    default:
        return "Unknown Driver";
    }
}

const char *VulkanTranslateDeviceType(VkPhysicalDeviceType device_type) {
    switch (device_type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return "Other Device Type";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    default:
        return "Unknown Device Type";
    }
}

test_status VulkanGetPhysicalQueueFamilyProperties(vulkan_physical_device *device, VkQueueFamilyProperties **queue_family_properties, uint32_t *queue_family_count) {
    if (queue_family_properties == NULL || queue_family_count == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, queue_family_count, NULL);
    *queue_family_properties = malloc((*queue_family_count) * sizeof(VkQueueFamilyProperties));
    if (queue_family_properties == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    vkGetPhysicalDeviceQueueFamilyProperties(device->physical_device, queue_family_count, *queue_family_properties);
    return TEST_OK;
}

test_status VulkanSelectQueueFamilyExact(uint32_t *queue_index, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags, uint32_t match_mask) {
    if (queue_index == NULL || queue_family_properties == NULL || queue_family_count == 0) {
        return TEST_INVALID_PARAMETER;
    }
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (((queue_family_properties[i].queueFlags & match_mask) ^ required_flags) == 0) {
            *queue_index = i;
            return TEST_OK;
        }
    }
    return TEST_VK_QUEUE_NOT_FOUND;
}

test_status VulkanSelectQueueFamily(uint32_t *queue_index, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags) {
    return VulkanSelectQueueFamilyExact(queue_index, queue_family_properties, queue_family_count, required_flags, required_flags);
}

test_status VulkanSelectQueueFamilyMultipleExact(uint32_t **queue_indices, uint32_t *selected_queue_count, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags, uint32_t match_mask) {
    if (queue_indices == NULL || selected_queue_count == NULL || queue_family_properties == NULL || queue_family_count == 0) {
        return TEST_INVALID_PARAMETER;
    }
    uint32_t suitable_queue_count = 0;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (((queue_family_properties[i].queueFlags & match_mask) ^ required_flags) == 0) {
            suitable_queue_count++;
        }
    }
    if (suitable_queue_count == 0) {
        return TEST_VK_QUEUE_NOT_FOUND;
    }
    uint32_t *queues = malloc(suitable_queue_count * sizeof(uint32_t));
    if (queues == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0, j = 0; i < queue_family_count; i++) {
        if (((queue_family_properties[i].queueFlags & match_mask) ^ required_flags) == 0) {
            queues[j] = i;
            j++;
        }
    }
    *queue_indices = queues;
    *selected_queue_count = suitable_queue_count;
    return TEST_OK;
}

test_status VulkanSelectQueueFamilyMultiple(uint32_t **queue_indices, uint32_t *selected_queue_count, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags) {
    return VulkanSelectQueueFamilyMultipleExact(queue_indices, selected_queue_count, queue_family_properties, queue_family_count, required_flags, required_flags);
}

test_status VulkanCreateDeviceWithQueues(vulkan_physical_device *physical_device, VkQueueFamilyProperties *queue_family_properties, uint32_t *queue_family_indices, uint32_t *queue_counts, uint32_t queue_family_count, vulkan_device *device, const void *pNext) {
    if (physical_device == NULL || device == NULL || queue_family_properties == NULL || queue_family_indices == NULL || queue_family_count == 0) {
        return TEST_INVALID_PARAMETER;
    }
    VkDeviceQueueCreateInfo *device_queue_create_infos = malloc(queue_family_count * sizeof(VkDeviceQueueCreateInfo));
    if (device_queue_create_infos == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    memset(device_queue_create_infos, 0, queue_family_count * sizeof(VkDeviceQueueCreateInfo));

    vulkan_queue_family *queue_families = malloc(queue_family_count * sizeof(vulkan_queue_family));
    if (queue_families == NULL) {
        free(device_queue_create_infos);
        return TEST_OUT_OF_MEMORY;
    }
    uint32_t total_queue_count = 0;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        queue_families[i].family_index = queue_family_indices[i];
        queue_families[i].queue_offset = total_queue_count;
        if (queue_counts == NULL || queue_counts[i] == VULKAN_QUEUES_ALL) {
            queue_families[i].queue_count = queue_family_properties[queue_family_indices[i]].queueCount;
        } else {
            queue_families[i].queue_count = queue_counts[i];
        }
        total_queue_count += queue_families[i].queue_count;
    }
    float *queue_priorities = malloc(total_queue_count * sizeof(float));
    if (queue_priorities == NULL) {
        free(device_queue_create_infos);
        free(queue_families);
        return TEST_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < total_queue_count; i++) {
        queue_priorities[i] = 1.0f;
    }
    for (uint32_t i = 0; i < queue_family_count; i++) {
        device_queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        device_queue_create_infos[i].pNext = NULL;
        device_queue_create_infos[i].queueFamilyIndex = queue_families[i].family_index;
        device_queue_create_infos[i].queueCount = queue_families[i].queue_count;
        device_queue_create_infos[i].pQueuePriorities = &(queue_priorities[queue_families[i].queue_offset]);
    }
    VkDeviceCreateInfo device_create_info = {0};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = pNext;
    device_create_info.queueCreateInfoCount = queue_family_count;
    device_create_info.pQueueCreateInfos = device_queue_create_infos;
    device_create_info.pEnabledFeatures = NULL;

    VkResult res = vkCreateDevice(physical_device->physical_device, &device_create_info, NULL, &(device->device));
    free(queue_priorities);
    free(device_queue_create_infos);
    if (res == VK_ERROR_FEATURE_NOT_PRESENT) {
        free(queue_families);
        return TEST_VK_FEATURE_UNSUPPORTED;
    } else if (!VULKAN_SUCCESS(res)) {
        free(queue_families);
        return TEST_VK_DEVICE_CREATE_ERROR;
    }
    device->physical_device = physical_device;
    device->queue_families = queue_families;
    device->queue_family_count = queue_family_count;
    device->total_queue_count = total_queue_count;
    device->queues = malloc(total_queue_count * sizeof(VkQueue));
    if (device->queues == NULL) {
        vkDestroyDevice(device->device, NULL);
        free(queue_families);
        return TEST_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < queue_family_count; i++) {
        for (uint32_t j = 0; j < queue_families[i].queue_count; j++) {
            vkGetDeviceQueue(device->device, queue_families[i].family_index, j, &(device->queues[queue_families[i].queue_offset + j]));
        }
    }
    return TEST_OK;
}

test_status VulkanCreateDeviceWithQueue(vulkan_physical_device *physical_device, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_index, uint32_t queue_count, vulkan_device *device, const void *pNext) {
    uint32_t queue_family_indices = queue_family_index;
    uint32_t queue_counts = queue_count;
    return VulkanCreateDeviceWithQueues(physical_device, queue_family_properties, &queue_family_indices, &queue_counts, 1, device, pNext);
}

test_status VulkanCreateDevice(vulkan_physical_device *physical_device, VkQueueFamilyProperties *queue_family_properties, uint32_t queue_family_count, VkQueueFlags required_flags, vulkan_device *device, const void *pNext) {
    uint32_t i = 0;
    test_status status = VulkanSelectQueueFamily(&i, queue_family_properties, queue_family_count, required_flags);
    TEST_RETFAIL(status);

    return VulkanCreateDeviceWithQueue(physical_device, queue_family_properties, i, 1, device, pNext);
}

test_status VulkanDestroyDevice(vulkan_device *device) {
    free(device->queues);
    vkDestroyDevice(device->device, NULL);
    return TEST_OK;
}

test_status VulkanCalculateWorkgroupDispatch(vulkan_device *device, uint64_t total_workgroups, uint32_t *x, uint32_t *y, uint32_t *z) {
    if (device == NULL || x == NULL || y == NULL || z == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    uint32_t groups_x = 1;
    uint32_t groups_y = 1;
    uint32_t groups_z = 1;
    if (total_workgroups == 0) {
        groups_x = 0;
        groups_y = 0;
        groups_z = 0;
    } else {
        uint32_t *group_size_limits = device->physical_device->physical_properties.properties.limits.maxComputeWorkGroupCount;

        uint64_t groups_max_x = HelperFindLargestPowerOfTwo(group_size_limits[0]);
        uint64_t groups_max_y = HelperFindLargestPowerOfTwo(group_size_limits[1]);
        uint64_t groups_max_z = HelperFindLargestPowerOfTwo(group_size_limits[2]);
        uint64_t workgroups = total_workgroups;
        if (workgroups > groups_max_x) {
            groups_x = (uint32_t)groups_max_x;
            workgroups /= groups_max_x;
            if (workgroups > groups_max_y) {
                groups_y = (uint32_t)groups_max_y;
                workgroups /= groups_max_y;
                if (workgroups > groups_max_z) {
                    groups_z = (uint32_t)groups_max_z;
                } else {
                    groups_z = (uint32_t)workgroups;
                }
            } else {
                groups_y = (uint32_t)workgroups;
            }
        } else {
            groups_x = (uint32_t)workgroups;
        }
    }
    *x = groups_x;
    *y = groups_y;
    *z = groups_z;

    if (((uint64_t)groups_x * (uint64_t)groups_y * (uint64_t)groups_z) != total_workgroups) {
        return TEST_VK_WORKGROUP_COUNT_NOT_DIVISIBLE;
    } else {
        return TEST_OK;
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL _VulkanDebugReportEXTCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
    TEST_UNUSED(user);
    const char *message_type_string;
    switch (message_type) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            message_type_string = "GENERAL";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            message_type_string = "VALIDATION";
            break;
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            message_type_string = "PERFORMANCE";
            break;
        default:
            message_type_string = "UNKNOWN_TYPE";
            break;
    }
    switch (message_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        TRACE("VK %s: %s\n", message_type_string, data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        DEBUG("VK %s: %s\n", message_type_string, data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        WARNING("VK %s: %s\n", message_type_string, data->pMessage);
        break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        FATAL("VK %s: %s\n", message_type_string, data->pMessage);
        break;
    }
    return ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0) ? VK_FALSE : VK_TRUE;
}

static test_status _VulkanGetSupportedLayersGlobal(VkLayerProperties **layers, uint32_t *layer_count) {
    if (layers == NULL || layer_count == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    VkResult res = vkEnumerateInstanceLayerProperties(layer_count, NULL);
    VULKAN_RETFAIL(res, TEST_VK_LAYER_ENUMERATION_ERROR);
    *layers = malloc((*layer_count) * sizeof(VkLayerProperties));
    if (layers == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    res = vkEnumerateInstanceLayerProperties(layer_count, *layers);
    if (!VULKAN_SUCCESS(res)) {
        free(*layers);
        return TEST_VK_LAYER_ENUMERATION_ERROR;
    }
    return TEST_OK;
}