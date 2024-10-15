
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

#include "wakeup.h"
#include "error.h"
#include "log.h"
#include "source.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>


struct _CWakeup
{
    cint fds[2];
};



CWakeup* c_wakeup_new (void)
{
    CError* error = NULL;
    CWakeup* wakeup = c_malloc0(sizeof (CWakeup));

    wakeup->fds[0] = -1;

    if (wakeup->fds[0] != -1) {
        wakeup->fds[1] = -1;
        return wakeup;
    }

    if (!c_unix_open_pipe (wakeup->fds, FD_CLOEXEC, &error)) {
        C_LOG_ERROR_CONSOLE("Creating pipes for GWakeup: %s", error->message);
    }

    if (!c_unix_set_fd_nonblocking (wakeup->fds[0], true, &error)
        || !c_unix_set_fd_nonblocking (wakeup->fds[1], true, &error)) {
        C_LOG_ERROR_CONSOLE("Set pipes non-blocking for GWakeup: %s", error->message);
    }

    return wakeup;
}

void c_wakeup_free (CWakeup* wakeup)
{
    c_return_if_fail(wakeup);

    close (wakeup->fds[0]);

    if (wakeup->fds[1] != -1) {
        close (wakeup->fds[1]);
    }

    c_free(wakeup);
}

void c_wakeup_get_pollfd (CWakeup* wakeup, CPollFD* pollFd)
{
    pollFd->fd = wakeup->fds[0];
    pollFd->events = C_IO_IN;
}

void c_wakeup_signal (CWakeup* wakeup)
{
    int res;

    if (wakeup->fds[1] == -1) {
        uint64_t one = 1;
        do {
            res = (int) write (wakeup->fds[0], (void*) &one, (csize) sizeof (one));
        }
        while (C_UNLIKELY (res == -1 && errno == EINTR));
    }
    else {
        uint8_t one = 1;
        do {
            res = (int) write (wakeup->fds[1], (void*) &one, (csize) sizeof (one));
        }
        while (C_UNLIKELY (res == -1 && errno == EINTR));
    }
}

void c_wakeup_acknowledge (CWakeup* wakeup)
{
    if (wakeup->fds[1] == -1) {
        uint64_t value;
        while (read (wakeup->fds[0], &value, sizeof (value)) == sizeof (value));
    }
    else {
        uint8_t value;
        while (read (wakeup->fds[0], &value, sizeof (value)) == sizeof (value));
    }
}
