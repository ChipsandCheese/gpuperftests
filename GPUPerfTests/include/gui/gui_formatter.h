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

#ifndef GUI_FORMATTER_H
#define GUI_FORMATTER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gui_formatted_segment_type_t {
    gui_formatted_segment_type_normal = 0,
    gui_formatted_segment_type_bold,
    gui_formatted_segment_type_italics,
    gui_formatted_segment_type_monospace,
    gui_formatted_segment_type_heading,
    gui_formatted_segment_type_separator,
    gui_formatted_segment_type_eol
} gui_formatted_segment_type;

typedef struct gui_formatted_segment_t {
    gui_formatted_segment_type type;
    const char *content;
    bool same_line;
} gui_formatted_segment;

typedef struct gui_formatted_string_t {
    helper_arraylist segments;
} gui_formatted_string;

test_status GuiFormatterFormatString(const char *string, gui_formatted_string **formatted_string);
void GuiFormatterRenderString(gui_formatted_string *formatted_string, ImFont *normal_font, ImFont *bold_font, ImFont *italics_font, ImFont *monospace_font, ImFont *header_font);

#ifdef __cplusplus
}
#endif
#endif