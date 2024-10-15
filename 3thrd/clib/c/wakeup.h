
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

#ifndef CLIBRARY_WAKEUP_H
#define CLIBRARY_WAKEUP_H
#if !defined (__CLIB_H_INSIDE__) && !defined (CLIB_COMPILATION)
#error "Only <clib.h> can be included directly."
#endif

#include <c/poll.h>
#include <c/macros.h>

C_BEGIN_EXTERN_C

typedef struct _CWakeup CWakeup;

CWakeup*    c_wakeup_new            (void);
void        c_wakeup_free           (CWakeup* wakeup);
void        c_wakeup_get_pollfd     (CWakeup* wakeup, CPollFD* pollFd);
void        c_wakeup_signal         (CWakeup* wakeup);
void        c_wakeup_acknowledge    (CWakeup* wakeup);

C_END_EXTERN_C


#endif //CLIBRARY_WAKEUP_H
