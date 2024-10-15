
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

#ifndef CLIBRARY_SOURCE_H
#define CLIBRARY_SOURCE_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/poll.h>
#include <c/slist.h>
#include <c/macros.h>
#include <c/thread.h>

C_BEGIN_EXTERN_C

#define C_PRIORITY_HIGH            -100
#define C_PRIORITY_DEFAULT          0
#define C_PRIORITY_HIGH_IDLE        100
#define C_PRIORITY_DEFAULT_IDLE     200
#define C_PRIORITY_LOW              300

#define C_SOURCE_REMOVE             false
#define C_SOURCE_CONTINUE           true


typedef void                        CMainContextPusher;

typedef void (*CClearHandleFunc) (cuint handleId);

void c_clear_handle_id (cuint* tagPtr, CClearHandleFunc clearFunc);


extern CSourceFuncs c_unix_signal_funcs;
extern CSourceFuncs c_unix_fd_source_funcs;


#define C_UNIX_ERROR (c_unix_error_quark())
CQuark          c_unix_error_quark          (void);
bool            c_unix_open_pipe            (cint* fds, cint flags, CError** error);
bool            c_unix_set_fd_nonblocking   (cint fd, bool nonblock, CError** error);
CSource*        c_unix_signal_source_new    (cint signum);
cuint           c_unix_signal_add_full      (cint priority, cint signum, CSourceFunc handler, void* udata, CDestroyNotify notify);
cuint           c_unix_signal_add           (cint signum, CSourceFunc handler, void* udata);
CSource*        c_unix_fd_source_new        (cint fd, CIOCondition condition);
cuint           c_unix_fd_add_full          (cint priority, cint fd, CIOCondition condition, CUnixFDSourceFunc function, void* udata, CDestroyNotify notify);
cuint           c_unix_fd_add               (cint fd, CIOCondition condition, CUnixFDSourceFunc function, void* udata);
struct passwd*  c_unix_get_passwd_entry     (const char* userName, CError** error);

CMainContext*   c_main_context_new                              (void);
CMainContext*   c_main_context_new_with_flags                   (CMainContextFlags flags);
CMainContext*   c_main_context_ref                              (CMainContext* context);
void            c_main_context_unref                            (CMainContext* context);
CMainContext*   c_main_context_default                          (void);
bool            c_main_context_iteration                        (CMainContext* context, bool mayBlock);
bool            c_main_context_pending                          (CMainContext* context);
CSource*        c_main_context_find_source_by_id                (CMainContext* context, cuint sourceId);
CSource*        c_main_context_find_source_by_user_data         (CMainContext* context, void* udata);
CSource*        c_main_context_find_source_by_funcs_user_data   (CMainContext* context, CSourceFuncs* funcs, void* udata);
void            c_main_context_wakeup                           (CMainContext* context);
bool            c_main_context_acquire                          (CMainContext* context);
void            c_main_context_release                          (CMainContext* context);
bool            c_main_context_is_owner                         (CMainContext* context);
bool            c_main_context_wait                             (CMainContext* context, CCond* cond, CMutex* mutex);
bool            c_main_context_prepare                          (CMainContext* context, cint* priority);
cint            c_main_context_query                            (CMainContext* context, cint maxPriority, cint* timeout_, CPollFD* fds, cint nFds);
bool            c_main_context_check                            (CMainContext* context, cint maxPriority, CPollFD* fds, cint nFds);
void            c_main_context_dispatch                         (CMainContext* context);
void            c_main_context_set_poll_func                    (CMainContext* context, CPollFunc func);
CPollFunc       c_main_context_get_poll_func                    (CMainContext* context);
void            c_main_context_add_poll                         (CMainContext* context, CPollFD* fd, cint priority);
void            c_main_context_remove_poll                      (CMainContext* context, CPollFD* fd);
cint            c_main_depth                                    (void);
CSource*        c_main_current_source                           (void);
void            c_main_context_push_thread_default              (CMainContext* context);
void            c_main_context_pop_thread_default               (CMainContext* context);
CMainContext*   c_main_context_get_thread_default               (void);
CMainContext*   c_main_context_ref_thread_default               (void);


CMainLoop*      c_main_loop_new         (CMainContext* context, bool isRunning);
void            c_main_loop_run         (CMainLoop* loop);
void            c_main_loop_quit        (CMainLoop* loop);
CMainLoop*      c_main_loop_ref         (CMainLoop* loop);
void            c_main_loop_unref       (CMainLoop* loop);
bool            c_main_loop_is_running  (CMainLoop* loop);
CMainContext*   c_main_loop_get_context (CMainLoop* loop);


CSource*        c_source_new                        (CSourceFuncs* sourceFuncs, cuint structSize);
void            c_source_set_dispose_function       (CSource* source, CSourceDisposeFunc  dispose);
CSource*        c_source_ref                        (CSource* source);
void            c_source_unref                      (CSource* source);
cuint           c_source_attach                     (CSource* source, CMainContext* context);
void            c_source_destroy                    (CSource* source);
void            c_source_set_priority               (CSource* source, cint priority);
cint            c_source_get_priority               (CSource* source);
void            c_source_set_can_recurse            (CSource* source, bool canRecurse);
bool            c_source_get_can_recurse            (CSource* source);
cuint           c_source_get_id                     (CSource* source);
CMainContext*   c_source_get_context                (CSource* source);
void            c_source_set_callback               (CSource* source, CSourceFunc func, void* data, CDestroyNotify notify);
void            c_source_set_funcs                  (CSource* source, CSourceFuncs* funcs);
bool            c_source_is_destroyed               (CSource* source);
void            c_source_set_name                   (CSource* source, const char* name);
void            c_source_set_static_name            (CSource* source, const char* name);
const char*     c_source_get_name                   (CSource* source);
void            c_source_set_name_by_id             (cuint tag, const char* name);
void            c_source_set_ready_time             (CSource* source, cint64 readyTime);
cint64          c_source_get_ready_time             (CSource* source);
void*           c_source_add_unix_fd                (CSource* source, cint fd, CIOCondition events);
void            c_source_modify_unix_fd             (CSource* source, void* tag, CIOCondition newEvents);
void            c_source_remove_unix_fd             (CSource* source, void* tag);
CIOCondition    c_source_query_unix_fd              (CSource* source, void* tag);
void            c_source_set_callback_indirect      (CSource* source, void* callbackData, CSourceCallbackFuncs* callbackFuncs);
void            c_source_add_poll                   (CSource* source, CPollFD* fd);
void            c_source_remove_poll                (CSource* source, CPollFD* fd);
void            c_source_add_child_source           (CSource* source, CSource* childSource);
void            c_source_remove_child_source        (CSource* source, CSource* childSource);
void            c_source_get_current_time           (CSource* source, CTimeVal* timeval);
cint64          c_source_get_time                   (CSource* source);
CSource*        c_idle_source_new                   (void);
CSource*        c_child_watch_source_new            (CPid pid);
CSource*        c_timeout_source_new                (cuint interval);
CSource*        c_timeout_source_new_seconds        (cuint interval);
void            c_get_current_time                  (CTimeVal* result);
cint64          c_get_monotonic_time                (void);
cint64          c_get_real_time                     (void);
bool            c_source_remove                     (cuint tag);
bool            c_source_remove_by_user_data        (void* udata);
bool            c_source_remove_by_funcs_user_data  (CSourceFuncs* funcs, void* udata);


cuint    c_timeout_add_full         (cint priority, cuint interval, CSourceFunc function, void* data, CDestroyNotify notify);
cuint    c_timeout_add              (cuint interval, CSourceFunc function, void* data);
cuint    c_timeout_add_once         (cuint interval, CSourceOnceFunc function, void* data);
cuint    c_timeout_add_seconds_full (cint priority, cuint interval, CSourceFunc function, void* data, CDestroyNotify notify);
cuint    c_timeout_add_seconds      (cuint interval, CSourceFunc function, void* data);
cuint    c_child_watch_add_full     (cint priority, CPid pid, CChildWatchFunc function, void* data, CDestroyNotify notify);
cuint    c_child_watch_add          (CPid pid, CChildWatchFunc function, void* data);
cuint    c_idle_add                 (CSourceFunc function, void* data);
cuint    c_idle_add_full            (cint priority, CSourceFunc function, void* data, CDestroyNotify notify);
cuint    c_idle_add_once            (CSourceOnceFunc function, void* data);
bool     c_idle_remove_by_data      (void* data);
void     c_main_context_invoke_full (CMainContext* context, cint priority, CSourceFunc function, void* data, CDestroyNotify notify);
void     c_main_context_invoke      (CMainContext* context, CSourceFunc function, void* data);




static inline CMainContextPusher* c_main_context_pusher_new (CMainContext *mainContext)
{
    c_main_context_push_thread_default (mainContext);
    return (CMainContextPusher*) mainContext;
}

static inline void c_main_context_pusher_free (CMainContextPusher* pusher)
{
    c_main_context_pop_thread_default ((CMainContext*) pusher);
}

static inline int c_steal_fd (int* fdPtr)
{
    int fd = *fdPtr;
    *fdPtr = -1;
    return fd;
}

extern CSourceFuncs c_timeout_funcs;
extern CSourceFuncs c_child_watch_funcs;
extern CSourceFuncs c_idle_funcs;
extern CSourceFuncs c_unix_signal_funcs;
extern CSourceFuncs c_unix_fd_source_funcs;

C_END_EXTERN_C

#endif //CLIBRARY_SOURCE_H
