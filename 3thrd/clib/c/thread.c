
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

#include "thread.h"

#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <linux/futex.h>

#include "log.h"
#include "str.h"
#include "error.h"
#include "slist.h"
#include "atomic.h"


C_DEFINE_QUARK (c_thread_error, c_thread_error)

#ifndef FUTEX_WAIT_PRIVATE
#define FUTEX_WAIT_PRIVATE FUTEX_WAIT
#define FUTEX_WAKE_PRIVATE FUTEX_WAKE
#endif

#define posix_check_err(err, name) C_STMT_START{ \
    int posixCheckError_ = (err); \
    if (posixCheckError_) { \
        C_LOG_ERROR_CONSOLE("error '%s' during '%s'", c_strerror (posixCheckError_), name); \
    } \
} C_STMT_END

#define posix_check_cmd(cmd) posix_check_err (cmd, #cmd)

#if 0
#define exchange_acquire(ptr, newT) \
    atomic_exchange_explicit((atomic_uint*) (ptr), (newT), __ATOMIC_ACQUIRE)
#define compare_exchange_acquire(ptr, old, newT) \
    atomic_compare_exchange_strong_explicit((atomic_uint*) (ptr), (old), (newT), __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)

#define exchange_release(ptr, newT) \
    atomic_exchange_explicit((atomic_uint*) (ptr), (newT), __ATOMIC_RELEASE)
#define store_release(ptr, new) \
    atomic_store_explicit((atomic_uint*) (ptr), (new), __ATOMIC_RELEASE)

#else

#define exchange_acquire(ptr, newT) \
    c_atomic_int_exchange((int*)(ptr), (newT))
#define compare_exchange_acquire(ptr, old, newT) \
    c_atomic_int_compare_and_exchange((int*)(ptr), (old), newT)

#define exchange_release(ptr, newT) \
    c_atomic_int_exchange((int*) (ptr), (newT))
//#define store_release(ptr, newT) \
//    c_atomic_int_store_4((ptr), (newT))
#endif


#if defined(__NR_futex) && defined(__NR_futex_time64)
#define c_futex_simple(uaddr, futex_op, ...) \
    C_STMT_START { \
        int res = syscall (__NR_futex_time64, uaddr, (csize) futex_op, __VA_ARGS__); \
        if (res < 0 && errno == ENOSYS) { \
            syscall (__NR_futex, uaddr, (csize) futex_op, __VA_ARGS__);              \
        } \
    } C_STMT_END
#elif defined(__NR_futex_time64)
#define c_futex_simple(uaddr, futex_op, ...) \
    C_STMT_START { \
        syscall (__NR_futex_time64, uaddr, (csize) futex_op, __VA_ARGS__); \
    } C_STMT_END
#elif defined(__NR_futex)
#define c_futex_simple(uaddr, futex_op, ...) \
    C_STMT_START { \
        syscall (__NR_futex, uaddr, (csize) futex_op, __VA_ARGS__); \
    } C_STMT_END
#else /* !defined(__NR_futex) && !defined(__NR_futex_time64) */
#error "Neither __NR_futex nor __NR_futex_time64 are defined but were found by meson"
#endif /* defined(__NR_futex) && defined(__NR_futex_time64) */


typedef enum
{
    C_MUTEX_STATE_EMPTY = 0,
    C_MUTEX_STATE_OWNED,
    C_MUTEX_STATE_CONTENDED,
} CMutexState;

struct _CRealThread
{
    CThread         thread;
    int             refCount;
    bool            ours;
    char*           name;
    void*           retVal;
};

typedef struct
{
    struct sched_attr*  attr;
    void*               dummy;
} CThreadSchedulerSettings;

typedef struct
{
    CRealThread         thread;
    pthread_t           systemThread;
    bool                joined;
    CMutex              lock;

    void *(*proxy) (void *);

    /* Must be statically allocated and valid forever */
    const CThreadSchedulerSettings* schedulerSettings;
} CThreadPosix;


static CMutex       gsOnceMutex;
static CCond        gsOnceCond;
static CSList*      gsOnceInitList = NULL;
static cuint        gsThreadNCreatedCounter = 0;    // atomic


static pthread_key_t*   c_private_impl_new      (CDestroyNotify notify);
static void             c_mutex_lock_slowpath   (CMutex *mutex);
static void             c_mutex_unlock_slowpath (CMutex* mutex, cuint prev);
static pthread_rwlock_t*c_rw_lock_get_impl      (CRWLock *lock);
static pthread_rwlock_t*c_rw_lock_impl_new      (void);
static void             c_rw_lock_impl_free     (pthread_rwlock_t* rwlock);
static pthread_mutex_t* c_rec_mutex_impl_new    (void);
static pthread_mutex_t* c_rec_mutex_get_impl    (CRecMutex *rec_mutex);
static void             c_rec_mutex_impl_free   (pthread_mutex_t *mutex);

void            c_system_thread_exit                    (void);
cuint           c_thread_n_created                      (void);
void*           c_thread_proxy                          (void* thread);
void            c_system_thread_set_name                (const char* name);
void            c_system_thread_wait                    (CRealThread* thread);
void            c_system_thread_free                    (CRealThread* thread);
void*           c_private_set_alloc0                    (CPrivate* key, csize size);
bool            c_system_thread_get_scheduler_settings  (CThreadSchedulerSettings* schedulerSettings);
bool            c_thread_get_scheduler_settings         (CThreadSchedulerSettings* schedulerSettings);
CRealThread*    c_system_thread_new                     (CThreadFunc proxy, culong stackSize, const CThreadSchedulerSettings* scheduleSettings, const char* name, CThreadFunc func, void* data, CError** error);
CThread*        c_thread_new_internal                   (const char* name, CThreadFunc proxy, CThreadFunc func, void* data, csize stackSize, const CThreadSchedulerSettings* schedulerSettings, CError** error);


static void c_thread_cleanup (void* data);
static void g_private_impl_free (pthread_key_t *key);
static void c_thread_abort (cint status, const char* function);


static CPrivate gsThreadSpecificPrivate = C_PRIVATE_INIT (c_thread_cleanup);


static void g_private_impl_free (pthread_key_t *key)
{
    cint status = pthread_key_delete (*key);
    if C_UNLIKELY (status != 0) {
        c_thread_abort (status, "pthread_key_delete");
    }
    free (key);
}

static inline pthread_key_t * c_private_get_impl (CPrivate* key)
{
    pthread_key_t* impl = c_atomic_pointer_get (&key->p);

    if C_UNLIKELY (impl == NULL) {
        impl = c_private_impl_new (key->notify);
        if (!c_atomic_pointer_compare_and_exchange (&key->p, NULL, impl)) {
            g_private_impl_free (impl);
            impl = key->p;
        }
    }

    return impl;
}

CThread* c_thread_ref (CThread* thread)
{
    CRealThread* real = (CRealThread*) thread;

    c_atomic_int_inc (&real->refCount);

    return thread;
}

void c_thread_unref (CThread* thread)
{
    c_return_if_fail(thread);

    CRealThread* real = (CRealThread*) thread;

    if (c_atomic_int_dec_and_test (&real->refCount)) {
        if (real->ours) {
            c_system_thread_free (real);
        }
        else {
            c_free (real);
        }
    }
}

CThread* c_thread_new (const char* name, CThreadFunc func, void* data)
{
    CError* error = NULL;

    CThread* thread = c_thread_new_internal (name, c_thread_proxy, func, data, 0, NULL, &error);

    if (C_UNLIKELY (thread == NULL)) {
        C_LOG_ERROR_CONSOLE("creating thread '%s': %s", name ? name : "", error->message);
    }

    return thread;
}

CThread* c_thread_try_new (const char* name, CThreadFunc func, void* data, CError** error)
{
    return c_thread_new_internal (name, c_thread_proxy, func, data, 0, NULL, error);
}

CThread* c_thread_self (void)
{
    CRealThread* thread = c_private_get (&gsThreadSpecificPrivate);

    if (!thread) {
        thread = c_malloc0(sizeof (CRealThread));
        thread->refCount = 1;

        c_private_set (&gsThreadSpecificPrivate, thread);
    }

    return (CThread*) thread;
}

void c_thread_exit (void* retVal)
{
    CRealThread* real = (CRealThread*) c_thread_self ();

    if (C_UNLIKELY (!real->ours)) {
        C_LOG_ERROR_CONSOLE("attempt to c_thread_exit() a thread not created by CLib");
    }

    real->retVal = retVal;

    c_system_thread_exit ();
}

void* c_thread_join (CThread* thread)
{
    CRealThread *real = (CRealThread*) thread;

    c_return_val_if_fail (thread, NULL);
    c_return_val_if_fail (real->ours, NULL);

    c_system_thread_wait (real);

    void* retVal = real->retVal;

    thread->joinable = 0;

    c_thread_unref (thread);

    return retVal;
}

void c_thread_yield (void)
{
    sched_yield();
}

void c_mutex_init (CMutex* mutex)
{
    mutex->i[0] = C_MUTEX_STATE_EMPTY;
}

void c_mutex_clear (CMutex* mutex)
{
    if C_UNLIKELY (mutex->i[0] != C_MUTEX_STATE_EMPTY) {
        C_LOG_ERROR_CONSOLE("c_mutex_clear() called on uninitialised or locked mutex");
        c_abort ();
    }
}

void c_mutex_lock (CMutex* mutex)
{
    if (C_UNLIKELY(!c_atomic_int_compare_and_exchange ((int*)&mutex->i[0], C_MUTEX_STATE_EMPTY, C_MUTEX_STATE_OWNED))) {
        c_mutex_lock_slowpath (mutex);
    }
}

bool c_mutex_trylock (CMutex* mutex)
{
    CMutexState empty = C_MUTEX_STATE_EMPTY;

    return compare_exchange_acquire (&mutex->i[0], empty, C_MUTEX_STATE_OWNED);
}

void c_mutex_unlock (CMutex* mutex)
{
    cuint prev;

    prev = exchange_release (&mutex->i[0], C_MUTEX_STATE_EMPTY);

    if C_UNLIKELY (prev != C_MUTEX_STATE_OWNED) {
        c_mutex_unlock_slowpath (mutex, prev);
    }
}

void c_rw_lock_init (CRWLock* rwLock)
{
    rwLock->p = c_rw_lock_impl_new();
}

void c_rw_lock_clear (CRWLock* rwLock)
{
    c_rw_lock_impl_free(rwLock->p);
}

void c_rw_lock_writer_lock (CRWLock* rwLock)
{
    int retval = pthread_rwlock_wrlock (c_rw_lock_get_impl (rwLock));

    if (retval != 0) {
        C_LOG_ERROR_CONSOLE("Failed to get RW lock %p: %s", rwLock, c_strerror (retval));
    }
}

bool c_rw_lock_writer_trylock (CRWLock* rwLock)
{
    if (pthread_rwlock_trywrlock (c_rw_lock_get_impl (rwLock)) != 0) {
        return false;
    }

    return true;
}

void c_rw_lock_writer_unlock (CRWLock* rwLock)
{
    pthread_rwlock_unlock (c_rw_lock_get_impl (rwLock));
}

void c_rw_lock_reader_lock (CRWLock* rwLock)
{
    int retval = pthread_rwlock_rdlock (c_rw_lock_get_impl (rwLock));

    if (retval != 0) {
        C_LOG_ERROR_CONSOLE("Failed to get RW lock %p: %s", rwLock, c_strerror (retval));
    }
}

bool c_rw_lock_reader_trylock (CRWLock* rwLock)
{
    if (pthread_rwlock_tryrdlock (c_rw_lock_get_impl (rwLock)) != 0) {
        return false;
    }

    return true;
}

void c_rw_lock_reader_unlock (CRWLock* rwLock)
{
    pthread_rwlock_unlock (c_rw_lock_get_impl (rwLock));
}

void c_rec_mutex_init (CRecMutex* recMutex)
{
    recMutex->p = c_rec_mutex_impl_new();
}

void c_rec_mutex_clear (CRecMutex* recMutex)
{
    c_rec_mutex_impl_free (recMutex->p);
}

void c_rec_mutex_lock (CRecMutex* recMutex)
{
    pthread_mutex_lock (c_rec_mutex_get_impl (recMutex));
}

bool c_rec_mutex_trylock (CRecMutex* recMutex)
{
    if (0 != pthread_mutex_trylock (c_rec_mutex_get_impl (recMutex))) {
        return false;
    }

    return true;
}

void c_rec_mutex_unlock (CRecMutex* recMutex)
{
    pthread_mutex_unlock (recMutex->p);
}

void c_cond_init (CCond* cond)
{
    cond->i[0] = 0;
}

void c_cond_clear (CCond* cond)
{

}

void c_cond_wait (CCond* cond, CMutex* mutex)
{
    cuint sampled = (cuint) c_atomic_int_get ((int*)&cond->i[0]);

    c_mutex_unlock (mutex);
    c_futex_simple (&cond->i[0], (csize) FUTEX_WAIT_PRIVATE, (csize) sampled, NULL);
    c_mutex_lock (mutex);
}

void c_cond_signal (CCond* cond)
{
    c_atomic_int_inc ((int*)&cond->i[0]);

    c_futex_simple (&cond->i[0], (csize) FUTEX_WAKE_PRIVATE, (csize) 1, NULL);
}

void c_cond_broadcast (CCond* cond)
{
    c_atomic_int_inc ((int*)&cond->i[0]);

    c_futex_simple (&cond->i[0], (csize) FUTEX_WAKE_PRIVATE, (csize) C_MAX_INT32, NULL);
}

bool c_cond_wait_until (CCond* cond, CMutex* mutex, cint64 endTime)
{

    struct timespec now;
    struct timespec span;

    cuint sampled;
    long res;
    bool success;

    if (endTime < 0) {
        return false;
    }

    clock_gettime (CLOCK_MONOTONIC, &now);
    span.tv_sec = (endTime / 1000000) - now.tv_sec;
    span.tv_nsec = ((endTime % 1000000) * 1000) - now.tv_nsec;
    if (span.tv_nsec < 0) {
        span.tv_nsec += 1000000000;
        span.tv_sec--;
    }

    if (span.tv_sec < 0) {
        return false;
    }

    sampled = cond->i[0];
    c_mutex_unlock (mutex);

#ifdef __NR_futex_time64
    {
    struct
    {
        gint64 tv_sec;
        gint64 tv_nsec;
    } span_arg;

    span_arg.tv_sec = span.tv_sec;
    span_arg.tv_nsec = span.tv_nsec;

    res = syscall (__NR_futex_time64, &cond->i[0], (gsize) FUTEX_WAIT_PRIVATE, (gsize) sampled, &span_arg);

    /* If the syscall does not exist (`ENOSYS`), we retry again below with the
     * normal `futex` syscall. This can happen if newer kernel headers are
     * used than the kernel that is actually running.
     */
#  ifdef __NR_futex
    if (res >= 0 || errno != ENOSYS)
#  endif /* defined(__NR_futex) */
    {
        success = (res < 0 && errno == ETIMEDOUT) ? FALSE : TRUE;
        g_mutex_lock (mutex);

        return success;
    }
}
#endif

#ifdef __NR_futex
    {
        struct {
            __kernel_long_t tv_sec;
            __kernel_long_t tv_nsec;
        } spanArg;

        /* Make sure to only ever call this if the end time actually fits into the target type */
        if (C_UNLIKELY (sizeof (__kernel_long_t) < 8 && span.tv_sec > C_MAX_INT32)) {
            C_LOG_ERROR_CONSOLE("%s: Can’t wait for more than %us", C_STRFUNC, C_MAX_INT32);
        }

        spanArg.tv_sec = span.tv_sec;
        spanArg.tv_nsec = span.tv_nsec;

        res = syscall (__NR_futex, &cond->i[0], (csize) FUTEX_WAIT_PRIVATE, (csize) sampled, &spanArg);
        success = (res < 0 && errno == ETIMEDOUT) ? false : true;
        c_mutex_lock (mutex);

        return success;
    }
#endif /* defined(__NR_futex) */

    c_assert_not_reached();
}

void* c_private_get (CPrivate* key)
{
    return pthread_getspecific (*c_private_get_impl(key));
}

void c_private_set (CPrivate* key, void* value)
{
    cint status;

    if C_UNLIKELY ((status = pthread_setspecific (*c_private_get_impl (key), value)) != 0) {
        c_thread_abort (status, "pthread_setspecific");
    }
}

void c_private_replace (CPrivate* key, void* value)
{
    pthread_key_t* impl = c_private_get_impl (key);
    cint status;

    void* old = pthread_getspecific (*impl);

    if C_UNLIKELY ((status = pthread_setspecific (*impl, value)) != 0) {
        c_thread_abort (status, "pthread_setspecific");
    }

    if (old && key->notify) {
        key->notify (old);
    }
}

bool c_once_init_enter (volatile void* location)
{
    csize* valueLocation = (csize*) location;
    bool needInit = false;
    c_mutex_lock (&gsOnceMutex);
    if (c_atomic_pointer_get (valueLocation) == 0) {
        if (!c_slist_find (gsOnceInitList, (void*) valueLocation)) {
            needInit = true;
            gsOnceInitList = c_slist_prepend (gsOnceInitList, (void*) valueLocation);
        }
        else {
            do {
                c_cond_wait (&gsOnceCond, &gsOnceMutex);
            }
            while (c_slist_find (gsOnceInitList, (void*) valueLocation));
        }
    }
    c_mutex_unlock (&gsOnceMutex);

    return needInit;
}

void c_once_init_leave (volatile void* location, csize result)
{
    csize* valueLocation = (csize*) location;

    c_return_if_fail (result != 0);

    csize oldValue = (csize) c_atomic_pointer_exchange (valueLocation, (void*) &result);
    c_return_if_fail (oldValue == 0);

    c_mutex_lock (&gsOnceMutex);
    c_return_if_fail (gsOnceInitList != NULL);
    gsOnceInitList = c_slist_remove (gsOnceInitList, (void*) valueLocation);
    c_cond_broadcast (&gsOnceCond);
    c_mutex_unlock (&gsOnceMutex);
}

cuint c_get_num_processors (void)
{
    return 1;
}

bool (c_once_init_enter_pointer) (void* location)
{
    void** valueLocation = (void**) location;
    bool needInit = false;
    c_mutex_lock (&gsOnceMutex);
    if (c_atomic_pointer_get (valueLocation) == 0) {
        if (!c_slist_find (gsOnceInitList, (void *) valueLocation)) {
            needInit = true;
            gsOnceInitList = c_slist_prepend (gsOnceInitList, (void *) valueLocation);
        }
        else {
            do {
                c_cond_wait (&gsOnceCond, &gsOnceMutex);
            } while (c_slist_find (gsOnceInitList, (void *) valueLocation));
        }
    }
    c_mutex_unlock (&gsOnceMutex);

    return needInit;
}

void (c_once_init_leave_pointer) (void* location, void* result)
{
    void** valueLocation = (void**) location;
    void* oldValue;

    c_return_if_fail (result != 0);

    oldValue = c_atomic_pointer_exchange (valueLocation, result);
    c_return_if_fail (oldValue == 0);

    c_mutex_lock (&gsOnceMutex);
    c_return_if_fail (gsOnceInitList != NULL);
    gsOnceInitList = c_slist_remove (gsOnceInitList, (void *) valueLocation);
    c_cond_broadcast (&gsOnceCond);
    c_mutex_unlock (&gsOnceMutex);
}



void* c_once_impl (COnce* once, CThreadFunc func, void* arg)
{
    c_mutex_lock (&gsOnceMutex);

    while (once->status == C_ONCE_STATUS_PROGRESS) {
        c_cond_wait (&gsOnceCond, &gsOnceMutex);
    }

    if (once->status != C_ONCE_STATUS_READY) {
        once->status = C_ONCE_STATUS_PROGRESS;

        c_mutex_unlock (&gsOnceMutex);
        void* retVal = func (arg);
        c_mutex_lock (&gsOnceMutex);

        once->retVal = retVal;
        once->status = C_ONCE_STATUS_READY;
        c_cond_broadcast (&gsOnceCond);
    }

    c_mutex_unlock (&gsOnceMutex);

    return (void*) once->retVal;
}

void* c_private_set_alloc0 (CPrivate* key, csize size)
{
    void* allocated = c_malloc0 (size);

    c_private_set (key, allocated);

    return c_steal_pointer (&allocated);
}

static void c_thread_cleanup (void* data)
{
    c_thread_unref (data);
}

void* c_thread_proxy (void* thread)
{
    CRealThread* threadT = thread;

    c_assert (thread);
    c_private_set (&gsThreadSpecificPrivate, thread);

    if (threadT->name) {
        c_system_thread_set_name (threadT->name);
        c_free (threadT->name);
        threadT->name = NULL;
    }

    threadT->retVal = threadT->thread.func (threadT->thread.data);

    return NULL;
}

cuint g_thread_n_created (void)
{
    return c_atomic_int_get ((int*) &gsThreadNCreatedCounter);
}

CThread* c_thread_new_internal (const char* name, CThreadFunc proxy, CThreadFunc func, void* data, csize stackSize, const CThreadSchedulerSettings* schedulerSettings, CError** error)
{
    c_return_val_if_fail (func != NULL, NULL);

    c_atomic_int_inc ((int*) &gsThreadNCreatedCounter);

    return (CThread*) c_system_thread_new (proxy, stackSize, schedulerSettings, name, func, data, error);
}

bool c_thread_get_scheduler_settings (CThreadSchedulerSettings* schedulerSettings)
{
    c_return_val_if_fail (schedulerSettings != NULL, false);

    return c_system_thread_get_scheduler_settings (schedulerSettings);
}

static void c_thread_abort (cint status, const char* function)
{
    fprintf (stderr, "CLib: Unexpected error from C library during '%s': %s.  Aborting.\n", function, strerror (status));
    c_abort ();
}

void c_system_thread_wait (CRealThread* thread)
{
    CThreadPosix* pt = (CThreadPosix*) thread;

    c_mutex_lock (&pt->lock);

    if (!pt->joined) {
        posix_check_cmd (pthread_join(pt->systemThread, NULL));
        pt->joined = true;
    }

    c_mutex_unlock (&pt->lock);
}

void c_system_thread_exit (void)
{
    pthread_exit (NULL);
}

void c_system_thread_set_name (const char* C_UNUSED name)
{
//#if defined(HAVE_PTHREAD_SETNAME_NP_WITHOUT_TID)
//    pthread_setname_np (name); /* on OS X and iOS */
//#elif defined(HAVE_PTHREAD_SETNAME_NP_WITH_TID)
//    pthread_setname_np (pthread_self (), name); /* on Linux and Solaris */
//#elif defined(HAVE_PTHREAD_SETNAME_NP_WITH_TID_AND_ARG)
//    pthread_setname_np (pthread_self (), "%s", (char*) name); /* on NetBSD */
//#elif defined(HAVE_PTHREAD_SET_NAME_NP)
//    pthread_set_name_np (pthread_self (), name); /* on FreeBSD, DragonFlyBSD, OpenBSD */
//#endif
}

void c_system_thread_free (CRealThread* thread)
{
    c_return_if_fail(thread);

    CThreadPosix* pt = (CThreadPosix*) thread;

    if (!pt->joined) {
        pthread_detach (pt->systemThread);
    }
    c_mutex_clear (&pt->lock);

    c_free(pt);
}

bool c_system_thread_get_scheduler_settings (CThreadSchedulerSettings* C_UNUSED schedulerSettings)
{
    return false;
}

CRealThread* c_system_thread_new (CThreadFunc proxy, culong C_UNUSED stackSize, const CThreadSchedulerSettings* schedulerSettings, const char* name, CThreadFunc func, void* data, CError** error)
{
    CThreadPosix* thread;
    CRealThread* baseThread;
    pthread_attr_t attr;
    cint ret;

    thread = c_malloc0(sizeof (CThreadPosix));
    baseThread = (CRealThread*)thread;
    baseThread->refCount = 2;
    baseThread->ours = true;
    baseThread->thread.joinable = true;
    baseThread->thread.func = func;
    baseThread->thread.data = data;
    baseThread->name = c_strdup (name);
    thread->schedulerSettings = schedulerSettings;
    thread->proxy = proxy;

    posix_check_cmd (pthread_attr_init (&attr));

    ret = pthread_create (&thread->systemThread, &attr, (void* (*)(void*))proxy, thread);

    posix_check_cmd (pthread_attr_destroy (&attr));

    if (ret == EAGAIN) {
        c_set_error (error, C_THREAD_ERROR, C_THREAD_ERROR_AGAIN, "Error creating thread: %s", c_strerror (ret));
        c_free (thread->thread.name);
        c_free(thread);
        return NULL;
    }

    posix_check_err (ret, "thread_create");

    c_mutex_init (&thread->lock);

    return (CRealThread*) thread;
}

cuint c_thread_n_created (void)
{
    return c_atomic_int_get ((int*)&gsThreadNCreatedCounter);
}

static void c_mutex_lock_slowpath (CMutex *mutex)
{
    while (exchange_acquire (&mutex->i[0], C_MUTEX_STATE_CONTENDED) != C_MUTEX_STATE_EMPTY) {
        c_futex_simple (&mutex->i[0], (csize) FUTEX_WAIT_PRIVATE, C_MUTEX_STATE_CONTENDED, NULL);
    }
}

static void c_mutex_unlock_slowpath (CMutex* mutex, cuint prev)
{
    if C_UNLIKELY (prev == C_MUTEX_STATE_EMPTY) {
        C_LOG_ERROR_CONSOLE("Attempt to unlock mutex that was not locked");
        c_abort ();
    }

    c_futex_simple (&mutex->i[0], (csize) FUTEX_WAKE_PRIVATE, (csize) 1, NULL);
}

static pthread_key_t* c_private_impl_new (CDestroyNotify notify)
{
    pthread_key_t *key = malloc (sizeof (pthread_key_t));
    if C_UNLIKELY (key == NULL) {
        c_thread_abort (errno, "malloc");
    }

    cint status = pthread_key_create (key, notify);
    if C_UNLIKELY (status != 0) {
        c_thread_abort (status, "pthread_key_create");
    }

    return key;
}

static pthread_rwlock_t* c_rw_lock_impl_new (void)
{
    cint status;

    pthread_rwlock_t *rwlock = malloc (sizeof (pthread_rwlock_t));
    if C_UNLIKELY (rwlock == NULL) {
        c_thread_abort (errno, "malloc");
    }

    if C_UNLIKELY ((status = pthread_rwlock_init (rwlock, NULL)) != 0) {
        c_thread_abort (status, "pthread_rwlock_init");
    }

    return rwlock;
}


static void c_rw_lock_impl_free (pthread_rwlock_t* rwlock)
{
    pthread_rwlock_destroy (rwlock);
    free (rwlock);
}

static pthread_rwlock_t* c_rw_lock_get_impl (CRWLock *lock)
{
    pthread_rwlock_t* impl = c_atomic_pointer_get (&lock->p);

    if C_UNLIKELY (impl == NULL) {
        impl = c_rw_lock_impl_new ();
        if (!c_atomic_pointer_compare_and_exchange (&lock->p, NULL, impl)) {
            c_rw_lock_impl_free (impl);
        }
        impl = lock->p;
    }

    return impl;
}

static pthread_mutex_t* c_rec_mutex_impl_new (void)
{
    pthread_mutexattr_t attr;
    pthread_mutex_t *mutex;

    mutex = malloc (sizeof (pthread_mutex_t));
    if C_UNLIKELY (mutex == NULL) {
        c_thread_abort (errno, "malloc");
    }

    pthread_mutexattr_init (&attr);
    pthread_mutexattr_settype (&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init (mutex, &attr);
    pthread_mutexattr_destroy (&attr);

    return mutex;
}


static void c_rec_mutex_impl_free (pthread_mutex_t *mutex)
{
    pthread_mutex_destroy (mutex);
    free (mutex);
}

static pthread_mutex_t* c_rec_mutex_get_impl (CRecMutex *rec_mutex)
{
    pthread_mutex_t *impl = c_atomic_pointer_get (&rec_mutex->p);

    if C_UNLIKELY (impl == NULL) {
        impl = c_rec_mutex_impl_new ();
        if (!c_atomic_pointer_compare_and_exchange (&rec_mutex->p, NULL, impl)) {
            c_rec_mutex_impl_free (impl);
        }
        impl = rec_mutex->p;
    }

    return impl;
}

