
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-25.
//

#ifndef CLIBRARY_CONVERT_H
#define CLIBRARY_CONVERT_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/error.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C

typedef struct _CIConv      *CIConv;

typedef enum
{
    C_CONVERT_ERROR_NO_CONVERSION,
    C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
    C_CONVERT_ERROR_FAILED,
    C_CONVERT_ERROR_PARTIAL_INPUT,
    C_CONVERT_ERROR_BAD_URI,
    C_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
    C_CONVERT_ERROR_NO_MEMORY,
    C_CONVERT_ERROR_EMBEDDED_NUL
} CConvertError;

#define C_CONVERT_ERROR     c_convert_error_quark()

CQuark c_convert_error_quark (void);

CIConv c_iconv_open   (const char* toCodeset, const char* fromCodeset);
csize  c_iconv        (CIConv converter, char** inBuf, csize* inBytesLeft, char** outbuf, csize* outBytesLeft);
cint   c_iconv_close  (CIConv converter);
char*  c_convert      (const char* str, cssize len, const char* toCodeset, const char* fromCodeset, csize* bytesRead, csize* bytesWritten, CError** error) C_MALLOC;
char*  c_convert_with_iconv (const char* str, cssize len, CIConv converter, csize* bytesRead, csize* bytesWritten, CError** error) C_MALLOC;
char*  c_convert_with_fallback (const char* str, cssize len, const char* toCodeset, const char* fromCodeset, const char* fallback, csize* bytesRead, csize* bytesWritten, CError** error) C_MALLOC;
char*  c_locale_to_utf8   (const char* opSysString, cssize len, csize* bytesRead, csize* bytesWritten, CError** error) C_MALLOC;
char*  c_locale_from_utf8 (const char* utf8string, cssize len, csize* bytesRead, csize* bytesWritten, CError** error) C_MALLOC;
char*  c_filename_to_utf8   (const char* opSysString, cssize len, csize* bytesRead, csize* bytesWritten, CError** error) C_MALLOC;
char*  c_filename_from_utf8 (const char* utf8string, cssize len, csize* bytesRead, csize* bytesWritten, CError** error) C_MALLOC;
char*  c_filename_from_uri (const char* uri, char** hostname, CError** error) C_MALLOC;
char*  c_filename_to_uri   (const char* filename, const char* hostname, CError** error) C_MALLOC;
char*  c_filename_display_name (const char* filename) C_MALLOC;
bool   c_get_filename_charsets (const char*** filenameCharsets);
char*  c_filename_display_basename (const char* filename) C_MALLOC;
char** c_uri_list_extract_uris (const char* uriList);


C_END_EXTERN_C


#endif //CLIBRARY_CONVERT_H
