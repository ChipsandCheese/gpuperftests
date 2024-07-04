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

#ifndef PROCESS_RUNNER_H
#define PROCESS_RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

#define PROCESS_RUNNER_MAXIMUM_RESULT_LENGTH    (128)

typedef struct process_runner_keypair_t {
    char key[PROCESS_RUNNER_MAXIMUM_RESULT_LENGTH];
    char result[PROCESS_RUNNER_MAXIMUM_RESULT_LENGTH];
} process_runner_keypair;

test_status ProcessRunnerRunTest(const char *test_name, uint32_t device_index, helper_arraylist *results);
test_status ProcessRunnerRunTestKillable(const char *test_name, uint32_t device_index, helper_arraylist *results, bool *kill_process);
test_status ProcessRunnerRunTestAsync(const char *test_name, uint32_t device_index, helper_arraylist *results, test_status *completion_code, void **kill_handle);
test_status ProcessRunnerTerminateAsync(void *kill_handle);

#ifdef __cplusplus
}
#endif
#endif