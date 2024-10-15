
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-18.
//

#include "bytes.h"
#include "atomic.h"

struct _CBytes
{
    const void*         data;  /* may be NULL iff (size == 0) */
    csize               size;  /* may be 0 */
    catomicrefcount     refCount;
    CDestroyNotify      freeFunc;
    void*               udata;
};


static void* try_steal_and_unref (CBytes* bytes, CDestroyNotify freeFunc, csize* size);


CBytes* c_bytes_new (const void* data, csize size)
{
    c_return_val_if_fail (data || 0 == size, NULL);

    return c_bytes_new_take (c_memdup(data, size), size);
}

CBytes* c_bytes_new_take (void* data, csize size)
{
    return c_bytes_new_with_free_func (data, size, c_free0, data);
}

CBytes* c_bytes_new_static (const void* data, csize size)
{
    return c_bytes_new_with_free_func (data, size, NULL, NULL);
}

CBytes* c_bytes_new_with_free_func (const void* data, csize size, CDestroyNotify freeFunc, void* udata)
{
    c_return_val_if_fail (data || 0 == size, NULL);

    CBytes* bytes = c_malloc0(sizeof(CBytes));
    bytes->data = data;
    bytes->size = size;
    bytes->freeFunc = freeFunc;
    bytes->udata = udata;
    c_atomic_ref_count_init (&bytes->refCount);

    return (CBytes*) bytes;
}

CBytes* c_bytes_new_from_bytes (CBytes* bytes, csize offset, csize length)
{
    c_return_val_if_fail (bytes != NULL, NULL);
    c_return_val_if_fail (offset <= bytes->size, NULL);
    c_return_val_if_fail (offset + length <= bytes->size, NULL);

    if (offset == 0 && length == bytes->size) {
        return c_bytes_ref (bytes);
    }

    char* base = (char*)bytes->data + offset;

    while (bytes->freeFunc == (void*) c_bytes_unref) {
        bytes = bytes->udata;
    }

    c_return_val_if_fail (bytes != NULL, NULL);
    c_return_val_if_fail (base >= (char*)bytes->data, NULL);
    c_return_val_if_fail (base <= (char*)bytes->data + bytes->size, NULL);
    c_return_val_if_fail (base + length <= (char*)bytes->data + bytes->size, NULL);

    return c_bytes_new_with_free_func (base, length, (CDestroyNotify) c_bytes_unref, c_bytes_ref (bytes));
}

const void* c_bytes_get_data (CBytes* bytes, csize* size)
{
    c_return_val_if_fail (bytes != NULL, NULL);

    if (size) {
        *size = bytes->size;
    }

    return bytes->data;
}

csize c_bytes_get_size (CBytes* bytes)
{
    c_return_val_if_fail (bytes != NULL, 0);

    return bytes->size;
}

CBytes* c_bytes_ref (CBytes* bytes)
{
    c_return_val_if_fail (bytes != NULL, NULL);

    c_atomic_ref_count_inc (&bytes->refCount);

    return bytes;
}

void c_bytes_unref (CBytes* bytes)
{
    c_return_if_fail(bytes);

    if (c_atomic_ref_count_dec (&bytes->refCount)) {
        if (bytes->freeFunc != NULL) {
            bytes->freeFunc (bytes->udata);
        }
        c_free(bytes);
    }
}

void* c_bytes_unref_to_data (CBytes* bytes, csize* size)
{
    c_return_val_if_fail (bytes, NULL);
    c_return_val_if_fail (size, NULL);

    void* result = try_steal_and_unref (bytes, c_free0, size);
    if (result == NULL) {
        result = c_memdup (bytes->data, bytes->size);
        *size = bytes->size;
        c_bytes_unref (bytes);
    }

    return result;
}

CByteArray* c_bytes_unref_to_array (CBytes* bytes)
{
    void* data = NULL;
    csize size;

    c_return_val_if_fail (bytes != NULL, NULL);

    data = c_bytes_unref_to_data (bytes, &size);

    return c_byte_array_new_take (data, size);
}

cuint c_bytes_hash (const CBytes* bytes)
{
    const CBytes *a = bytes;
    const signed char *p, *e;
    cuint32 h = 5381;

    c_return_val_if_fail (bytes, 0);

    for (p = (signed char *)a->data, e = (signed char *)a->data + a->size; p != e; p++) {
        h = (h << 5) + h + *p;
    }

    return h;
}

bool c_bytes_equal (const CBytes* bytes1, const CBytes* bytes2)
{
    const CBytes *b1 = bytes1;
    const CBytes *b2 = bytes2;

    c_return_val_if_fail (bytes1 != NULL, false);
    c_return_val_if_fail (bytes2 != NULL, false);

    return (b1->size == b2->size) && ((b1->size == 0) || memcmp (b1->data, b2->data, b1->size) == 0);
}

cint c_bytes_compare (const CBytes* bytes1, const CBytes* bytes2)
{
    const CBytes *b1 = bytes1;
    const CBytes *b2 = bytes2;
    cint ret;

    c_return_val_if_fail (bytes1 != NULL, 0);
    c_return_val_if_fail (bytes2 != NULL, 0);

    ret = memcmp (b1->data, b2->data, C_MIN (b1->size, b2->size));
    if (ret == 0 && b1->size != b2->size) {
        ret = b1->size < b2->size ? -1 : 1;
    }

    return ret;
}

const void* c_bytes_get_region (CBytes* bytes, csize elementSize, csize offset, csize nElements)
{
    csize totalSize = 0;
    csize endOffset = 0;

    c_return_val_if_fail (elementSize > 0, NULL);

    // 获取元素长度
    if (!c_size_checked_mul (&totalSize, elementSize, nElements)) {
        return NULL;
    }

    //
    if (!c_size_checked_add (&endOffset, offset, totalSize)) {
        return NULL;
    }

    /* We now have:
     *
     *   0 <= offset <= end_offset
     *
     * So we need only check that end_offset is within the range of the
     * size of @bytes and we're good to go.
     */

    if (endOffset > bytes->size) {
        return NULL;
    }

    return ((cuchar*) bytes->data) + offset;
}


static void* try_steal_and_unref (CBytes* bytes, CDestroyNotify freeFunc, csize* size)
{
    void* result;

    c_return_val_if_fail(bytes, NULL);

    if (bytes->freeFunc != freeFunc || bytes->data == NULL || bytes->udata != bytes->data) {
        return NULL;
    }

    if (c_atomic_ref_count_compare (&bytes->refCount, 1)) {
        *size = bytes->size;
        result = (void*) bytes->data;
        return result;
    }

    return NULL;
}
