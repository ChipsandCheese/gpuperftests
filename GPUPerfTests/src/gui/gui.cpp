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
#include "process_runner.h"
#include "logger.h"
#include "resources.h"
#include "gui/gui.h"
#include "gui/gui_benchmarks.h"
#include "gui/gui_localization.h"
#include "tests/test_vk_list.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <imgui_freetype.h>
#include <spng.h>
#include <nfd.h>
#include "gui/gui_formatter.h"
#include "build_info.h"

#define GUI_MINIMUM_REFRESHRATE     (5)

#define GUI_IMGUI_STRING(name, key, suffix) static gui_translated_imgui_string name = {{key, NULL}, NULL, suffix, NULL};

static helper_arraylist gui_gpus;
static helper_linkedlist gui_queue;
static gui_benchmark *gui_current_benchmark;
static bool gui_benchmark_running;
static bool gui_exporting;
static bool gui_about;
static GLuint gui_font_atlas_texture = 0;
static ImGuiStyle gui_default_style;
static uint32_t gui_queue_dispatch_counter;
static ImFont *gui_font_regular;
static ImFont *gui_font_italics;
static ImFont *gui_font_bold;
static ImFont *gui_font_heading;
static ImFont *gui_font_monospace;
static gui_formatted_string *gui_formatted_text_about;
static gui_formatted_string *gui_formatted_text_changelog;

// Title strings
GUI_IMGUI_STRING(gui_string_title_controls, "gui.title.controls", "Controls");
GUI_IMGUI_STRING(gui_string_title_benchmarks, "gui.title.benchmarks", "Benchmarks");
GUI_IMGUI_STRING(gui_string_title_export, "gui.title.export", "Export");
GUI_IMGUI_STRING(gui_string_title_about, "gui.title.about", "About");
// Controls section strings
static gui_localization_string gui_string_controls_current = {"gui.section.controls.current", NULL};
static gui_localization_string gui_string_controls_none = {"gui.section.controls.none", NULL};
GUI_IMGUI_STRING(gui_string_controls_list, "gui.section.controls.list", "Controls");
GUI_IMGUI_STRING(gui_string_controls_console, "gui.section.controls.button.console", "Controls");
GUI_IMGUI_STRING(gui_string_controls_about, "gui.section.controls.button.about", "Controls");
GUI_IMGUI_STRING(gui_string_controls_export, "gui.section.controls.button.export", "Controls");
GUI_IMGUI_STRING(gui_string_controls_cancel, "gui.section.controls.list.button.cancel", "Controls");
// Benchmarks section strings
GUI_IMGUI_STRING(gui_string_benchmarks_list, "gui.section.benchmarks.list", "Benchmarks");
// Main section strings
GUI_IMGUI_STRING(gui_string_main_none, "gui.section.main.none_selected", "Main");
static gui_localization_string gui_string_main_result_unsupported = {"gui.section.main.result.unsupported", NULL};
static gui_localization_string gui_string_main_result_error = {"gui.section.main.result.error", NULL};
static gui_localization_string gui_string_main_result_cancelled = {"gui.section.main.result.cancelled", NULL};
// Export popup strings
static gui_localization_string gui_string_export_instruct = {"gui.popup.export.instruct", NULL};
GUI_IMGUI_STRING(gui_string_export_selectall, "gui.popup.export.button.select_all", "Export");
GUI_IMGUI_STRING(gui_string_export_clearall, "gui.popup.export.button.clear_all", "Export");
GUI_IMGUI_STRING(gui_string_export_cancel, "gui.popup.export.button.cancel", "Export");
GUI_IMGUI_STRING(gui_string_export_export, "gui.popup.export.button.export", "Export");
// About popup strings
GUI_IMGUI_STRING(gui_string_about_about, "gui.popup.about.section.about", "About");
GUI_IMGUI_STRING(gui_string_about_changelog, "gui.popup.about.section.changelog", "About");
GUI_IMGUI_STRING(gui_string_about_build, "gui.popup.about.section.build", "About");
GUI_IMGUI_STRING(gui_string_about_close, "gui.popup.about.button.close", "About");
static gui_localization_string gui_string_about_buildinfo_title = {"gui.popup.about.buildinfo.title", NULL};
static gui_localization_string gui_string_about_buildinfo_version = {"gui.popup.about.buildinfo.version", NULL};
static gui_localization_string gui_string_about_buildinfo_type = {"gui.popup.about.buildinfo.type", NULL};
static gui_localization_string gui_string_about_buildinfo_configuration = {"gui.popup.about.buildinfo.configuration", NULL};
static gui_localization_string gui_string_about_buildinfo_configuration_release = {"gui.popup.about.buildinfo.configuration.release", NULL};
static gui_localization_string gui_string_about_buildinfo_configuration_debug = {"gui.popup.about.buildinfo.configuration.debug", NULL};
static gui_localization_string gui_string_about_buildinfo_source = {"gui.popup.about.buildinfo.source", NULL};
static gui_localization_string gui_string_about_buildinfo_source_local = {"gui.popup.about.buildinfo.source.local", NULL};
static gui_localization_string gui_string_about_buildinfo_branch = {"gui.popup.about.buildinfo.branch", NULL};
static gui_localization_string gui_string_about_buildinfo_branch_local = {"gui.popup.about.buildinfo.branch.local", NULL};

static void _GuiInitializeStyle();
static const char *_GuiTranslateImGuiStringUpdate(gui_translated_imgui_string *string, bool force_update);
static const char *_GuiTranslateImGuiString(gui_translated_imgui_string *string);
static const char *_GuiTranslateImGuiStringTestSuffix(gui_translated_benchmark_result_string *string, const char *test_name, uint32_t result_index, uint32_t gpu_index);
static const char *_GuiGetStringWithTestSuffix(gui_cached_benchmark_result_string *string, const char *test_name, uint32_t result_index, uint32_t gpu_index);

static ImFont *_GuiLoadFont(ImGuiIO &io, const char *filename, float font_size) {
    const char *font_file = NULL;
    size_t font_file_size = 0;
    test_status status = ResourcesGetFontFile(filename, &font_file, &font_file_size);
    if (!TEST_SUCCESS(status)) {
        WARNING("Failed to load font \"%s\"\n", filename);
        return io.Fonts->AddFontDefault();
    } else {
        ImFontConfig font_config;
        font_config.FontDataOwnedByAtlas = false;
        // Cast the font size to int first to make sure it is a whole number as this is better for fonts
        return io.Fonts->AddFontFromMemoryTTF((void *)font_file, (int)font_file_size, (float)(int)(font_size), &font_config);
    }
}

static void _GuiScaleAndApplyStyle(ImGuiIO &io, float scale_factor) {
    io.Fonts->Clear();
    gui_font_regular = _GuiLoadFont(io, "Roboto-Regular.ttf", 16.0f * scale_factor); // Standard text font
    gui_font_italics = _GuiLoadFont(io, "Roboto-Italic.ttf", 16.0f * scale_factor); // Italics text font
    gui_font_bold = _GuiLoadFont(io, "Roboto-Bold.ttf", 16.0f * scale_factor); // Bold text font
    gui_font_heading = _GuiLoadFont(io, "Roboto-Bold.ttf", 32.0f * scale_factor); // Heading text font
    gui_font_monospace = _GuiLoadFont(io, "RobotoMono-Regular.ttf", 16.0f * scale_factor); // Monospace text font
    io.Fonts->Build();
    io.Fonts->SetTexID((ImTextureID)(size_t)gui_font_atlas_texture);

    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    glBindTexture(GL_TEXTURE_2D, gui_font_atlas_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    ImGuiStyle &style = ImGui::GetStyle();
    memcpy(&style, &gui_default_style, sizeof(gui_default_style));
    style.ScaleAllSizes(scale_factor);
}

static test_status _GuiGetGPUList() {
    helper_arraylist results;
    test_status status = HelperArrayListInitialize(&results, sizeof(process_runner_keypair));
    TEST_RETFAIL(status);
    status = ProcessRunnerRunTest(TESTS_VULKAN_LIST_NAME, 0, &results);
    if (!TEST_SUCCESS(status)) {
        HelperArrayListClean(&results);
        return status;
    }
    status = HelperArrayListInitialize(&gui_gpus, sizeof(gui_gpu));
    if (!TEST_SUCCESS(status)) {
        HelperArrayListClean(&results);
        return status;
    }
    for (size_t i = 0; i < HelperArrayListSize(&results); i++) {
        char *gpu_name = ((process_runner_keypair *)HelperArrayListGet(&results, i))->key;
        helper_unit_pair vram;
        uint64_t vram_capacity = strtoull(((process_runner_keypair *)HelperArrayListGet(&results, i))->result, NULL, 10);
        HelperConvertUnitsBytes1024(vram_capacity, &vram);
        // Yeah, this isn't accurate for VRAM since AMD and NVIDIA exclude/include the host visible region differently.
        INFO("Detected \"%s\" (%.3f %s) as GPU #%llu\n", gpu_name, vram.value, vram.units, i);
        gui_gpu gpu;
        status = HelperPrintToBuffer(&gpu.name, NULL, gpu_name);
        if (!TEST_SUCCESS(status)) {
            HelperArrayListClean(&results);
            HelperArrayListClean(&gui_gpus);
            return status;
        }
        status = HelperPrintToBuffer(&gpu.display_name, NULL, "%llu: %s", i, gpu_name);
        if (!TEST_SUCCESS(status)) {
            free((void *)gpu.name);
            HelperArrayListClean(&results);
            HelperArrayListClean(&gui_gpus);
            return status;
        }
        gpu.index = (uint32_t)i;
        status = HelperArrayListAdd(&gui_gpus, &gpu, sizeof(gpu), NULL);
        if (!TEST_SUCCESS(status)) {
            free((void *)gpu.name);
            free((void *)gpu.display_name);
            HelperArrayListClean(&results);
            HelperArrayListClean(&gui_gpus);
            return status;
        }
    }

    HelperArrayListClean(&results);
    return TEST_OK;
}

static test_status _GuiConvertResult(process_runner_keypair *raw_result, const char **final_result, size_t result_buffer_size, gui_result_type result_type) {
    if (result_type == gui_result_type_byterate) {
        helper_unit_pair unit_pair;
        HelperConvertUnitsBytes1024(strtoull(raw_result->result, NULL, 10), &unit_pair);
        return HelperPrintToBuffer(final_result, NULL, "%.3f %s/s", unit_pair.value, unit_pair.units);
    } else if (result_type == gui_result_type_bitrate) {
        helper_unit_pair unit_pair;
        HelperConvertUnitsBits1000(strtoull(raw_result->result, NULL, 10), &unit_pair);
        return HelperPrintToBuffer(final_result, NULL, "%.3f %sps", unit_pair.value, unit_pair.units);
    } else if (result_type == gui_result_type_picoseconds) {
        return HelperPrintToBuffer(final_result, NULL, "%.3f ns", ((float)strtoull(raw_result->result, NULL, 10)) / 1000.0f);
    } else if (result_type == gui_result_type_ops || result_type == gui_result_type_flops || result_type == gui_result_type_iops) {
        const char *op_type = "OPS";
        if (result_type == gui_result_type_flops) {
            op_type = "FLOPS";
        } else if (result_type == gui_result_type_iops) {
            op_type = "IOPS";
        }
        helper_unit_pair unit_pair;
        HelperConvertUnitsPlain1000(strtoull(raw_result->result, NULL, 10), &unit_pair);
        return HelperPrintToBuffer(final_result, NULL, "%.3f %s%s", unit_pair.value, unit_pair.units, op_type);
    } else if (result_type == gui_result_type_error) {
        return HelperPrintToBuffer(final_result, NULL, GuiLocalizationTranslate(&gui_string_main_result_error));
    } else if (result_type == gui_result_type_unsupported) {
        return HelperPrintToBuffer(final_result, NULL, GuiLocalizationTranslate(&gui_string_main_result_unsupported));
    } else if (result_type == gui_result_type_cancelled) {
        return HelperPrintToBuffer(final_result, NULL, GuiLocalizationTranslate(&gui_string_main_result_cancelled));
    } else {
        return TEST_INVALID_PARAMETER;
    }
}

static gui_benchmark *_GuiDequeueBenchmark() {
    gui_queued_benchmark *benchmark = (gui_queued_benchmark *)HelperLinkedListGet(&gui_queue, 0);
    if (benchmark == NULL) {
        return NULL;
    }
    HelperLinkedListRemove(&gui_queue, 0);
    if (benchmark->benchmark->has_been_run) {
        HelperArrayListClean(&(benchmark->benchmark->raw_results));
        size_t result_count = HelperArrayListSize(&(benchmark->benchmark->results));
        for (size_t i = 0; i < result_count; i++) {
            gui_cached_benchmark_result_string *result_string = (gui_cached_benchmark_result_string *)HelperArrayListGet(&(benchmark->benchmark->results), i);
            if (result_string->string != NULL) {
                free((void *)result_string->string);
                result_string->string = NULL;
            }
        }
        HelperArrayListClean(&(benchmark->benchmark->results));
        benchmark->benchmark->has_been_run = false;
    }
    test_status status = HelperArrayListInitialize(&(benchmark->benchmark->raw_results), sizeof(process_runner_keypair));
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    status = HelperArrayListInitialize(&(benchmark->benchmark->results), sizeof(gui_cached_benchmark_result_string));
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    INFO("Running benchmark %s on GPU #%lu\n", benchmark->benchmark->test_name, benchmark->benchmark->gpu_index);
    status = ProcessRunnerRunTestAsync(benchmark->benchmark->test_name, benchmark->benchmark->gpu_index, &(benchmark->benchmark->raw_results), &(benchmark->benchmark->completion_code), &(benchmark->benchmark->process_handle));
    if (!TEST_SUCCESS(status)) {
        return NULL;
    }
    if (benchmark->cancel_button.imgui_string.imgui_identifier != NULL) {
        free((void *)benchmark->cancel_button.imgui_string.imgui_identifier);
    }
    if (benchmark->cancel_button.imgui_string.string_buffer != NULL) {
        free((void *)benchmark->cancel_button.imgui_string.string_buffer);
    }
    gui_benchmark *bench = benchmark->benchmark;
    free((void *)benchmark);
    return bench;
}

static void _GuiQueueBenchmark(gui_benchmark *benchmark) {
    gui_queued_benchmark *queued_bench = (gui_queued_benchmark *)malloc(sizeof(gui_queued_benchmark));
    if (queued_bench != NULL) {
        memset(queued_bench, 0, sizeof(gui_queued_benchmark));
        queued_bench->benchmark = benchmark;
        queued_bench->sequential_queue_id = gui_queue_dispatch_counter;
        queued_bench->cancel_button.imgui_string.localized_string.key = "gui.section.controls.list.button.remove";
        HelperLinkedListAdd(&gui_queue, queued_bench, NULL);

        gui_queue_dispatch_counter++;
    }
}

static void _GuiRenderSection(uint32_t gpu_count, gui_section *section) {
    float gpu_column_width = 0;

    for (uint32_t i = 0; i < gpu_count; i++) {
        gui_gpu *gpu = (gui_gpu *)HelperArrayListGet(&gui_gpus, i);
        float text_width = ImGui::CalcTextSize(gpu->display_name).x;
        if (text_width + 20 > gpu_column_width) {
            gpu_column_width = text_width + 20;
        }
    }
    if (section->benchmark_type == gui_benchmarks_type_single_result) {
        size_t test_count = HelperArrayListSize(&(section->benchmarks[0]));
        ImGui::BeginTable(_GuiTranslateImGuiString(&(section->display_name_mainsection)), gpu_count + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX);
        ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, gpu_column_width);
        for (size_t i = 0; i < gpu_count; i++) {
            gui_gpu *gpu = (gui_gpu *)HelperArrayListGet(&gui_gpus, i);
            ImGui::TableSetupColumn(gpu->display_name, ImGuiTableColumnFlags_WidthFixed, gpu_column_width);
        }
        ImGui::TableHeadersRow();
        for (size_t j = 0; j < test_count; j++) {
            gui_benchmark *display_benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[0]), j);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text(GuiLocalizationTranslate(&(display_benchmark->display_name)));
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(display_benchmark->test_name);
            }
            for (uint32_t i = 0; i < gpu_count; i++) {
                gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[i]), j);
                ImGui::TableNextColumn();
                const char *button_label = NULL;
                if (benchmark->has_been_run) {
                    gui_cached_benchmark_result_string *result_string = (gui_cached_benchmark_result_string *)HelperArrayListGet(&(benchmark->results), 0);
                    button_label = _GuiGetStringWithTestSuffix(result_string, benchmark->test_name, 0, i);
                } else if (benchmark->completion_code == TEST_ASYNC_PROCESS_RUNNING) {
                    button_label = _GuiTranslateImGuiStringTestSuffix(&(benchmark->button_running), benchmark->test_name, 0, i);
                }
                if (button_label == NULL) {
                    button_label = _GuiTranslateImGuiStringTestSuffix(&(benchmark->button_blank), benchmark->test_name, 0, i);
                }
                if (ImGui::Button(button_label, ImVec2(-1, 0))) {
                    _GuiQueueBenchmark(benchmark);
                }
            }
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        for (uint32_t i = 0; i < gpu_count; i++) {
            ImGui::TableNextColumn();
            gui_translated_benchmark_result_string *runall_button_text = (gui_translated_benchmark_result_string *)HelperArrayListGet(&(section->runall_buttons), i);
            if (ImGui::Button(_GuiTranslateImGuiStringTestSuffix(runall_button_text, "all", 0, i), ImVec2(0, 0))) {
                for (size_t j = 0; j < test_count; j++) {
                    gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[i]), j);
                    _GuiQueueBenchmark(benchmark);
                }
            }
        }
        ImGui::EndTable();
    } else {
        size_t max_result_count = 0;
        for (uint32_t i = 0; i < gpu_count; i++) {
            gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[i]), 0);
            if (benchmark->has_been_run) {
                size_t count = HelperArrayListSize(&(benchmark->raw_results));
                if (count > max_result_count) {
                    max_result_count = count;
                }
            }
        }
        if (max_result_count < GUI_DEFAULT_MULTI_TABLE_SIZE) {
            max_result_count = GUI_DEFAULT_MULTI_TABLE_SIZE;
            size_t label_count = HelperArrayListSize(&(section->labels));
            if (max_result_count > label_count) {
                max_result_count = label_count;
            }
        }
        ImGui::BeginTable(_GuiTranslateImGuiString(&(section->display_name_mainsection)), gpu_count + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX);
        ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, gpu_column_width);
        for (size_t i = 0; i < gpu_count; i++) {
            gui_gpu *gpu = (gui_gpu *)HelperArrayListGet(&gui_gpus, i);
            ImGui::TableSetupColumn(gpu->display_name, ImGuiTableColumnFlags_WidthFixed, gpu_column_width);
        }
        ImGui::TableHeadersRow();
        for (size_t j = 0; j < max_result_count; j++) {
            const char *label = (const char *)HelperArrayListGet(&(section->labels), j);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text(label);
            for (uint32_t i = 0; i < gpu_count; i++) {
                gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[i]), 0);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(benchmark->test_name);
                }
                ImGui::TableNextColumn();
                const char *button_label = NULL;
                if (benchmark->has_been_run) {
                    button_label = _GuiGetStringWithTestSuffix((gui_cached_benchmark_result_string *)HelperArrayListGet(&(benchmark->results), j), benchmark->test_name, (uint32_t)j, i);
                } else if (benchmark->completion_code == TEST_ASYNC_PROCESS_RUNNING) {
                    button_label = _GuiTranslateImGuiStringTestSuffix(&(benchmark->button_running), benchmark->test_name, (uint32_t)j, i);
                }
                if (button_label == NULL) {
                    button_label = _GuiTranslateImGuiStringTestSuffix(&(benchmark->button_blank), benchmark->test_name, (uint32_t)j, i);
                }
                if (ImGui::Button(button_label, ImVec2(-1, 0))) {
                    _GuiQueueBenchmark(benchmark);
                }
            }
        }
        ImGui::EndTable();
    }
}

static test_status _GuiLoadFormattedText(const char *filename, gui_formatted_string **formatted_text) {
    const char *about_content = NULL;
    size_t about_size = 0;
    test_status status = ResourcesGetTextFile(filename, &about_content, &about_size);
    if (!TEST_SUCCESS(status)) {
        WARNING("Failed to load text \"%s\"\n", filename);
        return status;
    }
    status = GuiFormatterFormatString(about_content, formatted_text);
    if (!TEST_SUCCESS(status)) {
        WARNING("Failed to format text \"%s\"\n", filename);
        return status;
    }
    return TEST_OK;
}

static test_status _GuiSetWindowIcon(GLFWwindow *window, const char *resource_name) {
    const char *icon_data = NULL;
    size_t icon_data_length = 0;
    test_status status = ResourcesGetIconFile(resource_name, &icon_data, &icon_data_length);
    if (!TEST_SUCCESS(status)) {
        return TEST_FAILED_TO_LOAD_WINDOW_ICON;
    }
    uint32_t icon_width = 0;
    uint32_t icon_height = 0;
    const char *icon_pixels = HelperDecodePng(icon_data, icon_data_length, SPNG_FMT_RGBA8, &icon_width, &icon_height);
    if (icon_pixels == NULL) {
        return TEST_FAILED_TO_DECODE_WINDOW_ICON;
    }
    GLFWimage glfw_icon;
    glfw_icon.width = icon_width;
    glfw_icon.height = icon_height;
    glfw_icon.pixels = (unsigned char *)icon_pixels;
    glfwSetWindowIcon(window, 1, &glfw_icon);
    free((void *)icon_pixels);
    return TEST_OK;
}

test_status GuiRun() {
    gui_benchmark_running = false;
    gui_exporting = false;
    gui_about = false;
    gui_queue_dispatch_counter = 0;
    test_status status = HelperLinkedListInitialize(&gui_queue);
    TEST_RETFAIL(status);
    status = _GuiGetGPUList();
    TEST_RETFAIL(status);
    // For now hard-coded to en_US
    status = GuiLocalizationInitialize("en_US");
    TEST_RETFAIL(status);

    status = _GuiLoadFormattedText("ABOUT.md", &gui_formatted_text_about);
    TEST_RETFAIL(status);
    status = _GuiLoadFormattedText("CHANGELOG.md", &gui_formatted_text_changelog);
    TEST_RETFAIL(status);

    size_t gpu_count = HelperArrayListSize(&gui_gpus);
    helper_arraylist *benchmark_panels = NULL;
    status = GuiBenchmarksRegister(&benchmark_panels, (uint32_t)gpu_count);
    TEST_RETFAIL(status);
    size_t panel_count = HelperArrayListSize(benchmark_panels);

    const char *about_section_version_string = NULL;
    status = HelperPrintToBuffer(&about_section_version_string, NULL, "%u.%u.%u.%s", TEST_VER_MAJOR(TEST_TOOL_VERSION), TEST_VER_MINOR(TEST_TOOL_VERSION), TEST_VER_PATCH(TEST_TOOL_VERSION), BUILD_INFO_IDENTIFIER);
    if (!TEST_SUCCESS(status)) {
        return status;
}

    const char *window_title = NULL;
#ifdef BUILD_INFO_TYPE_RELEASE
    status = HelperPrintToBuffer(&window_title, NULL, "GPU Performance Tester %u.%u.%u", TEST_VER_MAJOR(TEST_TOOL_VERSION), TEST_VER_MINOR(TEST_TOOL_VERSION), TEST_VER_PATCH(TEST_TOOL_VERSION));
#else
    status = HelperPrintToBuffer(&window_title, NULL, "GPU Performance Tester %u.%u.%u.%s%s", TEST_VER_MAJOR(TEST_TOOL_VERSION), TEST_VER_MINOR(TEST_TOOL_VERSION), TEST_VER_PATCH(TEST_TOOL_VERSION), BUILD_INFO_IDENTIFIER, BUILD_INFO_TYPE_SUFFIX);
#endif
    if (!TEST_SUCCESS(status)) {
        return status;
    }
    if (glfwInit() == GLFW_FALSE) {
        free((void *)about_section_version_string);
        free((void *)window_title);
        return TEST_FAILED_TO_INITIALIZE_GLFW;
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    float monitor_scale_x = 1.0f;
    float monitor_scale_y = 1.0f; // Scale the initial window size according to system DPI scaling
    glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &monitor_scale_x, &monitor_scale_y);
    GLFWwindow *window = glfwCreateWindow((int)(TEST_GUI_DEFAULT_WINDOW_WIDTH * monitor_scale_x), (int)(TEST_GUI_DEFAULT_WINDOW_HEIGHT * monitor_scale_y), window_title, NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        free((void *)about_section_version_string);
        free((void *)window_title);
        return TEST_FAILED_TO_CREATE_WINDOW;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free((void *)about_section_version_string);
        free((void *)window_title);
        return TEST_FAILED_TO_INITIALIZE_GLEW;
    }
#ifdef _DEBUG
    status = _GuiSetWindowIcon(window, "icon_debug.png");
#else
    status = _GuiSetWindowIcon(window, "icon.png");
#endif
    if (!TEST_SUCCESS(status)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        free((void *)about_section_version_string);
        free((void *)window_title);
        return status;
    }

    NFD_Init();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        free((void *)about_section_version_string);
        free((void *)window_title);
        NFD_Quit();
        return TEST_FAILED_TO_INITIALIZE_IMGUI;
    }
    if (!ImGui_ImplOpenGL3_Init()) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        free((void *)about_section_version_string);
        free((void *)window_title);
        NFD_Quit();
        return TEST_FAILED_TO_INITIALIZE_IMGUI;
    }
    glGenTextures(1, &gui_font_atlas_texture);
    _GuiInitializeStyle();

    bool render_gui = true;
    gui_panel *selected_panel = NULL;
    float scaling_x = 0.0f;
    float scaling_y = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        int width = 0;
        int height = 0;
        bool scaling_changed = false;
        float prev_scaling_x = scaling_x;
        float prev_scaling_y = scaling_y;
        glfwGetWindowContentScale(window, &scaling_x, &scaling_y);
        if (scaling_x != prev_scaling_x || scaling_y != prev_scaling_y) {
            scaling_changed = true;
        }
        if (scaling_changed) {
            _GuiScaleAndApplyStyle(io, scaling_y);
        }
        if (render_gui) {
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);

            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }
        
        if (gui_current_benchmark != NULL) {
            volatile test_status run_status = gui_current_benchmark->completion_code;
            if (run_status != TEST_ASYNC_PROCESS_RUNNING) {
                const char *result_buffer = NULL;
                gui_cached_benchmark_result_string result_string;
                memset(&result_string, 0, sizeof(result_string));
                size_t result_count = HelperArrayListSize(&(gui_current_benchmark->raw_results));

                if (result_count > 0) {
                    for (size_t i = 0; i < result_count; i++) {
                        process_runner_keypair *keypair = (process_runner_keypair *)HelperArrayListGet(&(gui_current_benchmark->raw_results), i);
                        _GuiConvertResult(keypair, &result_buffer, sizeof(result_buffer), gui_current_benchmark->result_type);
                        result_string.string = result_buffer;
                        HelperArrayListAdd(&(gui_current_benchmark->results), &result_string, sizeof(result_string), NULL);
                    }
                } else if (run_status == TEST_VK_FEATURE_UNSUPPORTED) {
                    _GuiConvertResult(NULL, &result_buffer, sizeof(result_buffer), gui_result_type_unsupported);
                    result_string.string = result_buffer;
                    HelperArrayListAdd(&(gui_current_benchmark->results), &result_string, sizeof(result_string), NULL);
                } else if (run_status == TEST_PROCESS_KILLED) {
                    _GuiConvertResult(NULL, &result_buffer, sizeof(result_buffer), gui_result_type_cancelled);
                    result_string.string = result_buffer;
                    HelperArrayListAdd(&(gui_current_benchmark->results), &result_string, sizeof(result_string), NULL);
                } else {
                    _GuiConvertResult(NULL, &result_buffer, sizeof(result_buffer), gui_result_type_error);
                    result_string.string = result_buffer;
                    HelperArrayListAdd(&(gui_current_benchmark->results), &result_string, sizeof(result_string), NULL);
                }
                gui_current_benchmark->has_been_run = true;
                free((void *)gui_current_benchmark->process_handle);
                gui_current_benchmark = _GuiDequeueBenchmark();
            }
        } else {
            gui_current_benchmark = _GuiDequeueBenchmark();
        }
        gui_benchmark_running = gui_current_benchmark != NULL;

        if (render_gui) {
            ImGuiStyle &style = ImGui::GetStyle();
            const float sidebar_width = 270 * scaling_x;
            const float info_corner_height = 300 * scaling_y;
            const float queue_button_width = 65 * scaling_x;
            const float tooltip_width = 250 * scaling_x;

            ImGui::SetNextWindowPos(ImVec2(0, (float)height - info_corner_height));
            ImGui::SetNextWindowSize(ImVec2(sidebar_width, info_corner_height));
            ImGui::Begin(_GuiTranslateImGuiString(&gui_string_title_controls), NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoResize);
            ImGui::Text(GuiLocalizationTranslate(&gui_string_controls_current));
            ImGui::Text(gui_current_benchmark != NULL ? gui_current_benchmark->test_name : GuiLocalizationTranslate(&gui_string_controls_none));
            if (gui_current_benchmark != NULL) {
                ImGui::SameLine(sidebar_width - style.ItemSpacing.x - queue_button_width - style.ScrollbarSize);
                if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_controls_cancel), ImVec2(queue_button_width, 0))) {
                    ProcessRunnerTerminateAsync(gui_current_benchmark->process_handle);
                }
            }
            ImGui::BeginListBox(_GuiTranslateImGuiString(&gui_string_controls_list), ImVec2(-1, ImGui::GetFrameHeight() - 3 * ImGui::GetTextLineHeightWithSpacing()));
            void *iterator = NULL;
            for (size_t queue_index = 0; queue_index < HelperLinkedListSize(&gui_queue); queue_index++) {
                gui_queued_benchmark *benchmark = (gui_queued_benchmark *)HelperLinkedListGet(&gui_queue, queue_index);

                ImGui::Text(benchmark->benchmark->test_name);
                ImGui::SameLine(sidebar_width - style.ItemSpacing.x - 2 * style.ItemInnerSpacing.x - queue_button_width - style.ScrollbarSize);
                if (ImGui::Button(_GuiTranslateImGuiStringTestSuffix(&(benchmark->cancel_button), "QueueList", benchmark->sequential_queue_id, 0), ImVec2(queue_button_width, 0))) {
                    HelperLinkedListRemove(&gui_queue, queue_index);
                    queue_index--;

                    if (benchmark->cancel_button.imgui_string.imgui_identifier != NULL) {
                        free((void *)benchmark->cancel_button.imgui_string.imgui_identifier);
                    }
                    if (benchmark->cancel_button.imgui_string.string_buffer != NULL) {
                        free((void *)benchmark->cancel_button.imgui_string.string_buffer);
                    }
                    free((void *)benchmark);
                }
            }
            float button_width = (ImGui::GetWindowWidth() - 2 * (style.ItemSpacing.x)) / 3;
            ImGui::EndListBox();
            if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_controls_console), ImVec2(button_width, 0))) {
                MainToggleConsoleWindow();
            }
            ImGui::SameLine();
            if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_controls_about), ImVec2(button_width, 0))) {
                gui_about = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_controls_export), ImVec2(button_width, 0))) {
                gui_exporting = true;
            }
            ImGui::End();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(sidebar_width, (float)height - info_corner_height));
            ImGui::Begin(_GuiTranslateImGuiString(&gui_string_title_benchmarks), NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoResize);

            ImGui::BeginListBox(_GuiTranslateImGuiString(&gui_string_benchmarks_list), ImVec2(-1, -1));
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));

            for (size_t i = 0; i < panel_count; i++) {
                gui_panel *panel = (gui_panel *)HelperArrayListGet(benchmark_panels, i);
                
                if (ImGui::Selectable(_GuiTranslateImGuiString(&(panel->display_name_benchmarklist)), &(panel->selected_in_ui), 0, ImVec2(0, 2.5f * ImGui::GetTextLineHeightWithSpacing()))) {
                    for (size_t j = 0; j < panel_count; j++) {
                        if (i == j) {
                            selected_panel = panel->selected_in_ui ? panel : NULL;
                        } else {
                            gui_panel *other_panel = (gui_panel *)HelperArrayListGet(benchmark_panels, j);
                            other_panel->selected_in_ui = false;
                        }
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetNextWindowSize(ImVec2(tooltip_width, 0.0f));
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::Text(GuiLocalizationTranslate(&(panel->panel_tooltip_description)));
                    ImGui::EndTooltip();
                }
            }
            ImGui::PopStyleVar();
            ImGui::EndListBox();
            ImGui::End();
            ImGui::SetNextWindowPos(ImVec2(sidebar_width, 0));
            ImGui::SetNextWindowSize(ImVec2((float)width - sidebar_width, (float)(height)));
            if (selected_panel != NULL) {
                ImGui::Begin(_GuiTranslateImGuiString(&(selected_panel->display_name_mainpanel)), NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize);
                size_t section_count = HelperArrayListSize(&(selected_panel->sections));
                bool tab_selected;
                if (section_count > 1) {
                    if (ImGui::BeginTabBar("##SectionTabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
                        for (size_t i = 0; i < section_count; i++) {
                            gui_section *section = (gui_section *)HelperArrayListGet(&(selected_panel->sections), i);
                            tab_selected = ImGui::BeginTabItem(_GuiTranslateImGuiString(&(section->display_name_sectiontab)), NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton);
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetNextWindowSize(ImVec2(tooltip_width, 0.0f));
                                ImGui::BeginTooltip();
                                ImGui::PushTextWrapPos(0.0f);
                                ImGui::Text(GuiLocalizationTranslate(&(section->section_tooltip_description)));
                                ImGui::EndTooltip();
                            }
                            if (tab_selected) {
                                _GuiRenderSection((uint32_t)gpu_count, section);
                                ImGui::EndTabItem();
                            }
                        }
                        ImGui::EndTabBar();
                    }
                } else {
                    gui_section *section = (gui_section *)HelperArrayListGet(&(selected_panel->sections), 0);
                    _GuiRenderSection((uint32_t)gpu_count, section);
                }
            } else {
                ImGui::Begin(_GuiTranslateImGuiString(&gui_string_main_none), NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize);
            }
            ImGui::End();

            if (gui_exporting) {
                const float window_width = 400 * scaling_x;
                const float window_height = height * 0.8f;
                const float button_width = 80 * scaling_x;

                ImGui::SetNextWindowPos(ImVec2((width - window_width) / 2, (height - window_height) / 2));
                ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
                ImGui::Begin(_GuiTranslateImGuiString(&gui_string_title_export), &gui_exporting, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoResize);
                ImGui::SetWindowFocus();
                ImGui::Text(GuiLocalizationTranslate(&gui_string_export_instruct));
                ImGui::BeginListBox("##BenchmarkSelectionListbox", ImVec2(-1, ImGui::GetFrameHeight() - 3 * ImGui::GetTextLineHeightWithSpacing()));
                for (size_t i = 0; i < panel_count; i++) {
                    gui_panel *panel = (gui_panel *)HelperArrayListGet(benchmark_panels, i);

                    ImGui::Checkbox(_GuiTranslateImGuiString(&(panel->display_name_exportcheckbox)), &(panel->selected_for_csv));
                }
                ImGui::EndListBox();
                if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_export_selectall), ImVec2(button_width, 0))) {
                    for (size_t i = 0; i < panel_count; i++) {
                        gui_panel *panel = (gui_panel *)HelperArrayListGet(benchmark_panels, i);
                        panel->selected_for_csv = true;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_export_clearall), ImVec2(button_width, 0))) {
                    for (size_t i = 0; i < panel_count; i++) {
                        gui_panel *panel = (gui_panel *)HelperArrayListGet(benchmark_panels, i);
                        panel->selected_for_csv = false;
                    }
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 2 * (button_width + style.ItemSpacing.x));
                if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_export_cancel), ImVec2(button_width, 0))) {
                    gui_exporting = false;
                }
                ImGui::SameLine();
                if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_export_export), ImVec2(button_width, 0))) {
                    gui_exporting = false;
                    nfdchar_t *filename;
                    const nfdfilteritem_t filter_item[1] = {{"CSV File", "csv"}};
                    nfdresult_t dialog_result = NFD_SaveDialog(&filename, filter_item, 1, NULL, "results.csv");

                    if (dialog_result == NFD_OKAY) {
                        INFO("Exporting CSV file \"%s\"\n", filename);
                        void *file = HelperOpenFileForWriting(filename);
                        if (file != NULL) {
                            for (size_t p = 0; p < panel_count; p++) {
                                gui_panel *panel = (gui_panel *)HelperArrayListGet(benchmark_panels, p);

                                if (panel->selected_for_csv) {
                                    size_t section_count = HelperArrayListSize(&(panel->sections));
                                    HelperWriteFile(file, "%s,\n", GuiLocalizationTranslate(&(panel->display_name_mainpanel.localized_string)));

                                    for (size_t s = 0; s < section_count; s++) {
                                        gui_section *section = (gui_section *)HelperArrayListGet(&(panel->sections), s);

                                        if (section->benchmark_type == gui_benchmarks_type_single_result) {
                                            size_t test_count = HelperArrayListSize(&(section->benchmarks[0]));
                                            HelperWriteFile(file, "%s,\n", GuiLocalizationTranslate(&(section->display_name_mainsection.localized_string)));
                                            HelperWriteFile(file, "GPU,");

                                            for (size_t i = 0; i < test_count; i++) {
                                                gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[0]), i);
                                                HelperWriteFile(file, "%s,", GuiLocalizationTranslate(&(benchmark->display_name)));
                                            }
                                            HelperWriteFile(file, "\n");
                                            for (size_t i = 0; i < gpu_count; i++) {
                                                gui_gpu *gpu = (gui_gpu *)HelperArrayListGet(&gui_gpus, i);
                                                HelperWriteFile(file, "%s,", gpu->display_name);

                                                for (size_t j = 0; j < test_count; j++) {
                                                    gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[i]), j);
                                                    process_runner_keypair *result = NULL;
                                                    if (benchmark->has_been_run) {
                                                        result = (process_runner_keypair *)HelperArrayListGet(&(benchmark->raw_results), 0);
                                                    }
                                                    HelperWriteFile(file, "%s,", (result == NULL) ? "" : ((strcmp(result->key, "") == 0) ? "N/A" : result->result));
                                                }
                                                HelperWriteFile(file, "\n");
                                            }
                                            HelperWriteFile(file, "\n");
                                        } else {
                                            size_t max_result_count = 0;
                                            for (uint32_t i = 0; i < gpu_count; i++) {
                                                gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[i]), 0);
                                                if (benchmark->has_been_run) {
                                                    size_t count = HelperArrayListSize(&(benchmark->raw_results));
                                                    if (count > max_result_count) {
                                                        max_result_count = count;
                                                    }
                                                }
                                            }
                                            HelperWriteFile(file, "GPU,");
                                            for (size_t i = 0; i < max_result_count; i++) {
                                                const char *label = (const char *)HelperArrayListGet(&(section->labels), i);
                                                HelperWriteFile(file, "%s,", label);
                                            }
                                            HelperWriteFile(file, "\n");
                                            for (uint32_t i = 0; i < gpu_count; i++) {
                                                gui_gpu *gpu = (gui_gpu *)HelperArrayListGet(&gui_gpus, i);
                                                HelperWriteFile(file, "%s,", gpu->display_name);
                                                gui_benchmark *benchmark = (gui_benchmark *)HelperArrayListGet(&(section->benchmarks[i]), 0);

                                                for (size_t j = 0; j < max_result_count; j++) {
                                                    process_runner_keypair *result = NULL;
                                                    if (benchmark->has_been_run) {
                                                        result = (process_runner_keypair *)HelperArrayListGet(&(benchmark->raw_results), j);
                                                    }
                                                    HelperWriteFile(file, "%s,", (result == NULL) ? "" : ((strcmp(result->key, "") == 0) ? "N/A" : result->result));
                                                }
                                                HelperWriteFile(file, "\n");
                                            }
                                            HelperWriteFile(file, "\n");
                                        }
                                    }
                                }
                            }
                            HelperCloseFile(file);
                        } else {
                            WARNING("Failed to create file \"%s\"\n", filename);
                        }
                        NFD_FreePath(filename);
                    }
                }
                ImGui::End();
            }
            if (gui_about) {
                const float window_width = 700 * scaling_x;
                const float window_height = height * 0.8f;
                ImGui::SetNextWindowPos(ImVec2((width - window_width) / 2, (height - window_height) / 2));
                ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
                ImGui::Begin(_GuiTranslateImGuiString(&gui_string_title_about), &gui_about, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoResize);
                ImGui::SetWindowFocus();

                if (ImGui::BeginTabBar("##AboutTabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
                    if (ImGui::BeginTabItem(_GuiTranslateImGuiString(&gui_string_about_about), NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton)) {
                        GuiFormatterRenderString(gui_formatted_text_about, gui_font_regular, gui_font_bold, gui_font_italics, gui_font_monospace, gui_font_heading);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem(_GuiTranslateImGuiString(&gui_string_about_changelog), NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton)) {
                        GuiFormatterRenderString(gui_formatted_text_changelog, gui_font_regular, gui_font_bold, gui_font_italics, gui_font_monospace, gui_font_heading);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem(_GuiTranslateImGuiString(&gui_string_about_build), NULL, ImGuiTabItemFlags_NoCloseWithMiddleMouseButton)) {
                        ImGui::PushFont(gui_font_heading);
                        ImGui::Text(GuiLocalizationTranslate(&gui_string_about_buildinfo_title));
                        ImGui::PopFont();
                        ImGui::Separator();
                        ImGui::Text(GuiLocalizationTranslate(&gui_string_about_buildinfo_version));
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - style.WindowPadding.x - ImGui::CalcTextSize(about_section_version_string).x);
                        ImGui::Text(about_section_version_string);
                        ImGui::Text(GuiLocalizationTranslate(&gui_string_about_buildinfo_type));
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - style.WindowPadding.x - ImGui::CalcTextSize(BUILD_INFO_TYPE_STRING).x);
                        ImGui::Text(BUILD_INFO_TYPE_STRING);
                        ImGui::Text(GuiLocalizationTranslate(&gui_string_about_buildinfo_configuration));
#ifdef _DEBUG
                        const char *configuration_name = GuiLocalizationTranslate(&gui_string_about_buildinfo_configuration_debug);
#else
                        const char *configuration_name = GuiLocalizationTranslate(&gui_string_about_buildinfo_configuration_release);
#endif
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - style.WindowPadding.x - ImGui::CalcTextSize(configuration_name).x);
                        ImGui::Text(configuration_name);
                        ImGui::Text(GuiLocalizationTranslate(&gui_string_about_buildinfo_source));
#ifdef BUILD_INFO_SHA
                        const char *build_source_string = BUILD_INFO_SHA;
                        const char *build_branch_string = BUILD_INFO_BRANCH;
#else
                        const char *build_source_string = GuiLocalizationTranslate(&gui_string_about_buildinfo_source_local);
                        const char *build_branch_string = GuiLocalizationTranslate(&gui_string_about_buildinfo_branch_local);
#endif
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - style.WindowPadding.x - ImGui::CalcTextSize(build_source_string).x);
                        ImGui::Text(build_source_string);
                        ImGui::Text(GuiLocalizationTranslate(&gui_string_about_buildinfo_branch));
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - style.WindowPadding.x - ImGui::CalcTextSize(build_branch_string).x);
                        ImGui::Text(build_branch_string);
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                // Move the close button to the bottom of the window if we don't have a scrollbar yet
                float button_height = style.ItemSpacing.y + ImGui::GetTextLineHeightWithSpacing();
                if (ImGui::GetCursorPosY() + button_height < ImGui::GetWindowHeight() - 2 * style.WindowPadding.y) {
                    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 2 * style.WindowPadding.y - button_height);
                }
                if (ImGui::Button(_GuiTranslateImGuiString(&gui_string_about_close), ImVec2(-1, 0))) {
                    gui_about = false;
                }
                ImGui::End();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }
        glfwWaitEventsTimeout(1.0f / (float)GUI_MINIMUM_REFRESHRATE);
        render_gui = !glfwGetWindowAttrib(window, GLFW_ICONIFIED);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glDeleteTextures(1, &gui_font_atlas_texture);
    NFD_Quit();
    glfwDestroyWindow(window);
    glfwTerminate();

    // Terminate the currently running benchmark if there is one
    if (gui_current_benchmark != NULL) {
        ProcessRunnerTerminateAsync(gui_current_benchmark->process_handle);
        free((void *)gui_current_benchmark->process_handle);
    }
    free((void *)about_section_version_string);
    free((void *)window_title);
    return status;
}

// Modified "CorporateGrey" Theme by malamanteau (https://github.com/ocornut/imgui/issues/707#issuecomment-468798935)
//#define IMGUI_STYLE_3D
static void _GuiInitializeStyle() {
    ImVec4 *colors = gui_default_style.Colors;

    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.12f, 0.12f, 0.12f, 0.71f);
    colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.42f, 0.42f, 0.42f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.42f, 0.42f, 0.42f, 0.40f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.67f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.17f, 0.17f, 0.17f, 0.90f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.335f, 0.335f, 0.335f, 1.000f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.24f, 0.24f, 0.24f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.54f, 0.54f, 0.54f, 0.35f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.52f, 0.52f, 0.52f, 0.59f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.76f, 0.76f, 0.76f, 0.77f);
    colors[ImGuiCol_Separator] = ImVec4(0.000f, 0.000f, 0.000f, 0.137f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.700f, 0.671f, 0.600f, 0.290f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.702f, 0.671f, 0.600f, 0.674f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.54f, 0.54f, 0.54f, 0.35f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.52f, 0.52f, 0.52f, 0.59f);
    colors[ImGuiCol_TabActive] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.24f, 0.24f, 0.24f, 0.53f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.12f, 0.12f, 0.12f, 0.71f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.17f, 0.17f, 0.17f, 0.90f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.73f, 0.73f, 0.73f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);

    gui_default_style.PopupRounding = 3;

    gui_default_style.WindowPadding = ImVec2(4, 4);
    gui_default_style.FramePadding = ImVec2(6, 4);
    gui_default_style.ItemSpacing = ImVec2(6, 2);

    gui_default_style.ScrollbarSize = 18;

    gui_default_style.WindowBorderSize = 1;
    gui_default_style.ChildBorderSize = 1;
    gui_default_style.PopupBorderSize = 1;
#ifdef IMGUI_STYLE_3D
    gui_default_style.FrameBorderSize = 1;
#else
    gui_default_style.FrameBorderSize = 0;
#endif

    gui_default_style.WindowRounding = 3;
    gui_default_style.ChildRounding = 3;
    gui_default_style.FrameRounding = 3;
    gui_default_style.ScrollbarRounding = 2;
    gui_default_style.GrabRounding = 3;

#ifdef IMGUI_HAS_DOCK 
#ifdef IMGUI_STYLE_3D
    gui_default_style.TabBorderSize = 1;
#else
    gui_default_style.TabBorderSize = 0;
#endif
    gui_default_style.TabRounding = 3;

    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.85f, 0.85f, 0.85f, 0.28f);

    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        gui_default_style.WindowRounding = 0.0f;
        gui_default_style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
#endif
#ifdef _DEBUG
    colors[ImGuiCol_WindowBg] = ImVec4(0.89f, 0.19f, 0.88f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.56f, 0.00f, 0.59f, 1.00f);
#endif
}

// Since a lot of ImGui texts/titles/labels/etc "need" (to avoid same-text collisions) a ##suffix, this is a helper function
// that takes a translated string and appends the suffix to it, also caching it
static const char *_GuiTranslateImGuiStringUpdate(gui_translated_imgui_string *string, bool force_update) {
    if (string == nullptr) {
        return nullptr;
    }
    const char *translated_string = GuiLocalizationTranslate(&(string->localized_string));
    // Check if we got the same string from the localizer, if not, update our cache
    if (force_update || string->localized_cache == nullptr || string->localized_cache != translated_string) {
        string->localized_cache = translated_string;
        if (string->string_buffer != nullptr) {
            free((void *)string->string_buffer);
            string->string_buffer = nullptr;
        }
        // Take the translated string and print the ImGui identifier behind it
        test_status status = HelperPrintToBuffer(&(string->string_buffer), nullptr, "%s##%s", translated_string, string->imgui_identifier);
        if (!TEST_SUCCESS(status)) {
            WARNING("Failed to translate ImGui string \"%s\"\n", string->localized_string.key);
            return nullptr;
        }
    }
    return string->string_buffer;
}

static const char *_GuiTranslateImGuiString(gui_translated_imgui_string *string) {
    return _GuiTranslateImGuiStringUpdate(string, false);
}

static const char *_GuiTranslateImGuiStringTestSuffix(gui_translated_benchmark_result_string *string, const char *test_name, uint32_t result_index, uint32_t gpu_index) {
    if (string == nullptr || test_name == nullptr) {
        return nullptr;
    }
    bool force_update = false;
    if (string->cached_gpu_index != gpu_index || string->cached_result_index != result_index || string->cached_test_name == nullptr || strcmp(string->cached_test_name, test_name) != 0) {
        if (string->imgui_string.imgui_identifier != nullptr) {
            free((void *)string->imgui_string.imgui_identifier);
            string->imgui_string.imgui_identifier = nullptr;
        }
        test_status status = HelperPrintToBuffer(&(string->imgui_string.imgui_identifier), nullptr, "%s-%lu-%lu", test_name, result_index, gpu_index);
        if (!TEST_SUCCESS(status)) {
            WARNING("Failed to translate ImGui test string \"%s\"\n", string->imgui_string.localized_string.key);
            return nullptr;
        }
        force_update = true;
        string->cached_gpu_index = gpu_index;
        string->cached_result_index = result_index;
        string->cached_test_name = test_name;
    }
    return _GuiTranslateImGuiStringUpdate(&(string->imgui_string), force_update);
}

static const char *_GuiGetStringWithTestSuffix(gui_cached_benchmark_result_string *string, const char *test_name, uint32_t result_index, uint32_t gpu_index) {
    if (string == nullptr || string->string == nullptr || test_name == nullptr) {
        return nullptr;
    }
    if (string->cached_gpu_index != gpu_index || string->cached_result_index != result_index || string->cached_test_name == nullptr || strcmp(string->cached_test_name, test_name) != 0) {
        if (string->string_with_suffix != nullptr) {
            free((void *)string->string_with_suffix);
            string->string_with_suffix = nullptr;
        }
        test_status status = HelperPrintToBuffer(&(string->string_with_suffix), nullptr, "%s##%s-%lu-%lu", string->string, test_name, result_index, gpu_index);
        if (!TEST_SUCCESS(status)) {
            WARNING("Failed to translate ImGui test string \"%s\"\n", string->string);
            return nullptr;
        }
        string->cached_gpu_index = gpu_index;
        string->cached_result_index = result_index;
        string->cached_test_name = test_name;
    }
    return string->string_with_suffix;
}