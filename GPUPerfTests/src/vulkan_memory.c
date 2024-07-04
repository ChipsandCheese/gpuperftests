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
#include "vulkan_memory.h"

#ifdef VULKAN_MEMORY_TRACE
#define TRACE_MEMORY(format, ...)   TRACE("[MEMORY] " format, __VA_ARGS__)
#else
#define TRACE_MEMORY(format, ...)
#endif

static void _VulkanMemoryDestroyBuffers(vulkan_memory *memory_handle);
static test_status _VulkanMemoryAddTextureGeneric(vulkan_memory *memory_handle, uint32_t width, uint32_t height, uint32_t depth, VkFormat texture_format, uint32_t mipmaps, const char *texture_name, uint32_t texture_type);

test_status VulkanMemoryInitializeForQueues(vulkan_device *device, uint32_t *queue_family_indices, uint32_t queue_family_count, uint32_t memory_type, vulkan_memory *memory_handle) {
    TRACE_MEMORY("Initializing memory pool 0x%p (type: %08x)\n", memory_handle, memory_type);
    if (memory_handle == NULL || device == NULL || queue_family_indices == NULL || queue_family_count == 0 || queue_family_count > device->queue_family_count) {
        return TEST_INVALID_PARAMETER;
    }
    memset(memory_handle, 0, sizeof(vulkan_memory));
    memory_handle->device = device;
    memory_handle->memory = VK_NULL_HANDLE;
    memory_handle->memory_type_flags = memory_type;
    memory_handle->total_size = 0;
    memory_handle->supported_memory_types_mask = 0xFFFFFFFFUL;
    memory_handle->mapped_pool_base = NULL;
    memory_handle->mapped_region_count = 0;
    memory_handle->queue_family_count = queue_family_count;
    memory_handle->queue_family_indices = malloc(queue_family_count * sizeof(uint32_t));
    if (memory_handle->queue_family_indices == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < queue_family_count; i++) {
        memory_handle->queue_family_indices[i] = device->queue_families[i].family_index;
    }
    test_status status = TEST_OK;
    for (uint32_t i = 0; i <= VULKAN_REGION_TYPE_MAX; i++) {
        status = HelperArrayListInitialize(&(memory_handle->buffer_list[i]), sizeof(vulkan_buffer));
        TEST_RETFAIL(status);
    }
    return status;
}

test_status VulkanMemoryInitialize(vulkan_device *device, uint32_t memory_type, vulkan_memory *memory_handle) {
    uint32_t queue_family_index = 0;
    return VulkanMemoryInitializeForQueues(device, &queue_family_index, 1, memory_type, memory_handle);
}

test_status VulkanMemoryCleanUp(vulkan_memory *memory_handle) {
    TRACE_MEMORY("Cleaning up memory pool 0x%p\n", memory_handle);
    if (memory_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (memory_handle->memory != VK_NULL_HANDLE) {
        return TEST_VK_BACKING_STILL_ALLOCATED;
    }
    if (memory_handle->mapped_pool_base != NULL) {
        memory_handle->mapped_pool_base = NULL;
        vkUnmapMemory(memory_handle->device->device, memory_handle->memory);
    }
    _VulkanMemoryDestroyBuffers(memory_handle);
    for (uint32_t i = 0; i <= VULKAN_REGION_TYPE_MAX; i++) {
        HelperArrayListClean(&(memory_handle->buffer_list[i]));
    }
    return TEST_OK;
}

bool VulkanMemoryIsAllocated(vulkan_memory *memory_handle) {
    return memory_handle->memory != VK_NULL_HANDLE;
}

test_status VulkanMemoryAddRegion(vulkan_memory *memory_handle, size_t region_size, const char *region_name, uint32_t region_type, uint32_t region_flags) {
    TRACE_MEMORY("Adding region to memory pool 0x%p (size: %llu, name: \"%s\", type: %lu, flags: %08x)\n", memory_handle, region_size, region_name, region_type, region_flags);
    if (memory_handle == NULL || region_name == NULL || region_size == 0 || region_type > VULKAN_REGION_TYPE_MAX) {
        return TEST_INVALID_PARAMETER;
    }
    if (memory_handle->memory != VK_NULL_HANDLE) {
        return TEST_VK_BACKING_ALREADY_ALLOCATED;
    }
    size_t name_length = strlen(region_name);
    if (name_length >= VULKAN_REGION_MAX_NAME_LENGTH) {
        return TEST_VK_BUFFER_NAME_TOO_LONG;
    }
    /* Find a backing buffer of the same type and with the same flags as the requested region */
    vulkan_buffer *backing_buffer = NULL;
    size_t buffer_count = HelperArrayListSize(&(memory_handle->buffer_list[region_type]));
    for (uint32_t i = 0; i < (uint32_t)buffer_count; i++) {
        vulkan_buffer *candidate_buffer = HelperArrayListGet(&(memory_handle->buffer_list[region_type]), i);

        if (candidate_buffer->buffer_flags == region_flags) {
            backing_buffer = candidate_buffer;
            TRACE_MEMORY("Found backing buffer for region \"%s\" (pool: 0x%p, flags: %08x, type: %lu, index: %lu)\n", region_name, backing_buffer->memory_pool, backing_buffer->buffer_flags, region_type, i);
            break;
        }
    }
    if (backing_buffer == NULL) {
        vulkan_buffer new_buffer = {0};
        new_buffer.memory_pool = memory_handle;
        new_buffer.buffer = VK_NULL_HANDLE;
        new_buffer.buffer_flags = region_flags;
        switch (region_type) {
        case VULKAN_REGION_STORAGE:
        case VULKAN_REGION_INDEX:
        case VULKAN_REGION_VERTEX:
            new_buffer.required_region_alignment = memory_handle->device->physical_device->physical_properties.properties.limits.minStorageBufferOffsetAlignment;
            break;
        case VULKAN_REGION_UNIFORM:
            new_buffer.required_region_alignment = memory_handle->device->physical_device->physical_properties.properties.limits.minUniformBufferOffsetAlignment;
            break;
        default:
            return TEST_VK_UNKNOWN_BUFFER_TYPE;
        }
        TRACE_MEMORY("Creating backing buffer for region \"%s\" (pool: 0x%p, flags: %08x, type: %lu, alignment: %lu)\n", region_name, new_buffer.memory_pool, new_buffer.buffer_flags, region_type, new_buffer.required_region_alignment);

        size_t buffer_index = 0;
        test_status status = HelperArrayListAdd(&(memory_handle->buffer_list[region_type]), &new_buffer, sizeof(new_buffer), &buffer_index);
        TEST_RETFAIL(status);
        backing_buffer = (vulkan_buffer *)HelperArrayListGet(&(memory_handle->buffer_list[region_type]), buffer_index);
    }

    vulkan_region region = {0};
    region.size = region_size;
    region.offset = 0;
    region.backing_buffer = VK_NULL_HANDLE;
    region.memory_pool = memory_handle;
    region.is_mapped = false;
    strcpy((char *)region.name, region_name);

    return HelperArrayListAdd(&(backing_buffer->regions), &region, sizeof(region), NULL);
}

test_status VulkanMemoryAddTexture1D(vulkan_memory *memory_handle, uint32_t width, VkFormat texture_format, uint32_t mipmaps, const char *texture_name) {
    return _VulkanMemoryAddTextureGeneric(memory_handle, width, 1, 1, texture_format, mipmaps, texture_name, VULKAN_REGION_TEXTURE_1D);
}

test_status VulkanMemoryAddTexture2D(vulkan_memory *memory_handle, uint32_t width, uint32_t height, VkFormat texture_format, uint32_t mipmaps, const char *texture_name) {
    return _VulkanMemoryAddTextureGeneric(memory_handle, width, height, 1, texture_format, mipmaps, texture_name, VULKAN_REGION_TEXTURE_2D);
}

test_status VulkanMemoryAddTexture3D(vulkan_memory *memory_handle, uint32_t width, uint32_t height, uint32_t depth, VkFormat texture_format, uint32_t mipmaps, const char *texture_name) {
    return _VulkanMemoryAddTextureGeneric(memory_handle, width, height, depth, texture_format, mipmaps, texture_name, VULKAN_REGION_TEXTURE_3D);
}

test_status VulkanMemoryAllocateBacking(vulkan_memory *memory_handle) {
    TRACE_MEMORY("Allocating backing of memory pool 0x%p\n", memory_handle);
    if (memory_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    /* Translate flags into actual Vulkan things */
    VkMemoryPropertyFlags property_flags = 0;
    VkMemoryAllocateFlags allocate_flags = 0;

    if ((memory_handle->memory_type_flags & VULKAN_MEMORY_HOST_LOCAL) != 0) {
        property_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    if ((memory_handle->memory_type_flags & VULKAN_MEMORY_VISIBLE) != 0) {
        property_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    if ((memory_handle->memory_type_flags & VULKAN_MEMORY_HOST_CACHED) != 0) {
        property_flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    if ((memory_handle->memory_type_flags & VULKAN_MEMORY_HOST_COHERENT) != 0) {
        property_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    if ((memory_handle->memory_type_flags & VULKAN_MEMORY_LARGE_BUFFERS) != 0) {
        allocate_flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }
    VkPhysicalDeviceMemoryProperties *physical_memory_properties = &(memory_handle->device->physical_device->physical_memory_properties.memoryProperties);
    bool allocated = false;

    /* Initialize buffers and calculate the total pool size */
    size_t total_size = 0;
    for (uint32_t i = 0; i <= VULKAN_REGION_TYPE_MAX; i++) {
        size_t buffer_count = HelperArrayListSize(&(memory_handle->buffer_list[i]));
        for (uint32_t j = 0; j < (uint32_t)buffer_count; j++) {
            vulkan_buffer *buffer = HelperArrayListGet(&(memory_handle->buffer_list[i]), j);
            if (i != VULKAN_REGION_TEXTURE_1D && i != VULKAN_REGION_TEXTURE_2D && i != VULKAN_REGION_TEXTURE_3D) {
                size_t region_count = HelperArrayListSize(&(buffer->regions));

                /* Calculate size of buffer */
                for (uint32_t k = 0; k < (uint32_t)region_count; k++) {
                    vulkan_region *region = HelperArrayListGet(&(buffer->regions), k);

                    size_t remainder = buffer->buffer_size % buffer->required_region_alignment;
                    if (remainder != 0) {
                        buffer->buffer_size += buffer->required_region_alignment - remainder;
                    }
                    region->offset = buffer->buffer_size;
                    buffer->buffer_size += region->size;
                    TRACE_MEMORY("Finalizing region \"%s\" of memory pool 0x%p (size: %lu, offset: %lu)\n", region->name, buffer->memory_pool, region->size, region->offset);
                }
            }
            /* Create buffer */
            /* Translate flags into actual Vulkan things */
            VkSharingMode sharing_mode = (buffer->buffer_flags & VULKAN_REGION_SHARED) != 0 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
            VkBufferUsageFlags buffer_usage = 0;
            switch (i) {
            case VULKAN_REGION_STORAGE:
                buffer_usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                break;
            case VULKAN_REGION_UNIFORM:
                buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                break;
            case VULKAN_REGION_INDEX:
                buffer_usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
                break;
            case VULKAN_REGION_VERTEX:
                buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
                break;
            case VULKAN_REGION_TEXTURE_1D:
            case VULKAN_REGION_TEXTURE_2D:
            case VULKAN_REGION_TEXTURE_3D:
                buffer_usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                break;
            default:
                return TEST_VK_UNKNOWN_BUFFER_TYPE;
            }
            if ((buffer->buffer_flags & VULKAN_REGION_LARGE_BUFFER) != 0) {
                if (i != VULKAN_REGION_TEXTURE_1D && i != VULKAN_REGION_TEXTURE_2D && i != VULKAN_REGION_TEXTURE_3D) {
                    buffer_usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
                }
            }
            if ((buffer->buffer_flags & VULKAN_REGION_TRANSFER_SOURCE) != 0) {
                if (i == VULKAN_REGION_TEXTURE_1D || i == VULKAN_REGION_TEXTURE_2D || i == VULKAN_REGION_TEXTURE_3D) {
                    buffer_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
                } else {
                    buffer_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                }
            }
            if ((buffer->buffer_flags & VULKAN_REGION_TRANSFER_DESTINATION) != 0) {
                if (i == VULKAN_REGION_TEXTURE_1D || i == VULKAN_REGION_TEXTURE_2D || i == VULKAN_REGION_TEXTURE_3D) {
                    buffer_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                } else {
                    buffer_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                }
            }
            /* Allocate buffer/image and bind it to this memory pool */
            if (i == VULKAN_REGION_TEXTURE_1D || i == VULKAN_REGION_TEXTURE_2D || i == VULKAN_REGION_TEXTURE_3D) {
                VkImageCreateInfo image_create_info = {0};
                image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                image_create_info.pNext = NULL;
                image_create_info.usage = buffer_usage;
                image_create_info.sharingMode = sharing_mode;
                image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
                switch (i) {
                case VULKAN_REGION_TEXTURE_1D:
                    image_create_info.imageType = VK_IMAGE_TYPE_1D;
                    break;
                case VULKAN_REGION_TEXTURE_3D:
                    image_create_info.imageType = VK_IMAGE_TYPE_3D;
                    break;
                default:
                    image_create_info.imageType = VK_IMAGE_TYPE_2D;
                    break;
                }
                image_create_info.extent.width = buffer->texture->image_width;
                image_create_info.extent.height = buffer->texture->image_height;
                image_create_info.extent.depth = buffer->texture->image_depth;
                image_create_info.mipLevels = buffer->texture->image_mips;
                image_create_info.format = buffer->texture->image_format;
                image_create_info.arrayLayers = 1;
                image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
                image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                image_create_info.pQueueFamilyIndices = memory_handle->queue_family_indices;
                image_create_info.queueFamilyIndexCount = memory_handle->queue_family_count;
                TRACE_MEMORY("Creating texture object for memory pool 0x%p (type: %lu, index: %lu, size: %llux%llux%llu)\n", buffer->memory_pool, i, j, buffer->texture->image_width, buffer->texture->image_height, buffer->texture->image_depth);
                VkResult res = vkCreateImage(memory_handle->device->device, &image_create_info, NULL, &(buffer->image));
                VULKAN_RETFAIL(res, TEST_VK_IMAGE_CREATION_ERROR);

                VkImageMemoryRequirementsInfo2 image_memory_requirements_info_2 = {0};
                image_memory_requirements_info_2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
                image_memory_requirements_info_2.pNext = NULL;
                image_memory_requirements_info_2.image = buffer->image;
                VkMemoryRequirements2 memory_requirements_2 = {0};
                memory_requirements_2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
                memory_requirements_2.pNext = NULL;
                vkGetImageMemoryRequirements2(memory_handle->device->device, &image_memory_requirements_info_2, &memory_requirements_2);
                buffer->required_size = memory_requirements_2.memoryRequirements.size;
                buffer->required_alignment = memory_requirements_2.memoryRequirements.alignment;

                memory_handle->supported_memory_types_mask &= memory_requirements_2.memoryRequirements.memoryTypeBits;
            } else {
                VkBufferCreateInfo buffer_create_info = {0};
                buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                buffer_create_info.pNext = NULL;
                buffer_create_info.usage = buffer_usage;
                buffer_create_info.sharingMode = sharing_mode;
                buffer_create_info.size = buffer->buffer_size;
                buffer_create_info.pQueueFamilyIndices = memory_handle->queue_family_indices;
                buffer_create_info.queueFamilyIndexCount = memory_handle->queue_family_count;
                TRACE_MEMORY("Creating buffer object for memory pool 0x%p (type: %lu, index: %lu, size: %llu)\n", buffer->memory_pool, i, j, buffer->buffer_size);
                VkResult res = vkCreateBuffer(memory_handle->device->device, &buffer_create_info, NULL, &(buffer->buffer));
                VULKAN_RETFAIL(res, TEST_VK_BUFFER_CREATION_ERROR);

                VkBufferMemoryRequirementsInfo2 buffer_memory_requirements_info_2 = {0};
                buffer_memory_requirements_info_2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
                buffer_memory_requirements_info_2.pNext = NULL;
                buffer_memory_requirements_info_2.buffer = buffer->buffer;
                VkMemoryRequirements2 memory_requirements_2 = {0};
                memory_requirements_2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
                memory_requirements_2.pNext = NULL;
                vkGetBufferMemoryRequirements2(memory_handle->device->device, &buffer_memory_requirements_info_2, &memory_requirements_2);
                buffer->required_size = memory_requirements_2.memoryRequirements.size;
                buffer->required_alignment = memory_requirements_2.memoryRequirements.alignment;

                memory_handle->supported_memory_types_mask &= memory_requirements_2.memoryRequirements.memoryTypeBits;
            }

            /* Update total memory pool size */
            size_t remainder = total_size % buffer->required_alignment;
            if (remainder != 0) {
                total_size += buffer->required_alignment - remainder;
            }
            buffer->global_offset = total_size;
            total_size += buffer->required_size;
        }
    }
    memory_handle->total_size = total_size;
    if (total_size == 0) {
        return TEST_VK_BACKING_SIZE_IS_ZERO;
    }
    VkDeviceSize non_coherent_size = memory_handle->device->physical_device->physical_properties.properties.limits.nonCoherentAtomSize;
    total_size = (memory_handle->total_size / non_coherent_size) * non_coherent_size;
    if (total_size != memory_handle->total_size) {
        total_size += non_coherent_size;
    }
    TRACE_MEMORY("Non-coherent atom size is %llu, adjusting memory size from %llu to %llu\n", non_coherent_size, memory_handle->total_size, total_size);
    memory_handle->total_size = total_size;
    TRACE_MEMORY("Final size of memory pool 0x%p: %llu\n", memory_handle, memory_handle->total_size);

    /* Allocate memory pool */
    for (uint32_t i = 0; i < physical_memory_properties->memoryTypeCount; i++) {
        VkMemoryType memory_type = physical_memory_properties->memoryTypes[i];
        if (((memory_handle->supported_memory_types_mask & (1 << i)) != 0) && ((memory_type.propertyFlags & property_flags) ^ property_flags) == 0) {
            VkMemoryHeap memory_heap = physical_memory_properties->memoryHeaps[memory_type.heapIndex];
            if (memory_heap.size >= memory_handle->total_size) {
                VkMemoryAllocateFlagsInfo memory_allocate_flags_info = {0};
                memory_allocate_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
                memory_allocate_flags_info.pNext = NULL;
                memory_allocate_flags_info.flags = allocate_flags;
                memory_allocate_flags_info.deviceMask = 0;
                VkMemoryAllocateInfo memory_allocate_info = {0};
                memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                memory_allocate_info.pNext = &memory_allocate_flags_info;
                memory_allocate_info.allocationSize = total_size;
                memory_allocate_info.memoryTypeIndex = i;

                VkResult res = vkAllocateMemory(memory_handle->device->device, &memory_allocate_info, NULL, &(memory_handle->memory));
                VULKAN_RETFAIL(res, TEST_VK_MEMORY_ALLOCATION_ERROR);
                memory_handle->memory_type_index = i;
                allocated = true;
                break;
            }
        }
    }
    if (!allocated) {
        return TEST_VK_SUITABLE_ALLOCATION_NOT_FOUND;
    }
    /* Allocate buffers */
    size_t current_offset = 0;
    for (uint32_t i = 0; i <= VULKAN_REGION_TYPE_MAX; i++) {
        size_t buffer_count = HelperArrayListSize(&(memory_handle->buffer_list[i]));
        for (uint32_t j = 0; j < (uint32_t)buffer_count; j++) {
            vulkan_buffer *buffer = HelperArrayListGet(&(memory_handle->buffer_list[i]), j);

            size_t remainder = current_offset % buffer->required_alignment;
            if (remainder != 0) {
                current_offset += buffer->required_alignment - remainder;
            }
            if (i == VULKAN_REGION_TEXTURE_1D || i == VULKAN_REGION_TEXTURE_2D || i == VULKAN_REGION_TEXTURE_3D) {
                VkResult res = vkBindImageMemory(memory_handle->device->device, buffer->image, memory_handle->memory, current_offset);
                current_offset += buffer->required_size;
                VULKAN_RETFAIL(res, TEST_VK_IMAGE_BIND_MEMORY_ERROR);

                buffer->texture->backing_image = buffer->image;

                VkImageViewCreateInfo image_view_create_info = {0};
                image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                image_view_create_info.pNext = NULL;
                image_view_create_info.image = buffer->image;
                switch (i) {
                case VULKAN_REGION_TEXTURE_1D:
                    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_1D;
                    break;
                case VULKAN_REGION_TEXTURE_3D:
                    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
                    break;
                default:
                    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    break;
                }
                image_view_create_info.format = buffer->texture->image_format;
                image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_view_create_info.subresourceRange.baseMipLevel = 0;
                image_view_create_info.subresourceRange.levelCount = buffer->texture->image_mips;
                image_view_create_info.subresourceRange.baseArrayLayer = 0;
                image_view_create_info.subresourceRange.layerCount = 1;
                image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

                res = vkCreateImageView(memory_handle->device->device, &image_view_create_info, NULL, &(buffer->texture->image_view));
                VULKAN_RETFAIL(res, TEST_VK_IMAGE_VIEW_CREATION_ERROR);

                /* Create a sampler tied to the texture (for now?) */
                VkSamplerCreateInfo sampler_create_info = {0};
                sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sampler_create_info.pNext = NULL;
                sampler_create_info.magFilter = VK_FILTER_NEAREST;
                sampler_create_info.minFilter = VK_FILTER_NEAREST;
                sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                sampler_create_info.anisotropyEnable = VK_FALSE;
                sampler_create_info.maxAnisotropy = 0;
                sampler_create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                sampler_create_info.unnormalizedCoordinates = VK_FALSE;
                sampler_create_info.compareEnable = VK_FALSE;
                sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
                sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                sampler_create_info.mipLodBias = 0.0;
                sampler_create_info.minLod = 0.0;
                sampler_create_info.maxLod = 0.0;
                
                res = vkCreateSampler(memory_handle->device->device, &sampler_create_info, NULL, &(buffer->texture->image_sampler));
                VULKAN_RETFAIL(res, TEST_VK_SAMPLER_CREATION_ERROR);
            } else {
                VkResult res = vkBindBufferMemory(memory_handle->device->device, buffer->buffer, memory_handle->memory, current_offset);
                current_offset += buffer->required_size;
                VULKAN_RETFAIL(res, TEST_VK_BUFFER_BIND_MEMORY_ERROR);

                size_t region_count = HelperArrayListSize(&(buffer->regions));
                for (uint32_t k = 0; k < (uint32_t)region_count; k++) {
                    vulkan_region *region = HelperArrayListGet(&(buffer->regions), k);

                    region->backing_buffer = buffer->buffer;
                    region->global_offset = region->offset + buffer->global_offset;
                }
            }
        }
    }
    return TEST_OK;
}

test_status VulkanMemoryFreeBuffersAndBacking(vulkan_memory *memory_handle) {
    TRACE_MEMORY("Freeing backing of memory pool 0x%p\n", memory_handle);
    if (memory_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (memory_handle->memory == VK_NULL_HANDLE) {
        return TEST_VK_BACKING_NOT_ALLOCATED;
    }
    if (memory_handle->mapped_pool_base != NULL) {
        memory_handle->mapped_pool_base = NULL;
        vkUnmapMemory(memory_handle->device->device, memory_handle->memory);
    }
    _VulkanMemoryDestroyBuffers(memory_handle);
    vkFreeMemory(memory_handle->device->device, memory_handle->memory, NULL);
    memory_handle->memory = VK_NULL_HANDLE;
    return TEST_OK;
}

vulkan_region *VulkanMemoryGetRegion(vulkan_memory *memory_handle, const char *region_name) {
    if (memory_handle == NULL || region_name == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i <= VULKAN_REGION_TYPE_MAX; i++) {
        if (i != VULKAN_REGION_TEXTURE_1D && i != VULKAN_REGION_TEXTURE_2D && i != VULKAN_REGION_TEXTURE_3D) {
            size_t buffer_count = HelperArrayListSize(&(memory_handle->buffer_list[i]));

            for (uint32_t j = 0; j < (uint32_t)buffer_count; j++) {
                vulkan_buffer *buffer = HelperArrayListGet(&(memory_handle->buffer_list[i]), j);
                size_t region_count = HelperArrayListSize(&(buffer->regions));

                for (uint32_t k = 0; k < (uint32_t)region_count; k++) {
                    vulkan_region *region = HelperArrayListGet(&(buffer->regions), k);
                    if (strcmp(region_name, region->name) == 0) {
                        return region;
                    }
                }
            }
        }
    }
    return NULL;
}

vulkan_texture *VulkanMemoryGetTexture(vulkan_memory *memory_handle, const char *texture_name) {
    if (memory_handle == NULL || texture_name == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i <= VULKAN_REGION_TYPE_MAX; i++) {
        if (i == VULKAN_REGION_TEXTURE_1D || i == VULKAN_REGION_TEXTURE_2D || i == VULKAN_REGION_TEXTURE_3D) {
            size_t buffer_count = HelperArrayListSize(&(memory_handle->buffer_list[i]));

            for (uint32_t j = 0; j < (uint32_t)buffer_count; j++) {
                vulkan_buffer *buffer = HelperArrayListGet(&(memory_handle->buffer_list[i]), j);

                if (buffer->texture != NULL && strcmp(texture_name, buffer->texture->name) == 0) {
                    return buffer->texture;
                }
            }
        }
    }
    return NULL;
}

void *VulkanMemoryMap(vulkan_region *region_handle) {
    if (region_handle == NULL || region_handle->is_mapped) {
        return NULL;
    }
    vulkan_memory *pool = region_handle->memory_pool;
    
    if (pool->mapped_pool_base == NULL) {
        void *data;
        VkResult res = vkMapMemory(pool->device->device, pool->memory, 0, VK_WHOLE_SIZE, 0, &data);
        if (!VULKAN_SUCCESS(res)) {
            return NULL;
        }
        pool->mapped_pool_base = data;
    }
    pool->mapped_region_count++;
    region_handle->is_mapped = true;
    return (void *)((uint64_t)pool->mapped_pool_base + region_handle->global_offset);
}

void VulkanMemoryUnmap(vulkan_region *region_handle) {
    if (region_handle == NULL || !region_handle->is_mapped) {
        return;
    }
    vulkan_memory *pool = region_handle->memory_pool;

    VulkanMemoryFlush(region_handle);

    region_handle->is_mapped = false;
    pool->mapped_region_count--;

    if (pool->mapped_region_count == 0) {
        vkUnmapMemory(region_handle->memory_pool->device->device, region_handle->memory_pool->memory);
        pool->mapped_pool_base = NULL;
    }
}

void VulkanMemoryFlush(vulkan_region *region_handle) {
    if (region_handle == NULL || !region_handle->is_mapped) {
        return;
    }
    vulkan_memory *pool = region_handle->memory_pool;
    VkDeviceSize non_coherent_size = region_handle->memory_pool->device->physical_device->physical_properties.properties.limits.nonCoherentAtomSize;
    VkDeviceSize flush_size = region_handle->size / non_coherent_size;
    if (flush_size * non_coherent_size != region_handle->size) {
        flush_size++;
    }
    flush_size *= non_coherent_size;
    VkDeviceSize flush_offset = region_handle->global_offset;
    if (flush_offset + flush_size > pool->total_size) {
        if (flush_size > pool->total_size) {
            flush_offset = 0;
            flush_size = VK_WHOLE_SIZE;
        } else {
            flush_offset = pool->total_size - flush_size;
        }
    }

    VkMappedMemoryRange mapped_memory_range = {0};
    mapped_memory_range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mapped_memory_range.pNext = NULL;
    mapped_memory_range.memory = pool->memory;
    mapped_memory_range.offset = flush_offset;
    mapped_memory_range.size = flush_size;

    vkFlushMappedMemoryRanges(region_handle->memory_pool->device->device, 1, &mapped_memory_range);
}

uint64_t VulkanMemoryGetPhysicalPoolSize(vulkan_memory *memory_handle) {
    if (memory_handle == NULL) {
        return 0;
    }
    VkPhysicalDeviceMemoryProperties *physical_memory_properties = &(memory_handle->device->physical_device->physical_memory_properties.memoryProperties);
    VkMemoryPropertyFlags property_flags = 0;

    if ((memory_handle->memory_type_flags & VULKAN_MEMORY_HOST_LOCAL) != 0) {
        property_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    if ((memory_handle->memory_type_flags & VULKAN_MEMORY_VISIBLE) != 0) {
        property_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    for (uint32_t i = 0; i < physical_memory_properties->memoryTypeCount; i++) {
        VkMemoryType memory_type = physical_memory_properties->memoryTypes[i];
        if (((memory_handle->supported_memory_types_mask & (1 << i)) != 0) && ((memory_type.propertyFlags & property_flags) ^ property_flags) == 0) {
            VkMemoryHeap memory_heap = physical_memory_properties->memoryHeaps[memory_type.heapIndex];
            return memory_heap.size;
        }
    }
    return 0;
}

static void _VulkanMemoryDestroyBuffers(vulkan_memory *memory_handle) {
    for (uint32_t i = 0; i <= VULKAN_REGION_TYPE_MAX; i++) {
        size_t buffer_count = HelperArrayListSize(&(memory_handle->buffer_list[i]));
        for (uint32_t j = 0; j < (uint32_t)buffer_count; j++) {
            vulkan_buffer *buffer = HelperArrayListGet(&(memory_handle->buffer_list[i]), j);

            if (i == VULKAN_REGION_TEXTURE_1D || i == VULKAN_REGION_TEXTURE_2D || i == VULKAN_REGION_TEXTURE_3D) {
                if (buffer->texture != NULL) {
                    if (buffer->texture->image_view != VK_NULL_HANDLE) {
                        vkDestroyImageView(memory_handle->device->device, buffer->texture->image_view, NULL);
                        buffer->texture->image_view = VK_NULL_HANDLE;
                    }
                    if (buffer->texture->image_sampler != VK_NULL_HANDLE) {
                        vkDestroySampler(memory_handle->device->device, buffer->texture->image_sampler, NULL);
                        buffer->texture->image_sampler = VK_NULL_HANDLE;
                    }
                    free(buffer->texture);
                    buffer->texture = NULL;
                }
                if (buffer->image != VK_NULL_HANDLE) {
                    vkDestroyImage(memory_handle->device->device, buffer->image, NULL);
                    buffer->image = VK_NULL_HANDLE;
                }
            } else {
                if (buffer->buffer != VK_NULL_HANDLE) {
                    vkDestroyBuffer(memory_handle->device->device, buffer->buffer, NULL);
                    buffer->buffer = VK_NULL_HANDLE;
                }
                HelperArrayListClean(&(buffer->regions));
            }
        }
    }
}

static test_status _VulkanMemoryAddTextureGeneric(vulkan_memory *memory_handle, uint32_t width, uint32_t height, uint32_t depth, VkFormat texture_format, uint32_t mipmaps, const char *texture_name, uint32_t texture_type) {
    TRACE_MEMORY("Adding texture to memory pool 0x%p (size: %llux%llux%llu, format: %llu, mips: %llu, name: \"%s\", type: %lu)\n", memory_handle, width, height, depth, texture_format, mipmaps, texture_name, texture_type);
    if (memory_handle == NULL || texture_name == NULL || ((size_t)width * (size_t)height * (size_t)depth) == 0) {
        return TEST_INVALID_PARAMETER;
    }
    if (memory_handle->memory != VK_NULL_HANDLE) {
        return TEST_VK_BACKING_ALREADY_ALLOCATED;
    }
    size_t name_length = strlen(texture_name);
    if (name_length >= VULKAN_REGION_MAX_NAME_LENGTH) {
        return TEST_VK_BUFFER_NAME_TOO_LONG;
    }
    vulkan_texture *texture = malloc(sizeof(vulkan_texture));
    if (texture == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    texture->backing_image = VK_NULL_HANDLE;
    texture->image_view = VK_NULL_HANDLE;
    texture->image_sampler = VK_NULL_HANDLE;
    texture->current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    texture->global_offset = 0;
    texture->image_mips = mipmaps;
    texture->image_width = width;
    texture->image_height = height;
    texture->image_depth = depth;
    texture->image_format = texture_format;
    texture->is_mapped = false;
    texture->memory_pool = memory_handle;
    strcpy((char *)texture->name, texture_name);

    vulkan_buffer new_buffer = {0};
    new_buffer.memory_pool = memory_handle;
    new_buffer.image = VK_NULL_HANDLE;
    new_buffer.texture = texture;
    new_buffer.required_region_alignment = memory_handle->device->physical_device->physical_properties.properties.limits.minTexelBufferOffsetAlignment;
    TRACE_MEMORY("Creating backing buffer for texture \"%s\" (pool: 0x%p, format: %llu, type: %lu, alignment: %lu)\n", texture_name, new_buffer.memory_pool, new_buffer.texture->image_format, texture_type, new_buffer.required_region_alignment);

    size_t buffer_index = 0;
    test_status status = HelperArrayListAdd(&(memory_handle->buffer_list[texture_type]), &new_buffer, sizeof(new_buffer), &buffer_index);
    if (!TEST_SUCCESS(status)) {
        free(texture);
    }
    return status;
}

bool VulkanMemoryIsMemoryTypePresent(vulkan_physical_device* physical_device, uint32_t required_properties) {
    VkPhysicalDeviceMemoryProperties* physical_memory_properties = &(physical_device->physical_memory_properties.memoryProperties);
    VkMemoryPropertyFlags required_flags = 0;
    if ((required_properties & VULKAN_MEMORY_HOST_LOCAL) != 0) {
        required_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    } else {
        required_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
    if ((required_properties & VULKAN_MEMORY_VISIBLE) != 0) {
        required_flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    if ((required_properties & VULKAN_MEMORY_HOST_CACHED) != 0) {
        required_flags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    if ((required_properties & VULKAN_MEMORY_HOST_COHERENT) != 0) {
        required_flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    for (uint32_t i = 0; i < physical_memory_properties->memoryTypeCount; i++) {
        VkMemoryPropertyFlags current_flags = physical_memory_properties->memoryTypes[i].propertyFlags;
        if ((current_flags & required_flags) == required_flags)
            return true;
    }
    return false;
}