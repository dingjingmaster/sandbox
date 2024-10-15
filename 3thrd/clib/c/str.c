
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
#include "str.h"

#include <ctype.h>

#include "log.h"
#include "array.h"


static char* c_stpcpy (char* dest, const char* src);
static cuint64 c_parse_long_long(const char* nPtr, const char** endPtr, cuint base, bool* negative);


static const cuint16 gsAsciiTableData[256] =
{
    0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
    0x004, 0x104, 0x104, 0x004, 0x104, 0x104, 0x004, 0x004,
    0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
    0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004, 0x004,
    0x140, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
    0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
    0x459, 0x459, 0x459, 0x459, 0x459, 0x459, 0x459, 0x459,
    0x459, 0x459, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
    0x0d0, 0x653, 0x653, 0x653, 0x653, 0x653, 0x653, 0x253,
    0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253,
    0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253, 0x253,
    0x253, 0x253, 0x253, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x0d0,
    0x0d0, 0x473, 0x473, 0x473, 0x473, 0x473, 0x473, 0x073,
    0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073,
    0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073, 0x073,
    0x073, 0x073, 0x073, 0x0d0, 0x0d0, 0x0d0, 0x0d0, 0x004
    /* the upper 128 are all zeroes */
};

C_SYMBOL_PROTECTED const cuint16* const gpAsciiTable = gsAsciiTableData;

char c_ascii_tolower (char c)
{
    return c_ascii_isupper(c) ? TOLOWER(c) : c;
}

char c_ascii_toupper (char c)
{
    return c_ascii_islower(c) ? TOUPPER(c) : c;
}

int c_ascii_digit_value (char c)
{
    if (c_ascii_isdigit (c)) {
        return c - '0';
    }

    return -1;
}

int c_ascii_xdigit_value (char c)
{
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    return c_ascii_digit_value (c);
}

int c_ascii_strcasecmp (const char* s1, const char* s2)
{
    int c1, c2;

    c_return_val_if_fail (s1 != NULL, 0);
    c_return_val_if_fail (s2 != NULL, 0);

    while (*s1 && *s2) {
        c1 = (int) (cuchar) TOLOWER (*s1);
        c2 = (int) (cuchar) TOLOWER (*s2);
        if (c1 != c2) {
            return (c1 - c2);
        }

        s1++; s2++;
    }

    return (((int) (cuchar) *s1) - ((int) (cuchar) *s2));
}

double c_ascii_strtod (const char* nPtr, char** endPtr)
{
    double val;
    int strtodErrno;
    char* failPos = NULL;
    const char* p = NULL;
    const char* end = NULL;
    cuint64 decimalPointLen = 1;
    const char* decimalPoint = ".";
    const char* decimalPointPos = NULL;

    c_return_val_if_fail(nPtr != NULL, 0);

    c_assert(decimalPointLen != 0);

    if ((decimalPoint[0] != '.') || (decimalPoint[1] != 0)) {
        p = nPtr;
        while (c_ascii_isspace (*p)) {
            p++;
        }

        if (*p == '+' || *p == '-') {
            p++;
        }

        // 十六进制
        if ((p[0] == '0') && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
            while (c_ascii_isxdigit (*p)) {
                p++;
            }

            if (*p == '.') {
                decimalPointPos = p++;
            }

            while (c_ascii_isxdigit (*p)) {
                p++;
            }

            if (*p == 'p' || *p == 'P') {
                p++;
            }
            if (*p == '+' || *p == '-') {
                p++;
            }
            while (c_ascii_isdigit (*p)) {
                p++;
            }

            end = p;
        }
        else if (c_ascii_isdigit (*p) || (*p == '.')) {
            while (c_ascii_isdigit (*p)) {
                p++;
            }

            if (*p == '.') {
                decimalPointPos = p++;
            }

            while (c_ascii_isdigit (*p)) {
                p++;
            }

            if (*p == 'e' || *p == 'E') {
                p++;
            }
            if (*p == '+' || *p == '-') {
                p++;
            }
            while (c_ascii_isdigit (*p)) {
                p++;
            }

            end = p;
        }
    }

    if (decimalPointPos) {
        char* c = NULL;
        char* copy = NULL;

        c_malloc(copy, end - nPtr + 1 + decimalPointLen);

        c = copy;
        memcpy (c, nPtr, decimalPointPos - nPtr);
        c += decimalPointPos - nPtr;
        memcpy (c, decimalPoint, decimalPointLen);
        c += decimalPointLen;
        memcpy (c, decimalPointPos + 1, end - (decimalPointPos + 1));
        c += end - (decimalPointPos + 1);
        *c = 0;

        errno = 0;
        val = strtod (copy, &failPos);
        strtodErrno = errno;

        if (failPos) {
            if (failPos - copy > decimalPointPos - nPtr) {
                failPos = (char*) nPtr + (failPos - copy) - (decimalPointLen - 1);
            }
            else {
                failPos = (char*) nPtr + (failPos - copy);
            }
        }

        c_free(copy);
    }
    else if (end) {
        char *copy = NULL;
        c_malloc (copy, end - (char*) nPtr + 1);
        memcpy (copy, nPtr, end - nPtr);
        *(copy + (end - (char*) nPtr)) = 0;

        errno = 0;
        val = strtod (copy, &failPos);
        strtodErrno = errno;

        if (failPos) {
            failPos = (char*) nPtr + (failPos - copy);
        }

        c_free (copy);
    }
    else {
        errno = 0;
        val = strtod (nPtr, &failPos);
        strtodErrno = errno;
    }

    if (endPtr) {
        *endPtr = failPos;
    }

    errno = strtodErrno;

    return val;
}

char* c_ascii_dtostr (char* buffer, int bufLen, double d)
{
    return c_ascii_formatd (buffer, bufLen, "%.17g", d);
}

char* c_ascii_strdown (const char* str, cuint64 len)
{
    char *result, *s;

    c_return_val_if_fail (str != NULL, NULL);

    if (len < 0) {
        len = (cuint64) strlen (str);
    }

    result = c_strndup (str, (cuint64) len);
    for (s = result; *s; s++) {
        *s = c_ascii_tolower (*s);
    }

    return result;
}

char* c_ascii_strup (const char* str, cuint64 len)
{
    char *result, *s;

    c_return_val_if_fail (str != NULL, NULL);

    if (len < 0) {
        len = (cuint64) strlen (str);
    }

    result = c_strndup (str, (cuint64) len);
    for (s = result; *s; s++) {
        *s = c_ascii_toupper (*s);
    }

    return result;
}

int c_ascii_strncasecmp (const char* s1, const char* s2, cint64 n)
{
    int c1, c2;

    c_return_val_if_fail (s1 != NULL, 0);
    c_return_val_if_fail (s2 != NULL, 0);

    while (n && *s1 && *s2) {
        n -= 1;
        c1 = (int) (cuchar) TOLOWER (*s1);
        c2 = (int) (cuchar) TOLOWER (*s2);
        if (c1 != c2) {
            return (c1 - c2);
        }
        s1++; s2++;
    }

    if (n) {
        return (((int) (cuchar) *s1) - ((int) (cuchar) *s2));
    }

    return 0;
}

cuint64 c_ascii_strtoull (const char* nPtr, char** endPtr, cuint base)
{
    bool negative;
    cuint64 result;

    result = c_parse_long_long (nPtr, (const char**) endPtr, base, &negative);

    return negative ? -result : result;
}

cint64 c_ascii_strtoll (const char* nPtr, char** endPtr, cuint base)
{
    bool negative;
    cuint64 result;

    result = c_parse_long_long (nPtr, (const char **) endPtr, base, &negative);

    if (negative && result > (cuint64) C_MIN_INT64) {
        errno = ERANGE;
        return C_MIN_INT64;
    }
    else if (!negative && result > (cuint64) C_MAX_INT64) {
        errno = ERANGE;
        return C_MAX_INT64;
    }
    else if (negative) {
        return - (cint64) result;
    }
    else {
        return (cint64) result;
    }
}

char* c_ascii_formatd (char* buffer, int bufLen, const char* format, double d)
{
    char* p = NULL;
    char formatChar;
    cuint64 restLen = 0;
    cuint64 decimalPointLen = 1;
    const char* decimalPoint = ".";

    c_return_val_if_fail (buffer != NULL, NULL);
    c_return_val_if_fail (format[0] == '%', NULL);
    c_return_val_if_fail (strpbrk (format + 1, "'l%") == NULL, NULL);

    formatChar = format[strlen (format) - 1];

    c_return_val_if_fail (formatChar == 'e' || formatChar == 'E'
                        || formatChar == 'f' || formatChar == 'F'
                        || formatChar == 'g' || formatChar == 'G',
                        NULL);

    if (format[0] != '%') {
        return NULL;
    }

    if (strpbrk (format + 1, "'l%")) {
        return NULL;
    }

//    if (!(formatChar == 'e' || formatChar == 'E'
//        || formatChar == 'f' || formatChar == 'F'
//        || formatChar == 'g' || formatChar == 'G')) {
//        return NULL;
//    }

    _c_snprintf (buffer, bufLen, format, d);


    c_assert (decimalPointLen != 0);

    if (decimalPoint[0] != '.' || decimalPoint[1] != 0) {
        p = buffer;
        while (c_ascii_isspace (*p)) {
            p++;
        }

        if (*p == '+' || *p == '-') {
            p++;
        }

        while (isdigit ((cuchar)*p)) {
            p++;
        }

        if (strncmp (p, decimalPoint, decimalPointLen) == 0) {
            *p = '.';
            p++;
//            if (decimalPointLen > 1) {
//                restLen = strlen (p + (decimalPointLen - 1));
//                memmove (p, p + (decimalPointLen - 1), restLen);
//                p[restLen] = 0;
//            }
        }
    }

    return buffer;
}

int c_printf (char const* format, ...)
{
    va_list args;
    int retval;

    va_start (args, format);
    retval = c_vprintf (format, args);
    va_end (args);

    return retval;
}

int c_vprintf (char const* format, va_list args)
{
    c_return_val_if_fail (format != NULL, -1);

    return _c_vprintf (format, args);
}

int c_sprintf (char* str, char const* format, ...)
{
    va_list args;
    int retval;

    va_start (args, format);
    retval = c_vsprintf (str, format, args);
    va_end (args);

    return retval;
}

int c_fprintf (FILE* file, char const *format, ...)
{
    va_list args;
    int retval;

    va_start (args, format);
    retval = c_vfprintf (file, format, args);
    va_end (args);

    return retval;
}

int c_vsprintf (char* str, char const* format, va_list args)
{
    c_return_val_if_fail (str != NULL, -1);
    c_return_val_if_fail (format != NULL, -1);

    return _c_vsprintf (str, format, args);
}

int c_vfprintf (FILE* file, char const* format, va_list args)
{
    c_return_val_if_fail (format != NULL, -1);

    return _c_vfprintf (file, format, args);
}

int c_vasprintf (char** str, char const* format, va_list args)
{
    int len;
    c_return_val_if_fail (str != NULL, -1);

    va_list args2;

    va_copy (args2, args);

    cuint64 bufLen = c_printf_string_upper_bound(format, args);
    c_malloc_type(*str, char, bufLen);
    len = _c_vsprintf (*str, format, args2);
    va_end (args2);

    if (len < 0) {
        c_free (*str);
        *str = NULL;
    }

    return len;
}

int c_snprintf (char* str, culong n, char const *format, ...)
{
    va_list args;
    int retval;

    va_start (args, format);
    retval = c_vsnprintf (str, n, format, args);
    va_end (args);

    return retval;
}

int c_vsnprintf (char* str, culong n, char const* format, va_list args)
{
    c_return_val_if_fail (n == 0 || str != NULL, -1);
    c_return_val_if_fail (format != NULL, -1);

    return _c_vsnprintf (str, n, format, args);
}

cuint64 c_printf_string_upper_bound (const char* format, va_list args)
{
    char c[1];
    return _c_vsnprintf (c, 1, format, args) + 1;
}

char* c_strup (const char* str)
{
    cuchar *s;

    c_return_val_if_fail (str != NULL, NULL);

    char* strT = c_strdup (str);
    s = (cuchar*) strT;

    while (*s) {
        if (ISLOWER (*s)) {
            *s = TOUPPER (*s);
        }
        s++;
    }

    return (char*) strT;
}

char* c_strchug (const char* str)
{
    cuchar* start = NULL;

    c_return_val_if_fail (str != NULL, NULL);

    for (start = (cuchar*) str; *start && c_ascii_isspace (*start); start++);

    return c_strdup ((char*) start);
}

char* c_strchomp (const char* str)
{
    cuint64 len;

    c_return_val_if_fail (str != NULL, NULL);

    len = strlen (str);

    char* strT = c_strdup (str);

    while (len--) {
        if (c_ascii_isspace ((cuchar) strT[len])) {
            strT[len] = '\0';
        }
        else {
            break;
        }
    }

    return strT;
}

void c_strtrim_arr(char str[])
{
    c_return_if_fail(str);

    int idx = 0;
    cuint64 len = c_strlen(str);
    for (idx = len; idx >=0; idx--) {
        if (c_ascii_isspace((cuchar)str[idx]) || (str[idx] == '\n') || (str[idx] == '\r')) {
            str[idx] = '\0';
        }
        else {
            break;
        }
    }
}

cuint64 c_strlen(const char * str)
{
    cuint64 len = 0;

    c_return_val_if_fail(str, len);

    while (str[len] != '\0') { len++; }

    return len;
}

char* c_strreverse (const char* str)
{
    c_return_val_if_fail (str != NULL, NULL);

    char* strT = c_strdup (str);

    if (*strT) {
        char *h, *t;

        h = strT;
        t = strT + strlen (strT) - 1;

        while (h < t) {
            char c;
            c = *h;
            *h = *t;
            h++;
            *t = c;
            t--;
        }
    }

    return strT;
}

char* c_strdown (const char* str)
{
    cuchar *s;

    c_return_val_if_fail (str != NULL, NULL);

    char* strT = c_strdup (str);
    s = (cuchar*) strT;

    while (*s) {
        if (ISUPPER(*s)) {
            *s = TOLOWER(*s);
        }
        s++;
    }

    return (char*) strT;
}

bool c_str_is_ascii (const char* str)
{
    cuint64 i;
    for (i = 0; str[i]; i++) {
        if (str[i] & 0x80) {
            return false;
        }
    }

    return true;
}

void c_strfreev (char** strArray)
{
    if (strArray) {
        cuint64 i;
        for (i = 0; strArray[i] != NULL; i++) {
            c_free(strArray[i]);
        }
        c_free (strArray);
    }
}

char** c_strdupv (const char** strArray)
{
    if (strArray) {
        cuint64 i = 0;
        char** retval = NULL;

        while (strArray[i]) {
            ++i;
        }

        c_malloc_type(retval, char*, i + 1);

        i = 0;
        while (strArray[i]) {
            retval[i] = c_strdup (strArray[i]);
            ++i;
        }
        retval[i] = NULL;
        return retval;
    }

    return NULL;
}

cuint c_strv_length (char** strArray)
{
    cuint64 count;
    c_ptr_array_count0(strArray, count);

    return count;
}

cuint c_strv_const_length (const char* const* strArray)
{
    cuint64 count;
    c_ptr_array_count0(strArray, count);

    return count;
}

const char* c_strerror (int errNum)
{
    return strerror (errNum);
}

const char* c_strsignal (int signum)
{
    char* msg = NULL;

    msg = strsignal (signum);
    if (!msg) {
        msg = c_strdup_printf ("unknown signal (%d)", signum);
    }

    return msg;

}

char* c_strdup (const char* str)
{
    char* newStr = NULL;

    if (str) {
        cuint64 len = strlen (str) + 1;
        c_malloc(newStr, len);
        memcpy (newStr, str, len);
    }
    else {
        newStr = NULL;
    }

    return newStr;

}

char* c_strcompress (const char* source)
{
    char* q = NULL;
    char* dest = NULL;
    const char *p = source, *octal = NULL;

    c_return_val_if_fail (source != NULL, NULL);

    c_malloc (dest, strlen (source) + 1);
    q = dest;

    while (*p) {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '\0': {
                    C_DEBUG_INFO("c_strcompress: trailing \\");
                    goto out;
                }
                case '0':  case '1':  case '2':  case '3':  case '4':
                case '5':  case '6':  case '7': {
                    *q = 0;
                    octal = p;
                    while ((p < octal + 3) && (*p >= '0') && (*p <= '7')) {
                        *q = (*q * 8) + (*p - '0');
                        p++;
                    }
                    q++;
                    p--;
                    break;
                }
                case 'b': {
                    *q++ = '\b';
                    break;
                }
                case 'f': {
                    *q++ = '\f';
                    break;
                }
                case 'n': {
                    *q++ = '\n';
                    break;
                }
                case 'r': {
                    *q++ = '\r';
                    break;
                }
                case 't': {
                    *q++ = '\t';
                    break;
                }
                case 'v': {
                    *q++ = '\v';
                    break;
                }
                default: {
                    *q++ = *p;
                    break;
                }
            }
        }
        else
            *q++ = *p;
        p++;
    }
out:
    *q = 0;

    return dest;
}

double c_strtod (const char* nPtr, char** endPtr)
{
    char* failPos1;
    char* failPos2;
    double val1;
    double val2 = 0;

    c_return_val_if_fail (nPtr != NULL, 0);

    failPos1 = NULL;
    failPos2 = NULL;

    val1 = strtod (nPtr, &failPos1);

    if (failPos1 && failPos1[0] != 0) {
        val2 = c_ascii_strtod (nPtr, &failPos2);
    }

    if (!failPos1 || failPos1[0] == 0 || failPos1 >= failPos2) {
        if (endPtr) {
            *endPtr = failPos1;
        }
        return val1;
    }
    else {
        if (endPtr) {
            *endPtr = failPos2;
        }
        return val2;
    }
}

char* c_strndup (const char* str, cuint64 n)
{
    char* newStr = NULL;
    if (str) {
        c_malloc_type(newStr, char, n + 1);
        strncpy (newStr, str, n);
        newStr[n] = '\0';
    }

    return newStr;
}

char* c_strnfill (cint64 length, char fillChar)
{
    char* str = NULL;

    c_malloc_type(str, char, length + 1);
    memset (str, (cuchar) fillChar, length);
    str[length] = '\0';

    return str;
}

char* c_strrstr (const char* haystack, const char* needle)
{
    cuint64 i = 0;
    cuint64 needleLen = 0;
    cuint64 haystackLen = 0;
    const char* p = NULL;

    c_return_val_if_fail (haystack != NULL, NULL);
    c_return_val_if_fail (needle != NULL, NULL);

    needleLen = strlen (needle);
    haystackLen = strlen (haystack);

    if (needleLen == 0) {
        return (char*) haystack;
    }

    if (haystackLen < needleLen) {
        return NULL;
    }

    p = haystack + haystackLen - needleLen;

    while (p >= haystack) {
        for (i = 0; i < needleLen; i++) {
            if (p[i] != needle[i]) {
                goto next;
            }
        }
        return (char*) p;
next:
        p--;
    }

    return NULL;
}

cuint64 c_strlcpy (char* dest, const char* src, cuint64 destSize)
{
    char *d = dest;
    const char *s = src;
    cuint64 n = destSize;

    c_return_val_if_fail (dest != NULL, 0);
    c_return_val_if_fail (src != NULL, 0);

    if (n != 0 && --n != 0) {
        do {
            char c = *s++;
            *d++ = c;
            if (c == 0) {
                break;
            }
        } while (--n != 0);
    }

    if (n == 0) {
        if (destSize != 0) {
            *d = 0;
        }
        while (*s++);
    }

    return s - src - 1;
}

cuint64 c_strlcat (char* dest, const char* src, cuint64 destSize)
{
    char *d = dest;
    cuint64 dLength = 0;
    const char *s = src;
    cuint64 bytesLeft = destSize;

    c_return_val_if_fail (dest != NULL, 0);
    c_return_val_if_fail (src != NULL, 0);

    while (*d != 0 && bytesLeft-- != 0) {
        d++;
    }
    dLength = d - dest;
    bytesLeft = destSize - dLength;

    if (bytesLeft == 0) {
        return dLength + strlen (s);
    }

    while (*s != 0) {
        if (bytesLeft != 1) {
            *d++ = *s;
            bytesLeft--;
        }
        s++;
    }
    *d = 0;

    return dLength + (s - src);
}

char* c_strjoinv (const char* separator, char** strArray)
{
    char* str = NULL;
    char* ptr = NULL;

    c_return_val_if_fail (strArray != NULL, NULL);

    if (separator == NULL) {
        separator = "";
    }

    if (*strArray) {
        cuint64 i = 0;
        cuint64 len = 0;
        cuint64 separatorLen = 0;

        separatorLen = strlen (separator);
        len = 1 + strlen (strArray[0]);
        for (i = 1; strArray[i] != NULL; i++) {
            len += strlen (strArray[i]);
        }
        len += separatorLen * (i - 1);

        c_malloc_type(str, char, len);
        ptr = c_stpcpy (str, *strArray);
        for (i = 1; strArray[i] != NULL; i++) {
            ptr = c_stpcpy (ptr, separator);
            ptr = c_stpcpy (ptr, strArray[i]);
        }
    }
    else {
        str = c_strdup ("");
    }

    return str;
}

char* c_strconcat (const char* str1, ...)
{
    cuint64 l;
    va_list args;
    char* s = NULL;
    char* ptr = NULL;
    char* concat = NULL;

    c_return_val_if_fail(str1, NULL);

    l = 1 + strlen (str1);
    va_start (args, str1);
    s = va_arg (args, char*);
    while (s) {
        l += strlen (s);
        s = va_arg (args, char*);
    }
    va_end (args);

    c_malloc_type(concat, char, l);
    c_return_val_if_fail(concat, NULL);

    ptr = concat;

    ptr = c_stpcpy (ptr, str1);
    va_start (args, str1);
    s = va_arg (args, char*);
    while (s) {
        ptr = c_stpcpy (ptr, s);
        s = va_arg (args, char*);
    }
    va_end (args);

    return concat;
}

char* c_strdup_printf (const char* format, ...)
{
    char* buffer = NULL;
    va_list args;

    va_start (args, format);
    buffer = c_strdup_vprintf (format, args);
    va_end (args);

    return buffer;
}

char* c_strescape (const char* source, const char* exceptions)
{
    char* q = NULL;
    char* dest = NULL;
    const cuchar* p = NULL;
    cuchar excmap[256] = {0};

    c_return_val_if_fail (source != NULL, NULL);

    p = (cuchar*) source;
    c_malloc(dest, strlen (source) * 4 + 1);
    q = dest;

    memset (excmap, 0, 256);
    if (exceptions) {
        cuchar *e = (cuchar*) exceptions;
        while (*e) {
            excmap[*e] = 1;
            e++;
        }
    }

    while (*p) {
        if (excmap[*p]) {
            *q++ = *p;
        }
        else {
            switch (*p) {
                case '\b': {
                    *q++ = '\\';
                    *q++ = 'b';
                    break;
                }
                case '\f': {
                    *q++ = '\\';
                    *q++ = 'f';
                    break;
                }
                case '\n': {
                    *q++ = '\\';
                    *q++ = 'n';
                    break;
                }
                case '\r': {
                    *q++ = '\\';
                    *q++ = 'r';
                    break;
                }
                case '\t': {
                    *q++ = '\\';
                    *q++ = 't';
                    break;
                }
                case '\v': {
                    *q++ = '\\';
                    *q++ = 'v';
                    break;
                }
                case '\\': {
                    *q++ = '\\';
                    *q++ = '\\';
                    break;
                }
                case '"': {
                    *q++ = '\\';
                    *q++ = '"';
                    break;
                }
                default: {
                    if ((*p < ' ') || (*p >= 0177)) {
                        *q++ = '\\';
                        *q++ = '0' + (((*p) >> 6) & 07);
                        *q++ = '0' + (((*p) >> 3) & 07);
                        *q++ = '0' + ((*p) & 07);
                    }
                    else {
                        *q++ = *p;
                    }
                    break;
                }
            }
        }
        p++;
    }
    *q = 0;
    return dest;
}

char* c_strcanon (const char* str, const char* validChars, char substitutor)
{
    char *c;

    c_return_val_if_fail (str != NULL, NULL);
    c_return_val_if_fail (validChars != NULL, NULL);

    char* strT = c_strdup (str);

    for (c = strT; *c; c++) {
        if (!strchr (validChars, *c)) {
            *c = substitutor;
        }
    }

    return strT;
}

char* c_strdelimit (const char* str, const char* delimiters, char newDelimiter)
{
    char *c;

    c_return_val_if_fail (str != NULL, NULL);

    if (!delimiters) {
        delimiters = C_STR_DELIMITERS;
    }

    char* cstr = c_strdup (str);
    for (c = cstr; *c; c++) {
        if (strchr (delimiters, *c)) {
            *c = newDelimiter;
        }
    }

    return cstr;
}

char* c_strjoin (const char* separator, ...)
{
    va_list args;
    cuint64 len = 0;
    cuint64 separatorLen = 0;
    char* s = NULL;
    char* ptr = NULL;
    char* str = NULL;

    if (separator == NULL) {
        separator = "";
    }

    separatorLen = strlen (separator);

    va_start (args, separator);

    s = va_arg (args, char*);

    if (s) {
        len = 1 + strlen (s);
        s = va_arg (args, char*);
        while (s) {
            len += separatorLen + strlen (s);
            s = va_arg (args, char*);
        }
        va_end (args);
        c_malloc_type(str, char, len);
        va_start (args, separator);

        s = va_arg (args, char*);
        ptr = c_stpcpy (str, s);

        s = va_arg (args, char*);
        while (s) {
            ptr = c_stpcpy (ptr, separator);
            ptr = c_stpcpy (ptr, s);
            s = va_arg (args, char*);
        }
    }
    else {
        str = c_strdup ("");
    }

    va_end (args);

    return str;
}

char** c_strsplit (const char* str, const char* delimiter, int maxTokens)
{
    char* s = NULL;
    const char* remainder = NULL;
    CPtrArray* strList = NULL;

    c_return_val_if_fail (str != NULL, NULL);
    c_return_val_if_fail (delimiter != NULL, NULL);
    c_return_val_if_fail (delimiter[0] != '\0', NULL);

    if (maxTokens < 1) {
        maxTokens = C_MAX_INT32;
        strList = c_ptr_array_new();
    }
    else {
        strList = c_ptr_array_new_full(maxTokens + 1, NULL);
    }

    remainder = str;
    s = c_strstr (remainder, delimiter);
    if (s) {
        cuint64 delimiterLen = strlen (delimiter);
        while (--maxTokens && s) {
            cuint64 len = 0;
            len = s - remainder;
            c_ptr_array_add(strList, c_strndup(remainder, len));
            remainder = s + delimiterLen;
            s = c_strstr (remainder, delimiter);
        }
    }
    if (*str) {
        c_ptr_array_add(strList, c_strdup (remainder));
    }
    c_ptr_array_add(strList, NULL);

    return (char**) c_ptr_array_free(strList, false);
}

char** c_strsplit_set (const char* str, const char* delimiters, int maxTokens)
{
    cuint8 delimTable[256];     // 0 表示非切割字符；1 表示切割字符
    int nTokens = 0;
    const char* s = NULL;
    const char* current = NULL;
    char* token = NULL;
    char** result = NULL;

    c_return_val_if_fail (str != NULL, NULL);
    c_return_val_if_fail (delimiters != NULL, NULL);

    if (maxTokens < 1) {
        maxTokens = C_MAX_INT32;
    }

    if (*str == '\0') {
        c_malloc_type(result, char*, 1);
        result[0] = NULL;
        return result;
    }

    // delimiters 中的字符保存
    memset (delimTable, false, sizeof (delimTable));
    for (s = delimiters; *s != '\0'; ++s) {
        delimTable[*(cuchar*)s] = true;
    }

    s = current = str;
    while (*s != '\0') {
        if (delimTable[*(cuchar*)s] && nTokens + 1 < maxTokens) {
            token = c_strndup (current, s - current);
            c_ptr_array_add1_0(result, char*, token);
            ++nTokens;
            current = s + 1;
        }
        ++s;
    }

    token = c_strndup (current, s - current);
    c_ptr_array_add1_0(result, char*, token);
    ++nTokens;

    return result;
}

char* c_strdup_vprintf (const char* format, va_list args)
{
    char* str = NULL;

    c_vasprintf (&str, format, args);

    return str;
}

char* c_strrstr_len (const char* haystack, cint64 haystackLen, const char* needle)
{
    c_return_val_if_fail (haystack != NULL, NULL);
    c_return_val_if_fail (needle != NULL, NULL);

    if (haystackLen < 0) {
        return c_strrstr (haystack, needle);
    }
    else {
        cuint64 i = 0;
        cuint64 needleLen = strlen (needle);
        const char* haystackMax = haystack + haystackLen;
        const char* p = haystack;

        while (p < haystackMax && *p) {
            p++;
        }

        if (p < haystack + needleLen) {
            return NULL;
        }

        p -= needleLen;

        while (p >= haystack) {
            for (i = 0; i < needleLen; i++) {
                if (p[i] != needle[i]) {
                    goto next;
                }
            }
            return (char *)p;
next:
            p--;
        }
        return NULL;
    }
}

char* c_strstr_len (const char* haystack, cint64 haystackLen, const char* needle)
{
    c_return_val_if_fail (haystack != NULL, NULL);
    c_return_val_if_fail (needle != NULL, NULL);

    if (haystackLen < 0) {
        return strstr (haystack, needle);
    }
    else {
        cuint64 i = 0;
        const char* p = haystack;
        cuint64 needleLen = strlen (needle);
        cuint64 haystackLenUnsigned = haystackLen;
        const char* end = NULL;

        if (needleLen == 0) {
            return (char*) haystack;
        }

        if (haystackLenUnsigned < needleLen) {
            return NULL;
        }

        end = haystack + haystackLen - needleLen;

        while (p <= end && *p) {
            for (i = 0; i < needleLen; i++) {
                if (p[i] != needle[i]) {
                    goto next;
                }
            }
            return (char*)p;
next:
            p++;
        }
        return NULL;
    }
}

bool c_str_has_suffix (const char* str, const char* suffix)
{
    cuint64 strLen;
    cuint64 suffixLen;

    c_return_val_if_fail (str != NULL, false);
    c_return_val_if_fail (suffix != NULL, false);

    strLen = strlen (str);
    suffixLen = strlen (suffix);

    if (strLen < suffixLen) {
        return false;
    }

    return strcmp (str + strLen - suffixLen, suffix) == 0;
}

bool c_str_has_prefix (const char* str, const char* prefix)
{
    c_return_val_if_fail (str != NULL, false);
    c_return_val_if_fail (prefix != NULL, false);

    return strncmp (str, prefix, strlen (prefix)) == 0;
}

char *c_strstr(const char *haystack, const char *needle)
{
    cuint64 i = 0;
    cuint64 needleLen = 0;
    cuint64 haystackLen = 0;
    const char* p = NULL;
    const char* tmp = NULL;

    c_return_val_if_fail (haystack != NULL, NULL);
    c_return_val_if_fail (needle != NULL, NULL);

    needleLen = c_strlen (needle);
    haystackLen = c_strlen (haystack);

    if (needleLen == 0) {
        return (char*) haystack;
    }

    if (haystackLen < needleLen) {
        return NULL;
    }

    tmp = haystack;
    p = haystack + haystackLen - needleLen;

    while (tmp <= p) {
        for (i = 0; i < needleLen; ++i) {
            if (tmp[i] != needle[i]) {
                goto next;
            }
        }
        return (char*) tmp;
next:
        tmp++;
    }

    return NULL;
}

void c_strchomp_arr(char *str)
{
    c_return_if_fail (str != NULL);

    cuint64 len = c_strlen (str);

    while (len--) {
        if (c_ascii_isspace ((cuchar) str[len])) {
            str[len] = '\0';
        }
        else {
            break;
        }
    }
}

// 去掉前面空格
void c_strchug_arr(char *str)
{
    int idx = 0;

    c_return_if_fail (str != NULL);

    cuint64 len = c_strlen (str);

    int idxNoSpace = 0;
    while ((idxNoSpace < len) && c_ascii_isspace(str[idxNoSpace])) {++idxNoSpace;};

    c_return_if_fail(idx != idxNoSpace);

    for (idx = 0; idxNoSpace < len; ++idx, ++idxNoSpace) {
        str[idx] = str[idxNoSpace];
    }
}

void c_strip_arr(char *str)
{
    c_return_if_fail (str != NULL);

    c_strchug_arr(str);
    c_strchomp_arr(str);
}


// static

static char* c_stpcpy (char* dest, const char* src)
{
    char* d = dest;
    const char* s = src;

    c_return_val_if_fail (dest != NULL, NULL);
    c_return_val_if_fail (src != NULL, NULL);
    do {
        *d++ = *s;
    }
    while (*s++ != '\0');

    return d - 1;
}

static cuint64 c_parse_long_long (const char* nptr, const char** endptr, cuint base, bool* negative)
{
    bool overflow;
    cuint64 cutoff;
    cuint64 cutlim;
    cuint64 ui64;
    const char *s, *save;
    cuchar c;

    c_return_val_if_fail (nptr != NULL, 0);

    *negative = false;
    if (base == 1 || base > 36) {
        errno = EINVAL;
        if (endptr) {
            *endptr = nptr;
        }
        return 0;
    }

    save = s = nptr;

    while (ISSPACE (*s)) {
        ++s;
    }

    if (C_UNLIKELY (!*s))
        goto noconv;

    if (*s == '-') {
        *negative = true;
        ++s;
    }
    else if (*s == '+') {
        ++s;
    }

    if (*s == '0') {
        if ((base == 0 || base == 16) && TOUPPER (s[1]) == 'X') {
            s += 2;
            base = 16;
        }
        else if (base == 0) {
            base = 8;
        }
    }
    else if (base == 0) {
        base = 10;
    }

    save = s;
    cutoff = C_MAX_UINT64 / base;
    cutlim = C_MAX_UINT64 % base;

    overflow = false;
    ui64 = 0;
    c = *s;
    for (; c; c = *++s) {
        if (c >= '0' && c <= '9') {
            c -= '0';
        }
        else if (ISALPHA (c)) {
            c = TOUPPER (c) - 'A' + 10;
        }
        else {
            break;
        }
        if (c >= base) {
            break;
        }
        /* Check for overflow.  */
        if ((ui64 > cutoff) || (ui64 == cutoff && c > cutlim)) {
            overflow = true;
        }
        else {
            ui64 *= base;
            ui64 += c;
        }
    }

    if (s == save) {
        goto noconv;
    }

    if (endptr) {
        *endptr = s;
    }

    if (C_UNLIKELY (overflow)) {
        errno = ERANGE;
        return C_MAX_UINT64;
    }

    return ui64;

noconv:
    if (endptr) {
        if (save - nptr >= 2 && TOUPPER (save[-1]) == 'X' && save[-2] == '0') {
            *endptr = &save[-1];
        }
        else {
            *endptr = nptr;
        }
    }
    return 0;
}
