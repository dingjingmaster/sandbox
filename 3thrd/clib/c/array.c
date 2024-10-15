
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

#include "array.h"
#include "bytes.h"
#include "atomic.h"


#define MIN_ARRAY_SIZE  16

#define c_array_elt_len(array,i)    ((csize)(array)->eltSize * (i))
#define c_array_elt_pos(array,i)    ((array)->data + c_array_elt_len((array),(i)))
#define c_array_elt_zero(array, pos, len) \
    (memset (c_array_elt_pos ((array), pos), 0,  c_array_elt_len ((array), len)))
#define c_array_zero_terminate(array) C_STMT_START{ \
    if ((array)->zeroTerminated) \
        c_array_elt_zero ((array), (array)->len, 1); \
} C_STMT_END


typedef struct _CRealArray      CRealArray;
typedef enum _ArrayFreeFlags    ArrayFreeFlags;
typedef struct _CRealPtrArray   CRealPtrArray;


enum _ArrayFreeFlags
{
    FREE_SEGMENT = 1 << 0,
    PRESERVE_WRAPPER = 1 << 1
};

struct _CRealArray
{
    cuint8*                 data;
    cuint                   len;
    cuint                   eltCapacity;
    cuint                   eltSize;
    cuint                   zeroTerminated : 1;
    cuint                   clear : 1;
    catomicrefcount         refCount;
    CDestroyNotify          clearFunc;
};

struct _CRealPtrArray
{
    void**                  pdata;
    cuint                   len;
    cuint                   alloc;
    catomicrefcount         refCount;
    cuint8                  nullTerminated;         // always either 0 or 1, so it can be added to array lengths
    CDestroyNotify          elementFreeFunc;
};



static cchar* array_free (CRealArray*, ArrayFreeFlags);
static void  c_array_maybe_expand (CRealArray* array, cuint len);

static void** ptr_array_free (CPtrArray*, ArrayFreeFlags);
static void ptr_array_maybe_null_terminate (CRealPtrArray* rarray);
static void c_ptr_array_maybe_expand (CRealPtrArray* array, cuint len);
static void* ptr_array_remove_index (CPtrArray* array, cuint index, bool fast, bool freeElement);
static CPtrArray* ptr_array_new (cuint reservedSize, CDestroyNotify elementFreeFunc, bool nullTerminated);



CArray* c_array_new (bool zeroTerminated, bool clear, csize elementSize)
{
    c_return_val_if_fail (elementSize > 0, NULL);
#if (UINT_WIDTH / 8) >= GLIB_SIZEOF_SIZE_T
    c_return_val_if_fail (elementSize <= C_MAX_SIZE / 2 - 1, NULL);
#endif

    return c_array_sized_new (zeroTerminated, clear, elementSize, 0);
}

void* c_array_steal (CArray* array, cuint64* len)
{
    CRealArray* rarray = NULL;
    void*       segment = NULL;

    c_return_val_if_fail (array != NULL, NULL);

    rarray = (CRealArray *) array;
    segment = (void*) rarray->data;

    if (len != NULL) {
        *len = rarray->len;
    }

    rarray->data  = NULL;
    rarray->len   = 0;
    rarray->eltCapacity = 0;

    return segment;
}

CArray* c_array_sized_new (bool zeroTerminated, bool clear, cuint elementSize, cuint reservedSize)
{
    CRealArray *array;

    c_return_val_if_fail (elementSize > 0, NULL);
#if (UINT_WIDTH / 8) >= GLIB_SIZEOF_SIZE_T
    c_return_val_if_fail (elementSize <= C_MAX_SIZE / 2 - 1, NULL);
#endif

    array = c_malloc0(sizeof (CRealArray));

    array->data            = NULL;
    array->len             = 0;
    array->eltCapacity     = 0;
    array->zeroTerminated  = (zeroTerminated ? 1 : 0);
    array->clear           = (clear ? 1 : 0);
    array->eltSize         = elementSize;
    array->clearFunc       = NULL;

    c_atomic_ref_count_init (&array->refCount);

    if (array->zeroTerminated || reservedSize != 0) {
        c_array_maybe_expand (array, reservedSize);
        c_assert (array->data != NULL);
        c_array_zero_terminate (array);
    }

    return (CArray*) array;
}

CArray* c_array_copy (CArray* array)
{
    CRealArray *rarray = (CRealArray*) array;
    CRealArray *newRarray;

    c_return_val_if_fail (rarray != NULL, NULL);

    newRarray = (CRealArray*) c_array_sized_new (rarray->zeroTerminated, rarray->clear, rarray->eltSize, rarray->eltCapacity);
    newRarray->len = rarray->len;
    if (rarray->len > 0) {
        memcpy (newRarray->data, rarray->data, rarray->len * rarray->eltSize);
    }

    c_array_zero_terminate (newRarray);

    return (CArray*) newRarray;
}

char* c_array_free (CArray* farray, bool freeSegment)
{
    CRealArray* array = (CRealArray*) farray;
    ArrayFreeFlags flags;

    c_return_val_if_fail (array, NULL);

    flags = (freeSegment ? FREE_SEGMENT : 0);

    /* if others are holding a reference, preserve the wrapper but do free/return the data */
    if (!c_atomic_ref_count_dec (&array->refCount)) {
        flags |= PRESERVE_WRAPPER;
    }

    return array_free (array, flags);

}

CArray* c_array_ref (CArray* array)
{
    c_return_val_if_fail (array != NULL, NULL);

    CRealArray* rarray = (CRealArray*) array;

    c_atomic_ref_count_inc (&rarray->refCount);

    return array;
}

void c_array_unref (CArray* array)
{
    CRealArray* rarray = (CRealArray*) array;
    c_return_if_fail (array);

    if (c_atomic_ref_count_dec (&rarray->refCount)) {
        array_free (rarray, FREE_SEGMENT);
    }
}

cuint c_array_get_element_size (CArray* array)
{
    CRealArray *rarray = (CRealArray*) array;

    c_return_val_if_fail (array, 0);

    return rarray->eltSize;
}

CArray* c_array_append_vals (CArray* farray, const void* data, cuint len)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_val_if_fail (array, NULL);

    if (len == 0) {
        return farray;
    }

    c_array_maybe_expand (array, len);

    memcpy (c_array_elt_pos (array, array->len), data, c_array_elt_len (array, len));

    array->len += len;

    c_array_zero_terminate (array);

    return farray;
}

CArray* c_array_prepend_vals (CArray* farray, const void* data, cuint len)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_val_if_fail (array, NULL);

    if (len == 0) {
        return farray;
    }

    c_array_maybe_expand (array, len);

    memmove (c_array_elt_pos (array, len), c_array_elt_pos (array, 0), c_array_elt_len (array, array->len));

    memcpy (c_array_elt_pos (array, 0), data, c_array_elt_len (array, len));

    array->len += len;

    c_array_zero_terminate (array);

    return farray;
}

CArray* c_array_insert_vals (CArray* farray, cuint index, const void* data, cuint len)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_val_if_fail (array, NULL);

    if (len == 0) {
        return farray;
    }

    if (index >= array->len) {
        c_array_maybe_expand (array, index - array->len + len);
        return c_array_append_vals (c_array_set_size (farray, index), data, len);
    }

    c_array_maybe_expand (array, len);

    memmove (c_array_elt_pos (array, len + index), c_array_elt_pos (array, index), c_array_elt_len (array, array->len - index));

    memcpy (c_array_elt_pos (array, index), data, c_array_elt_len (array, len));

    array->len += len;

    c_array_zero_terminate (array);

    return farray;
}

CArray* c_array_set_size (CArray* farray, cuint length)
{
    CRealArray *array = (CRealArray*) farray;

    c_return_val_if_fail (array, NULL);

    if (length > array->len) {
        c_array_maybe_expand (array, length - array->len);

        if (array->clear) {
            c_array_elt_zero (array, array->len, length - array->len);
        }
    }
    else if (length < array->len) {
        c_array_remove_range (farray, length, array->len - length);
    }

    array->len = length;

    c_array_zero_terminate (array);

    return farray;
}

CArray* c_array_remove_index (CArray* farray, cuint index)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_val_if_fail (array, NULL);

    c_return_val_if_fail (index < array->len, NULL);

    if (array->clearFunc != NULL) {
        array->clearFunc (c_array_elt_pos (array, index));
    }

    if (index != array->len - 1) {
        memmove (c_array_elt_pos (array, index), c_array_elt_pos (array, index + 1), c_array_elt_len (array, array->len - index - 1));
    }

    array->len -= 1;

    c_array_zero_terminate (array);

    return farray;
}

CArray* c_array_remove_index_fast (CArray* farray, cuint index)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_val_if_fail (array, NULL);
    c_return_val_if_fail (index < array->len, NULL);

    if (array->clearFunc != NULL) {
        array->clearFunc (c_array_elt_pos (array, index));
    }

    if (index != array->len - 1) {
        memcpy (c_array_elt_pos (array, index), c_array_elt_pos (array, array->len - 1), c_array_elt_len (array, 1));
    }

    array->len -= 1;

    c_array_zero_terminate (array);

    return farray;

}

CArray* c_array_remove_range (CArray* farray, cuint index, cuint length)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_val_if_fail (array, NULL);
    c_return_val_if_fail (index <= array->len, NULL);
    c_return_val_if_fail (index + length <= array->len, NULL);

    if (array->clearFunc != NULL) {
        cuint i;
        for (i = 0; i < length; i++) {
            array->clearFunc (c_array_elt_pos (array, index + i));
        }
    }

    if (index + length != array->len) {
        memmove (c_array_elt_pos (array, index), c_array_elt_pos (array, index + length), (array->len - (index + length)) * array->eltSize);
    }

    array->len -= length;
    c_array_zero_terminate (array);

    return farray;

}

void c_array_sort (CArray* farray, CCompareFunc compareFunc)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_if_fail (array != NULL);

    if (array->len > 0) {
        c_qsort_with_data (array->data, array->len, array->eltSize, (CCompareDataFunc)compareFunc, NULL);
    }
}

void c_array_sort_with_data (CArray* farray, CCompareDataFunc compareFunc, void* udata)
{
    CRealArray* array = (CRealArray*) farray;

    c_return_if_fail (array != NULL);

    if (array->len > 0) {
        c_qsort_with_data (array->data, array->len, array->eltSize, compareFunc, udata);
    }
}

bool c_array_binary_search (CArray* array, const void* target, CCompareFunc compareFunc, cuint* outMatchIndex)
{
    bool result = false;
    CRealArray* _array = (CRealArray *) array;
    cuint left, middle = 0, right;
    cint val;

    c_return_val_if_fail (_array != NULL, false);
    c_return_val_if_fail (compareFunc != NULL, false);

    if (C_LIKELY(_array->len)) {
        left = 0;
        right = _array->len - 1;
        while (left <= right) {
            middle = left + (right - left) / 2;
            val = compareFunc (_array->data + (_array->eltSize * middle), (void*) target);
            if (val == 0) {
                result = true;
                break;
            }
            else if (val < 0) {
                left = middle + 1;
            }
            else if (middle > 0) {
                right = middle - 1;
            }
            else {
                break;  /* element not found */
            }
        }
    }

    if (result && outMatchIndex != NULL) {
        *outMatchIndex = middle;
    }

    return result;

}

void c_array_set_clear_func (CArray* array, CDestroyNotify clearFunc)
{
    CRealArray* rarray = (CRealArray*) array;

    c_return_if_fail (array != NULL);

    rarray->clearFunc = clearFunc;
}

CPtrArray* c_ptr_array_new (void)
{
    return ptr_array_new (0, NULL, true);
}

CPtrArray* c_ptr_array_new_with_free_func (CDestroyNotify elementFreeFunc)
{
    return ptr_array_new (0, elementFreeFunc, true);
}

void** c_ptr_array_steal (CPtrArray* array, cuint64* len)
{
    void** segment = NULL;
    CRealPtrArray* rarray = NULL;

    c_return_val_if_fail (array != NULL, NULL);

    rarray = (CRealPtrArray *) array;
    segment = (void**) rarray->pdata;

    if (len != NULL) {
        *len = rarray->len;
    }

    rarray->pdata = NULL;
    rarray->len   = 0;
    rarray->alloc = 0;

    return segment;
}

CPtrArray* c_ptr_array_copy (CPtrArray* array, CCopyFunc func, void* udata)
{
    CRealPtrArray* rarray = (CRealPtrArray *) array;
    CPtrArray* newArray = NULL;

    c_return_val_if_fail (array != NULL, NULL);

    newArray = ptr_array_new (0, rarray->elementFreeFunc, rarray->nullTerminated);

    if (rarray->alloc > 0) {
        c_ptr_array_maybe_expand ((CRealPtrArray*) newArray, array->len + rarray->nullTerminated);

        if (array->len > 0) {
            if (func != NULL) {
                cuint i;
                for (i = 0; i < array->len; i++) {
                    ((CRealPtrArray*)(newArray))->pdata[i] = func (rarray->pdata[i], udata);
                }
            }
            else {
                memcpy (newArray->pdata, array->pdata, array->len * sizeof (*array->pdata));
            }
            newArray->len = array->len;
        }

        ptr_array_maybe_null_terminate ((CRealPtrArray*) newArray);
    }

    return newArray;
}

CPtrArray* c_ptr_array_sized_new (cuint reservedSize)
{
    return ptr_array_new (reservedSize, NULL, false);
}

CPtrArray* c_ptr_array_new_full (cuint reservedSize, CDestroyNotify elementFreeFunc)
{
    return ptr_array_new (reservedSize, elementFreeFunc, true);
}

CPtrArray* c_ptr_array_new_null_terminated (cuint reservedSize, CDestroyNotify elementFreeFunc, bool nullTerminated)
{
    return ptr_array_new (reservedSize, elementFreeFunc, nullTerminated);
}

void** c_ptr_array_free (CPtrArray* array, bool freeSeg)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;
    ArrayFreeFlags flags;

    c_return_val_if_fail (rarray, NULL);

    flags = (freeSeg ? FREE_SEGMENT : 0);

    return ptr_array_free (array, flags);
}

CPtrArray* c_ptr_array_ref (CPtrArray* array)
{
    CRealPtrArray* rarray = (CRealPtrArray*)array;

    c_return_val_if_fail(array, NULL);

    c_atomic_ref_count_inc (&rarray->refCount);

    return array;
}

void c_ptr_array_unref (CPtrArray* array)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;

    c_return_if_fail (array);

    if (c_atomic_ref_count_dec (&rarray->refCount)) {
        ptr_array_free (array, FREE_SEGMENT);
    }
}

void c_ptr_array_set_free_func (CPtrArray* array, CDestroyNotify elementFreeFunc)
{}

void c_ptr_array_set_size (CPtrArray* array, cint length)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;
    cuint lengthUnsigned;

    c_return_if_fail (rarray);
    c_return_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL));
    c_return_if_fail (length >= 0);

    lengthUnsigned = (cuint) length;

    if (lengthUnsigned > rarray->len) {
        cuint i;
        if (C_UNLIKELY (rarray->nullTerminated) && lengthUnsigned - rarray->len > C_MAX_UINT - 1) {
//            g_error ("array would overflow");
            c_assert(false);
        }

        c_ptr_array_maybe_expand (rarray, (lengthUnsigned - rarray->len) + rarray->nullTerminated);

        for (i = rarray->len; i < lengthUnsigned; i++) {
            rarray->pdata[i] = NULL;
        }
        rarray->len = lengthUnsigned;
        ptr_array_maybe_null_terminate (rarray);
    }
    else if (lengthUnsigned < rarray->len) {
        c_ptr_array_remove_range (array, lengthUnsigned, rarray->len - lengthUnsigned);
    }
}

void* c_ptr_array_remove_index (CPtrArray* array, cuint index)
{
    return ptr_array_remove_index (array, index, false, true);
}

void* c_ptr_array_remove_index_fast (CPtrArray* array, cuint index)
{
    return ptr_array_remove_index (array, index, true, true);
}

void* c_ptr_array_steal_index (CPtrArray* array, cuint index)
{
    return ptr_array_remove_index (array, index, false, false);
}

void* c_ptr_array_steal_index_fast (CPtrArray* array, cuint index)
{
    return ptr_array_remove_index (array, index, true, false);
}

bool c_ptr_array_remove (CPtrArray* array, void* data)
{
    cuint i;

    c_return_val_if_fail (array, false);
    c_return_val_if_fail (array->len == 0 || (array->len != 0 && array->pdata != NULL), false);

    CRealPtrArray* ptrArray = (CRealPtrArray*) array;

    for (i = 0; i < array->len; i += 1) {
        if (ptrArray->pdata[i] == data) {
            c_ptr_array_remove_index (array, i);
            return true;
        }
    }

    return false;
}

bool c_ptr_array_remove_fast (CPtrArray* array, void* data)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;
    cuint i;

    c_return_val_if_fail (rarray, false);
    c_return_val_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL), false);

    for (i = 0; i < rarray->len; i += 1) {
        if (rarray->pdata[i] == data) {
            c_ptr_array_remove_index_fast (array, i);
            return true;
        }
    }

    return false;
}

CPtrArray* c_ptr_array_remove_range (CPtrArray* array, cuint index, cuint length)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;
    cuint i;

    c_return_val_if_fail ((rarray != NULL) && rarray->pdata, NULL);
    c_return_val_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL), NULL);
    c_return_val_if_fail (index <= rarray->len, NULL);
    c_return_val_if_fail (length == 0 || index + length <= rarray->len, NULL);

    if (length == 0) {
        return array;
    }

    if (rarray->elementFreeFunc != NULL) {
        for (i = index; i < index + length; i++) {
            rarray->elementFreeFunc (rarray->pdata[i]);
        }
    }

    if (index + length != rarray->len) {
        memmove (&rarray->pdata[index], &rarray->pdata[index + length], (rarray->len - (index + length)) * sizeof (void*));
    }

    rarray->len -= length;
    ptr_array_maybe_null_terminate (rarray);

    return array;
}

void c_ptr_array_add (CPtrArray* array, void* data)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;

    c_return_if_fail (rarray);
    c_return_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL));

    c_ptr_array_maybe_expand (rarray, 1u + rarray->nullTerminated);

    rarray->pdata[rarray->len++] = data;

    ptr_array_maybe_null_terminate (rarray);
}

void c_ptr_array_extend (CPtrArray* arrayToExtend, CPtrArray* array, CCopyFunc func, void* udata)
{
    CRealPtrArray *rarrayToExtend = (CRealPtrArray*) arrayToExtend;

    c_return_if_fail (arrayToExtend != NULL);
    c_return_if_fail (array != NULL);

    if (array->len == 0u) {
        return;
    }

    if (C_UNLIKELY (array->len == C_MAX_UINT) && rarrayToExtend->nullTerminated) {
        c_assert(false);
    }

    c_ptr_array_maybe_expand (rarrayToExtend, array->len + rarrayToExtend->nullTerminated);

    if (func != NULL) {
        cuint i;
        for (i = 0; i < array->len; i++) {
            rarrayToExtend->pdata[i + rarrayToExtend->len] = func (rarrayToExtend->pdata[i], udata);
        }
    }
    else if (array->len > 0) {
        memcpy (rarrayToExtend->pdata + rarrayToExtend->len, array->pdata, array->len * sizeof (*array->pdata));
    }

    rarrayToExtend->len += array->len;

    ptr_array_maybe_null_terminate (rarrayToExtend);
}

void c_ptr_array_extend_and_steal (CPtrArray* arrayToExtend, CPtrArray* array)
{
    c_ptr_array_extend (arrayToExtend, array, NULL, NULL);

    void** pdata = c_steal_pointer (&array->pdata);
    array->len = 0;
    ((CRealPtrArray*) array)->alloc = 0;
    c_ptr_array_unref (array);
    c_free (pdata);
}

void c_ptr_array_insert (CPtrArray* array, cuint index, void* data)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;

    c_return_if_fail (rarray);
    c_return_if_fail (index >= -1);
    c_return_if_fail (index <= (cint)rarray->len);

    c_ptr_array_maybe_expand (rarray, 1u + rarray->nullTerminated);

    if (index < 0) {
        index = rarray->len;
    }

    if ((cuint) index < rarray->len) {
        memmove (&(rarray->pdata[index + 1]), &(rarray->pdata[index]), (rarray->len - index) * sizeof (void*));
    }

    rarray->len++;
    rarray->pdata[index] = data;

    ptr_array_maybe_null_terminate (rarray);
}

void c_ptr_array_sort (CPtrArray* array, CCompareFunc compareFunc)
{
    c_return_if_fail (array != NULL);

    if (array->len > 0) {
        c_qsort_with_data (array->pdata, array->len, sizeof (void*), (CCompareDataFunc)compareFunc, NULL);
    }
}

void c_ptr_array_sort_with_data (CPtrArray* array, CCompareDataFunc compareFunc, void* udata)
{
    c_return_if_fail (array != NULL);

    if (array->len > 0) {
        c_qsort_with_data (array->pdata, array->len, sizeof (void*), (CCompareDataFunc)compareFunc, udata);
    }
}

void c_ptr_array_foreach (CPtrArray* array, CFunc func, void* udata)
{
    c_return_if_fail (array);

    cuint i;
    for (i = 0; i < array->len; i++) {
        (*func) ((((CRealPtrArray*)array))->pdata[i], udata);
    }
}

bool c_ptr_array_find (CPtrArray* haystack, const void* needle, cuint* index)
{
    return c_ptr_array_find_with_equal_func (haystack, needle, NULL, index);
}

bool c_ptr_array_find_with_equal_func (CPtrArray* haystack, const void* needle, CEqualFunc equalFunc, cuint* index)
{
    c_return_val_if_fail (haystack != NULL, false);

    cuint i;
    if (equalFunc == NULL) {
        equalFunc = c_direct_equal;
    }

    for (i = 0; i < haystack->len; i++) {
        if (equalFunc (c_ptr_array_index ((CRealPtrArray*) haystack, i), needle)) {
            if (index != NULL) {
                *index = i;
            }
            return true;
        }
    }

    return false;
}

bool c_ptr_array_is_null_terminated (CPtrArray* array)
{}


CByteArray* c_byte_array_new (void)
{
    return (CByteArray*) c_array_sized_new (false, false, 1, 0);
}

CByteArray* c_byte_array_new_take (cuint8* data, cuint64 len)
{
    c_return_val_if_fail (len <= C_MAX_UINT, NULL);

    CByteArray* array = c_byte_array_new ();
    CRealArray* real = (CRealArray*) array;

    c_assert (real->data == NULL);
    c_assert (real->len == 0);

    real->data = data;
    real->len = len;
    real->eltCapacity = len;

    return array;
}

cuint8* c_byte_array_steal (CByteArray* array, cuint64* len)
{
    return (cuint8*) c_array_steal ((CArray*) array, len);
}

CByteArray* c_byte_array_sized_new (cuint reservedSize)
{
    return (CByteArray*) c_array_sized_new (false, false, 1, reservedSize);
}

cuint8* c_byte_array_free (CByteArray* array, bool freeSegment)
{
    return (cuint8*) c_array_free ((CArray*) array, freeSegment);
}

CBytes* c_byte_array_free_to_bytes (CByteArray* array)
{
    c_return_val_if_fail (array != NULL, NULL);

    csize length = array->len;

    return c_bytes_new_take (c_byte_array_free (array, false), length);
}

CByteArray* c_byte_array_ref (CByteArray* array)
{
    return (CByteArray*) c_array_ref ((CArray*) array);
}

void c_byte_array_unref (CByteArray* array)
{
    c_array_unref ((CArray*) array);
}

CByteArray* c_byte_array_append (CByteArray* array, const cuint8* data, cuint len)
{
    c_array_append_vals ((CArray*) array, (cuint8*) data, len);

    return array;
}

CByteArray* c_byte_array_prepend (CByteArray* array, const cuint8* data, cuint len)
{
    c_array_prepend_vals ((CArray*) array, (cuint8*) data, len);

    return array;
}

CByteArray* c_byte_array_set_size (CByteArray* array, cuint length)
{
    c_array_set_size ((CArray*) array, length);

    return array;
}

CByteArray* c_byte_array_remove_index (CByteArray* array, cuint index)
{
    c_array_remove_index ((CArray*) array, index);

    return array;
}

CByteArray* c_byte_array_remove_index_fast (CByteArray* array, cuint index)
{
    c_array_remove_index_fast ((CArray*) array, index);

    return array;
}

CByteArray* c_byte_array_remove_range (CByteArray* array, cuint index, cuint length)
{
    c_return_val_if_fail (array, NULL);
    c_return_val_if_fail (index <= array->len, NULL);
    c_return_val_if_fail (index + length <= array->len, NULL);

    return (CByteArray*) c_array_remove_range ((CArray*)array, index, length);
}

void c_byte_array_sort (CByteArray* array, CCompareFunc compareFunc)
{
    c_array_sort ((CArray*) array, compareFunc);
}

void c_byte_array_sort_with_data (CByteArray* array, CCompareDataFunc compareFunc, void* udata)
{
    c_array_sort_with_data ((CArray*) array, compareFunc, udata);
}


static cchar* array_free (CRealArray* array, ArrayFreeFlags flags)
{
    c_return_val_if_fail(array, NULL);

    cchar *segment;

    if (flags & FREE_SEGMENT) {
        if (array->clearFunc != NULL) {
            cuint i;
            for (i = 0; i < array->len; i++) {
                array->clearFunc (c_array_elt_pos (array, i));
            }
        }

        c_free (array->data);
        segment = NULL;
    }
    else {
        segment = (cchar*) array->data;
    }

    if (flags & PRESERVE_WRAPPER) {
        array->data            = NULL;
        array->len             = 0;
        array->eltCapacity     = 0;
    }
    else {
        c_free(array);
    }

    return segment;
}

static void c_array_maybe_expand (CRealArray* array, cuint len)
{
    cuint maxLen, wantLen;

    /**
     * The maximum array length is derived from following constraints:
     * - The number of bytes must fit into a gsize / 2.
     * - The number of elements must fit into guint.
     * - zero terminated arrays must leave space for the terminating element
     */
    maxLen = C_MIN (C_MAX_SIZE / 2 / array->eltSize, C_MAX_UINT) - array->zeroTerminated;

    /* Detect potential overflow */
    if C_UNLIKELY ((maxLen - array->len) < len) {
        fprintf(stderr, "adding %u to array would overflow", len);
        c_assert(false);
    }

    wantLen = array->len + len + array->zeroTerminated;
    if (wantLen > array->eltCapacity) {

        csize wantAlloc = c_nearest_pow (c_array_elt_len (array, wantLen));
        wantAlloc = C_MAX (wantAlloc, MIN_ARRAY_SIZE);

        array->data = c_realloc (array->data, wantAlloc);

        memset (c_array_elt_pos (array, array->eltCapacity), 0, c_array_elt_len (array, wantLen - array->eltCapacity));

        array->eltCapacity = C_MIN (wantAlloc / array->eltSize, C_MAX_UINT);
    }
}

static void ptr_array_maybe_null_terminate (CRealPtrArray* rarray)
{
    if (C_UNLIKELY (rarray->nullTerminated)) {
        rarray->pdata[rarray->len] = NULL;
    }
}

static CPtrArray* ptr_array_new (cuint reservedSize, CDestroyNotify elementFreeFunc, bool nullTerminated)
{
    CRealPtrArray* array = c_malloc0(sizeof (CRealPtrArray));

    array->pdata = NULL;
    array->len = 0;
    array->alloc = 0;
    array->nullTerminated = nullTerminated ? 1 : 0;
    array->elementFreeFunc = elementFreeFunc;

    c_atomic_ref_count_init (&array->refCount);

    if (reservedSize != 0) {
        if (C_LIKELY (reservedSize < C_MAX_UINT) && nullTerminated) {
            reservedSize++;
        }

        c_ptr_array_maybe_expand (array, reservedSize);
        c_assert (array->pdata != NULL);

        if (nullTerminated) {
            /* don't use ptr_array_maybe_null_terminate(). It helps the compiler
             * to see when @null_terminated is false and thereby inline
             * ptr_array_new() and possibly remove the code entirely. */
            array->pdata[0] = NULL;
        }
    }

    return (CPtrArray*) array;
}

static void** ptr_array_free (CPtrArray* array, ArrayFreeFlags flags)
{
    CRealPtrArray *rarray = (CRealPtrArray*) array;

    c_return_val_if_fail(array, NULL);

    void** segment = NULL;

    if (flags & FREE_SEGMENT) {
        void** stolenPdata = c_steal_pointer (&rarray->pdata);
        if (rarray->elementFreeFunc != NULL) {
            cuint i;
            for (i = 0; i < rarray->len; ++i) {
                rarray->elementFreeFunc (stolenPdata[i]);
            }
        }

        c_free (stolenPdata);
        segment = NULL;
    }
    else {
        segment = rarray->pdata;
        if (!segment && rarray->nullTerminated) {
            segment = (void**) c_malloc0(sizeof(void*));
        }
    }

    if (flags & PRESERVE_WRAPPER) {
        rarray->pdata = NULL;
        rarray->len = 0;
        rarray->alloc = 0;
    }
    else {
        c_free(rarray);
    }

    return segment;
}

static void c_ptr_array_maybe_expand (CRealPtrArray* array, cuint len)
{
    cuint maxLen = (cuint) C_MIN (C_MAX_SIZE / 2 / sizeof (void*), C_MAX_UINT);

    if C_UNLIKELY ((maxLen - array->len) < len) {
        assert(false);
    }

    if ((array->len + len) > array->alloc) {
        cuint oldAlloc = array->alloc;
        csize wantAlloc = c_nearest_pow (sizeof (void*) * (array->len + len));
        wantAlloc = C_MAX(wantAlloc, MIN_ARRAY_SIZE);
        array->alloc = C_MIN (wantAlloc / sizeof (void*), C_MAX_UINT);
        array->pdata = c_realloc (array->pdata, wantAlloc);
        for ( ; oldAlloc < array->alloc; oldAlloc++) {
            array->pdata [oldAlloc] = NULL;
        }
    }
}

static void* ptr_array_remove_index (CPtrArray* array, cuint index, bool fast, bool freeElement)
{
    CRealPtrArray* rarray = (CRealPtrArray*) array;
    void* result = NULL;

    c_return_val_if_fail (rarray && rarray->pdata, NULL);
    c_return_val_if_fail (rarray->len == 0 || (rarray->len != 0 && rarray->pdata != NULL), NULL);
    c_return_val_if_fail (index < rarray->len, NULL);

    result = rarray->pdata[index];

    if (rarray->elementFreeFunc != NULL && freeElement) {
        rarray->elementFreeFunc (rarray->pdata[index]);
    }

    if (index != rarray->len - 1 && !fast) {
        memmove (rarray->pdata + index, rarray->pdata + index + 1, sizeof (void*) * (rarray->len - index - 1));
    }
    else if (index != rarray->len - 1) {
        rarray->pdata[index] = rarray->pdata[rarray->len - 1];
    }

    rarray->len -= 1;

    if (rarray->nullTerminated) {
        rarray->pdata[rarray->len] = NULL;
    }

    return result;
}
