
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

#ifndef CLIBRARY_FILE_UTILS_H
#define CLIBRARY_FILE_UTILS_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <stdio.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C

#define C_FILE_ERROR c_file_error_quark()


CQuark      c_file_error_quark          (void);
CFileError  c_file_error_from_errno     (int errNo);
bool        c_file_test                 (const char* filename, CFileTest test);
bool        c_file_get_contents         (const char* filename, char** contents, csize* length, CError** error);
bool        c_file_set_contents         (const char* filename, const char* contents, cssize length, CError** error);
bool        c_file_set_contents_full    (const char* filename, const char* contents, cssize length, CFileSetContentsFlags flags, int mode, CError** error);
char*       c_file_read_link            (const char* filename, CError** error);
char*       c_mkdtemp                   (char* tmpl);
char*       c_mkdtemp_full              (char* tmpl, int mode);
cint        c_mkstemp                   (char* tmpl);
cint        c_mkstemp_full              (char* tmpl, cint flags, cint mode);
cint        c_file_open_tmp             (const char* tmpl, char** nameUsed, CError** error);
char*       c_dir_make_tmp              (const char* tmpl, CError** error);
char*       c_build_path                (const char* separator, const char* firstElement, ...) C_MALLOC C_NULL_TERMINATED;
char*       c_build_pathv               (const char* separator, char** args) C_MALLOC;
char*       c_build_filename            (const char* firstElement, ...) C_MALLOC C_NULL_TERMINATED;
char*       c_build_filenamev           (char** args) C_MALLOC;
char*       c_build_filename_valist     (const char* firstElement, va_list* args) C_MALLOC;
int         c_mkdir_with_parents        (const char* pathname, cint mode);
bool        c_path_is_absolute          (const char* fileName);
const char* c_path_skip_root            (const char* fileName);
const char* c_basename                  (const char* fileName);
char*       c_get_current_dir           (void);
char*       c_path_get_basename         (const char* fileName) C_MALLOC;
char*       c_path_get_dirname          (const char* fileName) C_MALLOC;
char*       c_canonicalize_filename     (const char* filename, const char* relativeTo) C_MALLOC;

/**
 * @brief 读取一行到数组中
 * @return 返回读取的字节数
 */
cuint64     c_file_read_line_arr        (FILE* fr, char lineBuf[], cuint64 bufLen);

/**
 * @brief 格式化路径，去掉路径中多余字符
 * @return 返回格式化的字符串
 * @note 无需释放结果
 */
char*       c_file_path_format_arr      (char pathBuf[]);


C_END_EXTERN_C

#endif //CLIBRARY_FILE_UTILS_H
