// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 6/9/24.
//

#include "host-utils.h"

#include <bits/posix1_lim.h>

#include "clib.h"

#define IDNA_ACE_PREFIX     "xn--"
#define IDNA_ACE_PREFIX_LEN 4

/* Punycode constants, from RFC 3492. */

#define PUNYCODE_BASE          36
#define PUNYCODE_TMIN           1
#define PUNYCODE_TMAX          26
#define PUNYCODE_SKEW          38
#define PUNYCODE_DAMP         700
#define PUNYCODE_INITIAL_BIAS  72
#define PUNYCODE_INITIAL_N   0x80

#define PUNYCODE_IS_BASIC(cp) ((cuint)(cp) < 0x80)

#define idna_is_junk(ch) ((ch) == 0x00AD || (ch) == 0x1806 || (ch) == 0x200B || (ch) == 0x2060 || (ch) == 0xFEFF || (ch) == 0x034F || (ch) == 0x180B || (ch) == 0x180C || (ch) == 0x180D || (ch) == 0x200C || (ch) == 0x200D || ((ch) >= 0xFE00 && (ch) <= 0xFE0F))

/* RFC 3490, section 3.1 says '.', 0x3002, 0xFF0E, and 0xFF61 count as
 * label-separating dots. @str must be '\0'-terminated.
 */
#define idna_is_dot(str) ( \
  ((cuchar)(str)[0] == '.') ||                                                 \
  ((cuchar)(str)[0] == 0xE3 && (cuchar)(str)[1] == 0x80 && (cuchar)(str)[2] == 0x82) || \
  ((cuchar)(str)[0] == 0xEF && (cuchar)(str)[1] == 0xBC && (cuchar)(str)[2] == 0x8E) || \
  ((cuchar)(str)[0] == 0xEF && (cuchar)(str)[1] == 0xBD && (cuchar)(str)[2] == 0xA1) )

/* Encode/decode a single base-36 digit */
static inline cchar encode_digit(cuint dig)
{
    if (dig < 26)
        return dig + 'a';
    else
        return dig - 26 + '0';
}

static inline cuint decode_digit(cchar dig)
{
    if (dig >= 'A' && dig <= 'Z')
        return dig - 'A';
    else if (dig >= 'a' && dig <= 'z')
        return dig - 'a';
    else if (dig >= '0' && dig <= '9')
        return dig - '0' + 26;
    else
        return C_MAX_UINT;
}

/* Punycode bias adaptation algorithm, RFC 3492 section 6.1 */
static cuint adapt(cuint delta, cuint numpoints, bool firsttime)
{
    cuint k;

    delta = firsttime ? delta / PUNYCODE_DAMP : delta / 2;
    delta += delta / numpoints;

    k = 0;
    while (delta > ((PUNYCODE_BASE - PUNYCODE_TMIN) * PUNYCODE_TMAX) / 2) {
        delta /= PUNYCODE_BASE - PUNYCODE_TMIN;
        k += PUNYCODE_BASE;
    }

    return k + ((PUNYCODE_BASE - PUNYCODE_TMIN + 1) * delta /
        (delta + PUNYCODE_SKEW));
}

static bool punycode_encode(const cchar * input_utf8, csize input_utf8_length, CString * output)
{
    cuint delta, handled_chars, num_basic_chars, bias, j, q, k, t, digit;
    cunichar n, m, * input;
    clong written_chars;
    csize input_length;
    bool success = false;

    /* Convert from UTF-8 to Unicode code points */
    input = c_utf8_to_ucs4(input_utf8, input_utf8_length, NULL, &written_chars, NULL);
    if (!input)
        return false;

    input_length = (csize)(written_chars > 0 ? written_chars : 0);

    /* Copy basic chars */
    for (j = num_basic_chars = 0; j < input_length; j++) {
        if (PUNYCODE_IS_BASIC(input[j])) {
            c_string_append_c(output, c_ascii_tolower(input[j]));
            num_basic_chars++;
        }
    }
    if (num_basic_chars)
        c_string_append_c(output, '-');

    handled_chars = num_basic_chars;

    /* Encode non-basic chars */
    delta = 0;
    bias = PUNYCODE_INITIAL_BIAS;
    n = PUNYCODE_INITIAL_N;
    while (handled_chars < input_length) {
        /* let m = the minimum {non-basic} code point >= n in the input */
        for (m = C_MAX_UINT, j = 0; j < input_length; j++) {
            if (input[j] >= n && input[j] < m)
                m = input[j];
        }

        if (m - n > (C_MAX_UINT - delta) / (handled_chars + 1))
            goto fail;
        delta += (m - n) * (handled_chars + 1);
        n = m;

        for (j = 0; j < input_length; j++) {
            if (input[j] < n) {
                if (++delta == 0)
                    goto fail;
            }
            else if (input[j] == n) {
                q = delta;
                for (k = PUNYCODE_BASE; ; k += PUNYCODE_BASE) {
                    if (k <= bias)
                        t = PUNYCODE_TMIN;
                    else if (k >= bias + PUNYCODE_TMAX)
                        t = PUNYCODE_TMAX;
                    else
                        t = k - bias;
                    if (q < t)
                        break;
                    digit = t + (q - t) % (PUNYCODE_BASE - t);
                    c_string_append_c(output, encode_digit(digit));
                    q = (q - t) / (PUNYCODE_BASE - t);
                }

                c_string_append_c(output, encode_digit(q));
                bias = adapt(delta, handled_chars + 1, handled_chars == num_basic_chars);
                delta = 0;
                handled_chars++;
            }
        }

        delta++;
        n++;
    }

    success = true;

fail:
    c_free(input);
    return success;
}

/* From RFC 3454, Table B.1 */

/* Scan @str for "junk" and return a cleaned-up string if any junk
 * is found. Else return %NULL.
 */
static cchar* remove_junk(const cchar * str, cint len)
{
    CString * cleaned = NULL;
    const cchar * p;
    cunichar ch;

    for (p = str; len == -1 ? *p : p < str + len; p = c_utf8_next_char(p)) {
        ch = c_utf8_get_char(p);
        if (idna_is_junk(ch)) {
            if (!cleaned) {
                cleaned = c_string_new(NULL);
                c_string_append_len(cleaned, str, p - str);
            }
        }
        else if (cleaned)
            c_string_append_unichar(cleaned, ch);
    }

    if (cleaned)
        return c_string_free(cleaned, false);
    else
        return NULL;
}

static inline bool contains_uppercase_letters(const cchar * str, cint len)
{
    const cchar * p;

    for (p = str; len == -1 ? *p : p < str + len; p = c_utf8_next_char(p)) {
        if (c_unichar_isupper(c_utf8_get_char(p)))
            return true;
    }
    return false;
}

static inline bool  contains_non_ascii(const cchar * str, cint len)
{
    const cchar * p;

    for (p = str; len == -1 ? *p : p < str + len; p++) {
        if ((cuchar) * p > 0x80)
            return true;
    }
    return false;
}

/* RFC 3454, Appendix C. ish. */
static inline bool idna_is_prohibited(cunichar ch)
{
    switch (c_unichar_type(ch)) {
    case C_UNICODE_CONTROL:
    case C_UNICODE_FORMAT:
    case C_UNICODE_UNASSIGNED:
    case C_UNICODE_PRIVATE_USE:
    case C_UNICODE_SURROGATE:
    case C_UNICODE_LINE_SEPARATOR:
    case C_UNICODE_PARAGRAPH_SEPARATOR:
    case C_UNICODE_SPACE_SEPARATOR:
        return true;

    case C_UNICODE_OTHER_SYMBOL:
        if (ch == 0xFFFC || ch == 0xFFFD ||
            (ch >= 0x2FF0 && ch <= 0x2FFB))
            return true;
        return false;

    case C_UNICODE_NON_SPACING_MARK:
        if (ch == 0x0340 || ch == 0x0341)
            return true;
        return false;

    default:
        return false;
    }
}

/* RFC 3491 IDN cleanup algorithm. */
static cchar* nameprep(const cchar * hostname, cint len, bool * is_unicode)
{
    cchar * name, * tmp = NULL, * p;

    /* It would be nice if we could do this without repeatedly
     * allocating strings and converting back and forth between
     * cunichars and UTF-8... The code does at least avoid doing most of
     * the sub-operations when they would just be equivalent to a
     * c_strdup().
     */

    /* Remove presentation-only characters */
    name = remove_junk(hostname, len);
    if (name) {
        tmp = name;
        len = -1;
    }
    else
        name = (cchar*)hostname;

    /* Convert to lowercase */
    if (contains_uppercase_letters(name, len)) {
        name = c_utf8_strdown(name, len);
        c_free(tmp);
        tmp = name;
        len = -1;
    }

    /* If there are no UTF8 characters, we're done. */
    if (!contains_non_ascii(name, len)) {
        *is_unicode = false;
        if (name == (cchar*)hostname)
            return len == -1 ? c_strdup(hostname) : c_strndup(hostname, len);
        else
            return name;
    }

    *is_unicode = true;

    /* Normalize */
    name = c_utf8_normalize(name, len, C_NORMALIZE_NFKC);
    c_free(tmp);
    tmp = name;

    if (!name)
        return NULL;

    /* KC normalization may have created more capital letters (eg,
     * angstrom -> capital A with ring). So we have to lowercasify a
     * second time. (This is more-or-less how the nameprep algorithm
     * does it. If tolower(nfkc(tolower(X))) is guaranteed to be the
     * same as tolower(nfkc(X)), then we could skip the first tolower,
     * but I'm not sure it is.)
     */
    if (contains_uppercase_letters(name, -1)) {
        name = c_utf8_strdown(name, -1);
        c_free(tmp);
        tmp = name;
    }

    /* Check for prohibited characters */
    for (p = name; *p; p = c_utf8_next_char(p)) {
        if (idna_is_prohibited(c_utf8_get_char(p))) {
            name = NULL;
            c_free(tmp);
            goto done;
        }
    }

    /* FIXME: We're supposed to verify certain constraints on bidi
     * characters, but glib does not appear to have that information.
     */

done:
    return name;
}



static const cchar*
idna_end_of_label(const cchar * str)
{
    for (; *str; str = c_utf8_next_char(str)) {
        if (idna_is_dot(str))
            return str;
    }
    return str;
}

static csize
get_hostname_max_length_bytes(void)
{
#if defined(C_OS_WIN32)
  wchar_t tmp[MAX_COMPUTERNAME_LENGTH];
  return sizeof (tmp) / sizeof (tmp[0]);
#elif defined(_SC_HOST_NAME_MAX)
    clong max = sysconf(_SC_HOST_NAME_MAX);
    if (max > 0)
        return (csize)max;

#ifdef HOST_NAME_MAX
  return HOST_NAME_MAX;
#else
    return _POSIX_HOST_NAME_MAX;
#endif /* HOST_NAME_MAX */
#else
  /* Fallback to some reasonable value
   * See https://stackoverflow.com/questions/8724954/what-is-the-maximum-number-of-characters-for-a-host-name-in-unix/28918017#28918017 */
  return 255;
#endif
}

/* Returns %true if `strlen (str) > comparison_length`, but without actually
 * running `strlen(str)`, as that would take a very long time for long
 * (untrusted) input strings. */
static bool
strlen_greater_than(const cchar * str,
                    csize comparison_length)
{
    csize i;

    for (i = 0; str[i] != '\0'; i++)
        if (i > comparison_length)
            return true;

    return false;
}

/**
 * c_hostname_to_ascii:
 * @hostname: a valid UTF-8 or ASCII hostname
 *
 * Converts @hostname to its canonical ASCII form; an ASCII-only
 * string containing no uppercase letters and not ending with a
 * trailing dot.
 *
 * Returns: (nullable) (transfer full): an ASCII hostname, which must be freed,
 *    or %NULL if @hostname is in some way invalid.
 *
 * Since: 2.22
 **/
cchar*
c_hostname_to_ascii(const cchar * hostname)
{
    cchar * name, * label, * p;
    CString * out;
    cssize llen, oldlen;
    bool unicode;
    csize hostname_max_length_bytes = get_hostname_max_length_bytes();

    /* Do an initial check on the hostname length, as overlong hostnames take a
     * long time in the IDN cleanup algorithm in nameprep(). The ultimate
     * restriction is that the IDN-decoded (i.e. pure ASCII) hostname cannot be
     * longer than 255 bytes. That’s the least restrictive limit on hostname
     * length of all the ways hostnames can be interpreted. Typically, the
     * hostname will be an FQDN, which is limited to 253 bytes long. POSIX
     * hostnames are limited to `get_hostname_max_length_bytes()` (typically 255
     * bytes).
     *
     * See https://stackoverflow.com/a/28918017/2931197
     *
     * It’s possible for a hostname to be %-encoded, in which case its decoded
     * length will be as much as 3× shorter.
     *
     * It’s also possible for a hostname to use overlong UTF-8 encodings, in which
     * case its decoded length will be as much as 4× shorter.
     *
     * Note: This check is not intended as an absolute guarantee that a hostname
     * is the right length and will be accepted by other systems. It’s intended to
     * stop wildly-invalid hostnames from taking forever in nameprep().
     */
    if (hostname_max_length_bytes <= C_MAX_SIZE / 4 && strlen_greater_than(hostname, 4 * C_MAX(255, hostname_max_length_bytes)))
        return NULL;

    label = name = nameprep(hostname, -1, &unicode);
    if (!name || !unicode)
        return name;

    out = c_string_new(NULL);

    do {
        unicode = false;
        for (p = label; *p && !idna_is_dot(p); p++) {
            if ((cuchar) * p > 0x80)
                unicode = true;
        }

        oldlen = out->len;
        llen = p - label;
        if (unicode) {
            if (!strncmp(label, IDNA_ACE_PREFIX, IDNA_ACE_PREFIX_LEN))
                goto fail;

            c_string_append(out, IDNA_ACE_PREFIX);
            if (!punycode_encode(label, llen, out))
                goto fail;
        }
        else
            c_string_append_len(out, label, llen);

        if (out->len - oldlen > 63)
            goto fail;

        label += llen;
        if (*label)
            label = c_utf8_next_char(label);
        if (*label)
            c_string_append_c(out, '.');
    }
    while (*label);

    c_free(name);
    return c_string_free(out, false);

fail:
    c_free(name);
    c_string_free(out, true);
    return NULL;
}

bool c_hostname_is_non_ascii(const cchar * hostname)
{
    return contains_non_ascii(hostname, -1);
}

static bool punycode_decode(const cchar * input, csize input_length, CString * output)
{
    CArray * output_chars;
    cunichar n;
    cuint i, bias;
    cuint oldi, w, k, digit, t;
    const cchar * split;

    n = PUNYCODE_INITIAL_N;
    i = 0;
    bias = PUNYCODE_INITIAL_BIAS;

    split = input + input_length - 1;
    while (split > input && *split != '-')
        split--;
    if (split > input) {
        output_chars = c_array_sized_new(false, false, sizeof (cunichar), split - input);
        input_length -= (split - input) + 1;
        while (input < split) {
            cunichar ch = (cunichar) * input++;
            if (!PUNYCODE_IS_BASIC(ch))
                goto fail;
            c_array_append_val(output_chars, ch);
        }
        input++;
    }
    else
        output_chars = c_array_new(false, false, sizeof (cunichar));

    while (input_length) {
        oldi = i;
        w = 1;
        for (k = PUNYCODE_BASE; ; k += PUNYCODE_BASE) {
            if (!input_length--)
                goto fail;
            digit = decode_digit(*input++);
            if (digit >= PUNYCODE_BASE)
                goto fail;
            if (digit > (C_MAX_UINT - i) / w)
                goto fail;
            i += digit * w;
            if (k <= bias)
                t = PUNYCODE_TMIN;
            else if (k >= bias + PUNYCODE_TMAX)
                t = PUNYCODE_TMAX;
            else
                t = k - bias;
            if (digit < t)
                break;
            if (w > C_MAX_UINT / (PUNYCODE_BASE - t))
                goto fail;
            w *= (PUNYCODE_BASE - t);
        }

        bias = adapt(i - oldi, output_chars->len + 1, oldi == 0);

        if (i / (output_chars->len + 1) > C_MAX_UINT - n)
            goto fail;
        n += i / (output_chars->len + 1);
        i %= (output_chars->len + 1);

        c_array_insert_val(output_chars, i++, n);
    }

    for (i = 0; i < output_chars->len; i++)
        c_string_append_unichar(output, c_array_index(output_chars, cunichar, i));
    c_array_free(output_chars, true);
    return true;

fail:
    c_array_free(output_chars, true);
    return false;
}

cchar* c_hostname_to_unicode(const cchar * hostname)
{
    CString * out;
    cssize llen;
    csize hostname_max_length_bytes = get_hostname_max_length_bytes();

    c_return_val_if_fail(hostname != NULL, NULL);

    /* See the comment at the top of c_hostname_to_ascii(). */
    if (hostname_max_length_bytes <= C_MAX_SIZE / 4 &&
        strlen_greater_than(hostname, 4 * C_MAX(255, hostname_max_length_bytes)))
        return NULL;

    out = c_string_new(NULL);

    do {
        llen = idna_end_of_label(hostname) - hostname;
        if (!c_ascii_strncasecmp(hostname, IDNA_ACE_PREFIX, IDNA_ACE_PREFIX_LEN)) {
            hostname += IDNA_ACE_PREFIX_LEN;
            llen -= IDNA_ACE_PREFIX_LEN;
            if (!punycode_decode(hostname, llen, out)) {
                c_string_free(out, true);
                return NULL;
            }
        }
        else {
            bool unicode;
            cchar * canonicalized = nameprep(hostname, llen, &unicode);

            if (!canonicalized) {
                c_string_free(out, true);
                return NULL;
            }
            c_string_append(out, canonicalized);
            c_free(canonicalized);
        }

        hostname += llen;
        if (*hostname)
            hostname = c_utf8_next_char(hostname);
        if (*hostname)
            c_string_append_c(out, '.');
    }
    while (*hostname);

    return c_string_free(out, false);
}

bool c_hostname_is_ascii_encoded(const cchar * hostname)
{
    while (1) {
        if (!c_ascii_strncasecmp(hostname, IDNA_ACE_PREFIX, IDNA_ACE_PREFIX_LEN))
            return true;
        hostname = idna_end_of_label(hostname);
        if (*hostname)
            hostname = c_utf8_next_char(hostname);
        if (!*hostname)
            return false;
    }
}

bool c_hostname_is_ip_address(const cchar * hostname)
{
    cchar * p, * end;
    cint nsegments, octet;

    /* On Linux we could implement this using inet_pton, but the Windows
     * equivalent of that requires linking against winsock, so we just
     * figure this out ourselves. Tested by tests/hostutils.c.
     */

    p = (char*)hostname;

    if (strchr(p, ':')) {
        bool skipped;

        /* If it contains a ':', it's an IPv6 address (assuming it's an
         * IP address at all). This consists of eight ':'-separated
         * segments, each containing a 1-4 digit hex number, except that
         * optionally: (a) the last two segments can be replaced by an
         * IPv4 address, and (b) a single span of 1 to 8 "0000" segments
         * can be replaced with just "::".
         */

        nsegments = 0;
        skipped = false;
        while (*p && *p != '%' && nsegments < 8) {
            /* Each segment after the first must be preceded by a ':'.
             * (We also handle half of the "string starts with ::" case
             * here.)
             */
            if (p != (char*)hostname || (p[0] == ':' && p[1] == ':')) {
                if (*p != ':')
                    return false;
                p++;
            }

            /* If there's another ':', it means we're skipping some segments */
            if (*p == ':' && !skipped) {
                skipped = true;
                nsegments++;

                /* Handle the "string ends with ::" case */
                if (!p[1])
                    p++;

                continue;
            }

            /* Read the segment, make sure it's valid. */
            for (end = p; c_ascii_isxdigit(*end); end++);
            if (end == p || end > p + 4)
                return false;

            if (*end == '.') {
                if ((nsegments == 6 && !skipped) || (nsegments <= 6 && skipped))
                    goto parse_ipv4;
                else
                    return false;
            }

            nsegments++;
            p = end;
        }

        return (!*p || (p[0] == '%' && p[1])) && (nsegments == 8 || skipped);
    }

parse_ipv4:

    /* Parse IPv4: N.N.N.N, where each N <= 255 and doesn't have leading 0s. */
    for (nsegments = 0; nsegments < 4; nsegments++) {
        if (nsegments != 0) {
            if (*p != '.')
                return false;
            p++;
        }

        /* Check the segment; a little tricker than the IPv6 case since
         * we can't allow extra leading 0s, and we can't assume that all
         * strings of valid length are within range.
         */
        octet = 0;
        if (*p == '0')
            end = p + 1;
        else {
            for (end = p; c_ascii_isdigit(*end); end++) {
                octet = 10 * octet + (*end - '0');

                if (octet > 255)
                    break;
            }
        }
        if (end == p || end > p + 3 || octet > 255)
            return false;

        p = end;
    }

    /* If there's nothing left to parse, then it's ok. */
    return !*p;
}
