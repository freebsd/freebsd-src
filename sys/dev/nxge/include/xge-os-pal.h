/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/nxge/include/xge-os-pal.h,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef XGE_OS_PAL_H
#define XGE_OS_PAL_H

#include <dev/nxge/include/xge-defs.h>

__EXTERN_BEGIN_DECLS

/*--------------------------- platform switch ------------------------------*/

/* platform specific header */
#include <dev/nxge/xge-osdep.h>

#if !defined(XGE_OS_PLATFORM_64BIT) && !defined(XGE_OS_PLATFORM_32BIT)
#error "either 32bit or 64bit switch must be defined!"
#endif

#if !defined(XGE_OS_HOST_BIG_ENDIAN) && !defined(XGE_OS_HOST_LITTLE_ENDIAN)
#error "either little endian or big endian switch must be defined!"
#endif

#if defined(XGE_OS_PLATFORM_64BIT)
#define XGE_OS_MEMORY_DEADCODE_PAT  0x5a5a5a5a5a5a5a5a
#else
#define XGE_OS_MEMORY_DEADCODE_PAT  0x5a5a5a5a
#endif

#define XGE_OS_TRACE_MSGBUF_MAX     512
typedef struct xge_os_tracebuf_t {
	int     wrapped_once;     /* circular buffer been wrapped */
	int     timestamp;        /* whether timestamps are enabled */
	volatile int    offset;           /* offset within the tracebuf */
	int     size;             /* total size of trace buffer */
	char        msg[XGE_OS_TRACE_MSGBUF_MAX]; /* each individual buffer */
	int     msgbuf_max;   /* actual size of msg buffer */
	char        *data;            /* pointer to data buffer */
} xge_os_tracebuf_t;
extern xge_os_tracebuf_t *g_xge_os_tracebuf;

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
extern xge_os_tracebuf_t *g_xge_os_tracebuf;
extern char *dmesg_start;

/* Calculate the size of the msg and copy it into the global buffer */  
#define __xge_trace(tb) { \
	int msgsize = xge_os_strlen(tb->msg) + 2; \
	int offset = tb->offset; \
	if (msgsize != 2 && msgsize < tb->msgbuf_max) { \
	    int leftsize =  tb->size - offset; \
	    if ((msgsize + tb->msgbuf_max) > leftsize) { \
	        xge_os_memzero(tb->data + offset, leftsize); \
	        offset = 0; \
	        tb->wrapped_once = 1; \
	    } \
	    xge_os_memcpy(tb->data + offset, tb->msg, msgsize-1); \
	    *(tb->data + offset + msgsize-1) = '\n'; \
	    *(tb->data + offset + msgsize) = 0; \
	    offset += msgsize; \
	    tb->offset = offset; \
	    dmesg_start = tb->data + offset; \
	    *tb->msg = 0; \
	} \
}

#define xge_os_vatrace(tb, fmt) { \
	if (tb != NULL) { \
	    char *_p = tb->msg; \
	    if (tb->timestamp) { \
	        xge_os_timestamp(tb->msg); \
	        _p = tb->msg + xge_os_strlen(tb->msg); \
	    } \
	    xge_os_vasprintf(_p, fmt); \
	    __xge_trace(tb); \
	} \
}

#ifdef __GNUC__
#define xge_os_trace(tb, fmt...) { \
	if (tb != NULL) { \
	    if (tb->timestamp) { \
	        xge_os_timestamp(tb->msg); \
	    } \
	    xge_os_sprintf(tb->msg + xge_os_strlen(tb->msg), fmt); \
	    __xge_trace(tb); \
	} \
}
#endif /* __GNUC__ */

#else
#define xge_os_vatrace(tb, fmt)
#ifdef __GNUC__
#define xge_os_trace(tb, fmt...)
#endif /* __GNUC__ */
#endif

__EXTERN_END_DECLS

#endif /* XGE_OS_PAL_H */
