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

#ifndef _HIETCD_RESPONSE_H_
#define _HIETCD_RESPONSE_H_

#include <curl/curl.h>

#define ETCD_DATA_BUFSIZE (1024*4)
#define ETCD_ERR_BUFSIZE 256

#define ETCD_OK 0
#define ETCD_ERR -1
#define ETCD_ERR_CURL -2 /* CURL error */
#define ETCD_ERR_PROTOCOL -3 /* Protocol error */
#define ETCD_ERR_RESPONSE -4 /* Etcd response error */

/* Etcd response headers */
#define ETCD_HEADER_ECID "X-Etcd-Cluster-Id"
#define ETCD_HEADER_EIDX "X-Etcd-Index"
#define ETCD_HEADER_RIDX "X-Raft-Index"
#define ETCD_HEADER_RTERM "X-Raft-Term"

/* Etcd response actions */
#define ETCD_ACTION_SET "set"
#define ETCD_ACTION_CREATE "create"
#define ETCD_ACTION_UPDATE "update"
#define ETCD_ACTION_DELETE "delete"

/* Etcd node structure */
typedef struct etcd_node {
    int isdir;
    int ttl; /* time to live */
    long long cidx; /* created Index */
    long long midx; /* modified Index */
    char expr[32]; /* expiration */
    char *key;
    char *value;
    long long ccount; /* number of childs */
    struct etcd_node *snode; /* sibling node */
    struct etcd_node *cnode; /* child node */
} etcd_node;

/* Etcd response structure */
typedef struct {
    CURLcode ccode; /* CURLcode */
    int hcode; /* http status code */
    int errcode; /* response error code */
    char errmsg[ETCD_ERR_BUFSIZE]; /* response error message */
    /* response headers */
    char cluster[32]; /* cluster id */
    long long idx; /* etcd index */
    long long ridx; /* raft index */
    long long rterm; /* raft term */
    /* response data */
    char data[ETCD_DATA_BUFSIZE];
    char action[8];
    etcd_node *node;
    etcd_node *pnode; /* prev node */
} etcd_response;

etcd_node *etcd_node_create(void);
void etcd_node_destroy(etcd_node *node);
etcd_response *etcd_response_create(void);
void etcd_response_cleanup(etcd_response *resp);
void etcd_response_destroy(etcd_response *resp);
size_t etcd_response_header_cb(char *buffer, size_t size, size_t nitems, void *userdata);
size_t etcd_response_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata);
int etcd_response_parse(etcd_response *resp);

#endif
