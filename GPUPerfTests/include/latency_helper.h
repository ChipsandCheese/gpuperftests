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

#ifndef LATENCY_HELPER_H
#define LATENCY_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct latency_helper_lru_t {
    uint32_t stride;
    uint32_t *lru_table;
} latency_helper_lru;

test_status LatencyHelperLRUInitialize(latency_helper_lru *lru, uint32_t stride_bytes);
test_status LatencyHelperLRUCleanUp(latency_helper_lru *lru);
test_status LatencyHelperLRUFillSubregion(latency_helper_lru *lru, vulkan_region *region, size_t size);
test_status LatencyHelperLRUFillRegion(latency_helper_lru *lru, vulkan_region *region);
uint64_t LatencyHelperLRUGetHopCount(latency_helper_lru *lru, vulkan_region *region);
uint32_t LatencyHelperLRUGetStartingOffset(latency_helper_lru *lru, vulkan_region *region, uint32_t desired_index);

#ifdef __cplusplus
}
#endif
#endif