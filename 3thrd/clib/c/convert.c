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

#include "convert.h"

#include "clib.h"

#include <iconv.h>

#define NUL_TERMINATOR_LENGTH 4

C_DEFINE_QUARK(c_convert_error, c_convert_error)

typedef enum
{
    CONVERT_CHECK_NO_NULS_IN_INPUT = 1 << 0,
    CONVERT_CHECK_NO_NULS_IN_OUTPUT = 1 << 1
} ConvertCheckFlags;


extern void* c_private_set_alloc0 (CPrivate* key, csize size);


static bool try_conversion(const char * to_codeset, const char * from_codeset, iconv_t * cd)
{
    *cd = iconv_open(to_codeset, from_codeset);

    if (*cd == (iconv_t)-1 && errno == EINVAL)
        return false;

#if defined(__FreeBSD__) && defined(ICONV_SET_ILSEQ_INVALID)
  /* On FreeBSD request GNU iconv compatible handling of characters that cannot
   * be represented in the destination character set.
   * See https://cgit.freebsd.org/src/commit/?id=7c5b23111c5fd1992047922d4247c4a1ce1bb6c3
   */
  int value = 1;
  if (iconvctl (*cd, ICONV_SET_ILSEQ_INVALID, &value) != 0)
    return false;
#endif
    return true;
}

static bool try_to_aliases(const char ** to_aliases, const char * from_codeset, iconv_t * cd)
{
    if (to_aliases) {
        const char ** p = to_aliases;
        while (*p) {
            if (try_conversion(*p, from_codeset, cd))
                return true;

            p++;
        }
    }

    return false;
}


CIConv c_iconv_open(const cchar * to_codeset, const cchar * from_codeset)
{
    iconv_t cd;

    if (!try_conversion(to_codeset, from_codeset, &cd)) {
        const char ** to_aliases = _c_charset_get_aliases(to_codeset);
        const char ** from_aliases = _c_charset_get_aliases(from_codeset);

        if (from_aliases) {
            const char ** p = from_aliases;
            while (*p) {
                if (try_conversion(to_codeset, *p, &cd))
                    goto out;

                if (try_to_aliases(to_aliases, *p, &cd))
                    goto out;

                p++;
            }
        }

        if (try_to_aliases(to_aliases, from_codeset, &cd))
            goto out;
    }

out:
    return (cd == (iconv_t)-1) ? (CIConv) - 1 : (CIConv)cd;
}


csize c_iconv(CIConv converter, cchar ** inbuf, csize * inbytes_left, cchar ** outbuf, csize * outbytes_left)
{
    iconv_t cd = (iconv_t)converter;

    return iconv(cd, inbuf, inbytes_left, outbuf, outbytes_left);
}

cint c_iconv_close(CIConv converter)
{
    iconv_t cd = (iconv_t)converter;

    return iconv_close(cd);
}

static CIConv open_converter(const cchar * to_codeset, const cchar * from_codeset, CError ** error)
{
    CIConv cd;

    cd = c_iconv_open(to_codeset, from_codeset);

    if (cd == (CIConv) - 1) {
        /* Something went wrong.  */
        if (error) {
            if (errno == EINVAL)
                c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_NO_CONVERSION,
                            _("Conversion from character set “%s” to “%s” is not supported"),
                            from_codeset, to_codeset);
            else
                c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_FAILED,
                            _("Could not open converter from “%s” to “%s”"),
                            from_codeset, to_codeset);
        }
    }

    return cd;
}

static int close_converter(CIConv cd)
{
    if (cd == (CIConv) - 1)
        return 0;

    return c_iconv_close(cd);
}


cchar* c_convert_with_iconv(const cchar * str, cssize len, CIConv converter, csize * bytes_read, csize * bytes_written, CError ** error)
{
    cchar * dest;
    cchar * outp;
    const cchar * p;
    csize inbytes_remaining;
    csize outbytes_remaining;
    csize err;
    csize outbuf_size;
    bool have_error = false;
    bool done = false;
    bool reset = false;

    c_return_val_if_fail(converter != (CIConv) -1, NULL);

    if (len < 0)
        len = strlen(str);

    p = str;
    inbytes_remaining = len;
    outbuf_size = len + NUL_TERMINATOR_LENGTH;

    outbytes_remaining = outbuf_size - NUL_TERMINATOR_LENGTH;
    outp = dest = c_malloc0(outbuf_size);

    while (!done && !have_error) {
        if (reset)
            err = c_iconv(converter, NULL, &inbytes_remaining, &outp, &outbytes_remaining);
        else
            err = c_iconv(converter, (char**)&p, &inbytes_remaining, &outp, &outbytes_remaining);

        if (err == (csize)-1) {
            switch (errno) {
            case EINVAL:
                /* Incomplete text, do not report an error */
                done = true;
                break;
            case E2BIG: {
                csize used = outp - dest;

                outbuf_size *= 2;
                dest = c_realloc(dest, outbuf_size);

                outp = dest + used;
                outbytes_remaining = outbuf_size - used - NUL_TERMINATOR_LENGTH;
            }
            break;
            case EILSEQ:
                c_set_error_literal(error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                    _("Invalid byte sequence in conversion input"));
                have_error = true;
                break;
            default: {
                int errsv = errno;

                c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_FAILED,
                            _("Error during conversion: %s"),
                            c_strerror(errsv));
            }
                have_error = true;
                break;
            }
        }
        else if (err > 0) {
            /* @err gives the number of replacement characters used. */
            c_set_error_literal(error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                _("Unrepresentable character in conversion input"));
            have_error = true;
        }
        else {
            if (!reset) {
                /* call c_iconv with NULL inbuf to cleanup shift state */
                reset = true;
                inbytes_remaining = 0;
            }
            else
                done = true;
        }
    }

    memset(outp, 0, NUL_TERMINATOR_LENGTH);

    if (bytes_read)
        *bytes_read = p - str;
    else {
        if ((p - str) != len) {
            if (!have_error) {
                c_set_error_literal(error, C_CONVERT_ERROR, C_CONVERT_ERROR_PARTIAL_INPUT,
                                    _("Partial character sequence at end of input"));
                have_error = true;
            }
        }
    }

    if (bytes_written)
        *bytes_written = outp - dest; /* Doesn't include '\0' */

    if (have_error) {
        c_free(dest);
        return NULL;
    }
    else
        return dest;
}


cchar* c_convert(const cchar * str, cssize len, const cchar * to_codeset, const cchar * from_codeset, csize * bytes_read, csize * bytes_written, CError ** error)
{
    cchar * res;
    CIConv cd;

    c_return_val_if_fail(str != NULL, NULL);
    c_return_val_if_fail(to_codeset != NULL, NULL);
    c_return_val_if_fail(from_codeset != NULL, NULL);

    cd = open_converter(to_codeset, from_codeset, error);

    if (cd == (CIConv) - 1) {
        if (bytes_read)
            *bytes_read = 0;

        if (bytes_written)
            *bytes_written = 0;

        return NULL;
    }

    res = c_convert_with_iconv(str, len, cd,
                               bytes_read, bytes_written,
                               error);

    close_converter(cd);

    return res;
}


cchar* c_convert_with_fallback(const cchar * str, cssize len, const cchar * to_codeset, const cchar * from_codeset, const cchar * fallback, csize * bytes_read, csize * bytes_written, CError ** error)
{
    cchar * utf8;
    cchar * dest;
    cchar * outp;
    const cchar * insert_str = NULL;
    const cchar * p;
    csize inbytes_remaining;
    const cchar * save_p = NULL;
    csize save_inbytes = 0;
    csize outbytes_remaining;
    csize err;
    CIConv cd;
    csize outbuf_size;
    bool have_error = false;
    bool done = false;

    CError * local_error = NULL;

    c_return_val_if_fail(str != NULL, NULL);
    c_return_val_if_fail(to_codeset != NULL, NULL);
    c_return_val_if_fail(from_codeset != NULL, NULL);

    if (len < 0)
        len = strlen(str);

    /* Try an exact conversion; we only proceed if this fails
     * due to an illegal sequence in the input string.
     */
    dest = c_convert(str, len, to_codeset, from_codeset,
                     bytes_read, bytes_written, &local_error);
    if (!local_error)
        return dest;

    c_assert(dest == NULL);

    if (!c_error_matches(local_error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE)) {
        c_propagate_error(error, local_error);
        return NULL;
    }
    else
        c_error_free(local_error);

    local_error = NULL;

    /* No go; to proceed, we need a converter from "UTF-8" to
     * to_codeset, and the string as UTF-8.
     */
    cd = open_converter(to_codeset, "UTF-8", error);
    if (cd == (CIConv) - 1) {
        if (bytes_read)
            *bytes_read = 0;

        if (bytes_written)
            *bytes_written = 0;

        return NULL;
    }

    utf8 = c_convert(str, len, "UTF-8", from_codeset,
                     bytes_read, &inbytes_remaining, error);
    if (!utf8) {
        close_converter(cd);
        if (bytes_written)
            *bytes_written = 0;
        return NULL;
    }

    p = utf8;

    outbuf_size = len + NUL_TERMINATOR_LENGTH;
    outbytes_remaining = outbuf_size - NUL_TERMINATOR_LENGTH;
    outp = dest = c_malloc0(outbuf_size);

    while (!done && !have_error) {
        csize inbytes_tmp = inbytes_remaining;
        err = c_iconv(cd, (char**)&p, &inbytes_tmp, &outp, &outbytes_remaining);
        inbytes_remaining = inbytes_tmp;

        if (err == (csize)-1) {
            switch (errno) {
            case EINVAL:
                c_assert_not_reached();
                break;
            case E2BIG: {
                csize used = outp - dest;

                outbuf_size *= 2;
                dest = c_realloc(dest, outbuf_size);

                outp = dest + used;
                outbytes_remaining = outbuf_size - used - NUL_TERMINATOR_LENGTH;

                break;
            }
            case EILSEQ:
                if (save_p) {
                    /* Error converting fallback string - fatal
                     */
                    c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                _("Cannot convert fallback “%s” to codeset “%s”"),
                                insert_str, to_codeset);
                    have_error = true;
                    break;
                }
                else if (p) {
                    if (!fallback) {
                        cunichar ch = c_utf8_get_char(p);
                        insert_str = c_strdup_printf(ch < 0x10000 ? "\\u%04x" : "\\U%08x",
                                                     ch);
                    }
                    else
                        insert_str = fallback;

                    save_p = c_utf8_next_char(p);
                    save_inbytes = inbytes_remaining - (save_p - p);
                    p = insert_str;
                    inbytes_remaining = strlen(p);
                    break;
                }
                C_FALLTHROUGH;
            default: {
                int errsv = errno;

                c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_FAILED,
                            _("Error during conversion: %s"),
                            c_strerror(errsv));
            }

                have_error = true;
                break;
            }
        }
        else {
            if (save_p) {
                if (!fallback)
                    c_free0((cchar*)insert_str);
                p = save_p;
                inbytes_remaining = save_inbytes;
                save_p = NULL;
            }
            else if (p) {
                /* call c_iconv with NULL inbuf to cleanup shift state */
                p = NULL;
                inbytes_remaining = 0;
            }
            else
                done = true;
        }
    }

    /* Cleanup
     */
    memset(outp, 0, NUL_TERMINATOR_LENGTH);

    close_converter(cd);

    if (bytes_written)
        *bytes_written = outp - dest; /* Doesn't include '\0' */

    c_free(utf8);

    if (have_error) {
        if (save_p && !fallback)
            c_free0((cchar*)insert_str);
        c_free(dest);
        return NULL;
    }
    else
        return dest;
}

static cchar* strdup_len(const cchar * string, cssize len, csize * bytes_read, csize * bytes_written, CError ** error)
{
    csize real_len;
    const cchar * end_valid;

    if (!c_utf8_validate(string, len, &end_valid)) {
        if (bytes_read)
            *bytes_read = end_valid - string;
        if (bytes_written)
            *bytes_written = 0;

        c_set_error_literal(error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                            _("Invalid byte sequence in conversion input"));
        return NULL;
    }

    real_len = end_valid - string;

    if (bytes_read)
        *bytes_read = real_len;
    if (bytes_written)
        *bytes_written = real_len;

    return c_strndup(string, real_len);
}

static cchar* convert_checked(const cchar * string, cssize len, const cchar * to_codeset, const cchar * from_codeset, ConvertCheckFlags flags, csize * bytes_read, csize * bytes_written, CError ** error)
{
    cchar * out;
    csize outbytes;

    if ((flags & CONVERT_CHECK_NO_NULS_IN_INPUT) && len > 0) {
        const cchar * early_nul = memchr(string, '\0', len);
        if (early_nul != NULL) {
            if (bytes_read)
                *bytes_read = early_nul - string;
            if (bytes_written)
                *bytes_written = 0;

            c_set_error_literal(error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                _("Embedded NUL byte in conversion input"));
            return NULL;
        }
    }

    out = c_convert(string, len, to_codeset, from_codeset,
                    bytes_read, &outbytes, error);
    if (out == NULL) {
        if (bytes_written)
            *bytes_written = 0;
        return NULL;
    }

    if ((flags & CONVERT_CHECK_NO_NULS_IN_OUTPUT)
        && memchr(out, '\0', outbytes) != NULL) {
        c_free(out);
        if (bytes_written)
            *bytes_written = 0;
        c_set_error_literal(error, C_CONVERT_ERROR, C_CONVERT_ERROR_EMBEDDED_NUL,
                            _("Embedded NUL byte in conversion output"));
        return NULL;
    }

    if (bytes_written)
        *bytes_written = outbytes;
    return out;
}

cchar* c_locale_to_utf8(const cchar * opsysstring, cssize len, csize * bytes_read, csize * bytes_written, CError ** error)
{
    const char * charset;

    if (c_get_charset(&charset))
        return strdup_len(opsysstring, len, bytes_read, bytes_written, error);
    else
        return convert_checked(opsysstring, len, "UTF-8", charset,
                               CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                               bytes_read, bytes_written, error);
}


cchar* _c_time_locale_to_utf8(const cchar * opsysstring, cssize len, csize * bytes_read, csize * bytes_written, CError ** error)
{
    const char * charset;

    if (_c_get_time_charset(&charset))
        return strdup_len(opsysstring, len, bytes_read, bytes_written, error);
    else
        return convert_checked(opsysstring, len, "UTF-8", charset,
                               CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                               bytes_read, bytes_written, error);
}

cchar* _c_ctype_locale_to_utf8(const cchar * opsysstring, cssize len, csize * bytes_read, csize * bytes_written, CError ** error)
{
    const char * charset;

    if (_c_get_ctype_charset(&charset))
        return strdup_len(opsysstring, len, bytes_read, bytes_written, error);
    else
        return convert_checked(opsysstring, len, "UTF-8", charset,
                               CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                               bytes_read, bytes_written, error);
}


cchar* c_locale_from_utf8(const cchar * utf8string, cssize len, csize * bytes_read, csize * bytes_written, CError ** error)
{
    const cchar * charset;

    if (c_get_charset(&charset))
        return strdup_len(utf8string, len, bytes_read, bytes_written, error);
    else
        return convert_checked(utf8string, len, charset, "UTF-8",
                               CONVERT_CHECK_NO_NULS_IN_INPUT,
                               bytes_read, bytes_written, error);
}

#ifndef C_PLATFORM_WIN32

typedef struct _CFilenameCharsetCache CFilenameCharsetCache;

struct _CFilenameCharsetCache
{
    bool is_utf8;
    cchar * charset;
    cchar ** filename_charsets;
};

static void  filename_charset_cache_free(void * data)
{
    CFilenameCharsetCache * cache = data;
    c_free(cache->charset);
    c_strfreev(cache->filename_charsets);
    c_free(cache);
}


bool c_get_filename_charsets(const cchar *** filename_charsets)
{
    static CPrivate cache_private = C_PRIVATE_INIT(filename_charset_cache_free);
    CFilenameCharsetCache * cache = c_private_get(&cache_private);
    const cchar * charset;

    if (!cache)
        cache = (CFilenameCharsetCache*) c_private_set_alloc0 (&cache_private, sizeof(CFilenameCharsetCache));

    c_get_charset(&charset);

    if (!(cache->charset && strcmp(cache->charset, charset) == 0)) {
        const cchar * new_charset;
        const cchar * p;
        cint i;

        c_free0(cache->charset);
        c_strfreev(cache->filename_charsets);
        cache->charset = c_strdup(charset);

        p = c_getenv("C_FILENAME_ENCODING");
        if (p != NULL && p[0] != '\0') {
            cache->filename_charsets = c_strsplit(p, ",", 0);
            cache->is_utf8 = (strcmp(cache->filename_charsets[0], "UTF-8") == 0);

            for (i = 0; cache->filename_charsets[i]; i++) {
                if (strcmp("@locale", cache->filename_charsets[i]) == 0) {
                    c_get_charset(&new_charset);
                    c_free(cache->filename_charsets[i]);
                    cache->filename_charsets[i] = c_strdup(new_charset);
                }
            }
        }
        else if (c_getenv("C_BROKEN_FILENAMES") != NULL) {
            cache->filename_charsets = c_malloc0(sizeof(cchar*) * 2);
            cache->is_utf8 = c_get_charset(&new_charset);
            cache->filename_charsets[0] = c_strdup(new_charset);
        }
        else {
            cache->filename_charsets = c_malloc0(sizeof(cchar*) * 3);
            cache->is_utf8 = true;
            cache->filename_charsets[0] = c_strdup("UTF-8");
            if (!c_get_charset(&new_charset))
                cache->filename_charsets[1] = c_strdup(new_charset);
        }
    }

    if (filename_charsets)
        *filename_charsets = (const cchar**)cache->filename_charsets;

    return cache->is_utf8;
}

#else /* C_PLATFORM_WIN32 */

bool
c_get_filename_charsets (const cchar ***filename_charsets)
{
  static const cchar *charsets[] = {
    "UTF-8",
    NULL
  };

#ifdef C_OS_WIN32
  /* On Windows GLib pretends that the filename charset is UTF-8 */
  if (filename_charsets)
    *filename_charsets = charsets;

  return true;
#else
  bool result;

  /* Cygwin works like before */
  result = c_get_charset (&(charsets[0]));

  if (filename_charsets)
    *filename_charsets = charsets;

  return result;
#endif
}

#endif /* C_PLATFORM_WIN32 */

static bool get_filename_charset(const cchar ** filename_charset)
{
    const cchar ** charsets;
    bool is_utf8;

    is_utf8 = c_get_filename_charsets(&charsets);

    if (filename_charset)
        *filename_charset = charsets[0];

    return is_utf8;
}


cchar* c_filename_to_utf8(const cchar * opsysstring,
                   cssize len,
                   csize * bytes_read,
                   csize * bytes_written,
                   CError ** error)
{
    const cchar * charset;

    c_return_val_if_fail(opsysstring != NULL, NULL);

    if (get_filename_charset(&charset))
        return strdup_len(opsysstring, len, bytes_read, bytes_written, error);
    else
        return convert_checked(opsysstring, len, "UTF-8", charset,
                               CONVERT_CHECK_NO_NULS_IN_INPUT |
                               CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                               bytes_read, bytes_written, error);
}


cchar* c_filename_from_utf8(const cchar * utf8string,
                     cssize len,
                     csize * bytes_read,
                     csize * bytes_written,
                     CError ** error)
{
    const cchar * charset;

    if (get_filename_charset(&charset))
        return strdup_len(utf8string, len, bytes_read, bytes_written, error);
    else
        return convert_checked(utf8string, len, charset, "UTF-8",
                               CONVERT_CHECK_NO_NULS_IN_INPUT |
                               CONVERT_CHECK_NO_NULS_IN_OUTPUT,
                               bytes_read, bytes_written, error);
}

/* Test of haystack has the needle prefix, comparing case
 * insensitive. haystack may be UTF-8, but needle must
 * contain only ascii. */
static bool
has_case_prefix(const cchar * haystack, const cchar * needle)
{
    const cchar * h, * n;

    /* Eat one character at a time. */
    h = haystack;
    n = needle;

    while (*n && *h &&
        c_ascii_tolower(*n) == c_ascii_tolower(*h)) {
        n++;
        h++;
    }

    return *n == '\0';
}

typedef enum
{
    UNSAFE_ALL = 0x1, /* Escape all unsafe characters   */
    UNSAFE_ALLOW_PLUS = 0x2, /* Allows '+'  */
    UNSAFE_PATH = 0x8, /* Allows '/', '&', '=', ':', '@', '+', '$' and ',' */
    UNSAFE_HOST = 0x10, /* Allows '/' and ':' and '@' */
    UNSAFE_SLASHES = 0x20 /* Allows all characters except for '/' and '%' */
} UnsafeCharacterSet;

static const cuchar acceptable[96] = {
    /* A table of the ASCII chars from space (32) to DEL (127) */
    /*      !    "    #    $    %    &    '    (    )    *    +    ,    -    .    / */
    0x00, 0x3F, 0x20, 0x20, 0x28, 0x00, 0x2C, 0x3F, 0x3F, 0x3F, 0x3F, 0x2A, 0x28, 0x3F, 0x3F, 0x1C,
    /* 0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ? */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x38, 0x20, 0x20, 0x2C, 0x20, 0x20,
    /* @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O */
    0x38, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    /* P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _ */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x20, 0x20, 0x20, 0x20, 0x3F,
    /* `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o */
    0x20, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
    /* p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~  DEL */
    0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x20, 0x20, 0x20, 0x3F, 0x20
};

static const cchar hex[] = "0123456789ABCDEF";

/* Note: This escape function works on file: URIs, but if you want to
 * escape something else, please read RFC-2396 */
static cchar* c_escape_uri_string(const cchar * string, UnsafeCharacterSet mask)
{
#define ACCEPTABLE(a) ((a)>=32 && (a)<128 && (acceptable[(a)-32] & use_mask))

    const cchar * p;
    cchar * q;
    cchar * result;
    int c;
    cint unacceptable;
    UnsafeCharacterSet use_mask;

    c_return_val_if_fail(mask == UNSAFE_ALL
                         || mask == UNSAFE_ALLOW_PLUS
                         || mask == UNSAFE_PATH
                         || mask == UNSAFE_HOST
                         || mask == UNSAFE_SLASHES, NULL);

    unacceptable = 0;
    use_mask = mask;
    for (p = string; *p != '\0'; p++) {
        c = (cuchar)*p;
        if (!ACCEPTABLE(c))
            unacceptable++;
    }

    result = c_malloc0(p - string + unacceptable * 2 + 1);

    use_mask = mask;
    for (q = result, p = string; *p != '\0'; p++) {
        c = (cuchar)*p;

        if (!ACCEPTABLE(c)) {
            *q++ = '%'; /* means hex coming */
            *q++ = hex[c >> 4];
            *q++ = hex[c & 15];
        }
        else
            *q++ = *p;
    }

    *q = '\0';

    return result;
}


static cchar* c_escape_file_uri(const cchar * hostname, const cchar * pathname)
{
    char * escaped_hostname = NULL;
    char * escaped_path;
    char * res;

#ifdef C_OS_WIN32
  char *p, *backslash;

  /* Turn backslashes into forward slashes. That's what Netscape
   * does, and they are actually more or less equivalent in Windows.
   */

  pathname = c_strdup (pathname);
  p = (char *) pathname;

  while ((backslash = strchr (p, '\\')) != NULL)
    {
      *backslash = '/';
      p = backslash + 1;
    }
#endif

    if (hostname && *hostname != '\0') {
        escaped_hostname = c_escape_uri_string(hostname, UNSAFE_HOST);
    }

    escaped_path = c_escape_uri_string(pathname, UNSAFE_PATH);

    res = c_strconcat("file://",
                      (escaped_hostname) ? escaped_hostname : "",
                      (*escaped_path != '/') ? "/" : "",
                      escaped_path,
                      NULL);

#ifdef C_OS_WIN32
  c_free ((char *) pathname);
#endif

    c_free(escaped_hostname);
    c_free(escaped_path);

    return res;
}

static int unescape_character(const char * scanner)
{
    int first_digit;
    int second_digit;

    first_digit = c_ascii_xdigit_value(scanner[0]);
    if (first_digit < 0)
        return -1;

    second_digit = c_ascii_xdigit_value(scanner[1]);
    if (second_digit < 0)
        return -1;

    return (first_digit << 4) | second_digit;
}

static cchar* c_unescape_uri_string(const char * escaped, int len, const char * illegal_escaped_characters, bool ascii_must_not_be_escaped)
{
    const cchar * in, * in_end;
    cchar * out, * result;
    int c;

    if (escaped == NULL)
        return NULL;

    if (len < 0)
        len = strlen(escaped);

    result = c_malloc0(len + 1);

    out = result;
    for (in = escaped, in_end = escaped + len; in < in_end; in++) {
        c = *in;

        if (c == '%') {
            /* catch partial escape sequences past the end of the substring */
            if (in + 3 > in_end)
                break;

            c = unescape_character(in + 1);

            /* catch bad escape sequences and NUL characters */
            if (c <= 0)
                break;

            /* catch escaped ASCII */
            if (ascii_must_not_be_escaped && c <= 0x7F)
                break;

            /* catch other illegal escaped characters */
            if (strchr(illegal_escaped_characters, c) != NULL)
                break;

            in += 2;
        }

        *out++ = c;
    }

    c_assert(out - result <= len);
    *out = '\0';

    if (in != in_end) {
        c_free(result);
        return NULL;
    }

    return result;
}

static bool is_asciialphanum(cunichar c)
{
    return c <= 0x7F && c_ascii_isalnum(c);
}

static bool is_asciialpha(cunichar c)
{
    return c <= 0x7F && c_ascii_isalpha(c);
}

/* allows an empty string */
static bool hostname_validate(const char * hostname)
{
    const char * p;
    cunichar c, first_char, last_char;

    p = hostname;
    if (*p == '\0')
        return true;
    do {
        /* read in a label */
        c = c_utf8_get_char(p);
        p = c_utf8_next_char(p);
        if (!is_asciialphanum(c))
            return false;
        first_char = c;
        do {
            last_char = c;
            c = c_utf8_get_char(p);
            p = c_utf8_next_char(p);
        }
        while (is_asciialphanum(c) || c == '-');
        if (last_char == '-')
            return false;

        /* if that was the last label, check that it was a toplabel */
        if (c == '\0' || (c == '.' && *p == '\0'))
            return is_asciialpha(first_char);
    }
    while (c == '.');
    return false;
}


cchar* c_filename_from_uri(const cchar * uri, cchar ** hostname, CError ** error)
{
    const char * past_scheme;
    const char * host_part;
    char * unescaped_hostname;
    char * result;
    char * filename;
    char * past_path;
    char * temp_uri;
    int offs;
#ifdef C_OS_WIN32
  char *p, *slash;
#endif

    if (hostname)
        *hostname = NULL;

    if (!has_case_prefix(uri, "file:/")) {
        c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_BAD_URI,
                    _("The URI “%s” is not an absolute URI using the “file” scheme"),
                    uri);
        return NULL;
    }

    temp_uri = c_strdup(uri);

    past_scheme = temp_uri + strlen("file:");

    past_path = strchr(past_scheme, '?');
    if (past_path != NULL)
        *past_path = '\0';

    past_path = strchr(past_scheme, '#');
    if (past_path != NULL)
        *past_path = '\0';

    if (has_case_prefix(past_scheme, "///"))
        past_scheme += 2;
    else if (has_case_prefix(past_scheme, "//")) {
        past_scheme += 2;
        host_part = past_scheme;

        past_scheme = strchr(past_scheme, '/');

        if (past_scheme == NULL) {
            c_free(temp_uri);
            c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_BAD_URI,
                        _("The URI “%s” is invalid"),
                        uri);
            return NULL;
        }

        unescaped_hostname = c_unescape_uri_string(host_part, past_scheme - host_part, "", true);

        if (unescaped_hostname == NULL ||
            !hostname_validate(unescaped_hostname)) {
            c_free(unescaped_hostname);
            c_free(temp_uri);
            c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_BAD_URI,
                        _("The hostname of the URI “%s” is invalid"),
                        uri);
            return NULL;
        }

        if (hostname)
            *hostname = unescaped_hostname;
        else
            c_free(unescaped_hostname);
    }

    filename = c_unescape_uri_string(past_scheme, -1, "/", false);

    if (filename == NULL) {
        c_free(temp_uri);
        c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_BAD_URI,
                    _("The URI “%s” contains invalidly escaped characters"),
                    uri);
        return NULL;
    }

    offs = 0;
#ifdef C_OS_WIN32
  /* Drop localhost */
  if (hostname && *hostname != NULL &&
      c_ascii_strcasecmp (*hostname, "localhost") == 0)
    {
      c_free (*hostname);
      *hostname = NULL;
    }

  /* Turn slashes into backslashes, because that's the canonical spelling */
  p = filename;
  while ((slash = strchr (p, '/')) != NULL)
    {
      *slash = '\\';
      p = slash + 1;
    }

  /* Windows URIs with a drive letter can be like "file://host/c:/foo"
   * or "file://host/c|/foo" (some Netscape versions). In those cases, start
   * the filename from the drive letter.
   */
  if (c_ascii_isalpha (filename[1]))
    {
      if (filename[2] == ':')
	offs = 1;
      else if (filename[2] == '|')
	{
	  filename[2] = ':';
	  offs = 1;
	}
    }
#endif

    result = c_strdup(filename + offs);
    c_free(filename);

    c_free(temp_uri);

    return result;
}


cchar* c_filename_to_uri(const cchar * filename, const cchar * hostname, CError ** error)
{
    char * escaped_uri;

    c_return_val_if_fail(filename != NULL, NULL);

    if (!c_path_is_absolute(filename)) {
        c_set_error(error, C_CONVERT_ERROR, C_CONVERT_ERROR_NOT_ABSOLUTE_PATH,
                    _("The pathname “%s” is not an absolute path"),
                    filename);
        return NULL;
    }

    if (hostname &&
        !(c_utf8_validate(hostname, -1, NULL)
            && hostname_validate(hostname))) {
        c_set_error_literal(error, C_CONVERT_ERROR, C_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                            _("Invalid hostname"));
        return NULL;
    }

#ifdef C_OS_WIN32
  /* Don't use localhost unnecessarily */
  if (hostname && c_ascii_strcasecmp (hostname, "localhost") == 0)
    hostname = NULL;
#endif

    escaped_uri = c_escape_file_uri(hostname, filename);

    return escaped_uri;
}


cchar** c_uri_list_extract_uris(const cchar * uri_list)
{
    CPtrArray * uris;
    const cchar * p, * q;

    uris = c_ptr_array_new();

    p = uri_list;

    /* We don't actually try to validate the URI according to RFC
     * 2396, or even check for allowed characters - we just ignore
     * comments and trim whitespace off the ends.  We also
     * allow LF delimination as well as the specified CRLF.
     *
     * We do allow comments like specified in RFC 2483.
     */
    while (p) {
        if (*p != '#') {
            while (c_ascii_isspace(*p))
                p++;

            q = p;
            while (*q && (*q != '\n') && (*q != '\r'))
                q++;

            if (q > p) {
                q--;
                while (q > p && c_ascii_isspace(*q))
                    q--;

                if (q > p)
                    c_ptr_array_add(uris, c_strndup(p, q - p + 1));
            }
        }
        p = strchr(p, '\n');
        if (p)
            p++;
    }

    c_ptr_array_add(uris, NULL);

    return (cchar**)c_ptr_array_free(uris, false);
}


cchar* c_filename_display_basename(const cchar * filename)
{
    char * basename;
    char * display_name;

    c_return_val_if_fail(filename != NULL, NULL);

    basename = c_path_get_basename(filename);
    display_name = c_filename_display_name(basename);
    c_free(basename);
    return display_name;
}


cchar* c_filename_display_name(const cchar * filename)
{
    cint i;
    const cchar ** charsets;
    cchar * display_name = NULL;
    bool is_utf8;

    is_utf8 = c_get_filename_charsets(&charsets);

    if (is_utf8) {
        if (c_utf8_validate(filename, -1, NULL))
            display_name = c_strdup(filename);
    }

    if (!display_name) {
        /* Try to convert from the filename charsets to UTF-8.
         * Skip the first charset if it is UTF-8.
         */
        for (i = is_utf8 ? 1 : 0; charsets[i]; i++) {
            display_name = c_convert(filename, -1, "UTF-8", charsets[i],
                                     NULL, NULL, NULL);

            if (display_name)
                break;
        }
    }

    /* if all conversions failed, we replace invalid UTF-8
     * by a question mark
     */
    if (!display_name)
        display_name = c_utf8_make_valid(filename, -1);

    return display_name;
}

#ifdef C_OS_WIN32

/* Binary compatibility versions. Not for newly compiled code. */

_GLIB_EXTERN cchar *c_filename_to_utf8_utf8   (const cchar  *opsysstring,
                                               cssize        len,
                                               csize        *bytes_read,
                                               csize        *bytes_written,
                                               CError      **error) C_GNUC_MALLOC;
_GLIB_EXTERN cchar *c_filename_from_utf8_utf8 (const cchar  *utf8string,
                                               cssize        len,
                                               csize        *bytes_read,
                                               csize        *bytes_written,
                                               CError      **error) C_GNUC_MALLOC;
_GLIB_EXTERN cchar *c_filename_from_uri_utf8  (const cchar  *uri,
                                               cchar       **hostname,
                                               CError      **error) C_GNUC_MALLOC;
_GLIB_EXTERN cchar *c_filename_to_uri_utf8    (const cchar  *filename,
                                               const cchar  *hostname,
                                               CError      **error) C_GNUC_MALLOC;

cchar *
c_filename_to_utf8_utf8 (const cchar *opsysstring,
                         cssize       len,
                         csize       *bytes_read,
                         csize       *bytes_written,
                         CError     **error)
{
  return c_filename_to_utf8 (opsysstring, len, bytes_read, bytes_written, error);
}

cchar *
c_filename_from_utf8_utf8 (const cchar *utf8string,
                           cssize       len,
                           csize       *bytes_read,
                           csize       *bytes_written,
                           CError     **error)
{
  return c_filename_from_utf8 (utf8string, len, bytes_read, bytes_written, error);
}

cchar *
c_filename_from_uri_utf8 (const cchar *uri,
                          cchar      **hostname,
                          CError     **error)
{
  return c_filename_from_uri (uri, hostname, error);
}

cchar *
c_filename_to_uri_utf8 (const cchar *filename,
                        const cchar *hostname,
                        CError     **error)
{
  return c_filename_to_uri (filename, hostname, error);
}

#endif
