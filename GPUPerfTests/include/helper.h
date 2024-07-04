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

#ifndef HELPER_H
#define HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#define HELPER_ARRAYLIST_SIZE_STEP  16

#define max(x, y)   (((x) > (y)) ? (x) : (y))
#define min(x, y)   (((x) < (y)) ? (x) : (y))

#define HelperClear(structure)      memset(&structure, 0, sizeof(structure))

typedef struct helper_arraylist_t {
    void *data;
    size_t capacity;
    size_t size;
    size_t element_size;
} helper_arraylist;

struct helper_linkedlist_node_t;

typedef struct helper_linkedlist_node_t {
    const void *data;
    struct helper_linkedlist_node_t *next;
} helper_linkedlist_node;

typedef struct helper_linkedlist_t {
    helper_linkedlist_node *root;
} helper_linkedlist;

typedef struct helper_unit_pair_t {
    float value;
    char units[4];
} helper_unit_pair;

typedef void *helper_thread;
typedef void (*helper_thread_func)(uint32_t thread_id, void *input_data);

typedef uint32_t helper_atomic_bool;
typedef int16_t helper_atomic_int16;
typedef uint16_t helper_atomic_uint16;
typedef int32_t helper_atomic_int32;
typedef uint32_t helper_atomic_uint32;
typedef int64_t helper_atomic_int64;
typedef uint64_t helper_atomic_uint64;

test_status HelperLoadFile(const char *filepath, const char **data, size_t *size);
void *HelperOpenFileForWriting(const char *filepath);
void HelperWriteFile(void *file_handle, const char *format, ...);
void HelperCloseFile(void *file_handle);
const char *HelperDecodePng(const char *data, size_t size, int spng_format, uint32_t *image_width, uint32_t *image_height);
void HelperResetTimestamp();
uint64_t HelperGetTimestamp();
uint64_t HelperMarkTimestamp();
void HelperSleep(uint64_t milliseconds);
test_status HelperPrintToBuffer(const char **buffer, size_t *length, const char *format, ...);
test_status HelperArrayListInitialize(helper_arraylist *arraylist, size_t element_size);
test_status HelperArrayListAdd(helper_arraylist *arraylist, const void *element_data, size_t element_size, size_t *index);
test_status HelperArrayListClean(helper_arraylist *arraylist);
void *HelperArrayListGet(helper_arraylist *arraylist, size_t index);
size_t HelperArrayListSize(helper_arraylist *arraylist);
void *HelperArrayListRawData(helper_arraylist *arraylist);
test_status HelperArrayListCopy(helper_arraylist *destination, helper_arraylist *source);
test_status HelperLinkedListInitialize(helper_linkedlist *linkedlist);
test_status HelperLinkedListAdd(helper_linkedlist *linkedlist, const void *element_data, size_t *index);
bool HelperLinkedListRemove(helper_linkedlist *linkedlist, size_t index);
test_status HelperLinkedListClean(helper_linkedlist *linkedlist);
void *HelperLinkedListGet(helper_linkedlist *linkedlist, size_t index);
size_t HelperLinkedListSize(helper_linkedlist *linkedlist);
bool HelperLinkedListIteratorHasNext(helper_linkedlist *linkedlist, void **context);
void *HelperLinkedListIteratorNext(helper_linkedlist *linkedlist, void **context);
uint64_t HelperFindLargestPowerOfTwo(uint64_t bound);
bool HelperStringPresent(const char *string, const char **strings, uint32_t strings_count);
void HelperConvertUnitsBytes1024(uint64_t number, helper_unit_pair *unit_pair);
void HelperConvertUnitsBits1024(uint64_t number, helper_unit_pair *unit_pair);
void HelperConvertUnitsBytes1000(uint64_t number, helper_unit_pair *unit_pair);
void HelperConvertUnitsBits1000(uint64_t number, helper_unit_pair *unit_pair);
void HelperConvertUnitsPlain1000(uint64_t number, helper_unit_pair *unit_pair);
void HelperSeedRandom(uint64_t *state, uint64_t seed);
uint64_t HelperGetFreeSystemMemory();
uint32_t HelperGetProcessorCount();
helper_thread HelperCreateThread(helper_thread_func thread_func, void *data);
void HelperWaitForThread(helper_thread thread);
void HelperCleanUpThread(helper_thread thread);
helper_thread *HelperCreateThreads(uint32_t thread_count, helper_thread_func thread_func, void **data);
void HelperWaitForThreads(helper_thread *threads, uint32_t thread_count);
void HelperCleanUpThreads(helper_thread *threads, uint32_t thread_count);
int16_t HelperAtomicIncrementInt16(helper_atomic_int16 *value);
uint16_t HelperAtomicIncrementUint16(helper_atomic_uint16 *value);
int32_t HelperAtomicIncrementInt32(helper_atomic_int32 *value);
uint32_t HelperAtomicIncrementUint32(helper_atomic_uint32 *value);
int64_t HelperAtomicIncrementInt64(helper_atomic_int64 *value);
uint64_t HelperAtomicIncrementUint64(helper_atomic_uint64 *value);
int16_t HelperAtomicDecrementInt16(helper_atomic_int16 *value);
uint16_t HelperAtomicDecrementUint16(helper_atomic_uint16 *value);
int32_t HelperAtomicDecrementInt32(helper_atomic_int32 *value);
uint32_t HelperAtomicDecrementUint32(helper_atomic_uint32 *value);
int64_t HelperAtomicDecrementInt64(helper_atomic_int64 *value);
uint64_t HelperAtomicDecrementUint64(helper_atomic_uint64 *value);
bool HelperAtomicBoolRead(helper_atomic_bool *atomic);
bool HelperAtomicBoolWrite(helper_atomic_bool *atomic, bool value);
bool HelperAtomicBoolSet(helper_atomic_bool *atomic);
bool HelperAtomicBoolClear(helper_atomic_bool *atomic);
test_status HelperConvertByteArrayInt64(uint8_t* byte_array, size_t bytes, int64_t* output);
test_status HelperConvertByteArrayUint64(uint8_t* byte_array, size_t bytes, uint64_t* output);

static inline uint64_t HelperGenerateRandom(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static inline float HelperGenerateRandomFloat(uint64_t *state) {
    uint64_t x = HelperGenerateRandom(state);
    return (float)((double)(x & INT_MAX) / (double)INT_MAX);
}

#ifdef __cplusplus
}
#endif
#endif