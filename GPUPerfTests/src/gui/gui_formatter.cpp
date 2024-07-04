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
#include "logger.h"
#include <imgui.h>
#include "gui/gui_formatter.h"

static inline bool _GuiFormatterIsCharText(char c) {
    if (c >= '0' && c <= '9') {
        return true;
    } else if (c >= 'A' && c <= 'Z') {
        return true;
    } else if (c >= 'a' && c <= 'z') {
        return true;
    } else if (c == '*') {
        return true;
    } else {
        return c == '\0';
    }
}

test_status GuiFormatterFormatString(const char *string, gui_formatted_string **formatted_string) {
    if (string == NULL || formatted_string == NULL) {
        return TEST_INVALID_PARAMETER;
    }
    gui_formatted_string *formatted = (gui_formatted_string *)malloc(sizeof(gui_formatted_string));
    if (formatted == NULL) {
        return TEST_OUT_OF_MEMORY;
    }
    memset(formatted, 0, sizeof(gui_formatted_string));
    test_status status = HelperArrayListInitialize(&(formatted->segments), sizeof(gui_formatted_segment));
    if (!TEST_SUCCESS(status)) {
        free(formatted);
        return status;
    }
    const char *current_line = string;
    const char *next_line = NULL;
    bool end_of_file = false;

    while (!end_of_file) {
        next_line = strchr(current_line, '\n');
        if (next_line == NULL) {
            next_line = string + strlen(string);
            end_of_file = true;
        }
        if (next_line != NULL) {
            gui_formatted_segment segment;
            HelperClear(segment);
            size_t line_length = next_line - current_line;
            if (line_length > 0) {
                if (current_line[line_length - 1] == '\r') {
                    line_length--;
                }
                if (line_length > 0) {
                    if (current_line[0] == '#' && current_line[1] == ' ') {
                        // heading
                        char *line_buffer = (char *)malloc(line_length - 2 + 1);
                        if (line_buffer == NULL) {
                            HelperArrayListClean(&(formatted->segments));
                            free(formatted);
                            return TEST_OUT_OF_MEMORY;
                        }
                        strncpy(line_buffer, current_line + 2, line_length - 2);
                        segment.same_line = false;
                        segment.type = gui_formatted_segment_type_heading;
                        line_buffer[line_length - 2] = '\0';
                        segment.content = line_buffer;

                        status = HelperArrayListAdd(&(formatted->segments), &segment, sizeof(segment), NULL);
                        if (!TEST_SUCCESS(status)) {
                            HelperArrayListClean(&(formatted->segments));
                            free(formatted);
                            return status;
                        }
                        segment.type = gui_formatted_segment_type_separator;
                        segment.content = NULL;

                        status = HelperArrayListAdd(&(formatted->segments), &segment, sizeof(segment), NULL);
                        if (!TEST_SUCCESS(status)) {
                            HelperArrayListClean(&(formatted->segments));
                            free(formatted);
                            return status;
                        }
                    } else if (current_line[0] == '-' && current_line[1] == '-' && current_line[2] == '-') {
                        segment.type = gui_formatted_segment_type_separator;
                        segment.content = NULL;
                        status = HelperArrayListAdd(&(formatted->segments), &segment, sizeof(segment), NULL);
                        if (!TEST_SUCCESS(status)) {
                            HelperArrayListClean(&(formatted->segments));
                            free(formatted);
                            return status;
                        }
                    } else {
                        segment.type = gui_formatted_segment_type_normal;
                        bool is_first_segment = true;
                        char last_char = '\0';
                        const char *segment_string_start = current_line;
                        const char *segment_string_end = segment_string_start;
                        gui_formatted_segment_type current_style_type = segment.type;

                        for (size_t char_index = 0; char_index < line_length + 1; char_index++) {
                            char current_char = '\0';
                            if (char_index < line_length) {
                                current_char = current_line[char_index];
                            } else {
                                current_style_type = gui_formatted_segment_type_eol;
                            }
                            char next_char = '\0';
                            if (char_index + 1 < line_length) {
                                next_char = current_line[char_index + 1];
                            }

                            if (current_char == '*' && next_char == '*') {
                                // upcoming bold, don't output the * char
                            } else if (current_char == '*' && last_char == '*') {
                                // bold
                                if (current_style_type == gui_formatted_segment_type_bold) {
                                    current_style_type = gui_formatted_segment_type_normal;
                                } else {
                                    current_style_type = gui_formatted_segment_type_bold;
                                }
                            } else if (current_char == '_' && !_GuiFormatterIsCharText(last_char)) {
                                // italics
                                current_style_type = gui_formatted_segment_type_italics;
                            } else if (current_char == '`' && !_GuiFormatterIsCharText(last_char)) {
                                // monospace
                                current_style_type = gui_formatted_segment_type_monospace;
                            } else if (current_char == '_' && (!_GuiFormatterIsCharText(next_char) || next_char == '\0')) {
                                // italics
                                current_style_type = gui_formatted_segment_type_normal;
                            } else if (current_char == '`' && (!_GuiFormatterIsCharText(next_char) || next_char == '\0')) {
                                // monospace
                                current_style_type = gui_formatted_segment_type_normal;
                            } else {
                                if (segment.type != current_style_type) {
                                    size_t segment_length = segment_string_end - segment_string_start;
                                    if (segment_length > 0) {
                                        char *line_buffer = (char *)malloc(segment_length + 1);
                                        if (line_buffer == NULL) {
                                            HelperArrayListClean(&(formatted->segments));
                                            free(formatted);
                                            return TEST_OUT_OF_MEMORY;
                                        }
                                        strncpy(line_buffer, segment_string_start, segment_length);
                                        line_buffer[segment_length] = '\0';
                                        segment.same_line = !is_first_segment;
                                        segment.content = line_buffer;

                                        status = HelperArrayListAdd(&(formatted->segments), &segment, sizeof(segment), NULL);
                                        if (!TEST_SUCCESS(status)) {
                                            HelperArrayListClean(&(formatted->segments));
                                            free(formatted);
                                            return status;
                                        }
                                        is_first_segment = false;
                                    }
                                    segment.type = current_style_type;
                                    segment_string_start = &(current_line[char_index]);
                                    segment_string_end = segment_string_start;
                                }
                                segment_string_end++;
                            }
                            last_char = current_char;
                        }
                    }
                }
            }
            if (line_length == 0) {
                // empty line
                segment.content = NULL;
                segment.same_line = false;
                segment.type = gui_formatted_segment_type_normal;

                status = HelperArrayListAdd(&(formatted->segments), &segment, sizeof(segment), NULL);
                if (!TEST_SUCCESS(status)) {
                    HelperArrayListClean(&(formatted->segments));
                    free(formatted);
                    return status;
                }
            }
            next_line++;
        }
        current_line = next_line;
    }
    *formatted_string = formatted;
    return TEST_OK;
}

void GuiFormatterRenderString(gui_formatted_string *formatted_string, ImFont *normal_font, ImFont *bold_font, ImFont *italics_font, ImFont *monospace_font, ImFont *header_font) {
    if (formatted_string != NULL) {
        ImGuiStyle &style = ImGui::GetStyle();

        size_t segment_count = HelperArrayListSize(&(formatted_string->segments));
        for (size_t i = 0; i < segment_count; i++) {
            gui_formatted_segment *segment = (gui_formatted_segment *)HelperArrayListGet(&(formatted_string->segments), i);

            if (segment->type == gui_formatted_segment_type_separator) {
                ImGui::Separator();
            } else {
                if (segment->same_line) {
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - style.ItemSpacing.x);
                }
                if (segment->content == NULL) {
                    ImGui::Text("");
                } else {
                    switch (segment->type) {
                    case gui_formatted_segment_type_italics:
                        ImGui::PushFont(italics_font);
                        break;
                    case gui_formatted_segment_type_bold:
                        ImGui::PushFont(bold_font);
                        break;
                    case gui_formatted_segment_type_monospace:
                        ImGui::PushFont(monospace_font);
                        break;
                    case gui_formatted_segment_type_heading:
                        ImGui::PushFont(header_font);
                        break;
                    default:
                        ImGui::PushFont(normal_font);
                        break;
                    }
                    const char* text = segment->content;
                    const char* text_end = text + strlen(text);
                    float scale = ImGui::GetIO().FontGlobalScale;
                    while (text < text_end) {
                        const char* wrap_point = ImGui::GetFont()->CalcWordWrapPositionA(scale, text, text + strlen(text), ImGui::GetContentRegionAvail().x);
                        ImGui::TextUnformatted(text, wrap_point);
                        text = wrap_point;
                    }
                    ImGui::PopFont();
                }
            }
        }
    }
}