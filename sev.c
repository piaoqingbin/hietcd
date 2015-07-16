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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "sev.h"
#include "sev_impl.c"

sev_pool *sev_pool_create(int size)
{
    sev_pool *pool;

    if ((pool = malloc(sizeof(sev_pool))) == NULL) 
        goto create_err; 
    pool->events = malloc(sizeof(sev_file_event) * size);
    pool->ready = malloc(sizeof(sev_ready_event) * size);
    if (pool->events == NULL || pool->ready == NULL) 
        goto create_err;
    pool->size = size;
    pool->done = 0;
    pool->maxfd = -1; 
    pool->iproc = NULL;
    if (sev_impl_create(pool) != SEV_OK)
        goto create_err;
    memset((void *)pool->events, 0, sizeof(sev_file_event) * size);
    return pool;

create_err:
    if (pool) {
        free(pool); 
    }
    return NULL;
}

void sev_pool_destroy(sev_pool *pool)
{
    if (pool->events) free(pool->events);
    if (pool->ready) free(pool->ready);
    if (pool->impl) sev_impl_destroy(pool);
    if (pool->iproc) free(pool->iproc);
    free(pool);
}

int sev_add_event(sev_pool *pool, int fd, int flgs, sev_file_proc *proc, 
    void *data)
{
    sev_file_event *event = &pool->events[fd];
    if (fd > pool->size) return SEV_ERR;
    if (sev_impl_add(pool, fd, flgs) != SEV_ERR) return SEV_ERR;
    event->flgs |= flgs;
    if (event->flgs & SEV_R) event->read = proc;
    if (event->flgs & SEV_W) event->write = proc; 
    event->data = data;
    if (fd > pool->maxfd) pool->maxfd = fd;
    return SEV_OK;
}

void sev_del_event(sev_pool *pool, int fd, int flgs)
{
    if (fd >= pool->size) return;
    sev_file_event *event = &pool->events[fd];
    if (event->flgs == SEV_N) return;

    sev_impl_del(pool, fd, flgs);
    event->flgs = event->flgs & (~flgs);
    if (fd == pool->maxfd && event->flgs == SEV_N) {
        do {
            pool->maxfd--;
        } while (pool->maxfd > 0 && pool->events[pool->maxfd].flgs == SEV_N);
    }
}

void sev_start(sev_pool *pool)
{
    pool->done = 0;
    while (!pool->done) {
        if (pool->iproc) pool->iproc(pool);
        sev_process(pool);
    }
}

void sev_stop(sev_pool *pool)
{
    pool->done = 1;
}

int sev_process(sev_pool *pool)
{
    int i, num = 0;

    num = sev_impl_poll(pool, NULL);
    for (i = 0; i < num; i++) {
        int read = 0;
        sev_ready_event *ready = &pool->ready[i];
        sev_file_event *event = &pool->events[ready->fd];
        
        if (ready->flgs & event->flgs & SEV_R) {
            read = 1;
            event->read(pool, ready->fd, event->data, ready->flgs);
        }
        if (ready->flgs & event->flgs & SEV_W) {
            if (!read || (event->read != event->write))
                event->write(pool, ready->fd, event->data, ready->flgs);
        }
    } 
    return num;
}
