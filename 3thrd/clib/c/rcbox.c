// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 6/9/24.
//

#include "rcbox.h"

#include "clib.h"


/* We use the same alignment as GTypeInstance and GNU libc's malloc */
#define C_BOX_MAGIC             0x44ae2bf0
#define ALIGN_STRUCT(offset)    ((offset + (STRUCT_ALIGNMENT - 1)) & -STRUCT_ALIGNMENT)
#define C_RC_BOX(p)             (CRcBox *) (((char *) (p)) - C_RC_BOX_SIZE)
#define STRUCT_ALIGNMENT        (2 * sizeof (csize))
#define C_RC_BOX_SIZE           sizeof (CRcBox)
#define C_ARC_BOX_SIZE          sizeof (CArcBox)
#define C_ARC_BOX(p)            (CArcBox*) (((char*) (p)) - C_ARC_BOX_SIZE)

typedef struct _CRcBox      CRcBox;
typedef struct _CArcBox     CArcBox;

struct _CRcBox
{
    crefcount       refCount;
    csize           memSize;
    csize           privateOffset;
    cuint32         magic;
};

struct _CArcBox
{
    catomicrefcount refCount;
    csize           memSize;
    csize           privateOffset;
    cuint32         magic;
};

/* Keep the two refcounted boxes identical in size */
C_STATIC_ASSERT (sizeof (CRcBox) == sizeof (CArcBox));


void* c_rc_box_alloc_full(csize block_size, csize alignment, bool atomic, bool clear)
{
    /* We don't do an (atomic ? C_ARC_BOX_SIZE : C_RC_BOX_SIZE) check, here
     * because we have a static assertion that sizeof(GArcBox) == sizeof(GRcBox)
     * inside grcboxprivate.h, and we don't want the compiler to unnecessarily
     * warn about both branches of the conditional yielding identical results
     */
    csize private_size = C_ARC_BOX_SIZE;
    csize private_offset = 0;
    csize real_size;
    char * allocated;

    c_assert(alignment != 0);

    /* We need to ensure that the private data is aligned */
    if (private_size % alignment != 0) {
        private_offset = private_size % alignment;
        private_size += (alignment - private_offset);
    }

    c_assert(block_size < (C_MAX_SIZE - private_size));
    real_size = private_size + block_size;

    /* The real allocated size must be a multiple of @alignment, to
     * maintain the alignment of block_size
     */
    if (real_size % alignment != 0) {
        csize offset = real_size % alignment;
        c_assert(real_size < (C_MAX_SIZE - (alignment - offset)));
        real_size += (alignment - offset);
    }

    {
        if (clear)
            allocated = c_malloc0(real_size);
        else
            allocated = c_malloc0(real_size);
    }

    if (atomic) {
        /* We leave the alignment padding at the top of the allocation,
         * so we have an in memory layout of:
         *
         *  |[ offset ][ sizeof(GArcBox) ]||[ block_size ]|
         */
        CArcBox * real_box = (CArcBox*)(allocated + private_offset);
        /* Store the real size */
        real_box->memSize = block_size;
        /* Store the alignment offset, to be used when freeing the
         * allocated block
         */
        real_box->privateOffset = private_offset;
        real_box->magic = C_BOX_MAGIC;
        c_atomic_ref_count_init(&real_box->refCount);
    }
    else {
        /* We leave the alignment padding at the top of the allocation,
         * so we have an in memory layout of:
         *
         *  |[ offset ][ sizeof(GRcBox) ]||[ block_size ]|
         */
        CRcBox * real_box = (CRcBox*)(allocated + private_offset);
        /* Store the real size */
        real_box->memSize = block_size;
        /* Store the alignment offset, to be used when freeing the
         * allocated block
         */
        real_box->privateOffset = private_offset;
        real_box->magic = C_BOX_MAGIC;
        c_ref_count_init(&real_box->refCount);
    }

    return allocated + private_size;
}


void* c_rc_box_alloc(csize block_size)
{
    c_return_val_if_fail(block_size > 0, NULL);

    return c_rc_box_alloc_full(block_size, STRUCT_ALIGNMENT, false, false);
}


void* c_rc_box_alloc0(csize block_size)
{
    c_return_val_if_fail(block_size > 0, NULL);

    return c_rc_box_alloc_full(block_size, STRUCT_ALIGNMENT, false, true);
}

void* (c_rc_box_dup)(csize block_size, const void * mem_block)
{
    void * res;

    c_return_val_if_fail(block_size > 0, NULL);
    c_return_val_if_fail(mem_block != NULL, NULL);

    res = c_rc_box_alloc_full(block_size, STRUCT_ALIGNMENT, false, false);
    memcpy(res, mem_block, block_size);

    return res;
}

void* (c_rc_box_acquire)(void * mem_block)
{
    CRcBox * real_box = C_RC_BOX(mem_block);

    c_return_val_if_fail(mem_block != NULL, NULL);
    c_return_val_if_fail(real_box->magic == C_BOX_MAGIC, NULL);

    c_ref_count_inc(&real_box->refCount);

    return mem_block;
}

void c_rc_box_release(void * mem_block)
{
    c_rc_box_release_full(mem_block, NULL);
}

void c_rc_box_release_full(void * mem_block, CDestroyNotify clear_func)
{
    CRcBox * real_box = C_RC_BOX(mem_block);

    c_return_if_fail(mem_block != NULL);
    c_return_if_fail(real_box->magic == C_BOX_MAGIC);

    if (c_ref_count_dec(&real_box->refCount)) {
        char * real_mem = (char*)real_box - real_box->privateOffset;

        if (clear_func != NULL)
            clear_func(mem_block);

        c_free(real_mem);
    }
}

csize c_rc_box_get_size(void * mem_block)
{
    CRcBox * real_box = C_RC_BOX(mem_block);

    c_return_val_if_fail(mem_block != NULL, 0);
    c_return_val_if_fail(real_box->magic == C_BOX_MAGIC, 0);

    return real_box->memSize;
}

void* c_atomic_rc_box_alloc (csize block_size)
{
    c_return_val_if_fail (block_size > 0, NULL);

    return c_rc_box_alloc_full (block_size, STRUCT_ALIGNMENT, true, false);
}

void* c_atomic_rc_box_alloc0 (csize block_size)
{
    c_return_val_if_fail (block_size > 0, NULL);

    return c_rc_box_alloc_full (block_size, STRUCT_ALIGNMENT, true, true);
}

void c_atomic_rc_box_release_full (void* mem_block, CDestroyNotify clear_func)
{
    CArcBox *real_box = C_ARC_BOX (mem_block);

    c_return_if_fail (mem_block != NULL);
    c_return_if_fail (real_box->magic == C_BOX_MAGIC);

    if (c_atomic_ref_count_dec (&real_box->refCount))
    {
        char *real_mem = (char *) real_box - real_box->privateOffset;

        if (clear_func != NULL)
            clear_func (mem_block);

        c_free (real_mem);
    }
}

void* (c_atomic_rc_box_acquire) (void* mem_block)
{
    CArcBox *real_box = C_ARC_BOX (mem_block);

    c_return_val_if_fail (mem_block != NULL, NULL);
    c_return_val_if_fail (real_box->magic == C_BOX_MAGIC, NULL);

    c_atomic_ref_count_inc (&real_box->refCount);

    return mem_block;
}
