// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 6/9/24.
//

#ifndef clibrary_RCBOX_H
#define clibrary_RCBOX_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

C_BEGIN_EXTERN_C
void*       c_rc_box_alloc                  (csize blockSize) C_MALLOC C_ALLOC_SIZE(1);
void*       c_rc_box_alloc0                 (csize blockSize) C_MALLOC C_ALLOC_SIZE(1);
void*       c_rc_box_dup                    (csize blockSize, const void* memBlock) C_ALLOC_SIZE(1);
void*       c_rc_box_acquire                (void* memBlock);
void        c_rc_box_release                (void* memBlock);
void        c_rc_box_release_full           (void* memBlock, CDestroyNotify clearFunc);
csize       c_rc_box_get_size               (void* memBlock);
void*       c_atomic_rc_box_alloc           (csize blockSize) C_MALLOC C_ALLOC_SIZE(1);
void*       c_atomic_rc_box_alloc0          (csize blockSize) C_MALLOC C_ALLOC_SIZE(1);
void*       c_atomic_rc_box_dup             (csize blockSize, const void* memBlock) C_ALLOC_SIZE(1);
void*       c_atomic_rc_box_acquire         (void* memBlock);
void        c_atomic_rc_box_release         (void* memBlock);
void        c_atomic_rc_box_release_full    (void* memBlock, CDestroyNotify clearFunc);
csize       c_atomic_rc_box_get_size        (void* memBlock);

#define c_rc_box_new(type) \
((type *) c_rc_box_alloc (sizeof (type)))
#define c_rc_box_new0(type) \
((type *) c_rc_box_alloc0 (sizeof (type)))
#define c_atomic_rc_box_new(type) \
((type *) c_atomic_rc_box_alloc (sizeof (type)))
#define c_atomic_rc_box_new0(type) \
((type *) c_atomic_rc_box_alloc0 (sizeof (type)))

#if defined(clib_typeof)
/* Type check to avoid assigning references to different types */
#define c_rc_box_acquire(memBlock) \
((clib_typeof (memBlock)) (c_rc_box_acquire) (memBlock))
#define c_atomic_rc_box_acquire(memBlock) \
((clib_typeof (memBlock)) (c_atomic_rc_box_acquire) (memBlock))

/* Type check to avoid duplicating data to different types */
#define c_rc_box_dup(blockSize, memBlock) \
((clib_typeof (memBlock)) (c_rc_box_dup) (blockSize, memBlock))
#define c_atomic_rc_box_dup(blockSize, memBlock) \
((clib_typeof (memBlock)) (c_atomic_rc_box_dup) (blockSize, memBlock))
#endif

extern void* c_rc_box_alloc_full(csize block_size, csize alignment, bool atomic, bool clear);

C_END_EXTERN_C

#endif // clibrary_RCBOX_H
