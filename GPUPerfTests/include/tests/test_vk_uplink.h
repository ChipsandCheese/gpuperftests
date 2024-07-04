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

#ifndef TEST_VK_UPLINK_H
#define TEST_VK_UPLINK_H

#ifdef __cplusplus
extern "C" {
#endif

#define TESTS_VULKAN_UPLINK_VERSION                 TEST_MKVERSION(1, 1, 2)
#define TESTS_VULKAN_UPLINK_CPU_READ_NAME           "vk_uplink_copy_read"
#define TESTS_VULKAN_UPLINK_CPU_WRITE_NAME          "vk_uplink_copy_write"
#define TESTS_VULKAN_UPLINK_GPU_READ_NAME           "vk_uplink_compute_read"
#define TESTS_VULKAN_UPLINK_GPU_WRITE_NAME          "vk_uplink_compute_write"
#define TESTS_VULKAN_UPLINK_MAP_READ_NAME           "vk_uplink_mapped_read"
#define TESTS_VULKAN_UPLINK_MAP_WRITE_NAME          "vk_uplink_mapped_write"
#define TESTS_VULKAN_UPLINK_CPU_LATENCY_NAME        "vk_uplink_latency"
#define TESTS_VULKAN_UPLINK_CPU_LATENCY_LONG_NAME   "vk_uplink_latency_long"

test_status TestsVulkanUplinkRegister();

#ifdef __cplusplus
}
#endif
#endif
