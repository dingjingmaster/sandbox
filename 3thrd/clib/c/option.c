
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

#include "option.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <printf.h>
#include <libintl.h>

#include "clib.h"

#if defined __OpenBSD__
#include <unistd.h>
#include <sys/sysctl.h>
#endif

#define OPTIONAL_ARG(entry)     ((entry)->arg == C_OPTION_ARG_CALLBACK && (entry)->flags & C_OPTION_FLAG_OPTIONAL_ARG)
#define TRANSLATE(group, str)   (((group)->translateFunc ? (* (group)->translateFunc) ((str), (group)->translateData) : (str)))
#define NO_ARG(entry)           ((entry)->arg == C_OPTION_ARG_NONE || ((entry)->arg == C_OPTION_ARG_CALLBACK && ((entry)->flags & C_OPTION_FLAG_NO_ARG)))

C_DEFINE_QUARK (c-option-context-error-quark, c_option_error)

typedef struct
{
    COptionArg  argType;
    void*       argData;
    union
    {
        bool boolean;
        cint integer;
        cchar *str;
        cchar **array;
        cdouble dbl;
        cint64 int64;
    } prev;
    union
    {
        cchar *str;
        struct
        {
            cint len;
            cchar **data;
        } array;
    } allocated;
} Change;

typedef struct
{
    cchar **ptr;
    cchar *value;
} PendingNull;

struct _COptionContext
{
    CList*              groups;

    cchar*              parameterString;  /* (nullable) */
    cchar*              summary;
    cchar*              description;

    CTranslateFunc      translateFunc;
    CDestroyNotify      translateNotify;
    void*               translateData;

    cuint               helpEnabled   : 1;
    cuint               ignoreUnknown : 1;
    cuint               strvMode      : 1;
    cuint               strictPosix   : 1;

    COptionGroup*       mainGroup;

    /* We keep a list of change so we can revert them */
    CList*              changes;

    /* We also keep track of all argv elements
     * that should be NULLed or modified.
     */
    CList*              pendingNulls;
};

struct _COptionGroup
{
    cchar*              name;
    cchar*              description;
    cchar*              helpDescription;

    cint                refCount;

    CDestroyNotify      destroyNotify;
    void*               userData;

    CTranslateFunc      translateFunc;
    CDestroyNotify      translateNotify;
    void*               translateData;

    COptionEntry*       entries;
    csize               nEntries;

    COptionErrorFunc    errorFunc;
    COptionParseFunc    preParseFunc;
    COptionParseFunc    postParseFunc;
};

static void free_changes_list   (COptionContext *context, bool revert);
static void free_pending_nulls  (COptionContext *context, bool performNulls);

static int _c_unichar_get_width (cunichar c)
{
    if (C_UNLIKELY (c_unichar_iszerowidth (c))) {
        return 0;
    }

    if (c_unichar_iswide (c)) {
        return 2;
    }

    return 1;
}

static clong _c_utf8_strwidth (const cchar *p)
{
    clong len = 0;
    c_return_val_if_fail (p != NULL, 0);

    while (*p) {
        len += _c_unichar_get_width (c_utf8_get_char (p));
        p = c_utf8_next_char (p);
    }

    return len;
}


COptionContext * c_option_context_new (const cchar *parameter_string)
{
    COptionContext *context;

    context = c_malloc0(sizeof(COptionContext));

    if (parameter_string != NULL && *parameter_string == '\0') {
        parameter_string = NULL;
    }

    context->parameterString = c_strdup (parameter_string);
    context->strictPosix = false;
    context->helpEnabled = true;
    context->ignoreUnknown = false;

    return context;
}

void c_option_context_free (COptionContext *context)
{
    c_return_if_fail (context != NULL);

    c_list_free_full (context->groups, (CDestroyNotify) c_option_group_unref);

    if (context->mainGroup)
        c_option_group_unref (context->mainGroup);

    free_changes_list (context, false);
    free_pending_nulls (context, false);

    c_free (context->parameterString);
    c_free (context->summary);
    c_free (context->description);

    if (context->translateNotify)
        (* context->translateNotify) (context->translateData);

    c_free (context);
}


void c_option_context_set_help_enabled (COptionContext *context,
                                        bool        help_enabled)

{
    c_return_if_fail (context != NULL);

    context->helpEnabled = help_enabled;
}

bool
c_option_context_get_help_enabled (COptionContext *context)
{
    c_return_val_if_fail (context != NULL, false);

    return context->helpEnabled;
}

void
c_option_context_set_ignore_unknown_options (COptionContext *context,
                                             bool        ignore_unknown)
{
    c_return_if_fail (context != NULL);

    context->ignoreUnknown = ignore_unknown;
}

bool c_option_context_get_ignore_unknown_options (COptionContext *context)
{
    c_return_val_if_fail (context != NULL, false);

    return context->ignoreUnknown;
}

void c_option_context_set_strict_posix (COptionContext *context, bool strict_posix)
{
    c_return_if_fail (context != NULL);

    context->strictPosix = strict_posix;
}

bool c_option_context_get_strict_posix (COptionContext *context)
{
    c_return_val_if_fail (context != NULL, false);

    return context->strictPosix;
}

void c_option_context_add_group (COptionContext *context, COptionGroup   *group)
{
    CList *list;

    c_return_if_fail (context != NULL);
    c_return_if_fail (group != NULL);
    c_return_if_fail (group->name != NULL);
    c_return_if_fail (group->description != NULL);
    c_return_if_fail (group->helpDescription != NULL);

    for (list = context->groups; list; list = list->next)
    {
        COptionGroup *g = (COptionGroup *)list->data;

        if ((group->name == NULL && g->name == NULL) ||
            (group->name && g->name && strcmp (group->name, g->name) == 0))
        C_LOG_WARNING ("A group named \"%s\" is already part of this COptionContext",
                       group->name);
    }

    context->groups = c_list_append (context->groups, group);
}

void
c_option_context_set_main_group (COptionContext *context,
                                 COptionGroup   *group)
{
    c_return_if_fail (context != NULL);
    c_return_if_fail (group != NULL);

    if (context->mainGroup)
    {
        C_LOG_WARNING ("This COptionContext already has a main group");

        return;
    }

    context->mainGroup = group;
}

COptionGroup *
c_option_context_get_main_group (COptionContext *context)
{
    c_return_val_if_fail (context != NULL, NULL);

    return context->mainGroup;
}

void
c_option_context_add_main_entries (COptionContext      *context,
                                   const COptionEntry  *entries,
                                   const cchar         *translation_domain)
{
    c_return_if_fail (context != NULL);
    c_return_if_fail (entries != NULL);

    if (!context->mainGroup)
        context->mainGroup = c_option_group_new (NULL, NULL, NULL, NULL, NULL);

    c_option_group_add_entries (context->mainGroup, entries);
    c_option_group_set_translation_domain (context->mainGroup, translation_domain);
}

static cint calculate_max_length (COptionGroup *group, CHashTable* aliases)
{
    COptionEntry *entry;
    csize i, len, max_length;
    const cchar *long_name;

    max_length = 0;

    for (i = 0; i < group->nEntries; i++) {
        entry = &group->entries[i];

        if (entry->flags & C_OPTION_FLAG_HIDDEN)
            continue;

        long_name = c_hash_table_lookup (aliases, &entry->longName);
        if (!long_name)
            long_name = entry->longName;
        len = _c_utf8_strwidth (long_name);

        if (entry->shortName)
            len += 4;

        if (!NO_ARG (entry) && entry->argDescription)
            len += 1 + _c_utf8_strwidth (TRANSLATE (group, entry->argDescription));

        max_length = C_MAX (max_length, len);
    }

    return max_length;
}

static void print_entry (COptionGroup* group, cint max_length, const COptionEntry* entry, CString* string, CHashTable* aliases)
{
    CString *str;
    const cchar *long_name;

    if (entry->flags & C_OPTION_FLAG_HIDDEN)
        return;

    if (entry->longName[0] == 0)
        return;

    long_name = c_hash_table_lookup (aliases, &entry->longName);
    if (!long_name)
        long_name = entry->longName;

    str = c_string_new (NULL);

    if (entry->shortName)
        c_string_append_printf (str, "  -%c, --%s", entry->shortName, long_name);
    else
        c_string_append_printf (str, "  --%s", long_name);

    if (entry->argDescription)
        c_string_append_printf (str, "=%s", TRANSLATE (group, entry->argDescription));

    c_string_append_printf (string, "%s%*s %s\n", str->str, (int) (max_length + 4 - _c_utf8_strwidth (str->str)), "", entry->description ? TRANSLATE (group, entry->description) : "");
    c_string_free (str, true);
}

static bool group_has_visible_entries (COptionContext *context, COptionGroup *group, bool      main_entries)
{
    COptionFlags reject_filter = C_OPTION_FLAG_HIDDEN;
    COptionEntry *entry;
    cint i, l;
    bool main_group = group == context->mainGroup;

    if (!main_entries)
        reject_filter |= C_OPTION_FLAG_IN_MAIN;

    for (i = 0, l = (group ? group->nEntries : 0); i < l; i++) {
        entry = &group->entries[i];

        if (main_entries && !main_group && !(entry->flags & C_OPTION_FLAG_IN_MAIN))
            continue;
        if (entry->longName[0] == 0) /* ignore rest entry */
            continue;
        if (!(entry->flags & reject_filter))
            return true;
    }

    return false;
}

static bool group_list_has_visible_entries (COptionContext *context, CList          *group_list, bool       main_entries)
{
    while (group_list) {
        if (group_has_visible_entries (context, group_list->data, main_entries))
            return true;

        group_list = group_list->next;
    }

    return false;
}

static bool
context_has_h_entry (COptionContext *context)
{
    csize i;
    CList *list;

    if (context->mainGroup) {
        for (i = 0; i < context->mainGroup->nEntries; i++) {
            if (context->mainGroup->entries[i].shortName == 'h') {
                return true;
            }
        }
    }

    for (list = context->groups; list != NULL; list = c_list_next (list)) {
        COptionGroup *group;
        group = (COptionGroup*)list->data;
        for (i = 0; i < group->nEntries; i++) {
            if (group->entries[i].shortName == 'h') {
                return true;
            }
        }
    }
    return false;
}

cchar* c_option_context_get_help (COptionContext *context, bool main_help, COptionGroup* group)
{
    CList *list;
    cint max_length = 0, len;
    csize i;
    COptionEntry *entry;
    CHashTable *shadow_map;
    CHashTable *aliases;
    bool seen[256];
    const cchar *rest_description;
    CString *string;
    cuchar token;

    c_return_val_if_fail (context != NULL, NULL);

    string = c_string_sized_new (1024);

    rest_description = NULL;
    if (context->mainGroup) {
        for (i = 0; i < context->mainGroup->nEntries; i++) {
            entry = &context->mainGroup->entries[i];
            if (entry->longName[0] == 0) {
                rest_description = TRANSLATE (context->mainGroup, entry->argDescription);
                break;
            }
        }
    }

    c_string_append_printf (string, "%s\n  %s", _("Usage:"), c_get_prgname ());
    if (context->helpEnabled || (context->mainGroup && context->mainGroup->nEntries > 0) || context->groups != NULL) {
        c_string_append_printf (string, " %s", _("[OPTION…]"));
    }

    if (rest_description) {
        c_string_append (string, " ");
        c_string_append (string, rest_description);
    }

    if (context->parameterString) {
        c_string_append (string, " ");
        c_string_append (string, TRANSLATE (context, context->parameterString));
    }

    c_string_append (string, "\n\n");

    if (context->summary) {
        c_string_append (string, TRANSLATE (context, context->summary));
        c_string_append (string, "\n\n");
    }

    memset (seen, 0, sizeof (bool) * 256);
    shadow_map = c_hash_table_new (c_str_hash, c_str_equal);
    aliases = c_hash_table_new_full (NULL, NULL, NULL, c_free0);

    if (context->mainGroup) {
        for (i = 0; i < context->mainGroup->nEntries; i++) {
            entry = &context->mainGroup->entries[i];
            c_hash_table_insert (shadow_map, (void*)entry->longName, entry);

            if (seen[(cuchar)entry->shortName])
                entry->shortName = 0;
            else
                seen[(cuchar)entry->shortName] = true;
        }
    }

    list = context->groups;
    while (list != NULL) {
        COptionGroup *g = list->data;
        for (i = 0; i < g->nEntries; i++) {
            entry = &g->entries[i];
            if (c_hash_table_lookup (shadow_map, entry->longName) && !(entry->flags & C_OPTION_FLAG_NOALIAS)) {
                c_hash_table_insert (aliases, &entry->longName, c_strdup_printf ("%s-%s", g->name, entry->longName));
            }
            else {
                c_hash_table_insert (shadow_map, (void*)entry->longName, entry);
            }

            if (seen[(cuchar)entry->shortName] &&
                !(entry->flags & C_OPTION_FLAG_NOALIAS))
                entry->shortName = 0;
            else
                seen[(cuchar)entry->shortName] = true;
        }
        list = list->next;
    }

    c_hash_table_destroy (shadow_map);

    list = context->groups;

    if (context->helpEnabled) {
        max_length = _c_utf8_strwidth ("-?, --help");
        if (list) {
            len = _c_utf8_strwidth ("--help-all");
            max_length = C_MAX (max_length, len);
        }
    }

    if (context->mainGroup) {
        len = calculate_max_length (context->mainGroup, aliases);
        max_length = C_MAX (max_length, len);
    }

    while (list != NULL) {
        COptionGroup *g = list->data;
        if (context->helpEnabled) {
            /* First, we check the --help-<groupname> options */
            len = _c_utf8_strwidth ("--help-") + _c_utf8_strwidth (g->name);
            max_length = C_MAX (max_length, len);
        }

        /* Then we go through the entries */
        len = calculate_max_length (g, aliases);
        max_length = C_MAX (max_length, len);

        list = list->next;
    }

    /* Add a bit of padding */
    max_length += 4;

    if (!group && context->helpEnabled)
    {
        list = context->groups;

        token = context_has_h_entry (context) ? '?' : 'h';

        c_string_append_printf (string, "%s\n  -%c, --%-*s %s\n",
                                _("Help Options:"), token, max_length - 4, "help",
                                _("Show help options"));

        /* We only want --help-all when there are groups */
        if (list)
            c_string_append_printf (string, "  --%-*s %s\n",
                                    max_length, "help-all",
                                    _("Show all help options"));

        while (list)
        {
            COptionGroup *g = list->data;

            if (group_has_visible_entries (context, g, false))
                c_string_append_printf (string, "  --help-%-*s %s\n",
                                        max_length - 5, g->name,
                                        TRANSLATE (g, g->helpDescription));

            list = list->next;
        }

        c_string_append (string, "\n");
    }

    if (group)
    {
        /* Print a certain group */

        if (group_has_visible_entries (context, group, false))
        {
            c_string_append (string, TRANSLATE (group, group->description));
            c_string_append (string, "\n");
            for (i = 0; i < group->nEntries; i++)
                print_entry (group, max_length, &group->entries[i], string, aliases);
            c_string_append (string, "\n");
        }
    }
    else if (!main_help)
    {
        /* Print all groups */

        list = context->groups;

        while (list)
        {
            COptionGroup *g = list->data;

            if (group_has_visible_entries (context, g, false))
            {
                c_string_append (string, g->description);
                c_string_append (string, "\n");
                for (i = 0; i < g->nEntries; i++)
                    if (!(g->entries[i].flags & C_OPTION_FLAG_IN_MAIN))
                        print_entry (g, max_length, &g->entries[i], string, aliases);

                c_string_append (string, "\n");
            }

            list = list->next;
        }
    }

    /* Print application options if --help or --help-all has been specified */
    if ((main_help || !group) &&
        (group_has_visible_entries (context, context->mainGroup, true) ||
         group_list_has_visible_entries (context, context->groups, true)))
    {
        list = context->groups;

        if (context->helpEnabled || list)
            c_string_append (string,  _("Application Options:"));
        else
            c_string_append (string, _("Options:"));
        c_string_append (string, "\n");
        if (context->mainGroup)
            for (i = 0; i < context->mainGroup->nEntries; i++)
                print_entry (context->mainGroup, max_length,
                             &context->mainGroup->entries[i], string, aliases);

        while (list != NULL)
        {
            COptionGroup *g = list->data;

            /* Print main entries from other groups */
            for (i = 0; i < g->nEntries; i++) {
                if (g->entries[i].flags & C_OPTION_FLAG_IN_MAIN) {
                    print_entry (g, max_length, &g->entries[i], string, aliases);
                }
            }

            list = list->next;
        }

        c_string_append (string, "\n");
    }

    if (context->description) {
        c_string_append (string, TRANSLATE (context, context->description));
        c_string_append (string, "\n");
    }

    c_hash_table_destroy (aliases);

    return c_string_free (string, false);
}

static void print_help (COptionContext *context, bool main_help, COptionGroup* group)
{
    cchar *help;

    help = c_option_context_get_help (context, main_help, group);
    c_print ("%s", help);
    c_free (help);

    exit (0);
}

static bool parse_int (const cchar *arg_name, const cchar *arg, cint* result, CError** error)
{
    cchar *end;
    clong tmp;

    errno = 0;
    tmp = strtol (arg, &end, 0);

    if (*arg == '\0' || *end != '\0') {
        c_set_error (error,
                     C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE,
                     _("Cannot parse integer value “%s” for %s"),
                     arg, arg_name);
        return false;
    }

    *result = tmp;
    if (*result != tmp || errno == ERANGE) {
        c_set_error (error,
                     C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE,
                     _("Integer value “%s” for %s out of range"),
                     arg, arg_name);
        return false;
    }

    return true;
}


static bool
parse_double (const cchar *arg_name,
           const cchar *arg,
           cdouble        *result,
           CError     **error)
{
    cchar *end;
    cdouble tmp;

    errno = 0;
    tmp = c_strtod (arg, &end);

    if (*arg == '\0' || *end != '\0')
    {
        c_set_error (error,
                     C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE,
                     _("Cannot parse double value “%s” for %s"),
                     arg, arg_name);
        return false;
    }
    if (errno == ERANGE)
    {
        c_set_error (error,
                     C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE,
                     _("Double value “%s” for %s out of range"),
                     arg, arg_name);
        return false;
    }

    *result = tmp;

    return true;
}


static bool parse_int64 (const cchar *arg_name, const cchar *arg, cint64* result, CError** error)
{
    cchar *end;
    cint64 tmp;

    errno = 0;
    tmp = c_ascii_strtoll (arg, &end, 0);

    if (*arg == '\0' || *end != '\0') {
        c_set_error (error,
                     C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE,
                     _("Cannot parse integer value “%s” for %s"),
                     arg, arg_name);
        return false;
    }
    if (errno == ERANGE) {
        c_set_error (error,
                     C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE,
                     _("Integer value “%s” for %s out of range"),
                     arg, arg_name);
        return false;
    }

    *result = tmp;

    return true;
}


static Change* get_change (COptionContext *context, COptionArg arg_type, void* arg_data)
{
    CList *list;
    Change *change = NULL;

    for (list = context->changes; list != NULL; list = list->next) {
        change = list->data;
        if (change->argData == arg_data) {
            goto found;
        }
    }

    change = c_malloc0 (sizeof(Change));
    change->argType = arg_type;
    change->argData = arg_data;

    context->changes = c_list_prepend (context->changes, change);

found:

    return change;
}

static void
add_pending_null (COptionContext *context,
                  cchar         **ptr,
                  cchar          *value)
{
    PendingNull *n;

    n = c_malloc0 (sizeof(PendingNull));
    n->ptr = ptr;
    n->value = value;

    context->pendingNulls = c_list_prepend (context->pendingNulls, n);
}

static bool parse_arg (COptionContext *context, COptionGroup* group, COptionEntry* entry, const cchar* value, const cchar* option_name, CError** error)
{
    Change *change;

    c_assert (value || OPTIONAL_ARG (entry) || NO_ARG (entry));

    switch (entry->arg) {
        case C_OPTION_ARG_NONE: {
            (void) get_change (context, C_OPTION_ARG_NONE, entry->argData);

            *(bool *)entry->argData = !(entry->flags & C_OPTION_FLAG_REVERSE);
            break;
        }
        case C_OPTION_ARG_STRING: {
            cchar *data;
            data = c_locale_to_utf8 (value, -1, NULL, NULL, error);
            if (!data) {
                return false;
            }

            change = get_change (context, C_OPTION_ARG_STRING, entry->argData);

            if (!change->allocated.str)
                change->prev.str = *(cchar **)entry->argData;
            else
                c_free (change->allocated.str);

            change->allocated.str = data;

            *(cchar **)entry->argData = data;
            break;
        }
        case C_OPTION_ARG_STRING_ARRAY: {
            cchar *data;
            data = c_locale_to_utf8 (value, -1, NULL, NULL, error);

            if (!data)
                return false;

            change = get_change (context, C_OPTION_ARG_STRING_ARRAY, entry->argData);

            if (change->allocated.array.len == 0) {
                change->prev.array = *(cchar ***)entry->argData;
                change->allocated.array.data = c_malloc0(sizeof(cchar*) * 2);
            }
            else {
                change->allocated.array.data = c_realloc(change->allocated.array.data, sizeof(cchar*) * (change->allocated.array.len + 2));
            }

            change->allocated.array.data[change->allocated.array.len] = data;
            change->allocated.array.data[change->allocated.array.len + 1] = NULL;
            change->allocated.array.len ++;
            *(cchar ***)entry->argData = change->allocated.array.data;

            break;
        }

        case C_OPTION_ARG_FILENAME: {
            cchar *data;
            data = c_strdup (value);
            change = get_change (context, C_OPTION_ARG_FILENAME, entry->argData);
            if (!change->allocated.str) {
                change->prev.str = *(cchar **)entry->argData;
            }
            else {
                c_free (change->allocated.str);
            }
            change->allocated.str = data;
            *(cchar **)entry->argData = data;
            break;
        }

        case C_OPTION_ARG_FILENAME_ARRAY: {
            cchar *data;
            data = c_strdup (value);
            change = get_change (context, C_OPTION_ARG_STRING_ARRAY, entry->argData);

            if (change->allocated.array.len == 0) {
                change->prev.array = *(cchar ***)entry->argData;
                change->allocated.array.data = c_malloc0(sizeof(cchar*) * 2);
            }
            else {
                change->allocated.array.data = c_realloc(change->allocated.array.data, sizeof (cchar*) * (change->allocated.array.len + 2));
            }

            change->allocated.array.data[change->allocated.array.len] = data;
            change->allocated.array.data[change->allocated.array.len + 1] = NULL;
            change->allocated.array.len ++;
            *(cchar ***)entry->argData = change->allocated.array.data;
            break;
        }

        case C_OPTION_ARG_INT: {
            cint data;
            if (!parse_int (option_name, value, &data, error)) {
                return false;
            }

            change = get_change (context, C_OPTION_ARG_INT, entry->argData);
            change->prev.integer = *(cint *)entry->argData;
            *(cint *)entry->argData = data;
            break;
        }
        case C_OPTION_ARG_CALLBACK: {
            cchar *data;
            bool retval;

            if (!value && entry->flags & C_OPTION_FLAG_OPTIONAL_ARG) {
                data = NULL;
            }
            else if (entry->flags & C_OPTION_FLAG_NO_ARG) {
                data = NULL;
            }
            else if (entry->flags & C_OPTION_FLAG_FILENAME) {
                data = c_strdup (value);
            }
            else {
                data = c_locale_to_utf8 (value, -1, NULL, NULL, error);
            }

            if (!(entry->flags & (C_OPTION_FLAG_NO_ARG|C_OPTION_FLAG_OPTIONAL_ARG)) && !data) {
                return false;
            }

            retval = (* (COptionArgFunc) entry->argData) (option_name, data, group->userData, error);

            if (!retval && error != NULL && *error == NULL) {
                c_set_error (error, C_OPTION_ERROR, C_OPTION_ERROR_FAILED, _("Error parsing option %s"), option_name);
            }

            c_free (data);
            return retval;
            break;
        }
        case C_OPTION_ARG_DOUBLE: {
            cdouble data;

            if (!parse_double (option_name, value, &data, error)) {
                return false;
            }

            change = get_change (context, C_OPTION_ARG_DOUBLE, entry->argData);
            change->prev.dbl = *(cdouble *)entry->argData;
            *(cdouble *)entry->argData = data;
            break;
        }
        case C_OPTION_ARG_INT64: {
            cint64 data;
            if (!parse_int64 (option_name, value, &data, error)) {
                return false;
            }

            change = get_change (context, C_OPTION_ARG_INT64, entry->argData);
            change->prev.int64 = *(cint64*)entry->argData;
            *(cint64 *)entry->argData = data;
            break;
        }
        default: {
            c_assert_not_reached ();
        }
    }

    return true;
}

static bool parse_short_option (COptionContext *context, COptionGroup* group, cint idx, cint* new_idx, cchar arg, cint* argc, cchar*** argv, CError** error, bool* parsed)
{
    csize j;

    for (j = 0; j < group->nEntries; j++) {
        if (arg == group->entries[j].shortName) {
            cchar *option_name;
            cchar *value = NULL;

            option_name = c_strdup_printf ("-%c", group->entries[j].shortName);

            if (NO_ARG (&group->entries[j])) {
                value = NULL;
            }
            else {
                if (*new_idx > idx) {
                    c_set_error (error, C_OPTION_ERROR, C_OPTION_ERROR_FAILED, _("Error parsing option %s"), option_name);
                    c_free (option_name);
                    return false;
                }

                if (idx < *argc - 1) {
                    if (OPTIONAL_ARG (&group->entries[j]) && ((*argv)[idx + 1][0] == '-')) {
                        value = NULL;
                    }
                    else {
                        value = (*argv)[idx + 1];
                        add_pending_null (context, &((*argv)[idx + 1]), NULL);
                        *new_idx = idx + 1;
                    }
                }
                else if (idx >= *argc - 1 && OPTIONAL_ARG (&group->entries[j])) {
                    value = NULL;
                }
                else {
                    c_set_error (error, C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE, _("Missing argument for %s"), option_name);
                    c_free (option_name);
                    return false;
                }
            }

            if (!parse_arg (context, group, &group->entries[j], value, option_name, error)) {
                c_free (option_name);
                return false;
            }

            c_free (option_name);
            *parsed = true;
        }
    }

    return true;
}

static bool parse_long_option (COptionContext *context, COptionGroup* group, cint* idx, cchar* arg, bool aliased, cint* argc, cchar*** argv, CError** error, bool* parsed)
{
    csize j;

    for (j = 0; j < group->nEntries; j++) {
        if (*idx >= *argc) {
            return true;
        }

        if (aliased && (group->entries[j].flags & C_OPTION_FLAG_NOALIAS)) {
            continue;
        }

        if (NO_ARG (&group->entries[j]) && strcmp (arg, group->entries[j].longName) == 0) {
            cchar *option_name;
            bool retval;

            option_name = c_strconcat ("--", group->entries[j].longName, NULL);
            retval = parse_arg (context, group, &group->entries[j], NULL, option_name, error);
            c_free (option_name);

            add_pending_null (context, &((*argv)[*idx]), NULL);
            *parsed = true;

            return retval;
        }
        else {
            cint len = strlen (group->entries[j].longName);
            if (strncmp (arg, group->entries[j].longName, len) == 0 && (arg[len] == '=' || arg[len] == 0)) {
                cchar *value = NULL;
                cchar *option_name;

                add_pending_null (context, &((*argv)[*idx]), NULL);
                option_name = c_strconcat ("--", group->entries[j].longName, NULL);

                if (arg[len] == '=') {
                    value = arg + len + 1;
                }
                else if (*idx < *argc - 1) {
                    if (!OPTIONAL_ARG (&group->entries[j])) {
                        value = (*argv)[*idx + 1];
                        add_pending_null (context, &((*argv)[*idx + 1]), NULL);
                        (*idx)++;
                    }
                    else {
                        if ((*argv)[*idx + 1][0] == '-') {
                            bool retval;
                            retval = parse_arg (context, group, &group->entries[j], NULL, option_name, error);
                            *parsed = true;
                            c_free (option_name);
                            return retval;
                        }
                        else {
                            value = (*argv)[*idx + 1];
                            add_pending_null (context, &((*argv)[*idx + 1]), NULL);
                            (*idx)++;
                        }
                    }
                }
                else if (*idx >= *argc - 1 && OPTIONAL_ARG (&group->entries[j])) {
                    bool retval;
                    retval = parse_arg (context, group, &group->entries[j], NULL, option_name, error);
                    *parsed = true;
                    c_free (option_name);
                    return retval;
                }
                else {
                    c_set_error (error, C_OPTION_ERROR, C_OPTION_ERROR_BAD_VALUE, _("Missing argument for %s"), option_name);
                    c_free (option_name);
                    return false;
                }

                if (!parse_arg (context, group, &group->entries[j], value, option_name, error)) {
                    c_free (option_name);
                    return false;
                }

                c_free (option_name);
                *parsed = true;
            }
        }
    }

    return true;
}

static bool parse_remaining_arg (COptionContext *context, COptionGroup* group, cint* idx, cint* argc, cchar*** argv, CError** error, bool* parsed)
{
    csize j;

    for (j = 0; j < group->nEntries; j++) {
        if (*idx >= *argc) {
            return true;
        }

        if (group->entries[j].longName[0]) {
            continue;
        }

        c_return_val_if_fail (group->entries[j].arg == C_OPTION_ARG_CALLBACK || group->entries[j].arg == C_OPTION_ARG_STRING_ARRAY || group->entries[j].arg == C_OPTION_ARG_FILENAME_ARRAY, false);
        add_pending_null (context, &((*argv)[*idx]), NULL);

        if (!parse_arg (context, group, &group->entries[j], (*argv)[*idx], "", error)) {
            return false;
        }

        *parsed = true;
        return true;
    }

    return true;
}

static void free_changes_list (COptionContext *context, bool revert)
{
    CList *list;

    for (list = context->changes; list != NULL; list = list->next) {
        Change *change = list->data;

        if (revert) {
            switch (change->argType) {
                case C_OPTION_ARG_NONE:
                    *(bool *)change->argData = change->prev.boolean;
                    break;
                case C_OPTION_ARG_INT:
                    *(cint *)change->argData = change->prev.integer;
                    break;
                case C_OPTION_ARG_STRING:
                case C_OPTION_ARG_FILENAME:
                    c_free (change->allocated.str);
                    *(cchar **)change->argData = change->prev.str;
                    break;
                case C_OPTION_ARG_STRING_ARRAY:
                case C_OPTION_ARG_FILENAME_ARRAY:
                    c_strfreev (change->allocated.array.data);
                    *(cchar ***)change->argData = change->prev.array;
                    break;
                case C_OPTION_ARG_DOUBLE:
                    *(cdouble*)change->argData = change->prev.dbl;
                    break;
                case C_OPTION_ARG_INT64:
                    *(cint64 *)change->argData = change->prev.int64;
                    break;
                default:
                    c_assert_not_reached ();
            }
        }

        c_free (change);
    }

    c_list_free (context->changes);
    context->changes = NULL;
}

static void free_pending_nulls (COptionContext *context, bool perform_nulls)
{
    CList *list;

    for (list = context->pendingNulls; list != NULL; list = list->next) {
        PendingNull *n = list->data;

        if (perform_nulls) {
            if (n->value) {
                /* Copy back the short options */
                *(n->ptr)[0] = '-';
                strcpy (*n->ptr + 1, n->value);
            }
            else {
                if (context->strvMode)
                    c_free (*n->ptr);

                *n->ptr = NULL;
            }
        }

        c_free (n->value);
        c_free (n);
    }

    c_list_free (context->pendingNulls);
    context->pendingNulls = NULL;
}

static char* platform_get_argv0 (void)
{
#ifdef HAVE_PROC_SELF_CMDLINE
    char *cmdline;
  char *base_arg0;
  gsize len;

  if (!g_file_get_contents ("/proc/self/cmdline",
			    &cmdline,
			    &len,
			    NULL))
    return NULL;

  /* g_file_get_contents() guarantees to put a NUL immediately after the
   * file's contents (at cmdline[len] here), even if the file itself was
   * not NUL-terminated. */
  g_assert (memchr (cmdline, 0, len + 1));

  /* We could just return cmdline, but I think it's better
   * to hold on to a smaller malloc block; the arguments
   * could be large.
   */
  base_arg0 = g_path_get_basename (cmdline);
  c_free (cmdline);
  return base_arg0;
#elif defined __OpenBSD__
    char **cmdline;
  char *base_arg0;
  gsize len;

  int mib[] = { CTL_KERN, KERN_PROC_ARGS, getpid(), KERN_PROC_ARGV };

  if (sysctl (mib, G_N_ELEMENTS (mib), NULL, &len, NULL, 0) == -1)
      return NULL;

  cmdline = g_malloc0 (len);

  if (sysctl (mib, G_N_ELEMENTS (mib), cmdline, &len, NULL, 0) == -1)
    {
      c_free (cmdline);
      return NULL;
    }

  /* We could just return cmdline, but I think it's better
   * to hold on to a smaller malloc block; the arguments
   * could be large.
   */
  base_arg0 = g_path_get_basename (*cmdline);
  c_free (cmdline);
  return base_arg0;
#elif defined G_OS_WIN32
    const wchar_t *cmdline;
  wchar_t **wargv;
  int wargc;
  cchar *utf8_buf = NULL;
  char *base_arg0 = NULL;

  /* Pretend it's const, since we're not allowed to free it */
  cmdline = (const wchar_t *) GetCommandLineW ();
  if (G_UNLIKELY (cmdline == NULL))
    return NULL;

  /* Skip leading whitespace. CommandLineToArgvW() is documented
   * to behave weirdly with that. The character codes below
   * correspond to the *only* unicode characters that are
   * considered to be spaces by CommandLineToArgvW(). The rest
   * (such as 0xa0 - NO-BREAK SPACE) are treated as
   * normal characters.
   */
  while (cmdline[0] == 0x09 ||
         cmdline[0] == 0x0a ||
         cmdline[0] == 0x0c ||
         cmdline[0] == 0x0d ||
         cmdline[0] == 0x20)
    cmdline++;

  wargv = CommandLineToArgvW (cmdline, &wargc);
  if (G_UNLIKELY (wargv == NULL))
    return NULL;

  if (wargc > 0)
    utf8_buf = g_utf16_to_utf8 (wargv[0], -1, NULL, NULL, NULL);

  LocalFree (wargv);

  if (G_UNLIKELY (utf8_buf == NULL))
    return NULL;

  /* We could just return cmdline, but I think it's better
   * to hold on to a smaller malloc block; the arguments
   * could be large.
   */
  base_arg0 = g_path_get_basename (utf8_buf);
  c_free (utf8_buf);
  return base_arg0;
#endif

    return NULL;
}

bool c_option_context_parse (COptionContext* context, cint* argc, cchar*** argv, CError** error)
{
    cint i, j, k;
    CList *list;

    c_return_val_if_fail (context != NULL, false);

    /* Set program name */
    if (!c_get_prgname()) {
        cchar *prgname;

        if (argc && argv && *argc) {
            prgname = c_path_get_basename ((*argv)[0]);
        }
        else {
            prgname = platform_get_argv0 ();
        }

        if (prgname) {
            c_set_prgname (prgname);
        }
        else {
            c_set_prgname ("<unknown>");
        }

        c_free (prgname);
    }

    /* Call pre-parse hooks */
    list = context->groups;
    while (list) {
        COptionGroup *group = list->data;
        if (group->preParseFunc) {
            if (!(* group->preParseFunc) (context, group, group->userData, error)) {
                goto fail;
            }
        }

        list = list->next;
    }

    if (context->mainGroup && context->mainGroup->preParseFunc) {
        if (!(* context->mainGroup->preParseFunc) (context, context->mainGroup, context->mainGroup->userData, error)) {
            goto fail;
        }
    }

    if (argc && argv) {
        bool stop_parsing = false;
        bool has_unknown = false;
        cint separator_pos = 0;

        for (i = 1; i < *argc; i++) {
            cchar *arg, *dash;
            bool parsed = false;

            if ((*argv)[i][0] == '-' && (*argv)[i][1] != '\0' && !stop_parsing) {
                if ((*argv)[i][1] == '-') {
                    /* -- option */
                    arg = (*argv)[i] + 2;

                    /* '--' terminates list of arguments */
                    if (*arg == 0) {
                        separator_pos = i;
                        stop_parsing = true;
                        continue;
                    }

                    /* Handle help options */
                    if (context->helpEnabled) {
                        if (strcmp (arg, "help") == 0) {
                            print_help (context, true, NULL);
                        }
                        else if (strcmp (arg, "help-all") == 0) {
                            print_help (context, false, NULL);
                        }
                        else if (strncmp (arg, "help-", 5) == 0) {
                            list = context->groups;
                            while (list) {
                                COptionGroup *group = list->data;

                                if (strcmp (arg + 5, group->name) == 0) {
                                    print_help (context, false, group);
                                }

                                list = list->next;
                            }
                        }
                    }

                    if (context->mainGroup && !parse_long_option (context, context->mainGroup, &i, arg, false, argc, argv, error, &parsed)) {
                        goto fail;
                    }

                    if (parsed) {
                        continue;
                    }

                    /* Try the groups */
                    list = context->groups;
                    while (list) {
                        COptionGroup *group = list->data;
                        if (!parse_long_option (context, group, &i, arg, false, argc, argv, error, &parsed)) {
                            goto fail;
                        }

                        if (parsed) {
                            break;
                        }
                        list = list->next;
                    }

                    if (parsed) {
                        continue;
                    }

                    /* Now look for --<group>-<option> */
                    dash = strchr (arg, '-');
                    if (dash && arg < dash) {
                        /* Try the groups */
                        list = context->groups;
                        while (list) {
                            COptionGroup *group = list->data;
                            if (strncmp (group->name, arg, dash - arg) == 0) {
                                if (!parse_long_option (context, group, &i, dash + 1, true, argc, argv, error, &parsed)) {
                                    goto fail;
                                }

                                if (parsed) {
                                    break;
                                }
                            }
                            list = list->next;
                        }
                    }

                    if (context->ignoreUnknown) {
                        continue;
                    }
                }
                else {
                    /* short option */
                    cint new_i = i, arg_length;
                    bool *nulled_out = NULL;
                    bool has_h_entry = context_has_h_entry (context);
                    arg = (*argv)[i] + 1;
                    arg_length = strlen (arg);
                    nulled_out = c_malloc0(sizeof (bool) * arg_length);
                    for (j = 0; j < arg_length; j++) {
                        if (context->helpEnabled && (arg[j] == '?' || (arg[j] == 'h' && !has_h_entry))) {
                            print_help (context, true, NULL);
                        }
                        parsed = false;
                        if (context->mainGroup && !parse_short_option (context, context->mainGroup, i, &new_i, arg[j], argc, argv, error, &parsed)) {
                            goto fail;
                        }
                        if (!parsed) {
                            /* Try the groups */
                            list = context->groups;
                            while (list) {
                                COptionGroup *group = list->data;
                                if (!parse_short_option (context, group, i, &new_i, arg[j], argc, argv, error, &parsed)) {
                                    goto fail;
                                }
                                if (parsed) {
                                    break;
                                }
                                list = list->next;
                            }
                        }

                        if (context->ignoreUnknown && parsed)
                            nulled_out[j] = true;
                        else if (context->ignoreUnknown)
                            continue;
                        else if (!parsed)
                            break;
                        /* !context->ignoreUnknown && parsed */
                    }
                    if (context->ignoreUnknown) {
                        cchar *new_arg = NULL;
                        cint arg_index = 0;
                        for (j = 0; j < arg_length; j++) {
                            if (!nulled_out[j]) {
                                if (!new_arg) {
                                    new_arg = c_malloc0 (arg_length + 1);
                                }
                                new_arg[arg_index++] = arg[j];
                            }
                        }
                        if (new_arg) {
                            new_arg[arg_index] = '\0';
                        }
                        add_pending_null (context, &((*argv)[i]), new_arg);
                        i = new_i;
                    }
                    else if (parsed) {
                        add_pending_null (context, &((*argv)[i]), NULL);
                        i = new_i;
                    }
                }

                if (!parsed) {
                    has_unknown = true;
                }

                if (!parsed && !context->ignoreUnknown) {
                    c_set_error (error, C_OPTION_ERROR, C_OPTION_ERROR_UNKNOWN_OPTION, _("Unknown option %s"), (*argv)[i]);
                    goto fail;
                }
            }
            else {
                if (context->strictPosix) {
                    stop_parsing = true;
                }

                /* Collect remaining args */
                if (context->mainGroup && !parse_remaining_arg (context, context->mainGroup, &i, argc, argv, error, &parsed)) {
                    goto fail;
                }

                if (!parsed && (has_unknown || (*argv)[i][0] == '-')) {
                    separator_pos = 0;
                }
            }
        }

        if (separator_pos > 0) {
            add_pending_null (context, &((*argv)[separator_pos]), NULL);
        }
    }

    /* Call post-parse hooks */
    list = context->groups;
    while (list) {
        COptionGroup *group = list->data;
        if (group->postParseFunc) {
            if (!(* group->postParseFunc) (context, group, group->userData, error)) {
                goto fail;
            }
        }

        list = list->next;
    }

    if (context->mainGroup && context->mainGroup->postParseFunc) {
        if (!(* context->mainGroup->postParseFunc) (context, context->mainGroup, context->mainGroup->userData, error)) {
            goto fail;
        }
    }

    if (argc && argv) {
        free_pending_nulls (context, true);
        for (i = 1; i < *argc; i++) {
            for (k = i; k < *argc; k++) {
                if ((*argv)[k] != NULL) {
                    break;
                }
            }

            if (k > i) {
                k -= i;
                for (j = i + k; j < *argc; j++) {
                    (*argv)[j-k] = (*argv)[j];
                    (*argv)[j] = NULL;
                }
                *argc -= k;
            }
        }
    }

    return true;

fail:

    /* Call error hooks */
    list = context->groups;
    while (list) {
        COptionGroup *group = list->data;
        if (group->errorFunc) {
            (* group->errorFunc) (context, group, group->userData, error);
        }

        list = list->next;
    }

    if (context->mainGroup && context->mainGroup->errorFunc) {
        (* context->mainGroup->errorFunc) (context, context->mainGroup, context->mainGroup->userData, error);
    }

    free_changes_list (context, true);
    free_pending_nulls (context, false);

    return false;
}

COptionGroup* c_option_group_new (const cchar* name, const cchar* description, const cchar* help_description, void* user_data, CDestroyNotify destroy)
{
    COptionGroup *group;

    group = c_malloc0 (sizeof(COptionGroup));
    group->refCount = 1;
    group->name = c_strdup (name);
    group->description = c_strdup (description);
    group->helpDescription = c_strdup (help_description);
    group->userData = user_data;
    group->destroyNotify = destroy;

    return group;
}


void c_option_group_free (COptionGroup *group)
{
    c_option_group_unref (group);
}

COptionGroup* c_option_group_ref (COptionGroup *group)
{
    c_return_val_if_fail (group != NULL, NULL);

    group->refCount++;

    return group;
}

void c_option_group_unref (COptionGroup *group)
{
    c_return_if_fail (group != NULL);

    if (--group->refCount == 0) {
        c_free (group->name);
        c_free (group->description);
        c_free (group->helpDescription);

        c_free (group->entries);

        if (group->destroyNotify)
            (* group->destroyNotify) (group->userData);

        if (group->translateNotify)
            (* group->translateNotify) (group->translateData);

        c_free (group);
    }
}

void c_option_group_add_entries (COptionGroup* group, const COptionEntry *entries)
{
    csize i, n_entries;

    c_return_if_fail (group != NULL);
    c_return_if_fail (entries != NULL);

    for (n_entries = 0; entries[n_entries].longName != NULL; n_entries++) ;

    c_return_if_fail (n_entries <= C_MAX_SIZE - group->nEntries);

    group->entries = c_realloc(group->entries, sizeof (COptionEntry) * (group->nEntries + n_entries));

    if (n_entries != 0) {
        memcpy (group->entries + group->nEntries, entries, sizeof (COptionEntry) * n_entries);
    }

    for (i = group->nEntries; i < group->nEntries + n_entries; i++) {
        cchar c = group->entries[i].shortName;
        if (c == '-' || (c != 0 && !c_ascii_isprint (c))) {
            C_LOG_WARNING (C_STRLOC ": ignoring invalid short option '%c' (%d) in entry %s:%s", c, c, group->name, group->entries[i].longName);
            group->entries[i].shortName = '\0';
        }

        if (group->entries[i].arg != C_OPTION_ARG_NONE && (group->entries[i].flags & C_OPTION_FLAG_REVERSE) != 0) {
            C_LOG_WARNING (C_STRLOC ": ignoring reverse flag on option of arg-type %d in entry %s:%s", group->entries[i].arg, group->name, group->entries[i].longName);
            group->entries[i].flags &= ~C_OPTION_FLAG_REVERSE;
        }

        if (group->entries[i].arg != C_OPTION_ARG_CALLBACK && (group->entries[i].flags & (C_OPTION_FLAG_NO_ARG|C_OPTION_FLAG_OPTIONAL_ARG|C_OPTION_FLAG_FILENAME)) != 0) {
            C_LOG_WARNING (C_STRLOC ": ignoring no-arg, optional-arg or filename flags (%d) on option of arg-type %d in entry %s:%s", group->entries[i].flags, group->entries[i].arg, group->name, group->entries[i].longName);
            group->entries[i].flags &= ~(C_OPTION_FLAG_NO_ARG|C_OPTION_FLAG_OPTIONAL_ARG|C_OPTION_FLAG_FILENAME);
        }
    }

    group->nEntries += n_entries;
}

void c_option_group_set_parse_hooks (COptionGroup* group, COptionParseFunc pre_parse_func, COptionParseFunc post_parse_func)
{
    c_return_if_fail (group != NULL);

    group->preParseFunc = pre_parse_func;
    group->postParseFunc = post_parse_func;
}

void c_option_group_set_error_hook (COptionGroup* group, COptionErrorFunc error_func)
{
    c_return_if_fail (group != NULL);

    group->errorFunc = error_func;
}


void c_option_group_set_translate_func (COptionGroup* group, CTranslateFunc func, void* data, CDestroyNotify destroy_notify)
{
    c_return_if_fail (group != NULL);

    if (group->translateNotify) {
        group->translateNotify (group->translateData);
    }

    group->translateFunc = func;
    group->translateData = data;
    group->translateNotify = destroy_notify;
}

static const cchar* dgettext_swapped (const cchar *msgid, const cchar *domainname)
{
    return c_dgettext (domainname, msgid);
}

void c_option_group_set_translation_domain (COptionGroup *group, const cchar  *domain)
{
    c_return_if_fail (group != NULL);

    c_option_group_set_translate_func (group, (CTranslateFunc)dgettext_swapped, c_strdup (domain), c_free0);
}

void c_option_context_set_translate_func (COptionContext *context, CTranslateFunc func, void* data, CDestroyNotify destroy_notify)
{
    c_return_if_fail (context != NULL);

    if (context->translateNotify) {
        context->translateNotify (context->translateData);
    }

    context->translateFunc = func;
    context->translateData = data;
    context->translateNotify = destroy_notify;
}

void c_option_context_set_translation_domain (COptionContext *context, const cchar* domain)
{
    c_return_if_fail (context != NULL);
    c_option_context_set_translate_func (context, (CTranslateFunc)dgettext_swapped, c_strdup (domain), c_free0);
}


void c_option_context_set_summary (COptionContext *context, const cchar* summary)
{
  c_return_if_fail (context != NULL);

  c_free (context->summary);
  context->summary = c_strdup (summary);
}


const cchar* c_option_context_get_summary (COptionContext *context)
{
  c_return_val_if_fail (context != NULL, NULL);

  return context->summary;
}

void c_option_context_set_description (COptionContext *context, const cchar* description)
{
    c_return_if_fail (context != NULL);

    c_free (context->description);
    context->description = c_strdup (description);
}


const cchar* c_option_context_get_description (COptionContext *context)
{
    c_return_val_if_fail (context != NULL, NULL);

    return context->description;
}


bool c_option_context_parse_strv (COptionContext* context, cchar*** arguments, CError** error)
{
    bool success;
    cint argc;

    c_return_val_if_fail (context != NULL, false);

    context->strvMode = true;
    argc = arguments && *arguments ? c_strv_length (*arguments) : 0;
    success = c_option_context_parse (context, &argc, arguments, error);
    context->strvMode = false;

    return success;
}
