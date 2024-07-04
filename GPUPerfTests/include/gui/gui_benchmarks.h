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

#ifndef GUI_BENCHMARKS_H
#define GUI_BENCHMARKS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gui_benchmarks_type_t {
    gui_benchmarks_type_single_result,
    gui_benchmarks_type_multiple_result
} gui_benchmarks_type;

typedef enum gui_result_type_t {
    gui_result_type_error,
    gui_result_type_unsupported,
    gui_result_type_cancelled,
    gui_result_type_byterate,
    gui_result_type_bitrate,
    gui_result_type_picoseconds,
    gui_result_type_ops,
    gui_result_type_iops,
    gui_result_type_flops
} gui_result_type;

typedef struct gui_benchmark_t {
    const char *test_name;
    gui_localization_string display_name;
    gui_translated_benchmark_result_string button_blank;
    gui_translated_benchmark_result_string button_running;
    uint32_t gpu_index;
    test_status completion_code;
    void *process_handle;
    bool has_been_run;
    gui_result_type result_type;
    helper_arraylist results;
    helper_arraylist raw_results;
} gui_benchmark;

typedef struct gui_section_t {
    gui_translated_imgui_string display_name_mainsection;
    gui_translated_imgui_string display_name_sectiontab;
    gui_localization_string section_tooltip_description;
    bool selected_in_ui;
    gui_benchmarks_type benchmark_type;
    helper_arraylist runall_buttons;
    helper_arraylist labels;
    helper_arraylist benchmarks[1];
} gui_section;

typedef struct gui_panel_t {
    gui_translated_imgui_string display_name_mainpanel;
    gui_translated_imgui_string display_name_benchmarklist;
    gui_translated_imgui_string display_name_exportcheckbox;
    gui_localization_string panel_tooltip_description;
    bool selected_in_ui;
    bool selected_for_csv;
    helper_arraylist sections;
} gui_panel;

typedef struct gui_queued_benchmark_t {
    gui_benchmark *benchmark;
    uint32_t sequential_queue_id;
    gui_translated_benchmark_result_string cancel_button;
} gui_queued_benchmark;

test_status GuiBenchmarksRegister(helper_arraylist **list, uint32_t gpu_count);

#ifdef __cplusplus
}
#endif
#endif