// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 6/9/24.
//

#ifndef clibrary_CHARSET_H
#define clibrary_CHARSET_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

C_BEGIN_EXTERN_C

bool                    c_get_charset                       (const char **charset);
cchar *                 c_get_codeset                       (void);
bool                    c_get_console_charset               (const char **charset);

const cchar * const *   c_get_language_names                (void);
const cchar * const *   c_get_language_names_with_category  (const cchar *category_name);
cchar **                c_get_locale_variants               (const cchar *locale);


/* private */
extern const char**  _c_charset_get_aliases (const char *canonicalName);
extern bool          _c_get_time_charset    (const char **charset);
extern bool          _c_get_ctype_charset   (const char **charset);

C_END_EXTERN_C

#endif // clibrary_CHARSET_H
