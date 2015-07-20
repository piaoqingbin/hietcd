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
#include <sys/time.h>

#include "sev.h"
#include "sev_impl.c"

static inline void sev_time_now(long *sec, long *msec);
static void sev_time_add_now(long long time_ms, long *sec, long *msec);
static int sev_timer_cmp(sev_timer *tm, sev_timer *ts);
static inline void sev_timer_swap(sev_timer **tm, sev_timer **ts);
static int sev_timers_resize(sev_pool *pool, int flgs);

sev_pool *sev_pool_create(int size)
{
    sev_pool *pool;
    sev_timer **timers;

    if ((pool = malloc(sizeof(sev_pool))) == NULL) 
        goto create_err; 
    pool->events = malloc(sizeof(sev_file_event) * size);
    pool->ready = malloc(sizeof(sev_ready_event) * size);
    if (pool->events == NULL || pool->ready == NULL) 
        goto create_err;
    pool->size = size;
    pool->done = 0;
    pool->maxfd = -1; 
    pool->cron = NULL;
    if (sev_impl_create(pool) != SEV_OK)
        goto create_err;
    memset((void *)pool->events, 0, sizeof(sev_file_event) * size);

    pool->tmaxid = 0;
    pool->tnum = 0;
    pool->tmaxnum = SEV_DEFAULT_HEAPSIZE;
    if ((timers = malloc(sizeof(sev_timer*) * pool->tmaxnum)) == NULL)
        goto create_err;

    memset((void *)timers, 0, sizeof(sev_timer*) * pool->tmaxnum);
    pool->timers = timers;

    return pool;

create_err:
    if (pool) {
        if (pool->events) free(pool->events);
        if (pool->ready) free(pool->ready);
        free(pool); 
    }
    return NULL;
}

void sev_pool_destroy(sev_pool *pool)
{
    if (pool->events) free(pool->events);
    if (pool->ready) free(pool->ready);
    if (pool->impl) sev_impl_destroy(pool);
    if (pool->timers) free(pool->timers);
    free(pool);
}

int sev_add_event(sev_pool *pool, int fd, int flgs, sev_file_proc *proc, 
    void *data)
{
    sev_file_event *event = &pool->events[fd];
    if (fd > pool->size) return SEV_ERR;
    if (sev_impl_add(pool, fd, flgs) != SEV_OK) return SEV_ERR;

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

static inline void sev_time_now(long *sec, long *msec)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    *sec = tv.tv_sec;
    *msec = tv.tv_usec/1000;
}

static void sev_time_add_now(long long time_ms, long *sec, long *msec)
{
    sev_time_now(sec, msec);
    *sec += time_ms / 1000;
    *msec += time_ms % 1000;
    if (*msec > 1000) {
        *sec += 1;
        *msec -= 1000; 
    }
}

/* [tm>ts,1|tm<ts,-1|tm==ts,0] */
static int sev_timer_cmp(sev_timer *tm, sev_timer *ts)
{
    if (tm->sec == ts->sec) {
        if (tm->msec == ts->msec)
            return 0; 
        else if (tm->msec > ts->msec)
            return 1;
        else
            return -1;
    } else if (tm->sec > ts->sec)
        return 1; 
    else 
        return -1;
}

static inline void sev_timer_swap(sev_timer **tm, sev_timer **ts)
{
    sev_timer *tmp;
    tmp = *tm;
    *tm = *ts; 
    *ts = tmp;
}

static int sev_timers_resize(sev_pool *pool, int flgs)
{
    sev_timer **timers;
    long tmaxnum = 0;

    if (flgs > 0) {
        tmaxnum = pool->tmaxnum << 1;
        if (tmaxnum > SEV_MAX_HEAPSIZE) return SEV_ERR;

        timers = malloc(sizeof(sev_timer*) * tmaxnum);
        if (timers == NULL) return SEV_ERR;
        memset(timers, 0, sizeof(sev_timer*) * tmaxnum);

        pool->tmaxnum = tmaxnum;
        memcpy(timers, pool->timers, sizeof(sev_timer*) * pool->tnum);
        free(pool->timers);
        pool->timers = timers;

    } else {
    
    }
    return SEV_OK;
}

long long sev_add_timer(sev_pool *pool, long long timeout_ms, 
        sev_timer_proc *proc, void *data)
{
    sev_timer *timer;
    int i, j;

    if ((timer = malloc(sizeof(sev_timer))) == NULL)
        return 0;

    if (pool->tnum >= pool->tmaxnum) {
        if (sev_timers_resize(pool, 1) != SEV_OK)
            return 0;
    }

    timer->id = ++pool->tmaxid;
    timer->proc = proc;
    timer->data = data;
    sev_time_add_now(timeout_ms, &timer->sec, &timer->msec); 

    i = pool->tnum;
    pool->timers[pool->tnum++] = timer;
    while (i > 0) {
        j = SEV_TIMER_PARENT(i);
        if (sev_timer_cmp(pool->timers[i], pool->timers[j]) < 0)
            sev_timer_swap(&pool->timers[i], &pool->timers[j]);
        i = j;
    }

    return timer->id;
}

int sev_del_timer(sev_pool *pool, long long id)
{
    return SEV_OK;
}

int sev_process(sev_pool *pool, struct timeval *tvp)
{
    int i, num = 0;

    num = sev_impl_poll(pool, tvp);
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

void sev_dispatch(sev_pool *pool, struct timeval *tvp)
{
    pool->done = 0;
    while (!pool->done) {
        if (pool->cron) pool->cron(pool);
        sev_process(pool, tvp);
    }
}
