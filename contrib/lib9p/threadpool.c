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

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#if defined(__FreeBSD__)
#include <pthread_np.h>
#endif
#include <sys/queue.h>
#include "lib9p.h"
#include "threadpool.h"

static void l9p_threadpool_rflush(struct l9p_threadpool *tp,
    struct l9p_request *req);

static void *
l9p_responder(void *arg)
{
	struct l9p_threadpool *tp;
	struct l9p_worker *worker = arg;
	struct l9p_request *req;

	tp = worker->ltw_tp;
	for (;;) {
		/* get next reply to send */
		pthread_mutex_lock(&tp->ltp_mtx);
		while (STAILQ_EMPTY(&tp->ltp_replyq) && !worker->ltw_exiting)
			pthread_cond_wait(&tp->ltp_reply_cv, &tp->ltp_mtx);
		if (worker->ltw_exiting) {
			pthread_mutex_unlock(&tp->ltp_mtx);
			break;
		}

		/* off reply queue */
		req = STAILQ_FIRST(&tp->ltp_replyq);
		STAILQ_REMOVE_HEAD(&tp->ltp_replyq, lr_worklink);

		/* request is now in final glide path, can't be Tflush-ed */
		req->lr_workstate = L9P_WS_REPLYING;

		/* any flushers waiting for this request can go now */
		if (req->lr_flushstate != L9P_FLUSH_NONE)
			l9p_threadpool_rflush(tp, req);

		pthread_mutex_unlock(&tp->ltp_mtx);

		/* send response */
		l9p_respond(req, false, true);
	}
	return (NULL);
}

static void *
l9p_worker(void *arg)
{
	struct l9p_threadpool *tp;
	struct l9p_worker *worker = arg;
	struct l9p_request *req;

	tp = worker->ltw_tp;
	pthread_mutex_lock(&tp->ltp_mtx);
	for (;;) {
		while (STAILQ_EMPTY(&tp->ltp_workq) && !worker->ltw_exiting)
			pthread_cond_wait(&tp->ltp_work_cv, &tp->ltp_mtx);
		if (worker->ltw_exiting)
			break;

		/* off work queue; now work-in-progress, by us */
		req = STAILQ_FIRST(&tp->ltp_workq);
		STAILQ_REMOVE_HEAD(&tp->ltp_workq, lr_worklink);
		req->lr_workstate = L9P_WS_INPROGRESS;
		req->lr_worker = worker;
		pthread_mutex_unlock(&tp->ltp_mtx);

		/* actually try the request */
		req->lr_error = l9p_dispatch_request(req);

		/* move to responder queue, updating work-state */
		pthread_mutex_lock(&tp->ltp_mtx);
		req->lr_workstate = L9P_WS_RESPQUEUED;
		req->lr_worker = NULL;
		STAILQ_INSERT_TAIL(&tp->ltp_replyq, req, lr_worklink);

		/* signal the responder */
		pthread_cond_signal(&tp->ltp_reply_cv);
	}
	pthread_mutex_unlock(&tp->ltp_mtx);
	return (NULL);
}

/*
 * Just before finally replying to a request that got touched by
 * a Tflush request, we enqueue its flushers (requests of type
 * Tflush, which are now on the flushee's lr_flushq) onto the
 * response queue.
 */
static void
l9p_threadpool_rflush(struct l9p_threadpool *tp, struct l9p_request *req)
{
	struct l9p_request *flusher;

	/*
	 * https://swtch.com/plan9port/man/man9/flush.html says:
	 *
	 * "Should multiple Tflushes be received for a pending
	 * request, they must be answered in order.  A Rflush for
	 * any of the multiple Tflushes implies an answer for all
	 * previous ones.  Therefore, should a server receive a
	 * request and then multiple flushes for that request, it
	 * need respond only to the last flush."  This means
	 * we could march through the queue of flushers here,
	 * marking all but the last one as "to be dropped" rather
	 * than "to be replied-to".
	 *
	 * However, we'll leave that for later, if ever -- it
	 * should be harmless to respond to each, in order.
	 */
	STAILQ_FOREACH(flusher, &req->lr_flushq, lr_flushlink) {
		flusher->lr_workstate = L9P_WS_RESPQUEUED;
#ifdef notdef
		if (not the last) {
			flusher->lr_flushstate = L9P_FLUSH_NOT_RUN;
			/* or, flusher->lr_drop = true ? */
		}
#endif
		STAILQ_INSERT_TAIL(&tp->ltp_replyq, flusher, lr_worklink);
	}
}

int
l9p_threadpool_init(struct l9p_threadpool *tp, int size)
{
	struct l9p_worker *worker;
#if defined(__FreeBSD__)
	char threadname[16];
#endif
	int error;
	int i, nworkers, nresponders;

	if (size <= 0)
		return (EINVAL);
	error = pthread_mutex_init(&tp->ltp_mtx, NULL);
	if (error)
		return (error);
	error = pthread_cond_init(&tp->ltp_work_cv, NULL);
	if (error)
		goto fail_work_cv;
	error = pthread_cond_init(&tp->ltp_reply_cv, NULL);
	if (error)
		goto fail_reply_cv;

	STAILQ_INIT(&tp->ltp_workq);
	STAILQ_INIT(&tp->ltp_replyq);
	LIST_INIT(&tp->ltp_workers);

	nresponders = 0;
	nworkers = 0;
	for (i = 0; i <= size; i++) {
		worker = calloc(1, sizeof(struct l9p_worker));
		worker->ltw_tp = tp;
		worker->ltw_responder = i == 0;
		error = pthread_create(&worker->ltw_thread, NULL,
		    worker->ltw_responder ? l9p_responder : l9p_worker,
		    (void *)worker);
		if (error) {
			free(worker);
			break;
		}
		if (worker->ltw_responder)
			nresponders++;
		else
			nworkers++;

#if defined(__FreeBSD__)
		if (worker->ltw_responder) {
			pthread_set_name_np(worker->ltw_thread, "9p-responder");
		} else {
			sprintf(threadname, "9p-worker:%d", i - 1);
			pthread_set_name_np(worker->ltw_thread, threadname);
		}
#endif

		LIST_INSERT_HEAD(&tp->ltp_workers, worker, ltw_link);
	}
	if (nresponders == 0 || nworkers == 0) {
		/* need the one responder, and at least one worker */
		l9p_threadpool_shutdown(tp);
		return (error);
	}
	return (0);

	/*
	 * We could avoid these labels by having multiple destroy
	 * paths (one for each error case), or by having booleans
	 * for which variables were initialized.  Neither is very
	 * appealing...
	 */
fail_reply_cv:
	pthread_cond_destroy(&tp->ltp_work_cv);
fail_work_cv:
	pthread_mutex_destroy(&tp->ltp_mtx);

	return (error);
}

/*
 * Run a request, usually by queueing it.
 */
void
l9p_threadpool_run(struct l9p_threadpool *tp, struct l9p_request *req)
{

	/*
	 * Flush requests must be handled specially, since they
	 * can cancel / kill off regular requests.  (But we can
	 * run them through the regular dispatch mechanism.)
	 */
	if (req->lr_req.hdr.type == L9P_TFLUSH) {
		/* not on a work queue yet so we can touch state */
		req->lr_workstate = L9P_WS_IMMEDIATE;
		(void) l9p_dispatch_request(req);
	} else {
		pthread_mutex_lock(&tp->ltp_mtx);
		req->lr_workstate = L9P_WS_NOTSTARTED;
		STAILQ_INSERT_TAIL(&tp->ltp_workq, req, lr_worklink);
		pthread_cond_signal(&tp->ltp_work_cv);
		pthread_mutex_unlock(&tp->ltp_mtx);
	}
}

/*
 * Run a Tflush request.  Called via l9p_dispatch_request() since
 * it has some debug code in it, but not called from worker thread.
 */
int
l9p_threadpool_tflush(struct l9p_request *req)
{
	struct l9p_connection *conn;
	struct l9p_threadpool *tp;
	struct l9p_request *flushee;
	uint16_t oldtag;
	enum l9p_flushstate nstate;

	/*
	 * Find what we're supposed to flush (the flushee, as it were).
	 */
	req->lr_error = 0;	/* Tflush always succeeds */
	conn = req->lr_conn;
	tp = &conn->lc_tp;
	oldtag = req->lr_req.tflush.oldtag;
	ht_wrlock(&conn->lc_requests);
	flushee = ht_find_locked(&conn->lc_requests, oldtag);
	if (flushee == NULL) {
		/*
		 * Nothing to flush!  The old request must have
		 * been done and gone already.  Just queue this
		 * Tflush for a success reply.
		 */
		ht_unlock(&conn->lc_requests);
		pthread_mutex_lock(&tp->ltp_mtx);
		goto done;
	}

	/*
	 * Found the original request.  We'll need to inspect its
	 * work-state to figure out what to do.
	 */
	pthread_mutex_lock(&tp->ltp_mtx);
	ht_unlock(&conn->lc_requests);

	switch (flushee->lr_workstate) {

	case L9P_WS_NOTSTARTED:
		/*
		 * Flushee is on work queue, but not yet being
		 * handled by a worker.
		 *
		 * The documentation -- see
		 * http://ericvh.github.io/9p-rfc/rfc9p2000.html
		 * https://swtch.com/plan9port/man/man9/flush.html
		 * -- says that "the server should answer the
		 * flush message immediately".  However, Linux
		 * sends flush requests for operations that
		 * must finish, such as Tclunk, and it's not
		 * possible to *answer* the flush request until
		 * it has been handled (if necessary) or aborted
		 * (if allowed).
		 *
		 * We therefore now just  the original request
		 * and let the request-handler do whatever is
		 * appropriate.  NOTE: we could have a table of
		 * "requests that can be aborted without being
		 * run" vs "requests that must be run to be
		 * aborted", but for now that seems like an
		 * unnecessary complication.
		 */
		nstate = L9P_FLUSH_REQUESTED_PRE_START;
		break;

	case L9P_WS_IMMEDIATE:
		/*
		 * This state only applies to Tflush requests, and
		 * flushing a Tflush is illegal.  But we'll do nothing
		 * special here, which will make us act like a flush
		 * request for the flushee that arrived too late to
		 * do anything about the flushee.
		 */
		nstate = L9P_FLUSH_REQUESTED_POST_START;
		break;

	case L9P_WS_INPROGRESS:
		/*
		 * Worker thread flushee->lr_worker is working on it.
		 * Kick it to get it out of blocking system calls.
		 * (This requires that it carefully set up some
		 * signal handlers, and may be FreeBSD-dependent,
		 * it probably cannot be handled this way on MacOS.)
		 */
#ifdef notyet
		pthread_kill(...);
#endif
		nstate = L9P_FLUSH_REQUESTED_POST_START;
		break;

	case L9P_WS_RESPQUEUED:
		/*
		 * The flushee is already in the response queue.
		 * We'll just mark it as having had some flush
		 * action applied.
		 */
		nstate = L9P_FLUSH_TOOLATE;
		break;

	case L9P_WS_REPLYING:
		/*
		 * Although we found the flushee, it's too late to
		 * make us depend on it: it's already heading out
		 * the door as a reply.
		 *
		 * We don't want to do anything to the flushee.
		 * Instead, we want to work the same way as if
		 * we had never found the tag.
		 */
		goto done;
	}

	/*
	 * Now add us to the list of Tflush-es that are waiting
	 * for the flushee (creating the list if needed, i.e., if
	 * this is the first Tflush for the flushee).  We (req)
	 * will get queued for reply later, when the responder
	 * processes the flushee and calls l9p_threadpool_rflush().
	 */
	if (flushee->lr_flushstate == L9P_FLUSH_NONE)
		STAILQ_INIT(&flushee->lr_flushq);
	flushee->lr_flushstate = nstate;
	STAILQ_INSERT_TAIL(&flushee->lr_flushq, req, lr_flushlink);

	pthread_mutex_unlock(&tp->ltp_mtx);

	return (0);

done:
	/*
	 * This immediate op is ready to be replied-to now, so just
	 * stick it onto the reply queue.
	 */
	req->lr_workstate = L9P_WS_RESPQUEUED;
	STAILQ_INSERT_TAIL(&tp->ltp_replyq, req, lr_worklink);
	pthread_mutex_unlock(&tp->ltp_mtx);
	pthread_cond_signal(&tp->ltp_reply_cv);
	return (0);
}

int
l9p_threadpool_shutdown(struct l9p_threadpool *tp)
{
	struct l9p_worker *worker, *tmp;

	LIST_FOREACH_SAFE(worker, &tp->ltp_workers, ltw_link, tmp) {
		pthread_mutex_lock(&tp->ltp_mtx);
		worker->ltw_exiting = true;
		if (worker->ltw_responder)
			pthread_cond_signal(&tp->ltp_reply_cv);
		else
			pthread_cond_broadcast(&tp->ltp_work_cv);
		pthread_mutex_unlock(&tp->ltp_mtx);
		pthread_join(worker->ltw_thread, NULL);
		LIST_REMOVE(worker, ltw_link);
		free(worker);
	}
	pthread_cond_destroy(&tp->ltp_reply_cv);
	pthread_cond_destroy(&tp->ltp_work_cv);
	pthread_mutex_destroy(&tp->ltp_mtx);

	return (0);
}
