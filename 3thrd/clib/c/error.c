
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

#include "error.h"

CQuark c_error_domain_register_static (const char* errorTypeName, csize errorTypePrivateSize, CErrorInitFunc errorTypeInit, CErrorCopyFunc errorTypeCopy, CErrorClearFunc errorTypeClear)
{}

CQuark c_error_domain_register (const char* errorTypeName, csize errorTypePrivateSize, CErrorInitFunc errorTypeInit, CErrorCopyFunc errorTypeCopy, CErrorClearFunc errorTypeClear)
{}

CError* c_error_new (CQuark domain, int code, const char* format, ...)
{}

CError* c_error_new_literal (CQuark domain, int code, const char* message)
{}

CError* c_error_new_valist (CQuark domain, int code, const char* format, va_list args)
{}

void c_error_free (CError* error)
{}

CError* c_error_copy (const CError* error)
{}

bool c_error_matches (const CError* error, CQuark domain, int code)
{}

void c_set_error (CError** err, CQuark domain, int code, const char* format, ...)
{}

void c_set_error_literal (CError** err, CQuark domain, int code, const char* message)
{}

void c_propagate_error (CError** dest, CError* src)
{}

void c_clear_error (CError** err)
{}

void c_prefix_error (CError** err, const char* format, ...)
{}

void c_prefix_error_literal (CError** err, const char* prefix)
{}

void c_propagate_prefixed_error (CError** dest, CError* src, const char* format, ...)
{}

