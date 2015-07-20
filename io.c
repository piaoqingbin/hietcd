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
#include "log.h"
#include "io.h"
#include "request.h"
#include "response.h"

static void etcd_io_cron(sev_pool *pool); 
static void etcd_io_read(sev_pool *pool, int fd, void *data, int flgs);
static void etcd_io_dispatch(etcd_io *io, etcd_request *req);
static int etcd_io_sock_cb(CURL *ch, curl_socket_t s, int what, 
        void *cbp, void *sockp);
static int etcd_io_timer_cb(CURLM *cmh, long timeout_ms, etcd_io *io);

etcd_io *etcd_io_create(void)
{
    etcd_io *io;
    
    if (!(io = malloc(sizeof(etcd_io))))
        return NULL;

    io->ready = 0;
    io->rfd = -1;
    io->size = -1;
    io->pool = NULL;
    io->cmh = NULL;
    memset(&io->elt, 0, sizeof(struct timeval));
    etcd_rq_init(&io->rq);
    pthread_mutex_init(&io->rqlock, NULL);

    pthread_cond_init(&io->cond, 0);
    pthread_mutex_init(&io->lock, 0);

    return io;
}

void etcd_io_destroy(etcd_io *io)
{
    if (io->pool) 
        sev_pool_destroy(io->pool);
    if (io->cmh)
        curl_multi_cleanup(io->cmh);
    pthread_mutex_destroy(&io->rqlock);
    pthread_mutex_destroy(&io->lock);
    pthread_cond_destroy(&io->cond);
    close(io->rfd);
    free(io);
}

static void etcd_io_read(sev_pool *pool, int fd, void *data, int flgs)
{
    char buf[1];
    if (read(fd, buf, sizeof(buf)) == 1) {
        etcd_io *io = (etcd_io *) data;
        etcd_request *req;

        fprintf(stderr, "read:%s\n", buf);
        req = etcd_io_pop_request(io);
        if (req != NULL) {
            fprintf(stderr, "req->url:%s\n", req->url);
            etcd_io_dispatch(io, req);
            etcd_request_destroy(req);
        }
    }
}

static void etcd_io_cron(sev_pool *pool) 
{
    fprintf(stderr, "polling...\n");
}

static void etcd_io_dispatch(etcd_io *io, etcd_request *req)
{
    CURLMcode code;
    CURL *ch;
    etcd_response *resp;

    if ((resp = etcd_response_create()) == NULL) {
        ETCD_LOG_ERROR("Failed to create response");
        return;
    }

    ch = curl_easy_init();
    if (!ch) {
        ETCD_LOG_ERROR("Failed to init curl handler");
        goto io_dispatch_err;
    }

    curl_easy_setopt(ch, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(ch, CURLOPT_FORBID_REUSE, 1);
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(ch, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
    curl_easy_setopt(ch, CURLOPT_TIMEOUT, 1);
    curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, 1);

    curl_easy_setopt(ch, CURLOPT_URL, req->url);
    curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, req->method);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, etcd_response_write_cb);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, resp->data);
    //curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, resp->errmsg);
    //curl_easy_setopt(conn->easy, CURLOPT_PRIVATE, conn);
    //curl_easy_setopt(conn->easy, CURLOPT_NOPROGRESS, 0L);
    //curl_easy_setopt(conn->easy, CURLOPT_PROGRESSFUNCTION, prog_cb);
    //curl_easy_setopt(conn->easy, CURLOPT_PROGRESSDATA, conn);

    code = curl_multi_add_handle(io->cmh, ch);
    if (code != CURLM_OK) {
        ETCD_LOG_ERROR("Failed to dispatch request: %d", code);
        goto io_dispatch_err;
    }

    return;

io_dispatch_err:
    etcd_response_destroy(resp);
}

static int etcd_io_sock_cb(CURL *ch, curl_socket_t s, int what, 
    void *cbp, void *sockp)
{
    etcd_io *io = (etcd_io *) cbp; 

    fprintf(stderr, "what:%d\n", what);
    return 0;
}

static int etcd_io_timer_cb(CURLM *cmh, long timeout_ms, etcd_io *io)
{
    /*
    struct timeval timeout;
    (void)multi;
 
    timeout.tv_sec = timeout_ms/1000;
    timeout.tv_usec = (timeout_ms%1000)*1000;
    fprintf(MSG_OUT, "multi_timer_cb: Setting timeout to %ld ms\n", timeout_ms);
    evtimer_add(g->timer_event, &timeout);
    */
    fprintf(stderr, "multi_timer_cb\n");
    return 0;
}

void *etcd_io_start(void *args)
{
    etcd_io *io = (etcd_io *) args;
    io->pool = sev_pool_create(io->size);
    sev_add_event(io->pool, io->rfd, SEV_R, etcd_io_read, args);
    sev_set_cron(io->pool, etcd_io_cron);

    io->cmh = curl_multi_init();
    curl_multi_setopt(io->cmh, CURLMOPT_SOCKETFUNCTION, etcd_io_sock_cb);
    curl_multi_setopt(io->cmh, CURLMOPT_SOCKETDATA, io);
    curl_multi_setopt(io->cmh, CURLMOPT_TIMERFUNCTION, etcd_io_timer_cb);
    curl_multi_setopt(io->cmh, CURLMOPT_TIMERDATA, io);
    
    // notify
    pthread_mutex_lock(&io->lock);
    io->ready = 1;
    pthread_cond_broadcast(&io->cond);
    pthread_mutex_unlock(&io->lock);

    // dispatch
    ETCD_LOG_DEBUG("Started IO thread");
    sev_dispatch(io->pool, &io->elt);
    ETCD_LOG_DEBUG("IO thread terminated");
    return NULL;
}

void etcd_io_stop(etcd_io *io)
{
    io->pool->done = 1;
}

void etcd_io_push_request(etcd_io *io, etcd_request *req)
{
    pthread_mutex_lock(&io->rqlock);
    etcd_rq_insert(&io->rq, &req->rq);
    pthread_mutex_unlock(&io->rqlock);
}

etcd_request *etcd_io_pop_request(etcd_io *io)
{
    etcd_request *req = NULL;
    pthread_mutex_lock(&io->rqlock);
    if (!etcd_rq_empty(&io->rq)) {
        etcd_rq *rq = etcd_rq_last(&io->rq); 
        req = etcd_rq_getreq(rq);
        etcd_rq_remove(rq); 
    }
    pthread_mutex_unlock(&io->rqlock);
    return req;
}
