
/*
 * Copyright © 2024 <dingjing@live.cn>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

//
// Created by dingjing on 24-4-24.
//

#ifndef CLIBRARY_TIMER_H
#define CLIBRARY_TIMER_H

#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/macros.h>

C_BEGIN_EXTERN_C

typedef struct _CTimer          CTimer;
typedef struct _CTimeVal        CTimeVal;

struct _CTimer
{
    cuint64 start;
    cuint64 end;

    cuint active : 1;
};

struct _CTimeVal
{
    clong tvSec;
    clong tvUsec;
};

#define C_USEC_PER_SEC 1000000

CTimer*  c_timer_new             (void);
void     c_timer_destroy         (CTimer* timer);
void     c_timer_start           (CTimer* timer);
void     c_timer_stop            (CTimer* timer);
void     c_timer_reset           (CTimer* timer);
void     c_timer_continue        (CTimer* timer);
double   c_timer_elapsed         (CTimer* timer, culong* microseconds);
bool     c_timer_is_active       (CTimer* timer);
void     c_usleep                (culong microseconds);
void     c_time_val_add          (CTimeVal* time_, clong microseconds);
bool     c_time_val_from_iso8601 (const char* isoDate, CTimeVal* time_);
char*    c_time_val_to_iso8601   (CTimeVal* time_) C_MALLOC;


C_END_EXTERN_C


#endif //CLIBRARY_TIMER_H
