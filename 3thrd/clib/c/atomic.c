
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*************************************************************************
> FileName: atomic.c
> Author  : DingJing
> Mail    : dingjing@live.cn
> Created Time: Thu 07 Sep 2022 22:20:19 PM CST
************************************************************************/

// #include "atomic.h"

#include "log.h"
#include "atomic.h"

#include <pthread.h>

static pthread_mutex_t gsAtomicLock = PTHREAD_MUTEX_INITIALIZER;

int c_atomic_int_get (const volatile cint *atomic)
{
    int value;

    pthread_mutex_lock (&gsAtomicLock);
    value = *atomic;
    pthread_mutex_unlock (&gsAtomicLock);

    return value;
}

void c_atomic_int_set (volatile int *atomic, int value)
{
    pthread_mutex_lock (&gsAtomicLock);
    *atomic = value;
    pthread_mutex_unlock (&gsAtomicLock);
}

void c_atomic_int_inc (volatile int *atomic)
{
    pthread_mutex_lock (&gsAtomicLock);
    (*atomic)++;
    pthread_mutex_unlock (&gsAtomicLock);
}

bool c_atomic_int_dec_and_test (volatile int *atomic)
{
    bool isZero;

    pthread_mutex_lock (&gsAtomicLock);
    isZero = (--(*atomic) == 0);
    pthread_mutex_unlock (&gsAtomicLock);

    return isZero;
}

bool c_atomic_int_compare_and_exchange (volatile int *atomic, int oldVal, int newVal)
{
    bool success;

    C_LOG_DEBUG_CONSOLE("lock -- lock -- lock 1");
    pthread_mutex_lock (&gsAtomicLock);
    C_LOG_DEBUG_CONSOLE("lock -- lock -- lock 2");

    if ((success = (*atomic == oldVal))) {
        *atomic = newVal;
    }

    C_LOG_DEBUG_CONSOLE("lock -- lock -- unlock 1");
    pthread_mutex_unlock (&gsAtomicLock);
    C_LOG_DEBUG_CONSOLE("lock -- lock -- unlock 2");

    return success;
}

bool c_atomic_int_compare_and_exchange_full (int *atomic, int oldVal, int newVal, int* preVal)
{
    bool     success;

    pthread_mutex_lock (&gsAtomicLock);

    *preVal = *atomic;

    if ((success = (*atomic == oldVal))) {
        *atomic = newVal;
    }

    pthread_mutex_unlock (&gsAtomicLock);

    return success;
}

int c_atomic_int_exchange (int *atomic, int newVal)
{
    int* ptr = atomic;
    int oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *ptr;
    *ptr = newVal;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

int c_atomic_int_add (volatile int *atomic, int val)
{
    int oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *atomic;
    *atomic = oldVal + val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cuint c_atomic_int_and (volatile cuint *atomic, cuint val)
{
    cuint oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *atomic;
    *atomic = oldVal & val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cuint c_atomic_int_or (volatile cuint *atomic, cuint val)
{
    cuint oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *atomic;
    *atomic = oldVal | val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cuint c_atomic_int_xor (volatile cuint *atomic, cuint val)
{
    cuint oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *atomic;
    *atomic = oldVal ^ val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

int c_atomic_int_exchange_and_add (volatile int *atomic, int val)
{
    return c_atomic_int_add ((cint*) atomic, val);
}

void* c_atomic_pointer_get (const volatile void* atomic)
{
    const void** ptr = (const void**) atomic;
    void* value;

    pthread_mutex_lock (&gsAtomicLock);
    value = (void*) *ptr;
    pthread_mutex_unlock (&gsAtomicLock);

    return value;
}


void c_atomic_pointer_set (volatile void* atomic, void* newVal)
{
    void** ptr = (void**) atomic;

    pthread_mutex_lock (&gsAtomicLock);
    *ptr = newVal;
    pthread_mutex_unlock (&gsAtomicLock);
}

bool c_atomic_pointer_compare_and_exchange (volatile void* atomic, void* oldVal, void* newVal)
{
    void** ptr = (void**) atomic;
    bool success;

    pthread_mutex_lock (&gsAtomicLock);
    if ((success = (*ptr == oldVal))) {
        *ptr = newVal;
    }
    pthread_mutex_unlock (&gsAtomicLock);

    return success;
}

bool c_atomic_pointer_compare_and_exchange_full (void* atomic, void* oldval, void* newval, void* preval)
{
    void** ptr = atomic;
    void** pre = preval;
    bool success;

    pthread_mutex_lock (&gsAtomicLock);

    *pre = *ptr;
    if ((success = (*ptr == oldval))) {
        *ptr = newval;
    }

    pthread_mutex_unlock (&gsAtomicLock);

    return success;
}

void* c_atomic_pointer_exchange (void* atomic, void* newVal)
{
    void** ptr = atomic;
    void* oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *ptr;
    *ptr = newVal;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cintptr c_atomic_pointer_add (volatile void* atomic, cssize val)
{
    cintptr* ptr = (cintptr*) atomic;
    cintptr oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *ptr;
    *ptr = oldVal + val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cuintptr c_atomic_pointer_and (volatile void* atomic, culong val)
{
    cuint64* ptr = (culong*) atomic;
    cuint64 oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *ptr;
    *ptr = oldVal & val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cuintptr c_atomic_pointer_or (volatile void* atomic, culong val)
{
    cuint64* ptr = (culong*) atomic;
    cuint64 oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *ptr;
    *ptr = oldVal | val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cuintptr c_atomic_pointer_xor (volatile void* atomic, csize val)
{
    cuint64* ptr = (culong*) atomic;
    cuint64 oldVal;

    pthread_mutex_lock (&gsAtomicLock);
    oldVal = *ptr;
    *ptr = oldVal ^ val;
    pthread_mutex_unlock (&gsAtomicLock);

    return oldVal;
}

cint g_atomic_int_exchange_and_add (volatile cint *atomic, cint val)
{
    return c_atomic_int_add ((cint*) atomic, val);
}

void c_ref_count_init (crefcount* rc)
{
    c_return_if_fail (rc != NULL);

    /* Non-atomic refcounting is implemented using the negative range
     * of signed integers:
     *
     * G_MININT                 Z¯< 0 > Z⁺                G_MAXINT
     * |----------------------------|----------------------------|
     *
     * Acquiring a reference moves us towards MININT, and releasing a
     * reference moves us towards 0.
     */

    *rc = -1;
}

void c_ref_count_inc (crefcount* rc)
{
    crefcount rrc;

    c_return_if_fail (rc != NULL);

    rrc = *rc;

    c_return_if_fail (rrc < 0);

    /* Check for saturation */
    if (rrc == C_MIN_INT32) {
        C_LOG_ERROR_CONSOLE("Reference count %p has reached saturation", rc);
        return;
    }

    rrc -= 1;

    *rc = rrc;
}

bool c_ref_count_dec (crefcount* rc)
{
    crefcount rrc;

    c_return_val_if_fail (rc != NULL, false);

    rrc = *rc;

    c_return_val_if_fail (rrc < 0, false);

    rrc += 1;
    if (rrc == 0) {
        return true;
    }

    *rc = rrc;

    return false;
}

bool c_ref_count_compare (crefcount* rc, cint val)
{
    crefcount rrc;

    c_return_val_if_fail (rc != NULL, false);
    c_return_val_if_fail (val >= 0, false);

    rrc = *rc;

    if (val == C_MAX_INT32) {
        return rrc == C_MIN_INT32;
    }

    return rrc == -val;
}

void c_atomic_ref_count_init (catomicrefcount* arc)
{
    c_return_if_fail (arc != NULL);

    /* Atomic refcounting is implemented using the positive range
     * of signed integers:
     *
     * G_MININT                 Z¯< 0 > Z⁺                G_MAXINT
     * |----------------------------|----------------------------|
     *
     * Acquiring a reference moves us towards MAXINT, and releasing a
     * reference moves us towards 0.
     */
    *arc = 1;
}

void c_atomic_ref_count_inc (catomicrefcount* arc)
{
    cint oldValue;

    c_return_if_fail (arc != NULL);
    oldValue = c_atomic_int_add (arc, 1);
    c_return_if_fail (oldValue > 0);

    if (oldValue == C_MAX_INT32) {
        C_LOG_ERROR_CONSOLE("Reference count has reached saturation");
        c_assert(false);
    }
}

bool c_atomic_ref_count_dec (catomicrefcount* arc)
{
    bool isZero;

    c_return_val_if_fail (arc, false);
    isZero = c_atomic_int_dec_and_test(arc);

    return isZero;
}

bool c_atomic_ref_count_compare (catomicrefcount* arc, cint val)
{
    c_return_val_if_fail (arc != NULL, false);
    c_return_val_if_fail (val >= 0, false);

    return c_atomic_int_get (arc) == val;
}

cint c_atomic_bool_get(volatile const bool *atomic)
{
    bool value;

    pthread_mutex_lock (&gsAtomicLock);
    value = *atomic;
    pthread_mutex_unlock (&gsAtomicLock);

    return value;
}

void c_atomic_bool_set(volatile bool *atomic, bool newval)
{
    pthread_mutex_lock (&gsAtomicLock);
    *atomic = newval;
    pthread_mutex_unlock (&gsAtomicLock);
}

bool c_atomic_bool_compare_and_exchange(volatile bool* atomic, bool oldVal, cint newVal)
{
    bool success;

    pthread_mutex_lock (&gsAtomicLock);

    if ((success = (*atomic == oldVal))) {
        *atomic = newVal;
    }

    pthread_mutex_unlock (&gsAtomicLock);

    return success;
}
