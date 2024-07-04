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

#include "main.h"
#ifdef _WIN32
#include "sanitize_windows_h.h"
#include <Windows.h>
#else
#include <pthread.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sched.h>
#include <unistd.h>
#endif
#include "logger.h"
#include "helper.h"
#include <stdarg.h>
#include <spng.h>

#define _HELPER_BYTE_SUFFIX "B"
#define _HELPER_BIT_SUFFIX  "b"
#define _HELPER_EXBI_VALUE  (1152921504606846976ULL)
#define _HELPER_PEBI_VALUE  (1125899906842624ULL)
#define _HELPER_TEBI_VALUE  (1099511627776ULL)
#define _HELPER_GIBI_VALUE  (1073741824ULL)
#define _HELPER_MEBI_VALUE  (1048576ULL)
#define _HELPER_KIBI_VALUE  (1024ULL)
#define _HELPER_EXBI_SUFFIX "Ei"
#define _HELPER_PEBI_SUFFIX "Pi"
#define _HELPER_TEBI_SUFFIX "Ti"
#define _HELPER_GIBI_SUFFIX "Gi"
#define _HELPER_MEBI_SUFFIX "Mi"
#define _HELPER_KIBI_SUFFIX "Ki"
#define _HELPER_EXA_VALUE   (1000000000000000000ULL)
#define _HELPER_PETA_VALUE  (1000000000000000ULL)
#define _HELPER_TERA_VALUE  (1000000000000ULL)
#define _HELPER_GIGA_VALUE  (1000000000ULL)
#define _HELPER_MEGA_VALUE  (1000000ULL)
#define _HELPER_KILO_VALUE  (1000ULL)
#define _HELPER_EXA_SUFFIX  "E"
#define _HELPER_PETA_SUFFIX "P"
#define _HELPER_TERA_SUFFIX "T"
#define _HELPER_GIGA_SUFFIX "G"
#define _HELPER_MEGA_SUFFIX "M"
#define _HELPER_KILO_SUFFIX "K"

typedef struct helper_thread_data_t {
    helper_thread_func thread_func;
    uint32_t thread_id;
    void *input_data;
} helper_thread_data;

static uint64_t timer_start;
static uint64_t timer_us_in_ticks;

static void _HelperConvertUnits1024(uint64_t number, helper_unit_pair *unit_pair);
static void _HelperConvertUnits1000(uint64_t number, helper_unit_pair *unit_pair);
#ifdef _WIN32
static DWORD WINAPI _HelperInternalThreadFunc(void *thread_data_ptr);
#else
static void *_HelperInternalThreadFunc(void *thread_data_ptr);
#endif

test_status HelperLoadFile(const char *filepath, const char **data, size_t *size) {
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        return TEST_FILE_NOT_FOUND;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return TEST_FILE_IO_ERROR;
    }
    long file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return TEST_FILE_IO_ERROR;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return TEST_FILE_IO_ERROR;
    }
    char *content = malloc((size_t)file_size + 1);
    if (content == NULL) {
        fclose(file);
        return TEST_OUT_OF_MEMORY;
    }
    memset(content, '\0', (size_t)file_size + 1);
    size_t read_data = fread(content, 1, (size_t)file_size, file);
    fclose(file);
    *data = content;
    *size = (size_t)read_data;
    return TEST_OK;
}

void *HelperOpenFileForWriting(const char *filepath) {
    return (void *)fopen(filepath, "wb");
}

void HelperWriteFile(void *file_handle, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf((FILE *)file_handle, format, args);
    va_end(args);
}

void HelperCloseFile(void *file_handle) {
    if (file_handle != NULL) {
        fclose((FILE *)file_handle);
    }
}

const char *HelperDecodePng(const char *data, size_t size, int spng_format, uint32_t *image_width, uint32_t *image_height) {
    spng_ctx *ctx = spng_ctx_new(0);
    spng_set_png_buffer(ctx, data, size);
    struct spng_ihdr ihdr;
    if (spng_get_ihdr(ctx, &ihdr)) {
        spng_ctx_free(ctx);
        return NULL;
    }
    size_t png_size = 0;
    spng_decoded_image_size(ctx, spng_format, &png_size);
    void *png_data = malloc(png_size);
    if (png_data == NULL) {
        spng_ctx_free(ctx);
        return NULL;
    }
    memset(png_data, 0, png_size);
    spng_decode_image(ctx, png_data, png_size, spng_format, 0);
    spng_ctx_free(ctx);

    if (image_width != NULL) {
        *image_width = ihdr.width;
    }
    if (image_height != NULL) {
        *image_height = ihdr.height;
    }
    return (const char *)png_data;
}

void HelperResetTimestamp() {
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    timer_us_in_ticks = li.QuadPart / 1000000ULL;
    QueryPerformanceCounter(&li);
    timer_start = li.QuadPart;
#else
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) {
        timer_start = 0;
    } else {
        uint64_t time = (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
        timer_start = time;
    }
#endif
}

uint64_t HelperGetTimestamp() {
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (li.QuadPart - timer_start) / timer_us_in_ticks;
#else
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) {
        return 0;
    } else {
        uint64_t time = (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
        return time - timer_start;
    }
#endif
}

uint64_t HelperMarkTimestamp() {
#ifdef _WIN32
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    uint64_t micros = (li.QuadPart - timer_start) / timer_us_in_ticks;
    timer_start = li.QuadPart;
    return micros;
#else
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) {
        return 0;
    } else {
        uint64_t time = (uint64_t)tv.tv_sec * 1000000 + (uint64_t)tv.tv_usec;
        uint64_t micros = time - timer_start;
        timer_start = time;
        return micros;
    }
#endif
}

void HelperSleep(uint64_t milliseconds) {
#ifdef _WIN32
    Sleep((DWORD)milliseconds);
#else
    usleep((useconds_t)milliseconds * 1000);
#endif
}

test_status HelperPrintToBuffer(const char **buffer, size_t *length, const char *format, ...) {
    va_list list;
    va_start(list, format);
    int string_length = vsnprintf(NULL, 0, format, list);
    va_end(list);

    if (string_length < 0) {
        return TEST_PRINTF_FORMAT_ERROR;
    }
    if (buffer == NULL) {
        if (length != NULL) {
            *length = (size_t)string_length;
        } else {
            return TEST_INVALID_PARAMETER;
        }
    } else {
        char *string_buffer = malloc((size_t)string_length + 1);
        if (string_buffer == NULL) {
            return TEST_OUT_OF_MEMORY;
        }
        memset(string_buffer, '\0', (size_t)string_length + 1);
        va_start(list, format);
        int string_length2 = vsnprintf(string_buffer, (size_t)string_length + 1, format, list);
        va_end(list);
        if (string_length2 < 0 || string_length2 != string_length) {
            free(string_buffer);
            return TEST_PRINTF_FORMAT_ERROR;
        }
        *buffer = string_buffer;
        if (length != NULL) {
            *length = (size_t)string_length2;
        }
    }
    return TEST_OK;
}

test_status HelperArrayListInitialize(helper_arraylist *arraylist, size_t element_size) {
    if (element_size == 0) {
        return TEST_INVALID_PARAMETER;
    }
    arraylist->element_size = element_size;
    arraylist->capacity = HELPER_ARRAYLIST_SIZE_STEP;
    arraylist->size = 0;
    arraylist->data = malloc(arraylist->element_size * arraylist->capacity);
    if (arraylist->data == NULL) {
        arraylist->capacity = 0;
        return TEST_OUT_OF_MEMORY;
    } else {
        memset(arraylist->data, 0, arraylist->element_size * arraylist->capacity);
        return TEST_OK;
    }
}

test_status HelperArrayListAdd(helper_arraylist *arraylist, const void *element_data, size_t element_size, size_t *index) {
    if (element_data == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    if (arraylist->data == NULL) {
        test_status status = HelperArrayListInitialize(arraylist, element_size);
        TEST_RETFAIL(status);
    }
    if (arraylist->capacity <= arraylist->size) {
        arraylist->capacity += HELPER_ARRAYLIST_SIZE_STEP;
        void *new_test_list = realloc(arraylist->data, arraylist->capacity * arraylist->element_size);
        if (new_test_list == NULL) {
            free(arraylist->data);
            arraylist->data = NULL;
            arraylist->capacity = 0;
            arraylist->size = 0;
            return TEST_OUT_OF_MEMORY;
        } else {
            arraylist->data = new_test_list;
        }
    }
    void *destination = (void *)((size_t)arraylist->data + arraylist->size * arraylist->element_size);
    memcpy(destination, element_data, element_size);
    if (index != NULL) {
        *index = arraylist->size;
    }
    arraylist->size++;
    return TEST_OK;
}

test_status HelperArrayListClean(helper_arraylist *arraylist) {
    if (arraylist->data == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    free(arraylist->data);
    arraylist->data = NULL;
    arraylist->size = 0;
    arraylist->capacity = 0;
    return TEST_OK;
}

void *HelperArrayListGet(helper_arraylist *arraylist, size_t index) {
    if (arraylist->data == NULL) {
        return NULL;
    }
    size_t offset = arraylist->element_size * index;
    if (offset > (arraylist->size - 1) * arraylist->element_size) {
        return NULL;
    }
    return (void *)((size_t)arraylist->data + offset);
}

size_t HelperArrayListSize(helper_arraylist *arraylist) {
    if (arraylist->data == NULL) {
        return 0;
    }
    return arraylist->size;
}

void *HelperArrayListRawData(helper_arraylist *arraylist) {
    return arraylist->data;
}

test_status HelperArrayListCopy(helper_arraylist *destination, helper_arraylist *source) {
    if (HelperArrayListRawData(destination) != NULL) {
        HelperArrayListClean(destination);
    }
    memcpy(destination, source, sizeof(helper_arraylist));
    void *source_raw = HelperArrayListRawData(source);
    if (source_raw != NULL) {
        void *destination_raw = malloc(source->element_size * source->capacity);
        if (destination_raw == NULL) {
            return TEST_OUT_OF_MEMORY;
        }
        memcpy(destination_raw, source_raw, source->element_size * source->capacity);
        destination->data = destination_raw;
    }
    return TEST_OK;
}

test_status HelperLinkedListInitialize(helper_linkedlist *linkedlist) {
    linkedlist->root = NULL;
    return TEST_OK;
}

test_status HelperLinkedListAdd(helper_linkedlist *linkedlist, const void *element_data, size_t *index) {
    helper_linkedlist_node *new_node = malloc(sizeof(helper_linkedlist_node));
    if (new_node == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    new_node->data = element_data;
    new_node->next = NULL;
    helper_linkedlist_node **next = &(linkedlist->root);
    size_t idx = 0;

    while (*next != NULL) {
        next = &((*next)->next);
        idx++;
    }
    *next = new_node;
    if (index != NULL) {
        *index = idx;
    }
    return TEST_OK;
}

test_status HelperLinkedListClean(helper_linkedlist *linkedlist) {
    helper_linkedlist_node *node = linkedlist->root;
    while (node != NULL) {
        helper_linkedlist_node *next_node = node->next;
        free(node);
        node = next_node;
    }
    return HelperLinkedListInitialize(linkedlist);
}

void *HelperLinkedListGet(helper_linkedlist *linkedlist, size_t index) {
    size_t current_index = 0;
    helper_linkedlist_node *node = linkedlist->root;
    while (node != NULL) {
        if (current_index == index) {
            return (void *)node->data;
        }
        node = node->next;
        current_index++;
    }
    return NULL;
}

bool HelperLinkedListRemove(helper_linkedlist *linkedlist, size_t index) {
    size_t current_index = 0;
    helper_linkedlist_node **node = &(linkedlist->root);
    while (*node != NULL) {
        if (current_index == index) {
            helper_linkedlist_node *to_remove = *node;
            *node = (*node)->next;
            free(to_remove);
            return true;
        }
        node = &((*node)->next);
        current_index++;
    }
    return false;
}

size_t HelperLinkedListSize(helper_linkedlist *linkedlist) {
    size_t size = 0;
    helper_linkedlist_node *node = linkedlist->root;
    while (node != NULL) {
        node = node->next;
        size++;
    }
    return size;
}

bool HelperLinkedListIteratorHasNext(helper_linkedlist *linkedlist, void **context) {
    helper_linkedlist_node *node = ((*context) == NULL) ? linkedlist->root : ((helper_linkedlist_node *)*context);
    helper_linkedlist_node *saved_node = ((helper_linkedlist_node *)*context);
    if (saved_node == NULL) {
        node = linkedlist->root;
    } else {
        node = saved_node->next;
    }
    return node != NULL;
}

void *HelperLinkedListIteratorNext(helper_linkedlist *linkedlist, void **context) {
    helper_linkedlist_node *node = ((*context) == NULL) ? linkedlist->root : ((helper_linkedlist_node *)*context);
    helper_linkedlist_node *saved_node = ((helper_linkedlist_node *)*context);
    if (saved_node == NULL) {
        node = linkedlist->root;
    } else {
        node = saved_node->next;
    }
    *context = (void *)node;
    if (node == NULL) {
        return NULL;
    } else {
        return (void *)node->data;
    }
}

uint64_t HelperFindLargestPowerOfTwo(uint64_t bound) {
    uint64_t power = 0;
    uint64_t next_power = 1;
    while (next_power <= bound) {
        power = next_power;
        next_power *= 2;
    }
    return power;
}

bool HelperStringPresent(const char *string, const char **strings, uint32_t strings_count) {
    if (string != NULL && strings != NULL) {
        for (uint32_t i = 0; i < strings_count; i++) {
            if (strings[i] != NULL && strcmp(string, strings[i]) == 0) {
                return true;
            }
        }
    }
    return false;
}

void HelperConvertUnitsBytes1024(uint64_t number, helper_unit_pair *unit_pair) {
    _HelperConvertUnits1024(number, unit_pair);
    strcat(unit_pair->units, _HELPER_BYTE_SUFFIX);
}

void HelperConvertUnitsBits1024(uint64_t number, helper_unit_pair *unit_pair) {
    _HelperConvertUnits1024(number, unit_pair);
    strcat(unit_pair->units, _HELPER_BIT_SUFFIX);
}

void HelperConvertUnitsBytes1000(uint64_t number, helper_unit_pair *unit_pair) {
    _HelperConvertUnits1000(number, unit_pair);
    strcat(unit_pair->units, _HELPER_BYTE_SUFFIX);
}

void HelperConvertUnitsBits1000(uint64_t number, helper_unit_pair *unit_pair) {
    _HelperConvertUnits1000(number, unit_pair);
    strcat(unit_pair->units, _HELPER_BIT_SUFFIX);
}

void HelperConvertUnitsPlain1000(uint64_t number, helper_unit_pair *unit_pair) {
    _HelperConvertUnits1000(number, unit_pair);
}

static void _HelperConvertUnits1024(uint64_t number, helper_unit_pair *unit_pair) {
    memset(unit_pair->units, '\0', sizeof(unit_pair->units));
    if (number >= _HELPER_EXBI_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_EXBI_VALUE);
        memcpy(unit_pair->units, _HELPER_EXBI_SUFFIX, sizeof(_HELPER_EXBI_SUFFIX));
    } else if (number >= _HELPER_PEBI_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_PEBI_VALUE);
        memcpy(unit_pair->units, _HELPER_PEBI_SUFFIX, sizeof(_HELPER_PEBI_SUFFIX));
    } else if (number >= _HELPER_TEBI_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_TEBI_VALUE);
        memcpy(unit_pair->units, _HELPER_TEBI_SUFFIX, sizeof(_HELPER_TEBI_SUFFIX));
    } else if (number >= _HELPER_GIBI_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_GIBI_VALUE);
        memcpy(unit_pair->units, _HELPER_GIBI_SUFFIX, sizeof(_HELPER_GIBI_SUFFIX));
    } else if (number >= _HELPER_MEBI_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_MEBI_VALUE);
        memcpy(unit_pair->units, _HELPER_MEBI_SUFFIX, sizeof(_HELPER_MEBI_SUFFIX));
    } else if (number >= _HELPER_KIBI_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_KIBI_VALUE);
        memcpy(unit_pair->units, _HELPER_KIBI_SUFFIX, sizeof(_HELPER_KIBI_SUFFIX));
    } else {
        unit_pair->value = (float)number;
    }
}

static void _HelperConvertUnits1000(uint64_t number, helper_unit_pair *unit_pair) {
    memset(unit_pair->units, '\0', sizeof(unit_pair->units));
    if (number >= _HELPER_EXA_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_EXA_VALUE);
        memcpy(unit_pair->units, _HELPER_EXA_SUFFIX, sizeof(_HELPER_EXA_SUFFIX));
    } else if (number >= _HELPER_PETA_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_PETA_VALUE);
        memcpy(unit_pair->units, _HELPER_PETA_SUFFIX, sizeof(_HELPER_PETA_SUFFIX));
    } else if (number >= _HELPER_TERA_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_TERA_VALUE);
        memcpy(unit_pair->units, _HELPER_TERA_SUFFIX, sizeof(_HELPER_TERA_SUFFIX));
    } else if (number >= _HELPER_GIGA_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_GIGA_VALUE);
        memcpy(unit_pair->units, _HELPER_GIGA_SUFFIX, sizeof(_HELPER_GIGA_SUFFIX));
    } else if (number >= _HELPER_MEGA_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_MEGA_VALUE);
        memcpy(unit_pair->units, _HELPER_MEGA_SUFFIX, sizeof(_HELPER_MEGA_SUFFIX));
    } else if (number >= _HELPER_KILO_VALUE) {
        unit_pair->value = (float)((double)number / (double)_HELPER_KILO_VALUE);
        memcpy(unit_pair->units, _HELPER_KILO_SUFFIX, sizeof(_HELPER_KILO_SUFFIX));
    } else {
        unit_pair->value = (float)number;
    }
}

void HelperSeedRandom(uint64_t *state, uint64_t seed) {
    if (seed == 0) {
        WARNING("HelperRNG: Initializing with zero seed!\n");
    }
    *state = seed;
}

uint64_t HelperGetFreeSystemMemory() {
#ifdef _WIN32
    MEMORYSTATUSEX memory_status;
    memory_status.dwLength = sizeof(memory_status);

    if (GlobalMemoryStatusEx(&memory_status) != 0) {
        return (uint64_t)memory_status.ullAvailPhys;
    } else {
        return 0;
    }
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (uint64_t)info.freeram * (uint64_t)info.mem_unit;
    } else {
        return 0;
    }
#endif
}

uint32_t HelperGetProcessorCount() {
#ifdef _WIN32
    SYSTEM_INFO system_info;
    
    GetSystemInfo(&system_info);

    return (uint32_t)system_info.dwNumberOfProcessors;
#else
    cpu_set_t cpu_set;
    HelperClear(cpu_set);
    if (sched_getaffinity(0, sizeof(cpu_set), &cpu_set) == 0) {
        uint32_t thread_count = 0;
        for (int i = 0; i < (__CPU_SETSIZE / __NCPUBITS); i++) {
            __cpu_mask mask = cpu_set.__bits[i];
            for (int j = 0; j < __NCPUBITS; j++) {
                if ((mask & ((__cpu_mask)1 << j)) != 0) {
                    thread_count++;
                }
            }
        }
        return thread_count;
    } else {
        return 1;
    }
#endif
}

helper_thread HelperCreateThread(helper_thread_func thread_func, void *data) {
    helper_thread_data *thread_data = malloc(sizeof(helper_thread_data));
    if (thread_data == NULL) {
        return NULL;
    }
    thread_data->thread_func = thread_func;
    thread_data->thread_id = 0;
    thread_data->input_data = data;

#ifdef _WIN32
    HANDLE handle = CreateThread(NULL, 0, _HelperInternalThreadFunc, thread_data, 0, NULL);
    return (helper_thread)handle;
#else
    pthread_t handle;
    pthread_create(&handle, NULL, _HelperInternalThreadFunc, thread_data);
    return (void *)(size_t)handle;
#endif
}

void HelperWaitForThread(helper_thread thread) {
    if (thread != NULL) {
#ifdef _WIN32
        WaitForSingleObject((HANDLE)thread, INFINITE);
#else
        pthread_join((pthread_t)(size_t)thread, NULL);
#endif
    }
}

void HelperCleanUpThread(helper_thread thread) {
    if (thread != NULL) {
#ifdef _WIN32
        CloseHandle((HANDLE)thread);
#endif
    }
}

helper_thread *HelperCreateThreads(uint32_t thread_count, helper_thread_func thread_func, void **data) {
    helper_thread *threads = malloc(sizeof(helper_thread) * thread_count);
    if (threads == NULL) {
        return NULL;
    }
    for (uint32_t i = 0; i < thread_count; i++) {
        helper_thread_data *thread_data = malloc(sizeof(helper_thread_data));
        if (thread_data == NULL) {
            return NULL;
        }
        thread_data->thread_func = thread_func;
        thread_data->thread_id = i;
        thread_data->input_data = data[i];

#ifdef _WIN32
        HANDLE handle = CreateThread(NULL, 0, _HelperInternalThreadFunc, thread_data, 0, NULL);
        threads[i] = (helper_thread)handle;
#else
        pthread_t handle;
        pthread_create(&handle, NULL, _HelperInternalThreadFunc, thread_data);
        threads[i] = (void *)(size_t)handle;
#endif
    }
    return threads;
}

void HelperWaitForThreads(helper_thread *threads, uint32_t thread_count) {
    if (threads != NULL && thread_count > 0) {
#ifdef _WIN32
        uint32_t remaining_waits = thread_count;
        uint32_t wait_offset = 0;
        while (remaining_waits > 0) {
            uint32_t current_waits = min(remaining_waits, MAXIMUM_WAIT_OBJECTS);
            WaitForMultipleObjects((DWORD)current_waits, (const HANDLE *)&(threads[wait_offset]), TRUE, INFINITE);
            remaining_waits -= current_waits;
            wait_offset += current_waits;
        }
#else
        for (uint32_t i = 0; i < thread_count; i++) {
            pthread_join((pthread_t)(size_t)(threads[i]), NULL);
        }
#endif
    }
}

void HelperCleanUpThreads(helper_thread *threads, uint32_t thread_count) {
    if (threads != NULL && thread_count > 0) {
#ifdef _WIN32
        for (uint32_t i = 0; i < thread_count; i++) {
            if (threads[i] != NULL) {
                CloseHandle((HANDLE)(threads[i]));
            }
        }
#endif
        free(threads);
    }
}

#ifdef _WIN32
static DWORD WINAPI _HelperInternalThreadFunc(void *thread_data_ptr) {
#else
static void *_HelperInternalThreadFunc(void *thread_data_ptr) {
#endif
    helper_thread_data *thread_data = (helper_thread_data *)thread_data_ptr;
    if (thread_data != NULL) {
        if (thread_data->thread_func != NULL) {
            thread_data->thread_func(thread_data->thread_id, thread_data->input_data);
        }
        free(thread_data);
    }
    return 0;
}

int16_t HelperAtomicIncrementInt16(helper_atomic_int16 *value) {
#ifdef _WIN32
    return InterlockedIncrement16(value);
#else
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
#endif
}

uint16_t HelperAtomicIncrementUint16(helper_atomic_uint16 *value) {
#ifdef _WIN32
    return InterlockedIncrement16(value);
#else
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
#endif
}

int32_t HelperAtomicIncrementInt32(helper_atomic_int32 *value) {
#ifdef _WIN32
    return InterlockedIncrement(value);
#else
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
#endif
}

uint32_t HelperAtomicIncrementUint32(helper_atomic_uint32 *value) {
#ifdef _WIN32
    return InterlockedIncrement(value);
#else
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
#endif
}

int64_t HelperAtomicIncrementInt64(helper_atomic_int64 *value) {
#ifdef _WIN32
    return InterlockedIncrement64(value);
#else
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
#endif
}

uint64_t HelperAtomicIncrementUint64(helper_atomic_uint64 *value) {
#ifdef _WIN32
    return InterlockedIncrement64(value);
#else
    return __atomic_fetch_add(value, 1, __ATOMIC_SEQ_CST) + 1;
#endif
}

int16_t HelperAtomicDecrementInt16(helper_atomic_int16 *value) {
#ifdef _WIN32
    return InterlockedDecrement16(value);
#else
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
#endif
}

uint16_t HelperAtomicDecrementUint16(helper_atomic_uint16 *value) {
#ifdef _WIN32
    return InterlockedDecrement16(value);
#else
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
#endif
}

int32_t HelperAtomicDecrementInt32(helper_atomic_int32 *value) {
#ifdef _WIN32
    return InterlockedDecrement(value);
#else
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
#endif
}

uint32_t HelperAtomicDecrementUint32(helper_atomic_uint32 *value) {
#ifdef _WIN32
    return InterlockedDecrement(value);
#else
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
#endif
}

int64_t HelperAtomicDecrementInt64(helper_atomic_int64 *value) {
#ifdef _WIN32
    return InterlockedDecrement64(value);
#else
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
#endif
}

uint64_t HelperAtomicDecrementUint64(helper_atomic_uint64 *value) {
#ifdef _WIN32
    return InterlockedDecrement64(value);
#else
    return __atomic_fetch_sub(value, 1, __ATOMIC_SEQ_CST) - 1;
#endif
}

bool HelperAtomicBoolRead(helper_atomic_bool *atomic) {
#ifdef _WIN32
    return InterlockedOr(atomic, 0) != 0;
#else
    return __atomic_load_n(atomic, __ATOMIC_SEQ_CST) != 0;
#endif
}

bool HelperAtomicBoolWrite(helper_atomic_bool *atomic, bool value) {
#ifdef _WIN32
    return InterlockedExchange(atomic, value ? 1 : 0) != 0;
#else
    return __atomic_exchange_n(atomic, value ? 1 : 0, __ATOMIC_SEQ_CST) != 0;
#endif
}

bool HelperAtomicBoolSet(helper_atomic_bool *atomic) {
    return HelperAtomicBoolWrite(atomic, true);
}

bool HelperAtomicBoolClear(helper_atomic_bool *atomic) {
    return HelperAtomicBoolWrite(atomic, false);
}

test_status HelperConvertByteArrayInt64(uint8_t* byte_array, size_t bytes, int64_t* output) {
    if (bytes > 8) {
        return TEST_INVALID_PARAMETER;
    }
    *output = 0;
    for (size_t i = 0; i < bytes; i++) {
        *output += (int64_t)byte_array[i] << (i * 8);
    }
    return TEST_OK;
}

test_status HelperConvertByteArrayUint64(uint8_t* byte_array, size_t bytes, uint64_t* output) {
    if (bytes > 8) {
        return TEST_INVALID_PARAMETER;
    }
    *output = 0;
    for (size_t i = 0; i < bytes; i++) {
        *output += (uint64_t)byte_array[i] << (i * 8);
    }
    return TEST_OK;
}