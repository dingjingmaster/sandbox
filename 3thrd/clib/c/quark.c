
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-19.
//

#include "quark.h"

#include "c/hash-table.h"
#include "c/thread.h"
#include "str.h"
#include "atomic.h"

#define QUARK_BLOCK_SIZE            2048
#define QUARK_STRING_BLOCK_SIZE     (4096 - sizeof (csize))

static CQuark quark_new (char* str);
static char* quark_strdup (const char* str);
static CQuark quark_from_string (const char* str, bool duplicate);
static CQuark quark_from_string_locked (const char* str, bool duplicate);
static const char* quark_intern_string_locked (const char* str, bool duplicate);

C_LOCK_DEFINE_STATIC (gsQuarkGlobal);

static CHashTable*      gsQuarkHt = NULL;
static char**           gsQuarks = NULL;
static int              gsQuarkSeqID = 0;
static char*            gsQuarkBlock = NULL;
static csize            gsQuarkBlockOffset = 0;

void c_quark_init (void)
{
    c_assert (gsQuarkSeqID == 0);
    gsQuarkHt = c_hash_table_new (c_str_hash, c_str_equal);
    gsQuarks = c_malloc0(sizeof (char*) * QUARK_BLOCK_SIZE);
    gsQuarks[0] = NULL;
    gsQuarkSeqID = 1;
}

CQuark c_quark_try_string (const char* str)
{
    if (str == NULL) {
        return 0;
    }

    C_LOCK (gsQuarkGlobal);
    CQuark quark = C_POINTER_TO_UINT(c_hash_table_lookup (gsQuarkHt, str));
    C_UNLOCK (gsQuarkGlobal);

    return quark;
}

CQuark c_quark_from_static_string (const char* str)
{
    return quark_from_string_locked (str, false);
}

CQuark c_quark_from_string (const char* str)
{
    return quark_from_string_locked (str, true);
}

const char* c_quark_to_string (CQuark quark)
{
    char* res= NULL;
    cuint seqID = (cuint) c_atomic_int_get (&gsQuarkSeqID);
    char** strT = c_atomic_pointer_get (&gsQuarks);

    if (quark < seqID) {
        res = strT[quark];
    }

    return res;
}

const char* c_intern_string (const char* str)
{
    return quark_intern_string_locked (str, true);
}

const char* c_intern_static_string (const char* str)
{
    return quark_intern_string_locked (str, false);
}

static char* quark_strdup (const char* str)
{
    csize len = strlen (str) + 1;

    if (len > QUARK_STRING_BLOCK_SIZE / 2) {
        return c_strdup (str);
    }

    if (gsQuarkBlock == NULL || QUARK_STRING_BLOCK_SIZE - gsQuarkBlockOffset < len) {
        gsQuarkBlock = c_malloc0(QUARK_STRING_BLOCK_SIZE);
        gsQuarkBlockOffset = 0;
    }

    char* copy = gsQuarkBlock + gsQuarkBlockOffset;
    memcpy (copy, str, len);
    gsQuarkBlockOffset += len;

    return copy;
}

static CQuark quark_from_string (const char* str, bool duplicate)
{
    CQuark quark = C_POINTER_TO_UINT (c_hash_table_lookup (gsQuarkHt, str));
    if (!quark) {
        quark = quark_new (duplicate ? quark_strdup (str) : (char*) str);
    }

    return quark;
}

static CQuark quark_from_string_locked (const char* str, bool duplicate)
{
    if (!str) {
        return 0;
    }

    C_LOCK (gsQuarkGlobal);
    CQuark quark = quark_from_string (str, duplicate);
    C_UNLOCK (gsQuarkGlobal);

    return quark;
}
static CQuark quark_new (char* str)
{
    CQuark quark;
    char** quarksNew = NULL;

    if (gsQuarkSeqID % QUARK_BLOCK_SIZE == 0) {
        quarksNew = c_malloc0(sizeof(char*) * (gsQuarkSeqID + QUARK_BLOCK_SIZE));
        if (gsQuarkSeqID != 0) {
            memcpy (quarksNew, gsQuarks, sizeof (char*) * gsQuarkSeqID);
        }
        memset (quarksNew + gsQuarkSeqID, 0, sizeof (char*) * QUARK_BLOCK_SIZE);
        c_atomic_pointer_set (&gsQuarks, quarksNew);
    }

    quark = gsQuarkSeqID;
    c_atomic_pointer_set (&gsQuarks[quark], str);
    c_hash_table_insert (gsQuarkHt, str, C_UINT_TO_POINTER (quark));
    c_atomic_int_inc (&gsQuarkSeqID);

    return quark;
}

static const char* quark_intern_string_locked (const char* str, bool duplicate)
{
    const char *result;
    CQuark quark;

    if (!str) {
        return NULL;
    }

    C_LOCK (gsQuarkGlobal);
    quark = quark_from_string (str, duplicate);
    result = gsQuarks[quark];
    C_UNLOCK (gsQuarkGlobal);

    return result;
}
