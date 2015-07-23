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
#include "response.h"

#define HIETCD_OK 0
#define HIETCD_ERR -1

/* Client/Server version */
#define HIETCD_VERSION "0.02"
#define HIETCD_SERVER_VERSION "v2"

#define HIETCD_MAX_NODE_NUM 11

#define HIETCD_DEFAULT_TIMEOUT 30
#define HIETCD_DEFAULT_CONNTIMEOUT 1
#define HIETCD_DEFAULT_KEEPALIVE 1

#define HIETCD_URL_BUFSIZE 512

typedef struct etcd_client etcd_client;

/* Response processor */
typedef void etcd_response_proc(etcd_client *client, etcd_response *resp, void *userdata);

/* Etcd client structure */
struct etcd_client {
    short timeout;
    short conntimeout;
    short keepalive;
    short snum; /* number of servers */
    int wfd; /* writable pipe fd */
    pthread_t tid; /* thread id */
    char *certfile;
    char *servers[HIETCD_MAX_NODE_NUM];
    struct etcd_io *io; /* io thread */
    etcd_response_proc *proc;
    void *userdata;
};

etcd_client *etcd_client_create(void);
void etcd_client_destroy(etcd_client *client);
void etcd_set_response_proc(etcd_client *client, etcd_response_proc *proc, void *userdata);
int etcd_start_io_thread(etcd_client *client);
void etcd_stop_io_thread(etcd_client *client);

/* Async api */
int etcd_amkdir(etcd_client *client, const char *key, int ttl);
int etcd_aset(etcd_client *client, const char *key, const char *value, size_t len, int ttl);
int etcd_aget(etcd_client *client, const char *key);

/*
int etcd_add_server(etcd_client *client, const char *server, size_t len);

int32_t etcd_delete(etcd_client *client, const char *key, etcd_response *resp);
int32_t etcd_watch(etcd_client *client, const char *key, int32_t nonblock, etcd_response *resp);

*/

#endif
