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

#ifndef TEST_VK_LATENCY_H
#define TEST_VK_LATENCY_H

#ifdef __cplusplus
extern "C" {
#endif

#define TESTS_VULKAN_LATENCY_VERSION    TEST_MKVERSION(1, 2, 1)
#define TESTS_VULKAN_LATENCY_VEC_NAME   "vk_latency_vector"
#define TESTS_VULKAN_LATENCY_SCLR_NAME  "vk_latency_scalar"

test_status TestsVulkanLatencyRegister();
const uint64_t *VulkanLatencyGetRegionSizes();
size_t VulkanLatencyGetRegionCount();

#ifdef __cplusplus
}
#endif
#endif