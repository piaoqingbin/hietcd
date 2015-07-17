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
#ifndef _SEV_H_
#define _SEV_H_

#define SEV_OK 0
#define SEV_ERR -1

#define SEV_N 0 /* Null event flag */
#define SEV_R 1 /* Readable event flag */
#define SEV_W 2 /* Writable event flag */

/* Macros */
#define sev_stop(p) ((p)->done = 1)
#define sev_set_cron(p,i) ((p)->cron = (i))

/* Event pool */
struct sev_pool;

/* Event handlers */
typedef void sev_file_proc(struct sev_pool *pool, int fd, void *data, int flgs);
typedef void sev_cron_proc(struct sev_pool *pool);

/* File event */
typedef struct {
    int flgs; /* (Readable/Writable) flags */
    sev_file_proc *read; /* Readable event handler */
    sev_file_proc *write; /* Writable event handler */
    void *data;
} sev_file_event;

typedef struct {
    int fd;
    int flgs;
} sev_ready_event;

/* Event pool structure */
typedef struct sev_pool {
    int done;
    int size;
    int maxfd;
    void *impl; /* Polling implementation */
    sev_file_event *events;
    sev_ready_event *ready;
    sev_cron_proc *cron; 
} sev_pool;

sev_pool *sev_pool_create(int size);
void sev_pool_destroy(sev_pool *pool);
int sev_add_event(sev_pool *pool, int fd, int flgs, sev_file_proc *proc, void *data);
void sev_del_event(sev_pool *pool, int fd, int flgs);
int sev_process(sev_pool *pool, struct timeval *tvp);
void sev_dispatch(sev_pool *pool, struct timeval *tvp);

#endif
