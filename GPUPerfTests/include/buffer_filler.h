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

#ifndef BUFFER_FILLER_H
#define BUFFER_FILLER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef test_status(buffer_filler_prep_func)(uint32_t block_count, void *custom_data);
typedef test_status(buffer_filler_block_func)(void *block_data, size_t block_offset, size_t block_size, uint32_t block_index, void *custom_data);

test_status BufferFillerGeneric(vulkan_region *region, buffer_filler_block_func *block_function, size_t data_unit_size, void *custom_data, buffer_filler_prep_func *per_block_initialize, buffer_filler_prep_func *per_block_cleanup);
test_status BufferFillerGenericOffset(vulkan_region *region, size_t region_size_override, size_t offset_override, buffer_filler_block_func *block_function, size_t data_unit_size, void *custom_data, buffer_filler_prep_func *per_block_initialize, buffer_filler_prep_func *per_block_cleanup);

test_status BufferFillerZero(vulkan_region *region);
test_status BufferFillerValueByte(vulkan_region *region, int8_t value);
test_status BufferFillerValueShort(vulkan_region *region, int16_t value);
test_status BufferFillerValueInt(vulkan_region *region, int32_t value);
test_status BufferFillerValueLong(vulkan_region *region, int64_t value);
test_status BufferFillerValueUByte(vulkan_region *region, uint8_t value);
test_status BufferFillerValueUShort(vulkan_region *region, uint16_t value);
test_status BufferFillerValueUInt(vulkan_region *region, uint32_t value);
test_status BufferFillerValueULong(vulkan_region *region, uint64_t value);
test_status BufferFillerValueFloat(vulkan_region *region, float value);
test_status BufferFillerValueDouble(vulkan_region *region, double value);
test_status BufferFillerRandomIntegers(vulkan_region *region, uint64_t seed);
test_status BufferFillerRandomFloats(vulkan_region *region, uint64_t seed);

#ifdef __cplusplus
}
#endif
#endif