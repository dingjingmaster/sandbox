
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

#ifndef CLIBRARY_THREAD_H
#define CLIBRARY_THREAD_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/error.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C


#define C_LOCK_NAME(name)               c__ ## name ## _lock
#define C_LOCK_DEFINE_STATIC(name)      static C_LOCK_DEFINE (name)
#define C_LOCK_DEFINE(name)             CMutex C_LOCK_NAME (name)
#define C_LOCK_EXTERN(name)             extern CMutex C_LOCK_NAME (name)

#define C_PRIVATE_INIT(notify)          { NULL, (notify), { NULL, NULL } }
#define C_ONCE_INIT                     { C_ONCE_STATUS_NOTCALLED, NULL }

#define C_LOCK(name)                    c_mutex_lock (&C_LOCK_NAME (name))
#define C_UNLOCK(name)                  c_mutex_unlock (&C_LOCK_NAME (name))
#define C_TRYLOCK(name)                 c_mutex_trylock (&C_LOCK_NAME (name))

#define C_THREAD_ERROR c_thread_error_quark()


#if defined(C_ATOMIC_LOCK_FREE) && defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4) && defined(__ATOMIC_SEQ_CST)
#define c_once(once, func, arg) \
    ((__atomic_load_n (&(once)->status, __ATOMIC_ACQUIRE) == C_ONCE_STATUS_READY) ? \
    (once)->retval \
    : c_once_impl ((once), (func), (arg)))
#else
#define c_once(once, func, arg) c_once_impl ((once), (func), (arg))
#endif


typedef struct _CCond                                           CCond;
typedef struct _COnce                                           COnce;
typedef union  _CMutex                                          CMutex;
typedef struct _CRWLock                                         CRWLock;
typedef struct _CThread                                         CThread;
typedef struct _CPrivate                                        CPrivate;
typedef struct _CRecMutex                                       CRecMutex;
typedef struct _CRealThread                                     CRealThread;
typedef void                                                    CMutexLocker;
typedef void                                                    CRecMutexLocker;
typedef void                                                    CRWLockWriterLocker;
typedef void                                                    CRWLockReaderLocker;

typedef void* (*CThreadFunc) (void* udata);


typedef enum
{
    C_ONCE_STATUS_NOTCALLED,
    C_ONCE_STATUS_PROGRESS,
    C_ONCE_STATUS_READY
} COnceStatus;

typedef enum
{
    C_THREAD_ERROR_AGAIN,
} CThreadError;

typedef enum
{
    C_THREAD_PRIORITY_LOW,
    C_THREAD_PRIORITY_NORMAL,
    C_THREAD_PRIORITY_HIGH,
    C_THREAD_PRIORITY_URGENT
} CThreadPriority;

union _CMutex
{
    /*< private >*/
    void*           p;
    cuint           i[2];
};

struct _CRWLock
{
    /*< private >*/
    void*           p;
    cuint           i[2];
};

struct _CCond
{
    /*< private >*/
    void*           p;
    cuint           i[2];
};

struct _CRecMutex
{
    /*< private >*/
    void*           p;
    cuint           i[2];
};

struct _CPrivate
{
    /*< private >*/
    void*           p;
    CDestroyNotify  notify;
    void*           future[2];
};

struct _COnce
{
    volatile COnceStatus    status;
    volatile void*          retVal;
};

struct _CThread
{
    /*< private >*/
    CThreadFunc             func;
    void*                   data;
    bool                    joinable;
    CThreadPriority         priority;
};

//struct _CRealThread
//{
//    CThread         thread;
//
//    int             refCount;
//    bool            ours;
//    char*           name;
//    void*           retVal;
//};



CQuark          c_thread_error_quark            (void);
CThread *       c_thread_ref                    (CThread* thread);
void            c_thread_unref                  (CThread* thread);
CThread *       c_thread_new                    (const char* name, CThreadFunc func, void* data);
CThread *       c_thread_try_new                (const char* name, CThreadFunc func, void* data, CError** error);
CThread *       c_thread_self                   (void);
void            c_thread_exit                   (void* retVal);
void*           c_thread_join                   (CThread* thread);
void            c_thread_yield                  (void);

void            c_mutex_init                    (CMutex* mutex);
void            c_mutex_clear                   (CMutex* mutex);
void            c_mutex_lock                    (CMutex* mutex);
bool            c_mutex_trylock                 (CMutex* mutex);
void            c_mutex_unlock                  (CMutex* mutex);
void            c_rw_lock_init                  (CRWLock* rwLock);
void            c_rw_lock_clear                 (CRWLock* rwLock);
void            c_rw_lock_writer_lock           (CRWLock* rwLock);
bool            c_rw_lock_writer_trylock        (CRWLock* rwLock);
void            c_rw_lock_writer_unlock         (CRWLock* rwLock);
void            c_rw_lock_reader_lock           (CRWLock* rwLock);
bool            c_rw_lock_reader_trylock        (CRWLock* rwLock);
void            c_rw_lock_reader_unlock         (CRWLock* rwLock);
void            c_rec_mutex_init                (CRecMutex* recMutex);
void            c_rec_mutex_clear               (CRecMutex* recMutex);
void            c_rec_mutex_lock                (CRecMutex* recMutex);
bool            c_rec_mutex_trylock             (CRecMutex* recMutex);
void            c_rec_mutex_unlock              (CRecMutex* recMutex);
void            c_cond_init                     (CCond* cond);
void            c_cond_clear                    (CCond* cond);
void            c_cond_wait                     (CCond* cond, CMutex* mutex);
void            c_cond_signal                   (CCond* cond);
void            c_cond_broadcast                (CCond* cond);
bool            c_cond_wait_until               (CCond* cond, CMutex* mutex, cint64 endTime);
void*           c_private_get                   (CPrivate* key);
void            c_private_set                   (CPrivate* key, void* value);
void            c_private_replace               (CPrivate* key, void* value);
void*           c_once_impl                     (COnce* once, CThreadFunc func, void* arg);
bool            c_once_init_enter               (volatile void* location);
void            c_once_init_leave               (volatile void* location, csize result);
cuint           c_get_num_processors            (void);
bool            c_once_init_enter_pointer       (void *location);
void            c_once_init_leave_pointer       (void *location, void* result);



static inline CMutexLocker* c_mutex_locker_new (CMutex *mutex)
{
    c_mutex_lock (mutex);

    return (CMutexLocker*) mutex;
}

static inline void c_mutex_locker_free (CMutexLocker* locker)
{
    c_mutex_unlock ((CMutex*) locker);
}

static inline CRecMutexLocker* c_rec_mutex_locker_new (CRecMutex *recMutex)
{
    c_rec_mutex_lock (recMutex);
    return (CRecMutexLocker*) recMutex;
}

static inline void c_rec_mutex_locker_free (CRecMutexLocker* locker)
{
    c_rec_mutex_unlock ((CRecMutex*) locker);
}

static inline CRWLockWriterLocker* c_rw_lock_writer_locker_new (CRWLock* rwLock)
{
    c_rw_lock_writer_lock (rwLock);

    return (CRWLockWriterLocker*) rwLock;
}

static inline void c_rw_lock_writer_locker_free (CRWLockWriterLocker* locker)
{
    c_rw_lock_writer_unlock ((CRWLock*) locker);
}

static inline CRWLockReaderLocker* c_rw_lock_reader_locker_new (CRWLock* rwLock)
{
    c_rw_lock_reader_lock (rwLock);
    return (CRWLockReaderLocker*) rwLock;
}

static inline void c_rw_lock_reader_locker_free (CRWLockReaderLocker *locker)
{
    c_rw_lock_reader_unlock ((CRWLock*) locker);
}


C_END_EXTERN_C

#endif //CLIBRARY_THREAD_H
