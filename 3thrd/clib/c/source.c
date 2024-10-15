
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-25.
//

#include "source.h"

#include <pwd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <linux/wait.h>
#include <sys/syscall.h>
#include <time.h>


#include "log.h"
#include "str.h"
#include "hash-table.h"
#include "hook.h"
#include "queue.h"
#include "error.h"
#include "thread.h"
#include "atomic.h"
#include "timer.h"
#include "wakeup.h"

#ifndef W_STOPCODE
#define W_STOPCODE(sig)      ((sig) << 8 | 0x7f)
#endif

#ifndef W_EXITCODE
#define W_EXITCODE(ret, sig) ((ret) << 8 | (sig))
#endif

#define LOCK_CONTEXT(context) c_mutex_lock (&context->mutex)
#define UNLOCK_CONTEXT(context) c_mutex_unlock (&context->mutex)
#define C_THREAD_SELF c_thread_self ()
#define SOURCE_DESTROYED(source) (((source)->flags & C_HOOK_FLAG_ACTIVE) == 0)
#define SOURCE_BLOCKED(source) (((source)->flags & C_SOURCE_BLOCKED) != 0)


C_DEFINE_QUARK (c-unix-error-quark, c_unix_error)


typedef struct _CIdleSource             CIdleSource;
typedef struct _CTimeoutSource          CTimeoutSource;
typedef struct _CChildWatchSource       CChildWatchSource;
typedef struct _CUnixSignalWatchSource  CUnixSignalWatchSource;
typedef struct _CPollRec                CPollRec;
typedef struct _CSourceCallback         CSourceCallback;
typedef struct _CUnixSignalWatchSource  CUnixSignalWatchSource;
typedef struct _CSourceList             CSourceList;
typedef struct _CMainWaiter             CMainWaiter;
typedef struct _CMainDispatch           CMainDispatch;


typedef enum
{
    C_SOURCE_READY          = 1 << (C_HOOK_FLAG_USER_SHIFT),
    C_SOURCE_CAN_RECURSE    = 1 << (C_HOOK_FLAG_USER_SHIFT + 1),
    C_SOURCE_BLOCKED        = 1 << (C_HOOK_FLAG_USER_SHIFT + 2)
} CSourceFlags;

struct _CMainWaiter
{
    CCond*  cond;
    CMutex* mutex;
};

struct _CMainDispatch
{
    cint        depth;
    CSource*    source;
};

struct _CSourceList
{
    CSource *head, *tail;
    cint priority;
};

typedef struct
{
    CSource     source;
    cint        fd;
    void*       tag;
} CUnixFDSource;

struct _CUnixSignalWatchSource
{
    CSource     source;
    int         signum;
    bool        pending; /* (atomic) */
};

struct _CMainContext
{
    CMutex              mutex;
    CCond               cond;
    CThread*            owner;
    cuint               ownerCount;
    CMainContextFlags   flags;
    CSList*             waiters;
    cint                refCount;               // (atomic)
    CHashTable*         sources;                // guint -> CSource
    CPtrArray*          pendingDispatches;
    cint                timeoutUsec;            // Timeout for current iteration
    cuint               nextId;
    CList*              sourceLists;
    cint                inCheckOrPrepare;
    CPollRec*           pollRecords;
    cuint               nPollRecords;
    CPollFD*            cachedPollArray;
    cuint               cachedPollArraySize;
    CWakeup*            wakeup;
    CPollFD             wakeupRec;
    bool                pollChanged;
    CPollFunc           pollFunc;
    cint64              time;
    bool                timeIsFresh;
};

struct _CSourceCallback
{
    cint                refCount;  /* (atomic) */
    CSourceFunc         func;
    void*               data;
    CDestroyNotify      notify;
};

struct _CMainLoop
{
    CMainContext*       context;
    bool                isRunning; /* (atomic) */
    cint                refCount;  /* (atomic) */
};

struct _CIdleSource
{
    CSource             source;
    bool                oneShot;
};

struct _CTimeoutSource
{
    CSource             source;
    cuint               interval;
    bool                seconds;
    bool                oneShot;
};

struct _CChildWatchSource
{
    CSource             source;
    CPid                pid;
    cint                childStatus;
    CPollFD             poll;
    bool                childExited; /* (atomic); not used iff @using_pidfd is set */
    bool                usingPidFd;
};

struct _CPollRec
{
    CPollFD*            fd;
    CPollRec*           prev;
    CPollRec*           next;
    cint                priority;
};

struct _CSourcePrivate
{
    CSList*             childSources;
    CSource*            parentSource;
    cint64              readyTime;
    CSList*             fds;
    CSourceDisposeFunc  dispose;
    bool                staticName;
};

typedef struct _CSourceIter
{
    CMainContext*       context;
    bool                mayModify;
    CList*              currentList;
    CSource*            source;
} CSourceIter;


CMainContext* c_get_worker_context (void);


extern void* c_private_set_alloc0 (CPrivate* key, csize size);

static void c_source_callback_ref   (void* cbData);
static void c_source_callback_unref (void* cbData);
static void c_source_callback_get   (void* cbData, CSource* source, CSourceFunc* func, void** data);

static CMainDispatch*   get_dispatch (void);
static void             free_context (void* data);
static void             dispatch_unix_signals (void);
static void             wake_source (CSource *source);
static void*            clib_worker_main (void* data);
static const char*      signum_to_string (int signum);
static CSource*         idle_source_new (bool oneShot);
static void             free_context_stack (void* data);
static void             unblock_source (CSource *source);
static void             c_main_dispatch_free (void* dispatch);
static void             dispatch_unix_signals_unlocked (void);
static void             c_main_dispatch (CMainContext* context);
static bool             c_unix_signal_watch_check (CSource* source);
static int              round_timeout_to_msec (cint64 timeout_usec);
CSource*                _c_main_create_unix_signal_watch (int signum);
static void             ref_unix_signal_handler_unlocked (int signum);
static void             unref_unix_signal_handler_unlocked (int signum);
static int              siginfo_t_to_wait_status (const siginfo_t *info);
static void             c_main_context_release_unlocked (CMainContext* context);
static bool             c_main_context_acquire_unlocked (CMainContext *context);
static void             c_main_context_dispatch_unlocked (CMainContext *context);
static inline void      poll_rec_list_free (CMainContext* context, CPollRec* list);
static bool             c_unix_set_error_from_errno (CError** error, int savedErrno);
static bool             c_unix_signal_watch_prepare (CSource* source, cint* timeout);
static void             source_add_to_context (CSource* source, CMainContext* context);
static CSource*         timeout_source_new (cuint interval, bool seconds, bool one_shot);
static void             source_remove_from_context (CSource* source, CMainContext* context);
static bool             c_main_context_prepare_unlocked (CMainContext* context, cint* priority);
static void             c_source_set_name_full (CSource* source, const char *name, bool is_static);
bool                    c_unix_fd_source_dispatch (CSource* source, CSourceFunc callback, void* udata);
static void             c_timeout_set_expiration (CTimeoutSource* timeout_source, cint64 current_time);
static bool             c_main_context_wait_internal (CMainContext* context, CCond* cond, CMutex* mutex);
static bool             c_unix_signal_watch_dispatch (CSource* source, CSourceFunc callback, void* udata);
static CSourceList*     find_source_list_for_priority (CMainContext* context, cint priority, bool create);
static cuint            c_source_attach_unlocked (CSource* source, CMainContext* context, bool do_wakeup);
static bool             c_main_context_iterate (CMainContext* context, bool block, bool dispatch, CThread* self);
static bool             c_main_context_iterate_unlocked (CMainContext* context, bool block, bool dispatch, CThread* self);
static bool             c_main_context_check_unlocked (CMainContext *context, cint max_priority, CPollFD* fds, cint n_fds);
static void             c_main_context_poll_unlocked (CMainContext *context, cint64 timeout_usec, int priority, CPollFD* fds, int n_fds);
static cint             c_main_context_query_unlocked (CMainContext* context, cint max_priority, cint64* timeout_usec, CPollFD* fds, cint n_fds);
static cuint            timeout_add_full (cint priority, cuint interval, bool seconds, bool one_shot, CSourceFunc function, void* data, CDestroyNotify notify);

static cuint            idle_add_full (cint priority, bool oneShot, CSourceFunc function, void* data, CDestroyNotify notify);
static void             c_source_unref_internal             (CSource* source, CMainContext* context, bool haveLock);
static void             c_source_destroy_internal           (CSource* source, CMainContext* context, bool haveLock);
static void             c_source_set_priority_unlocked      (CSource* source, CMainContext* context, cint priority);
static void             c_child_source_remove_internal      (CSource* childSource, CMainContext* context);
static void             c_main_context_poll                 (CMainContext* context, cint timeout, cint priority, CPollFD* fds, cint nFds);
static void             c_main_context_add_poll_unlocked    (CMainContext* context, cint priority, CPollFD* fd);
static void             c_main_context_remove_poll_unlocked (CMainContext* context, CPollFD* fd);
static void             c_source_iter_init (CSourceIter* iter, CMainContext* context, bool mayModify);
static bool             c_source_iter_next (CSourceIter* iter, CSource** source);
static void             c_source_iter_clear (CSourceIter* iter);
static bool             c_timeout_dispatch (CSource* source, CSourceFunc callback, void* udata);
static bool             c_child_watch_prepare (CSource* source, cint* timeout);
static bool             c_child_watch_check (CSource* source);
static bool             c_child_watch_dispatch (CSource* source, CSourceFunc callback, void* udata);
static void             c_child_watch_finalize (CSource* source);
static void             c_unix_signal_handler (int signum);
static bool             c_unix_signal_watch_prepare  (CSource* source, cint* timeout);
static bool             c_unix_signal_watch_check    (CSource* source);
static bool             c_unix_signal_watch_dispatch (CSource* source, CSourceFunc callback, void* udata);
static void             c_unix_signal_watch_finalize  (CSource* source);
static bool             c_idle_prepare     (CSource* source, cint* timeout);
static bool             c_idle_check       (CSource* source);
static bool             c_idle_dispatch    (CSource* source, CSourceFunc callback, void* udata);
static void             block_source (CSource* source);

static void             free_context (void* data);
static void             free_context_stack (void* data);


C_LOCK_DEFINE_STATIC (gsUnixSignalLock);
static cuint gsUnixSignalRefcount[NSIG];
static CSList* gsUnixSignalWatches;
static CSList* gsUnixChildWatches;
static CPrivate gsThreadContextStack = C_PRIVATE_INIT(free_context_stack);


CSourceFuncs c_unix_fd_source_funcs = {
    NULL, NULL, c_unix_fd_source_dispatch, NULL, NULL, NULL
};
static volatile sig_atomic_t gsUnixSignalPending[NSIG];
static volatile sig_atomic_t gsAnyUnixSignalPending;
static CMainContext* gsClibWorkerContext;
C_LOCK_DEFINE_STATIC (gsUnixSignalLock);
static cuint gsUnixSignalRefcount[NSIG];
static CSList* gsUnixSignalWatches;
static CSList* gsUnixChildWatches;

static CSourceCallbackFuncs gsSourceCallbackFuncs = {
    c_source_callback_ref,
    c_source_callback_unref,
    c_source_callback_get,
};

CSourceFuncs c_unix_signal_funcs =
    {
        c_unix_signal_watch_prepare,
        c_unix_signal_watch_check,
        c_unix_signal_watch_dispatch,
        c_unix_signal_watch_finalize,
        NULL, NULL
    };
C_LOCK_DEFINE_STATIC (gsMainContextList);
static CSList* gsMainContextList = NULL;

CSourceFuncs c_timeout_funcs =
    {
        NULL, /* prepare */
        NULL, /* check */
        c_timeout_dispatch,
        NULL, NULL, NULL
    };

CSourceFuncs c_child_watch_funcs =
    {
        c_child_watch_prepare,
        c_child_watch_check,
        c_child_watch_dispatch,
        c_child_watch_finalize,
        NULL, NULL
    };

CSourceFuncs c_idle_funcs =
    {
        c_idle_prepare,
        c_idle_check,
        c_idle_dispatch,
        NULL, NULL, NULL
    };

bool c_unix_open_pipe (cint* fds, cint flags, CError** error)
{
    int eCode;

    c_return_val_if_fail ((flags & (FD_CLOEXEC)) == flags, false);

    eCode = pipe (fds);
    if (eCode == -1) {
        return c_unix_set_error_from_errno (error, errno);
    }

    if (flags == 0) {
        return true;
    }

    eCode = fcntl (fds[0], F_SETFD, flags);
    if (eCode == -1) {
        int savedErrno = errno;
        close (fds[0]);
        close (fds[1]);
        return c_unix_set_error_from_errno (error, savedErrno);
    }
    eCode = fcntl (fds[1], F_SETFD, flags);
    if (eCode == -1) {
        int savedErrno = errno;
        close (fds[0]);
        close (fds[1]);
        return c_unix_set_error_from_errno (error, savedErrno);
    }

    return true;
}

bool c_unix_set_fd_nonblocking (cint fd, bool nonblock, CError** error)
{
#ifdef F_GETFL
    clong fcntlFlags = fcntl (fd, F_GETFL);
    if (fcntlFlags == -1) {
        return c_unix_set_error_from_errno (error, errno);
    }

    if (nonblock) {
#ifdef O_NONBLOCK
        fcntlFlags |= O_NONBLOCK;
#else
        fcntlFlags |= O_NDELAY;
#endif
    }
    else {
#ifdef O_NONBLOCK
        fcntlFlags &= ~O_NONBLOCK;
#else
        fcntlFlags &= ~O_NDELAY;
#endif
    }

    if (fcntl (fd, F_SETFL, fcntlFlags) == -1) {
        return c_unix_set_error_from_errno (error, errno);
    }
    return true;
#else
    return c_unix_set_error_from_errno (error, EINVAL);
#endif
}

CSource* c_unix_signal_source_new (cint signum)
{
    c_return_val_if_fail (signum == SIGHUP || signum == SIGINT || signum == SIGTERM || signum == SIGUSR1 || signum == SIGUSR2 || signum == SIGWINCH, NULL);

    return _c_main_create_unix_signal_watch (signum);
}

cuint c_unix_signal_add_full (cint priority, cint signum, CSourceFunc handler, void* udata, CDestroyNotify notify)
{
    CSource *source = c_unix_signal_source_new (signum);

    if (priority != C_PRIORITY_DEFAULT) {
        c_source_set_priority (source, priority);
    }

    c_source_set_callback (source, handler, udata, notify);
    cuint id = c_source_attach (source, NULL);
    c_source_unref (source);

    return id;
}

cuint c_unix_signal_add (cint signum, CSourceFunc handler, void* udata)
{
    return c_unix_signal_add_full (C_PRIORITY_DEFAULT, signum, handler, udata, NULL);
}

CSource* c_unix_fd_source_new (cint fd, CIOCondition condition)
{
    CUnixFDSource* fdSource;
    CSource* source = c_source_new (&c_unix_fd_source_funcs, sizeof (CUnixFDSource));

    fdSource = (CUnixFDSource*) source;

    fdSource->fd = fd;
    fdSource->tag = c_source_add_unix_fd (source, fd, condition);

    return source;
}

cuint c_unix_fd_add_full (cint priority, cint fd, CIOCondition condition, CUnixFDSourceFunc function, void* udata, CDestroyNotify notify)
{
    CSource *source;
    cuint id;

    c_return_val_if_fail (function != NULL, 0);

    source = c_unix_fd_source_new (fd, condition);

    if (priority != C_PRIORITY_DEFAULT) {
        c_source_set_priority (source, priority);
    }

    c_source_set_callback (source, (CSourceFunc) function, udata, notify);
    id = c_source_attach (source, NULL);
    c_source_unref (source);

    return id;
}

cuint c_unix_fd_add (cint fd, CIOCondition condition, CUnixFDSourceFunc function, void* udata)
{
    return c_unix_fd_add_full (C_PRIORITY_DEFAULT, fd, condition, function, udata, NULL);
}

struct passwd* c_unix_get_passwd_entry (const char* userName, CError** error)
{
    struct passwd* passwdFileEntry;
    struct
    {
        struct passwd pwd;
        char stringBuffer[];
    } *buffer = NULL;
    csize stringBufferSize = 0;
    CError* localError = NULL;

    c_return_val_if_fail (userName != NULL, NULL);
    c_return_val_if_fail (error == NULL || *error == NULL, NULL);

#ifdef _SC_GETPW_R_SIZE_MAX
    {
        clong stringBufferSizeLong = sysconf (_SC_GETPW_R_SIZE_MAX);
        if (stringBufferSizeLong > 0) {
            stringBufferSize = stringBufferSizeLong;
        }
    }
#endif /* _SC_GETPW_R_SIZE_MAX */

    if (stringBufferSize == 0) {
        stringBufferSize = 64;
    }

    do {
        int retVal;
        c_free (buffer);
        buffer = c_malloc0 (sizeof (*buffer) + stringBufferSize + 6);
        retVal = getpwnam_r (userName, &buffer->pwd, buffer->stringBuffer, stringBufferSize, &passwdFileEntry);
        if (passwdFileEntry != NULL) {
            break;
        }
        else if (retVal == 0 || retVal == ENOENT || retVal == ESRCH || retVal == EBADF || retVal == EPERM) {
            c_unix_set_error_from_errno (&localError, retVal);
            break;
        }
        else if (retVal == ERANGE) {
            if (stringBufferSize > 32 * 1024) {
                c_unix_set_error_from_errno (&localError, retVal);
                break;
            }
            stringBufferSize *= 2;
            continue;
        }
        else {
            c_unix_set_error_from_errno (&localError, retVal);
            break;
        }
    }
    while (passwdFileEntry == NULL);

    c_assert (passwdFileEntry == NULL || (void*) passwdFileEntry == (void*) buffer);

    /* Success or error. */
    if (localError != NULL) {
        c_clear_pointer ((void**) &buffer, c_free0);
        c_propagate_error (error, c_steal_pointer (&localError));
    }

    return (struct passwd *) c_steal_pointer (&buffer);
}

CSource* _c_main_create_unix_signal_watch (int signum)
{
    CSource *source = c_source_new (&c_unix_signal_funcs, sizeof (CUnixSignalWatchSource));

    CUnixSignalWatchSource* unixSignalSource = (CUnixSignalWatchSource*) source;

    unixSignalSource->signum = signum;
    unixSignalSource->pending = false;

    /* Set a default name on the source, just in case the caller does not. */
    c_source_set_static_name (source, signum_to_string (signum));

    C_LOCK (gsUnixSignalLock);
    ref_unix_signal_handler_unlocked (signum);
    gsUnixSignalWatches = c_slist_prepend (gsUnixSignalWatches, unixSignalSource);
    dispatch_unix_signals_unlocked ();
    C_UNLOCK (gsUnixSignalLock);

    return source;
}

//
bool c_unix_fd_source_dispatch (CSource* source, CSourceFunc callback, void* udata)
{
    CUnixFDSource* fdSource = (CUnixFDSource*) source;
    CUnixFDSourceFunc func = (CUnixFDSourceFunc) callback;

    if (!callback) {
        C_LOG_WARNING_CONSOLE("GUnixFDSource dispatched without callback. You must call g_source_set_callback().");
        return false;
    }

    return (* func) (fdSource->fd, c_source_query_unix_fd (source, fdSource->tag), udata);
}

CMainContext* c_main_context_ref (CMainContext* context)
{
    c_return_val_if_fail (context != NULL, NULL);

    int oldRefCount = c_atomic_int_add (&context->refCount, 1);

    c_return_val_if_fail (oldRefCount > 0, NULL);

    return context;
}

void c_main_context_unref (CMainContext* context)
{
    CSourceIter iter;
    CSource *source;
    CList *sl_iter;
    CSList *s_iter, *remaining_sources = NULL;
    CSourceList *list;
    cuint i;

    c_return_if_fail (context != NULL);
    c_return_if_fail (c_atomic_int_get (&context->refCount) > 0);

    if (!c_atomic_int_dec_and_test (&context->refCount)) {
        return;
    }

    C_LOCK (gsMainContextList);
    gsMainContextList = c_slist_remove (gsMainContextList, context);
    C_UNLOCK (gsMainContextList);

    for (i = 0; i < context->pendingDispatches->len; i++) {
        c_source_unref_internal ((CSource*) (context->pendingDispatches->pdata[i]), context, false);
    }

    LOCK_CONTEXT (context);

    c_source_iter_init (&iter, context, false);
    while (c_source_iter_next (&iter, &source)) {
        source->context = NULL;
        remaining_sources = c_slist_prepend (remaining_sources, c_source_ref (source));
    }
    c_source_iter_clear (&iter);

    for (s_iter = remaining_sources; s_iter; s_iter = s_iter->next) {
        source = s_iter->data;
        c_source_destroy_internal (source, context, true);
    }

    for (sl_iter = context->sourceLists; sl_iter; sl_iter = sl_iter->next) {
        list = sl_iter->data;
        c_free (list);
    }
    c_list_free (context->sourceLists);

    c_hash_table_destroy (context->sources);

    UNLOCK_CONTEXT (context);
    c_mutex_clear (&context->mutex);

    c_ptr_array_free (context->pendingDispatches, true);
    c_free (context->cachedPollArray);

    poll_rec_list_free (context, context->pollRecords);

    c_wakeup_free (context->wakeup);
    c_cond_clear (&context->cond);

    c_free (context);

    for (s_iter = remaining_sources; s_iter; s_iter = s_iter->next) {
        source = s_iter->data;
        c_source_unref_internal (source, NULL, false);
    }
    c_slist_free (remaining_sources);
}

CMainContext* c_main_context_new_with_next_id (cuint nextId)
{
    CMainContext *ret = c_main_context_new ();

    ret->nextId = nextId;

    return ret;
}

CMainContext* c_main_context_new (void)
{
    return c_main_context_new_with_flags (C_MAIN_CONTEXT_FLAGS_NONE);
}

CMainContext* c_main_context_new_with_flags (CMainContextFlags flags)
{
    static csize initialised;

    if (c_once_init_enter (&initialised)) {
        //
        c_once_init_leave (&initialised, true);
    }

    CMainContext* context = c_malloc0(sizeof(CMainContext));

    c_mutex_init (&context->mutex);
    c_cond_init (&context->cond);

    context->nextId = 1;
    context->refCount = 1;
    context->owner = NULL;
    context->flags = flags;
    context->waiters = NULL;
    context->pollFunc = c_poll;
    context->sourceLists = NULL;
    context->timeIsFresh = false;
    context->cachedPollArray = NULL;
    context->cachedPollArraySize = 0;
    context->pendingDispatches = c_ptr_array_new ();
    context->sources = c_hash_table_new (NULL, NULL);

    context->wakeup = c_wakeup_new ();
    c_wakeup_get_pollfd (context->wakeup, &context->wakeupRec);
    c_main_context_add_poll_unlocked (context, 0, &context->wakeupRec);

    C_LOCK (gsMainContextList);
    gsMainContextList = c_slist_append (gsMainContextList, context);
    C_UNLOCK (gsMainContextList);

    return context;
}

CMainContext* c_main_context_default (void)
{
    static CMainContext* defaultMainContext = NULL;

    if (c_once_init_enter (&defaultMainContext)) {
        CMainContext *context = c_main_context_new ();
        c_once_init_leave (&defaultMainContext, (culong) context);
    }

    return defaultMainContext;
}

bool c_main_context_iteration (CMainContext* context, bool mayBlock)
{
    bool retval;

    if (!context) {
        context = c_main_context_default();
    }

    LOCK_CONTEXT (context);
    retval = c_main_context_iterate_unlocked (context, mayBlock, true, C_THREAD_SELF);
    UNLOCK_CONTEXT (context);

    return retval;
}

bool c_main_context_pending (CMainContext* context)
{
    bool retval;

    if (!context) {
        context = c_main_context_default();
    }

    LOCK_CONTEXT (context);
    retval = c_main_context_iterate_unlocked (context, false, false, C_THREAD_SELF);
    UNLOCK_CONTEXT (context);

    return retval;
}

CSource* c_main_context_find_source_by_id (CMainContext* context, cuint sourceId)
{
    CSource *source = NULL;
    const void* ptr;

    c_return_val_if_fail (sourceId > 0, NULL);

    if (context == NULL) {
        context = c_main_context_default ();
    }

    LOCK_CONTEXT (context);
    ptr = c_hash_table_lookup (context->sources, &sourceId);
    if (ptr) {
        source = C_CONTAINER_OF (ptr, CSource, sourceId);
        if (SOURCE_DESTROYED (source)) {
            source = NULL;
        }
    }
    UNLOCK_CONTEXT (context);

    return source;
}

CSource* c_main_context_find_source_by_user_data (CMainContext* context, void* udata)
{
    CSourceIter iter;
    CSource *source;

    if (context == NULL) {
        context = c_main_context_default ();
    }

    LOCK_CONTEXT (context);

    c_source_iter_init (&iter, context, false);
    while (c_source_iter_next (&iter, &source)) {
        if (!SOURCE_DESTROYED (source) && source->callbackFuncs) {
            CSourceFunc callback;
            void* callback_data = NULL;
            source->callbackFuncs->get (source->callbackData, source, &callback, &callback_data);
            if (callback_data == udata) {
                break;
            }
        }
    }
    c_source_iter_clear (&iter);

    UNLOCK_CONTEXT (context);

    return source;
}

CSource* c_main_context_find_source_by_funcs_user_data (CMainContext* context, CSourceFuncs* funcs, void* udata)
{
    CSourceIter iter;
    CSource *source;

    c_return_val_if_fail (funcs != NULL, NULL);

    if (context == NULL) {
        context = c_main_context_default ();
    }

    LOCK_CONTEXT (context);

    c_source_iter_init (&iter, context, false);
    while (c_source_iter_next (&iter, &source)) {
        if (!SOURCE_DESTROYED (source) && source->sourceFuncs == funcs && source->callbackFuncs) {
            CSourceFunc callback;
            void* callback_data;
            source->callbackFuncs->get (source->callbackData, source, &callback, &callback_data);
            if (callback_data == udata) {
                break;
            }
        }
    }
    c_source_iter_clear (&iter);

    UNLOCK_CONTEXT (context);

    return source;
}

void c_main_context_wakeup (CMainContext* context)
{
    if (!context) {
        context = c_main_context_default ();
    }

    c_return_if_fail (c_atomic_int_get (&context->refCount) > 0);

    c_wakeup_signal (context->wakeup);
}

bool c_main_context_acquire (CMainContext* context)
{
    bool result = false;
    CThread* self = C_THREAD_SELF;

    if (context == NULL) {
        context = c_main_context_default ();
    }

    C_LOG_DEBUG_CONSOLE("lock 1");
    LOCK_CONTEXT (context);
    C_LOG_DEBUG_CONSOLE("lock 2");

    if (NULL == context->owner) {
        context->owner = self;
        c_assert(context->ownerCount == 0);
    }

    if (context->owner == self) {
        context->ownerCount++;
        result = true;
    }

    // UNLOCK_CONTEXT (context);

    return result;
}

void c_main_context_release (CMainContext* context)
{
    if (context == NULL) {
        context = c_main_context_default ();
    }

    LOCK_CONTEXT (context);

    context->ownerCount--;
    if (context->ownerCount == 0) {
        context->owner = NULL;
        if (context->waiters) {
            CMainWaiter *waiter = context->waiters->data;
            bool loopInternalWaiter = (waiter->mutex == &context->mutex);
            context->waiters = c_slist_delete_link (context->waiters, context->waiters);
            if (!loopInternalWaiter) {
                c_mutex_lock (waiter->mutex);
            }
            c_cond_signal (waiter->cond);

            if (!loopInternalWaiter) {
                c_mutex_unlock (waiter->mutex);
            }
        }
    }

    UNLOCK_CONTEXT (context);
}

bool c_main_context_is_owner (CMainContext* context)
{
    bool is_owner;

    if (!context) {
        context = c_main_context_default ();
    }

    LOCK_CONTEXT (context);
    is_owner = context->owner == C_THREAD_SELF;
    UNLOCK_CONTEXT (context);

    return is_owner;
}

bool c_main_context_wait (CMainContext* context, CCond* cond, CMutex* mutex)
{
    if (context == NULL) {
        context = c_main_context_default ();
    }

    if (C_UNLIKELY (cond != &context->cond || mutex != &context->mutex)) {
        static bool warned;
        if (!warned) {
            C_LOG_CRIT_CONSOLE("WARNING!! g_main_context_wait() will be removed in a future release. If you see this message, please file a bug immediately.");
            warned = true;
        }
    }

    return c_main_context_wait_internal (context, cond, mutex);
}

bool c_main_context_prepare (CMainContext* context, cint* priority)
{
    C_LOG_DEBUG_CONSOLE("");
    cuint i;
    cint n_ready = 0;
    cint currentPriority = C_MAX_INT32;
    CSource *source;
    CSourceIter iter;

    if (context == NULL) {
        context = c_main_context_default ();
    }

    LOCK_CONTEXT (context);

    context->timeIsFresh = false;

    if (context->inCheckOrPrepare) {
        C_LOG_WARNING_CONSOLE("c_main_context_prepare() called recursively from within a source's check() or prepare() member.");
        UNLOCK_CONTEXT (context);
        return false;
    }

    /* If recursing, clear list of pending dispatches */
    for (i = 0; i < context->pendingDispatches->len; i++) {
        if (context->pendingDispatches->pdata[i]) {
            c_source_unref_internal ((CSource*)context->pendingDispatches->pdata[i], context, true);
        }
    }
    c_ptr_array_set_size (context->pendingDispatches, 0);

    /* Prepare all sources */
    context->timeoutUsec = -1;

    c_source_iter_init (&iter, context, true);
    while (c_source_iter_next (&iter, &source)) {
        cint sourceTimeout = -1;
        if (SOURCE_DESTROYED (source) || SOURCE_BLOCKED (source)) {
            continue;
        }

        if ((n_ready > 0) && (source->priority > currentPriority)) {
            break;
        }

        if (!(source->flags & C_SOURCE_READY)) {
            bool result;
            bool (*prepare) (CSource* source, cint* timeout);
            prepare = source->sourceFuncs->prepare;

            if (prepare) {
                cint64 begin_time_nsec C_UNUSED;
                context->inCheckOrPrepare++;
                UNLOCK_CONTEXT (context);
                result = (*prepare) (source, &sourceTimeout);
                LOCK_CONTEXT (context);
                context->inCheckOrPrepare--;
            }
            else {
                sourceTimeout = -1;
                result = false;
            }

            if (result == false && source->priv->readyTime != -1) {
                if (!context->timeIsFresh) {
                    context->time = c_get_monotonic_time ();
                    context->timeIsFresh = true;
                }

                if (source->priv->readyTime <= context->time) {
                    sourceTimeout = 0;
                    result = true;
                }
                else {
                    cint64 timeout;
                    /* rounding down will lead to spinning, so always round up */
                    timeout = (source->priv->readyTime - context->time + 999) / 1000;
                    if (sourceTimeout < 0 || timeout < sourceTimeout) {
                        sourceTimeout = C_MIN (timeout, C_MAX_INT32);
                    }
                }
            }

            if (result) {
                CSource *readySource = source;
                while (readySource) {
                    readySource->flags |= C_SOURCE_READY;
                    readySource = readySource->priv->parentSource;
                }
            }
        }

        if (source->flags & C_SOURCE_READY) {
            n_ready++;
                currentPriority = source->priority;
            context->timeoutUsec = 0;
        }

        if (sourceTimeout >= 0) {
            if (context->timeoutUsec < 0) {
                context->timeoutUsec = sourceTimeout;
            }
            else {
                context->timeoutUsec = C_MIN (context->timeoutUsec, sourceTimeout);
            }
        }
    }
    c_source_iter_clear (&iter);

    UNLOCK_CONTEXT (context);

    if (priority) {
        *priority = currentPriority;
    }

    return (n_ready > 0);
}

cint c_main_context_query (CMainContext* context, cint maxPriority, cint* timeout_, CPollFD* fds, cint nFds)
{
    cint n_poll;
    CPollRec *pollrec, *lastpollrec;
    cushort events;

    LOCK_CONTEXT (context);

    /* fds is filled sequentially from poll_records. Since poll_records
     * are incrementally sorted by file descriptor identifier, fds will
     * also be incrementally sorted.
     */
    n_poll = 0;
    lastpollrec = NULL;
    for (pollrec = context->pollRecords; pollrec; pollrec = pollrec->next) {
        if (pollrec->priority > maxPriority) {
            continue;
        }

        /* In direct contradiction to the Unix98 spec, IRIX runs into
         * difficulty if you pass in POLLERR, POLLHUP or POLLNVAL
         * flags in the events field of the pollfd while it should
         * just ignoring them. So we mask them out here.
         */
        events = pollrec->fd->events & ~(C_IO_ERR|C_IO_HUP|C_IO_NVAL);

        /* This optimization --using the same GPollFD to poll for more
         * than one poll record-- relies on the poll records being
         * incrementally sorted.
         */
        if (lastpollrec && pollrec->fd->fd == lastpollrec->fd->fd) {
            if (n_poll - 1 < nFds) {
                fds[n_poll - 1].events |= events;
            }
        }
        else {
            if (n_poll < nFds) {
                fds[n_poll].fd = pollrec->fd->fd;
                fds[n_poll].events = events;
                fds[n_poll].rEvents = 0;
            }

            n_poll++;
        }
        lastpollrec = pollrec;
    }

    context->pollChanged = false;

    if (timeout_) {
        *timeout_ = context->timeoutUsec;
        if (*timeout_ != 0) {
            context->timeIsFresh = false;
        }
    }

    UNLOCK_CONTEXT (context);

    return n_poll;
}

bool c_main_context_check (CMainContext* context, cint maxPriority, CPollFD* fds, cint nFds)
{
    bool ready;

    LOCK_CONTEXT (context);

    ready = c_main_context_check_unlocked (context, maxPriority, fds, nFds);

    UNLOCK_CONTEXT (context);

    return ready;
}

void c_main_context_dispatch (CMainContext* context)
{
    LOCK_CONTEXT (context);

    if (context->pendingDispatches->len > 0) {
        c_main_dispatch (context);
    }

    UNLOCK_CONTEXT (context);
}

void c_main_context_set_poll_func (CMainContext* context, CPollFunc func)
{
    if (!context) {
        context = c_main_context_default ();
    }

    c_return_if_fail (c_atomic_int_get (&context->refCount) > 0);

    LOCK_CONTEXT (context);

    if (func) {
        context->pollFunc = func;
    }
    else {
        context->pollFunc = c_poll;
    }

    UNLOCK_CONTEXT (context);
}

CPollFunc c_main_context_get_poll_func (CMainContext* context)
{
    CPollFunc result;

    if (!context) {
        context = c_main_context_default ();
    }

    c_return_val_if_fail (c_atomic_int_get (&context->refCount) > 0, NULL);

    LOCK_CONTEXT (context);
    result = context->pollFunc;
    UNLOCK_CONTEXT (context);

    return result;
}

void c_main_context_add_poll (CMainContext* context, CPollFD* fd, cint priority)
{
    if (!context) {
        context = c_main_context_default ();
    }

    c_return_if_fail (c_atomic_int_get (&context->refCount) > 0);
    c_return_if_fail (fd);

    LOCK_CONTEXT (context);
    c_main_context_add_poll_unlocked (context, priority, fd);
    UNLOCK_CONTEXT (context);
}

void c_main_context_remove_poll (CMainContext* context, CPollFD* fd)
{
    if (!context) {
        context = c_main_context_default ();
    }

    c_return_if_fail (c_atomic_int_get (&context->refCount) > 0);
    c_return_if_fail (fd);

    LOCK_CONTEXT (context);
    c_main_context_remove_poll_unlocked (context, fd);
    UNLOCK_CONTEXT (context);
}

cint c_main_depth (void)
{
    CMainDispatch *dispatch = get_dispatch ();
    return dispatch->depth;
}

CSource* c_main_current_source (void)
{
    CMainDispatch *dispatch = get_dispatch ();
    return dispatch->source;
}

void c_main_context_push_thread_default (CMainContext* context)
{
    CQueue *stack;
    bool acquired_context;

    acquired_context = c_main_context_acquire (context);
    c_return_if_fail (acquired_context);

    if (context == c_main_context_default ()) {
        context = NULL;
    }
    else if (context) {
        c_main_context_ref (context);
    }

    stack = c_private_get (&gsThreadContextStack);
    if (!stack) {
        stack = c_queue_new ();
        c_private_set (&gsThreadContextStack, stack);
    }

    c_queue_push_head (stack, context);
}

void c_main_context_pop_thread_default (CMainContext* context)
{
    CQueue *stack;

    if (context == c_main_context_default ()) {
        context = NULL;
    }

    stack = c_private_get (&gsThreadContextStack);

    c_return_if_fail (stack != NULL);
    c_return_if_fail (c_queue_peek_head (stack) == context);

    c_queue_pop_head (stack);
    c_main_context_release (context);
    if (context) {
        c_main_context_unref (context);
    }
}

CMainContext* c_main_context_get_thread_default (void)
{
    CQueue* stack = c_private_get (&gsThreadContextStack);
    if (stack) {
        return c_queue_peek_head (stack);
    }
    else {
        return NULL;
    }
}

CMainContext* c_main_context_ref_thread_default (void)
{
    CMainContext *context;

    context = c_main_context_get_thread_default ();
    if (!context) {
        context = c_main_context_default ();
    }

    return c_main_context_ref (context);
}

CSource* c_child_watch_source_new (CPid pid)
{
    C_LOG_DEBUG_CONSOLE("");
    CSource* source;
    CChildWatchSource *child_watch_source;

#ifdef SYS_pidfd_open
    int errsv;
#endif

    c_return_val_if_fail (pid > 0, NULL);

    source = c_source_new (&c_child_watch_funcs, sizeof (CChildWatchSource));
    child_watch_source = (CChildWatchSource *)source;

    c_source_set_static_name (source, "CChildWatchSource");

    child_watch_source->pid = pid;

#ifdef SYS_pidfd_open
    child_watch_source->poll.fd = (int) syscall (SYS_pidfd_open, pid, 0);
    errsv = errno;
    if (child_watch_source->poll.fd >= 0) {
        child_watch_source->usingPidFd = true;
        child_watch_source->poll.events = C_IO_IN;
        c_source_add_poll (source, &child_watch_source->poll);
        return source;
    }
    else {
        C_LOG_DEBUG_CONSOLE("pidfd_open(%ul) failed with error: %s", pid, c_strerror (errsv));
    }
#else
    // FIXME://
    child_watch_source->poll.fd = -1;
    child_watch_source->poll.fd = (gintptr) pid;
    child_watch_source->poll.events = G_IO_IN;
    c_source_add_poll (source, &child_watch_source->poll);
#endif

    C_LOCK (gsUnixSignalLock);
    ref_unix_signal_handler_unlocked (SIGCHLD);
    gsUnixChildWatches = c_slist_prepend (gsUnixChildWatches, child_watch_source);
    if (waitpid (pid, &child_watch_source->childStatus, WNOHANG) > 0) {
        child_watch_source->childExited = true;
    }
    C_UNLOCK (gsUnixSignalLock);

    return source;
}

cuint c_child_watch_add_full (cint priority, CPid pid, CChildWatchFunc function, void* data, CDestroyNotify notify)
{
    cuint id;

    c_return_val_if_fail (function != NULL, 0);
    c_return_val_if_fail (pid > 0, 0);

    CSource* source = c_child_watch_source_new (pid);

    if (priority != C_PRIORITY_DEFAULT) {
        c_source_set_priority (source, priority);
    }

    c_source_set_callback (source, (CSourceFunc) function, data, notify);
    id = c_source_attach (source, NULL);
    c_source_unref (source);

    return id;
}

cuint c_child_watch_add (CPid pid, CChildWatchFunc function, void* data)
{
    return c_child_watch_add_full (C_PRIORITY_DEFAULT, pid, function, data, NULL);
}

CSource* c_idle_source_new (void)
{
    return idle_source_new (false);
}

cuint c_idle_add_full (cint priority, CSourceFunc function, void* data, CDestroyNotify notify)
{
    return idle_add_full (priority, false, function, data, notify);
}

cuint c_idle_add (CSourceFunc function, void* data)
{
    return c_idle_add_full (C_PRIORITY_DEFAULT_IDLE, function, data, NULL);
}

cuint c_idle_add_once (CSourceOnceFunc function, void* data)
{
    return idle_add_full (C_PRIORITY_DEFAULT_IDLE, true, (CSourceFunc) function, data, NULL);
}

bool c_idle_remove_by_data (void* data)
{
    return c_source_remove_by_funcs_user_data (&c_idle_funcs, data);
}

void c_main_context_invoke (CMainContext* context, CSourceFunc function, void* data)
{
    c_main_context_invoke_full (context, C_PRIORITY_DEFAULT, function, data, NULL);
}

void c_main_context_invoke_full (CMainContext* context, cint priority, CSourceFunc function, void* data, CDestroyNotify notify)
{
    c_return_if_fail (function != NULL);

    if (!context) {
        context = c_main_context_default ();
    }

    if (c_main_context_is_owner (context)) {
        while (function (data));
        if (notify != NULL) {
            notify (data);
        }
    }
    else {
        CMainContext *thread_default = c_main_context_get_thread_default ();
        if (!thread_default) {
            thread_default = c_main_context_default ();
        }

        if (thread_default == context && c_main_context_acquire (context)) {
            while (function (data));
            c_main_context_release (context);
            if (notify != NULL) {
                notify (data);
            }
        }
        else {
            CSource *source = c_idle_source_new ();
            c_source_set_priority (source, priority);
            c_source_set_callback (source, function, data, notify);
            c_source_attach (source, context);
            c_source_unref (source);
        }
    }
}

CMainLoop* c_main_loop_new(CMainContext *context, bool isRunning)
{
    CMainLoop *loop;

    if (!context) {
        context = c_main_context_default();
    }

    c_main_context_ref (context);

    loop = c_malloc0(sizeof(CMainLoop));
    loop->context = context;
    loop->isRunning = isRunning != false;
    loop->refCount = 1;

    return loop;
}

void c_main_loop_run(CMainLoop *loop)
{
    CThread *self = C_THREAD_SELF;

    c_return_if_fail (loop != NULL);
    c_return_if_fail (c_atomic_int_get (&loop->refCount) > 0);

    /* Hold a reference in case the loop is unreffed from a callback function */
    c_atomic_int_inc (&loop->refCount);

    C_LOG_DEBUG_CONSOLE("loop run");
    if (!c_main_context_acquire (loop->context)) {
        bool gotOwnership = false;

        /* Another thread owns this context */
        LOCK_CONTEXT (loop->context);
        c_atomic_bool_set (&loop->isRunning, true);

        while (c_atomic_bool_get (&loop->isRunning) && !gotOwnership) {
            gotOwnership = c_main_context_wait_internal (loop->context, &loop->context->cond, &loop->context->mutex);
        }

        if (!c_atomic_bool_get (&loop->isRunning)) {
            UNLOCK_CONTEXT (loop->context);
            if (gotOwnership) {
                c_main_context_release (loop->context);
            }
            c_main_loop_unref (loop);
            return;
        }
        c_assert (gotOwnership);
    }
    else {
        LOCK_CONTEXT (loop->context);
    }

    if (loop->context->inCheckOrPrepare) {
        C_LOG_WARNING_CONSOLE("c_main_loop_run(): called recursively from within a source's check() or prepare() member, iteration not possible.");
        c_main_loop_unref (loop);
        return;
    }

    C_LOG_DEBUG_CONSOLE("2");
    c_atomic_bool_set (&loop->isRunning, true);
    while (c_atomic_bool_get (&loop->isRunning)) {
        c_main_context_iterate (loop->context, true, true, self);
    }

    UNLOCK_CONTEXT (loop->context);

    c_main_context_release (loop->context);

    c_main_loop_unref (loop);
}

void c_main_loop_quit(CMainLoop *loop)
{
    c_return_if_fail (loop != NULL);
    c_return_if_fail (c_atomic_int_get (&loop->refCount) > 0);

    LOCK_CONTEXT (loop->context);
    c_atomic_bool_set (&loop->isRunning, false);
    c_wakeup_signal (loop->context->wakeup);

    c_cond_broadcast (&loop->context->cond);

    UNLOCK_CONTEXT (loop->context);
}

CMainLoop *c_main_loop_ref(CMainLoop *loop)
{
    c_return_val_if_fail (loop != NULL, NULL);
    c_return_val_if_fail (c_atomic_int_get (&loop->refCount) > 0, NULL);

    c_atomic_int_inc (&loop->refCount);

    return loop;
}

void c_main_loop_unref(CMainLoop *loop)
{
    c_return_if_fail (loop != NULL);
    c_return_if_fail (c_atomic_int_get (&loop->refCount) > 0);

    if (!c_atomic_int_dec_and_test (&loop->refCount)) {
        return;
    }

    c_main_context_unref (loop->context);
    c_free (loop);
}

bool c_main_loop_is_running(CMainLoop *loop)
{
    c_return_val_if_fail (loop != NULL, false);
    c_return_val_if_fail (c_atomic_int_get (&loop->refCount) > 0, false);

    return c_atomic_bool_get (&loop->isRunning);
}

CMainContext *c_main_loop_get_context(CMainLoop *loop)
{
    c_return_val_if_fail (loop != NULL, NULL);
    c_return_val_if_fail (c_atomic_int_get (&loop->refCount) > 0, NULL);

    return loop->context;
}

CSource *c_source_new(CSourceFuncs *sourceFuncs, cuint structSize)
{
    CSource *source;

    c_return_val_if_fail (sourceFuncs != NULL, NULL);
    c_return_val_if_fail (structSize >= sizeof (CSource), NULL);

    source = (CSource*) c_malloc0 (structSize);
    source->priv = c_malloc0(sizeof(CSourcePrivate));
    source->sourceFuncs = sourceFuncs;
    source->refCount = 1;

    source->priority = C_PRIORITY_DEFAULT;
    source->flags = C_HOOK_FLAG_ACTIVE;

    source->priv->readyTime = -1;

    return source;
}

void c_source_set_dispose_function(CSource *source, CSourceDisposeFunc dispose)
{
    c_return_if_fail (source != NULL);
    c_return_if_fail (source->priv->dispose == NULL);
    c_return_if_fail (c_atomic_int_get ((cint*) &(source->refCount)) > 0);
    source->priv->dispose = dispose;
}

CSource *c_source_ref(CSource *source)
{
    c_return_val_if_fail (source != NULL, NULL);
    c_return_val_if_fail (c_atomic_int_get ((cint*) &source->refCount) >= 0, NULL);

    c_atomic_int_inc ((cint*)&source->refCount);

    return source;
}

void c_source_unref(CSource *source)
{
    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);

    c_source_unref_internal (source, source->context, false);
}

cuint c_source_attach(CSource *source, CMainContext *context)
{
    cuint result = 0;

    c_return_val_if_fail (source != NULL, 0);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, 0);
    c_return_val_if_fail (source->context == NULL, 0);
    c_return_val_if_fail (!SOURCE_DESTROYED (source), 0);

    if (!context) {
        context = c_main_context_default ();
    }

    LOCK_CONTEXT (context);

    result = c_source_attach_unlocked (source, context, true);

    UNLOCK_CONTEXT (context);

    return result;
}

void c_source_destroy(CSource *source)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);

    context = source->context;

    if (context) {
        c_source_destroy_internal (source, context, false);
    }
    else {
        source->flags &= ~C_HOOK_FLAG_ACTIVE;
    }
}

void c_source_set_priority(CSource *source, cint priority)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (source->priv->parentSource == NULL);

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }
    c_source_set_priority_unlocked (source, context, priority);
    if (context) {
        UNLOCK_CONTEXT (context);
    }
}

cint c_source_get_priority(CSource *source)
{
    c_return_val_if_fail (source != NULL, 0);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, 0);

    return source->priority;
}

void c_source_set_can_recurse(CSource *source, bool canRecurse)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get((cint*)&source->refCount) > 0);

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    if (canRecurse) {
        source->flags |= C_SOURCE_CAN_RECURSE;
    }
    else {
        source->flags &= ~C_SOURCE_CAN_RECURSE;
    }

    if (context) {
        UNLOCK_CONTEXT (context);
    }
}

bool c_source_get_can_recurse(CSource *source)
{
    c_return_val_if_fail (source != NULL, false);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, false);

    return (source->flags & C_SOURCE_CAN_RECURSE) != 0;
}

cuint c_source_get_id(CSource *source)
{
    cuint result;

    c_return_val_if_fail (source != NULL, 0);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, 0);
    c_return_val_if_fail (source->context != NULL, 0);

    LOCK_CONTEXT (source->context);
    result = source->sourceId;
    UNLOCK_CONTEXT (source->context);

    return result;
}

CMainContext *c_source_get_context(CSource *source)
{
    c_return_val_if_fail (source != NULL, NULL);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, NULL);
    c_return_val_if_fail (source->context != NULL || !SOURCE_DESTROYED (source), NULL);

    return source->context;
}

void c_source_set_callback(CSource *source, CSourceFunc func, void *data, CDestroyNotify notify)
{
    CSourceCallback *new_callback;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);

    new_callback = c_malloc0(sizeof(CSourceCallback));

    new_callback->refCount = 1;
    new_callback->func = func;
    new_callback->data = data;
    new_callback->notify = notify;

    c_source_set_callback_indirect (source, new_callback, &gsSourceCallbackFuncs);
}

void c_source_set_funcs(CSource *source, CSourceFuncs *funcs)
{
    c_return_if_fail (source != NULL);
    c_return_if_fail (source->context == NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (funcs != NULL);

    source->sourceFuncs = funcs;
}

bool c_source_is_destroyed(CSource *source)
{
    c_return_val_if_fail (source != NULL, true);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, true);

    return SOURCE_DESTROYED (source);
}

void c_source_set_name(CSource *source, const char *name)
{
    c_source_set_name_full (source, name, false);
}

void c_source_set_static_name(CSource *source, const char *name)
{
    c_source_set_name_full (source, name, true);
}

const char *c_source_get_name(CSource *source)
{
    c_return_val_if_fail (source != NULL, NULL);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, NULL);

    return source->name;
}

void c_source_set_name_by_id(cuint tag, const char *name)
{
    CSource *source;

    c_return_if_fail (tag > 0);

    source = c_main_context_find_source_by_id (NULL, tag);
    if (source == NULL) {
        return;
    }

    c_source_set_name (source, name);
}

void c_source_set_ready_time(CSource *source, cint64 readyTime)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*) &source->refCount) > 0);

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    if (source->priv->readyTime == readyTime) {
        if (context) {
            UNLOCK_CONTEXT (context);
        }

        return;
    }

    source->priv->readyTime = readyTime;

    if (context) {
        if (!SOURCE_BLOCKED (source)) {
            c_wakeup_signal (context->wakeup);
        }
        UNLOCK_CONTEXT (context);
    }
}

cint64 c_source_get_ready_time(CSource *source)
{
    c_return_val_if_fail (source != NULL, -1);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, -1);

    return source->priv->readyTime;
}

void *c_source_add_unix_fd(CSource *source, cint fd, CIOCondition events)
{
    CMainContext *context;
    CPollFD *poll_fd;

    c_return_val_if_fail (source != NULL, NULL);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, NULL);
    c_return_val_if_fail (!SOURCE_DESTROYED (source), NULL);

    poll_fd = c_malloc0(sizeof(CPollFD));
    poll_fd->fd = fd;
    poll_fd->events = events;
    poll_fd->rEvents = 0;

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    source->priv->fds = c_slist_prepend (source->priv->fds, poll_fd);
    if (context) {
        if (!SOURCE_BLOCKED (source)) {
            c_main_context_add_poll_unlocked (context, source->priority, poll_fd);
        }
        UNLOCK_CONTEXT (context);
    }

    return poll_fd;
}

void c_source_modify_unix_fd(CSource *source, void *tag, CIOCondition newEvents)
{
    CMainContext *context;
    CPollFD *poll_fd;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (c_slist_find (source->priv->fds, tag));

    context = source->context;
    poll_fd = tag;

    poll_fd->events = newEvents;

    if (context) {
        c_main_context_wakeup (context);
    }
}

void c_source_remove_unix_fd(CSource *source, void *tag)
{
    CMainContext *context;
    CPollFD *poll_fd;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (c_slist_find (source->priv->fds, tag));

    context = source->context;
    poll_fd = tag;

    if (context) {
        LOCK_CONTEXT (context);
    }

    source->priv->fds = c_slist_remove (source->priv->fds, poll_fd);

    if (context) {
        if (!SOURCE_BLOCKED (source)) {
            c_main_context_remove_poll_unlocked (context, poll_fd);
        }

        UNLOCK_CONTEXT (context);
    }

    c_free (poll_fd);
}

CIOCondition c_source_query_unix_fd(CSource *source, void *tag)
{
    CPollFD *poll_fd;

    c_return_val_if_fail (source != NULL, 0);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, 0);
    c_return_val_if_fail (c_slist_find (source->priv->fds, tag), 0);

    poll_fd = tag;

    return poll_fd->rEvents;
}

void c_source_set_callback_indirect(CSource *source, void *callbackData, CSourceCallbackFuncs *callbackFuncs)
{
    CMainContext *context;
    void* old_cb_data;
    CSourceCallbackFuncs *old_cb_funcs;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (callbackFuncs != NULL || callbackData == NULL);

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    if (callbackFuncs != &gsSourceCallbackFuncs) {
//        TRACE (GLIB_SOURCE_SET_CALLBACK_INDIRECT (source, callback_data,
//                                                  callback_funcs->ref,
//                                                  callback_funcs->unref,
//                                                  callback_funcs->get));
    }

    old_cb_data = source->callbackData;
    old_cb_funcs = source->callbackFuncs;

    source->callbackData = callbackData;
    source->callbackFuncs = callbackFuncs;

    if (context) {
        UNLOCK_CONTEXT (context);
    }

    if (old_cb_funcs) {
        old_cb_funcs->unref (old_cb_data);
    }
}

void c_source_add_poll(CSource *source, CPollFD *fd)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (fd != NULL);
    c_return_if_fail (!SOURCE_DESTROYED (source));

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    source->pollFds = c_slist_prepend (source->pollFds, fd);

    if (context) {
        if (!SOURCE_BLOCKED (source)) {
            c_main_context_add_poll_unlocked (context, source->priority, fd);
        }
        UNLOCK_CONTEXT (context);
    }
}

void c_source_remove_poll(CSource *source, CPollFD *fd)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (fd != NULL);
    c_return_if_fail (!SOURCE_DESTROYED (source));

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    source->pollFds = c_slist_remove (source->pollFds, fd);

    if (context) {
        if (!SOURCE_BLOCKED (source)) {
            c_main_context_remove_poll_unlocked (context, fd);
        }
        UNLOCK_CONTEXT (context);
    }
}

void c_source_add_child_source(CSource *source, CSource *childSource)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (childSource != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&childSource->refCount) > 0);
    c_return_if_fail (!SOURCE_DESTROYED (source));
    c_return_if_fail (!SOURCE_DESTROYED (childSource));
    c_return_if_fail (childSource->context == NULL);
    c_return_if_fail (childSource->priv->parentSource == NULL);

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

//    TRACE (GLIB_SOURCE_ADD_CHILD_SOURCE (source, child_source));

    source->priv->childSources = c_slist_prepend (source->priv->childSources, c_source_ref (childSource));
    childSource->priv->parentSource = source;
    c_source_set_priority_unlocked (childSource, NULL, source->priority);
    if (SOURCE_BLOCKED (source)) {
        block_source (childSource);
    }

    if (context) {
        c_source_attach_unlocked (childSource, context, true);
        UNLOCK_CONTEXT (context);
    }
}

void c_source_remove_child_source(CSource *source, CSource *childSource)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);
    c_return_if_fail (childSource != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&childSource->refCount) > 0);
    c_return_if_fail (childSource->priv->parentSource == source);
    c_return_if_fail (!SOURCE_DESTROYED (source));
    c_return_if_fail (!SOURCE_DESTROYED (childSource));

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    c_child_source_remove_internal (childSource, context);

    if (context) {
        UNLOCK_CONTEXT (context);
    }
}

void c_source_get_current_time(CSource *source, CTimeVal *timeval)
{
    c_get_current_time (timeval);
}

cint64 c_source_get_time(CSource *source)
{
    CMainContext *context;
    cint64 result;

    c_return_val_if_fail (source != NULL, 0);
    c_return_val_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0, 0);
    c_return_val_if_fail (source->context != NULL, 0);

    context = source->context;

    LOCK_CONTEXT (context);

    if (!context->timeIsFresh) {
        context->time = c_get_monotonic_time ();
        context->timeIsFresh = true;
    }

    result = context->time;

    UNLOCK_CONTEXT (context);

    return result;
}

CSource *c_timeout_source_new(cuint interval)
{
    return timeout_source_new (interval, false, false);
}

CSource *c_timeout_source_new_seconds(cuint interval)
{
    return timeout_source_new (interval, true, false);
}

void c_get_current_time(CTimeVal *result)
{
    cint64 tv;

    c_return_if_fail (result != NULL);

    tv = c_get_real_time ();

    result->tvSec = tv / 1000000;
    result->tvUsec = tv % 1000000;
}

bool c_source_remove(cuint tag)
{
    CSource *source;

    c_return_val_if_fail (tag > 0, false);

    source = c_main_context_find_source_by_id (NULL, tag);
    if (source) {
        c_source_destroy (source);
    }
    else {
        C_LOG_CRIT_CONSOLE("Source ID %u was not found when attempting to remove it", tag);
    }

    return source != NULL;
}

bool c_source_remove_by_user_data(void *udata)
{
    CSource *source;

    source = c_main_context_find_source_by_user_data (NULL, udata);
    if (source) {
        c_source_destroy (source);
        return true;
    }
    else {
        return false;
    }
}

bool c_source_remove_by_funcs_user_data(CSourceFuncs *funcs, void *udata)
{
    CSource *source;

    c_return_val_if_fail (funcs != NULL, false);

    source = c_main_context_find_source_by_funcs_user_data (NULL, funcs, udata);
    if (source) {
        c_source_destroy (source);
        return true;
    }
    else {
        return false;
    }
}

cuint c_timeout_add_full(cint priority, cuint interval, CSourceFunc function, void *data, CDestroyNotify notify)
{
    return timeout_add_full (priority, interval, false, false, function, data, notify);
}

cuint c_timeout_add(cuint interval, CSourceFunc function, void *data)
{
    return c_timeout_add_full (C_PRIORITY_DEFAULT, interval, function, data, NULL);
}

cuint c_timeout_add_once(cuint interval, CSourceOnceFunc function, void *data)
{
    return timeout_add_full (C_PRIORITY_DEFAULT, interval, false, true, (CSourceFunc) function, data, NULL);
}

cuint c_timeout_add_seconds_full(cint priority, cuint interval, CSourceFunc function, void *data, CDestroyNotify notify)
{
    CSource *source;
    cuint id;

    c_return_val_if_fail (function != NULL, 0);

    source = c_timeout_source_new_seconds (interval);

    if (priority != C_PRIORITY_DEFAULT) {
        c_source_set_priority (source, priority);
    }

    c_source_set_callback (source, function, data, notify);
    id = c_source_attach (source, NULL);
    c_source_unref (source);

    return id;
}

cuint c_timeout_add_seconds(cuint interval, CSourceFunc function, void *data)
{
    c_return_val_if_fail (function != NULL, 0);

    return c_timeout_add_seconds_full (C_PRIORITY_DEFAULT, interval, function, data, NULL);
}

CMainContext* c_get_worker_context (void)
{
    static csize initialised;

    if (c_once_init_enter (&initialised)) {
        sigset_t prevMask;
        sigset_t all;

        sigfillset (&all);
        pthread_sigmask (SIG_SETMASK, &all, &prevMask);
        gsClibWorkerContext = c_main_context_new ();
        c_thread_new ("cmain", clib_worker_main, NULL);
        pthread_sigmask (SIG_SETMASK, &prevMask, NULL);
        c_once_init_leave (&initialised, true);
    }

    return gsClibWorkerContext;
}





static inline void poll_rec_list_free (CMainContext* context, CPollRec* list)
{
    // FIXME:// 实现
//    c_slice_free_chain (CPollRec, list, next);
}

static bool c_unix_set_error_from_errno (CError** error, int savedErrno)
{
    c_set_error_literal (error, C_UNIX_ERROR, 0, c_strerror (savedErrno));
    errno = savedErrno;

    return false;
}

static void unref_unix_signal_handler_unlocked (int signum)
{
    gsUnixSignalRefcount[signum]--;
    if (gsUnixSignalRefcount[signum] == 0) {
        struct sigaction action;
        memset (&action, 0, sizeof (action));
        action.sa_handler = SIG_DFL;
        sigemptyset (&action.sa_mask);
        sigaction (signum, &action, NULL);
    }
}

static void ref_unix_signal_handler_unlocked (int signum)
{
    C_LOG_DEBUG_CONSOLE("");
    /* Ensure we have the worker context */
    c_get_worker_context ();
    gsUnixSignalRefcount[signum]++;
    if (gsUnixSignalRefcount[signum] == 1) {
        struct sigaction action;
        action.sa_handler = c_unix_signal_handler;
        sigemptyset (&action.sa_mask);
#ifdef SA_RESTART
        action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
#else
        action.sa_flags = SA_NOCLDSTOP;
#endif
        sigaction (signum, &action, NULL);
    }
}

static const char* signum_to_string (int signum)
{
    /* See `man 0P signal.h` */
#define SIGNAL(s) \
    case (s): \
        return ("CUnixSignalSource: " #s);
    switch (signum) {
        /* These signals are guaranteed to exist by POSIX. */
        SIGNAL (SIGABRT)
        SIGNAL (SIGFPE)
        SIGNAL (SIGILL)
        SIGNAL (SIGINT)
        SIGNAL (SIGSEGV)
        SIGNAL (SIGTERM)
        /* Frustratingly, these are not, and hence for brevity the list is
         * incomplete. */
#ifdef SIGALRM
        SIGNAL (SIGALRM)
#endif
#ifdef SIGCHLD
        SIGNAL (SIGCHLD)
#endif
#ifdef SIGHUP
        SIGNAL (SIGHUP)
#endif
#ifdef SIGKILL
        SIGNAL (SIGKILL)
#endif
#ifdef SIGPIPE
        SIGNAL (SIGPIPE)
#endif
#ifdef SIGQUIT
        SIGNAL (SIGQUIT)
#endif
#ifdef SIGSTOP
        SIGNAL (SIGSTOP)
#endif
#ifdef SIGUSR1
        SIGNAL (SIGUSR1)
#endif
#ifdef SIGUSR2
        SIGNAL (SIGUSR2)
#endif
#ifdef SIGPOLL
        SIGNAL (SIGPOLL)
#endif
#ifdef SIGPROF
        SIGNAL (SIGPROF)
#endif
#ifdef SIGTRAP
        SIGNAL (SIGTRAP)
#endif
        default:
            return "CUnixSignalSource: Unrecognized signal";
    }
#undef SIGNAL
}

static bool c_unix_signal_watch_dispatch (CSource* source, CSourceFunc callback, void* udata)
{
    bool again;

    CUnixSignalWatchSource* unixSignalSource = (CUnixSignalWatchSource*) source;

    if (!callback) {
        C_LOG_WARNING_CONSOLE("Unix signal source dispatched without callback. You must call g_source_set_callback().");
        return false;
    }

    c_atomic_bool_set (&unixSignalSource->pending, false);

    again = (callback) (udata);

    return again;
}

static bool c_unix_signal_watch_check (CSource* source)
{
    CUnixSignalWatchSource* unixSignalSource = (CUnixSignalWatchSource*) source;

    return c_atomic_bool_get (&unixSignalSource->pending);
}

static bool c_unix_signal_watch_prepare (CSource* source, cint* timeout)
{
    CUnixSignalWatchSource *unixSignalSource = (CUnixSignalWatchSource*) source;

    return c_atomic_bool_get (&unixSignalSource->pending);
}

static bool c_child_watch_check (CSource* source)
{
    CChildWatchSource* childWatchSource = (CChildWatchSource*) source;

    if (childWatchSource->usingPidFd) {
        bool child_exited = childWatchSource->poll.rEvents & C_IO_IN;

        if (child_exited) {
            siginfo_t child_info = { 0, };
            if (waitid (P_PIDFD, childWatchSource->poll.fd, &child_info, WEXITED | WNOHANG) >= 0 && child_info.si_pid != 0) {
                childWatchSource->childStatus = siginfo_t_to_wait_status (&child_info);
                childWatchSource->childExited = true;
            }
        }
        return child_exited;
    }

    return c_atomic_bool_get (&childWatchSource->childExited);
}

static void free_context (void* data)
{
    CMainContext *context = data;

    c_main_context_release (context);
    if (context) {
        c_main_context_unref (context);
    }
}

static int siginfo_t_to_wait_status (const siginfo_t *info)
{
    switch (info->si_code) {
        case CLD_EXITED:
            return W_EXITCODE (info->si_status, 0);
        case CLD_KILLED:
            return W_EXITCODE (0, info->si_status);
        case CLD_DUMPED:
#ifdef WCOREFLAG
            return W_EXITCODE (0, info->si_status | WCOREFLAG);
#else
            c_assert_not_reached ();
#endif
        case CLD_CONTINUED:
#ifdef __W_CONTINUED
            return __W_CONTINUED;
#else
            c_assert_not_reached ();
#endif
        case CLD_STOPPED:
        case CLD_TRAPPED:
        default: {
            return W_STOPCODE (info->si_status);
        }
    }
}

static void c_unix_signal_watch_finalize (CSource* source)
{
    CUnixSignalWatchSource* unix_signal_source = (CUnixSignalWatchSource*) source;

    C_LOCK (gsUnixSignalLock);
    unref_unix_signal_handler_unlocked (unix_signal_source->signum);
    gsUnixSignalWatches = c_slist_remove (gsUnixSignalWatches, source);
    C_UNLOCK (gsUnixSignalLock);
}

static void c_child_watch_finalize (CSource* source)
{
    CChildWatchSource *child_watch_source = (CChildWatchSource*) source;

    if (child_watch_source->usingPidFd) {
        if (child_watch_source->poll.fd >= 0) {
            close (child_watch_source->poll.fd);
        }
        return;
    }

    C_LOCK (gsUnixSignalLock);
    gsUnixSignalWatches = c_slist_remove (gsUnixSignalWatches, source);
    unref_unix_signal_handler_unlocked (SIGCHLD);
    C_UNLOCK (gsUnixSignalLock);
}

static bool c_child_watch_dispatch (CSource* source, CSourceFunc callback, void* udata)
{
    CChildWatchSource *child_watch_source;
    CChildWatchFunc child_watch_callback = (CChildWatchFunc) callback;

    child_watch_source = (CChildWatchSource*) source;

    if (!callback) {
        C_LOG_WARNING_CONSOLE("Child watch source dispatched without callback. You must call g_source_set_callback().");
        return false;
    }

    (child_watch_callback) (child_watch_source->pid, child_watch_source->childStatus, udata);

    return false;
}

static void c_unix_signal_handler (int signum)
{
    cint saved_errno = errno;

    c_atomic_int_set (&gsUnixSignalPending[signum], 1);
    c_atomic_int_set (&gsAnyUnixSignalPending, 1);

    c_wakeup_signal (gsClibWorkerContext->wakeup);

    errno = saved_errno;
}

static bool c_idle_prepare  (CSource* source, cint* timeout)
{
    *timeout = 0;

    return true;
}

static bool c_idle_check (CSource* C_UNUSED source)
{
    return true;
}

static bool c_idle_dispatch (CSource* source, CSourceFunc callback, void* udata)
{
    CIdleSource *idle_source = (CIdleSource*) source;
    bool again;

    if (!callback) {
        C_LOG_WARNING_CONSOLE("Idle source dispatched without callback. You must call g_source_set_callback().");
        return false;
    }

    if (idle_source->oneShot) {
        CSourceOnceFunc once_callback = (CSourceOnceFunc) callback;
        once_callback (udata);
        again = C_SOURCE_REMOVE;
    }
    else {
        again = callback (udata);
    }

    return again;
}

static CSource* idle_source_new (bool oneShot)
{
    CSource *source;
    CIdleSource *idle_source;

    source = c_source_new (&c_idle_funcs, sizeof (CIdleSource));
    idle_source = (CIdleSource *) source;

    idle_source->oneShot = oneShot;

    c_source_set_priority (source, C_PRIORITY_DEFAULT_IDLE);

    c_source_set_static_name (source, "CIdleSource");

    return source;
}

static cuint idle_add_full (cint priority, bool oneShot, CSourceFunc function, void* data, CDestroyNotify notify)
{
    cuint id;

    c_return_val_if_fail (function != NULL, 0);

    CSource *source = idle_source_new (oneShot);

    if (priority != C_PRIORITY_DEFAULT_IDLE) {
        c_source_set_priority (source, priority);
    }

    c_source_set_callback (source, function, data, notify);
    id = c_source_attach (source, NULL);

    c_source_unref (source);

    return id;
}

static void* clib_worker_main (void* data)
{
    while (true) {
        c_main_context_iteration (gsClibWorkerContext, true);
        if (c_atomic_int_get (&gsAnyUnixSignalPending)) {
            dispatch_unix_signals ();
        }
    }

    return NULL; /* worst GCC warning message ever... */
}

static void dispatch_unix_signals_unlocked (void)
{
    bool pending[NSIG];
    CSList *node;
    cint i;

    c_atomic_int_set (&gsAnyUnixSignalPending, 0);
    for (i = 0; i < NSIG; i++) {
        pending[i] = c_atomic_int_compare_and_exchange (&gsUnixSignalPending[i], 1, 0);
    }

    if (pending[SIGCHLD]) {
        for (node = gsUnixChildWatches; node; node = node->next) {
            CChildWatchSource *source = node->data;
            if (!source->usingPidFd&& !c_atomic_bool_get (&source->childExited)) {
                pid_t pid;
                do {
                    c_assert (source->pid > 0);
                    pid = waitpid (source->pid, &source->childStatus, WNOHANG);
                    if (pid > 0) {
                        c_atomic_bool_set (&source->childExited, true);
                        wake_source ((CSource*) source);
                    }
                    else if (pid == -1 && errno == ECHILD) {
                        C_LOG_WARNING_CONSOLE("CChildWatchSource: Exit status of a child process was requested but ECHILD was received by waitpid(). See the documentation of g_child_watch_source_new() for possible causes.");
                        source->childStatus = 0;
                        c_atomic_bool_set (&source->childExited, true);
                        wake_source ((CSource *) source);
                    }
                }
                while (pid == -1 && errno == EINTR);
            }
        }
    }

    for (node = gsUnixSignalWatches; node; node = node->next) {
        CUnixSignalWatchSource *source = node->data;
        if (pending[source->signum] && c_atomic_bool_compare_and_exchange (&source->pending, false, true)) {
            wake_source ((CSource*) source);
        }
    }
}

static void wake_source (CSource *source)
{
    CMainContext *context;
    C_LOCK(gsMainContextList);
    context = source->context;
    if (context) {
        c_wakeup_signal (context->wakeup);
    }
    C_UNLOCK(gsMainContextList);
}

static void dispatch_unix_signals (void)
{
    C_LOCK(gsUnixSignalLock);
    dispatch_unix_signals_unlocked ();
    C_UNLOCK(gsUnixSignalLock);
}

static void c_source_unref_internal (CSource* source, CMainContext* context, bool have_lock)
{
    void* old_cb_data = NULL;
    CSourceCallbackFuncs *old_cb_funcs = NULL;

    c_return_if_fail (source != NULL);

    if (!have_lock && context) {
        LOCK_CONTEXT (context);
    }

    if (c_atomic_int_dec_and_test ((cint*) &source->refCount)) {
        if (source->priv->dispose) {
            c_atomic_int_inc ((cint*) &source->refCount);
            if (context) {
                UNLOCK_CONTEXT (context);
            }
            source->priv->dispose (source);
            if (context) {
                LOCK_CONTEXT (context);
            }

            if (!c_atomic_int_dec_and_test ((cint*) &source->refCount)) {
                if (!have_lock && context) {
                    UNLOCK_CONTEXT (context);
                }
                return;
            }
        }

        old_cb_data = source->callbackData;
        old_cb_funcs = source->callbackFuncs;

        source->callbackData = NULL;
        source->callbackFuncs = NULL;

        if (context) {
            if (!SOURCE_DESTROYED (source)) {
                C_LOG_WARNING_CONSOLE("refCount == 0, but source was still attached to a context!");
            }
            source_remove_from_context (source, context);
            c_hash_table_remove (context->sources, (void*) *((void**) &(source->sourceId)));
        }

        if (source->sourceFuncs->finalize) {
            cint old_ref_count;

            c_atomic_int_inc ((cint*)&source->refCount);
            if (context) {
                UNLOCK_CONTEXT (context);
            }
            source->sourceFuncs->finalize (source);
            if (context) {
                LOCK_CONTEXT (context);
            }
            old_ref_count = c_atomic_int_add ((void*)&source->refCount, -1);
            c_warn_if_fail (old_ref_count == 1);
        }

        if (old_cb_funcs) {
            cint old_ref_count;
            c_atomic_int_inc ((cint*)&source->refCount);
            if (context) {
                UNLOCK_CONTEXT (context);
            }

            old_cb_funcs->unref (old_cb_data);
            if (context) {
                LOCK_CONTEXT (context);
            }
            old_ref_count = c_atomic_int_add ((cint*) &source->refCount, -1);
            c_warn_if_fail (old_ref_count == 1);
        }

        if (!source->priv->staticName) {
            c_free (source->name);
        }
        source->name = NULL;

        c_slist_free (source->pollFds);
        source->pollFds = NULL;
        c_slist_free_full (source->priv->fds, c_free0);
        while (source->priv->childSources) {
            CSource *child_source = source->priv->childSources->data;
            source->priv->childSources = c_slist_remove (source->priv->childSources, child_source);
            child_source->priv->parentSource = NULL;
            c_source_unref_internal (child_source, context, true);
        }

        c_free (source->priv);
        source->priv = NULL;

        c_free (source);
    }

    if (!have_lock && context) {
        UNLOCK_CONTEXT (context);
    }
}

static bool c_child_watch_prepare (CSource* source, cint* timeout)
{
    *timeout = -1;
    return false;
}

static bool c_timeout_dispatch (CSource* source, CSourceFunc callback, void* user_data)
{
    CTimeoutSource* timeout_source = (CTimeoutSource*) source;
    bool again;

    if (!callback) {
        C_LOG_WARNING_CONSOLE("Timeout source dispatched without callback. You must call g_source_set_callback().");
        return false;
    }

    if (timeout_source->oneShot) {
        CSourceOnceFunc once_callback = (CSourceOnceFunc) callback;
        once_callback (user_data);
        again = C_SOURCE_REMOVE;
    }
    else {
        again = callback (user_data);
    }

    if (again) {
        c_timeout_set_expiration (timeout_source, c_source_get_time (source));
    }

    return again;
}

static void c_timeout_set_expiration (CTimeoutSource* timeout_source, cint64 current_time)
{
    cint64 expiration;

    if (timeout_source->seconds) {
        cint64 remainder;
        static cint timer_perturb = -1;

        if (timer_perturb == -1) {
            const char *session_bus_address = c_getenv ("DBUS_SESSION_BUS_ADDRESS");
            if (!session_bus_address) {
                session_bus_address = c_getenv ("HOSTNAME");
            }
            if (session_bus_address) {
                timer_perturb = C_ABS ((cint) c_str_hash (session_bus_address)) % 1000000;
            }
            else {
                timer_perturb = 0;
            }
        }

        expiration = current_time + (cuint64) timeout_source->interval * 1000 * 1000;

        expiration -= timer_perturb;

        remainder = expiration % 1000000;
        if (remainder >= 1000000/4) {
            expiration += 1000000;
        }

        expiration -= remainder;
        expiration += timer_perturb;
    }
    else {
        expiration = current_time + (cuint64) timeout_source->interval * 1000;
    }

    c_source_set_ready_time ((CSource*) timeout_source, expiration);
}

static void c_source_iter_clear (CSourceIter* iter)
{
    if (iter->source && iter->mayModify) {
        c_source_unref_internal (iter->source, iter->context, true);
        iter->source = NULL;
    }
}

static bool c_source_iter_next (CSourceIter* iter, CSource** source)
{
    CSource* next_source;

    if (iter->source) {
        next_source = iter->source->next;
    }
    else {
        next_source = NULL;
    }

    if (!next_source) {
        if (iter->currentList) {
            iter->currentList = iter->currentList->next;
        }
        else {
            iter->currentList = iter->context->sourceLists;
        }

        if (iter->currentList) {
            CSourceList *source_list = iter->currentList->data;
            next_source = source_list->head;
        }
    }

    if (next_source && iter->mayModify) {
        c_source_ref (next_source);
    }

    if (iter->source && iter->mayModify) {
        c_source_unref_internal (iter->source, iter->context, true);
    }
    iter->source = next_source;

    *source = iter->source;
    return *source != NULL;
}

static void c_source_iter_init (CSourceIter* iter, CMainContext* context, bool may_modify)
{
    iter->context = context;
    iter->currentList = NULL;
    iter->source = NULL;
    iter->mayModify = may_modify;
}

static void c_main_context_add_poll_unlocked (CMainContext* context, cint priority, CPollFD* fd)
{
    CPollRec *prevrec, *nextrec;
    CPollRec *newrec = c_malloc0(sizeof(CPollRec));

    fd->rEvents = 0;
    newrec->fd = fd;
    newrec->priority = priority;

    prevrec = NULL;
    nextrec = context->pollRecords;
    while (nextrec) {
        if (nextrec->fd->fd > fd->fd) {
            break;
        }
        prevrec = nextrec;
        nextrec = nextrec->next;
    }

    if (prevrec) {
        prevrec->next = newrec;
    }
    else {
        context->pollRecords = newrec;
    }

    newrec->prev = prevrec;
    newrec->next = nextrec;

    if (nextrec) {
        nextrec->prev = newrec;
    }

    context->nPollRecords++;
    context->pollChanged = true;

    if (fd != &context->wakeupRec) {
        c_wakeup_signal (context->wakeup);
    }
}

static void c_source_destroy_internal (CSource* source, CMainContext* context, bool have_lock)
{
    if (!have_lock) {
        LOCK_CONTEXT (context);
    }

    if (!SOURCE_DESTROYED (source)) {
        CSList *tmp_list;
        void* old_cb_data;
        CSourceCallbackFuncs *old_cb_funcs;

        source->flags &= ~C_HOOK_FLAG_ACTIVE;

        old_cb_data = source->callbackData;
        old_cb_funcs = source->callbackFuncs;

        source->callbackData = NULL;
        source->callbackFuncs = NULL;

        if (old_cb_funcs) {
            UNLOCK_CONTEXT (context);
            old_cb_funcs->unref (old_cb_data);
            LOCK_CONTEXT (context);
        }

        if (!SOURCE_BLOCKED (source)) {
            tmp_list = source->pollFds;
            while (tmp_list) {
                c_main_context_remove_poll_unlocked (context, tmp_list->data);
                tmp_list = tmp_list->next;
            }

            for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next) {
                c_main_context_remove_poll_unlocked (context, tmp_list->data);
            }
        }

        while (source->priv->childSources) {
            c_child_source_remove_internal (source->priv->childSources->data, context);
        }

        if (source->priv->parentSource) {
            c_child_source_remove_internal (source, context);
        }

        c_source_unref_internal (source, context, true);
    }

    if (!have_lock) {
        UNLOCK_CONTEXT (context);
    }
}

static void c_main_context_remove_poll_unlocked (CMainContext* context, CPollFD* fd)
{
    CPollRec *pollrec, *prevrec, *nextrec;

    prevrec = NULL;
    pollrec = context->pollRecords;

    while (pollrec) {
        nextrec = pollrec->next;
        if (pollrec->fd == fd) {
            if (prevrec != NULL) {
                prevrec->next = nextrec;
            }
            else {
                context->pollRecords = nextrec;
            }

            if (nextrec != NULL) {
                nextrec->prev = prevrec;
            }
            c_free(pollrec);
            context->nPollRecords--;
            break;
        }
        prevrec = pollrec;
        pollrec = nextrec;
    }

    context->pollChanged = true;

    c_wakeup_signal (context->wakeup);
}

static void c_child_source_remove_internal (CSource* child_source, CMainContext* context)
{
    CSource *parent_source = child_source->priv->parentSource;

    parent_source->priv->childSources =
        c_slist_remove (parent_source->priv->childSources, child_source);
    child_source->priv->parentSource = NULL;

    c_source_destroy_internal (child_source, context, true);
    c_source_unref_internal (child_source, context, true);
}

static void source_remove_from_context (CSource* source, CMainContext* context)
{
    CSourceList *source_list;

    source_list = find_source_list_for_priority (context, source->priority, false);
    c_return_if_fail (source_list != NULL);

    if (source->prev)
        source->prev->next = source->next;
    else
        source_list->head = source->next;

    if (source->next)
        source->next->prev = source->prev;
    else
        source_list->tail = source->prev;

    source->prev = NULL;
    source->next = NULL;

    if (source_list->head == NULL) {
        context->sourceLists = c_list_remove (context->sourceLists, source_list);
        c_free(source_list);
    }
}

static CSourceList* find_source_list_for_priority (CMainContext* context, cint priority, bool create)
{
    CList *iter, *last;
    CSourceList *source_list;

    last = NULL;
    for (iter = context->sourceLists; iter != NULL; last = iter, iter = iter->next) {
        source_list = iter->data;

        if (source_list->priority == priority)
            return source_list;

        if (source_list->priority > priority) {
            if (!create) {
                return NULL;
            }

            source_list = c_malloc0(sizeof(CSourceList));
            source_list->priority = priority;
            context->sourceLists = c_list_insert_before (context->sourceLists, iter, source_list);
            return source_list;
        }
    }

    if (!create) {
        return NULL;
    }

    source_list = c_malloc0(sizeof(CSourceList));
    source_list->priority = priority;

    if (!last) {
        context->sourceLists = c_list_append (NULL, source_list);
    }
    else {
        last = c_list_append (last, source_list);
        (void) last;
    }
    return source_list;
}

static bool c_main_context_iterate_unlocked (CMainContext* context, bool block, bool dispatch, CThread* self)
{
    cint max_priority = 0;
    cint64 timeout_usec;
    bool some_ready;
    cint nfds, allocated_nfds;
    CPollFD *fds = NULL;

    if (!c_main_context_acquire_unlocked (context)) {
        bool got_ownership;

        if (!block) {
            return false;
        }

        got_ownership = c_main_context_wait_internal (context, &context->cond, &context->mutex);

        if (!got_ownership) {
            return false;
        }
    }

    if (!context->cachedPollArray) {
        context->cachedPollArraySize = context->nPollRecords;
        context->cachedPollArray = c_malloc0(sizeof(CPollFD) * context->nPollRecords);
    }

    allocated_nfds = context->cachedPollArraySize;
    fds = context->cachedPollArray;

    c_main_context_prepare_unlocked (context, &max_priority);

    while ((nfds = c_main_context_query_unlocked (context, max_priority, &timeout_usec, fds, allocated_nfds)) > allocated_nfds) {
        c_free (fds);
        context->cachedPollArraySize = allocated_nfds = nfds;
        context->cachedPollArray = fds = c_malloc0(sizeof(CPollFD) * nfds);
    }

    if (!block) {
        timeout_usec = 0;
    }

    c_main_context_poll_unlocked (context, timeout_usec, max_priority, fds, nfds);

    some_ready = c_main_context_check_unlocked (context, max_priority, fds, nfds);

    if (dispatch) {
        c_main_context_dispatch_unlocked (context);
    }

    c_main_context_release_unlocked (context);

    return some_ready;
}

static void c_main_context_dispatch_unlocked (CMainContext *context)
{
    if (context->pendingDispatches->len > 0) {
        c_main_dispatch (context);
    }
}

static bool c_main_context_check_unlocked (CMainContext *context, cint max_priority, CPollFD* fds, cint n_fds)
{
    CSource *source;
    CSourceIter iter;
    CPollRec *pollrec;
    cint n_ready = 0;
    cint i;

    if (context == NULL) {
        context = c_main_context_default ();
    }

    if (context->inCheckOrPrepare) {
        C_LOG_WARNING_CONSOLE("g_main_context_check() called recursively from within a source's check() or prepare() member.");
        return false;
    }

    for (i = 0; i < n_fds; i++) {
        if (fds[i].fd == context->wakeupRec.fd) {
            if (fds[i].rEvents) {
                c_wakeup_acknowledge (context->wakeup);
            }
            break;
        }
    }

    /**
     * If the set of poll file descriptors changed, bail out
     * and let the main loop rerun
     */
    if (context->pollChanged) {
        return false;
    }

    /**
     * The linear iteration below relies on the assumption that both
     * poll records and the fds array are incrementally sorted by file
     * descriptor identifier.
     */
    pollrec = context->pollRecords;
    i = 0;
    while (pollrec && i < n_fds) {
        /* Make sure that fds is sorted by file descriptor identifier. */
        c_assert (i <= 0 || fds[i - 1].fd < fds[i].fd);

        /* Skip until finding the first GPollRec matching the current GPollFD. */
        while (pollrec && pollrec->fd->fd != fds[i].fd) {
            pollrec = pollrec->next;
        }

        /* Update all consecutive GPollRecs that match. */
        while (pollrec && pollrec->fd->fd == fds[i].fd) {
            if (pollrec->priority <= max_priority) {
                pollrec->fd->rEvents = fds[i].rEvents & (pollrec->fd->events | C_IO_ERR | C_IO_HUP | C_IO_NVAL);
            }
            pollrec = pollrec->next;
        }

        /* Iterate to next GPollFD. */
        i++;
    }

    c_source_iter_init (&iter, context, true);
    while (c_source_iter_next (&iter, &source)) {
        if (SOURCE_DESTROYED (source) || SOURCE_BLOCKED (source)) {
            continue;
        }

        if ((n_ready > 0) && (source->priority > max_priority)) {
            break;
        }

        if (!(source->flags & C_SOURCE_READY)) {
            bool result;
            bool (*check) (CSource* source);
            check = source->sourceFuncs->check;
            if (check) {
                /* If the check function is set, call it. */
                context->inCheckOrPrepare++;
                UNLOCK_CONTEXT (context);
                result = (*check) (source);
                LOCK_CONTEXT (context);
                context->inCheckOrPrepare--;
            }
            else {
                result = false;
            }

            if (result == false) {
                CSList *tmp_list;

                /**
                 * If not already explicitly flagged ready by ->check()
                 * (or if we have no check) then we can still be ready if
                 * any of our fds poll as ready.
                 */
                for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next) {
                    CPollFD *pollfd = tmp_list->data;
                    if (pollfd->rEvents) {
                        result = true;
                        break;
                    }
                }
            }

            if (result == false && source->priv->readyTime != -1) {
                if (!context->timeIsFresh) {
                    context->time = c_get_monotonic_time ();
                    context->timeIsFresh = true;
                }

                if (source->priv->readyTime <= context->time) {
                    result = true;
                }
            }

            if (result) {
                CSource *ready_source = source;
                while (ready_source) {
                    ready_source->flags |= C_SOURCE_READY;
                    ready_source = ready_source->priv->parentSource;
                }
            }
        }

        if (source->flags & C_SOURCE_READY) {
            c_source_ref (source);
            c_ptr_array_add (context->pendingDispatches, source);
            n_ready++;

            /**
             * never dispatch sources with less priority than the first
             * one we choose to dispatch
             */
            max_priority = source->priority;
        }
    }
    c_source_iter_clear (&iter);

    return n_ready > 0;
}

static cint c_main_context_query_unlocked (CMainContext* context, cint max_priority, cint64* timeout_usec, CPollFD* fds, cint n_fds)
{
    cint n_poll;
    CPollRec *pollrec, *lastpollrec;
    cushort events;

    /**
     * fds is filled sequentially from poll_records. Since poll_records
     * are incrementally sorted by file descriptor identifier, fds will
     * also be incrementally sorted.
     */
    n_poll = 0;
    lastpollrec = NULL;
    for (pollrec = context->pollRecords; pollrec; pollrec = pollrec->next) {
        if (pollrec->priority > max_priority) {
            continue;
        }

        /**
         * In direct contradiction to the Unix98 spec, IRIX runs into
         * difficulty if you pass in POLLERR, POLLHUP or POLLNVAL
         * flags in the events field of the pollfd while it should
         * just ignoring them. So we mask them out here.
         */
        events = pollrec->fd->events & ~(C_IO_ERR|C_IO_HUP|C_IO_NVAL);

        /**
         * This optimization --using the same GPollFD to poll for more
         * than one poll record-- relies on the poll records being
         * incrementally sorted.
         */
        if (lastpollrec && pollrec->fd->fd == lastpollrec->fd->fd) {
            if (n_poll - 1 < n_fds) {
                fds[n_poll - 1].events |= events;
            }
        }
        else {
            if (n_poll < n_fds) {
                fds[n_poll].fd = pollrec->fd->fd;
                fds[n_poll].events = events;
                fds[n_poll].rEvents = 0;
            }
            n_poll++;
        }
        lastpollrec = pollrec;
    }

    context->pollChanged = false;

    if (timeout_usec) {
        *timeout_usec = context->timeoutUsec;
        if (*timeout_usec != 0) {
            context->timeIsFresh = false;
        }
    }

    return n_poll;
}

static CMainDispatch* get_dispatch (void)
{
    static CPrivate depth_private = C_PRIVATE_INIT (c_main_dispatch_free);

    CMainDispatch *dispatch = c_private_get (&depth_private);
    if (!dispatch) {
        dispatch = c_private_set_alloc0 (&depth_private, sizeof (CMainDispatch));
    }

    return dispatch;
}

static bool c_main_context_acquire_unlocked (CMainContext *context)
{
    CThread *self = C_THREAD_SELF;

    if (!context->owner) {
        context->owner = self;
        c_assert (context->ownerCount == 0);
    }

    if (context->owner == self) {
        context->ownerCount++;
        return true;
    }
    else {
        return false;
    }
}

static bool c_main_context_wait_internal (CMainContext* context, CCond* cond, CMutex* mutex)
{
    bool result = false;
    CThread* self = C_THREAD_SELF;

    if (context == NULL) {
        context = c_main_context_default();
    }

    bool loopInternalWaiter = (mutex == &context->mutex);

    if (!loopInternalWaiter) {
        LOCK_CONTEXT (context);
    }

    if (context->owner && context->owner != self) {
        CMainWaiter waiter;

        waiter.cond = cond;
        waiter.mutex = mutex;

        context->waiters = c_slist_append (context->waiters, &waiter);

        if (!loopInternalWaiter) {
            UNLOCK_CONTEXT (context);
        }

        c_cond_wait (cond, mutex);

        if (!loopInternalWaiter) {
            LOCK_CONTEXT (context);
        }

        context->waiters = c_slist_remove (context->waiters, &waiter);
    }

    if (NULL == context->owner) {
        context->owner = self;
        c_assert (context->ownerCount == 0);
    }

    if (context->owner == self) {
        context->ownerCount++;
        result = true;
    }

    if (!loopInternalWaiter) {
        UNLOCK_CONTEXT (context);
    }

    return result;
}


static bool c_main_context_prepare_unlocked (CMainContext* context, cint* priority)
{
    cuint i;
    cint n_ready = 0;
    cint current_priority = C_MAX_INT32;
    CSource *source;
    CSourceIter iter;

    context->timeIsFresh = false;

    if (context->inCheckOrPrepare) {
        C_LOG_WARNING_CONSOLE("c_main_context_prepare() called recursively from within a source's check() or prepare() member.");
        return false;
    }

    /* If recursing, clear list of pending dispatches */
    for (i = 0; i < context->pendingDispatches->len; i++) {
        if (context->pendingDispatches->pdata[i]) {
            c_source_unref_internal ((CSource*)context->pendingDispatches->pdata[i], context, true);
        }
    }
    c_ptr_array_set_size (context->pendingDispatches, 0);

    /* Prepare all sources */
    context->timeoutUsec = -1;

    c_source_iter_init (&iter, context, true);
    while (c_source_iter_next (&iter, &source)) {
        cint64 source_timeout_usec = -1;
        if (SOURCE_DESTROYED (source) || SOURCE_BLOCKED (source)) {
            continue;
        }
        if ((n_ready > 0) && (source->priority > current_priority)) {
            break;
        }
        if (!(source->flags & C_SOURCE_READY)) {
            bool result;
            bool (*prepare) (CSource* source, cint* timeout);
            prepare = source->sourceFuncs->prepare;
            if (prepare) {
                cint64 begin_time_nsec C_UNUSED;
                int source_timeout_msec = -1;

                context->inCheckOrPrepare++;
                UNLOCK_CONTEXT (context);
                result = (*prepare) (source, &source_timeout_msec);
                // source_timeout_usec = extend_timeout_to_usec (source_timeout_msec);

                LOCK_CONTEXT (context);
                context->inCheckOrPrepare--;
            }
            else {
                result = false;
            }

            if (result == false && source->priv->readyTime != -1) {
                if (!context->timeIsFresh) {
                    context->time = c_get_monotonic_time ();
                    context->timeIsFresh = true;
                }

                if (source->priv->readyTime <= context->time) {
                    source_timeout_usec = 0;
                    result = true;
                }
                else if (source_timeout_usec < 0 || (source->priv->readyTime < context->time + source_timeout_usec)) {
                  source_timeout_usec = C_MAX (0, source->priv->readyTime - context->time);
                }
            }

            if (result) {
                CSource *ready_source = source;
                while (ready_source) {
                  ready_source->flags |= C_SOURCE_READY;
                  ready_source = ready_source->priv->parentSource;
                }
            }
        }
        if (source->flags & C_SOURCE_READY) {
            n_ready++;
            current_priority = source->priority;
            context->timeoutUsec = 0;
        }
        if (source_timeout_usec >= 0) {
            if (context->timeoutUsec < 0) {
                context->timeoutUsec = source_timeout_usec;
            }
            else {
                context->timeoutUsec = C_MIN (context->timeoutUsec, source_timeout_usec);
            }
        }
    }
    c_source_iter_clear (&iter);

    if (priority) {
        *priority = current_priority;
    }

    return (n_ready > 0);
}

static void  c_main_context_poll_unlocked (CMainContext *context, cint64 timeout_usec, int priority, CPollFD* fds, int n_fds)
{
#ifdef  G_MAIN_POLL_DEBUG
  GTimer *poll_timer;
  GPollRec *pollrec;
  gint i;
#endif

    CPollFunc poll_func;
    if (n_fds || timeout_usec != 0) {
        int ret, errsv;

        poll_func = context->pollFunc;
        {
            int timeout_msec = round_timeout_to_msec (timeout_usec);
            UNLOCK_CONTEXT (context);
            ret = (*poll_func) (fds, n_fds, timeout_msec);
            LOCK_CONTEXT (context);
        }

        errsv = errno;
        if (ret < 0 && errsv != EINTR) {
            C_LOG_WARNING_CONSOLE("poll(2) failed due to: %s.", c_strerror (errsv));
        }
    } /* if (n_fds || timeout_usec != 0) */
}

static void c_main_context_release_unlocked (CMainContext* context)
{
    c_return_if_fail (context->ownerCount > 0);

    context->ownerCount--;
    if (context->ownerCount == 0) {
        context->owner = NULL;
        if (context->waiters) {
            CMainWaiter *waiter = context->waiters->data;
            bool loop_internal_waiter = (waiter->mutex == &context->mutex);
            context->waiters = c_slist_delete_link (context->waiters, context->waiters);
            if (!loop_internal_waiter) {
                c_mutex_lock (waiter->mutex);
            }
            c_cond_signal (waiter->cond);

            if (!loop_internal_waiter) {
                c_mutex_unlock (waiter->mutex);
            }
        }
    }
}

static void c_main_dispatch (CMainContext* context)
{
    C_LOG_DEBUG_CONSOLE("");
    CMainDispatch *current = get_dispatch ();

    cuint i;
    for (i = 0; i < context->pendingDispatches->len; i++) {
        CSource *source = context->pendingDispatches->pdata[i];
        context->pendingDispatches->pdata[i] = NULL;
        c_assert (source);

        source->flags &= ~C_SOURCE_READY;

        if (!SOURCE_DESTROYED (source)) {
            bool was_in_call;
            void* user_data = NULL;
            CSourceFunc callback = NULL;
            CSourceCallbackFuncs *cb_funcs;
            void* cb_data;
            bool need_destroy;

            bool(*dispatch) (CSource*, CSourceFunc, void*);
            CSource *prev_source;
            cint64 begin_time_nsec C_UNUSED;

            dispatch = source->sourceFuncs->dispatch;
            cb_funcs = source->callbackFuncs;
            cb_data = source->callbackData;

            if (cb_funcs) {
                cb_funcs->ref (cb_data);
            }

            if ((source->flags & C_SOURCE_CAN_RECURSE) == 0) {
                block_source (source);
            }

            was_in_call = source->flags & C_HOOK_FLAG_IN_CALL;
            source->flags |= C_HOOK_FLAG_IN_CALL;

            if (cb_funcs) {
                cb_funcs->get (cb_data, source, &callback, &user_data);
            }

            UNLOCK_CONTEXT (context);

            prev_source = current->source;
            current->source = source;
            current->depth++;

            need_destroy = !(* dispatch) (source, callback, user_data);
            current->source = prev_source;
            current->depth--;

            if (cb_funcs) {
                cb_funcs->unref (cb_data);
            }
            LOCK_CONTEXT (context);

            if (!was_in_call) {
                source->flags &= ~C_HOOK_FLAG_IN_CALL;
            }

            if (SOURCE_BLOCKED (source) && !SOURCE_DESTROYED (source)) {
                unblock_source (source);
            }

            if (need_destroy && !SOURCE_DESTROYED (source)) {
                c_assert (source->context == context);
                c_source_destroy_internal (source, context, true);
            }
        }

        c_source_unref_internal (source, context, true);
    }

    c_ptr_array_set_size (context->pendingDispatches, 0);
}

static void c_main_dispatch_free (void* dispatch)
{
    c_free (dispatch);
}

static int round_timeout_to_msec (cint64 timeout_usec)
{
    /* We need to round to milliseconds from our internal microseconds for
     * various external API and GPollFunc which requires milliseconds.
     *
     * However, we want to ensure a few invariants for this.
     *
     *   Return == -1 if we have no timeout specified
     *   Return ==  0 if we don't want to block at all
     *   Return  >  0 if we have any timeout to avoid spinning the CPU
     *
     * This does cause jitter if the microsecond timeout is < 1000 usec
     * because that is beyond our precision. However, using ppoll() instead
     * of poll() (when available) avoids this jitter.
     */

    if (timeout_usec == 0)
        return 0;

    if (timeout_usec > 0) {
        cuint64 timeout_msec = (timeout_usec + 999) / 1000;
        return (int) C_MIN (timeout_msec, C_MAX_INT32);
    }

    return -1;
}

static void block_source (CSource *source)
{
    CSList *tmp_list;

    c_return_if_fail (!SOURCE_BLOCKED (source));

    source->flags |= C_SOURCE_BLOCKED;

    if (source->context) {
        tmp_list = source->pollFds;
        while (tmp_list) {
            c_main_context_remove_poll_unlocked (source->context, tmp_list->data);
            tmp_list = tmp_list->next;
        }

        for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next) {
            c_main_context_remove_poll_unlocked (source->context, tmp_list->data);
        }
    }

    if (source->priv && source->priv->childSources) {
        tmp_list = source->priv->childSources;
        while (tmp_list) {
            block_source (tmp_list->data);
            tmp_list = tmp_list->next;
        }
    }
}

static void unblock_source (CSource *source)
{
    c_return_if_fail (SOURCE_BLOCKED (source)); /* Source already unblocked */
    c_return_if_fail (!SOURCE_DESTROYED (source));

    source->flags &= ~C_SOURCE_BLOCKED;

    CSList *tmpList = source->pollFds;
    while (tmpList) {
        c_main_context_add_poll_unlocked (source->context, source->priority, tmpList->data);
        tmpList = tmpList->next;
    }

    for (tmpList = source->priv->fds; tmpList; tmpList = tmpList->next) {
        c_main_context_add_poll_unlocked (source->context, source->priority, tmpList->data);
    }

    if (source->priv && source->priv->childSources) {
        tmpList = source->priv->childSources;
        while (tmpList) {
            unblock_source (tmpList->data);
            tmpList = tmpList->next;
        }
    }
}

static void free_context_stack (void* data)
{
    c_queue_free_full((CQueue*) data, (CDestroyNotify) free_context);
}

static bool c_main_context_iterate (CMainContext* context, bool block, bool dispatch, CThread* self)
{
    C_LOG_DEBUG_CONSOLE("");

    cint maxPriority = 0;
    cint timeout;
    cboolean someReady;
    cint nfds, allocatedNfds;
    CPollFD *fds = NULL;
    cint64 begin_time_nsec C_UNUSED;

    UNLOCK_CONTEXT (context);

    C_LOG_DEBUG_CONSOLE("start iterate");
    if (!c_main_context_acquire (context)) {
        LOCK_CONTEXT (context);
        if (!block) {
            return false;
        }

        bool gotOwnership = c_main_context_wait_internal (context, &context->cond, &context->mutex);
        if (!gotOwnership) {
            return false;
        }
    }
    else {
        LOCK_CONTEXT (context);
    }

    if (NULL == context->cachedPollArray) {
        context->cachedPollArraySize = context->nPollRecords;
        context->cachedPollArray = c_malloc0(sizeof(CPollFD) * context->nPollRecords);
    }

    allocatedNfds = context->cachedPollArraySize;
    fds = context->cachedPollArray;

    UNLOCK_CONTEXT (context);

    c_main_context_prepare (context, &maxPriority);

    while ((nfds = c_main_context_query (context, maxPriority, &timeout, fds, allocatedNfds)) > allocatedNfds) {
        LOCK_CONTEXT (context);
        c_free (fds);
        context->cachedPollArraySize = allocatedNfds = nfds;
        context->cachedPollArray = fds = c_malloc0(sizeof(CPollFD) * nfds);
        UNLOCK_CONTEXT (context);
    }

    if (!block) {
        timeout = 0;
    }

    c_main_context_poll (context, timeout, maxPriority, fds, nfds);
    someReady = c_main_context_check (context, maxPriority, fds, nfds);

    if (dispatch) {
        c_main_context_dispatch (context);
    }

    c_main_context_release (context);

    LOCK_CONTEXT (context);

    return someReady;
}

static void c_main_context_poll (CMainContext* context, cint timeout, cint priority, CPollFD* fds, cint nFds)
{
    CPollFunc pollFunc;

    if (nFds || timeout != 0) {
        int ret, errsv;

        LOCK_CONTEXT (context);
        pollFunc = context->pollFunc;
        UNLOCK_CONTEXT (context);

        ret = (*pollFunc) (fds, nFds, timeout);
        errsv = errno;
        if (ret < 0 && errsv != EINTR) {
            C_LOG_WARNING_CONSOLE("poll(2) failed due to: %s.", c_strerror (errsv));
        }
    }
}


static void c_source_set_priority_unlocked (CSource* source, CMainContext* context, cint priority)
{
    CSList *tmp_list;
    c_return_if_fail (source->priv->parentSource == NULL || source->priv->parentSource->priority == priority);

    if (context) {
        /* Remove the source from the context's source and then
         * add it back after so it is sorted in the correct place
         */
        source_remove_from_context (source, source->context);
    }

    source->priority = priority;

    if (context) {
        source_add_to_context (source, source->context);

        if (!SOURCE_BLOCKED (source)) {
            tmp_list = source->pollFds;
            while (tmp_list) {
                c_main_context_remove_poll_unlocked (context, tmp_list->data);
                c_main_context_add_poll_unlocked (context, priority, tmp_list->data);
                tmp_list = tmp_list->next;
            }

            for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next) {
                c_main_context_remove_poll_unlocked (context, tmp_list->data);
                c_main_context_add_poll_unlocked (context, priority, tmp_list->data);
            }
        }
    }

    if (source->priv->childSources) {
        tmp_list = source->priv->childSources;
        while (tmp_list) {
            c_source_set_priority_unlocked (tmp_list->data, context, priority);
            tmp_list = tmp_list->next;
        }
    }
}

static void source_add_to_context (CSource* source, CMainContext* context)
{
    CSourceList *source_list;
    CSource *prev, *next;

    source_list = find_source_list_for_priority (context, source->priority, true);

    if (source->priv->parentSource) {
        c_assert (source_list->head != NULL);

        /* Put the source immediately before its parent */
        prev = source->priv->parentSource->prev;
        next = source->priv->parentSource;
    }
    else {
        prev = source_list->tail;
        next = NULL;
    }

    source->next = next;
    if (next) {
        next->prev = source;
    }
    else {
        source_list->tail = source;
    }

    source->prev = prev;
    if (prev) {
        prev->next = source;
    }
    else {
        source_list->head = source;
    }
}

static cuint c_source_attach_unlocked (CSource* source, CMainContext* context, bool do_wakeup)
{
    CSList *tmp_list;
    cuint id;

    do {
        id = context->nextId++;
    }
    while (id == 0 || c_hash_table_contains (context->sources, C_UINT_TO_POINTER (id)));

    source->context = context;
    source->sourceId = id;
    c_source_ref (source);

    c_hash_table_insert (context->sources, C_UINT_TO_POINTER (id), source);

    source_add_to_context (source, context);

    if (!SOURCE_BLOCKED (source)) {
        tmp_list = source->pollFds;
        while (tmp_list) {
            c_main_context_add_poll_unlocked (context, source->priority, tmp_list->data);
            tmp_list = tmp_list->next;
        }

        for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next) {
            c_main_context_add_poll_unlocked (context, source->priority, tmp_list->data);
        }
    }

    tmp_list = source->priv->childSources;
    while (tmp_list) {
        c_source_attach_unlocked (tmp_list->data, context, false);
        tmp_list = tmp_list->next;
    }

    /* If another thread has acquired the context, wake it up since it
     * might be in poll() right now.
     */
    if (do_wakeup && (context->flags & C_MAIN_CONTEXT_FLAGS_OWNERLESS_POLLING || (context->owner && context->owner != C_THREAD_SELF))) {
        c_wakeup_signal (context->wakeup);
    }

    return source->sourceId;
}
static void c_source_set_name_full (CSource* source, const char *name, bool is_static)
{
    CMainContext *context;

    c_return_if_fail (source != NULL);
    c_return_if_fail (c_atomic_int_get ((cint*)&source->refCount) > 0);

    context = source->context;

    if (context) {
        LOCK_CONTEXT (context);
    }

    if (!source->priv->staticName) {
        c_free (source->name);
    }

    if (is_static) {
        source->name = (char *)name;
    }
    else {
        source->name = c_strdup (name);
    }

    source->priv->staticName = is_static;

    if (context) {
        UNLOCK_CONTEXT (context);
    }
}

static CSource* timeout_source_new (cuint interval, bool seconds, bool one_shot)
{
    CSource *source = c_source_new (&c_timeout_funcs, sizeof (CTimeoutSource));
    CTimeoutSource *timeout_source = (CTimeoutSource*)source;

    timeout_source->interval = interval;
    timeout_source->seconds = seconds;
    timeout_source->oneShot = one_shot;

    c_timeout_set_expiration (timeout_source, c_get_monotonic_time ());

    return source;
}

static cuint timeout_add_full (cint priority, cuint interval, bool seconds, bool one_shot, CSourceFunc function, void* data, CDestroyNotify notify)
{
    CSource *source;
    cuint id;

    c_return_val_if_fail (function != NULL, 0);

    source = timeout_source_new (interval, seconds, one_shot);

    if (priority != C_PRIORITY_DEFAULT) {
        c_source_set_priority (source, priority);
    }

    c_source_set_callback (source, function, data, notify);
    id = c_source_attach (source, NULL);

//    TRACE (GLIB_TIMEOUT_ADD (source, g_main_context_default (), id, priority, interval, function, data));

    c_source_unref (source);

    return id;
}

static void c_source_callback_ref (void* cbData)
{
    CSourceCallback *callback = cbData;

    c_atomic_int_inc (&callback->refCount);
}

static void c_source_callback_unref (void* cbData)
{
    CSourceCallback *callback = cbData;

    if (c_atomic_int_dec_and_test (&callback->refCount)) {
        if (callback->notify) {
            callback->notify (callback->data);
        }
        c_free (callback);
    }
}

static void c_source_callback_get (void* cbData, CSource* source, CSourceFunc* func, void** data)
{
    CSourceCallback* callback = cbData;

    *func = callback->func;
    *data = callback->data;
}

