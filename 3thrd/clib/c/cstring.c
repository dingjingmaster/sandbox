
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

#include "cstring.h"

#include <ctype.h>

#include "str.h"
#include "log.h"
#include "bytes.h"

extern void _uri_encoder (CString* out, const cuchar* start, csize length, const char* reservedCharsAllowed, bool allowUtf8);

static void c_string_maybe_expand (CString* str, csize len);


CString* c_string_new (const char* init)
{
    CString* str;

    if (init == NULL || *init == '\0') {
        str = c_string_sized_new (2);
    }
    else {
        cint len = (int) strlen (init);
        str = c_string_sized_new (len + 2);
        c_string_append_len (str, init, len);
    }

    return str;
}

CString* c_string_new_len (const char* init, cssize len)
{
    CString* str;

    if (len < 0) {
        return c_string_new (init);
    }
    else {
        str = c_string_sized_new (len);
        if (init) {
            c_string_append_len (str, init, len);
        }
        return str;
    }
}

CString* c_string_sized_new (csize dflSize)
{
    CString* str = c_malloc0(sizeof(CString));

    str->allocatedLen = 0;
    str->len   = 0;
    str->str   = NULL;

    c_string_maybe_expand (str, C_MAX (dflSize, 64));
    str->str[0] = 0;

    return str;
}

char* c_string_free (CString* str, bool freeSegment)
{
    char* segment;

    c_return_val_if_fail (str != NULL, NULL);

    if (freeSegment) {
        c_free (str->str);
        segment = NULL;
    }
    else {
        segment = str->str;
    }

    c_free(str);

    return segment;
}

CBytes* c_string_free_to_bytes (CString* str)
{
    csize len;
    char* buf;

    c_return_val_if_fail (str != NULL, NULL);

    len = str->len;

    buf = c_string_free (str, false);

    return c_bytes_new_take (buf, len);
}

bool c_string_equal (const CString* v, const CString* v2)
{
    char *p, *q;
    CString* string1 = (CString*) v;
    CString* string2 = (CString*) v2;
    csize i = string1->len;

    if (i != string2->len) {
        return false;
    }

    p = string1->str;
    q = string2->str;
    while (i) {
        if (*p != *q) {
            return false;
        }
        p++;
        q++;
        i--;
    }

    return true;
}

cuint c_string_hash (const CString* str)
{
    const char* p = str->str;
    csize n = str->len;
    cuint h = 0;

    /* 31 bit hash function */
    while (n--) {
        h = (h << 5) - h + *p;
        p++;
    }

    return h;
}

CString* c_string_assign (CString* str, const char* rVal)
{
    c_return_val_if_fail (str != NULL, NULL);
    c_return_val_if_fail (rVal != NULL, str);

    if (str->str != rVal) {
        c_string_truncate (str, 0);
        c_string_append (str, rVal);
    }

    return str;
}

CString* c_string_truncate (CString* str, csize len)
{
    c_return_val_if_fail (str != NULL, NULL);

    str->len = C_MIN (len, str->len);
    str->str[str->len] = 0;

    return str;
}

CString* c_string_set_size (CString* str, csize len)
{
    c_return_val_if_fail (str != NULL, NULL);

    if (len >= str->allocatedLen) {
        c_string_maybe_expand (str, len - str->len);
    }

    str->len = len;
    str->str[len] = 0;

    return str;

}

CString* c_string_insert_len (CString* str, cssize pos, const char* val, cssize len)
{
    csize lenUnsigned, posUnsigned;

    c_return_val_if_fail (str != NULL, NULL);
    c_return_val_if_fail (len == 0 || val != NULL, str);

    if (len == 0) {
        return str;
    }

    if (len < 0) {
        len = (cssize) strlen (val);
    }
    lenUnsigned = len;

    if (pos < 0) {
        posUnsigned = str->len;
    }
    else {
        posUnsigned = pos;
        c_return_val_if_fail (posUnsigned <= str->len, str);
    }


    if (C_UNLIKELY (val >= str->str && val <= str->str + str->len)) {
        csize offset = val - str->str;
        csize precount = 0;

        c_string_maybe_expand (str, lenUnsigned);
        val = str->str + offset;

        if (posUnsigned < str->len) {
            memmove (str->str + posUnsigned + lenUnsigned, str->str + posUnsigned, str->len - posUnsigned);
        }

        if (offset < posUnsigned) {
            precount = C_MIN (lenUnsigned, posUnsigned - offset);
            memcpy (str->str + posUnsigned, val, precount);
        }

        if (lenUnsigned > precount) {
            memcpy (str->str + posUnsigned + precount, val + precount + lenUnsigned, lenUnsigned - precount);
        }
    }
    else {
        c_string_maybe_expand (str, lenUnsigned);

        if (posUnsigned < str->len) {
            memmove (str->str + posUnsigned + lenUnsigned, str->str + posUnsigned, str->len - posUnsigned);
        }

        if (lenUnsigned == 1) {
            str->str[posUnsigned] = *val;
        }
        else {
            memcpy (str->str + posUnsigned, val, lenUnsigned);
        }
    }

    str->len += lenUnsigned;
    str->str[str->len] = 0;

    return str;
}

CString* c_string_append (CString* str, const char* val)
{
    return c_string_insert_len (str, -1, val, -1);
}

CString* c_string_append_len (CString* str, const char* val, cssize len)
{
    return c_string_insert_len (str, -1, val, len);
}

CString* c_string_append_c (CString* str, char c)
{
    c_return_val_if_fail (str != NULL, NULL);

    return c_string_insert_c (str, -1, c);
}

CString* c_string_append_unichar (CString* str, cunichar wc)
{
    c_return_val_if_fail (str != NULL, NULL);

    return c_string_insert_unichar (str, -1, wc);
}

CString* c_string_prepend (CString* str, const char* val)
{
    return c_string_insert_len (str, 0, val, -1);
}

CString* c_string_prepend_c (CString* str, char c)
{
    c_return_val_if_fail (str != NULL, NULL);

    return c_string_insert_c (str, 0, c);
}

CString* c_string_prepend_unichar (CString* str, cunichar wc)
{
    c_return_val_if_fail (str != NULL, NULL);

    return c_string_insert_unichar (str, 0, wc);
}

CString* c_string_prepend_len (CString* str, const char* val, cssize len)
{
    return c_string_insert_len (str, 0, val, len);
}

CString* c_string_insert (CString* str, cssize pos, const char* val)
{
    return c_string_insert_len (str, pos, val, -1);
}

CString* c_string_insert_c (CString* str, cssize pos, char c)
{
    csize posUnsigned;

    c_return_val_if_fail (str != NULL, NULL);

    c_string_maybe_expand (str, 1);

    if (pos < 0) {
        pos = (cssize) str->len;
    }
    else {
        c_return_val_if_fail ((csize) pos <= str->len, str);
    }
    posUnsigned = pos;

    /* If not just an append, move the old stuff */
    if (posUnsigned < str->len) {
        memmove (str->str + posUnsigned + 1, str->str + posUnsigned, str->len - posUnsigned);
    }

    str->str[posUnsigned] = c;

    str->len += 1;

    str->str[str->len] = 0;

    return str;
}

CString* c_string_insert_unichar (CString* str, cssize pos, cunichar wc)
{
    cint charLen, first, i;
    char* dest = NULL;

    c_return_val_if_fail (str != NULL, NULL);

    /* Code copied from g_unichar_to_utf() */
    if (wc < 0x80) {
        first = 0;
        charLen = 1;
    }
    else if (wc < 0x800) {
        first = 0xc0;
        charLen = 2;
    }
    else if (wc < 0x10000) {
        first = 0xe0;
        charLen = 3;
    }
    else if (wc < 0x200000) {
        first = 0xf0;
        charLen = 4;
    }
    else if (wc < 0x4000000) {
        first = 0xf8;
        charLen = 5;
    }
    else {
        first = 0xfc;
        charLen = 6;
    }
    /* End of copied code */

    c_string_maybe_expand (str, charLen);

    if (pos < 0) {
        pos = (cssize) str->len;
    }
    else {
        c_return_val_if_fail ((csize) pos <= str->len, str);
    }

    if ((csize) pos < str->len) {
        memmove (str->str + pos + charLen, str->str + pos, str->len - pos);
    }

    dest = str->str + pos;
    for (i = charLen - 1; i > 0; --i) {
        dest[i] = (char) ((wc & 0x3f) | 0x80);
        wc >>= 6;
    }
    dest[0] = (char) (wc | first);
    /* End of copied code */

    str->len += charLen;
    str->str[str->len] = 0;

    return str;
}

CString* c_string_overwrite (CString* str, csize pos, const char* val)
{
    c_return_val_if_fail (val != NULL, str);

    return c_string_overwrite_len (str, pos, val, (cssize) strlen (val));
}

CString* c_string_overwrite_len (CString* str, csize pos, const char* val, cssize len)
{
    csize end;

    c_return_val_if_fail (str != NULL, NULL);

    if (!len) {
        return str;
    }

    c_return_val_if_fail (val != NULL, str);
    c_return_val_if_fail (pos <= str->len, str);

    if (len < 0) {
        len = (cssize) strlen (val);
    }

    end = pos + len;

    if (end > str->len) {
        c_string_maybe_expand (str, end - str->len);
    }

    memcpy (str->str + pos, val, len);

    if (end > str->len) {
        str->str[end] = '\0';
        str->len = end;
    }

    return str;

}

CString* c_string_erase (CString* str, cssize pos, cssize len)
{
    csize lenUnsigned, posUnsigned;

    c_return_val_if_fail (str != NULL, NULL);
    c_return_val_if_fail (pos >= 0, str);
    posUnsigned = pos;

    c_return_val_if_fail (posUnsigned <= str->len, str);

    if (len < 0) {
        lenUnsigned = str->len - posUnsigned;
    }
    else {
        lenUnsigned = len;
        c_return_val_if_fail (posUnsigned + lenUnsigned <= str->len, str);
        if (posUnsigned + lenUnsigned < str->len) {
            memmove (str->str + posUnsigned, str->str + posUnsigned + lenUnsigned, str->len - (posUnsigned + lenUnsigned));
        }
    }

    str->len -= lenUnsigned;
    str->str[str->len] = 0;

    return str;
}

cuint c_string_replace (CString* str, const char* find, const char* replace, cuint limit)
{
    csize fLen, rLen, pos;
    char *cur, *next;
    cuint n = 0;

    c_return_val_if_fail (str != NULL, 0);
    c_return_val_if_fail (find != NULL, 0);
    c_return_val_if_fail (replace != NULL, 0);

    fLen = strlen (find);
    rLen = strlen (replace);
    cur = str->str;

    while ((next = strstr (cur, find)) != NULL) {
        pos = next - str->str;
        c_string_erase (str, (cssize) pos, (cssize) fLen);
        c_string_insert (str, (cssize) pos, replace);
        cur = str->str + pos + rLen;
        n++;
        if (fLen == 0) {
            if (cur[0] == '\0') {
                break;
            }
            else {
                cur++;
            }
        }
        if (n == limit) {
            break;
        }
    }

    return n;
}

CString* c_string_ascii_down (CString* str)
{
    cint n;
    char* s = NULL;

    c_return_val_if_fail (str != NULL, NULL);

    n = (cint) str->len;
    s = str->str;

    while (n) {
        *s = c_ascii_tolower (*s);
        s++;
        n--;
    }

    return str;
}

CString* c_string_ascii_up (CString* str)
{
    cint n;
    char* s = NULL;

    c_return_val_if_fail (str != NULL, NULL);

    n = (cint) str->len;
    s = str->str;

    while (n) {
        *s = c_ascii_toupper (*s);
        s++;
        n--;
    }

    return str;
}

void c_string_vprintf (CString* str, const char* format, va_list args)
{
    c_string_truncate (str, 0);

    c_string_append_vprintf (str, format, args);
}

void c_string_printf (CString* str, const char* format, ...)
{
    va_list args;
    c_string_truncate (str, 0);
    va_start (args, format);
    c_string_append_vprintf (str, format, args);
    va_end (args);

}

void c_string_append_vprintf (CString* str, const char* format, va_list args)
{
    char* buf = NULL;
    cint len;

    c_return_if_fail (str != NULL);
    c_return_if_fail (format != NULL);

    len = c_vasprintf (&buf, format, args);

    if (len >= 0) {
        c_string_maybe_expand (str, len);
        memcpy (str->str + str->len, buf, len + 1);
        str->len += len;
        c_free (buf);
    }
}

void c_string_append_printf (CString* str, const char* format, ...)
{
    va_list args;

    va_start (args, format);
    c_string_append_vprintf (str, format, args);
    va_end (args);
}

CString* c_string_append_uri_escaped (CString* str, const char* unescaped, const char* reservedCharsAllowed, bool allowUtf8)
{
    _uri_encoder (str, (const cuchar*) unescaped, strlen (unescaped), reservedCharsAllowed, allowUtf8);
    return str;
}

CString* c_string_down (CString* str)
{
    clong n;
    cuchar* s;

    c_return_val_if_fail (str != NULL, NULL);

    n = (clong) str->len;
    s = (cuchar*) str->str;

    while (n) {
        if (isupper (*s)) {
            *s = tolower (*s);
        }
        s++;
        n--;
    }

    return str;
}

CString* c_string_up (CString* str)
{
    clong n;
    cuchar* s = NULL;

    c_return_val_if_fail (str != NULL, NULL);

    n = (clong) str->len;
    s = (cuchar*) str->str;

    while (n) {
        if (islower (*s)) {
            *s = toupper (*s);
        }
        s++;
        n--;
    }

    return str;
}


static void c_string_maybe_expand (CString* str, csize len)
{
    if C_UNLIKELY ((C_MAX_SIZE - str->len - 1) < len) {
        C_LOG_ERROR_CONSOLE("adding %ul to string would overflow", len);
    }

    if (str->len + len >= str->allocatedLen) {
        str->allocatedLen = c_nearest_pow (str->len + len + 1);
        if (str->allocatedLen == 0) {
            str->allocatedLen = str->len + len + 1;
        }
        str->str = c_realloc (str->str, str->allocatedLen);
    }
}
