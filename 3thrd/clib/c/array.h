
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-16.
//

#ifndef CLIBRARY_ARRAY_H
#define CLIBRARY_ARRAY_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

C_BEGIN_EXTERN_C

typedef struct _CBytes              CBytes;
typedef struct _CArray              CArray;
typedef struct _CPtrArray           CPtrArray;
typedef struct _CByteArray          CByteArray;

struct _CArray
{
    char*               data;
    cuint               len;
};

struct _CByteArray
{
    cuint8*             data;
    cuint               len;
};

struct _CPtrArray
{
    void**              pdata;
    cuint               len;
};


#define c_array_append_val(a,v)             c_array_append_vals(a, &(v), 1)
#define c_array_prepend_val(a,v)            c_array_prepend_vals(a, &(v), 1)
#define c_array_insert_val(a,i,v)           c_array_insert_vals(a, i, &(v), 1)
#define c_array_index(a,t,i)                (((t*) (void*) (a)->data) [(i)])
#define c_ptr_array_index(array,index_)     ((array)->pdata)[index_]


CArray* c_array_new                 (bool zeroTerminated, bool clear, csize elementSize);
void*   c_array_steal               (CArray* array, cuint64* len);
CArray* c_array_sized_new           (bool zeroTerminated, bool clear, cuint elementSize, cuint reservedSize);
CArray* c_array_copy                (CArray* array);
char*   c_array_free                (CArray* array, bool freeSegment);
CArray* c_array_ref                 (CArray* array);
void    c_array_unref               (CArray* array);
cuint   c_array_get_element_size    (CArray* array);
CArray* c_array_append_vals         (CArray* array, const void* data, cuint len);
CArray* c_array_prepend_vals        (CArray* array, const void* data, cuint len);
CArray* c_array_insert_vals         (CArray* array, cuint index, const void* data, cuint len);
CArray* c_array_set_size            (CArray* array, cuint length);
CArray* c_array_remove_index        (CArray* array, cuint index);
CArray* c_array_remove_index_fast   (CArray* array, cuint index);
CArray* c_array_remove_range        (CArray* array, cuint index, cuint length);
void    c_array_sort                (CArray* array, CCompareFunc compareFunc);
void    c_array_sort_with_data      (CArray* array, CCompareDataFunc compareFunc, void* udata);
bool    c_array_binary_search       (CArray* array, const void* target, CCompareFunc compareFunc, cuint* outMatchIndex);
void    c_array_set_clear_func      (CArray* array, CDestroyNotify clearFunc);

CPtrArray*  c_ptr_array_new                 (void);
CPtrArray*  c_ptr_array_new_with_free_func  (CDestroyNotify elementFreeFunc);
void**      c_ptr_array_steal               (CPtrArray* array, cuint64* len);
CPtrArray*  c_ptr_array_copy                (CPtrArray* array, CCopyFunc func, void* udata);
CPtrArray*  c_ptr_array_sized_new           (cuint reservedSize);
CPtrArray*  c_ptr_array_new_full            (cuint reservedSize, CDestroyNotify elementFreeFunc);
CPtrArray*  c_ptr_array_new_null_terminated (cuint reservedSize, CDestroyNotify elementFreeFunc, bool nullTerminated);
void**      c_ptr_array_free                (CPtrArray* array, bool freeSeg);
CPtrArray*  c_ptr_array_ref                 (CPtrArray* array);
void        c_ptr_array_unref               (CPtrArray* array);
void        c_ptr_array_set_free_func       (CPtrArray* array, CDestroyNotify elementFreeFunc);
void        c_ptr_array_set_size            (CPtrArray* array, cint length);
void*       c_ptr_array_remove_index        (CPtrArray* array, cuint index);
void*       c_ptr_array_remove_index_fast   (CPtrArray* array, cuint index);
void*       c_ptr_array_steal_index         (CPtrArray* array, cuint index);
void*       c_ptr_array_steal_index_fast    (CPtrArray* array, cuint index);
bool        c_ptr_array_remove              (CPtrArray* array, void* data);
bool        c_ptr_array_remove_fast         (CPtrArray* array, void* data);
CPtrArray*  c_ptr_array_remove_range        (CPtrArray* array, cuint index, cuint length);
void        c_ptr_array_add                 (CPtrArray* array, void* data);
void        c_ptr_array_extend              (CPtrArray* arrayToExtend, CPtrArray* array, CCopyFunc func, void* udata);
void        c_ptr_array_extend_and_steal    (CPtrArray* arrayToExtend, CPtrArray* array);
void        c_ptr_array_insert              (CPtrArray* array, cuint index, void* data);
void        c_ptr_array_sort                (CPtrArray* array, CCompareFunc compareFunc);
void        c_ptr_array_sort_with_data      (CPtrArray* array, CCompareDataFunc compareFunc, void* udata);
void        c_ptr_array_foreach             (CPtrArray* array, CFunc func, void* udata);
bool        c_ptr_array_find                (CPtrArray* haystack, const void* needle, cuint* index);
bool        c_ptr_array_find_with_equal_func(CPtrArray* haystack, const void* needle, CEqualFunc equalFunc, cuint* index);
bool        c_ptr_array_is_null_terminated  (CPtrArray* array);

CByteArray* c_byte_array_new                (void);
CByteArray* c_byte_array_new_take           (cuint8* data, cuint64 len);
cuint8*     c_byte_array_steal              (CByteArray* array, cuint64* len);
CByteArray* c_byte_array_sized_new          (cuint reservedSize);
cuint8*     c_byte_array_free               (CByteArray* array, bool freeSegment);
CBytes*     c_byte_array_free_to_bytes      (CByteArray* array);
CByteArray* c_byte_array_ref                (CByteArray* array);
void        c_byte_array_unref              (CByteArray* array);
CByteArray* c_byte_array_append             (CByteArray* array, const cuint8* data, cuint len);
CByteArray* c_byte_array_prepend            (CByteArray* array, const cuint8* data, cuint len);
CByteArray* c_byte_array_set_size           (CByteArray* array, cuint length);
CByteArray* c_byte_array_remove_index       (CByteArray* array, cuint index);
CByteArray* c_byte_array_remove_index_fast  (CByteArray* array, cuint index);
CByteArray* c_byte_array_remove_range       (CByteArray* array, cuint index, cuint length);
void        c_byte_array_sort               (CByteArray* array, CCompareFunc compareFunc);
void        c_byte_array_sort_with_data     (CByteArray* array, CCompareDataFunc compareFunc, void* udata);

C_END_EXTERN_C

#endif //CLIBRARY_ARRAY_H
