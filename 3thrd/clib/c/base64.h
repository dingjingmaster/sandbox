
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-3-18.
//

#ifndef CLIBRARY_BASE64_H
#define CLIBRARY_BASE64_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif
#include <c/macros.h>

C_BEGIN_EXTERN_C

cuint64 c_base64_encode_step    (const cuchar* in, cuint64 len, bool breakLines, char* out, int* state, int* save);
cuint64 c_base64_encode_close   (bool breakLines, char* out, int* state, int* save);
char*   c_base64_encode         (const cuchar* data, cuint64 len) C_MALLOC;
cuint64 c_base64_decode_step    (const char* in, cuint64 len, cuchar* out, int* state, cuint* save);
cuchar* c_base64_decode         (const char* text, cuint64* outLen) C_MALLOC;
cuchar* c_base64_decode_inplace (char* text, cuint64* outLen);

C_END_EXTERN_C

#endif //CLIBRARY_BASE64_H
