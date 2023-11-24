/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef	LIB9P_THREADPOOL_H
#define	LIB9P_THREADPOOL_H

#include <stdbool.h>
#include <pthread.h>
#include <sys/queue.h>
#include "lib9p.h"

STAILQ_HEAD(l9p_request_queue, l9p_request);

/*
 * Most of the workers in the threadpool run requests.
 *
 * One distinguished worker delivers responses from the
 * response queue.  The reason this worker exists is to
 * guarantee response order, so that flush responses go
 * after their flushed requests.
 */
struct l9p_threadpool {
    struct l9p_connection *	ltp_conn;	/* the connection */
    struct l9p_request_queue	ltp_workq;	/* requests awaiting a worker */
    struct l9p_request_queue	ltp_replyq;	/* requests that are done */
    pthread_mutex_t		ltp_mtx;	/* locks queues and cond vars */
    pthread_cond_t		ltp_work_cv;	/* to signal regular workers */
    pthread_cond_t		ltp_reply_cv;	/* to signal reply-worker */
    LIST_HEAD(, l9p_worker)	ltp_workers;	/* list of all workers */
};

/*
 * All workers, including the responder, use this as their
 * control structure.  (The only thing that distinguishes the
 * responder is that it runs different code and waits on the
 * reply_cv.)
 */
struct l9p_worker {
    struct l9p_threadpool *	ltw_tp;
    pthread_t			ltw_thread;
    bool			ltw_exiting;
    bool			ltw_responder;
    LIST_ENTRY(l9p_worker)	ltw_link;
};

/*
 * Each request has a "work state" telling where the request is,
 * in terms of workers working on it.  That is, this tells us
 * which threadpool queue, if any, the request is in now or would
 * go in, or what's happening with it.
 */
enum l9p_workstate {
	L9P_WS_NOTSTARTED,		/* not yet started */
	L9P_WS_IMMEDIATE,		/* Tflush being done sans worker */
	L9P_WS_INPROGRESS,		/* worker is working on it */
	L9P_WS_RESPQUEUED,		/* worker is done, response queued */
	L9P_WS_REPLYING,		/* responder is in final reply path */
};

/*
 * Each request has a "flush state", initally NONE meaning no
 * Tflush affected the request.
 *
 * If a Tflush comes in before we ever assign a work thread,
 * the flush state goes to FLUSH_REQUESTED_PRE_START.
 *
 * If a Tflush comes in after we assign a work thread, the
 * flush state goes to FLUSH_REQUESTED_POST_START.  The flush
 * request may be too late: the request might finish anyway.
 * Or it might be soon enough to abort.  In all cases, though, the
 * operation requesting the flush (the "flusher") must wait for
 * the other request (the "flushee") to go through the respond
 * path.  The respond routine gets to decide whether to send a
 * normal response, send an error, or drop the request
 * entirely.
 *
 * There's one especially annoying case: what if a Tflush comes in
 * *while* we're sending a response?  In this case it's too late:
 * the flush just waits for the fully-composed response.
 */
enum l9p_flushstate {
	L9P_FLUSH_NONE = 0,		/* must be zero */
	L9P_FLUSH_REQUESTED_PRE_START,	/* not even started before flush */
	L9P_FLUSH_REQUESTED_POST_START,	/* started, then someone said flush */
	L9P_FLUSH_TOOLATE		/* too late, already responding */
};

void	l9p_threadpool_flushee_done(struct l9p_request *);
int	l9p_threadpool_init(struct l9p_threadpool *, int);
void	l9p_threadpool_run(struct l9p_threadpool *, struct l9p_request *);
int	l9p_threadpool_shutdown(struct l9p_threadpool *);
int	l9p_threadpool_tflush(struct l9p_request *);

#endif	/* LIB9P_THREADPOOL_H  */
