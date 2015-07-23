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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <curl/curl.h>

#include "hietcd.h"
#include "log.h"
#include "io.h"
#include "request.h"

static int etcd_set_nonblock(int fd);
static inline int etcd_fmt_url(etcd_client *client, const char *key, char *url);
static inline int etcd_notify_io_thread(etcd_client *client);
static inline int etcd_send_queue(etcd_client *client, etcd_request *req);

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
    client->proc = NULL;
    client->userdata = NULL;

    return client;
}

void etcd_client_destroy(etcd_client *client)
{
    etcd_stop_io_thread(client);
    while (--client->snum >= 0)
        free(client->servers[client->snum]);
    close(client->wfd);
    free(client); 
}

void etcd_set_response_proc(etcd_client *client, etcd_response_proc *proc, void *userdata)
{
    client->proc = proc;
    client->userdata = userdata;
}

int etcd_start_io_thread(etcd_client *client)
{
    etcd_io *io;

    if (client->io != NULL) return HIETCD_OK;

    if ((io = etcd_io_create()) == NULL) {
        ETCD_LOG_ERROR("Out of memory");
        return HIETCD_ERR;
    }
    io->client = client;

    int fds[2] = {0};

    if (pipe(fds) == -1) {
        free(io); 
        ETCD_LOG_ERROR("Can't make a pipe %d", errno);
        return HIETCD_ERR;
    }
    etcd_set_nonblock(fds[0]);
    etcd_set_nonblock(fds[1]);

    io->rfd = fds[0];
    io->size = 10240;
    io->elt.tv_sec = 0;
    io->elt.tv_usec = 1000;

    ETCD_LOG_DEBUG("Starting IO thread...");
    pthread_create(&client->tid, 0, etcd_io_start, (void *)io);

    pthread_mutex_lock(&io->lock);
    while (io->ready != 1) 
        pthread_cond_wait(&io->cond, &io->lock);
    pthread_mutex_unlock(&io->lock); 

    client->wfd = fds[1];
    client->io = io;
    return HIETCD_OK;
}

void etcd_stop_io_thread(etcd_client *client)
{
    if (client->io != NULL) {
        etcd_io_stop(client->io);
        etcd_notify_io_thread(client); 
        pthread_join(client->tid, 0);
        etcd_io_destroy(client->io);
        client->io = NULL;
    }
}

static inline int etcd_notify_io_thread(etcd_client *client)
{
    char c = 0;
    return write(client->wfd, &c, 1) == 1 ? HIETCD_OK : HIETCD_ERR;
}

static int etcd_set_nonblock(int fd)
{
    long l = fcntl(fd, F_GETFL);
    if(l & O_NONBLOCK) return 0;
    return fcntl(fd, F_SETFL, l | O_NONBLOCK);
}

static inline int etcd_fmt_url(etcd_client *client, const char *key, char *url) 
{
    return snprintf(url, HIETCD_URL_BUFSIZE, "%s/%s/keys%s", 
            client->servers[0], HIETCD_SERVER_VERSION, key);
}


static inline int etcd_send_queue(etcd_client *client, etcd_request *req)
{
    etcd_io_push_request(client->io, req);
    return etcd_notify_io_thread(client);
}

int etcd_amkdir(etcd_client *client, const char *key, int ttl)
{
    etcd_request *req;
    char url[HIETCD_URL_BUFSIZE] = {0}; 
    const char data[] = "dir=true";
    int n = 0;
    
    n = etcd_fmt_url(client, key, url);
    if (ttl > 0) 
        n += snprintf(url + n, HIETCD_URL_BUFSIZE - n, "?ttl=%d", ttl);

    if ((req = etcd_request_create(url, n, ETCD_REQUEST_PUT)) == NULL)
        return HIETCD_ERR;

    etcd_request_dup_data(req, data, sizeof(data));
    return etcd_send_queue(client, req);
}

int etcd_aset(etcd_client *client, const char *key, const char *value, 
    size_t len, int ttl)
{
    etcd_request *req;
    char url[HIETCD_URL_BUFSIZE] = {0}, *data = NULL; 
    size_t datasize = len + 22; /* value=%s;ttl=%d */
    int n = 0;
    
    n = etcd_fmt_url(client, key, url);
    if ((req = etcd_request_create(url, n, ETCD_REQUEST_PUT)) == NULL)
        return HIETCD_ERR;

    if ((data = malloc(datasize)) == NULL) return HIETCD_ERR;
    n = snprintf(data, datasize, "value=%s", value);
    if (ttl > 0) snprintf(data + n, datasize - n, ";ttl=%d", ttl);
    etcd_request_set_data(req, data);

    return etcd_send_queue(client, req);

}

int etcd_aget(etcd_client *client, const char *key)
{
    etcd_request *req;
    char url[HIETCD_URL_BUFSIZE] = {0}; 
    int n = 0;
    
    n = etcd_fmt_url(client, key, url);
    n += snprintf(url + n, HIETCD_URL_BUFSIZE - n, "?recursive=true");

    if ((req = etcd_request_create(url, n, ETCD_REQUEST_GET)) == NULL)
        return HIETCD_ERR;
    return etcd_send_queue(client, req);
}

int etcd_adelete(etcd_client *client, const char *key)
{
    etcd_request *req;
    char url[HIETCD_URL_BUFSIZE] = {0}; 
    int n = 0;

    n = etcd_fmt_url(client, key, url);
    n += snprintf(url + n, HIETCD_URL_BUFSIZE - n, "?recursive=true");

    if ((req = etcd_request_create(url, n, ETCD_REQUEST_DELETE)) == NULL)
        return HIETCD_ERR;
    return etcd_send_queue(client, req);
}

int etcd_awatch(etcd_client *client, const char *key)
{
    etcd_request *req;
    char url[HIETCD_URL_BUFSIZE] = {0}; 
    int n = 0;
    
    n = etcd_fmt_url(client, key, url);
    n += snprintf(url + n, HIETCD_URL_BUFSIZE - n, "?wait=true&recursive=true");

    if ((req = etcd_request_create(url, n, ETCD_REQUEST_GET)) == NULL)
        return HIETCD_ERR;
    return etcd_send_queue(client, req);
}
