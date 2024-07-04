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

#include <stdlib.h>
#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#endif

#define WRITE(file, string) fwrite(string, 1, sizeof(string) - 1, file)

static const char **file_list;
static size_t file_list_size;
static size_t file_list_capacity;

static const char filename_allowed_chars[] = "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static const char *sanitizeFile(const char *content, size_t content_size) {
    // Worst-case we get all values out as 255, so 4 bytes per input byte including the comma separator
    char *result_data = malloc(content_size * sizeof(char) * 4);
    if (result_data == NULL) {
        return NULL;
    }
    memset(result_data, 0, content_size * sizeof(char) * 4);
    size_t result_index = 0;

    for (size_t i = 0; i < content_size; i++) {
        unsigned char byte = (unsigned char)content[i];
        if (byte > 99) {
            result_data[result_index++] = (byte / 100) + '0';
            result_data[result_index++] = ((byte % 100) / 10) + '0';
            result_data[result_index++] = (byte % 10) + '0';
        } else if (byte > 9) {
            result_data[result_index++] = (byte / 10) + '0';
            result_data[result_index++] = (byte % 10) + '0';
        } else {
            result_data[result_index++] = byte + '0';
        }
        if (i + 1 < content_size) {
            result_data[result_index++] = ',';
        }
    }
    return (const char *)result_data;
}

static const char *readFile(const char *filepath, size_t *size, bool verbose) {
    const char *return_value = NULL;
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        printf("Failed to open file: %s\n", filepath);
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        printf("Failed to fseek(END) file: %s\n", filepath);
        goto cleanup;
    }

    long long int ftell_size = 0;
#ifdef _WIN32
    ftell_size = _ftelli64(file);
#else
    ftell_size = ftello(file);
#endif
    if (ftell_size == -1) {
        printf("Failed to ftell file: %s\n", filepath);
        goto cleanup;
    }
    size_t file_length = (size_t)ftell_size;
    if (fseek(file, 0, SEEK_SET) != 0) {
        printf("Failed to fseek(SET) file: %s\n", filepath);
        goto cleanup;
    }
    if (verbose) {
        printf("File size: %llu\n", file_length);
    }
    char *buffer = (char *)malloc(file_length);
    if (buffer == NULL) {
        printf("Failed to allocate buffer for file: %s\n", filepath);
        goto cleanup;
    }
    size_t read_elements = fread(buffer, 1, file_length, file);
    if (read_elements != file_length) {
        printf("Failed to fread file: %s\n", filepath);
    } else {
        *size = file_length;
        return_value = (const char *)buffer;
    }
cleanup:
    fclose(file);
    return return_value;
}

static bool addFileToList(const char *file) {
    if (file_list == NULL) {
        file_list_size = 0;
        file_list_capacity = 16;
        file_list = (const char **)malloc(file_list_capacity * sizeof(const char *));
        if (file_list == NULL) {
            return false;
        }
    }
    if (file_list_size == file_list_capacity) {
        file_list_capacity *= 2;
        char **new_file_list = (char **)malloc(file_list_capacity * sizeof(const char *));
        if (new_file_list == NULL) {
            return false;
        }
        memset(new_file_list, 0, file_list_capacity * sizeof(const char *));
        memcpy(new_file_list, file_list, file_list_size * sizeof(const char *));
        file_list = (const char **)new_file_list;
    }
    file_list[file_list_size] = file;
    file_list_size++;
    return true;
}

static bool processWildcardFilename(const char *wildcard_name) {
    const char *wildcard_ptr = strchr(wildcard_name, '*');
    if (wildcard_ptr == NULL) {
        return false;
    }
#ifdef _WIN32
    WIN32_FIND_DATAA find_data;

    HANDLE search_handle = FindFirstFileA(wildcard_name, &find_data);
    if (search_handle == INVALID_HANDLE_VALUE) {
        printf("Failed to list wildcard: %s\n", wildcard_name);
        return false;
    }
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            size_t full_path_length = strlen(wildcard_name) + sizeof(find_data.cFileName);
            char *full_path = malloc(full_path_length);
            if (full_path == NULL) {
                FindClose(search_handle);
                return false;
            }
            memset(full_path, 0, full_path_length);
            snprintf(full_path, full_path_length, "%.*s%s", (unsigned int)(wildcard_ptr - wildcard_name), wildcard_name, find_data.cFileName);
            if (!addFileToList(full_path)) {
                free(full_path);
                FindClose(search_handle);
                return false;
            }
        }
    } while (FindNextFileA(search_handle, &find_data) != FALSE);
    
    FindClose(search_handle);
#else
    size_t wildcard_dir_path_len = (size_t)(wildcard_ptr - wildcard_name);
    char *directory_path = malloc(wildcard_dir_path_len + 1);
    if (directory_path == NULL) {
        printf("Failed to allocate memory for wildcard directory name: %s\n", wildcard_name);
        return false;
    }
    memcpy(directory_path, wildcard_name, wildcard_dir_path_len);
    directory_path[wildcard_dir_path_len] = '\0';
    const char *file_extension = wildcard_ptr + 1;
    size_t extension_len = strlen(file_extension);
    
    struct dirent *dir_entry;
    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        free(directory_path);
        return false;
    }
    while ((dir_entry = readdir(dir)) != NULL) {
        size_t entry_name_len = strlen(dir_entry->d_name);
        if (entry_name_len > extension_len) {
            if (strncmp(dir_entry->d_name + entry_name_len - extension_len, file_extension, extension_len) == 0) {
                char *full_path = malloc(wildcard_dir_path_len + entry_name_len + 1);
                if (full_path == NULL) {
                    closedir(dir);
                    free(directory_path);
                    return false;
                }
                memset(full_path, 0, wildcard_dir_path_len + entry_name_len + 1);
                strcat(full_path, directory_path);
                strcat(full_path, dir_entry->d_name);
                if (!addFileToList(full_path)) {
                    free(full_path);
                    closedir(dir);
                    free(directory_path);
                    return false;
                }
            }
        }
    }
    closedir(dir);
    free(directory_path);
#endif
    return true;
}

static const char *sanitizeOutputFilename(const char *output_filename) {
    char *sanitized_output_filename_buffer = malloc(strlen(output_filename) + 1);
    if (sanitized_output_filename_buffer == NULL) {
        printf("Failed to allocate memory for output filename buffer, aborting!\n");
        return NULL;
    }
    strcpy(sanitized_output_filename_buffer, output_filename);
    char *sanitized_output_filename = strrchr(sanitized_output_filename_buffer, '\\');
    if (sanitized_output_filename == NULL) {
        sanitized_output_filename = strrchr(sanitized_output_filename_buffer, '/');
        if (sanitized_output_filename == NULL) {
            sanitized_output_filename = sanitized_output_filename_buffer;
        } else {
            sanitized_output_filename += 1;
        }
    } else {
        sanitized_output_filename += 1;
    }
    size_t output_filename_length = strlen(sanitized_output_filename);
    for (size_t i = 0; i < output_filename_length; i++) {
        bool allowed = false;
        for (size_t j = 0; j < sizeof(filename_allowed_chars) - 1; j++) {
            if (sanitized_output_filename[i] == filename_allowed_chars[j]) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            sanitized_output_filename[i] = '_';
        }
    }
    return sanitized_output_filename;
}

int main(int argc, const char **argv) {
    file_list = NULL;
    file_list_size = 0;
    file_list_capacity = 0;

    // That ought to be plenty of space to print a size_t/uint64_t
    char size_print_buffer[32];
    const char *output_filename = NULL;
    bool capturing_output_name = false;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        
        if (strcmp(arg, "-o") == 0 || strcmp(arg, "-O") == 0) {
            capturing_output_name = true;
        } else if (strcmp(arg, "-v") == 0 || strcmp(arg, "-V") == 0) {
            verbose = true;
        } else {
            if (capturing_output_name) {
                output_filename = arg;
                capturing_output_name = false;
            } else {
                if (strchr(arg, '*') != NULL) {
                    if (!processWildcardFilename(arg)) {
                        printf("An error occurred while processing arguments, aborting!\n");
                        return 1;
                    }
                } else {
                    if (!addFileToList(arg)) {
                        printf("An error occurred while processing arguments, aborting!\n");
                        return 1;
                    }
                }
            }
        }
    }
    if (output_filename == NULL) {
        printf("No output filename given, aborting!\n");
        return 1;
    }
    if (verbose) {
        printf("Output header file: %s\n", output_filename);
    }
    FILE *output_file = fopen(output_filename, "wb");
    if (output_file == NULL) {
        printf("Failed to open output file: %s\n", output_filename);
        return 1;
    }
    const char *sanitized_output_filename = sanitizeOutputFilename(output_filename);
    if (sanitized_output_filename == NULL) {
        printf("Failed to write #ifdef\n");
        return 1;
    }
    WRITE(output_file, "// !!! THIS FILE IS MACHINE GENERATED, ANY EDITS WILL BE OVERWRITTEN !!!\n\n");
    WRITE(output_file, "#ifndef _AB_EMBEDDED_RESOURCES_");
    fwrite(sanitized_output_filename, 1, strlen(sanitized_output_filename), output_file);
    WRITE(output_file, "\n#define _AB_EMBEDDED_RESOURCES_");
    fwrite(sanitized_output_filename, 1, strlen(sanitized_output_filename), output_file);
    WRITE(output_file, "\n\n#ifdef EMBED_RESOURCES\n\n");

    snprintf(size_print_buffer, sizeof(size_print_buffer), "%llu", file_list_size);
    WRITE(output_file, "#ifndef _AB_EMBEDDED_RESOURCE_STRUCT_DEFINED\n");
    WRITE(output_file, "#define _AB_EMBEDDED_RESOURCE_STRUCT_DEFINED\n");
    WRITE(output_file, "struct _ab_embedded_resource_t {\n");
    WRITE(output_file, "    const char *file_name;\n");
    WRITE(output_file, "    size_t file_size;\n");
    WRITE(output_file, "    const unsigned char *file_data;\n");
    WRITE(output_file, "};\n");
    WRITE(output_file, "#endif\n\n");
    WRITE(output_file, "static size_t ab_embedded_resource_count_");
    fwrite(sanitized_output_filename, 1, strlen(sanitized_output_filename), output_file);
    WRITE(output_file, " = ");
    fwrite(size_print_buffer, 1, strlen(size_print_buffer), output_file);
    WRITE(output_file, ";\n");

    size_t *file_content_sizes = (size_t *)malloc(file_list_size * sizeof(size_t));
    if (file_content_sizes == NULL) {
        printf("Failed to allocate file content sizes array!\n");
        return 1;
    }

    bool err = false;
    for (size_t i = 0; i < file_list_size; i++) {
        const char *filename = file_list[i];
        const char *filename_top = strrchr(filename, '\\');
        if (filename_top == NULL) {
            filename_top = strrchr(filename, '/');
            if (filename_top == NULL) {
                filename_top = filename;
            } else {
                filename_top += 1;
            }
        } else {
            filename_top += 1;
        }
        if (verbose) {
            printf("Processing %s (%s)\n", filename_top, filename);
        }
        size_t file_size = 0;
        const char *file_content = readFile(filename, &file_size, verbose);
        if (file_content == NULL) {
            err = true;
            break;
        }
        const char *sanitized_content = sanitizeFile(file_content, file_size);
        if (sanitized_content == NULL) {
            err = true;
            break;
        }
        free((void *)file_content);
        size_t sanitized_size = strlen(sanitized_content);
        if (verbose) {
            printf("Sanitized size: %llu\n", sanitized_size);
        }
        file_content_sizes[i] = file_size;

        WRITE(output_file, "static const unsigned char ab_embedded_resources_");
        fwrite(sanitized_output_filename, 1, strlen(sanitized_output_filename), output_file);
        WRITE(output_file, "_content_");
        snprintf(size_print_buffer, sizeof(size_print_buffer), "%llu", i);
        fwrite(size_print_buffer, 1, strlen(size_print_buffer), output_file);
        WRITE(output_file, "[] = {\n    ");
        fwrite(sanitized_content, 1, sanitized_size, output_file);
        WRITE(output_file, "\n};\n");
    }

    WRITE(output_file, "\nstatic struct _ab_embedded_resource_t ab_embedded_resources_");
    fwrite(sanitized_output_filename, 1, strlen(sanitized_output_filename), output_file);
    WRITE(output_file, "[] = {\n");

    for (size_t i = 0; i < file_list_size; i++) {
        const char *filename = file_list[i];
        const char *filename_top = strrchr(filename, '\\');
        if (filename_top == NULL) {
            filename_top = strrchr(filename, '/');
            if (filename_top == NULL) {
                filename_top = filename;
            } else {
                filename_top += 1;
            }
        } else {
            filename_top += 1;
        }
        size_t file_size = file_content_sizes[i];
        snprintf(size_print_buffer, sizeof(size_print_buffer), "%llu", file_size);

        WRITE(output_file, "{\n    \"");
        fwrite(filename_top, 1, strlen(filename_top), output_file);
        WRITE(output_file, "\",\n    ");
        fwrite(size_print_buffer, 1, strlen(size_print_buffer), output_file);
        WRITE(output_file, ",\n    ab_embedded_resources_");
        fwrite(sanitized_output_filename, 1, strlen(sanitized_output_filename), output_file);
        WRITE(output_file, "_content_");
        snprintf(size_print_buffer, sizeof(size_print_buffer), "%llu", i);
        fwrite(size_print_buffer, 1, strlen(size_print_buffer), output_file);
        if (i + 1 < file_list_size) {
            WRITE(output_file, "\n},\n");
        } else {
            WRITE(output_file, "\n}\n");
        }
    }

    free(file_content_sizes);
    WRITE(output_file, "};\n\n");
    WRITE(output_file, "#endif\n#endif");
    fclose(output_file);
    if (err) {
        return 1;
    } else {
        return 0;
    }
}