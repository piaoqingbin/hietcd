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

#include "hietcd.h"

static void inline etcd_response_init(etcd_response *resp);

etcd_node *etcd_node_create(void)
{
    etcd_node *node = malloc(sizeof(etcd_node));

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

static void inline etcd_response_init(etcd_response *resp)
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
