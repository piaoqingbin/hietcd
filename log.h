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

#ifndef _HIETCD_LOG_H_
#define _HIETCD_LOG_H_

#include <stdio.h>

#define ETCD_LOG_TIME_BUFSIZE 128
#define ETCD_LOG_MSG_BUFSIZE 4096

/* Etcd log levels */
typedef enum {
    ETCD_LOG_LEVEL_ERROR = 1,
    ETCD_LOG_LEVEL_WARN = 2,
    ETCD_LOG_LEVEL_INFO = 3,
    ETCD_LOG_LEVEL_DEBUG = 4
} etcd_log_level;

extern etcd_log_level etcd_ll;

#define ETCD_LOG_ERROR(...)   if (etcd_ll >= ETCD_LOG_LEVEL_ERROR) \
    etcd_log(ETCD_LOG_LEVEL_ERROR, __LINE__, __func__, __VA_ARGS__)
#define ETCD_LOG_WARN(...)   if (etcd_ll >= ETCD_LOG_LEVEL_WARN) \
    etcd_log(ETCD_LOG_LEVEL_WARN, __LINE__, __func__, __VA_ARGS__)
#define ETCD_LOG_INFO(...)   if (etcd_ll >= ETCD_LOG_LEVEL_INFO) \
    etcd_log(ETCD_LOG_LEVEL_INFO, __LINE__, __func__, __VA_ARGS__)
#define ETCD_LOG_DEBUG(...)   if (etcd_ll >= ETCD_LOG_LEVEL_DEBUG) \
    etcd_log(ETCD_LOG_LEVEL_DEBUG, __LINE__, __func__, __VA_ARGS__)

FILE *get_log_handler();
void etcd_set_log_level(etcd_log_level level);
void etcd_set_log_handler(FILE *handler);
void etcd_log(etcd_log_level etcd_ll, int line, const char *func, 
        const char *fmt, ...);

#endif
