
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-24.
//

#ifndef CLIBRARY_CSTRING_H
#define CLIBRARY_CSTRING_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

C_BEGIN_EXTERN_C

typedef struct _CString         CString;

struct _CString
{
    char*       str;
    csize       len;
    csize       allocatedLen;
};


CString*        c_string_new                (const char* init);
CString*        c_string_new_len            (const char* init, cssize len);
CString*        c_string_sized_new          (csize dflSize);
char*           c_string_free               (CString* str, bool freeSegment);
CBytes*         c_string_free_to_bytes      (CString* str);
bool            c_string_equal              (const CString* v, const CString* v2);
cuint           c_string_hash               (const CString* str);
CString*        c_string_assign             (CString* str, const char* rVal);
CString*        c_string_truncate           (CString* str, csize len);
CString*        c_string_set_size           (CString* str, csize len);
CString*        c_string_insert_len         (CString* str, cssize pos, const char* val, cssize len);
CString*        c_string_append             (CString* str, const char* val);
CString*        c_string_append_len         (CString* str, const char* val, cssize len);
CString*        c_string_append_c           (CString* str, char c);
CString*        c_string_append_unichar     (CString* str, cunichar wc);
CString*        c_string_prepend            (CString* str, const char* val);
CString*        c_string_prepend_c          (CString* str, char c);
CString*        c_string_prepend_unichar    (CString* str, cunichar wc);
CString*        c_string_prepend_len        (CString* str, const char* val, cssize len);
CString*        c_string_insert             (CString* str, cssize pos, const char* val);
CString*        c_string_insert_c           (CString* str, cssize pos, char c);
CString*        c_string_insert_unichar     (CString* str, cssize pos, cunichar wc);
CString*        c_string_overwrite          (CString* str, csize pos, const char* val);
CString*        c_string_overwrite_len      (CString* str, csize pos, const char* val, cssize len);
CString*        c_string_erase              (CString* str, cssize pos, cssize len);
cuint           c_string_replace            (CString* str, const char* find, const char* replace, cuint limit);
CString*        c_string_ascii_down         (CString* str);
CString*        c_string_ascii_up           (CString* str);
void            c_string_vprintf            (CString* str, const char* format, va_list args) C_PRINTF(2, 0);
void            c_string_printf             (CString* str, const char* format, ...) C_PRINTF (2, 3);
void            c_string_append_vprintf     (CString* str, const char* format, va_list args) C_PRINTF(2, 0);
void            c_string_append_printf      (CString* str, const char* format, ...) C_PRINTF (2, 3);
CString*        c_string_append_uri_escaped (CString* str, const char* unescaped, const char* reservedCharsAllowed, bool allowUtf8);
CString*        c_string_down               (CString* str);
CString*        c_string_up                 (CString* str);

C_END_EXTERN_C

#endif //CLIBRARY_CSTRING_H
