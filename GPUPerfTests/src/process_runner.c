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
#include "logger.h"
#include "helper.h"
#include "process_runner.h"
#include "sanitize_windows_h.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <poll.h>
#include <unistd.h>
#endif

#define PROCESS_RUNNER_ALIVE_CHECKER_INTERVAL_MS    (100)
#define PROCESS_RUNNER_STDOUT_BUFFER_SIZE           (4096)

typedef struct process_runner_thread_data_t {
    const char *test_name;
    uint32_t device_index;
    helper_arraylist *results;
    test_status *completion_code;
    bool has_kill_handle;
    bool kill_process;
} process_runner_thread_data;

static void ProcessRunnerAsyncThreadFunction(uint32_t thread_id, void *input_data);

#ifdef _WIN32
static const char *_ProcessRunnerReadLine(char *buffer, int max_count, HANDLE file, size_t *context) {
#else
static const char *_ProcessRunnerReadLine(char *buffer, int max_count, int file, size_t *context) {
#endif
    if (max_count <= 1 || context == NULL) {
        return NULL;
    }
    size_t previous_line_end = *context;
    while (true) {
        char *current_line = buffer + previous_line_end;
        size_t char_index = (size_t)(current_line - buffer);
        char prev_char = '\0';

        while (char_index < max_count - 1) {
            char c = buffer[char_index];
            if (c == '\n') {
                buffer[char_index] = '\0';
                if (prev_char == '\r' && char_index > 0) {
                    buffer[char_index - 1] = '\0';
                }
                *context = char_index + 1;
                return current_line;
            } else if (c == '\0') {
                // We either don't have enough data
                break;
            }
            prev_char = c;
            char_index++;
        }
        // If we got here for any reason, we need to load more data
        if (previous_line_end == 0 && char_index == max_count - 1) {
            // This means we went through the whole buffer without completing a single line, just abort
            return NULL;
        }
        // Copy any existing good data to the start of the buffer
        size_t j = 0;
        for (size_t i = previous_line_end; i < char_index; i++, j++) {
            buffer[j] = buffer[i];
        }
        buffer[j] = '\0'; // Just in case
        char_index -= previous_line_end; // Offset this properly
        previous_line_end = 0; // And reset this to the start of the buffer
        size_t max_read = ((size_t)max_count - 1 - char_index);
#ifdef _WIN32
        DWORD read_data = 0;
        BOOL success = ReadFile(file, buffer + char_index, (DWORD)max_read, &read_data, NULL);
        if (success == FALSE || read_data == 0) {
            return NULL;
        }
#else
        struct pollfd poller = {file, POLLIN};
        int rval = poll(&poller, 1, -1);
        size_t read_data = 0;
        if ((poller.revents & POLLIN) != 0) {
            read_data = read(file, buffer + char_index, max_read);
        } else {
            return NULL;
        }
#endif
        buffer[char_index + read_data] = '\0';
        // We loaded data, now let's repeat the loop and try again
    }
}

test_status ProcessRunnerRunTestKillable(const char *test_name, uint32_t device_index, helper_arraylist *results, bool *kill_process) {
    if (test_name == NULL || results == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    const char *command = NULL;
#ifdef _WIN32
    test_status status = HelperPrintToBuffer(&command, NULL, "\"%s\" --cli --raw --test %s --device %lu", MainGetBinaryPath(), test_name, device_index);
#else
    // In Linux we just use this to get a stringified device_index as the args have to be separate strings
    test_status status = HelperPrintToBuffer(&command, NULL, "%lu", device_index);
#endif
    if (command == NULL) {
        return status;
    }
    char *line_buffer = malloc(PROCESS_RUNNER_STDOUT_BUFFER_SIZE * sizeof(char));
    if (line_buffer == NULL) {
        free((void *)command);
        return TEST_OUT_OF_MEMORY;
    }
    memset(line_buffer, 0, PROCESS_RUNNER_STDOUT_BUFFER_SIZE * sizeof(char));

#ifdef _WIN32
    SECURITY_ATTRIBUTES security_attributes;
    security_attributes.nLength = sizeof(security_attributes);
    security_attributes.bInheritHandle = TRUE;
    security_attributes.lpSecurityDescriptor = NULL;

    HANDLE stdout_read;
    HANDLE stdout_write;
    if (!CreatePipe(&stdout_read, &stdout_write, &security_attributes, 0)) {
        free((void *)command);
        free(line_buffer);
        return TEST_FAILED_TO_SPAWN_TEST_PROCESS;
    }
    if (!SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        free((void *)command);
        free(line_buffer);
        return TEST_FAILED_TO_SPAWN_TEST_PROCESS;
    }
    STARTUPINFOA startup_info;
    memset(&startup_info, 0, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.hStdError = stdout_write;
    startup_info.hStdOutput = stdout_write;
    startup_info.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    if (!CreateProcessA(NULL, (char *)command, NULL, NULL, TRUE, 0, NULL, NULL, &startup_info, &process_info)) {
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        CloseHandle(stdout_write);
        CloseHandle(stdout_read);
        free((void *)command);
        free(line_buffer);
        return TEST_FAILED_TO_SPAWN_TEST_PROCESS;
    }
    // Close the write handle, we are only reading
    CloseHandle(stdout_write);
#else
    int cout_pipe[2];
    int cerr_pipe[2];

    if (pipe(cout_pipe) || pipe(cerr_pipe)) {
        free((void *)command);
        free(line_buffer);
        return TEST_FAILED_TO_SPAWN_TEST_PROCESS;
    }
    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    posix_spawn_file_actions_addclose(&file_actions, cout_pipe[0]);
    posix_spawn_file_actions_addclose(&file_actions, cerr_pipe[0]);
    posix_spawn_file_actions_adddup2(&file_actions, cout_pipe[1], 1);
    posix_spawn_file_actions_adddup2(&file_actions, cout_pipe[1], 2);
    posix_spawn_file_actions_addclose(&file_actions, cout_pipe[1]);
    posix_spawn_file_actions_addclose(&file_actions, cerr_pipe[1]);

    pid_t pid;
    const char *argv[] = {MainGetBinaryPath(), "--cli", "--raw", "--test", test_name, "--device", command, NULL};
    char *env[] = {NULL};
    int spawn_status = posix_spawn(&pid, MainGetBinaryPath(), &file_actions, NULL, (char **)argv, env);
    if (spawn_status != 0) {
        posix_spawn_file_actions_destroy(&file_actions);
        free((void *)command);
        free(line_buffer);
        return TEST_FAILED_TO_SPAWN_TEST_PROCESS;
    }
    close(cout_pipe[1]);
    close(cerr_pipe[1]);
#endif

    bool process_terminated = false;
    size_t reader_context = 0;
    const char *line = NULL;
#ifdef _WIN32
    while ((line = _ProcessRunnerReadLine(line_buffer, PROCESS_RUNNER_STDOUT_BUFFER_SIZE, stdout_read, &reader_context)) != NULL) {
#else
    while ((line = _ProcessRunnerReadLine(line_buffer, PROCESS_RUNNER_STDOUT_BUFFER_SIZE, cout_pipe[0], &reader_context)) != NULL) {
#endif
        if (TEST_SUCCESS(status)) {
            if (strstr(line, "[RESLT] ") == line) {
                const char *key_start = strstr(line + 8, ": ");
                if (key_start != NULL) {
                    key_start += 2;
                    const char *key_end = strstr(key_start, " = ");
                    if (key_end != NULL) {
                        const char *value_start = key_end + 3;
                        process_runner_keypair result;
                        memset(&result, 0, sizeof(result));
                        strncpy(result.key, key_start, min(key_end - key_start, PROCESS_RUNNER_MAXIMUM_RESULT_LENGTH));
                        strncpy(result.result, value_start, PROCESS_RUNNER_MAXIMUM_RESULT_LENGTH);
                        size_t result_len = strlen(result.result);
                        if (result_len > 1 && result.result[result_len - 1] == '\n') {
                            result.result[result_len - 1] = '\0';
                        }
                        status = HelperArrayListAdd(results, &result, sizeof(result), NULL);
                    }
                }
            } else if (strstr(line, "[ABORT] ") == line) {
                if (strlen(line) >= 20) {
                    status = (test_status)strtoull((const char *)line + 10, NULL, 16);
                }
            }
        }
        if (!process_terminated && kill_process != NULL && *kill_process == true) {
#ifdef _WIN32
            TerminateProcess(process_info.hProcess, 0);
#else
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
#endif
            process_terminated = true;
        }
#ifdef PROCESS_RUNNER_TRACE
        printf("> %s\n", line);
#endif
    }
    if (process_terminated) {
        status = TEST_PROCESS_KILLED;
    }
#ifdef _WIN32
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    CloseHandle(stdout_read);
#else
    waitpid(pid, NULL, 0);
    posix_spawn_file_actions_destroy(&file_actions);
#endif
    free((void *)command);
    free(line_buffer);
    return status;
}

test_status ProcessRunnerRunTest(const char *test_name, uint32_t device_index, helper_arraylist *results) {
    return ProcessRunnerRunTestKillable(test_name, device_index, results, NULL);
}

test_status ProcessRunnerRunTestAsync(const char *test_name, uint32_t device_index, helper_arraylist *results, test_status *completion_code, void **kill_handle) {
    if (completion_code == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    process_runner_thread_data *thread_data = malloc(sizeof(process_runner_thread_data));
    if (thread_data == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    thread_data->test_name = test_name;
    thread_data->device_index = device_index;
    thread_data->results = results;
    thread_data->completion_code = completion_code;
    thread_data->kill_process = false;
    thread_data->has_kill_handle = kill_handle != NULL;

    *completion_code = TEST_ASYNC_PROCESS_RUNNING;
    helper_thread thread = HelperCreateThread(&ProcessRunnerAsyncThreadFunction, (void *)thread_data);
    if (thread == NULL) {
        *completion_code = TEST_FAILED_TO_SPAWN_THREAD;
        return TEST_FAILED_TO_SPAWN_THREAD;
    }
    if (kill_handle != NULL) {
        *kill_handle = (void *)thread_data;
    }
    return TEST_OK;
}

static void ProcessRunnerAsyncThreadFunction(uint32_t thread_id, void *input_data) {
    process_runner_thread_data *thread_data = (process_runner_thread_data *)input_data;

    *(thread_data->completion_code) = ProcessRunnerRunTestKillable(thread_data->test_name, thread_data->device_index, thread_data->results, &(thread_data->kill_process));

    if (!thread_data->has_kill_handle) {
        free(thread_data);
    }
}

test_status ProcessRunnerTerminateAsync(void *kill_handle) {
    if (kill_handle == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    process_runner_thread_data *thread_data = (process_runner_thread_data *)kill_handle;
    thread_data->kill_process = true;
    while (*thread_data->completion_code == TEST_ASYNC_PROCESS_RUNNING) {
        HelperSleep(PROCESS_RUNNER_ALIVE_CHECKER_INTERVAL_MS);
    }
    return TEST_OK;
}