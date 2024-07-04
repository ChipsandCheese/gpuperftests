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

#ifndef VULKAN_MEMORY_H
#define VULKAN_MEMORY_H

#ifdef __cplusplus
extern "C" {
#endif

#define VULKAN_MEMORY_NORMAL                (0)
#define VULKAN_MEMORY_VISIBLE               (1 << 0)
#define VULKAN_MEMORY_LARGE_BUFFERS         (1 << 1)
#define VULKAN_MEMORY_HOST_LOCAL            (1 << 2)
#define VULKAN_MEMORY_HOST_CACHED           (1 << 3)
#define VULKAN_MEMORY_HOST_COHERENT         (1 << 4)

#define VULKAN_REGION_NORMAL                (0)
#define VULKAN_REGION_SHARED                (1 << 0)
#define VULKAN_REGION_LARGE_BUFFER          (1 << 1)
#define VULKAN_REGION_TRANSFER_SOURCE       (1 << 2)
#define VULKAN_REGION_TRANSFER_DESTINATION  (1 << 3)
#define VULKAN_REGION_STORAGE               (0)
#define VULKAN_REGION_UNIFORM               (1)
#define VULKAN_REGION_INDEX                 (2)
#define VULKAN_REGION_VERTEX                (3)
#define VULKAN_REGION_TEXTURE_1D            (4)
#define VULKAN_REGION_TEXTURE_2D            (5)
#define VULKAN_REGION_TEXTURE_3D            (6)
#define VULKAN_REGION_TYPE_MAX              (VULKAN_REGION_TEXTURE_3D)

#define VULKAN_REGION_MAX_NAME_LENGTH       (32)

typedef struct vulkan_memory_t {
    vulkan_device *device;
    VkDeviceMemory memory;
    uint32_t memory_type_index;
    uint32_t memory_type_flags;
    size_t total_size;
    uint32_t supported_memory_types_mask;
    uint32_t mapped_region_count;
    void *mapped_pool_base;
    uint32_t *queue_family_indices;
    uint32_t queue_family_count;
    helper_arraylist buffer_list[VULKAN_REGION_TYPE_MAX + 1];
} vulkan_memory;

typedef struct vulkan_region_t {
    VkBuffer backing_buffer;
    vulkan_memory *memory_pool;
    size_t size;
    size_t offset;
    size_t global_offset;
    bool is_mapped;
    const char name[VULKAN_REGION_MAX_NAME_LENGTH];
} vulkan_region;

typedef struct vulkan_texture_t {
    VkImage backing_image;
    VkImageView image_view;
    VkSampler image_sampler;
    vulkan_memory *memory_pool;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_depth;
    uint32_t image_mips;
    VkFormat image_format;
    VkImageLayout current_layout;
    size_t global_offset;
    bool is_mapped;
    const char name[VULKAN_REGION_MAX_NAME_LENGTH];
} vulkan_texture;

typedef struct vulkan_buffer_t {
    vulkan_memory *memory_pool;
    union {
        struct {
            VkBuffer buffer;
            size_t buffer_size;
            helper_arraylist regions;
        };
        struct {
            VkImage image;
            vulkan_texture *texture;
        };
    };
    uint32_t buffer_flags;
    size_t required_size;
    size_t required_alignment;
    size_t required_region_alignment;
    size_t global_offset;
} vulkan_buffer;

test_status VulkanMemoryInitializeForQueues(vulkan_device *device, uint32_t *queue_family_indices, uint32_t queue_family_count, uint32_t memory_type, vulkan_memory *memory_handle);
test_status VulkanMemoryInitialize(vulkan_device *device, uint32_t memory_type, vulkan_memory *memory_handle);
test_status VulkanMemoryCleanUp(vulkan_memory *memory_handle);
bool VulkanMemoryIsAllocated(vulkan_memory *memory_handle);
test_status VulkanMemoryAddRegion(vulkan_memory *memory_handle, size_t region_size, const char *region_name, uint32_t region_type, uint32_t region_flags);
test_status VulkanMemoryAddTexture1D(vulkan_memory *memory_handle, uint32_t width, VkFormat texture_format, uint32_t mipmaps, const char *texture_name);
test_status VulkanMemoryAddTexture2D(vulkan_memory *memory_handle, uint32_t width, uint32_t height, VkFormat texture_format, uint32_t mipmaps, const char *texture_name);
test_status VulkanMemoryAddTexture3D(vulkan_memory *memory_handle, uint32_t width, uint32_t height, uint32_t depth, VkFormat texture_format, uint32_t mipmaps, const char *texture_name);
test_status VulkanMemoryAllocateBacking(vulkan_memory *memory_handle);
test_status VulkanMemoryFreeBuffersAndBacking(vulkan_memory *memory_handle);
vulkan_region *VulkanMemoryGetRegion(vulkan_memory *memory_handle, const char *region_name);
vulkan_texture *VulkanMemoryGetTexture(vulkan_memory *memory_handle, const char *texture_name);
void *VulkanMemoryMap(vulkan_region *region_handle);
void VulkanMemoryUnmap(vulkan_region *region_handle);
void VulkanMemoryFlush(vulkan_region *region_handle);
uint64_t VulkanMemoryGetPhysicalPoolSize(vulkan_memory *memory_handle);
bool VulkanMemoryIsMemoryTypePresent(vulkan_physical_device* physical_device, uint32_t required_properties);

#ifdef __cplusplus
}
#endif
#endif