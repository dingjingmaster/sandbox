
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

#ifndef CLIBRARY_ERROR_H
#define CLIBRARY_ERROR_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/quark.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C

typedef struct _CError              CError;

struct _CError
{
    CQuark              domain;
    cint                code;
    char*               message;
};

#define C_DEFINE_EXTENDED_ERROR(ErrorType, error_type) \
static inline ErrorType ## Private* error_type ## _get_private (const CError* error) \
{ \
    const csize sa = 2 * sizeof (csize); \
    const csize as = (sizeof (ErrorType ## Private) + (sa - 1)) & -sa; \
    c_return_val_if_fail (error != NULL, NULL); \
    c_return_val_if_fail (error->domain == error_type ## _quark (), NULL); \
    return (ErrorType ## Private*) (((cuint8*) error) - as); \
} \
\
static void c_error_with_ ## error_type ## _private_init (CError* error) \
{ \
    ErrorType ## Private* priv = error_type ## _get_private (error); \
    error_type ## _private_init (priv); \
} \
\
static void c_error_with_ ## error_type ## _private_copy (const CError* srcError, CError* destError) \
{ \
    const ErrorType ## Private* srcPriv = error_type ## _get_private (srcError); \
    ErrorType ## Private* destPriv = error_type ## _get_private (destError); \
    error_type ## _private_copy (srcPriv, destPriv); \
} \
\
static void c_error_with_ ## error_type ## _private_clear (CError* error) \
{ \
    ErrorType ## Private *priv = error_type ## _get_private (error); \
    error_type ## _private_clear (priv); \
} \
\
CQuark error_type ## _quark (void) \
{ \
    static CQuark q; \
    static csize initialized = 0; \
    if (c_once_init_enter (&initialized)) { \
        q = c_error_domain_register_static (#ErrorType, sizeof (ErrorType ## Private), \
                                            c_error_with_ ## error_type ## _private_init, \
                                            c_error_with_ ## error_type ## _private_copy, \
                                            c_error_with_ ## error_type ## _private_clear); \
        c_once_init_leave (&initialized, 1); \
    } \
\
    return q; \
}
typedef void (*CErrorInitFunc)  (CError* error);
typedef void (*CErrorClearFunc) (CError* error);
typedef void (*CErrorCopyFunc)  (const CError* srcError, CError* destError);

CQuark      c_error_domain_register_static  (const char* errorTypeName, csize errorTypePrivateSize, CErrorInitFunc errorTypeInit, CErrorCopyFunc errorTypeCopy, CErrorClearFunc errorTypeClear);
CQuark      c_error_domain_register         (const char* errorTypeName, csize errorTypePrivateSize, CErrorInitFunc errorTypeInit, CErrorCopyFunc errorTypeCopy, CErrorClearFunc errorTypeClear);
CError*     c_error_new                     (CQuark domain, int code, const char* format, ...) C_PRINTF (3, 4);
CError*     c_error_new_literal             (CQuark domain, int code, const char* message);
CError*     c_error_new_valist              (CQuark domain, int code, const char* format, va_list args) C_PRINTF(3, 0);
void        c_error_free                    (CError* error);
CError*     c_error_copy                    (const CError* error);
bool        c_error_matches                 (const CError* error, CQuark domain, int code);
void        c_set_error                     (CError** err, CQuark domain, int code, const char* format, ...) C_PRINTF (4, 5);
void        c_set_error_literal             (CError** err, CQuark domain, int code, const char* message);
void        c_propagate_error               (CError** dest, CError* src);
void        c_clear_error                   (CError** err);
void        c_prefix_error                  (CError** err, const char* format, ...) C_PRINTF (2, 3);
void        c_prefix_error_literal          (CError** err, const char* prefix);
void        c_propagate_prefixed_error      (CError** dest, CError* src, const char* format, ...) C_PRINTF (3, 4);


C_END_EXTERN_C


#endif //CLIBRARY_ERROR_H
