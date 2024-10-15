
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

#include "poll.h"


cint c_poll (CPollFD* fds, cuint num, cint timeout)
{
    return poll ((struct pollfd*) fds, num, timeout);
}

#if 0
// 使用 select 实现 poll
cint cc_poll (CPollFD* fds, cuint num, cint timeout)
{

    struct timeval tv;
    fd_set rset, wset, xset;
    CPollFD* f = NULL;
    int ready = 0;
    int maxfd = 0;

    FD_ZERO (&rset);
    FD_ZERO (&wset);
    FD_ZERO (&xset);

    for (f = fds; f < &fds[num]; ++f) {
        if (f->fd >= 0) {
            if (f->events & C_IO_IN) {
                FD_SET (f->fd, &rset);
            }
            if (f->events & C_IO_OUT) {
                FD_SET (f->fd, &wset);
            }
            if (f->events & C_IO_PRI) {
                FD_SET (f->fd, &xset);
            }
            if (f->fd > maxfd && (f->events & (C_IO_IN|C_IO_OUT|C_IO_PRI))) {
                maxfd = f->fd;
            }
        }
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    ready = select (maxfd + 1, &rset, &wset, &xset, timeout == -1 ? NULL : &tv);
    if (ready > 0) {
        for (f = fds; f < &fds[num]; ++f) {
            f->rEvents = 0;
            if (f->fd >= 0) {
                if (FD_ISSET (f->fd, &rset)) {
                    f->rEvents |= C_IO_IN;
                }
                if (FD_ISSET (f->fd, &wset)) {
                    f->rEvents |= C_IO_OUT;
                }
                if (FD_ISSET (f->fd, &xset)) {
                    f->rEvents |= C_IO_PRI;
                }
            }
        }
    }

    return ready;
}
#endif
