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
#include <stdlib.h>
#include <string.h>
#include<unistd.h>
#include <pthread.h>

#include <curl/curl.h>

#include "hietcd.h"
#include "request.h"

static inline void etcd_response_init(etcd_response *resp);
static int etcd_gen_url(etcd_client *client, const char *key, char *url);
static inline int etcd_async_send_request(etcd_client *client, 
        etcd_request *req);

etcd_node *etcd_node_create(void)
{
    etcd_node *node;
    
    if (!(node = malloc(sizeof(etcd_node))))
        return NULL;

    node->key = NULL;
    node->value = NULL;
    node->isdir = 0;
    node->ttl = -1;
    node->expr[0] = '\0';
    node->cidx = -1;
    node->midx = -1;
    node->snode = NULL;
    node->cnode = NULL;
    node->ccount = 0;

    return node;
}

void etcd_node_destroy(etcd_node *node)
{
    if (node) {
        if (node->key) free(node->key); 
        if (node->value) free(node->value);
        if (node->snode) 
            etcd_node_destroy(node->snode);
        if (node->cnode)
            etcd_node_destroy(node->cnode);
        free(node);
    }
}

etcd_response *etcd_response_create(void)
{
    etcd_response *resp = malloc(sizeof(etcd_response));
    etcd_response_init(resp);
    return resp;
}

static inline void etcd_response_init(etcd_response *resp)
{
    resp->ccode = CURLE_OK;
    resp->hcode = -1;
    resp->errcode = HIETCD_OK;
    resp->errmsg[0] = '\0';
    resp->cluster[0] = '\0';
    resp->idx = -1;
    resp->ridx = -1;
    resp->rterm = -1;
    resp->data[0] = '\0';
    resp->action[0] = '\0';
    resp->node = NULL;
    resp->pnode = NULL;
}

void etcd_response_cleanup(etcd_response *resp)
{
    if (resp->node) 
        etcd_node_destroy(resp->node);

    if (resp->pnode)
        etcd_node_destroy(resp->pnode);

    etcd_response_init(resp);
}

void etcd_response_destroy(etcd_response *resp)
{
    etcd_response_cleanup(resp);
    free(resp);
}

etcd_client *etcd_client_create(void)
{
    etcd_client *client;
    
    if ((client = malloc(sizeof(etcd_client))) == NULL)
        return NULL; 

    client->timeout = HIETCD_DEFAULT_TIMEOUT;
    client->conntimeout = HIETCD_DEFAULT_TIMEOUT;
    client->keepalive = HIETCD_DEFAULT_KEEPALIVE;
    client->snum = 0;
    client->certfile = NULL;
    client->io = NULL;

    return client;
}

void etcd_client_destroy(etcd_client *client)
{
    while (--client->snum >= 0)
        free(client->servers[client->snum]);
    free(client); 
}

void etcd_async_start(etcd_client *client)
{
    if (client->io == NULL) {
        int fds[2] = {0};

        // create hio
        etcd_hio *io = etcd_hio_create();
        if (!io) {
            // TODO 
        }
        client->io = io;

        if (pipe(fds) == -1) {
            // TODO 
            return;
        }

        client->wfd = fds[1];

        // Configure
        io->rfd = fds[0];
        io->elt.tv_sec = 10;
        io->elt.tv_usec = 0;
        io->size = 1024;

        pthread_create(&io->tid, NULL, etcd_hio_start, (void *)io); 
    }
}

static inline int etcd_async_send_request(etcd_client *client, etcd_request *req)
{
    if (client->io) return HIETCD_ERR;
    if (client->certfile) 
        etcd_request_set_certfile(req, client->certfile);
    etcd_hio_push_request(client->io, req); 
    write(client->wfd, "0", 1);
    return HIETCD_OK;
}

static char *etcd_get_server(etcd_client *client)
{
    return client->servers[0];
}

static int etcd_gen_url(etcd_client *client, const char *key, char *url) 
{
    return snprintf(url, HIETCD_URL_BUFSIZE, "%s/%s/keys%s", 
            etcd_get_server(client), HIETCD_SERVER_VERSION, key);
}

int etcd_amkdir(etcd_client *client, const char *key, size_t len, long long ttl)
{
    int n;
    etcd_request *req;
    char url[HIETCD_URL_BUFSIZE] = {0}; 
    char post[9] = "dir=true";

    n = etcd_gen_url(client, key, url);
    if (ttl > 0) 
        n += snprintf(url+n, HIETCD_URL_BUFSIZE-n, "?ttl=%llu", ttl);

    req = etcd_request_create(url, n, HIETCD_REQUEST_PUT);
    if (req == NULL) return HIETCD_ERR;
    etcd_request_set_data(req, post, sizeof(post));

    return etcd_async_send_request(client, req);
}

/*
int etcd_add_server(etcd_client *client, const char *server, size_t len)
{
    if (client->snum >= HIETCD_MAX_NODE_NUM)
        return HIETCD_ERR;

    client->servers[client->snum++] = strndup(server, len); 
    return HIETCD_OK;
}

int etcd_mkdir(etcd_client *client, const char *key, size_t len, int ttl, 
    etcd_response *resp)
{
    
}

int etcd_send_request(etcd_client *client, const char *method, const char *key,
    const char *query, const char *post, etcd_response *resp)
{
    CURL *ch;

    ch = curl_easy_init();
}
*/
