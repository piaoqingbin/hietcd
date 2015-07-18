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
#include "request.h"

#include "hio.h"

static void etcd_hio_cron(sev_pool *pool); 
static void etcd_hio_read(sev_pool *pool, int fd, void *data, int flgs);

etcd_hio *etcd_hio_create(void)
{
    etcd_hio *io;
    
    if (!(io = malloc(sizeof(etcd_hio))))
        return NULL;

    io->stop = 0;
    io->rfd = -1;
    io->size = -1;
    io->pool = NULL;
    io->cmh = NULL;
    memset(&io->elt, 0, sizeof(struct timeval));
    etcd_rq_init(&io->rq);
    pthread_mutex_init(&io->rqlock, NULL);

    return io;
}

void etcd_hio_destroy(etcd_hio *io)
{
    if (io->pool) 
        sev_pool_destroy(io->pool);
    if (io->cmh)
        curl_multi_cleanup(io->cmh);
    pthread_mutex_destroy(&io->rqlock);
    free(io);
}

static void etcd_hio_read(sev_pool *pool, int fd, void *data, int flgs)
{
    char buf[32] = {0};
    size_t len = 32;
    read(fd, buf, len);
    fprintf(stderr, "read:%s\n", buf);
}

static void etcd_hio_cron(sev_pool *pool) 
{
    fprintf(stderr, "polling...\n");
}

void *etcd_hio_start(void *args)
{
    etcd_hio *io = (etcd_hio *) args;
    io->cmh = curl_multi_init();
    io->pool = sev_pool_create(io->size);
    sev_add_event(io->pool, io->rfd, SEV_R, etcd_hio_read, NULL);
    sev_set_cron(io->pool, etcd_hio_cron);
    sev_dispatch(io->pool, &io->elt);
    return NULL;
}

void etcd_hio_push_request(etcd_hio *io, etcd_request *req)
{
    pthread_mutex_lock(&io->rqlock);
    etcd_rq_insert(io->rq, req->rq);
    pthread_mutex_unlock(&io->rqlock);
}

etcd_request *etcd_hio_pop_request(etcd_hio *io)
{
    etcd_request *req = NULL;
    pthread_mutex_lock(&io->rqlock);
    if (!etcd_rq_empty(io->rq)) {
        req = etcd_rq_last(io->rq); 
        etcd_rq_remove(req->rq); 
    }
    pthread_mutex_unlock(&io->rqlock);
    return req;
}
