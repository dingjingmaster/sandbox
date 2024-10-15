
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-6-12.
//

#ifndef clibrary_CLIBRARY_OPTION_H
#define clibrary_CLIBRARY_OPTION_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/error.h>
#include <c/quark.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C

#define C_OPTION_ERROR (c_option_error_quark ())
#define C_OPTION_REMAINING ""
#define C_OPTION_ENTRY_NULL \
    { NULL, 0, 0, 0, NULL, NULL, NULL }

typedef struct _COptionGroup   COptionGroup;
typedef struct _COptionEntry   COptionEntry;
typedef struct _COptionContext COptionContext;

typedef enum
{
    C_OPTION_FLAG_NONE            = 0,
    C_OPTION_FLAG_HIDDEN          = 1 << 0,
    C_OPTION_FLAG_IN_MAIN         = 1 << 1,
    C_OPTION_FLAG_REVERSE         = 1 << 2,
    C_OPTION_FLAG_NO_ARG          = 1 << 3,
    C_OPTION_FLAG_FILENAME        = 1 << 4,
    C_OPTION_FLAG_OPTIONAL_ARG    = 1 << 5,
    C_OPTION_FLAG_NOALIAS         = 1 << 6
} COptionFlags;

typedef enum
{
    C_OPTION_ARG_NONE,
    C_OPTION_ARG_STRING,
    C_OPTION_ARG_INT,
    C_OPTION_ARG_CALLBACK,
    C_OPTION_ARG_FILENAME,
    C_OPTION_ARG_STRING_ARRAY,
    C_OPTION_ARG_FILENAME_ARRAY,
    C_OPTION_ARG_DOUBLE,
    C_OPTION_ARG_INT64
} COptionArg;

typedef bool (*COptionArgFunc) (const cchar* optionName, const cchar* value, void* udata, CError** error);
typedef bool (*COptionParseFunc) (COptionContext* context, COptionGroup* group, void* udata, CError** error);
typedef void (*COptionErrorFunc) (COptionContext* context, COptionGroup* group, void* udata, CError** error);


typedef enum
{
    C_OPTION_ERROR_UNKNOWN_OPTION,
    C_OPTION_ERROR_BAD_VALUE,
    C_OPTION_ERROR_FAILED
} COptionError;

CQuark c_option_error_quark (void);

struct _COptionEntry
{
    const cchar*        longName;
    cchar               shortName;
    cint                flags;

    COptionArg          arg;
    void*               argData;

    const cchar*        description;
    const cchar*        argDescription;
};

COptionContext* c_option_context_new                        (const cchar* parameterString);
void            c_option_context_set_summary                (COptionContext* context, const cchar *summary);
const cchar*    c_option_context_get_summary                (COptionContext* context);
void            c_option_context_set_description            (COptionContext* context, const cchar* description);
const cchar*    c_option_context_get_description            (COptionContext* context);
void            c_option_context_free                       (COptionContext* context);
void            c_option_context_set_help_enabled           (COptionContext* context, bool helpEnabled);
bool            c_option_context_get_help_enabled           (COptionContext* context);
void            c_option_context_set_ignore_unknown_options (COptionContext* context, bool ignoreUnknown);
bool            c_option_context_get_ignore_unknown_options (COptionContext* context);
void            c_option_context_set_strict_posix           (COptionContext* context, bool strictPosix);
bool            c_option_context_get_strict_posix           (COptionContext* context);
void            c_option_context_add_main_entries           (COptionContext* context, const COptionEntry  *entries, const cchar* translationDomain);
bool            c_option_context_parse                      (COptionContext* context, cint* argc, cchar*** argv, CError** error);
bool            c_option_context_parse_strv                 (COptionContext* context, cchar*** arguments, CError** error);
void            c_option_context_set_translate_func         (COptionContext* context, CTranslateFunc func, void* data, CDestroyNotify destroyNotify);
void            c_option_context_set_translation_domain     (COptionContext* context, const cchar* domain);
void            c_option_context_add_group                  (COptionContext *context, COptionGroup* group);
void            c_option_context_set_main_group             (COptionContext *context, COptionGroup* group);
COptionGroup*   c_option_context_get_main_group             (COptionContext *context);
cchar*          c_option_context_get_help                   (COptionContext *context, bool mainHelp, COptionGroup* group);
COptionGroup*   c_option_group_new                          (const cchar* name, const cchar* description, const cchar* helpDescription, void* udata, CDestroyNotify destroy);
void            c_option_group_set_parse_hooks              (COptionGroup* group, COptionParseFunc preParseFunc, COptionParseFunc postParseFunc);
void            c_option_group_set_error_hook               (COptionGroup* group, COptionErrorFunc errorFunc);
void            c_option_group_free                         (COptionGroup* group);
COptionGroup*   c_option_group_ref                          (COptionGroup* group);
void            c_option_group_unref                        (COptionGroup* group);
void            c_option_group_add_entries                  (COptionGroup* group, const COptionEntry *entries);
void            c_option_group_set_translate_func           (COptionGroup* group, CTranslateFunc func, void* data, CDestroyNotify destroyNotify);
void            c_option_group_set_translation_domain       (COptionGroup* group, const cchar* domain);


C_END_EXTERN_C

#endif // clibrary_CLIBRARY_OPTION_H
