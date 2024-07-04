// Copyright (c) 2023, Nemes <nemes@nemez.net>
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

#ifdef _WIN32

#ifndef WINDOWS_HELPER_H
#define WINDOWS_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#define WIN_MAX_STRING_LENGTH 48

typedef UINT	D3DKMT_HANDLE;

typedef struct windows_helper_gpu_t {
    char name[WIN_MAX_STRING_LENGTH];
    WCHAR path[MAX_PATH];
    size_t gpu_index;
    LUID device_luid;
    D3DKMT_HANDLE adapter_handle;
} windows_helper_gpu;

test_status WindowsHelperInitializeGPUs(void *win_devices, uint32_t *device_count);
test_status WindowsHelperCleanGPUs(void *win_devices);
uint64_t WindowsHelperGPUGetLUID(windows_helper_gpu *device);

#ifdef __cplusplus
}
#endif
#endif
#endif
