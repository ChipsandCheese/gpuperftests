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

#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DEBUG
#define VULKAN_MEMORY_TRACE
#define VULKAN_SHADER_TRACE
#define VULKAN_COMPUTE_PIPELINE_TRACE
#define VULKAN_COMMAND_BUFFER_TRACE
#define VULKAN_STAGING_TRACE
#define VULKAN_TEXTURE_TRACE
#endif
#define PROCESS_RUNNER_TRACE

#define GPUPERF_APP_NAME        "GPUPerfTest"
#define GPUPERF_VERSION_MAJOR   1
#define GPUPERF_VERSION_MINOR   0
#define GPUPERF_VERSION_PATCH   0

#define TEST_MKVERSION(major, minor, patch) (((major & 0xFFF) << 20) | ((minor & 0x3FF) << 10) | (patch & 0x3FF))
#define TEST_VER_MAJOR(version)             ((version >> 20) & 0xFFF)
#define TEST_VER_MINOR(version)             ((version >> 10) & 0x3FF)
#define TEST_VER_PATCH(version)             (version & 0x3FF)

#define TEST_TOOL_VERSION                   TEST_MKVERSION(GPUPERF_VERSION_MAJOR, GPUPERF_VERSION_MINOR, GPUPERF_VERSION_PATCH)

#define TEST_GUI_DEFAULT_WINDOW_WIDTH       (1000)
#define TEST_GUI_DEFAULT_WINDOW_HEIGHT      (900)

#define TEST_SUCCESS(status)                ((status) == TEST_OK)
#define TEST_RETFAIL(status)                {\
    test_status local_stat = (status);\
    if (!TEST_SUCCESS(local_stat)) return local_stat;\
}

#define TEST_UNUSED(x)  ((void)x)

/* Standard status range 0-2047 */
#define TEST_OK                                             0
#define TEST_UNKNOWN_ERROR                                  1
#define TEST_OUT_OF_MEMORY                                  2
#define TEST_FILE_NOT_FOUND                                 3
#define TEST_FILE_IO_ERROR                                  4
#define TEST_INVALID_PARAMETER                              5
#define TEST_UNKNOWN_TESTCASE                               6
#define TEST_UNKNOWN_DEVICE                                 7
#define TEST_NO_TEST_SPECIFIED                              8
#define TEST_PRINTF_FORMAT_ERROR                            9
#define TEST_OUT_OF_RANGE                                   10
#define TEST_PROGRAMMING_ERROR                              11
#define TEST_FAILED_TO_SPAWN_TEST_PROCESS                   12
#define TEST_FAILED_TO_INITIALIZE_GLFW                      13
#define TEST_FAILED_TO_INITIALIZE_GLEW                      14
#define TEST_FAILED_TO_INITIALIZE_IMGUI                     15
#define TEST_FAILED_TO_CREATE_WINDOW                        16
#define TEST_FAILED_TO_SPAWN_THREAD                         17
#define TEST_ASYNC_PROCESS_RUNNING                          18
#define TEST_PROCESS_KILLED                                 19
#define TEST_EMBEDDED_RESOURCE_NOT_FOUND                    20
#define TEST_FAILED_TO_LOAD_WINDOW_ICON                     21
#define TEST_FAILED_TO_DECODE_WINDOW_ICON                   22

/* Vulkan status range 2048-4095 */
#define TEST_VK_CREATE_INSTANCE_ERROR                       2048
#define TEST_VK_LAYER_ENUMERATION_ERROR                     2049
#define TEST_VK_LAYER_UNSUPPORTED                           2050
#define TEST_VK_DEBUG_MESSENGER_CREATE_ERROR                2051
#define TEST_VK_DEBUG_UTILS_PROC_ADDR_ERROR                 2052
#define TEST_VK_DEVICE_ENUMERATION_ERROR                    2053
#define TEST_VK_QUEUE_NOT_FOUND                             2054
#define TEST_VK_DEVICE_CREATE_ERROR                         2055
#define TEST_VK_SUITABLE_ALLOCATION_NOT_FOUND               2056
#define TEST_VK_MEMORY_ALLOCATION_ERROR                     2057
#define TEST_VK_BUFFER_CREATION_ERROR                       2058
#define TEST_VK_BUFFER_BIND_MEMORY_ERROR                    2059
#define TEST_VK_EXTENSION_ENUMERATION_ERROR                 2060
#define TEST_VK_SHADER_MODULE_CREATION_ERROR                2061
#define TEST_VK_PIPELINE_LAYOUT_CREATION_ERROR              2062
#define TEST_VK_COMPUTE_PIPELINE_CREATION_ERROR             2063
#define TEST_VK_DESCRIPTOR_SET_LAYOUT_CREATION_ERROR        2064
#define TEST_VK_DESCRIPTOR_POOL_CREATION_ERROR              2065
#define TEST_VK_DESCRIPTOR_SET_ALLOCATION_ERROR             2066
#define TEST_VK_COMMAND_POOL_CREATION_ERROR                 2067
#define TEST_VK_COMMAND_BUFFER_ALLOCATION_ERROR             2068
#define TEST_VK_COMMAND_BUFFER_BEGIN_ERROR                  2069
#define TEST_VK_COMMAND_BUFFER_END_ERROR                    2070
#define TEST_VK_SEMAPHORE_CREATION_ERROR                    2071
#define TEST_VK_FENCE_CREATION_ERROR                        2072
#define TEST_VK_QUEUE_SUBMIT_ERROR                          2073
#define TEST_VK_WAIT_FOR_FENCES_ERROR                       2074
#define TEST_VK_WAIT_FOR_FENCES_TIMEOUT                     2075
#define TEST_VK_BACKING_ALREADY_ALLOCATED                   2076
#define TEST_VK_CYBERPUNK_CRASHED_AGAIN                     2077
#define TEST_VK_BUFFER_NAME_TOO_LONG                        2078
#define TEST_VK_BACKING_SIZE_IS_ZERO                        2079
#define TEST_VK_BACKING_NOT_ALLOCATED                       2080
#define TEST_VK_BACKING_STILL_ALLOCATED                     2081
#define TEST_VK_UNKNOWN_BUFFER_TYPE                         2082
#define TEST_VK_SHADER_DESCRIPTORS_ALREADY_CREATED          2083
#define TEST_VK_SHADER_ZERO_SIZED_DESCRIPTOR_SET            2084
#define TEST_VK_SHADER_DESCRIPTORS_NOT_CREATED              2085
#define TEST_VK_COMPUTE_PIPELINE_SHADER_STAGE_MISMATCH      2086
#define TEST_VK_BINDING_UNKNOWN_DESCRIPTOR                  2087
#define TEST_VK_BINDING_UNKNOWN_MEMORY_REGION               2088
#define TEST_VK_OUT_OF_COMMAND_BUFFERS                      2089
#define TEST_VK_COMMAND_SEQUENCE_ALREADY_STARTED            2090
#define TEST_VK_COMMAND_SEQUENCE_NOT_STARTED                2091
#define TEST_VK_COMMAND_BUFFER_RESET_ERROR                  2092
#define TEST_VK_DISPATCH_TOO_LARGE                          2093
#define TEST_VK_MEMORY_MAPPING_ERROR                        2094
#define TEST_VK_IMAGE_CREATION_ERROR                        2095
#define TEST_VK_IMAGE_BIND_MEMORY_ERROR                     2096
#define TEST_VK_UNSUPPORTED_IMAGE_LAYOUT_TRANSITION         2097
#define TEST_VK_BINDING_UNKNOWN_TEXTURE                     2098
#define TEST_VK_IMAGE_VIEW_CREATION_ERROR                   2099
#define TEST_VK_SAMPLER_CREATION_ERROR                      2100
#define TEST_VK_WORKGROUP_COUNT_NOT_DIVISIBLE               2101
#define TEST_VK_FEATURE_UNSUPPORTED                         2102
#define TEST_VK_INVALID_QUEUE_INDEX                         2103
#define TEST_VK_COMMAND_SEQUENCE_NOT_MULTI_QUEUE            2104
#define TEST_VK_WAIT_FOR_FENCES_NOT_READY                   2105

/* Windows status range 4096-4099 */
#define WIN_D3DKMT_FAIL_INIT                                4096
#define WIN_SETUPDIGETCLASSDEV_FAIL                         4097
#define WIN_D3DKMT_OPENADAPTERFROMDEVICENAME_FAIL           4098
#define WIN_D3DKMT_CLOSEADAPTER_FAIL                        4099

typedef uint32_t test_status;
typedef enum test_result_output_t {
    test_result_readable,
    test_result_raw,
    test_result_csv
} test_result_output;

typedef enum test_ui_mode_t {
    test_ui_mode_gui,
    test_ui_mode_cli
} test_ui_mode;

test_result_output MainGetTestResultFormat();
const char *MainGetBinaryPath();
test_ui_mode MainGetTestUIMode();
void MainToggleConsoleWindow();

#ifdef __cplusplus
}
#endif
#endif