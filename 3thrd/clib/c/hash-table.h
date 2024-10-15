
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

#ifndef CLIBRARY_HASH_TABLE_H
#define CLIBRARY_HASH_TABLE_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/list.h>
#include <c/array.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C


typedef struct _CHashTable          CHashTable;
typedef struct _CHashTableIter      CHashTableIter;

typedef bool  (*CHRFunc)  (void* key, void* value, void* udata);


struct _CHashTableIter
{
    /*< private >*/
    void*           dummy1;
    void*           dummy2;
    void*           dummy3;
    int             dummy4;
    bool            dummy5;
    void*           dummy6;
};


CHashTable*     c_hash_table_new                        (CHashFunc hashFunc, CEqualFunc keyEqualFunc);
CHashTable*     c_hash_table_new_full                   (CHashFunc hashFunc, CEqualFunc keyEqualFunc, CDestroyNotify keyDestroyFunc, CDestroyNotify valueDestroyFunc);
CHashTable*     c_hash_table_new_similar                (CHashTable* otherHashTable);
void            c_hash_table_destroy                    (CHashTable* hashTable);
bool            c_hash_table_insert                     (CHashTable* hashTable, void* key, void* value);
bool            c_hash_table_replace                    (CHashTable* hashTable, void* key, void* value);
bool            c_hash_table_add                        (CHashTable* hashTable, void* key);
bool            c_hash_table_remove                     (CHashTable* hashTable, const void* key);
void            c_hash_table_remove_all                 (CHashTable* hashTable);
bool            c_hash_table_steal                      (CHashTable* hashTable, const void* key);
bool            c_hash_table_steal_extended             (CHashTable* hashTable, const void* lookupKey, void** stolenKey, void** stolenValue);
void            c_hash_table_steal_all                  (CHashTable* hashTable);
CPtrArray*      c_hash_table_steal_all_keys             (CHashTable* hashTable);
CPtrArray*      c_hash_table_steal_all_values           (CHashTable* hashTable);

/**
 * @brief 根据 key 查找 value
 * @note
 */
void*           c_hash_table_lookup                     (CHashTable* hashTable, const void* key);

/**
 * @brief 判断 hash 表中是否包含 key
 */
bool            c_hash_table_contains                   (CHashTable* hashTable, const void* key);
bool            c_hash_table_lookup_extended            (CHashTable* hashTable, const void* lookupKey, void** origKey, void** value);
void            c_hash_table_foreach                    (CHashTable* hashTable, CHFunc func, void* udata);
void*           c_hash_table_find                       (CHashTable* hashTable, CHRFunc predicate, void* udata);
cuint           c_hash_table_foreach_remove             (CHashTable* hashTable, CHRFunc func, void* udata);
cuint           c_hash_table_foreach_steal              (CHashTable* hashTable, CHRFunc func, void* udata);
cuint           c_hash_table_size                       (CHashTable* hashTable);
CList*          c_hash_table_get_keys                   (CHashTable* hashTable);
CList*          c_hash_table_get_values                 (CHashTable* hashTable);
void**          c_hash_table_get_keys_as_array          (CHashTable* hashTable, cuint* length);
CPtrArray*      c_hash_table_get_keys_as_ptr_array      (CHashTable* hashTable);
CPtrArray*      c_hash_table_get_values_as_ptr_array    (CHashTable* hashTable);
void            c_hash_table_iter_init                  (CHashTableIter* iter, CHashTable* hashTable);
bool            c_hash_table_iter_next                  (CHashTableIter* iter, void** key, void** value);
CHashTable*     c_hash_table_iter_get_hash_table        (CHashTableIter* iter);
void            c_hash_table_iter_remove                (CHashTableIter* iter);
void            c_hash_table_iter_replace               (CHashTableIter* iter, void* value);
void            c_hash_table_iter_steal                 (CHashTableIter* iter);
CHashTable*     c_hash_table_ref                        (CHashTable* hashTable);
void            c_hash_table_unref                      (CHashTable* hashTable);

cuint    c_int_hash     (const void* v);
cuint    c_str_hash     (const void* v);
cuint    c_int64_hash   (const void* v);
cuint    c_double_hash  (const void* v);
cuint    c_direct_hash  (const void* v) C_CONST;

C_END_EXTERN_C

#endif //CLIBRARY_HASH_TABLE_H
