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

#ifndef GUI_H
#define GUI_H

#include "gui/gui_localization.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUI_DEFAULT_MULTI_TABLE_SIZE    10

typedef struct gui_gpu_t {
    const char *name;
    const char *display_name;
    uint32_t index;
} gui_gpu;

typedef struct gui_translated_imgui_string_t {
    gui_localization_string localized_string;
    const char *localized_cache;
    const char *imgui_identifier;
    const char *string_buffer;
} gui_translated_imgui_string;

typedef struct gui_translated_benchmark_result_string_t {
    gui_translated_imgui_string imgui_string;
    const char *cached_test_name;
    uint32_t cached_result_index;
    uint32_t cached_gpu_index;
} gui_translated_benchmark_result_string;

typedef struct gui_cached_benchmark_result_string_t {
    const char *string_with_suffix;
    const char *string;
    const char *cached_test_name;
    uint32_t cached_result_index;
    uint32_t cached_gpu_index;
} gui_cached_benchmark_result_string;

test_status GuiRun();

#ifdef __cplusplus
}
#endif
#endif