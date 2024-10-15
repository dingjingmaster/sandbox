// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 6/9/24.
//

#ifndef clibrary_URI_H
#define clibrary_URI_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>
#include <c/hash-table.h>

C_BEGIN_EXTERN_C

#define C_URI_ERROR (c_uri_error_quark ())

#define C_URI_RESERVED_CHARS_GENERIC_DELIMITERS         ":/?#[]@"
#define C_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS    "!$&'()*+,;="
#define C_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT    C_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS ":@"
#define C_URI_RESERVED_CHARS_ALLOWED_IN_PATH            C_URI_RESERVED_CHARS_ALLOWED_IN_PATH_ELEMENT "/"
#define C_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO        C_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS ":"


typedef struct _CUri                CUri;
typedef struct _CUriParamsIter      CUriParamsIter;


enum _CUriHideFlags
{
    C_URI_HIDE_NONE        = 0,
    C_URI_HIDE_USERINFO    = 1 << 0,
    C_URI_HIDE_PASSWORD    = 1 << 1,
    C_URI_HIDE_AUTH_PARAMS = 1 << 2,
    C_URI_HIDE_QUERY       = 1 << 3,
    C_URI_HIDE_FRAGMENT    = 1 << 4,
};

enum _CUriError
{
    C_URI_ERROR_FAILED,
    C_URI_ERROR_BAD_SCHEME,
    C_URI_ERROR_BAD_USER,
    C_URI_ERROR_BAD_PASSWORD,
    C_URI_ERROR_BAD_AUTH_PARAMS,
    C_URI_ERROR_BAD_HOST,
    C_URI_ERROR_BAD_PORT,
    C_URI_ERROR_BAD_PATH,
    C_URI_ERROR_BAD_QUERY,
    C_URI_ERROR_BAD_FRAGMENT,
};

enum _CUriFlags
{
    C_URI_FLAGS_NONE            = 0,
    C_URI_FLAGS_PARSE_RELAXED   = 1 << 0,
    C_URI_FLAGS_HAS_PASSWORD    = 1 << 1,
    C_URI_FLAGS_HAS_AUTH_PARAMS = 1 << 2,
    C_URI_FLAGS_ENCODED         = 1 << 3,
    C_URI_FLAGS_NON_DNS         = 1 << 4,
    C_URI_FLAGS_ENCODED_QUERY   = 1 << 5,
    C_URI_FLAGS_ENCODED_PATH    = 1 << 6,
    C_URI_FLAGS_ENCODED_FRAGMENT = 1 << 7,
    C_URI_FLAGS_SCHEME_NORMALIZE = 1 << 8,
};

enum _CUriParamsFlags
{
    C_URI_PARAMS_NONE             = 0,
    C_URI_PARAMS_CASE_INSENSITIVE = 1 << 0,
    C_URI_PARAMS_WWW_FORM         = 1 << 1,
    C_URI_PARAMS_PARSE_RELAXED    = 1 << 2,
};

typedef enum _CUriError             CUriError;
typedef enum _CUriFlags             CUriFlags;
typedef enum _CUriHideFlags         CUriHideFlags;
typedef enum _CUriParamsFlags       CUriParamsFlags;

struct _CUriParamsIter
{
    /*< private >*/
    cint        dummy0;
    void*       dummy1;
    void*       dummy2;
    cuint8      dummy3[256];
};

extern void _uri_encoder (CString* out, const cuchar* start, csize length, const char* reservedCharsAllowed, bool allowUtf8);

CUri*       c_uri_ref                   (CUri *uri);
void        c_uri_unref                 (CUri *uri);
bool        c_uri_split                 (const cchar* uriRef,
                                            CUriFlags flags,
                                            cchar** scheme,
                                            cchar** userinfo,
                                            cchar** host,
                                            cint* port,
                                            cchar** path,
                                            cchar** query,
                                            cchar** fragment,
                                            CError** error);
bool        c_uri_split_with_user       (const cchar* uriRef,
                                            CUriFlags flags,
                                            cchar** scheme,
                                            cchar** user,
                                            cchar** password,
                                            cchar** authParams,
                                            cchar** host,
                                            cint* port,
                                            cchar** path,
                                            cchar** query,
                                            cchar** fragment,
                                            CError **error);
bool     c_uri_split_network            (const cchar* uriStr,
                                            CUriFlags flags,
                                            cchar** scheme,
                                            cchar** host,
                                            cint* port,
                                            CError** error);
bool     c_uri_is_valid                 (const cchar* uriString,
                                            CUriFlags flags,
                                            CError** error);
cchar*   c_uri_join                     (CUriFlags flags,
                                            const cchar* scheme,
                                            const cchar* userinfo,
                                            const cchar* host,
                                            cint port,
                                            const cchar* path,
                                            const cchar* query,
                                            const cchar* fragment);
cchar*  c_uri_join_with_user            (CUriFlags flags,
                                            const cchar* scheme,
                                            const cchar* user,
                                            const cchar* password,
                                            const cchar* authParams,
                                            const cchar* host,
                                            cint port,
                                            const cchar* path,
                                            const cchar* query,
                                            const cchar* fragment);
CUri*   c_uri_parse                     (const cchar* uriString,
                                            CUriFlags flags,
                                            CError** error);
CUri*   c_uri_parse_relative            (CUri* baseUri,
                                            const cchar* uriRef,
                                            CUriFlags flags,
                                            CError** error);
cchar*  c_uri_resolve_relative          (const cchar* baseUriString,
                                            const cchar* uriRef,
                                            CUriFlags flags,
                                            CError** error);

CUri*   c_uri_build                     (CUriFlags flags,
                                            const cchar* scheme,
                                            const cchar* userinfo,
                                            const cchar* host,
                                            cint port,
                                            const cchar* path,
                                            const cchar* query,
                                            const cchar* fragment);
CUri*   c_uri_build_with_user           (CUriFlags flags,
                                            const cchar* scheme,
                                            const cchar* user,
                                            const cchar* password,
                                            const cchar* authParams,
                                            const cchar* host,
                                            cint port,
                                            const cchar* path,
                                            const cchar* query,
                                            const cchar* fragment);
char*   c_uri_to_string                 (CUri* uri);
char*   c_uri_to_string_partial         (CUri* uri,
                                            CUriHideFlags flags);

const cchar* c_uri_get_scheme           (CUri* uri);
const cchar* c_uri_get_userinfo         (CUri* uri);
const cchar* c_uri_get_user             (CUri* uri);
const cchar* c_uri_get_password         (CUri* uri);
const cchar* c_uri_get_auth_params      (CUri* uri);
const cchar* c_uri_get_host             (CUri* uri);
cint         c_uri_get_port             (CUri* uri);
const cchar* c_uri_get_path             (CUri* uri);
const cchar* c_uri_get_query            (CUri* uri);
const cchar* c_uri_get_fragment         (CUri* uri);
CUriFlags    c_uri_get_flags            (CUri* uri);
CHashTable*  c_uri_parse_params         (const cchar* params,
                                            cssize length,
                                            const cchar* separators,
                                            CUriParamsFlags flags,
                                            CError** error);
void        c_uri_params_iter_init      (CUriParamsIter* iter,
                                            const cchar* params,
                                            cssize length,
                                            const cchar* separators,
                                            CUriParamsFlags flags);
bool        c_uri_params_iter_next      (CUriParamsIter* iter,
                                            cchar** attribute,
                                            cchar** value,
                                            CError** error);
CQuark      c_uri_error_quark           (void);
char*       c_uri_unescape_string       (const char *escapedString,
                                            const char *illegalCharacters);
char*       c_uri_unescape_segment      (const char *escapedString,
                                            const char *escapedStringEnd,
                                            const char *illegalCharacters);
char*       c_uri_parse_scheme          (const char *uri);
const char* c_uri_peek_scheme           (const char *uri);
char*       c_uri_escape_string         (const char *unescaped,
                                            const char *reservedCharsAllowed,
                                            bool allowUtf8);
CBytes*     c_uri_unescape_bytes        (const char* escapedString,
                                            cssize length,
                                            const char* illegalCharacters,
                                            CError** error);
char*       c_uri_escape_bytes          (const cuint8 *unescaped,
                                            csize length,
                                            const char* reservedCharsAllowed);

C_END_EXTERN_C

#endif // clibrary_URI_H
