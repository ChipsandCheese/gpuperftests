// Copyright (c) 2022, Nemes <nemes@nemez.net>
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
#include "resources.h"
#define EMBED_RESOURCES
#include "ab_embedded_languages.h"
#include "ab_embedded_shaders.h"
#include "ab_embedded_icons.h"
#include "ab_embedded_fonts.h"
#include "ab_embedded_texts.h"

test_status ResourcesGetShaderFile(const char *filepath, const char **data, size_t *size) {
    for (size_t i = 0; i < ab_embedded_resource_count_ab_embedded_shaders_h; i++) {
        struct _ab_embedded_resource_t *resource = &(ab_embedded_resources_ab_embedded_shaders_h[i]);
        if (strcmp(filepath, resource->file_name) == 0) {
            *size = resource->file_size;
            *data = (const char *)resource->file_data;
            return TEST_OK;
        }
    }
    return TEST_EMBEDDED_RESOURCE_NOT_FOUND;
}

test_status ResourcesGetLanguageFile(const char *filepath, const char **data, size_t *size) {
    for (size_t i = 0; i < ab_embedded_resource_count_ab_embedded_languages_h; i++) {
        struct _ab_embedded_resource_t *resource = &(ab_embedded_resources_ab_embedded_languages_h[i]);
        if (strcmp(filepath, resource->file_name) == 0) {
            *size = resource->file_size;
            *data = (const char *)resource->file_data;
            return TEST_OK;
        }
    }
    return TEST_EMBEDDED_RESOURCE_NOT_FOUND;
}

test_status ResourcesGetIconFile(const char *filepath, const char **data, size_t *size) {
    for (size_t i = 0; i < ab_embedded_resource_count_ab_embedded_icons_h; i++) {
        struct _ab_embedded_resource_t *resource = &(ab_embedded_resources_ab_embedded_icons_h[i]);
        if (strcmp(filepath, resource->file_name) == 0) {
            *size = resource->file_size;
            *data = (const char *)resource->file_data;
            return TEST_OK;
        }
    }
    return TEST_EMBEDDED_RESOURCE_NOT_FOUND;
}

test_status ResourcesGetFontFile(const char *filepath, const char **data, size_t *size) {
    for (size_t i = 0; i < ab_embedded_resource_count_ab_embedded_fonts_h; i++) {
        struct _ab_embedded_resource_t *resource = &(ab_embedded_resources_ab_embedded_fonts_h[i]);
        if (strcmp(filepath, resource->file_name) == 0) {
            *size = resource->file_size;
            *data = (const char *)resource->file_data;
            return TEST_OK;
        }
    }
    return TEST_EMBEDDED_RESOURCE_NOT_FOUND;
}

test_status ResourcesGetTextFile(const char *filepath, const char **data, size_t *size) {
    for (size_t i = 0; i < ab_embedded_resource_count_ab_embedded_texts_h; i++) {
        struct _ab_embedded_resource_t *resource = &(ab_embedded_resources_ab_embedded_texts_h[i]);
        if (strcmp(filepath, resource->file_name) == 0) {
            *size = resource->file_size;
            *data = (const char *)resource->file_data;
            return TEST_OK;
        }
    }
    return TEST_EMBEDDED_RESOURCE_NOT_FOUND;
}