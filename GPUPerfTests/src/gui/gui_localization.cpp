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
#include "helper.h"
#include "gui/gui_localization.h"
#include "resources.h"
#include "logger.h"

#define GUI_LOCALIZATION_DEFAULT_LANGUAGE   "en_US"
#define GUI_LOCALIZATION_FILE_EXTENSION     ".lang"
#define GUI_LOCALIZATION_ERROR_UNLOADED     "ERROR: LANG NOT LOADED"
#define GUI_LOCALIZATION_ERROR_STRING_NULL  "ERROR: STRING NULL"
#define GUI_LOCALIZATION_ERROR_KEY_NULL     "ERROR: KEY NULL"
//#define GUI_LOCALIZATION_DEBUG

static size_t gui_localization_entry_count = 0;
static const char **gui_localization_keys = nullptr;
static const char **gui_localization_values = nullptr;
static const char *gui_localization_raw_data = nullptr;
static int gui_localization_invalidation_counter = 0;

static test_status _GuiLocalizationProcessFile(const char *data, size_t length, const char *language, size_t *entry_count) {
    const char *key = nullptr;
    const char *value = nullptr;
    size_t key_length = 0;
    size_t value_length = 0;
    size_t entries = 0;

    // Parse the file char by char
    for (size_t i = 0; i < length; i++) {
        char c = data[i];
        if ((c == '\n' || i + 1 == length) && key != nullptr && value != nullptr) {
            // We got a KV pair
            if (c != '\n') {
                // Last character of the file is not a newline, so assume it is a part of a value
                value_length++;
            }
            // Only add the entry to the KV array if we are not just checking the amount of entries
            if (entry_count == nullptr) {
                gui_localization_keys[entries] = key;
                gui_localization_values[entries] = value;
                ((char *)key)[key_length] = '\0'; // Just add a NUL into the existing data array in-place, no need to re-allocate them
                ((char *)value)[value_length] = '\0'; // since there is a '=' after every key and a LF after every value (or a NUL already at the very end)
#ifdef GUI_LOCALIZATION_DEBUG
                DEBUG("\"%s\" = \"%s\"\n", key, value);
#endif
            }
            entries++;
            key = nullptr;
            value = nullptr;
        } else if (c == '=' && key != nullptr && value == nullptr) {
            // We got a full key, move onto the value
            value = data + i + 1; // +1 to skip the current '='
            value_length = 0;
        } else if (c != '\r') {
            if (value == nullptr) {
                // We got the start of a key
                if (key == nullptr) {
                    key = data + i;
                    key_length = 0;
                }
                if (c == '\n') {
                    if (key != nullptr && ((key_length >= 2 && key[0] == '/' && key[1] == '/') || (key_length == 0))) {
                        // This is actually a comment or empty line, skip it
                    } else {
                        WARNING("Malformed key \"%.*s\" in language file %s\n", key_length, key, language);
                    }
                    key = nullptr;
                }
                key_length++;
            } else {
                value_length++;
            }
        }
    }
    if (entry_count != nullptr) {
        *entry_count = entries;
    }
    return TEST_OK;
}

test_status GuiLocalizationInitialize(const char *language) {
    const char *filepath = nullptr;
    const char *lang_data_ro = nullptr;
    size_t filepath_length = 0;
    size_t file_size = 0;
    bool load_default = true;

    if (language == nullptr) {
        return TEST_INVALID_PARAMETER;
    }

    // Create the path with the supplied language and load the file
    test_status status = HelperPrintToBuffer(&filepath, &filepath_length, "%s" GUI_LOCALIZATION_FILE_EXTENSION, language);
    if (TEST_SUCCESS(status)) {
        status = ResourcesGetLanguageFile(filepath, &lang_data_ro, &file_size);
        free((void *)filepath);
        if (TEST_SUCCESS(status)) {
            load_default = false;
        }
    }
    if (load_default) {
        // Load the default language file instead
        status = ResourcesGetLanguageFile(GUI_LOCALIZATION_DEFAULT_LANGUAGE GUI_LOCALIZATION_FILE_EXTENSION, &lang_data_ro, &file_size);
        TEST_RETFAIL(status);
    }
    // Since we directly modify the file, we need to make a copy
    const char *lang_data = (const char *)malloc(file_size);
    if (lang_data == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    memcpy((void *)lang_data, (void *)lang_data_ro, file_size);
    // Count the amount of entries in the language file
    size_t entry_count = 0;
    status = _GuiLocalizationProcessFile(lang_data, file_size, load_default ? GUI_LOCALIZATION_DEFAULT_LANGUAGE : language, &entry_count);
    if (!TEST_SUCCESS(status)) {
        free((void *)lang_data);
        return status;
    }
    // Allocate the keys and values arrays
    gui_localization_keys = (const char **)malloc(entry_count * sizeof(const char *));
    if (gui_localization_keys == nullptr) {
        free((void *)lang_data);
        return TEST_OUT_OF_MEMORY;
    }
    gui_localization_values = (const char **)malloc(entry_count * sizeof(const char *));
    if (gui_localization_values == nullptr) {
        free((void *)lang_data);
        free((void *)gui_localization_values);
        return TEST_OUT_OF_MEMORY;
    }
    // Populate the keys/values arrays
    status = _GuiLocalizationProcessFile(lang_data, file_size, load_default ? GUI_LOCALIZATION_DEFAULT_LANGUAGE : language, nullptr);
    if (!TEST_SUCCESS(status)) {
        free((void *)lang_data);
        free((void *)gui_localization_keys);
        free((void *)gui_localization_values);
        return status;
    }
    // Only set it now after everything is initialized
    gui_localization_raw_data = lang_data;
    gui_localization_entry_count = entry_count;
    return TEST_OK;
}

const char *GuiLocalizationTranslate(gui_localization_string *string) {
    if (string == nullptr) {
        return GUI_LOCALIZATION_ERROR_STRING_NULL;
    }
    if (gui_localization_raw_data == nullptr || gui_localization_entry_count == 0) {
        return GUI_LOCALIZATION_ERROR_UNLOADED;
    }
    if (string->cached_value == nullptr || string->last_invalidation_id != gui_localization_invalidation_counter) {
        if (string->key == nullptr) {
            return GUI_LOCALIZATION_ERROR_KEY_NULL;
        }
        for (size_t i = 0; i < gui_localization_entry_count; i++) {
            if (strcmp(string->key, gui_localization_keys[i]) == 0) {
                string->cached_value = gui_localization_values[i];
                break;
            }
        }
        // If we can't find a translation, just return the key
        if (string->cached_value == nullptr) {
            string->cached_value = string->key;
        }
        string->last_invalidation_id = gui_localization_invalidation_counter;
#ifdef GUI_LOCALIZATION_DEBUG
        DEBUG("Translating %s -> %s\n", string->key, string->cached_value);
#endif
    }
    return string->cached_value;
}

void GuiLocalizationInvalidateTranslations() {
    gui_localization_invalidation_counter++;
}