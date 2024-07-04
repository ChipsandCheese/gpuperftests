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

#include "main.h"
#include "helper.h"
#include "gui/gui.h"
#include "gui/gui_benchmarks.h"
#include "tests/test_vk_bandwidth.h"
#include "tests/test_vk_latency.h"

static helper_arraylist gui_panels;

static gui_panel *_GuiBenchmarksNewPanel(uint32_t gpu_count, const char *display_name, const char* panel_tooltip) {
    if (display_name == NULL) {
        return NULL;
    }
    gui_panel panel = {};

    test_status status = HelperArrayListInitialize(&(panel.sections), sizeof(gui_section) + gpu_count * sizeof(helper_arraylist));
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    memset(&(panel.display_name_mainpanel), 0, sizeof(panel.display_name_mainpanel));
    panel.display_name_mainpanel.imgui_identifier = "MainPanel";
    panel.display_name_mainpanel.localized_string.key = display_name;
    memset(&(panel.display_name_benchmarklist), 0, sizeof(panel.display_name_benchmarklist));
    panel.display_name_benchmarklist.imgui_identifier = "BenchmarkList";
    panel.display_name_benchmarklist.localized_string.key = display_name;
    memset(&(panel.display_name_exportcheckbox), 0, sizeof(panel.display_name_exportcheckbox));
    panel.display_name_exportcheckbox.imgui_identifier = "ExportCheckbox";
    panel.display_name_exportcheckbox.localized_string.key = display_name;
    memset(&(panel.panel_tooltip_description), 0, sizeof(panel.panel_tooltip_description));
    panel.panel_tooltip_description.key = panel_tooltip;
    panel.selected_in_ui = false;
    panel.selected_for_csv = false;

    size_t index = 0;
    status = HelperArrayListAdd(&gui_panels, &panel, sizeof(panel), &index);
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    return (gui_panel *)HelperArrayListGet(&gui_panels, index);
}

static gui_section *_GuiBenchmarksNewSectionMultiResult(uint32_t gpu_count, gui_panel *panel, const char *display_name, helper_arraylist *labels, const char* section_tooltip) {
    if (display_name == NULL || panel == NULL) {
        return NULL;
    }
    gui_section *section = (gui_section *)malloc(sizeof(gui_section) + gpu_count * sizeof(helper_arraylist));
    if (section == NULL) {
        return NULL;
    }
    memset(section, 0, sizeof(gui_section) + gpu_count * sizeof(helper_arraylist));

    for (uint32_t i = 0; i < gpu_count; i++) {
        test_status status = HelperArrayListInitialize(&(section->benchmarks[i]), sizeof(gui_benchmark));
        if (!TEST_SUCCESS(status)) {
            return NULL;
        }
    }
    memset(&(section->display_name_mainsection), 0, sizeof(section->display_name_mainsection));
    section->display_name_mainsection.imgui_identifier = "MainSection";
    section->display_name_mainsection.localized_string.key = display_name;
    memset(&(section->display_name_sectiontab), 0, sizeof(section->display_name_sectiontab));
    section->display_name_sectiontab.imgui_identifier = "SectionTab";
    section->display_name_sectiontab.localized_string.key = display_name;
    memset(&(section->section_tooltip_description), 0, sizeof(section->section_tooltip_description));
    section->section_tooltip_description.key = section_tooltip;
    section->benchmark_type = gui_benchmarks_type_multiple_result;
    test_status status = HelperArrayListCopy(&(section->labels), labels);
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }

    size_t index = 0;
    status = HelperArrayListAdd(&(panel->sections), section, sizeof(gui_section) + gpu_count * sizeof(helper_arraylist), &index);
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    return (gui_section *)HelperArrayListGet(&(panel->sections), index);
}

static gui_section *_GuiBenchmarksNewSectionSingleResult(uint32_t gpu_count, gui_panel *panel, const char *display_name, const char *section_tooltip) {
    if (display_name == NULL || panel == NULL) {
        return NULL;
    }
    gui_section *section = (gui_section *)malloc(sizeof(gui_section) + gpu_count * sizeof(helper_arraylist));
    if (section == NULL) {
        return NULL;
    }
    memset(section, 0, sizeof(gui_section) + gpu_count * sizeof(helper_arraylist));
    test_status status = HelperArrayListInitialize(&(section->runall_buttons), sizeof(gui_translated_benchmark_result_string));
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    gui_translated_benchmark_result_string runall_button;
    memset(&runall_button, 0, sizeof(runall_button));
    runall_button.imgui_string.localized_string.key = "gui.section.main.run_all";

    for (uint32_t i = 0; i < gpu_count; i++) {
        status = HelperArrayListInitialize(&(section->benchmarks[i]), sizeof(gui_benchmark));
        if (!TEST_SUCCESS(status)) {
            return NULL;
        }
        status = HelperArrayListAdd(&(section->runall_buttons), &runall_button, sizeof(runall_button), NULL);
        if (!TEST_SUCCESS(status)) {
            return NULL;
        }
    }
    memset(&(section->display_name_mainsection), 0, sizeof(section->display_name_mainsection));
    section->display_name_mainsection.imgui_identifier = "MainSection";
    section->display_name_mainsection.localized_string.key = display_name;
    memset(&(section->display_name_sectiontab), 0, sizeof(section->display_name_sectiontab));
    section->display_name_sectiontab.imgui_identifier = "SectionTab";
    section->display_name_sectiontab.localized_string.key = display_name;
    memset(&(section->section_tooltip_description), 0, sizeof(section->section_tooltip_description));
    section->section_tooltip_description.key = section_tooltip;
    section->benchmark_type = gui_benchmarks_type_single_result;

    size_t index = 0;
    status = HelperArrayListAdd(&(panel->sections), section, sizeof(gui_section) + gpu_count * sizeof(helper_arraylist), &index);
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    return (gui_section *)HelperArrayListGet(&(panel->sections), index);
}

static test_status _GuiBenchmarksAddBenchmark(uint32_t gpu_count, gui_section *section, const char *display_name, const char *test_name, gui_result_type result_type) {
    if (section == NULL || display_name == NULL || test_name == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    gui_benchmark benchmark = {};

    memset(&(benchmark.display_name), 0, sizeof(benchmark.display_name));
    benchmark.display_name.key = display_name;
    benchmark.test_name = test_name;
    benchmark.completion_code = TEST_OK;
    benchmark.has_been_run = false;
    benchmark.result_type = result_type;
    memset(&(benchmark.button_blank), 0, sizeof(benchmark.button_blank));
    memset(&(benchmark.button_running), 0, sizeof(benchmark.button_running));

    for (uint32_t i = 0; i < gpu_count; i++) {
        benchmark.gpu_index = i;
        benchmark.button_blank.imgui_string.localized_string.key = "gui.section.main.result.blank";
        benchmark.button_running.imgui_string.localized_string.key = "gui.section.main.result.running";

        test_status status = HelperArrayListAdd(&(section->benchmarks[i]), &benchmark, sizeof(benchmark), NULL);
        TEST_RETFAIL(status);
    }
    return TEST_OK;
}

static test_status _GuiBenchmarksCreateLabelsFromCapacities(helper_arraylist *labels, const uint64_t *data, size_t element_count) {
    test_status status = HelperArrayListInitialize(labels, sizeof(const char *));
    TEST_RETFAIL(status);

    for (size_t i = 0; i < element_count; i++) {
        const char *label = NULL;

        helper_unit_pair unit_pair;
        HelperConvertUnitsBytes1024(data[i], &unit_pair);
        const char* format = "%.0f %s";
        float fractional_part = unit_pair.value - (float)((int32_t)unit_pair.value);
        if (fractional_part != 0.0f) {
            // We hardcode 0.5/0.25/0.75 thresholds since those are the only relevant ones to truncate
            // as this will always be showing some MB/GB capacities in predictable increments
            if (fractional_part == 0.5f) {
                format = "%.1f %s";
            } else if (fractional_part == 0.25f || fractional_part == 0.75f) {
                format = "%.2f %s";
            } else {
                format = "%.3f %s";
            }
        }
        status = HelperPrintToBuffer(&label, NULL, format, unit_pair.value, unit_pair.units);
        if (!TEST_SUCCESS(status)) {
            HelperArrayListClean(labels);
            return status;
        }
        status = HelperArrayListAdd(labels, label, sizeof(label), NULL);
        if (!TEST_SUCCESS(status)) {
            HelperArrayListClean(labels);
            return status;
        }
    }
    return TEST_OK;
}

test_status GuiBenchmarksRegister(helper_arraylist **list, uint32_t gpu_count) {
    test_status status = HelperArrayListInitialize(&gui_panels, sizeof(gui_panel));
    TEST_RETFAIL(status);
    gui_panel *panel = NULL;
    gui_section *section = NULL;
    helper_arraylist labels;

    panel = _GuiBenchmarksNewPanel(gpu_count, "bench.insnrate.panel", "bench.insnrate.tooltip");
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.madd", "bench.insnrate.tooltip.madd");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_mac", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_mac", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_mac", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int8", "vk_rate_int8_mac", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int16", "vk_rate_int16_mac", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int32", "vk_rate_int32_mac", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int64", "vk_rate_int64_mac", gui_result_type_iops));
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.add", "bench.insnrate.tooltip.add");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_add", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_add", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_add", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int8", "vk_rate_int8_add", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int16", "vk_rate_int16_add", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int32", "vk_rate_int32_add", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int64", "vk_rate_int64_add", gui_result_type_iops));
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.sub", "bench.insnrate.tooltip.sub");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_sub", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_sub", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_sub", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int8", "vk_rate_int8_sub", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int16", "vk_rate_int16_sub", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int32", "vk_rate_int32_sub", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int64", "vk_rate_int64_sub", gui_result_type_iops));
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.mul", "bench.insnrate.tooltip.mul");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_mul", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_mul", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_mul", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int8", "vk_rate_int8_mul", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int16", "vk_rate_int16_mul", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int32", "vk_rate_int32_mul", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int64", "vk_rate_int64_mul", gui_result_type_iops));
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.div", "bench.insnrate.tooltip.div");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_div", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_div", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_div", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int8", "vk_rate_int8_div", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int16", "vk_rate_int16_div", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int32", "vk_rate_int32_div", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int64", "vk_rate_int64_div", gui_result_type_iops));
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.rem", "bench.insnrate.tooltip.rem");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_rem", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_rem", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_rem", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int8", "vk_rate_int8_rem", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int16", "vk_rate_int16_rem", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int32", "vk_rate_int32_rem", gui_result_type_iops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.int64", "vk_rate_int64_rem", gui_result_type_iops));
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.rcp", "bench.insnrate.tooltip.rcp");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_rcp", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_rcp", gui_result_type_flops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_rcp", gui_result_type_flops));
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.insnrate.section.isqrt", "bench.insnrate.tooltip.isqrt");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp16", "vk_rate_fp16_isqrt", gui_result_type_ops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp32", "vk_rate_fp32_isqrt", gui_result_type_ops));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.insnrate.format.fp64", "vk_rate_fp64_isqrt", gui_result_type_ops));

    TEST_RETFAIL(_GuiBenchmarksCreateLabelsFromCapacities(&labels, VulkanBandwidthGetRegionSizes(), VulkanBandwidthGetRegionCount()));
    panel = _GuiBenchmarksNewPanel(gpu_count, "bench.membandwidth.panel", "bench.membandwidth.tooltip");
    section = _GuiBenchmarksNewSectionMultiResult(gpu_count, panel, "bench.membandwidth.panel", &labels, "bench.membandwidth.tooltip");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.membandwidth.panel", "vk_bandwidth", gui_result_type_byterate));
    HelperArrayListClean(&labels);

    TEST_RETFAIL(_GuiBenchmarksCreateLabelsFromCapacities(&labels, VulkanLatencyGetRegionSizes(), VulkanLatencyGetRegionCount()));
    panel = _GuiBenchmarksNewPanel(gpu_count, "bench.memlatency.panel", "bench.memlatency.tooltip");
    section = _GuiBenchmarksNewSectionMultiResult(gpu_count, panel, "bench.memlatency.section.vector", &labels, "bench.memlatency.tooltip.vector");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.memlatency.section.vector", "vk_latency_vector", gui_result_type_picoseconds));
    section = _GuiBenchmarksNewSectionMultiResult(gpu_count, panel, "bench.memlatency.section.scalar", &labels, "bench.memlatency.tooltip.scalar");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.memlatency.section.scalar", "vk_latency_scalar", gui_result_type_picoseconds));
    HelperArrayListClean(&labels);
    
    panel = _GuiBenchmarksNewPanel(gpu_count, "bench.uplink.panel", "bench.uplink.tooltip");
    section = _GuiBenchmarksNewSectionSingleResult(gpu_count, panel, "bench.uplink.panel", "bench.uplink.tooltip");
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.gpu_to_cpu.copy", "vk_uplink_copy_read", gui_result_type_byterate));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.gpu_to_cpu.compute", "vk_uplink_compute_read", gui_result_type_byterate));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.gpu_to_cpu.mapped", "vk_uplink_mapped_read", gui_result_type_byterate));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.cpu_to_gpu.copy", "vk_uplink_copy_write", gui_result_type_byterate));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.cpu_to_gpu.compute", "vk_uplink_compute_write", gui_result_type_byterate));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.cpu_to_gpu.mapped", "vk_uplink_mapped_write", gui_result_type_byterate));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.latency.short", "vk_uplink_latency", gui_result_type_picoseconds));
    TEST_RETFAIL(_GuiBenchmarksAddBenchmark(gpu_count, section, "bench.uplink.latency.long", "vk_uplink_latency_long", gui_result_type_picoseconds));
    HelperArrayListClean(&labels);

    *list = &gui_panels;
    return TEST_OK;
}