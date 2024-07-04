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
#ifdef _WIN32
#include <Windows.h>
#include <commdlg.h>
#endif
#include "logger.h"
#include "helper.h"
#include "runner.h"
#include "gui/gui.h"
#include "build_info.h"

static test_result_output result_format;
static test_ui_mode ui_mode;
static const char *binary_path;
static bool console_visible;

int main(int argc, const char **argv) {
    SEPARATOR();
    LOG("MAINV", "GPUPerfTest Backend %u.%u.%u.%s%s\n", TEST_VER_MAJOR(TEST_TOOL_VERSION), TEST_VER_MINOR(TEST_TOOL_VERSION), TEST_VER_PATCH(TEST_TOOL_VERSION), BUILD_INFO_IDENTIFIER, BUILD_INFO_TYPE_SUFFIX);
    binary_path = argv[0];
    console_visible = true;

    int32_t gpu_identifier = -1;
    const char *test_identifier = NULL;
    bool print_help = false;
    result_format = test_result_readable;
    ui_mode = test_ui_mode_gui;
    const char *current_key = NULL;
    for (int i = 1; i < argc; i++) {
        if (current_key == NULL) {
            current_key = argv[i];

            if (strcmp(current_key, "--help") == 0 || strcmp(current_key, "-h") == 0) {
                print_help = true;
                current_key = NULL;
            } else if (strcmp(current_key, "--csv") == 0 || strcmp(current_key, "-s") == 0) {
                result_format = test_result_csv;
                current_key = NULL;
            } else if (strcmp(current_key, "--raw") == 0 || strcmp(current_key, "-r") == 0) {
                result_format = test_result_raw;
                current_key = NULL;
            } else if (strcmp(current_key, "--cli") == 0 || strcmp(current_key, "-c") == 0) {
                ui_mode = test_ui_mode_cli;
                current_key = NULL;
            }
        } else {
            const char *current_value = argv[i];
            if (strcmp(current_key, "--device") == 0 || strcmp(current_key, "-d") == 0) {
                gpu_identifier = strtol(current_value, NULL, 10);
            } else if (strcmp(current_key, "--test") == 0 || strcmp(current_key, "-t") == 0) {
                test_identifier = current_value;
            } else if (strcmp(current_key, "--mode") == 0 || strcmp(current_key, "-m") == 0) {
                if (strcmp(current_value, "cli") == 0) {
                    ui_mode = test_ui_mode_cli;
                } else {
                    ui_mode = test_ui_mode_gui;
                }
            }
            current_key = NULL;
        }
    }
    if (ui_mode != test_ui_mode_cli) {
        if (ui_mode == test_ui_mode_gui) {
#ifndef _DEBUG
#ifdef _WIN32
            ShowWindow(GetConsoleWindow(), SW_HIDE);
#endif
            console_visible = false;
#endif
            GuiRun();
#ifndef _DEBUG
#ifdef _WIN32
            ShowWindow(GetConsoleWindow(), SW_SHOW);
#endif
            console_visible = true;
#endif
        }
    } else {
        test_status status = RunnerRegisterTests();
        if (!TEST_SUCCESS(status)) {
            ABORT(status);
            return 1;
        }
        if (print_help) {
            SEPARATOR();
            INFO("ARGUMENTS:\n");
            INFO("    --help/-h: Prints this\n");
            INFO("    --mode/-m <mode>: Specify how to run the application (gui, cli). Default: gui\n");
            INFO("    --cli/-c: Shorthand for '--mode cli'\n");
            INFO("    --device/-d <device id>: Specifies which device to run tests on. Default: -1\n");
            INFO("    --test/-t <test id>: Specifies which test to run. Required\n");
            INFO("    --csv/-s: Print final results in CSV format. Optional\n");
            INFO("    --raw/-r: Print final results in raw format. Optional\n");
            INFO("TESTS:\n");
            RunnerPrintTests();
            SEPARATOR();
        } else {
            if (test_identifier != NULL) {
                status = RunnerExecuteTests(test_identifier, gpu_identifier);
                if (!TEST_SUCCESS(status)) {
                    ABORT(status);
                    SEPARATOR();
                    return 1;
                }
            } else {
                ABORT(TEST_NO_TEST_SPECIFIED);
                SEPARATOR();
                return 1;
            }
            SEPARATOR();
        }
        RunnerCleanUp();
    }
    return 0;
}

test_result_output MainGetTestResultFormat() {
    return result_format;
}

const char *MainGetBinaryPath() {
    return binary_path;
}

test_ui_mode MainGetTestUIMode() {
    return ui_mode;
}

void MainToggleConsoleWindow() {
#ifdef _WIN32
    ShowWindow(GetConsoleWindow(), console_visible ? SW_HIDE : SW_SHOW);
#endif
    console_visible = !console_visible;
}