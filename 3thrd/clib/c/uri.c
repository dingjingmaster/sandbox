/* vim: set tabstop=4 shiftwidth=4 expandtab: */
// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 6/9/24.
//

#include "uri.h"

#include "clib.h"

#define XDIGIT(c)         ((c) <= '9' ? (c) - '0' : ((c) & 0x4F) - 'A' + 10)
#define HEXCHAR(s)        ((XDIGIT (s[1]) << 4) + XDIGIT (s[2]))

struct _CUri
{
    cchar*              scheme;
    cchar*              userinfo;
    cchar*              host;
    cint                port;
    cchar*              path;
    cchar*              query;
    cchar*              fragment;

    cchar*              user;
    cchar*              password;
    cchar*              authParams;

    CUriFlags           flags;
};


typedef struct
{
    CUriParamsFlags     flags;
    const cchar*        attr;
    const cchar*        end;
    cuint8              sepTable[256]; /* 1 = index is a separator; 0 otherwise */
} RealIter;

C_STATIC_ASSERT(sizeof (CUriParamsIter) == sizeof (RealIter));
C_STATIC_ASSERT(C_ALIGNOF (CUriParamsIter) >= C_ALIGNOF (RealIter));


static cssize uri_decoder(cchar ** out, const cchar * illegal_chars, const cchar * start, csize length, bool just_normalize, bool www_form, CUriFlags flags, CUriError parse_error, CError ** error);


cchar* c_uri_to_strinc_partial(CUri* uri, CUriHideFlags flags);
void _uri_encoder (CString* out, const cuchar* start, csize length, const char* reservedCharsAllowed, bool allowUtf8);


extern const char * const gsUtf8Skip;


CUri* c_uri_ref(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return c_atomic_rc_box_acquire(uri);
}

static void c_uri_clear(CUri * uri)
{
    c_free(uri->scheme);
    c_free(uri->userinfo);
    c_free(uri->host);
    c_free(uri->path);
    c_free(uri->query);
    c_free(uri->fragment);
    c_free(uri->user);
    c_free(uri->password);
    c_free(uri->authParams);
}

void c_uri_unref(CUri * uri)
{
    c_return_if_fail(uri != NULL);

    c_atomic_rc_box_release_full(uri, (CDestroyNotify)c_uri_clear);
}

static bool c_uri_char_is_unreserved(cchar ch)
{
    if (c_ascii_isalnum(ch)) {
        return true;
    }

    return ch == '-' || ch == '.' || ch == '_' || ch == '~';
}


static cssize uri_decoder(cchar ** out, const cchar * illegal_chars, const cchar * start, csize length, bool just_normalize, bool www_form, CUriFlags flags, CUriError parse_error, CError ** error)
{
    cchar c;
    CString * decoded;
    const cchar * invalid, * s, * end;
    cssize len;

    if (!(flags & C_URI_FLAGS_ENCODED)) {
        just_normalize = false;
    }

    decoded = c_string_sized_new(length + 1);
    for (s = start, end = s + length; s < end; s++) {
        if (*s == '%') {
            if (s + 2 >= end || !c_ascii_isxdigit(s[1]) || !c_ascii_isxdigit(s[2])) {
                /* % followed by non-hex or the end of the string; this is an error */
                if (!(flags & C_URI_FLAGS_PARSE_RELAXED)) {
                    c_set_error_literal(error, C_URI_ERROR, parse_error, /* xgettext: no-c-format */ _("Invalid %-encoding in URI"));
                    c_string_free(decoded, true);
                    return -1;
                }

                c_string_append_c(decoded, *s);
                continue;
            }

            c = HEXCHAR(s);
            if (illegal_chars && strchr(illegal_chars, c)) {
                c_set_error_literal(error, C_URI_ERROR, parse_error, _("Illegal character in URI"));
                c_string_free(decoded, true);
                return -1;
            }

            if (just_normalize && !c_uri_char_is_unreserved(c)) {
                /* Leave the % sequence there but normalize it. */
                c_string_append_c(decoded, *s);
                c_string_append_c(decoded, c_ascii_toupper(s[1]));
                c_string_append_c(decoded, c_ascii_toupper(s[2]));
                s += 2;
            }
            else {
                c_string_append_c(decoded, c);
                s += 2;
            }
        }
        else if (www_form && *s == '+') {
            c_string_append_c(decoded, ' ');
        }
        else if (just_normalize && (!c_ascii_isgraph(*s))) {
            c_string_append_printf(decoded, "%%%02X", (cuchar)*s);
        }
        else {
            c_string_append_c(decoded, *s);
        }
    }

    len = decoded->len;
    c_assert(len >= 0);

    if (!(flags & C_URI_FLAGS_ENCODED) && !c_utf8_validate(decoded->str, len, &invalid)) {
        c_set_error_literal(error, C_URI_ERROR, parse_error, _("Non-UTF-8 characters in URI"));
        c_string_free(decoded, true);
        return -1;
    }

    if (out) {
        *out = c_string_free(decoded, false);
    }
    else {
        c_string_free(decoded, true);
    }

    return len;
}

static bool  uri_decode(cchar ** out, const cchar * illegal_chars, const cchar * start, csize length, bool www_form, CUriFlags flags, CUriError parse_error, CError ** error)
{
    return uri_decoder(out, illegal_chars, start, length, false, www_form, flags, parse_error, error) != -1;
}

static bool uri_normalize(cchar ** out, const cchar * start, csize length, CUriFlags flags, CUriError parse_error, CError ** error)
{
    return uri_decoder(out, NULL, start, length, true, false, flags, parse_error, error) != -1;
}

static bool is_valid(cuchar c, const cchar * reserved_chars_allowed)
{
    if (c_uri_char_is_unreserved(c))
        return true;

    if (reserved_chars_allowed && strchr(reserved_chars_allowed, c))
        return true;

    return false;
}

void _uri_encoder(CString * out, const cuchar * start, csize length, const cchar * reserved_chars_allowed, bool allow_utf8)
{
    static const cchar hex[] = "0123456789ABCDEF";
    const cuchar * p = start;
    const cuchar * end = p + length;

    while (p < end) {
        cunichar multibyte_utf8_char = 0;

        if (allow_utf8 && *p >= 0x80)
            multibyte_utf8_char = c_utf8_get_char_validated((cchar*)p, end - p);

        if (multibyte_utf8_char > 0 &&
            multibyte_utf8_char != (cunichar) - 1 && multibyte_utf8_char != (cunichar) - 2) {
            cint len = gsUtf8Skip[*p];
            c_string_append_len(out, (cchar*)p, len);
            p += len;
        }
        else if (is_valid(*p, reserved_chars_allowed)) {
            c_string_append_c(out, *p);
            p++;
        }
        else {
            c_string_append_c(out, '%');
            c_string_append_c(out, hex[*p >> 4]);
            c_string_append_c(out, hex[*p & 0xf]);
            p++;
        }
    }
}

/* Parse the IP-literal construction from RFC 6874 (which extends RFC 3986 to
 * support IPv6 zone identifiers.
 *
 * Currently, IP versions beyond 6 (i.e. the IPvFuture rule) are unsupported.
 * There’s no point supporting them until (a) they exist and (b) the rest of the
 * stack (notably, sockets) supports them.
 *
 * Rules:
 *
 * IP-literal = "[" ( IPv6address / IPv6addrz / IPvFuture  ) "]"
 *
 * ZoneID = 1*( unreserved / pct-encoded )
 *
 * IPv6addrz = IPv6address "%25" ZoneID
 *
 * If %C_URI_FLAGS_PARSE_RELAXED is specified, this function also accepts:
 *
 * IPv6addrz = IPv6address "%" ZoneID
 */
static bool parse_ip_literal(const cchar * start, csize length, CUriFlags flags, cchar ** out, CError ** error)
{
    cchar * pct, * zone_id = NULL;
    cchar * addr = NULL;
    csize addr_length = 0;
    csize zone_id_length = 0;
    cchar * decoded_zone_id = NULL;

    if (start[length - 1] != ']')
        goto bad_ipv6_literal;

    /* Drop the square brackets */
    addr = c_strndup(start + 1, length - 2);
    addr_length = length - 2;

    /* If there's an IPv6 scope ID, split out the zone. */
    pct = strchr(addr, '%');
    if (pct != NULL) {
        *pct = '\0';

        if (addr_length - (pct - addr) >= 4 &&
            *(pct + 1) == '2' && *(pct + 2) == '5') {
            zone_id = pct + 3;
            zone_id_length = addr_length - (zone_id - addr);
        }
        else if (flags & C_URI_FLAGS_PARSE_RELAXED &&
            addr_length - (pct - addr) >= 2) {
            zone_id = pct + 1;
            zone_id_length = addr_length - (zone_id - addr);
        }
        else
            goto bad_ipv6_literal;

        c_assert(zone_id_length >= 1);
    }

    /* addr must be an IPv6 address */
    if (!c_hostname_is_ip_address(addr) || !strchr(addr, ':'))
        goto bad_ipv6_literal;

    /* Zone ID must be valid. It can contain %-encoded characters. */
    if (zone_id != NULL &&
        !uri_decode(&decoded_zone_id, NULL, zone_id, zone_id_length, false,
                    flags, C_URI_ERROR_BAD_HOST, NULL))
        goto bad_ipv6_literal;

    /* Success */
    if (out != NULL && decoded_zone_id != NULL)
        *out = c_strconcat(addr, "%", decoded_zone_id, NULL);
    else if (out != NULL)
        *out = c_steal_pointer(&addr);

    c_free(addr);
    c_free(decoded_zone_id);

    return true;

bad_ipv6_literal:
    c_free(addr);
    c_free(decoded_zone_id);
    c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_HOST,
                _("Invalid IPv6 address ‘%.*s’ in URI"),
                (cint)length, start);

    return false;
}

static bool parse_host(const cchar * start, csize length, CUriFlags flags, cchar ** out, CError ** error)
{
    cchar * decoded = NULL, * host;
    cchar * addr = NULL;

    if (*start == '[') {
        if (!parse_ip_literal(start, length, flags, &host, error))
            return false;
        goto ok;
    }

    if (c_ascii_isdigit(*start)) {
        addr = c_strndup(start, length);
        if (c_hostname_is_ip_address(addr)) {
            host = addr;
            goto ok;
        }
        c_free(addr);
    }

    if (flags & C_URI_FLAGS_NON_DNS) {
        if (!uri_normalize(&decoded, start, length, flags,
                           C_URI_ERROR_BAD_HOST, error))
            return false;
        host = c_steal_pointer(&decoded);
        goto ok;
    }

    flags &= ~C_URI_FLAGS_ENCODED;
    if (!uri_decode(&decoded, NULL, start, length, false, flags,
                    C_URI_ERROR_BAD_HOST, error))
        return false;

    /* You're not allowed to %-encode an IP address, so if it wasn't
     * one before, it better not be one now.
     */
    if (c_hostname_is_ip_address(decoded)) {
        c_free(decoded);
        c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_HOST,
                    _("Illegal encoded IP address ‘%.*s’ in URI"),
                    (cint)length, start);
        return false;
    }

    if (c_hostname_is_non_ascii(decoded)) {
        host = c_hostname_to_ascii(decoded);
        if (host == NULL) {
            c_free(decoded);
            c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_HOST,
                        _("Illegal internationalized hostname ‘%.*s’ in URI"),
                        (cint)length, start);
            return false;
        }
    }
    else {
        host = c_steal_pointer(&decoded);
    }

ok:
    if (out)
        *out = c_steal_pointer(&host);
    c_free(host);
    c_free(decoded);

    return true;
}

static bool parse_port(const cchar * start, csize length, cint * out, CError ** error)
{
    cchar * end;
    culong parsed_port;

    /* strtoul() allows leading + or -, so we have to check this first. */
    if (!c_ascii_isdigit(*start)) {
        c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_PORT,
                    _("Could not parse port ‘%.*s’ in URI"),
                    (cint)length, start);
        return false;
    }

    /* We know that *(start + length) is either '\0' or a non-numeric
     * character, so strtoul() won't scan beyond it.
     */
    parsed_port = strtoul(start, &end, 10);
    if (end != start + length) {
        c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_PORT,
                    _("Could not parse port ‘%.*s’ in URI"),
                    (cint)length, start);
        return false;
    }
    else if (parsed_port > 65535) {
        c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_PORT,
                    _("Port ‘%.*s’ in URI is out of range"),
                    (cint)length, start);
        return false;
    }

    if (out)
        *out = parsed_port;
    return true;
}

static bool parse_userinfo(const cchar * start, csize length, CUriFlags flags, cchar ** user, cchar ** password, cchar ** auth_params, CError ** error)
{
    const cchar * user_end = NULL, * password_end = NULL, * auth_params_end;

    auth_params_end = start + length;
    if (flags & C_URI_FLAGS_HAS_AUTH_PARAMS)
        password_end = memchr(start, ';', auth_params_end - start);
    if (!password_end)
        password_end = auth_params_end;
    if (flags & C_URI_FLAGS_HAS_PASSWORD)
        user_end = memchr(start, ':', password_end - start);
    if (!user_end)
        user_end = password_end;

    if (!uri_normalize(user, start, user_end - start, flags,
                       C_URI_ERROR_BAD_USER, error))
        return false;

    if (*user_end == ':') {
        start = user_end + 1;
        if (!uri_normalize(password, start, password_end - start, flags,
                           C_URI_ERROR_BAD_PASSWORD, error)) {
            if (user) {
                c_clear_pointer((void*)user, c_free0);
            }
            return false;
        }
    }
    else if (password)
        *password = NULL;

    if (*password_end == ';') {
        start = password_end + 1;
        if (!uri_normalize(auth_params, start, auth_params_end - start, flags,
                           C_URI_ERROR_BAD_AUTH_PARAMS, error)) {
            if (user)
                c_clear_pointer((void*)user, c_free0);
            if (password)
                c_clear_pointer((void*)password, c_free0);
            return false;
        }
    }
    else if (auth_params)
        *auth_params = NULL;

    return true;
}

static cchar* uri_cleanup(const cchar * uri_string)
{
    CString * copy;
    const cchar * end;

    /* Skip leading whitespace */
    while (c_ascii_isspace(*uri_string))
        uri_string++;

    /* Ignore trailing whitespace */
    end = uri_string + strlen(uri_string);
    while (end > uri_string && c_ascii_isspace(*(end - 1)))
        end--;

    /* Copy the rest, encoding unencoded spaces and stripping other whitespace */
    copy = c_string_sized_new(end - uri_string);
    while (uri_string < end) {
        if (*uri_string == ' ')
            c_string_append(copy, "%20");
        else if (c_ascii_isspace(*uri_string));
        else
            c_string_append_c(copy, *uri_string);
        uri_string++;
    }

    return c_string_free(copy, false);
}

static bool should_normalize_empty_path(const char * scheme)
{
    const char * const schemes[] = {"https", "http", "wss", "ws"};
    csize i;
    for (i = 0; i < C_N_ELEMENTS(schemes); ++i) {
        if (!strcmp(schemes[i], scheme))
            return true;
    }
    return false;
}

static int normalize_port(const char * scheme, int port)
{
    const char * default_schemes[3] = {NULL};
    int i;

    switch (port) {
    case 21:
        default_schemes[0] = "ftp";
        break;
    case 80:
        default_schemes[0] = "http";
        default_schemes[1] = "ws";
        break;
    case 443:
        default_schemes[0] = "https";
        default_schemes[1] = "wss";
        break;
    default:
        break;
    }

    for (i = 0; default_schemes[i]; ++i) {
        if (!strcmp(scheme, default_schemes[i]))
            return -1;
    }

    return port;
}

int c_uri_get_default_scheme_port(const char * scheme)
{
    if (strcmp(scheme, "http") == 0 || strcmp(scheme, "ws") == 0)
        return 80;

    if (strcmp(scheme, "https") == 0 || strcmp(scheme, "wss") == 0)
        return 443;

    if (strcmp(scheme, "ftp") == 0)
        return 21;

    if (strstr(scheme, "socks") == scheme)
        return 1080;

    return -1;
}

static bool c_uri_split_internal(const cchar * uri_string, CUriFlags flags, cchar ** scheme, cchar ** userinfo, cchar ** user, cchar ** password, cchar ** auth_params, cchar ** host, cint * port, cchar ** path, cchar ** query, cchar ** fragment, CError ** error)
{
    const cchar * end, * colon, * at, * path_start, * semi, * question;
    const cchar * p, * bracket, * hostend;
    cchar * cleaned_uri_string = NULL;
    cchar * normalized_scheme = NULL;

    if (scheme)
        *scheme = NULL;
    if (userinfo)
        *userinfo = NULL;
    if (user)
        *user = NULL;
    if (password)
        *password = NULL;
    if (auth_params)
        *auth_params = NULL;
    if (host)
        *host = NULL;
    if (port)
        *port = -1;
    if (path)
        *path = NULL;
    if (query)
        *query = NULL;
    if (fragment)
        *fragment = NULL;

    if ((flags & C_URI_FLAGS_PARSE_RELAXED) && strpbrk(uri_string, " \t\n\r")) {
        cleaned_uri_string = uri_cleanup(uri_string);
        uri_string = cleaned_uri_string;
    }

    /* Find scheme */
    p = uri_string;
    while (*p && (c_ascii_isalpha(*p) ||
        (p > uri_string && (c_ascii_isdigit(*p) ||
            *p == '.' || *p == '+' || *p == '-'))))
        p++;

    if (p > uri_string && *p == ':') {
        normalized_scheme = c_ascii_strdown(uri_string, p - uri_string);
        if (scheme)
            *scheme = c_steal_pointer(&normalized_scheme);
        p++;
    }
    else {
        if (scheme)
            *scheme = NULL;
        p = uri_string;
    }

    /* Check for authority */
    if (strncmp(p, "//", 2) == 0) {
        p += 2;

        path_start = p + strcspn(p, "/?#");
        at = memchr(p, '@', path_start - p);
        if (at) {
            if (flags & C_URI_FLAGS_PARSE_RELAXED) {
                cchar * next_at;

                /* Any "@"s in the userinfo must be %-encoded, but
                 * people get this wrong sometimes. Since "@"s in the
                 * hostname are unlikely (and also wrong anyway), assume
                 * that if there are extra "@"s, they belong in the
                 * userinfo.
                 */
                do {
                    next_at = memchr(at + 1, '@', path_start - (at + 1));
                    if (next_at)
                        at = next_at;
                }
                while (next_at);
            }

            if (user || password || auth_params ||
                (flags & (C_URI_FLAGS_HAS_PASSWORD | C_URI_FLAGS_HAS_AUTH_PARAMS))) {
                if (!parse_userinfo(p, at - p, flags,
                                    user, password, auth_params,
                                    error))
                    goto fail;
            }

            if (!uri_normalize(userinfo, p, at - p, flags,
                               C_URI_ERROR_BAD_USER, error))
                goto fail;

            p = at + 1;
        }

        if (flags & C_URI_FLAGS_PARSE_RELAXED) {
            semi = strchr(p, ';');
            if (semi && semi < path_start) {
                path_start = semi;
            }
        }

        if (*p == '[') {
            bracket = memchr(p, ']', path_start - p);
            if (bracket && *(bracket + 1) == ':')
                colon = bracket + 1;
            else
                colon = NULL;
        }
        else
            colon = memchr(p, ':', path_start - p);

        hostend = colon ? colon : path_start;
        if (!parse_host(p, hostend - p, flags, host, error))
            goto fail;

        if (colon && colon != path_start - 1) {
            p = colon + 1;
            if (!parse_port(p, path_start - p, port, error))
                goto fail;
        }

        p = path_start;
    }

    /* Find fragment. */
    end = p + strcspn(p, "#");
    if (*end == '#') {
        if (!uri_normalize(fragment, end + 1, strlen(end + 1),
                           flags | (flags & C_URI_FLAGS_ENCODED_FRAGMENT ? C_URI_FLAGS_ENCODED : 0),
                           C_URI_ERROR_BAD_FRAGMENT, error))
            goto fail;
    }

    /* Find query */
    question = memchr(p, '?', end - p);
    if (question) {
        if (!uri_normalize(query, question + 1, end - (question + 1),
                           flags | (flags & C_URI_FLAGS_ENCODED_QUERY ? C_URI_FLAGS_ENCODED : 0),
                           C_URI_ERROR_BAD_QUERY, error))
            goto fail;
        end = question;
    }

    if (!uri_normalize(path, p, end - p,
                       flags | (flags & C_URI_FLAGS_ENCODED_PATH ? C_URI_FLAGS_ENCODED : 0),
                       C_URI_ERROR_BAD_PATH, error))
        goto fail;

    /* Scheme-based normalization */
    if (flags & C_URI_FLAGS_SCHEME_NORMALIZE && ((scheme && *scheme) || normalized_scheme)) {
        const char * scheme_str = scheme && *scheme ? *scheme : normalized_scheme;

        if (should_normalize_empty_path(scheme_str) && path && !**path) {
            c_free(*path);
            *path = c_strdup("/");
        }

        if (port && *port == -1)
            *port = c_uri_get_default_scheme_port(scheme_str);
    }

    c_free(normalized_scheme);
    c_free(cleaned_uri_string);
    return true;

fail:
    if (scheme)
        c_clear_pointer(C_POINTER(scheme), c_free0);
    if (userinfo)
        c_clear_pointer(C_POINTER(userinfo), c_free0);
    if (host)
        c_clear_pointer(C_POINTER(host), c_free0);
    if (port)
        *port = -1;
    if (path)
        c_clear_pointer(C_POINTER(path), c_free0);
    if (query)
        c_clear_pointer(C_POINTER(query), c_free0);
    if (fragment)
        c_clear_pointer(C_POINTER(fragment), c_free0);

    c_free(normalized_scheme);
    c_free(cleaned_uri_string);
    return false;
}


bool c_uri_split(const cchar * uri_ref, CUriFlags flags, cchar ** scheme, cchar ** userinfo, cchar ** host, cint * port, cchar ** path, cchar ** query, cchar ** fragment, CError ** error)
{
    c_return_val_if_fail(uri_ref != NULL, false);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    return c_uri_split_internal(uri_ref, flags,
                                scheme, userinfo, NULL, NULL, NULL,
                                host, port, path, query, fragment,
                                error);
}


bool
c_uri_split_with_user(const cchar * uri_ref,
                      CUriFlags flags,
                      cchar ** scheme,
                      cchar ** user,
                      cchar ** password,
                      cchar ** auth_params,
                      cchar ** host,
                      cint * port,
                      cchar ** path,
                      cchar ** query,
                      cchar ** fragment,
                      CError ** error)
{
    c_return_val_if_fail(uri_ref != NULL, false);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    return c_uri_split_internal(uri_ref, flags,
                                scheme, NULL, user, password, auth_params,
                                host, port, path, query, fragment,
                                error);
}


bool
c_uri_split_network(const cchar * uri_string,
                    CUriFlags flags,
                    cchar ** scheme,
                    cchar ** host,
                    cint * port,
                    CError ** error)
{
    cchar * my_scheme = NULL, * my_host = NULL;

    c_return_val_if_fail(uri_string != NULL, false);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    if (!c_uri_split_internal(uri_string, flags,
                              &my_scheme, NULL, NULL, NULL, NULL,
                              &my_host, port, NULL, NULL, NULL,
                              error))
        return false;

    if (!my_scheme || !my_host) {
        if (!my_scheme) {
            c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_SCHEME,
                        _("URI ‘%s’ is not an absolute URI"),
                        uri_string);
        }
        else {
            c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_HOST,
                        _("URI ‘%s’ has no host component"),
                        uri_string);
        }
        c_free(my_scheme);
        c_free(my_host);

        return false;
    }

    if (scheme)
        *scheme = c_steal_pointer(&my_scheme);
    if (host)
        *host = c_steal_pointer(&my_host);

    c_free(my_scheme);
    c_free(my_host);

    return true;
}

bool
c_uri_is_valid(const cchar * uri_string,
               CUriFlags flags,
               CError ** error)
{
    cchar * my_scheme = NULL;

    c_return_val_if_fail(uri_string != NULL, false);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    if (!c_uri_split_internal(uri_string, flags,
                              &my_scheme, NULL, NULL, NULL, NULL,
                              NULL, NULL, NULL, NULL, NULL,
                              error))
        return false;

    if (!my_scheme) {
        c_set_error(error, C_URI_ERROR, C_URI_ERROR_BAD_SCHEME,
                    _("URI ‘%s’ is not an absolute URI"),
                    uri_string);
        return false;
    }

    c_free(my_scheme);

    return true;
}


static void
remove_dot_segments(cchar * path)
{
    /* The output can be written to the same buffer that the input
     * is read from, as the output pointer is only ever increased
     * when the input pointer is increased as well, and the input
     * pointer is never decreased. */
    cchar * input = path;
    cchar * output = path;

    if (!*path)
        return;

    while (*input) {
        /*  A.  If the input buffer begins with a prefix of "../" or "./",
         *      then remove that prefix from the input buffer; otherwise,
         */
        if (strncmp(input, "../", 3) == 0)
            input += 3;
        else if (strncmp(input, "./", 2) == 0)
            input += 2;

            /*  B.  if the input buffer begins with a prefix of "/./" or "/.",
             *      where "." is a complete path segment, then replace that
             *      prefix with "/" in the input buffer; otherwise,
             */
        else if (strncmp(input, "/./", 3) == 0)
            input += 2;
        else if (strcmp(input, "/.") == 0)
            input[1] = '\0';

            /*  C.  if the input buffer begins with a prefix of "/../" or "/..",
             *      where ".." is a complete path segment, then replace that
             *      prefix with "/" in the input buffer and remove the last
             *      segment and its preceding "/" (if any) from the output
             *      buffer; otherwise,
             */
        else if (strncmp(input, "/../", 4) == 0) {
            input += 3;
            if (output > path) {
                do {
                    output--;
                }
                while (*output != '/' && output > path);
            }
        }
        else if (strcmp(input, "/..") == 0) {
            input[1] = '\0';
            if (output > path) {
                do {
                    output--;
                }
                while (*output != '/' && output > path);
            }
        }

        /*  D.  if the input buffer consists only of "." or "..", then remove
         *      that from the input buffer; otherwise,
         */
        else if (strcmp(input, "..") == 0 || strcmp(input, ".") == 0)
            input[0] = '\0';

        /*  E.  move the first path segment in the input buffer to the end of
         *      the output buffer, including the initial "/" character (if
         *      any) and any subsequent characters up to, but not including,
         *      the next "/" character or the end of the input buffer.
         */
        else {
            *output++ = *input++;
            while (*input && *input != '/')
                *output++ = *input++;
        }
    }
    *output = '\0';
}


CUri*
c_uri_parse(const cchar * uri_string,
            CUriFlags flags,
            CError ** error)
{
    c_return_val_if_fail(uri_string != NULL, NULL);
    c_return_val_if_fail(error == NULL || *error == NULL, NULL);

    return c_uri_parse_relative(NULL, uri_string, flags, error);
}


CUri*
c_uri_parse_relative(CUri * base_uri,
                     const cchar * uri_ref,
                     CUriFlags flags,
                     CError ** error)
{
    CUri * uri = NULL;

    c_return_val_if_fail(uri_ref != NULL, NULL);
    c_return_val_if_fail(error == NULL || *error == NULL, NULL);
    c_return_val_if_fail(base_uri == NULL || base_uri->scheme != NULL, NULL);


    uri = c_atomic_rc_box_new0(CUri);
    uri->flags = flags;

    if (!c_uri_split_internal(uri_ref, flags,
                              &uri->scheme, &uri->userinfo,
                              &uri->user, &uri->password, &uri->authParams,
                              &uri->host, &uri->port,
                              &uri->path, &uri->query, &uri->fragment,
                              error)) {
        c_uri_unref(uri);
        return NULL;
    }

    if (!uri->scheme && !base_uri) {
        c_set_error_literal(error, C_URI_ERROR, C_URI_ERROR_FAILED,
                            _("URI is not absolute, and no base URI was provided"));
        c_uri_unref(uri);
        return NULL;
    }

    if (base_uri) {
        if (uri->scheme)
            remove_dot_segments(uri->path);
        else {
            uri->scheme = c_strdup(base_uri->scheme);
            if (uri->host)
                remove_dot_segments(uri->path);
            else {
                if (!*uri->path) {
                    c_free(uri->path);
                    uri->path = c_strdup(base_uri->path);
                    if (!uri->query)
                        uri->query = c_strdup(base_uri->query);
                }
                else {
                    if (*uri->path == '/')
                        remove_dot_segments(uri->path);
                    else {
                        cchar * newpath, * last;

                        last = strrchr(base_uri->path, '/');
                        if (last) {
                            newpath = c_strdup_printf("%.*s/%s",
                                                      (cint)(last - base_uri->path),
                                                      base_uri->path,
                                                      uri->path);
                        }
                        else
                            newpath = c_strdup_printf("/%s", uri->path);

                        c_free(uri->path);
                        uri->path = c_steal_pointer(&newpath);

                        remove_dot_segments(uri->path);
                    }
                }

                uri->userinfo = c_strdup(base_uri->userinfo);
                uri->user = c_strdup(base_uri->user);
                uri->password = c_strdup(base_uri->password);
                uri->authParams = c_strdup(base_uri->authParams);
                uri->host = c_strdup(base_uri->host);
                uri->port = base_uri->port;
            }
        }

        if (flags & C_URI_FLAGS_SCHEME_NORMALIZE) {
            if (should_normalize_empty_path(uri->scheme) && !*uri->path) {
                c_free(uri->path);
                uri->path = c_strdup("/");
            }

            uri->port = normalize_port(uri->scheme, uri->port);
        }
    }
    else {
        remove_dot_segments(uri->path);
    }

    return c_steal_pointer(&uri);
}


cchar*
c_uri_resolve_relative(const cchar * base_uri_string,
                       const cchar * uri_ref,
                       CUriFlags flags,
                       CError ** error)
{
    CUri * base_uri, * resolved_uri;
    cchar * resolved_uri_string;

    c_return_val_if_fail(uri_ref != NULL, NULL);
    c_return_val_if_fail(error == NULL || *error == NULL, NULL);

    flags |= C_URI_FLAGS_ENCODED;

    if (base_uri_string) {
        base_uri = c_uri_parse(base_uri_string, flags, error);
        if (!base_uri)
            return NULL;
    }
    else
        base_uri = NULL;

    resolved_uri = c_uri_parse_relative(base_uri, uri_ref, flags, error);
    if (base_uri)
        c_uri_unref(base_uri);
    if (!resolved_uri)
        return NULL;

    resolved_uri_string = c_uri_to_string(resolved_uri);
    c_uri_unref(resolved_uri);
    return c_steal_pointer(&resolved_uri_string);
}


#define USERINFO_ALLOWED_CHARS C_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO
#define USER_ALLOWED_CHARS "!$&'()*+,="
#define PASSWORD_ALLOWED_CHARS "!$&'()*+,=:"
#define AUTH_PARAMS_ALLOWED_CHARS USERINFO_ALLOWED_CHARS
#define IP_ADDR_ALLOWED_CHARS ":"
#define HOST_ALLOWED_CHARS C_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS
#define PATH_ALLOWED_CHARS C_URI_RESERVED_CHARS_ALLOWED_IN_PATH
#define QUERY_ALLOWED_CHARS C_URI_RESERVED_CHARS_ALLOWED_IN_PATH "?"
#define FRAGMENT_ALLOWED_CHARS C_URI_RESERVED_CHARS_ALLOWED_IN_PATH "?"

static cchar*
c_uri_join_internal(CUriFlags flags,
                    const cchar * scheme,
                    bool userinfo,
                    const cchar * user,
                    const cchar * password,
                    const cchar * auth_params,
                    const cchar * host,
                    cint port,
                    const cchar * path,
                    const cchar * query,
                    const cchar * fragment)
{
    bool encoded = (flags & C_URI_FLAGS_ENCODED);
    CString * str;
    char * normalized_scheme = NULL;


    c_return_val_if_fail(path != NULL, NULL);
    c_return_val_if_fail(host == NULL || (path[0] == '\0' || path[0] == '/'), NULL);
    c_return_val_if_fail(host != NULL || (path[0] != '/' || path[1] != '/'), NULL);

    /* Arbitrarily chosen default size which should handle most average length
     * URIs. This should avoid a few reallocations of the buffer in most cases.
     * It’s 1B shorter than a power of two, since CString will add a
     * nul-terminator byte. */
    str = c_string_sized_new(127);

    if (scheme) {
        c_string_append(str, scheme);
        c_string_append_c(str, ':');
    }

    if (flags & C_URI_FLAGS_SCHEME_NORMALIZE && scheme && ((host && port != -1) || path[0] == '\0'))
        normalized_scheme = c_ascii_strdown(scheme, -1);

    if (host) {
        c_string_append(str, "//");

        if (user) {
            if (encoded)
                c_string_append(str, user);
            else {
                if (userinfo)
                    c_string_append_uri_escaped(str, user, USERINFO_ALLOWED_CHARS, true);
                else
                    /* Encode ':' and ';' regardless of whether we have a
                     * password or auth params, since it may be parsed later
                     * under the assumption that it does.
                     */
                    c_string_append_uri_escaped(str, user, USER_ALLOWED_CHARS, true);
            }

            if (password) {
                c_string_append_c(str, ':');
                if (encoded)
                    c_string_append(str, password);
                else
                    c_string_append_uri_escaped(str, password, PASSWORD_ALLOWED_CHARS, true);
            }

            if (auth_params) {
                c_string_append_c(str, ';');
                if (encoded)
                    c_string_append(str, auth_params);
                else
                    c_string_append_uri_escaped(str, auth_params,
                                                AUTH_PARAMS_ALLOWED_CHARS, true);
            }

            c_string_append_c(str, '@');
        }

        if (strchr(host, ':') && c_hostname_is_ip_address(host)) {
            c_string_append_c(str, '[');
            if (encoded)
                c_string_append(str, host);
            else
                c_string_append_uri_escaped(str, host, IP_ADDR_ALLOWED_CHARS, true);
            c_string_append_c(str, ']');
        }
        else {
            if (encoded)
                c_string_append(str, host);
            else
                c_string_append_uri_escaped(str, host, HOST_ALLOWED_CHARS, true);
        }

        if (port != -1 && (!normalized_scheme || normalize_port(normalized_scheme, port) != -1))
            c_string_append_printf(str, ":%d", port);
    }

    if (path[0] == '\0' && normalized_scheme && should_normalize_empty_path(normalized_scheme))
        c_string_append(str, "/");
    else if (encoded || flags & C_URI_FLAGS_ENCODED_PATH)
        c_string_append(str, path);
    else
        c_string_append_uri_escaped(str, path, PATH_ALLOWED_CHARS, true);

    c_free(normalized_scheme);

    if (query) {
        c_string_append_c(str, '?');
        if (encoded || flags & C_URI_FLAGS_ENCODED_QUERY)
            c_string_append(str, query);
        else
            c_string_append_uri_escaped(str, query, QUERY_ALLOWED_CHARS, true);
    }
    if (fragment) {
        c_string_append_c(str, '#');
        if (encoded || flags & C_URI_FLAGS_ENCODED_FRAGMENT)
            c_string_append(str, fragment);
        else
            c_string_append_uri_escaped(str, fragment, FRAGMENT_ALLOWED_CHARS, true);
    }

    return c_string_free(str, false);
}


cchar*
c_uri_join(CUriFlags flags,
           const cchar * scheme,
           const cchar * userinfo,
           const cchar * host,
           cint port,
           const cchar * path,
           const cchar * query,
           const cchar * fragment)
{
    c_return_val_if_fail(port >= -1 && port <= 65535, NULL);
    c_return_val_if_fail(path != NULL, NULL);

    return c_uri_join_internal(flags,
                               scheme,
                               true, userinfo, NULL, NULL,
                               host,
                               port,
                               path,
                               query,
                               fragment);
}


cchar*
c_uri_join_with_user(CUriFlags flags,
                     const cchar * scheme,
                     const cchar * user,
                     const cchar * password,
                     const cchar * auth_params,
                     const cchar * host,
                     cint port,
                     const cchar * path,
                     const cchar * query,
                     const cchar * fragment)
{
    c_return_val_if_fail(port >= -1 && port <= 65535, NULL);
    c_return_val_if_fail(path != NULL, NULL);

    return c_uri_join_internal(flags,
                               scheme,
                               false, user, password, auth_params,
                               host,
                               port,
                               path,
                               query,
                               fragment);
}

CUri*
c_uri_build(CUriFlags flags,
            const cchar * scheme,
            const cchar * userinfo,
            const cchar * host,
            cint port,
            const cchar * path,
            const cchar * query,
            const cchar * fragment)
{
    CUri * uri;

    c_return_val_if_fail(scheme != NULL, NULL);
    c_return_val_if_fail(port >= -1 && port <= 65535, NULL);
    c_return_val_if_fail(path != NULL, NULL);

    uri = c_atomic_rc_box_new0(CUri);
    uri->flags = flags;
    uri->scheme = c_ascii_strdown(scheme, -1);
    uri->userinfo = c_strdup(userinfo);
    uri->host = c_strdup(host);
    uri->port = port;
    uri->path = c_strdup(path);
    uri->query = c_strdup(query);
    uri->fragment = c_strdup(fragment);

    return c_steal_pointer(&uri);
}


CUri*
c_uri_build_with_user(CUriFlags flags,
                      const cchar * scheme,
                      const cchar * user,
                      const cchar * password,
                      const cchar * auth_params,
                      const cchar * host,
                      cint port,
                      const cchar * path,
                      const cchar * query,
                      const cchar * fragment)
{
    CUri * uri;
    CString * userinfo;

    c_return_val_if_fail(scheme != NULL, NULL);
    c_return_val_if_fail(password == NULL || user != NULL, NULL);
    c_return_val_if_fail(auth_params == NULL || user != NULL, NULL);
    c_return_val_if_fail(port >= -1 && port <= 65535, NULL);
    c_return_val_if_fail(path != NULL, NULL);

    uri = c_atomic_rc_box_new0(CUri);
    uri->flags = flags | C_URI_FLAGS_HAS_PASSWORD;
    uri->scheme = c_ascii_strdown(scheme, -1);
    uri->user = c_strdup(user);
    uri->password = c_strdup(password);
    uri->authParams = c_strdup(auth_params);
    uri->host = c_strdup(host);
    uri->port = port;
    uri->path = c_strdup(path);
    uri->query = c_strdup(query);
    uri->fragment = c_strdup(fragment);

    if (user) {
        userinfo = c_string_new(user);
        if (password) {
            c_string_append_c(userinfo, ':');
            c_string_append(userinfo, uri->password);
        }
        if (auth_params) {
            c_string_append_c(userinfo, ';');
            c_string_append(userinfo, uri->authParams);
        }
        uri->userinfo = c_string_free(userinfo, false);
    }

    return c_steal_pointer(&uri);
}


cchar*
c_uri_to_string(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return c_uri_to_strinc_partial(uri, C_URI_HIDE_NONE);
}


cchar* c_uri_to_strinc_partial(CUri* uri, CUriHideFlags flags)
{
    bool hide_user = (flags & C_URI_HIDE_USERINFO);
    bool hide_password = (flags & (C_URI_HIDE_USERINFO | C_URI_HIDE_PASSWORD));
    bool hide_auth_params = (flags & (C_URI_HIDE_USERINFO | C_URI_HIDE_AUTH_PARAMS));
    bool hide_query = (flags & C_URI_HIDE_QUERY);
    bool hide_fragment = (flags & C_URI_HIDE_FRAGMENT);

    c_return_val_if_fail(uri != NULL, NULL);

    if (uri->flags & (C_URI_FLAGS_HAS_PASSWORD | C_URI_FLAGS_HAS_AUTH_PARAMS)) {
        return c_uri_join_with_user(uri->flags,
                                    uri->scheme,
                                    hide_user ? NULL : uri->user,
                                    hide_password ? NULL : uri->password,
                                    hide_auth_params ? NULL : uri->authParams,
                                    uri->host,
                                    uri->port,
                                    uri->path,
                                    hide_query ? NULL : uri->query,
                                    hide_fragment ? NULL : uri->fragment);
    }

    return c_uri_join(uri->flags,
                      uri->scheme,
                      hide_user ? NULL : uri->userinfo,
                      uri->host,
                      uri->port,
                      uri->path,
                      hide_query ? NULL : uri->query,
                      hide_fragment ? NULL : uri->fragment);
}

/* This is just a copy of c_str_hash() with c_ascii_toupper() added */
static cuint str_ascii_case_hash(const void* v)
{
    const signed char * p;
    cuint32 h = 5381;

    for (p = v; *p != '\0'; p++) {
        h = (h << 5) + h + c_ascii_toupper(*p);
    }

    return h;
}

static bool str_ascii_case_equal(const void* v1, const void* v2)
{
    const cchar * string1 = v1;
    const cchar * string2 = v2;

    return c_ascii_strcasecmp(string1, string2) == 0;
}


void
c_uri_params_iter_init(CUriParamsIter * iter,
                       const cchar * params,
                       cssize length,
                       const cchar * separators,
                       CUriParamsFlags flags)
{
    RealIter * ri = (RealIter*)iter;
    const cchar * s;

    c_return_if_fail(iter != NULL);
    c_return_if_fail(length == 0 || params != NULL);
    c_return_if_fail(length >= -1);
    c_return_if_fail(separators != NULL);

    ri->flags = flags;

    if (length == -1) {
        ri->end = params + strlen(params);
    }
    else {
        ri->end = params + length;
    }

    memset(ri->sepTable, false, sizeof (ri->sepTable));
    for (s = separators; *s != '\0'; ++s) {
        ri->sepTable[*(cuchar*)s] = true;
    }

    ri->attr = params;
}

bool
c_uri_params_iter_next(CUriParamsIter * iter,
                       cchar ** attribute,
                       cchar ** value,
                       CError ** error)
{
    RealIter * ri = (RealIter*)iter;
    const cchar * attr_end, * val, * val_end;
    cchar * decoded_attr, * decoded_value;
    bool www_form = ri->flags & C_URI_PARAMS_WWW_FORM;
    CUriFlags decode_flags = C_URI_FLAGS_NONE;

    c_return_val_if_fail(iter != NULL, false);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    /* Pre-clear these in case of failure or finishing. */
    if (attribute)
        *attribute = NULL;
    if (value)
        *value = NULL;

    if (ri->attr >= ri->end)
        return false;

    if (ri->flags & C_URI_PARAMS_PARSE_RELAXED)
        decode_flags |= C_URI_FLAGS_PARSE_RELAXED;

    for (val_end = ri->attr; val_end < ri->end; val_end++)
        if (ri->sepTable[*(cuchar*)val_end])
            break;

    attr_end = memchr(ri->attr, '=', val_end - ri->attr);
    if (!attr_end) {
        c_set_error_literal(error, C_URI_ERROR, C_URI_ERROR_FAILED, _("Missing ‘=’ and parameter value"));
        return false;
    }
    if (!uri_decode(&decoded_attr, NULL, ri->attr, attr_end - ri->attr, www_form, decode_flags, C_URI_ERROR_FAILED, error)) {
        return false;
    }

    val = attr_end + 1;
    if (!uri_decode(&decoded_value, NULL, val, val_end - val,
                    www_form, decode_flags, C_URI_ERROR_FAILED, error)) {
        c_free(decoded_attr);
        return false;
    }

    if (attribute)
        *attribute = c_steal_pointer(&decoded_attr);
    if (value)
        *value = c_steal_pointer(&decoded_value);

    c_free(decoded_attr);
    c_free(decoded_value);

    ri->attr = val_end + 1;
    return true;
}


CHashTable* c_uri_parse_params(const cchar * params, cssize length, const cchar * separators, CUriParamsFlags flags, CError ** error)
{
    CHashTable * hash;
    CUriParamsIter iter;
    cchar * attribute, * value;
    CError * err = NULL;

    c_return_val_if_fail(length == 0 || params != NULL, NULL);
    c_return_val_if_fail(length >= -1, NULL);
    c_return_val_if_fail(separators != NULL, NULL);
    c_return_val_if_fail(error == NULL || *error == NULL, false);

    if (flags & C_URI_PARAMS_CASE_INSENSITIVE) {
        hash = c_hash_table_new_full(str_ascii_case_hash, str_ascii_case_equal, c_free0, c_free0);
    }
    else {
        hash = c_hash_table_new_full(c_str_hash, c_str_equal, c_free0, c_free0);
    }

    c_uri_params_iter_init(&iter, params, length, separators, flags);

    while (c_uri_params_iter_next(&iter, &attribute, &value, &err))
        c_hash_table_insert(hash, attribute, value);

    if (err) {
        c_propagate_error(error, c_steal_pointer(&err));
        c_hash_table_destroy(hash);
        return NULL;
    }

    return c_steal_pointer(&hash);
}


const cchar* c_uri_get_scheme(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->scheme;
}


const cchar* c_uri_get_userinfo(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->userinfo;
}


const cchar* c_uri_get_user(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->user;
}


const cchar* c_uri_get_password(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->password;
}


const cchar* c_uri_get_auth_params(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->authParams;
}


const cchar* c_uri_get_host(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->host;
}

cint c_uri_get_port(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, -1);

    if (uri->port == -1 && uri->flags & C_URI_FLAGS_SCHEME_NORMALIZE) {
        return c_uri_get_default_scheme_port(uri->scheme);
    }

    return uri->port;
}


const cchar* c_uri_get_path(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->path;
}


const cchar* c_uri_get_query(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->query;
}


const cchar* c_uri_get_fragment(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, NULL);

    return uri->fragment;
}


CUriFlags c_uri_get_flags(CUri * uri)
{
    c_return_val_if_fail(uri != NULL, C_URI_FLAGS_NONE);

    return uri->flags;
}

cchar* c_uri_unescape_segment(const cchar * escaped_string, const cchar * escaped_strinc_end, const cchar * illegal_characters)
{
    cchar * unescaped;
    csize length;
    cssize decoded_len;

    if (!escaped_string) {
        return NULL;
    }

    if (escaped_strinc_end)
        length = escaped_strinc_end - escaped_string;
    else
        length = strlen(escaped_string);

    decoded_len = uri_decoder(&unescaped,
                              illegal_characters,
                              escaped_string, length,
                              false, false,
                              C_URI_FLAGS_ENCODED,
                              0, NULL);
    if (decoded_len < 0) {
        return NULL;
    }

    if (memchr(unescaped, '\0', decoded_len)) {
        c_free(unescaped);
        return NULL;
    }

    return unescaped;
}


cchar* c_uri_unescape_string(const cchar * escaped_string, const cchar * illegal_characters)
{
    return c_uri_unescape_segment(escaped_string, NULL, illegal_characters);
}

cchar* c_uri_escape_string(const cchar * unescaped, const cchar * reserved_chars_allowed, bool allow_utf8)
{
    CString * s;

    c_return_val_if_fail(unescaped != NULL, NULL);

    s = c_string_sized_new(strlen(unescaped) * 1.25);

    c_string_append_uri_escaped(s, unescaped, reserved_chars_allowed, allow_utf8);

    return c_string_free(s, false);
}


CBytes* c_uri_unescape_bytes(const cchar * escaped_string, cssize length, const char * illegal_characters, CError ** error)
{
    cchar * buf;
    cssize unescaped_length;

    c_return_val_if_fail(escaped_string != NULL, NULL);
    c_return_val_if_fail(error == NULL || *error == NULL, NULL);

    if (length == -1) {
        length = strlen(escaped_string);
    }

    unescaped_length = uri_decoder(&buf,
                                   illegal_characters,
                                   escaped_string, length,
                                   false,
                                   false,
                                   C_URI_FLAGS_ENCODED,
                                   C_URI_ERROR_FAILED, error);
    if (unescaped_length == -1) {
        return NULL;
    }

    return c_bytes_new_take(buf, unescaped_length);
}


cchar* c_uri_escape_bytes(const cuint8 * unescaped, csize length, const cchar * reserved_chars_allowed)
{
    CString * string;

    c_return_val_if_fail(unescaped != NULL, NULL);

    string = c_string_sized_new(length * 1.25);

    _uri_encoder(string, unescaped, length, reserved_chars_allowed, false);

    return c_string_free(string, false);
}

static cssize c_uri_scheme_length(const cchar * uri)
{
    const cchar * p;

    p = uri;
    if (!c_ascii_isalpha(*p)) {
        return -1;
    }
    p++;
    while (c_ascii_isalnum(*p) || *p == '.' || *p == '+' || *p == '-') {
        p++;
    }

    if (p > uri && *p == ':') {
        return p - uri;
    }

    return -1;
}

cchar* c_uri_parse_scheme(const cchar * uri)
{
    cssize len;

    c_return_val_if_fail(uri != NULL, NULL);

    len = c_uri_scheme_length(uri);
    return len == -1 ? NULL : c_strndup(uri, len);
}

const cchar* c_uri_peek_scheme(const cchar * uri)
{
    cssize len;
    cchar * lower_scheme;
    const cchar * scheme;

    c_return_val_if_fail(uri != NULL, NULL);

    len = c_uri_scheme_length(uri);
    if (len == -1) {
        return NULL;
    }

    lower_scheme = c_ascii_strdown(uri, len);
    scheme = c_intern_string(lower_scheme);
    c_free(lower_scheme);

    return scheme;
}

C_DEFINE_QUARK(c-uri-quark, c_uri_error)
