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

#ifndef _HIETCD_IO_H_
#define _HIETCD_IO_H_

#include <pthread.h>

#include <curl/curl.h>

#include "sev.h"
#include "request.h"
#include "hietcd.h"

typedef struct etcd_io etcd_io;

/* Etcd http io structure */
struct etcd_io {
    int ready;
    int rfd; /* Readable pipe fd */
    int size; /* Event pool size */
    int running; /* Still running */
    long long tid; /* Timer id */
    struct timeval elt; /* Event loop timeout */
    sev_pool *pool; /* Event pool */
    struct etcd_client *client; /* Global config */
    CURLM *cmh; /* CURL multi handler */
    /* Request queue */
    etcd_rq rq;
    pthread_mutex_t rqlock;
    /* cond&lock */
    pthread_cond_t cond;
    pthread_mutex_t lock;
};

/* Etcd socket info structure */
typedef struct etcd_io_sock {
    int action;
    curl_socket_t fd;
    CURL *ch;
} etcd_io_sock;

etcd_io *etcd_io_create(void);
void etcd_io_destroy(etcd_io *io);
void *etcd_io_start(void *args);
void etcd_io_stop(etcd_io *io);
void etcd_io_push_request(etcd_io *io, etcd_request *req);
etcd_request *etcd_io_pop_request(etcd_io *io);

#endif
