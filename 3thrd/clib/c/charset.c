// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 6/9/24.
//

#include "charset.h"

#include "clib.h"

#if (HAVE_LANGINFO_TIME_CODESET || HAVE_LANGINFO_CODESET)
#include <langinfo.h>
#endif

#include <locale.h>


#define relocate(pathname) (pathname)

/* Get GLIB_CHARSETALIAS_DIR.  */
#ifndef CLIB_CHARSETALIAS_DIR
#define CLIB_CHARSETALIAS_DIR "/lib/charset"        // FIXME:// NOTE://
#endif

#ifndef DIRECTORY_SEPARATOR
# define DIRECTORY_SEPARATOR C_DIR_SEPARATOR
#endif

#ifndef ISSLASH
# define ISSLASH(C) ((C) == DIRECTORY_SEPARATOR)
#endif


C_LOCK_DEFINE_STATIC(aliases);

typedef struct _CCharsetCache CCharsetCache;
typedef struct _CLanguageNamesCache CLanguageNamesCache;

enum
{
    COMPONENT_CODESET = 1 << 0,
    COMPONENT_TERRITORY = 1 << 1,
    COMPONENT_MODIFIER = 1 << 2
};


struct _CLanguageNamesCache
{
    cchar * languages;
    cchar ** language_names;
};


struct _CCharsetCache
{
    bool is_utf8;
    cchar * raw;
    cchar * charset;
};

extern const char* _c_locale_charset_raw(void);
extern const char* _c_locale_get_charset_aliases(void);
extern const char* _c_locale_charset_unalias(const char * codeset);

extern void* c_private_set_alloc0 (CPrivate* key, csize size);


static const char * volatile gsCharsetAliases;


static CHashTable* get_alias_hash(void)
{
    static CHashTable * alias_hash = NULL;
    const char * aliases;

    C_LOCK(aliases);

    if (!alias_hash) {
        alias_hash = c_hash_table_new(c_str_hash, c_str_equal);

        aliases = _c_locale_get_charset_aliases();
        while (*aliases != '\0') {
            const char * canonical;
            const char * alias;
            const char ** alias_array;
            int count = 0;

            alias = aliases;
            aliases += strlen(aliases) + 1;
            canonical = aliases;
            aliases += strlen(aliases) + 1;

            alias_array = c_hash_table_lookup(alias_hash, canonical);
            if (alias_array) {
                while (alias_array[count])
                    count++;
            }

            alias_array = c_realloc(alias_array, sizeof(char*) * (count + 2));
            alias_array[count] = alias;
            alias_array[count + 1] = NULL;

            c_hash_table_insert(alias_hash, (char*)canonical, alias_array);
        }
    }

    C_UNLOCK(aliases);

    return alias_hash;
}

/* As an abuse of the alias table, the following routines gets
 * the charsets that are aliases for the canonical name.
 */
const char** _c_charset_get_aliases(const char * canonical_name)
{
    CHashTable * alias_hash = get_alias_hash();

    return c_hash_table_lookup(alias_hash, canonical_name);
}

static bool c_utf8_get_charset_internal(const char * raw_data, const char ** a)
{
    /* Allow CHARSET to override the charset of any locale category. Users should
     * probably never be setting this — instead, just add the charset after a `.`
     * in `LANGUAGE`/`LC_ALL`/`LC_*`/`LANG`. I can’t find any reference (in
     * `git log`, code comments, or man pages) to this environment variable being
     * standardised or documented or even used anywhere outside GLib. Perhaps it
     * should eventually be removed. */
    const char * charset = c_getenv("CHARSET");

    if (charset && *charset) {
        *a = charset;

        if (charset && strstr(charset, "UTF-8"))
            return true;
        else
            return false;
    }

    /* The libcharset code tries to be thread-safe without
     * a lock, but has a memory leak and a missing memory
     * barrier, so we lock for it
     */
    C_LOCK(aliases);
    charset = _c_locale_charset_unalias(raw_data);
    C_UNLOCK(aliases);

    if (charset && *charset) {
        *a = charset;

        if (charset && strstr(charset, "UTF-8"))
            return true;
        else
            return false;
    }

    /* Assume this for compatibility at present.  */
    *a = "US-ASCII";

    return false;
}


static void charset_cache_free(void * data)
{
    CCharsetCache * cache = data;
    c_free(cache->raw);
    c_free(cache->charset);
    c_free(cache);
}

bool c_get_charset(const char ** charset)
{
    static CPrivate cache_private = C_PRIVATE_INIT(charset_cache_free);
    CCharsetCache * cache = c_private_get(&cache_private);
    const cchar * raw;

    if (!cache)
        cache = c_private_set_alloc0(&cache_private, sizeof(CCharsetCache));

    C_LOCK(aliases);
    raw = _c_locale_charset_raw();
    C_UNLOCK(aliases);

    if (cache->raw == NULL || strcmp(cache->raw, raw) != 0) {
        const cchar * new_charset;

        c_free(cache->raw);
        c_free(cache->charset);
        cache->raw = c_strdup(raw);
        cache->is_utf8 = c_utf8_get_charset_internal(raw, &new_charset);
        cache->charset = c_strdup(new_charset);
    }

    if (charset)
        *charset = cache->charset;

    return cache->is_utf8;
}

bool _c_get_time_charset(const char ** charset)
{
    static CPrivate cache_private = C_PRIVATE_INIT(charset_cache_free);
    CCharsetCache * cache = c_private_get(&cache_private);
    const cchar * raw;

    if (!cache)
        cache = c_private_set_alloc0(&cache_private, sizeof(CCharsetCache));

#ifdef HAVE_LANGINFO_TIME_CODESET
  raw = nl_langinfo (_NL_TIME_CODESET);
#else
    C_LOCK(aliases);
    raw = _c_locale_charset_raw();
    C_UNLOCK(aliases);
#endif

    if (cache->raw == NULL || strcmp(cache->raw, raw) != 0) {
        const cchar * new_charset;

        c_free(cache->raw);
        c_free(cache->charset);
        cache->raw = c_strdup(raw);
        cache->is_utf8 = c_utf8_get_charset_internal(raw, &new_charset);
        cache->charset = c_strdup(new_charset);
    }

    if (charset)
        *charset = cache->charset;

    return cache->is_utf8;
}

bool _c_get_ctype_charset(const char ** charset)
{
    static CPrivate cache_private = C_PRIVATE_INIT(charset_cache_free);
    CCharsetCache * cache = c_private_get(&cache_private);
    const cchar * raw;

    if (!cache)
        cache = c_private_set_alloc0(&cache_private, sizeof(CCharsetCache));

#ifdef HAVE_LANGINFO_CODESET
  raw = nl_langinfo (CODESET);
#else
    C_LOCK(aliases);
    raw = _c_locale_charset_raw();
    C_UNLOCK(aliases);
#endif

    if (cache->raw == NULL || strcmp(cache->raw, raw) != 0) {
        const cchar * new_charset;

        c_free(cache->raw);
        c_free(cache->charset);
        cache->raw = c_strdup(raw);
        cache->is_utf8 = c_utf8_get_charset_internal(raw, &new_charset);
        cache->charset = c_strdup(new_charset);
    }

    if (charset)
        *charset = cache->charset;

    return cache->is_utf8;
}

cchar* c_get_codeset(void)
{
    const cchar * charset;

    c_get_charset(&charset);

    return c_strdup(charset);
}

bool c_get_console_charset(const char ** charset)
{
    /* assume the locale settings match the console encoding on non-Windows OSs */
    return c_get_charset(charset);
}

#ifndef C_OS_WIN32

/* read an alias file for the locales */
static void read_aliases(const cchar * file, CHashTable * alias_table)
{
    FILE * fp;
    char buf[256];

    fp = fopen(file, "re");
    if (!fp)
        return;
    while (fgets(buf, 256, fp)) {
        char * p, * q;

        c_strstrip(buf);

        /* Line is a comment */
        if ((buf[0] == '#') || (buf[0] == '\0'))
            continue;

        /* Reads first column */
        for (p = buf, q = NULL; *p; p++) {
            if ((*p == '\t') || (*p == ' ') || (*p == ':')) {
                *p = '\0';
                q = p + 1;
                while ((*q == '\t') || (*q == ' ')) {
                    q++;
                }
                break;
            }
        }
        /* The line only had one column */
        if (!q || *q == '\0')
            continue;

        /* Read second column */
        for (p = q; *p; p++) {
            if ((*p == '\t') || (*p == ' ')) {
                *p = '\0';
                break;
            }
        }

        /* Add to alias table if necessary */
        if (!c_hash_table_lookup(alias_table, buf)) {
            c_hash_table_insert(alias_table, c_strdup(buf), c_strdup(q));
        }
    }
    fclose(fp);
}

#endif

static char* unalias_lang(char * lang)
{
#ifndef C_OS_WIN32
    static CHashTable * alias_table = NULL;
    char * p;
    int i;

    if (c_once_init_enter_pointer(&alias_table)) {
        CHashTable * table = c_hash_table_new(c_str_hash, c_str_equal);
        read_aliases("/usr/share/locale/locale.alias", table);
        c_once_init_leave_pointer(&alias_table, table);
    }

    i = 0;
    while ((p = c_hash_table_lookup(alias_table, lang)) && (strcmp(p, lang) != 0)) {
        lang = p;
        if (i++ == 30) {
            static bool said_before = false;
            if (!said_before)
                C_LOG_WARNING_CONSOLE("Too many alias levels for a locale, may indicate a loop");
            said_before = true;
            return lang;
        }
    }
#endif
    return lang;
}

static cuint explode_locale(const cchar * locale, cchar ** language, cchar ** territory, cchar ** codeset, cchar ** modifier)
{
    const cchar * uscore_pos;
    const cchar * at_pos;
    const cchar * dot_pos;

    cuint mask = 0;

    uscore_pos = strchr(locale, '_');
    dot_pos = strchr(uscore_pos ? uscore_pos : locale, '.');
    at_pos = strchr(dot_pos ? dot_pos : (uscore_pos ? uscore_pos : locale), '@');

    if (at_pos) {
        mask |= COMPONENT_MODIFIER;
        *modifier = c_strdup(at_pos);
    }
    else
        at_pos = locale + strlen(locale);

    if (dot_pos) {
        mask |= COMPONENT_CODESET;
        *codeset = c_strndup(dot_pos, at_pos - dot_pos);
    }
    else
        dot_pos = at_pos;

    if (uscore_pos) {
        mask |= COMPONENT_TERRITORY;
        *territory = c_strndup(uscore_pos, dot_pos - uscore_pos);
    }
    else
        uscore_pos = dot_pos;

    *language = c_strndup(locale, uscore_pos - locale);

    return mask;
}

static void append_locale_variants(CPtrArray * array, const cchar * locale)
{
    cchar * language = NULL;
    cchar * territory = NULL;
    cchar * codeset = NULL;
    cchar * modifier = NULL;

    cuint mask;
    cuint i, j;

    c_return_if_fail(locale != NULL);

    mask = explode_locale(locale, &language, &territory, &codeset, &modifier);

    /* Iterate through all possible combinations, from least attractive
     * to most attractive.
     */
    for (j = 0; j <= mask; ++j) {
        i = mask - j;

        if ((i & ~mask) == 0) {
            cchar * val = c_strconcat(language, (i & COMPONENT_TERRITORY) ? territory : "", (i & COMPONENT_CODESET) ? codeset : "", (i & COMPONENT_MODIFIER) ? modifier : "", NULL);
            c_ptr_array_add(array, val);
        }
    }

    c_free(language);
    if (mask & COMPONENT_CODESET)
        c_free(codeset);
    if (mask & COMPONENT_TERRITORY)
        c_free(territory);
    if (mask & COMPONENT_MODIFIER)
        c_free(modifier);
}

cchar** c_get_locale_variants(const cchar * locale)
{
    CPtrArray * array;

    c_return_val_if_fail(locale != NULL, NULL);

    array = c_ptr_array_sized_new(8);
    append_locale_variants(array, locale);
    c_ptr_array_add(array, NULL);

    return (cchar**)c_ptr_array_free(array, false);
}

static const cchar* guess_category_value(const cchar * category_name)
{
    const cchar * retval;

    retval = c_getenv("LANGUAGE");
    if ((retval != NULL) && (retval[0] != '\0'))
        return retval;

    retval = c_getenv("LC_ALL");
    if ((retval != NULL) && (retval[0] != '\0'))
        return retval;

    /* Next comes the name of the desired category.  */
    retval = c_getenv(category_name);
    if ((retval != NULL) && (retval[0] != '\0'))
        return retval;

    /* Last possibility is the LANG environment variable.  */
    retval = c_getenv("LANG");
    if ((retval != NULL) && (retval[0] != '\0'))
        return retval;

#ifdef C_PLATFORM_WIN32
  /* c_win32_getlocale() first checks for LC_ALL, LC_MESSAGES and
   * LANG, which we already did above. Oh well. The main point of
   * calling c_win32_getlocale() is to get the thread's locale as used
   * by Windows and the Microsoft C runtime (in the "English_United
   * States" format) translated into the Unixish format.
   */
  {
    char *locale = c_win32_getlocale ();
    retval = c_intern_string (locale);
    c_free (locale);
    return retval;
  }
#endif

    return NULL;
}

static void language_names_cache_free(void * data)
{
    CLanguageNamesCache * cache = data;
    c_free(cache->languages);
    c_strfreev(cache->language_names);
    c_free(cache);
}

const cchar* const * c_get_language_names(void)
{
    return c_get_language_names_with_category("LC_MESSAGES");
}

const cchar* const* c_get_language_names_with_category(const cchar * category_name)
{
    static CPrivate cache_private = C_PRIVATE_INIT((void (*)(void*))c_hash_table_unref);
    CHashTable * cache = c_private_get(&cache_private);
    const cchar * languages;
    CLanguageNamesCache * name_cache;

    c_return_val_if_fail(category_name != NULL, NULL);

    if (!cache) {
        cache = c_hash_table_new_full(c_str_hash, c_str_equal, c_free0, language_names_cache_free);
        c_private_set(&cache_private, cache);
    }

    languages = guess_category_value(category_name);
    if (!languages)
        languages = "C";

    name_cache = (CLanguageNamesCache*)c_hash_table_lookup(cache, category_name);
    if (!(name_cache && name_cache->languages &&
        strcmp(name_cache->languages, languages) == 0)) {
        CPtrArray * array;
        cchar ** alist, ** a;

        c_hash_table_remove(cache, category_name);

        array = c_ptr_array_sized_new(8);

        alist = c_strsplit(languages, ":", 0);
        for (a = alist; *a; a++)
            append_locale_variants(array, unalias_lang(*a));
        c_strfreev(alist);
        c_ptr_array_add(array, c_strdup("C"));
        c_ptr_array_add(array, NULL);

        name_cache = c_malloc0(sizeof(CLanguageNamesCache));
        name_cache->languages = c_strdup(languages);
        name_cache->language_names = (cchar**)c_ptr_array_free(array, false);
        c_hash_table_insert(cache, c_strdup(category_name), name_cache);
    }

    return (const cchar* const *)name_cache->language_names;
}

const char* _c_locale_get_charset_aliases(void)
{
    const char * cp;

    cp = gsCharsetAliases;
    if (cp == NULL) {
        gsCharsetAliases = cp;
    }

    return cp;
}

const char* _c_locale_charset_raw(void)
{
    const char * codeset;
    const char * locale = NULL;

    if (locale == NULL || locale[0] == '\0') {
        locale = getenv("LC_ALL");
        if (locale == NULL || locale[0] == '\0') {
            locale = getenv("LC_CTYPE");
            if (locale == NULL || locale[0] == '\0') {
                locale = getenv("LANG");
            }
        }
    }

    codeset = locale;

    return codeset;
}

const char* _c_locale_charset_unalias(const char * codeset)
{
    const char * aliases;

    if (codeset == NULL)
        /* The canonical name cannot be determined.  */
        codeset = "";

    /* Resolve alias. */
    for (aliases = _c_locale_get_charset_aliases(); *aliases != '\0'; aliases += strlen(aliases) + 1, aliases += strlen(aliases) + 1) {
        if (strcmp(codeset, aliases) == 0 || (aliases[0] == '*' && aliases[1] == '\0')) {
            codeset = aliases + strlen(aliases) + 1;
            break;
        }
    }

    if (codeset[0] == '\0') {
        codeset = "ASCII";
    }

    return codeset;
}
