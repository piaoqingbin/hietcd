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
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

#include "etcd_log.h"

etcd_log_level etcd_ll = ETCD_LOG_LEVEL_INFO;

static FILE *log_handler = NULL;

static const char *log_level_str[] = {
    "UNKNOWN", "ERROR", "WARN", "INFO", "DEBUG"
};

static pthread_key_t time_buf_key;
static pthread_key_t msg_buf_key;

static char *get_thread_buf(pthread_key_t key, unsigned long bufsize);
static void thread_buf_destroy(void *buf);

FILE *get_log_handler()
{
    if (log_handler == NULL)
        log_handler = stderr;
    return log_handler;
}

static char *get_thread_buf(pthread_key_t key, unsigned long bufsize)
{
    char *buf = pthread_getspecific(key);
    if (buf != 0) return buf;
    int ret;
    buf = calloc(1, bufsize);
    if ((ret = pthread_setspecific(key, buf)) != 0)
        fprintf(stderr, "Failed to init thread buffer: %d", ret);
    return buf;
}

static void thread_buf_destroy(void *buf)
{
    if (buf) free(buf);
}

__attribute__((constructor)) void thread_buf_init() {
    pthread_key_create(&time_buf_key, thread_buf_destroy);
    pthread_key_create(&msg_buf_key, thread_buf_destroy);
}

void etcd_set_log_level(etcd_log_level level)
{
    if (level < ETCD_LOG_LEVEL_ERROR) 
        level = ETCD_LOG_LEVEL_ERROR;
    if (level > ETCD_LOG_LEVEL_DEBUG)
        level = ETCD_LOG_LEVEL_DEBUG;
    etcd_ll = level;
}

void etcd_set_log_handler(FILE *handler)
{
    log_handler = handler;
}

void etcd_log(etcd_log_level etcd_ll, int line, const char *func, 
        const char *fmt, ...)
{
    static pid_t pid = 0;
    char *time_buf, *msg_buf;
    struct timeval tv;
    struct tm lt;
    va_list va;
    time_t now = 0;
    int n = 0;

    if (pid == 0) pid = getpid();

    time_buf = get_thread_buf(time_buf_key, ETCD_LOG_TIME_BUFSIZE); 
    gettimeofday(&tv,0);
    now = tv.tv_sec;
    localtime_r(&now, &lt);
    n = strftime(time_buf, ETCD_LOG_TIME_BUFSIZE, "%Y-%m-%d %H:%M:%S", &lt);
    n += snprintf(time_buf + n, ETCD_LOG_TIME_BUFSIZE - n, ".%03d",
        (int)(tv.tv_usec/1000));

    *msg_buf = get_thread_buf(msg_buf_key, ETCD_LOG_MSG_BUFSIZE);
    n = snprintf(msg_buf, ETCD_LOG_MSG_BUFSIZE, "%s (%ld-0x%lx,%s@%d) [%s]: ", 
        time_buf, (long) pid, (unsigned long int)(pthread_self()), 
        func, line, log_level_str[etcd_ll]);

    va_start(va, fmt);
    vsnprintf(msg_buf + n, ETCD_LOG_MSG_BUFSIZE - 1 - n, fmt, va);
    va_end(va);

    fprintf(get_log_handler(), "%s\n", msg_buf);
    fflush(get_log_handler());
}

