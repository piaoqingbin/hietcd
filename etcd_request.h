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

#ifndef _HIETCD_REQUEST_H_
#define _HIETCD_REQUEST_H_

/* Etcd request methods */
#define HIETCD_REQUEST_GET "GET"
#define HIETCD_REQUSET_POST "POST"
#define HIETCD_REQUEST_PUT "PUT"
#define HIETCD_REQUEST_DELETE "DELETE"

/* Etcd request queue */
typedef struct etcd_request_queue etcd_rq;

struct etcd_request_queue {
    etcd_rq *prev; 
    etcd_rq *next; 
};

#define etcd_rq_init(q)             \
    (q)->prev = (q);                \
    (q)->next = (q)

#define etcd_rq_insert(h,n)         \
    (h)->next->prev = (n);          \
    (n)->next = (h)->next;          \
    (n)->prev = (h);                \
    (h)->next = (n)

#define etcd_rq_remove(n)           \
    (n)->next->prev = (n)->prev;    \
    (n)->prev->next = (n)->next

#define etcd_rq_empty(h)    ((h) == (h)->next)
#define etcd_rq_head(h)     ((h)->next)
#define etcd_rq_last(h)     ((h)->prev)
#define etcd_rq_prev(q)     ((q)->prev)
#define etcd_rq_next(q)     ((q)->next)

/* Etcd request structure */
typedef struct {
    char *url; /* http://host:port/path/to/key?foo=bar */
    const char *method; /* http method */
    const char *certfile;
    char *data;
    etcd_rq rq; 
} etcd_request;

#define etcd_request_set_data(r,d,l)    ((r)->data = strndup((d),(l)))
#define etcd_request_set_certfile(r,c)  ((r)->certfile = (c))

etcd_request *etcd_request_create(char *url, size_t len, const char *method);
void etcd_request_destroy(etcd_request *req);

#endif
