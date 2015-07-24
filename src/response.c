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

#include <yajl/yajl_tree.h>

#include "hietcd.h"
#include "response.h"

/* Parse flags */
#define PF_NULL     0
#define PF_LONG     1
#define PF_STRCPY   2

typedef enum {
    ETCD_RESP_KEY_ERRCODE = 0,
    ETCD_RESP_KEY_MESSAGE,
    ETCD_RESP_KEY_ACTION,
    ETCD_RESP_KEY_NODE,
    ETCD_RESP_KEY_PNODE,
    ETCD_RESP_KEY_KEY,
    ETCD_RESP_KEY_DIR,
    ETCD_RESP_KEY_VALUE,
    ETCD_RESP_KEY_CIDX,
    ETCD_RESP_KEY_MIDX,
    ETCD_RESP_KEY_TTL,
    ETCD_RESP_KEY_EXPR,
    ETCD_RESP_KEY_NODES
} etcd_resp_key;

static const char *etcd_resp_key_path[13][2] = {
    {"errorCode", NULL},
    {"message", NULL},
    {"action", NULL},
    {"node", NULL},
    {"prevNode", NULL},
    {"key", NULL},
    {"dir", NULL},
    {"value", NULL},
    {"createdIndex", NULL},
    {"modifiedIndex", NULL},
    {"ttl", NULL},
    {"expiration", NULL},
    {"nodes", NULL}
};

static yajl_type etcd_resp_key_type[] = {
    yajl_t_number,
    yajl_t_string,
    yajl_t_string,
    yajl_t_object,
    yajl_t_object,
    yajl_t_string,
    yajl_t_true,
    yajl_t_string,
    yajl_t_number,
    yajl_t_number,
    yajl_t_number,
    yajl_t_string,
    yajl_t_array
};

static inline void etcd_response_init(etcd_response *resp);
static int etcd_response_parse_err(etcd_response *resp, yajl_val obj);
static etcd_node *etcd_response_parse_node(yajl_val obj, int parse_child);
static int etcd_response_parse_key(yajl_val obj, etcd_resp_key, void *data, int flgs);

etcd_node *etcd_node_create(void)
{
    etcd_node *node;
    
    if (!(node = malloc(sizeof(etcd_node))))
        return NULL;

    node->isdir = 0;
    node->ttl = -1;
    node->cidx = -1;
    node->midx = -1;
    node->expr[0] = '\0';
    node->key = NULL;
    node->value = NULL;
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
    etcd_response *resp;
    
    if ((resp = malloc(sizeof(etcd_response))) == NULL)
        return NULL;

    etcd_response_init(resp);
    return resp;
}

static inline void etcd_response_init(etcd_response *resp)
{
    resp->ccode = CURLE_OK;
    resp->hcode = -1;
    resp->errcode = ETCD_OK;
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

size_t etcd_response_header_cb(char *buffer, size_t size, size_t nitems, 
    void *userdata)
{
    size_t n = 0;
    char *p = buffer;
    etcd_response *resp = userdata;

    if (!resp) goto response_header_cb_done;

    if (strstr(p, ETCD_HEADER_ECID) == p) {
        n = sizeof(ETCD_HEADER_ECID);
        strncpy(resp->cluster, p + n + 1, nitems - n - 2);
    } else if (strstr(p, ETCD_HEADER_EIDX) == p) {
        n = sizeof(ETCD_HEADER_EIDX);
        resp->idx = atoll(p + n + 1);
    } else if (strstr(p, ETCD_HEADER_RIDX) == p) {
        n = sizeof(ETCD_HEADER_RIDX);
        resp->ridx = atoll(p + n + 1);
    } else if (strstr(p, ETCD_HEADER_RTERM) == p) {
        n = sizeof(ETCD_HEADER_RTERM);
        resp->rterm = atoll(p + n + 1);
    }

response_header_cb_done:
    return nitems * size;
}

size_t etcd_response_write_cb(char *ptr, size_t size, size_t nmemb, 
    void *userdata)
{
    size_t ret_size = size * nmemb;
    strncat(userdata, ptr, ret_size);
    return ret_size;
}

int etcd_response_parse(etcd_response *resp)
{
    yajl_val obj, val;
    etcd_node *node;

    obj = yajl_tree_parse(resp->data, resp->errmsg, sizeof(resp->errmsg));
    if (!obj || !YAJL_IS_OBJECT(obj)) {
        resp->errcode = ETCD_ERR_PROTOCOL;
        goto response_parse_done;
    }

    if (etcd_response_parse_err(resp, obj) == ETCD_OK)
        goto response_parse_done;
    
    etcd_response_parse_key(obj, ETCD_RESP_KEY_ACTION, &resp->action, PF_STRCPY);

    // node
    if (etcd_response_parse_key(obj, ETCD_RESP_KEY_NODE, &val, PF_NULL) == ETCD_OK)
        resp->node = etcd_response_parse_node(val, 1); 
    if (etcd_response_parse_key(obj, ETCD_RESP_KEY_PNODE, &val, PF_NULL) == ETCD_OK)
        resp->pnode = etcd_response_parse_node(val, 1); 

    resp->errcode = ETCD_OK;

response_parse_done:
    yajl_tree_free(obj);
    return resp->errcode;
}

static int etcd_response_parse_err(etcd_response *resp, yajl_val obj)
{
    int ret;
    ret = etcd_response_parse_key(obj, ETCD_RESP_KEY_ERRCODE, &resp->errcode, PF_NULL);
    if (ret != ETCD_OK) return ret;
    ret = etcd_response_parse_key(obj, ETCD_RESP_KEY_MESSAGE, &resp->errmsg, PF_STRCPY);
    return ret;
}

static etcd_node *etcd_response_parse_node(yajl_val obj, int parse_child)
{
    yajl_val val;
    etcd_node *node;

    if ((node = etcd_node_create()) == NULL)
        return NULL;

    etcd_response_parse_key(obj, ETCD_RESP_KEY_KEY, &node->key, PF_NULL);
    etcd_response_parse_key(obj, ETCD_RESP_KEY_VALUE, &node->value, PF_NULL);
    etcd_response_parse_key(obj, ETCD_RESP_KEY_DIR, &node->isdir, PF_NULL);
    etcd_response_parse_key(obj, ETCD_RESP_KEY_CIDX, &node->cidx, PF_LONG);
    etcd_response_parse_key(obj, ETCD_RESP_KEY_MIDX, &node->midx, PF_LONG);
    etcd_response_parse_key(obj, ETCD_RESP_KEY_TTL, &node->ttl, PF_NULL);
    etcd_response_parse_key(obj, ETCD_RESP_KEY_EXPR, &node->expr, PF_STRCPY);

    if (etcd_response_parse_key(obj, ETCD_RESP_KEY_NODES, &val, PF_NULL) == ETCD_OK) {

        node->ccount = val->u.array.len;
        if (parse_child) {
            int i;
            etcd_node *cnode, *pcnode;

            cnode = pcnode = NULL;
            for (i = 0; i < node->ccount; i++) {
                cnode = etcd_response_parse_node(val->u.array.values[i], 0);
                if (!cnode) continue;
                if (i == 0) node->cnode = cnode;
                if (pcnode) pcnode->snode = cnode;
                pcnode = cnode; 
            }
        }
    }
    return node;
}

static int etcd_response_parse_key(yajl_val obj, etcd_resp_key k, void *data, int flgs)
{
    yajl_val val;
    yajl_type t = etcd_resp_key_type[k];

    val = yajl_tree_get(obj, etcd_resp_key_path[k], t);
    if (!val) return ETCD_ERR_PROTOCOL;

    switch (t) {

    case yajl_t_number:  
        if (YAJL_IS_NUMBER(val)) {
            if (flgs & PF_LONG) {
                *((long long *)data) = YAJL_GET_INTEGER(val);
            } else { 
                *((int *)data) = (int)YAJL_GET_INTEGER(val);
            }
        }
        break;

    case yajl_t_string:
        if (YAJL_IS_STRING(val)) {
            if (flgs & PF_STRCPY) {
                strcpy((char *)data, YAJL_GET_STRING(val));
            } else {
                *((char **)data) = strdup(YAJL_GET_STRING(val));
            }
        }
        break;

    case yajl_t_object:
        if (YAJL_IS_OBJECT(val)) *((yajl_val *)data) = val; 
        break;

    case yajl_t_true:
        *((int *)data) = YAJL_IS_TRUE(val) ? 1 : 0;
        break;

    case yajl_t_array:
        if (YAJL_IS_ARRAY(val)) *((yajl_val *)data) = val;
        break;

    default:
        return ETCD_ERR_PROTOCOL;
    } 

    return ETCD_OK;
}
