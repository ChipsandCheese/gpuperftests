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

#ifndef VULKAN_STAGING_H
#define VULKAN_STAGING_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vulkan_staging_t {
    vulkan_region *destination_region;
    vulkan_texture *destination_texture;
    vulkan_memory staging_memory;
    vulkan_command_buffer command_buffer;
    void *staging_buffer_base;
    uint32_t subregion_width;
    uint32_t subregion_height;
    uint32_t subregion_depth;
} vulkan_staging;

test_status VulkanStagingInitializeRange(vulkan_region *destination_region, vulkan_staging *staging_handle);
test_status VulkanStagingInitializeSubrange(vulkan_region *destination_region, size_t subregion_size, vulkan_staging *staging_handle);
test_status VulkanStagingInitializeImage(vulkan_texture *destination_texture, size_t source_data_size, vulkan_staging *staging_handle);
test_status VulkanStagingInitializeSubimage(vulkan_texture *destination_texture, size_t source_data_size, uint32_t width, uint32_t height, uint32_t depth, vulkan_staging *staging_handle);
test_status VulkanStagingCleanUp(vulkan_staging *staging_handle);
test_status VulkanStagingTransferSubrange(vulkan_staging *staging_handle, size_t buffer_offset);
test_status VulkanStagingTransferRange(vulkan_staging *staging_handle);
test_status VulkanStagingTransferSubimage(vulkan_staging *staging_handle, int32_t texture_offset_x, int32_t texture_offset_y, int32_t texture_offset_z, uint32_t mip_level);
test_status VulkanStagingTransferImage(vulkan_staging *staging_handle, uint32_t mip_level);
void *VulkanStagingGetBuffer(vulkan_staging *staging_handle);

#ifdef __cplusplus
}
#endif
#endif