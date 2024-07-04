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

#ifndef TEST_VK_RATE_H
#define TEST_VK_RATE_H

#ifdef __cplusplus
extern "C" {
#endif

#define TESTS_VULKAN_RATE_VERSION           TEST_MKVERSION(1, 5, 0)

#define TESTS_VULKAN_RATE_TYPE_FP16         "fp16"
#define TESTS_VULKAN_RATE_TYPE_FP32         "fp32"
#define TESTS_VULKAN_RATE_TYPE_FP64         "fp64"
#define TESTS_VULKAN_RATE_TYPE_INT8         "int8"
#define TESTS_VULKAN_RATE_TYPE_INT16        "int16"
#define TESTS_VULKAN_RATE_TYPE_INT32        "int32"
#define TESTS_VULKAN_RATE_TYPE_INT64        "int64"

#define TESTS_VULKAN_RATE_OP_MAC            "mac"
#define TESTS_VULKAN_RATE_OP_ADD            "add"
#define TESTS_VULKAN_RATE_OP_SUB            "sub"
#define TESTS_VULKAN_RATE_OP_MUL            "mul"
#define TESTS_VULKAN_RATE_OP_DIV            "div"
#define TESTS_VULKAN_RATE_OP_REM            "rem"
#define TESTS_VULKAN_RATE_OP_RCP            "rcp"
#define TESTS_VULKAN_RATE_OP_ISQRT          "isqrt"

#define TESTS_VULKAN_RATE_NAME_PREFIX       "vk_rate_"
#define TESTS_VULKAN_RATE_NAME_SEPARATOR    "_"

test_status TestsVulkanRateRegister();

#ifdef __cplusplus
}
#endif
#endif