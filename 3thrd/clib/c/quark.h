
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-19.
//

#ifndef CLIBRARY_QUARK_H
#define CLIBRARY_QUARK_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>


typedef cuint32             CQuark;

#define C_DEFINE_QUARK(QN, q_n) \
CQuark q_n##_quark (void) \
{ \
    static CQuark q; \
    if (C_UNLIKELY (q == 0)) { \
        q = c_quark_from_static_string (#QN); \
    } \
    return q; \
}


CQuark      c_quark_try_string          (const char* string);
CQuark      c_quark_from_static_string  (const char* string);
CQuark      c_quark_from_string         (const char* string);
const char* c_quark_to_string           (CQuark quark) C_CONST;

const char* c_intern_string         (const char* string);
const char* c_intern_static_string  (const char* string);



#endif //CLIBRARY_QUARK_H
