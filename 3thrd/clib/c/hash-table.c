
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-22.
//

#include "hash-table.h"

#include "atomic.h"

// 1 << 3 == 8 buckets
#define HASH_TABLE_MIN_SHIFT        3
#define UNUSED_HASH_VALUE           0
#define TOMBSTONE_HASH_VALUE        1

#define HASH_IS_REAL(h_)            ((h_) >= 2)
#define HASH_IS_UNUSED(h_)          ((h_) == UNUSED_HASH_VALUE)
#define HASH_IS_TOMBSTONE(h_)       ((h_) == TOMBSTONE_HASH_VALUE)

#define BIG_ENTRY_SIZE              (SIZEOF_VOID_P)
#define SMALL_ENTRY_SIZE            (SIZEOF_INT)

//#if SMALL_ENTRY_SIZE < BIG_ENTRY_SIZE
//#endif
#undef USE_SMALL_ARRAYS

#define DEFINE_RESIZE_FUNC(fname) \
static void fname (CHashTable* hashTable, cuint oldSize, cuint32* reallocatedBucketsBitmap) \
{ \
    cuint i; \
    for (i = 0; i < oldSize; i++) { \
        cuint nodeHash = hashTable->hashes[i]; \
        void* key, *value C_UNUSED; \
        if (!HASH_IS_REAL (nodeHash)) { \
            hashTable->hashes[i] = UNUSED_HASH_VALUE; \
            continue; \
        } \
        if (get_status_bit (reallocatedBucketsBitmap, i)) { \
            continue; \
        } \
        hashTable->hashes[i] = UNUSED_HASH_VALUE; \
        EVICT_KEYVAL (hashTable, i, NULL, NULL, key, value); \
        for (;;) { \
            cuint hashVal; \
            cuint replacedHash; \
            cuint step = 0; \
            hashVal = c_hash_table_hash_to_index (hashTable, nodeHash); \
            while (get_status_bit (reallocatedBucketsBitmap, hashVal)) { \
                step++; \
                hashVal += step; \
                hashVal &= hashTable->mask; \
            } \
            set_status_bit (reallocatedBucketsBitmap, hashVal); \
            replacedHash = hashTable->hashes[hashVal]; \
            hashTable->hashes[hashVal] = nodeHash; \
            if (!HASH_IS_REAL (replacedHash)) { \
                ASSIGN_KEYVAL (hashTable, hashVal, key, value); \
                break; \
            } \
            nodeHash = replacedHash; \
            EVICT_KEYVAL (hashTable, hashVal, key, value, key, value); \
        } \
    } \
}


struct _CHashTable
{
    csize               size;
    cint                mod;
    cuint               mask;
    cuint               nnodes;
    cuint               noccupied;  /* nnodes + tombstones */

    cuint               haveBigKeys : 1;
    cuint               haveBigValues : 1;

    void*               keys;
    cuint*              hashes;
    void*               values;

    CHashFunc           hashFunc;
    CEqualFunc          keyEqualFunc;
    catomicrefcount     refCount;
    int                 version;
    CDestroyNotify      keyDestroyFunc;
    CDestroyNotify      valueDestroyFunc;
};

typedef struct
{
    CHashTable*         hashTable;
    void*               dummy1;
    void*               dummy2;
    cint                position;
    bool                dummy3;
    cint                version;
} RealIter;


static int c_hash_table_find_closest_shift (int n);
static void c_hash_table_resize (CHashTable* hashTable);
static void iter_remove_or_steal (RealIter* ri, bool notify);
static void c_hash_table_setup_storage (CHashTable* hashTable);
static void realloc_arrays (CHashTable* hashTable, bool isASet);
static inline void set_status_bit (cuint32 *bitmap, cuint index);
static inline void c_hash_table_maybe_resize (CHashTable* hashTable);
static void c_hash_table_set_shift (CHashTable* hashTable, int shift);
static inline bool get_status_bit (const cuint32 *bitmap, cuint index);
static void c_hash_table_set_shift_from_size (CHashTable* hashTable, cint size);
static void* c_hash_table_fetch_key_or_value (void* a, cuint index, bool isBig);
static void c_hash_table_remove_node (CHashTable* hashTable, int i, bool notify);
static inline cuint c_hash_table_hash_to_index (CHashTable* hashTable, cuint hash);
static void* c_hash_table_evict_key_or_value (void* a, cuint index, bool isBig, void* v);
static void c_hash_table_assign_key_or_value (void* a, cuint index, bool isBig, void* v);
static bool c_hash_table_remove_internal (CHashTable* hashTable, const void* key, bool notify);
static void* c_hash_table_realloc_key_or_value_array (void* a, cuint size, C_UNUSED bool isBig);
static void c_hash_table_remove_all_nodes (CHashTable* hashTable, bool notify, bool destruction);
static inline void c_hash_table_ensure_keyval_fits (CHashTable* hashTable, void* key, void* value);
static inline cuint c_hash_table_lookup_node (CHashTable* hashTable, const void* key, cuint* hashReturn);
static bool c_hash_table_insert_internal (CHashTable* hashTable, void* key, void* value, bool keepNewKey);
static cuint c_hash_table_foreach_remove_or_steal (CHashTable* hashTable, CHRFunc func, void* udata, bool notify);
static bool c_hash_table_insert_node (CHashTable* hashTable, cuint nodeIndex, cuint keyHash, void* newKey, void* newValue, bool keepNewKey, bool reusingKey);


static const cint gsPrimeMod [] =
    {
        1,          /* For 1 << 0 */
        2,
        3,
        7,
        13,
        31,
        61,
        127,
        251,
        509,
        1021,
        2039,
        4093,
        8191,
        16381,
        32749,
        65521,      /* For 1 << 16 */
        131071,
        262139,
        524287,
        1048573,
        2097143,
        4194301,
        8388593,
        16777213,
        33554393,
        67108859,
        134217689,
        268435399,
        536870909,
        1073741789,
        2147483647  /* For 1 << 31 */
    };


#define ASSIGN_KEYVAL(ht, index, key, value) C_STMT_START{ \
    c_hash_table_assign_key_or_value ((ht)->keys, (index), (ht)->haveBigKeys, (key)); \
    c_hash_table_assign_key_or_value ((ht)->values, (index), (ht)->haveBigValues, (value)); \
} C_STMT_END

#define EVICT_KEYVAL(ht, index, key, value, outkey, outvalue) C_STMT_START{ \
    (outkey) = c_hash_table_evict_key_or_value ((ht)->keys, (index), (ht)->haveBigKeys, (key)); \
    (outvalue) = c_hash_table_evict_key_or_value ((ht)->values, (index), (ht)->haveBigValues, (value)); \
} C_STMT_END

DEFINE_RESIZE_FUNC (resize_map)

#undef ASSIGN_KEYVAL
#undef EVICT_KEYVAL

#define ASSIGN_KEYVAL(ht, index, key, value) C_STMT_START{ \
    c_hash_table_assign_key_or_value ((ht)->keys, (index), (ht)->haveBigKeys, (key)); \
} C_STMT_END

#define EVICT_KEYVAL(ht, index, key, value, outkey, outvalue) C_STMT_START{ \
    (outkey) = c_hash_table_evict_key_or_value ((ht)->keys, (index), (ht)->haveBigKeys, (key)); \
} C_STMT_END

DEFINE_RESIZE_FUNC (resize_set)

#undef ASSIGN_KEYVAL
#undef EVICT_KEYVAL



CHashTable* c_hash_table_new (CHashFunc hashFunc, CEqualFunc keyEqualFunc)
{
    return c_hash_table_new_full (hashFunc, keyEqualFunc, NULL, NULL);
}

CHashTable* c_hash_table_new_full (CHashFunc hashFunc, CEqualFunc keyEqualFunc, CDestroyNotify keyDestroyFunc, CDestroyNotify valueDestroyFunc)
{
    CHashTable* hashTable = c_malloc0(sizeof(CHashTable));
    c_atomic_ref_count_init (&hashTable->refCount);
    hashTable->nnodes             = 0;
    hashTable->noccupied          = 0;
    hashTable->hashFunc           = hashFunc ? hashFunc : c_direct_hash;
    hashTable->keyEqualFunc       = keyEqualFunc;
    hashTable->version            = 0;
    hashTable->keyDestroyFunc     = keyDestroyFunc;
    hashTable->valueDestroyFunc   = valueDestroyFunc;

    c_hash_table_setup_storage (hashTable);

    return hashTable;
}

CHashTable* c_hash_table_new_similar (CHashTable* otherHashTable)
{
    c_return_val_if_fail (otherHashTable, NULL);

    return c_hash_table_new_full (otherHashTable->hashFunc, otherHashTable->keyEqualFunc, otherHashTable->keyDestroyFunc, otherHashTable->valueDestroyFunc);
}

void c_hash_table_destroy (CHashTable* hashTable)
{
    c_return_if_fail (hashTable != NULL);
    c_hash_table_remove_all (hashTable);
    c_hash_table_unref (hashTable);
}

bool c_hash_table_insert (CHashTable* hashTable, void* key, void* value)
{
    return c_hash_table_insert_internal (hashTable, key, value, false);
}

bool c_hash_table_replace (CHashTable* hashTable, void* key, void* value)
{
    return c_hash_table_insert_internal (hashTable, key, value, true);
}

bool c_hash_table_add (CHashTable* hashTable, void* key)
{
    return c_hash_table_insert_internal (hashTable, key, key, true);
}

bool c_hash_table_remove (CHashTable* hashTable, const void* key)
{
    return c_hash_table_remove_internal (hashTable, key, true);
}

void c_hash_table_remove_all (CHashTable* hashTable)
{
    c_return_if_fail (hashTable != NULL);

    if (hashTable->nnodes != 0) {
        hashTable->version++;
    }

    c_hash_table_remove_all_nodes (hashTable, true, false);
    c_hash_table_maybe_resize (hashTable);
}

bool c_hash_table_steal (CHashTable* hashTable, const void* key)
{
    return c_hash_table_remove_internal (hashTable, key, false);
}

bool c_hash_table_steal_extended (CHashTable* hashTable, const void* lookupKey, void** stolenKey, void** stolenValue)
{
    cuint nodeIndex;
    cuint nodeHash;

    c_return_val_if_fail (hashTable != NULL, false);

    nodeIndex = c_hash_table_lookup_node (hashTable, lookupKey, &nodeHash);

    if (!HASH_IS_REAL (hashTable->hashes[nodeIndex])) {
        if (stolenKey != NULL) {
            *stolenKey = NULL;
        }

        if (stolenValue != NULL) {
            *stolenValue = NULL;
        }

        return false;
    }

    if (stolenKey != NULL) {
        *stolenKey = c_hash_table_fetch_key_or_value (hashTable->keys, nodeIndex, (int) hashTable->haveBigKeys);
        c_hash_table_assign_key_or_value (hashTable->keys, nodeIndex, (int) hashTable->haveBigKeys, NULL);
    }

    if (stolenValue != NULL) {
        *stolenValue = c_hash_table_fetch_key_or_value (hashTable->values, nodeIndex, (int) hashTable->haveBigValues);
        c_hash_table_assign_key_or_value (hashTable->values, nodeIndex, (int) hashTable->haveBigValues, NULL);
    }

    c_hash_table_remove_node (hashTable, (int) nodeIndex, false);
    c_hash_table_maybe_resize (hashTable);

    hashTable->version++;

    return true;
}

void c_hash_table_steal_all (CHashTable* hashTable)
{
    c_return_if_fail (hashTable != NULL);

    if (hashTable->nnodes != 0) {
        hashTable->version++;
    }

    c_hash_table_remove_all_nodes (hashTable, false, false);
    c_hash_table_maybe_resize (hashTable);
}

CPtrArray* c_hash_table_steal_all_keys (CHashTable* hashTable)
{
    CDestroyNotify keyDestroyFunc;

    c_return_val_if_fail (hashTable != NULL, NULL);

    CPtrArray* array = c_hash_table_get_keys_as_ptr_array (hashTable);

    keyDestroyFunc = c_steal_pointer (&hashTable->keyDestroyFunc);
    c_ptr_array_set_free_func (array, keyDestroyFunc);

    c_hash_table_remove_all (hashTable);
    hashTable->keyDestroyFunc = c_steal_pointer (&keyDestroyFunc);

    return array;
}

CPtrArray* c_hash_table_steal_all_values (CHashTable* hashTable)
{
    CDestroyNotify valueDestroyFunc;

    c_return_val_if_fail (hashTable != NULL, NULL);

    CPtrArray* array = c_hash_table_get_values_as_ptr_array (hashTable);

    valueDestroyFunc = c_steal_pointer (&hashTable->valueDestroyFunc);
    c_ptr_array_set_free_func (array, valueDestroyFunc);

    c_hash_table_remove_all (hashTable);
    hashTable->valueDestroyFunc = c_steal_pointer (&valueDestroyFunc);

    return array;
}

void* c_hash_table_lookup (CHashTable* hashTable, const void* key)
{
    cuint nodeIndex;
    cuint nodeHash;

    c_return_val_if_fail (hashTable != NULL, NULL);

    nodeIndex = c_hash_table_lookup_node (hashTable, key, &nodeHash);

    return HASH_IS_REAL (hashTable->hashes[nodeIndex]) ? c_hash_table_fetch_key_or_value (hashTable->values, nodeIndex, hashTable->haveBigValues) : NULL;
}

bool c_hash_table_contains (CHashTable* hashTable, const void* key)
{
    cuint nodeIndex;
    cuint nodeHash;

    c_return_val_if_fail (hashTable != NULL, false);

    nodeIndex = c_hash_table_lookup_node (hashTable, key, &nodeHash);

    return HASH_IS_REAL (hashTable->hashes[nodeIndex]);
}

bool c_hash_table_lookup_extended (CHashTable* hashTable, const void* lookupKey, void** origKey, void** value)
{
    cuint nodeHash;

    c_return_val_if_fail (hashTable != NULL, false);

    cuint nodeIndex = c_hash_table_lookup_node (hashTable, lookupKey, &nodeHash);

    if (!HASH_IS_REAL (hashTable->hashes[nodeIndex])) {
        if (origKey != NULL) {
            *origKey = NULL;
        }

        if (value != NULL) {
            *value = NULL;
        }

        return false;
    }

    if (origKey) {
        *origKey = c_hash_table_fetch_key_or_value (hashTable->keys, nodeIndex, hashTable->haveBigKeys);
    }

    if (value) {
        *value = c_hash_table_fetch_key_or_value (hashTable->values, nodeIndex, hashTable->haveBigValues);
    }

    return true;
}

void c_hash_table_foreach (CHashTable* hashTable, CHFunc func, void* udata)
{
    csize i;

    c_return_if_fail (hashTable != NULL);
    c_return_if_fail (func != NULL);

    cint version = hashTable->version;

    for (i = 0; i < hashTable->size; i++) {
        cuint nodeHash = hashTable->hashes[i];
        void* nodeKey = c_hash_table_fetch_key_or_value (hashTable->keys, i, (int) hashTable->haveBigKeys);
        void* nodeValue = c_hash_table_fetch_key_or_value (hashTable->values, i, (int) hashTable->haveBigValues);
        if (HASH_IS_REAL (nodeHash)) {
            (*func) (nodeKey, nodeValue, udata);
        }
        c_return_if_fail (version == hashTable->version);
    }
}

void* c_hash_table_find (CHashTable* hashTable, CHRFunc predicate, void* udata)
{
    csize i;
    bool match;

    c_return_val_if_fail (hashTable != NULL, NULL);
    c_return_val_if_fail (predicate != NULL, NULL);

    cint version = hashTable->version;

    match = false;

    for (i = 0; i < hashTable->size; i++) {
        cuint nodeHash = hashTable->hashes[i];
        void* nodeKey = c_hash_table_fetch_key_or_value (hashTable->keys, i, (int) hashTable->haveBigKeys);
        void* nodeValue = c_hash_table_fetch_key_or_value (hashTable->values, i, (int) hashTable->haveBigValues);

        if (HASH_IS_REAL (nodeHash)) {
            match = predicate (nodeKey, nodeValue, udata);
        }

        c_return_val_if_fail (version == hashTable->version, NULL);

        if (match) {
            return nodeValue;
        }
    }

    return NULL;
}

cuint c_hash_table_foreach_remove (CHashTable* hashTable, CHRFunc func, void* udata)
{
    c_return_val_if_fail (hashTable != NULL, 0);
    c_return_val_if_fail (func != NULL, 0);

    return c_hash_table_foreach_remove_or_steal (hashTable, func, udata, true);
}

cuint c_hash_table_foreach_steal (CHashTable* hashTable, CHRFunc func, void* udata)
{
    c_return_val_if_fail (hashTable != NULL, 0);
    c_return_val_if_fail (func != NULL, 0);

    return c_hash_table_foreach_remove_or_steal (hashTable, func, udata, false);
}

cuint c_hash_table_size (CHashTable* hashTable)
{
    c_return_val_if_fail (hashTable != NULL, 0);

    return hashTable->nnodes;
}

CList* c_hash_table_get_keys (CHashTable* hashTable)
{
    csize i;

    c_return_val_if_fail (hashTable != NULL, NULL);

    CList* retval = NULL;
    for (i = 0; i < hashTable->size; i++) {
        if (HASH_IS_REAL (hashTable->hashes[i])) {
            retval = c_list_prepend (retval, c_hash_table_fetch_key_or_value (hashTable->keys, i, (int) hashTable->haveBigKeys));
        }
    }

    return retval;
}

CList* c_hash_table_get_values (CHashTable* hashTable)
{
    csize i;
    CList* retval;

    c_return_val_if_fail (hashTable != NULL, NULL);

    retval = NULL;
    for (i = 0; i < hashTable->size; i++) {
        if (HASH_IS_REAL (hashTable->hashes[i])) {
            retval = c_list_prepend (retval, c_hash_table_fetch_key_or_value (hashTable->values, i, hashTable->haveBigValues));
        }
    }

    return retval;
}

void** c_hash_table_get_keys_as_array (CHashTable* hashTable, cuint* length)
{
    csize i, j = 0;

    void** result = c_malloc0(sizeof(void*) * hashTable->nnodes + 1);
    for (i = 0; i < hashTable->size; i++) {
        if (HASH_IS_REAL (hashTable->hashes[i])) {
            result[j++] = c_hash_table_fetch_key_or_value (hashTable->keys, i, (int) hashTable->haveBigKeys);
        }
    }
    c_assert (j == hashTable->nnodes);
    result[j] = NULL;

    if (length) {
        *length = j;
    }

    return result;
}

CPtrArray* c_hash_table_get_keys_as_ptr_array (CHashTable* hashTable)
{
    c_return_val_if_fail (hashTable != NULL, NULL);

    CPtrArray* array = c_ptr_array_sized_new (hashTable->size);
    csize i = 0;
    for (i = 0; i < hashTable->size; ++i) {
        if (HASH_IS_REAL (hashTable->hashes[i])) {
            c_ptr_array_add (array, c_hash_table_fetch_key_or_value (hashTable->keys, i, (int) hashTable->haveBigKeys));
        }
    }
    c_assert (array->len == hashTable->nnodes);

    return array;
}

CPtrArray* c_hash_table_get_values_as_ptr_array (CHashTable* hashTable)
{
    CPtrArray *array;

    c_return_val_if_fail (hashTable != NULL, NULL);

    array = c_ptr_array_sized_new (hashTable->size);
    csize i = 0;
    for (i = 0; i < hashTable->size; ++i) {
        if (HASH_IS_REAL (hashTable->hashes[i])) {
            c_ptr_array_add (array, c_hash_table_fetch_key_or_value (hashTable->values, i, hashTable->haveBigValues));
        }
    }
    c_assert (array->len == hashTable->nnodes);

    return array;
}

void c_hash_table_iter_init (CHashTableIter* iter, CHashTable* hashTable)
{
    RealIter *ri = (RealIter*) iter;

    c_return_if_fail (iter != NULL);
    c_return_if_fail (hashTable != NULL);

    ri->hashTable = hashTable;
    ri->position = -1;
    ri->version = hashTable->version;
}

bool c_hash_table_iter_next (CHashTableIter* iter, void** key, void** value)
{
    RealIter *ri = (RealIter*) iter;
    int position;

    c_return_val_if_fail (iter != NULL, false);
    c_return_val_if_fail (ri->version == ri->hashTable->version, false);
    c_return_val_if_fail (ri->position < (csize) ri->hashTable->size, false);

    position = ri->position;

    do {
        position++;
        if (position >= (csize) ri->hashTable->size) {
            ri->position = position;
            return false;
        }
    }
    while (!HASH_IS_REAL (ri->hashTable->hashes[position]));

    if (key != NULL) {
        *key = c_hash_table_fetch_key_or_value (ri->hashTable->keys, position, (int) ri->hashTable->haveBigKeys);
    }

    if (value != NULL) {
        *value = c_hash_table_fetch_key_or_value (ri->hashTable->values, position, (int) ri->hashTable->haveBigValues);
    }

    ri->position = position;

    return true;
}

CHashTable* c_hash_table_iter_get_hash_table (CHashTableIter* iter)
{
    c_return_val_if_fail (iter != NULL, NULL);

    return ((RealIter *) iter)->hashTable;
}

void c_hash_table_iter_remove (CHashTableIter* iter)
{
    iter_remove_or_steal ((RealIter*) iter, true);
}

void c_hash_table_iter_replace (CHashTableIter* iter, void* value)
{
    RealIter* ri;
    cuint nodeHash;
    void* key;

    ri = (RealIter*) iter;

    c_return_if_fail (ri != NULL);
    c_return_if_fail (ri->version == ri->hashTable->version);
    c_return_if_fail (ri->position >= 0);
    c_return_if_fail ((csize) ri->position < ri->hashTable->size);

    nodeHash = ri->hashTable->hashes[ri->position];

    key = c_hash_table_fetch_key_or_value (ri->hashTable->keys, ri->position, (int) ri->hashTable->haveBigKeys);

    c_hash_table_insert_node (ri->hashTable, ri->position, nodeHash, key, value, true, true);

    ri->version++;
    ri->hashTable->version++;
}

void c_hash_table_iter_steal (CHashTableIter* iter)
{
    iter_remove_or_steal ((RealIter*) iter, false);
}

CHashTable* c_hash_table_ref (CHashTable* hashTable)
{
    c_return_val_if_fail (hashTable != NULL, NULL);
    c_atomic_ref_count_inc (&hashTable->refCount);

    return hashTable;
}

void c_hash_table_unref (CHashTable* hashTable)
{
    c_return_if_fail (hashTable != NULL);

    if (c_atomic_ref_count_dec (&hashTable->refCount)) {
        c_hash_table_remove_all_nodes (hashTable, true, true);
        if (hashTable->keys != hashTable->values) {
            c_free (hashTable->values);
        }
        c_free (hashTable->keys);
        c_free (hashTable->hashes);
        c_free (hashTable);
    }
}

cuint c_int_hash (const void* v)
{
    return *(const cint*) v;
}

cuint c_str_hash (const void* v)
{
    const signed char *p;
    cuint32 h = 5381;

    for (p = v; *p != '\0'; p++) {
        h = (h << 5) + h + *p;
    }

    return h;
}

cuint c_int64_hash (const void* v)
{
    const cuint64 *bits = v;

    return (cuint) ((*bits >> 32) ^ (*bits & 0xffffffffU));
}

cuint c_double_hash (const void* v)
{
    const cuint64 *bits = v;

    return (cuint) ((*bits >> 32) ^ (*bits & 0xffffffffU));
}

cuint c_direct_hash (const void* v)
{
    return C_POINTER_TO_UINT (v);
}


static void c_hash_table_set_shift (CHashTable* hashTable, int shift)
{
    hashTable->size = 1 << shift;
    hashTable->mod  = gsPrimeMod[shift];

    c_assert ((hashTable->size & (hashTable->size - 1)) == 0);
    hashTable->mask = hashTable->size - 1;
}


static int c_hash_table_find_closest_shift (int n)
{
    int i;

    for (i = 0; n; i++) {
        n >>= 1;
    }

    return i;
}


static void c_hash_table_set_shift_from_size (CHashTable* hashTable, cint size)
{
    cint shift;

    shift = c_hash_table_find_closest_shift (size);
    shift = C_MAX (shift, HASH_TABLE_MIN_SHIFT);

    c_hash_table_set_shift (hashTable, shift);
}

static void* c_hash_table_realloc_key_or_value_array (void* a, cuint size, C_UNUSED bool isBig)
{
    return c_realloc (a, size * (isBig ? BIG_ENTRY_SIZE : SMALL_ENTRY_SIZE));
}

static void* c_hash_table_fetch_key_or_value (void* a, cuint index, bool isBig)
{
    isBig = true;

    return isBig ? *(((void**) a) + index) : C_UINT_TO_POINTER (*(((cuint*) a) + index));
}

static void c_hash_table_assign_key_or_value (void* a, cuint index, bool isBig, void* v)
{
    isBig = true;

    if (isBig) {
        *(((void**) a) + index) = v;
    }
    else {
        *(((cuint*) a) + index) = C_POINTER_TO_UINT (v);
    }
}

static void* c_hash_table_evict_key_or_value (void* a, cuint index, bool isBig, void* v)
{
    isBig = true;

    if (isBig) {
        void* r = *(((void**) a) + index);
        *(((void**) a) + index) = v;
        return r;
    }
    else {
        void*r = C_UINT_TO_POINTER (*(((cuint*) a) + index));
        *(((cuint*) a) + index) = C_POINTER_TO_UINT (v);
        return r;
    }
}

static inline cuint c_hash_table_hash_to_index (CHashTable* hashTable, cuint hash)
{
    return (hash * 11) % hashTable->mod;
}

static inline cuint c_hash_table_lookup_node (CHashTable* hashTable, const void* key, cuint* hashReturn)
{
    cuint nodeIndex;
    cuint nodeHash;
    cuint hashValue;
    cuint step = 0;
    cuint firstTombstone = 0;
    bool haveTombstone = false;

    hashValue = hashTable->hashFunc (key);
    if (C_UNLIKELY (!HASH_IS_REAL (hashValue))) {
        hashValue = 2;
    }

    *hashReturn = hashValue;

    nodeIndex = c_hash_table_hash_to_index (hashTable, hashValue);
    nodeHash = hashTable->hashes[nodeIndex];

    while (!HASH_IS_UNUSED (nodeHash)) {
        if (nodeHash == hashValue) {
            void* nodeKey = c_hash_table_fetch_key_or_value (hashTable->keys, nodeIndex, hashTable->haveBigKeys);
            if (hashTable->keyEqualFunc) {
                if (hashTable->keyEqualFunc(nodeKey, key)) {
                    return nodeIndex;
                }
            }
            else if (nodeKey == key) {
                return nodeIndex;
            }
        }
        else if (HASH_IS_TOMBSTONE (nodeHash) && !haveTombstone) {
            firstTombstone = nodeIndex;
            haveTombstone = true;
        }

        step++;
        nodeIndex += step;
        nodeIndex &= hashTable->mask;
        nodeHash = hashTable->hashes[nodeIndex];
    }

    if (haveTombstone) {
        return firstTombstone;
    }

    return nodeIndex;
}

static void c_hash_table_remove_node (CHashTable* hashTable, int i, bool notify)
{
    c_return_if_fail(hashTable);

    void* key = c_hash_table_fetch_key_or_value (hashTable->keys, i, (bool) hashTable->haveBigKeys);
    void* value = c_hash_table_fetch_key_or_value (hashTable->values, i, (bool) hashTable->haveBigValues);

    /* Erect tombstone */
    hashTable->hashes[i] = TOMBSTONE_HASH_VALUE;

    /* Be GC friendly */
    c_hash_table_assign_key_or_value (hashTable->keys, i, (bool) hashTable->haveBigKeys, NULL);
    c_hash_table_assign_key_or_value (hashTable->values, i, (bool) hashTable->haveBigValues, NULL);

    c_assert (hashTable->nnodes > 0);
    hashTable->nnodes--;

    if (notify && hashTable->keyDestroyFunc) {
        hashTable->keyDestroyFunc (key);
    }

    if (notify && hashTable->valueDestroyFunc) {
        hashTable->valueDestroyFunc (value);
    }
}

static void c_hash_table_setup_storage (CHashTable* hashTable)
{
    bool small = false;

    c_hash_table_set_shift (hashTable, HASH_TABLE_MIN_SHIFT);

    hashTable->haveBigKeys = !small;
    hashTable->haveBigValues = !small;
    hashTable->keys = c_hash_table_realloc_key_or_value_array (NULL, hashTable->size, hashTable->haveBigKeys);
    hashTable->values = hashTable->keys;
    hashTable->hashes = c_malloc0(sizeof(cuint) * hashTable->size);
}

static void c_hash_table_remove_all_nodes (CHashTable* hashTable, bool notify, bool destruction)
{
    int i;
    void* key;
    void* value;
    int oldSize;
    void** oldKeys;
    void** oldValues;
    cuint* oldHashes;
    bool oldHaveBigKeys;
    bool oldHaveBigValues;

    if (hashTable->nnodes == 0) {
        return;
    }

    hashTable->nnodes = 0;
    hashTable->noccupied = 0;

    if (!notify || (hashTable->keyDestroyFunc == NULL && hashTable->valueDestroyFunc == NULL)) {
        if (!destruction) {
            memset (hashTable->hashes, 0, hashTable->size * sizeof (cuint));
            memset (hashTable->keys, 0, hashTable->size * sizeof (void*));
            memset (hashTable->values, 0, hashTable->size * sizeof (void*));
        }
        return;
    }

    oldSize = hashTable->size;
    oldHaveBigKeys = hashTable->haveBigKeys;
    oldHaveBigValues = hashTable->haveBigValues;
    oldKeys   = c_steal_pointer (&hashTable->keys);
    oldValues = c_steal_pointer (&hashTable->values);
    oldHashes = c_steal_pointer (&hashTable->hashes);

    if (!destruction) {
        c_hash_table_setup_storage (hashTable);
    }
    else {
        hashTable->size = hashTable->mod = (hashTable->mask) = 0;
    }

    for (i = 0; i < oldSize; i++) {
        if (HASH_IS_REAL (oldHashes[i])) {
            key = c_hash_table_fetch_key_or_value (oldKeys, i, oldHaveBigKeys);
            value = c_hash_table_fetch_key_or_value (oldValues, i, oldHaveBigValues);

            oldHashes[i] = UNUSED_HASH_VALUE;

            c_hash_table_assign_key_or_value (oldKeys, i, oldHaveBigKeys, NULL);
            c_hash_table_assign_key_or_value (oldValues, i, oldHaveBigValues, NULL);

            if (hashTable->keyDestroyFunc != NULL) {
                hashTable->keyDestroyFunc (key);
            }

            if (hashTable->valueDestroyFunc != NULL) {
                hashTable->valueDestroyFunc (value);
            }
        }
    }

    /* Destroy old storage space. */
    if (oldKeys != oldValues) {
        c_free (oldValues);
    }

    c_free (oldKeys);
    c_free (oldHashes);
}

static void realloc_arrays (CHashTable* hashTable, bool isASet)
{
    hashTable->hashes = c_realloc(hashTable->hashes, hashTable->size);
    hashTable->keys = c_hash_table_realloc_key_or_value_array (hashTable->keys, hashTable->size, (bool) hashTable->haveBigKeys);

    if (isASet) {
        hashTable->values = hashTable->keys;
    }
    else {
        hashTable->values = c_hash_table_realloc_key_or_value_array (hashTable->values, hashTable->size, (bool) hashTable->haveBigValues);
    }
}


static inline bool get_status_bit (const cuint32 *bitmap, cuint index)
{
    return (bool) (bitmap[index / 32] >> (index % 32)) & 1;
}

static inline void set_status_bit (cuint32 *bitmap, cuint index)
{
    bitmap[index / 32] |= 1U << (index % 32);
}

static void c_hash_table_resize (CHashTable* hashTable)
{
    cuint32* reallocatedBucketsBitmap;
    csize oldSize;
    bool isASet;

    oldSize = hashTable->size;
    isASet = hashTable->keys == hashTable->values;

    c_hash_table_set_shift_from_size (hashTable, hashTable->nnodes * 1.333);

    if (hashTable->size > oldSize) {
        realloc_arrays (hashTable, isASet);
        memset (&hashTable->hashes[oldSize], 0, (hashTable->size - oldSize) * sizeof (cuint));
        reallocatedBucketsBitmap = c_malloc0(sizeof(cuint32) * (hashTable->size + 31) / 32);
    }
    else {
        reallocatedBucketsBitmap = c_malloc0(sizeof(cuint32) * (oldSize + 31) / 32);
    }

    if (isASet) {
        resize_set (hashTable, oldSize, reallocatedBucketsBitmap);
    }
    else {
        resize_map (hashTable, oldSize, reallocatedBucketsBitmap);
    }

    c_free (reallocatedBucketsBitmap);

    if (hashTable->size < oldSize) {
        realloc_arrays (hashTable, isASet);
    }

    hashTable->noccupied = hashTable->nnodes;
}

static inline void c_hash_table_maybe_resize (CHashTable* hashTable)
{
    csize noccupied = hashTable->noccupied;
    csize size = hashTable->size;

    if ((size > hashTable->nnodes * 4 && size > 1 << HASH_TABLE_MIN_SHIFT) || (size <= noccupied + (noccupied / 16))) {
        c_hash_table_resize (hashTable);
    }
}

static inline void c_hash_table_ensure_keyval_fits (CHashTable* hashTable, void* key, void* value)
{
    bool isASet = (hashTable->keys == hashTable->values);

    /* Just split if necessary */
    if (isASet && key != value) {
        hashTable->values = c_memdup (hashTable->keys, sizeof (void*) * hashTable->size);
    }
}

static void iter_remove_or_steal (RealIter* ri, bool notify)
{
    c_return_if_fail (ri != NULL);
    c_return_if_fail (ri->version == ri->hashTable->version);
    c_return_if_fail (ri->position >= 0);
    c_return_if_fail ((csize) ri->position < ri->hashTable->size);
    c_hash_table_remove_node (ri->hashTable, ri->position, notify);

    ri->version++;
    ri->hashTable->version++;
}

static bool c_hash_table_insert_node (CHashTable* hashTable, cuint nodeIndex, cuint keyHash, void* newKey, void* newValue, bool keepNewKey, bool reusingKey)
{
    bool alreadyExists;
    cuint oldHash;
    void* keyToFree = NULL;
    void* keyToKeep = NULL;
    void* valueToFree = NULL;

    oldHash = hashTable->hashes[nodeIndex];
    alreadyExists = HASH_IS_REAL (oldHash);

    if (alreadyExists) {
        valueToFree = c_hash_table_fetch_key_or_value (hashTable->values, nodeIndex, (int) hashTable->haveBigValues);

        if (keepNewKey) {
            keyToFree = c_hash_table_fetch_key_or_value (hashTable->keys, nodeIndex, (int) hashTable->haveBigKeys);
            keyToKeep = newKey;
        }
        else {
            keyToFree = newKey;
            keyToKeep = c_hash_table_fetch_key_or_value (hashTable->keys, nodeIndex, (int) hashTable->haveBigKeys);
        }
    }
    else {
        hashTable->hashes[nodeIndex] = keyHash;
        keyToKeep = newKey;
    }

    c_hash_table_ensure_keyval_fits (hashTable, keyToKeep, newValue);
    c_hash_table_assign_key_or_value (hashTable->keys, nodeIndex, (int) hashTable->haveBigKeys, keyToKeep);

    c_hash_table_assign_key_or_value (hashTable->values, nodeIndex, (int) hashTable->haveBigValues, newValue);

    /* Now, the bookkeeping... */
    if (!alreadyExists) {
        hashTable->nnodes++;
        if (HASH_IS_UNUSED (oldHash)) {
            hashTable->noccupied++;
            c_hash_table_maybe_resize (hashTable);
        }
        hashTable->version++;
    }

    if (alreadyExists) {
        if (hashTable->keyDestroyFunc && !reusingKey) {
            (*hashTable->keyDestroyFunc) (keyToFree);
        }
        if (hashTable->valueDestroyFunc) {
            (*hashTable->valueDestroyFunc) (valueToFree);
        }
    }

    return !alreadyExists;
}
static bool c_hash_table_insert_internal (CHashTable* hashTable, void* key, void* value, bool keepNewKey)
{
    cuint keyHash;
    cuint nodeIndex;

    c_return_val_if_fail (hashTable != NULL, false);

    nodeIndex = c_hash_table_lookup_node (hashTable, key, &keyHash);

    return c_hash_table_insert_node (hashTable, nodeIndex, keyHash, key, value, keepNewKey, false);
}

static bool c_hash_table_remove_internal (CHashTable* hashTable, const void* key, bool notify)
{
    cuint nodeIndex;
    cuint nodeHash;

    c_return_val_if_fail (hashTable != NULL, false);

    nodeIndex = c_hash_table_lookup_node (hashTable, key, &nodeHash);

    if (!HASH_IS_REAL (hashTable->hashes[nodeIndex])) {
        return false;
    }

    c_hash_table_remove_node (hashTable, (int) nodeIndex, notify);
    c_hash_table_maybe_resize (hashTable);

    hashTable->version++;

    return true;
}

static cuint c_hash_table_foreach_remove_or_steal (CHashTable* hashTable, CHRFunc func, void* udata, bool notify)
{
    cuint deleted = 0;
    csize i;
    cint version = hashTable->version;

    for (i = 0; i < hashTable->size; i++) {
        cuint nodeHash = hashTable->hashes[i];
        void* nodeKey = c_hash_table_fetch_key_or_value (hashTable->keys, i, (int) hashTable->haveBigKeys);
        void* nodeValue = c_hash_table_fetch_key_or_value (hashTable->values, i, (int) hashTable->haveBigValues);

        if (HASH_IS_REAL (nodeHash) && (*func) (nodeKey, nodeValue, udata)) {
            c_hash_table_remove_node (hashTable, (int) i, notify);
            deleted++;
        }
        c_return_val_if_fail (version == hashTable->version, 0);
    }

    c_hash_table_maybe_resize (hashTable);

    if (deleted > 0) {
        hashTable->version++;
    }

    return deleted;
}
