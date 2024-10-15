
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-3-12.
//

#ifndef CLIBRARY_STR_H
#define CLIBRARY_STR_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif
#include <c/macros.h>

#include <stdio.h>
#include <stdarg.h>

C_BEGIN_EXTERN_C

#define  C_STR_DELIMITERS       "_-|> <."

#define c_ascii_isalnum(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_ALNUM) != 0)
#define c_ascii_isalpha(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_ALPHA) != 0)
#define c_ascii_iscntrl(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_CNTRL) != 0)
#define c_ascii_isdigit(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_DIGIT) != 0)
#define c_ascii_isgraph(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_GRAPH) != 0)
#define c_ascii_islower(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_LOWER) != 0)
#define c_ascii_isprint(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_PRINT) != 0)
#define c_ascii_ispunct(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_PUNCT) != 0)
#define c_ascii_isspace(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_SPACE) != 0)
#define c_ascii_isupper(c)      ((gpAsciiTable[(cuchar) (c)] & C_ASCII_UPPER) != 0)
#define c_ascii_isxdigit(c)     ((gpAsciiTable[(cuchar) (c)] & C_ASCII_XDIGIT) != 0)

#define c_strstrip(str)	        c_strchomp (c_strchug(str))

typedef enum
{
    C_ASCII_ALNUM  = 1 << 0,
    C_ASCII_ALPHA  = 1 << 1,
    C_ASCII_CNTRL  = 1 << 2,
    C_ASCII_DIGIT  = 1 << 3,
    C_ASCII_GRAPH  = 1 << 4,
    C_ASCII_LOWER  = 1 << 5,
    C_ASCII_PRINT  = 1 << 6,
    C_ASCII_PUNCT  = 1 << 7,
    C_ASCII_SPACE  = 1 << 8,
    C_ASCII_UPPER  = 1 << 9,
    C_ASCII_XDIGIT = 1 << 10
} CAsciiType;

C_SYMBOL_PROTECTED extern const cuint16* const gpAsciiTable;

/**
 * @brief ASCII 字母转为小写
 * @return
 */
char                  c_ascii_tolower       (char c) C_CONST;

/**
 * @brief ASCII 字母转为大写
 * @return
 */
char                  c_ascii_toupper       (char c) C_CONST;

/**
 * @brief ASCII 字母转为数字
 * @return 失败返回 -1
 */
int                   c_ascii_digit_value   (char c) C_CONST;

/**
 * @brief ASCII 中十六进制字符转为数字
 * @return 失败返回 -1
 */
int                   c_ascii_xdigit_value  (char c) C_CONST;

/**
 * @brief 字符串忽略大小写比较
 * @return 同 strcmp 返回值
 */
int                   c_ascii_strcasecmp    (const char* s1, const char* s2);

/**
 * @brief 字符串中数字部分转为浮点型
 * @return 失败返回0
 */
double                c_ascii_strtod        (const char* nPtr, char** endPtr);

/**
 * @brief 数字转为字符串
 * @return 返回字符串
 * @note 无须释放资源, char* == buffer
 */
char*                 c_ascii_dtostr        (char* buffer, int bufLen, double d);

/**
 * @brief 字符串转为小写
 * @return 返回转换的字符串
 * @note 须释放资源
 */
char*                 c_ascii_strdown       (const char* str, cuint64 len) C_MALLOC;

/**
 * @brief 字符串转为大写
 * @return 返回转换的字符串
 * @note 须释放资源
 */
char*                 c_ascii_strup         (const char* str, cuint64 len) C_MALLOC;

/**
 * @brief 字符串忽略大小写后比较大小
 * @return 同 strcmp
 */
int                   c_ascii_strncasecmp   (const char* s1, const char* s2, cint64 n);

/**
 * @brief 字符串转无符号整型
 * @return 返回被转换的整型
 */
cuint64               c_ascii_strtoull      (const char* nPtr, char** endPtr, cuint base);

/**
 * @brief 字符串转数字
 * @return 返回被转换的整型数字
 */
cint64                c_ascii_strtoll       (const char* nPtr, char** endPtr, cuint base);

/**
 * @brief 数字(double)转字符串
 * @return 返回被转换的字符串
 * @note 无须释放资源
 */
char*                 c_ascii_formatd       (char* buffer, int bufLen, const char* format, double d);

/**
 * @brief 输出
 */
int                   c_printf              (char const* format, ...) C_PRINTF (1, 2);
int                   c_vprintf             (char const* format, va_list args) C_PRINTF(1, 0);
int                   c_sprintf             (char* str, char const* format, ...) C_PRINTF (2, 3);
int                   c_fprintf             (FILE* file, char const *format, ...) C_PRINTF (2, 3);
int                   c_vsprintf            (char* str, char const* format, va_list args) C_PRINTF(2, 0);
int                   c_vfprintf            (FILE* file, char const* format, va_list args) C_PRINTF(2, 0);
int                   c_vasprintf           (char** str, char const* format, va_list args) C_PRINTF(2, 0);
int                   c_snprintf            (char* str, culong n, char const *format, ...);
int                   c_vsnprintf           (char* str, culong n, char const* format, va_list args);

cuint64               c_printf_string_upper_bound (const char* format, va_list args) C_PRINTF(1, 0);

/**
 * @brief 字符串转为大写
 * @return 返回转换的字符串
 * @note 须释放资源
 */
char*                 c_strup               (const char* str);

/**
 * @brief 去掉字符串前的空格
 * @return 返回去掉前边空格的字符串
 * @note 须释放资源
 */
char*                 c_strchug             (const char* str);

/**
 * @brief 去掉字符串前的空格
 * @note 无须释放资源，针对数组
 */
void                  c_strchug_arr         (char* str);

/**
 * @brief 去掉字符串前面和后面的空格
 * @note 无须释放资源，针对数组
 */
void                  c_strip_arr           (char* str);

/**
 * @brief 去掉字符串后的空格
 * @return 返回去掉后边空格的字符串
 * @note 须释放资源
 */
char*                 c_strchomp            (const char* str);

/**
 * @brief 末尾的空格和换行符
 * @note 无需释放内存
 */
void                  c_strtrim_arr       (char str[]);

/**
 * @brief 计算字符串长度，字符串以 '\0' 结束
 */
cuint64               c_strlen              (const char* str);

/**
 * @brief 去掉字符串后面的空格
 * @note 无须释放资源，针对数组
 */
void                  c_strchomp_arr        (char* str);

/**
 * @brief 字符串反转
 * @return 返回反转后的字符串
 * @note 须释放资源
 */
char*                 c_strreverse          (const char* str);

/**
 * @brief 字符串转小写
 * @return 返回转换为小写的字符串
 * @note 须释放资源
 */
char*                 c_strdown             (const char* str);

/**
 * @brief 检查是否是 ascii 字符串
 * @return
 */
bool                  c_str_is_ascii        (const char* str);

/**
 * @brief 释放字符串数组资源（先释放每个元素资源直到遇到NULL、再释放整个数组资源）
 * @return void
 */
void                  c_strfreev            (char** strArray);

/**
 * @brief 复制字符串数组
 * @return 返回复制后的字符串数组
 * @note 需要使用 c_strfreev 释放资源
 */
char**                c_strdupv             (const char** strArray);

/**
 * @brief 计算字符串数组的长度
 * @return 返回字符串数组的长度
 */
cuint                 c_strv_length         (char** strArray);

/**
 * @brief 计算字符串数组的长度
 * @return 返回字符串数组的长度
 */
cuint                 c_strv_const_length   (const char* const* strArray);

/**
 * @brief 根据错误码，返回描述信息
 * @return 返回描述信息字符串
 */
const char*           c_strerror            (int errNum) C_CONST;

/**
 * @brief 根据信号码返回描述信息
 */
const char*           c_strsignal           (int signum) C_CONST;

/**
 * @brief 复制字符串并返回
 */
char*                 c_strdup              (const char* str) C_MALLOC;

/**
 * @brief 将转义字符替换为等效的单字节字符(处理\\n、\"这种)
 */
char*                 c_strcompress         (const char* source) C_MALLOC;

/**
 * @brief 字符串转 double 数字
 */
double                c_strtod              (const char* nPtr, char** endPtr);

/**
 * @brief 复制字符串前N个字节
 * @return 返回复制后的字符串
 * @note 需要释放资源
 */
char*                 c_strndup             (const char* str, cuint64 n) C_MALLOC;

/**
 * @brief 申请 length 长度字符串，并用 fillChar 填充申请的每个字节
 * @return 返回申请的内存
 * @note 需要释放资源
 */
char*                 c_strnfill            (cint64 length, char fillChar) C_MALLOC;

/**
 * @brief 查找子串
 * @note 无须释放资源
 */
char*                 c_strrstr             (const char* haystack, const char* needle);

/**
 * 查找子串，从左到右
 * @note 无须释放资源
 */
char*                 c_strstr              (const char* haystack, const char* needle);

/**
 * @brief 复制指定长度的字符串到 dest
 * @return 返回src剩余未复制到dest的字节数
 */
cuint64               c_strlcpy             (char* dest, const char* src, cuint64 destSize);

/**
 * @brief 将 src 字符串追加到 dest
 * @return 返回追加后字符串总长度
 */
cuint64               c_strlcat             (char* dest, const char* src, cuint64 destSize);

/**
 * @brief 将字符串数组拼接为一个长字符串
 * @return 返回拼接后的字符串
 * @note 需要释放资源
 */
char*                 c_strjoinv            (const char* separator, char** strArray) C_MALLOC;

/**
 * @brief 追加字符串
 * @return 返回拼接后的字符串
 * @note 需要释放资源
 */
char*                 c_strconcat           (const char* str1, ...) C_MALLOC C_NULL_TERMINATED;

/**
 * @brief 格式化字符串
 * @note 需要释放资源
 */
char*                 c_strdup_printf       (const char* format, ...) C_PRINTF (1, 2) C_MALLOC;

/**
 * @brief 字符串转为转义后的字符串
 * @note 需要释放资源
 */
char*                 c_strescape           (const char* source, const char* exceptions) C_MALLOC;

/**
 * @brief 处理字符串，将 str 中不在 validChars 中的字符替换为 substitutor
 * @note 需要释放资源
 */
char*                 c_strcanon            (const char* str, const char* validChars, char substitutor);

/**
 * @brief 处理字符串，将 str 中指定 delimiters 字符替换为 newDelimiter 字符
 * @note 需要释放资源
 */
char*                 c_strdelimit          (const char* str, const char* delimiters, char newDelimiter);

/**
 * @brief 将所有子串用 separator 分隔符连接
 * @note 需要释放资源
 */
char*                 c_strjoin             (const char* separator, ...) C_MALLOC C_NULL_TERMINATED;

/**
 * @brief 将字符串切割为字符串数组
 * @note 需要释放资源
 */
char**                c_strsplit            (const char* str, const char* delimiter, int maxTokens);

/**
 * @brief 字符串切割，delimiters 指定多个切割字符
 * @note 需要释放资源
 */
char**                c_strsplit_set        (const char* str, const char* delimiters, int maxTokens);

/**
 * @brief 复制字符串
 * @note 需要释放资源
 */
char*                 c_strdup_vprintf      (const char* format, va_list args) C_PRINTF(1, 0) C_MALLOC;
char*                 c_strrstr_len         (const char* haystack, cint64 haystackLen, const char* needle);
char*                 c_strstr_len          (const char* haystack, cint64 haystackLen, const char* needle);

bool                  c_str_has_suffix      (const char* str, const char* suffix);
bool                  c_str_has_prefix      (const char* str, const char* prefix);

C_END_EXTERN_C

#endif //CLIBRARY_STR_H
