/*
 * Copyright (c) 2014-2015, Qingbin Piao <piaoqingbin at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifdef HAVE_EPOLL


#else

#include <sys/select.h>

#define SEV_IMPL_NAME "select"

typedef struct {
    fd_set rset, _rset;
    fd_set wset, _wset;
} sev_impl;

static int sev_impl_create(sev_pool *pool)
{
    sev_impl *impl = malloc(sizeof(sev_impl));
    FD_ZERO(&impl->rset);
    FD_ZERO(&impl->wset);
    pool->impl = impl; 
    return SEV_OK;
}

static void sev_impl_free(sev_pool *pool)
{
    free(pool->impl);
}

static int sev_impl_add(sev_pool *pool, int fd, int flgs)
{
    sev_impl *impl = pool->impl;
    if (flgs & SEV_R) FD_SET(fd, &impl->rset);
    if (flgs & SEV_W) FD_SET(fd, &impl->wset);
    return SEV_OK;
}

static void sev_impl_del(sev_pool *pool, int fd, int flgs)
{
    sev_impl *impl = pool->impl;
    if (flgs & SEV_R) FD_CLR(fd, &impl->rset);
    if (flgs & SEV_W) FD_CLR(fd, &impl->wset);
}

static int sev_impl_poll(sev_pool *pool, struct timeval *tvp)
{
    sev_impl *impl = pool->impl;
    int fd, num = 0; /* number of events */

    memcpy(&impl->_rset, &impl->rset, sizeof(fd_set));
    memcpy(&impl->_wset, &impl->wset, sizeof(fd_set));

    if (select(pool->maxfd+1, &impl->_rset, &impl->_wset, NULL, tvp) > 0) {
        for (fd = 0; fd <= pool->maxfd; fd++, num++) {
            int flgs = 0;
            sev_file_event *event = &pool->events[fd];  

            if (event->flgs == SEV_N) continue;
            pool->ready[num].fd = fd;
            if (event->flgs & SEV_R && FD_ISSET(fd, &impl->_rset)) 
                flgs |= SEV_R;
            if (event->flgs & SEV_W && FD_ISSET(fd, &impl->_wset)) 
                flgs |= SEV_W;
            pool->ready[num].flgs = flgs;
        }
    }
    return num;
}

static void sev_impl_destroy(sev_pool *pool)
{
    free(pool->impl);
}

#endif
