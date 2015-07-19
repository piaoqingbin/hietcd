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

#ifndef _HIETCD_H_
#define _HIETCD_H_

#include <curl/curl.h>

#include "io.h"

#define HIETCD_OK 0
#define HIETCD_ERR -1

/* Client/Server version */
#define HIETCD_VERSION "0.02"
#define HIETCD_SERVER_VERSION "v2"

#define HIETCD_ERR_CURL -2 /* CURL error */
#define HIETCD_ERR_PROTOCOL -3 /* Protocol error */
#define HIETCD_ERR_RESPONSE -4 /* Etcd response error */

#define HIETCD_MAX_NODE_NUM 11

#define HIETCD_DEFAULT_TIMEOUT 30
#define HIETCD_DEFAULT_CONNTIMEOUT 1
#define HIETCD_DEFAULT_KEEPALIVE 1

/* Request/Response buffer size */
#define HIETCD_REQ_BUFSIZE (1024*1)
#define HIETCD_RESP_BUFSIZE (1024*4)
#define HIETCD_ERR_BUFSIZE 128

/* Etcd response headers */
#define HIETCD_HEADER_ECID "X-Etcd-Cluster-Id"
#define HIETCD_HEADER_EIDX "X-Etcd-Index"
#define HIETCD_HEADER_RIDX "X-Raft-Index"
#define HIETCD_HEADER_RTERM "X-Raft-Term"

/* Etcd response actions */
#define HIETCD_ACTION_SET "set"
#define HIETCD_ACTION_CREATE "create"
#define HIETCD_ACTION_UPDATE "update"
#define HIETCD_ACTION_DELETE "delete"

/* Etcd node structure */
typedef struct etcd_node {
    char *key;
    char *value;
    int isdir;
    int ttl; /* time to live */
    char expr[32]; /* expiration */
    long long cidx; /* created Index */
    long long midx; /* modified Index */
    struct etcd_node *snode; /* sibling node */
    struct etcd_node *cnode; /* child node */
    unsigned long long ccount; /* number of childs */
} etcd_node;

/* Etcd http response structure */
typedef struct {
    CURLcode ccode; /* CURLcode */
    int hcode; /* http status code */
    int errcode;
    char errmsg[HIETCD_ERR_BUFSIZE];
    /* response headers */
    char cluster[16]; /* cluster id */
    long long idx; /* etcd index */
    long long ridx; /* raft index */
    long long rterm; /* raft term */
    /* response data */
    char data[HIETCD_RESP_BUFSIZE];
    char action[16];
    etcd_node *node;
    etcd_node *pnode; /* prev node */
} etcd_response;

struct etcd_client;

/*
typedef void etcd_response_proc(etcd_client *client, 
    etcd_response *resp, void *data);
*/

/* Etcd client structure */
typedef struct etcd_client {
    short timeout;
    short conntimeout;
    short keepalive;
    short snum; /* number of servers */
    char *certfile;
    char *servers[HIETCD_MAX_NODE_NUM];
    int wfd; /* Writable pipe fd */
    etcd_io *io; /* io thread */
    pthread_t tid; /* thread id */
} etcd_client;

etcd_node *etcd_node_create(void);
void etcd_node_destroy(etcd_node *node);

etcd_response *etcd_response_create(void);
void etcd_response_cleanup(etcd_response *resp);
void etcd_response_destroy(etcd_response *resp);

etcd_client *etcd_client_create(void);
void etcd_client_destroy(etcd_client *client);

int etcd_start_io_thread(etcd_client *client);
void etcd_stop_io_thread(etcd_client *client);

/* Async api */
int etcd_amkdir(etcd_client *client, const char *key, size_t len, long long ttl);

/*
etcd_client *etcd_client_create(void);
void etcd_client_destroy(etcd_client *client);
int etcd_add_server(etcd_client *client, const char *server, size_t len);

int etcd_mkdir(etcd_client *client, const char *key, size_t len, int ttl, etcd_response *resp);

int etcd_send_request(etcd_client *client, const char *method, const char *key,
    const char *query, const char *post, etcd_response *resp);

int32_t etcd_mkdir(etcd_client *client, const char *key, uint64_t ttl, etcd_response *resp);
int32_t etcd_set(etcd_client *client, const char *key, const char *value, uint64_t ttl, etcd_response *resp);
int32_t etcd_get(etcd_client *client, const char *key, etcd_response *resp);
int32_t etcd_delete(etcd_client *client, const char *key, etcd_response *resp);
int32_t etcd_watch(etcd_client *client, const char *key, int32_t nonblock, etcd_response *resp);

int32_t etcd_request_send(etcd_client *client, const char *server, const char *method, 
    const char *key, const char *query, const char *post, etcd_response *resp);
*/

#endif
