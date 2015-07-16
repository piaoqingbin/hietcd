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

#include <sys/epoll.h>

#define SEV_IMPL_NAME "epoll"

typedef struct {
    int epfd;
    struct epoll_event *ee;
} sev_impl;


static int sev_impl_create(sev_pool *pool)
{
    sev_impl *impl;
    
    if (!(impl = malloc(sizeof(sev_impl))))
        goto impl_create_err;

    if (!(impl->ee = malloc(sizeof(struct epoll_event)*pool->size)))
        goto impl_create_err;

    if ((impl->epfd = epoll_create(1024)) == -1)
        goto impl_create_err;

    pool->impl = impl;
    return SEV_OK;

impl_create_err:
    if (impl) {
        if (impl->events) free(impl->events);
        free(impl); 
    }
    return SEV_ERR;
}

static void sev_impl_destroy(sev_pool *pool)
{
    close(pool->impl->epfd);
    free(pool->impl->ee);
    free(pool->impl); 
}

static int sev_impl_add(sev_pool *pool, int fd, int flgs)
{
    sev_impl *impl = pool->impl;
    struct epoll_event ee;
    int op;
    
    ee.events = 0;
    if (flgs & SEV_R) ee.events |= EPOLLIN;
    if (flgs & SEV_W) ee.events |= EPOLLOUT;
    ee.data.u64 = 0;
    ee.data.fd = fd;

    if (pool->events[fd].flgs == SEV_N)
        op = EPOLL_CTL_ADD; 
    else
        op = EPOLL_CTL_MOD; 

    if (epoll_ctl(impl->epfd, op, fd, &ee) == -1)  
        return SEV_ERR;

    return SEV_OK;
}

static void sev_impl_del(sev_pool *pool, int fd, int flgs)
{
    sev_impl *impl = pool->impl;
    struct epoll_event ee;
    int op;
    
    ee.events = 0;
    if (flgs & SEV_R) ee.events |= EPOLLIN;
    if (flgs & SEV_W) ee.events |= EPOLLOUT;
    ee.data.u64 = 0;
    ee.data.fd = fd;

    if (pool->events[fd].flgs & (~flgs) == SEV_N)
        op = EPOLL_CTL_DEL;
    else
        op = EPOLL_CTL_MOD;

    epoll_ctl(impl->epfd, EPOLL_CTL_MOD, fd, &ee);
}

static int sev_impl_poll(sev_pool *pool, struct timeval *tvp)
{
    sev_impl *impl = pool->impl;
    int i, num = 0, timeout = -1;

    if (tvp) timeout = (tvp->tv_sec*1000) + (tvp->tv_usec/1000);

    num = epoll_wait(impl->epfd, impl->ee, pool->size, timeout);
    if (num > 0) {
        int flgs;
        struct epoll_event *ee;

        for (i = 0; i < num; i++) {
            flgs = 0;
            ee = impl->ee[i];

            if (ee->events & EPOLLIN) 
                flgs |= SEV_R;

            if (ee->events & EPOLLOUT || ee->events & EPOLLERR || 
                    ee->events & EPOLLHUP)
                flgs |= SEV_W;

            pool->ready[i].fd = ee->data.fd;
            pool->ready[i].flgs = flgs;
        }
    }

    return num;
}

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

static void sev_impl_destroy(sev_pool *pool)
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

#endif
