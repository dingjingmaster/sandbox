// Copyright (c) 2024. Lorem ipsum dolor sit amet, consectetur adipiscing elit.
// Morbi non lorem porttitor neque feugiat blandit. Ut vitae ipsum eget quam lacinia accumsan.
// Etiam sed turpis ac ipsum condimentum fringilla. Maecenas magna.
// Proin dapibus sapien vel ante. Aliquam erat volutpat. Pellentesque sagittis ligula eget metus.
// Vestibulum commodo. Ut rhoncus gravida arcu.

//
// Created by dingjing on 5/26/24.
//

#ifndef clibrary_ATOMIC_H
#define clibrary_ATOMIC_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

C_BEGIN_EXTERN_C

#define C_REF_COUNT_INIT            -1
#define C_ATOMIC_REF_COUNT_INIT     1

void            c_ref_count_init                (crefcount* rc);
void            c_ref_count_inc                 (crefcount* rc);
bool            c_ref_count_dec                 (crefcount* rc);
bool            c_ref_count_compare             (crefcount* rc, cint val);
void            c_atomic_ref_count_init         (catomicrefcount* arc);
void            c_atomic_ref_count_inc          (catomicrefcount* arc);
bool            c_atomic_ref_count_dec          (catomicrefcount* arc);
bool            c_atomic_ref_count_compare      (catomicrefcount* arc, cint val);

cint            c_atomic_bool_get                           (const volatile bool *atomic);
void            c_atomic_bool_set                           (volatile bool* atomic, bool newval);
bool            c_atomic_bool_compare_and_exchange          (volatile bool* atomic, bool oldval, cint newval);

cint            c_atomic_int_get                            (const volatile cint *atomic);
void            c_atomic_int_set                            (volatile cint* atomic, cint newval);
void            c_atomic_int_inc                            (volatile cint* atomic);
bool            c_atomic_int_dec_and_test                   (volatile cint* atomic);
bool            c_atomic_int_compare_and_exchange           (volatile cint* atomic, cint oldval, cint newval);
bool            c_atomic_int_compare_and_exchange_full      (cint* atomic, cint oldval, cint newval, cint* preval);
cint            c_atomic_int_exchange                       (cint* atomic, cint newval);
cint            c_atomic_int_add                            (volatile cint* atomic, cint val);
cuint           c_atomic_int_and                            (volatile cuint* atomic, cuint val);
cuint           c_atomic_int_or                             (volatile cuint* atomic, cuint val);
cuint           c_atomic_int_xor                            (volatile cuint* atomic, cuint val);

void*           c_atomic_pointer_get                        (const volatile void* atomic);
void            c_atomic_pointer_set                        (volatile void* atomic, void* newval);
bool            c_atomic_pointer_compare_and_exchange       (volatile void* atomic, void* oldval, void* newval);
bool            c_atomic_pointer_compare_and_exchange_full  (void* atomic, void* oldval, void* newval, void* preval);
void*           c_atomic_pointer_exchange                   (void* atomic, void* newval);
cintptr         c_atomic_pointer_add                        (volatile void* atomic, cssize val);
cuintptr        c_atomic_pointer_and                        (volatile void* atomic, csize val);
cuintptr        c_atomic_pointer_or                         (volatile void* atomic, csize val);
cuintptr        c_atomic_pointer_xor                        (volatile void* atomic, csize val);
cint            c_atomic_int_exchange_and_add               (volatile cint* atomic, cint val);

C_END_EXTERN_C

#endif // clibrary_ATOMIC_H
