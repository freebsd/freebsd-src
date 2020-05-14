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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/queue.h>
#include "lib9p.h"
#include "lib9p_impl.h"
#include "fid.h"
#include "hashtable.h"
#include "log.h"
#include "threadpool.h"
#include "backend/backend.h"

int
l9p_server_init(struct l9p_server **serverp, struct l9p_backend *backend)
{
	struct l9p_server *server;

	server = l9p_calloc(1, sizeof (*server));
	server->ls_max_version = L9P_2000L;
	server->ls_backend = backend;
	LIST_INIT(&server->ls_conns);

	*serverp = server;
	return (0);
}

int
l9p_connection_init(struct l9p_server *server, struct l9p_connection **conn)
{
	struct l9p_connection *newconn;

	assert(server != NULL);
	assert(conn != NULL);

	newconn = calloc(1, sizeof (*newconn));
	if (newconn == NULL)
		return (-1);
	newconn->lc_server = server;
	newconn->lc_msize = L9P_DEFAULT_MSIZE;
	if (l9p_threadpool_init(&newconn->lc_tp, L9P_NUMTHREADS)) {
		free(newconn);
		return (-1);
	}
	ht_init(&newconn->lc_files, 100);
	ht_init(&newconn->lc_requests, 100);
	LIST_INSERT_HEAD(&server->ls_conns, newconn, lc_link);
	*conn = newconn;

	return (0);
}

void
l9p_connection_free(struct l9p_connection *conn)
{

	LIST_REMOVE(conn, lc_link);
	free(conn);
}

void
l9p_connection_recv(struct l9p_connection *conn, const struct iovec *iov,
    const size_t niov, void *aux)
{
	struct l9p_request *req;
	int error;

	req = l9p_calloc(1, sizeof (struct l9p_request));
	req->lr_aux = aux;
	req->lr_conn = conn;

	req->lr_req_msg.lm_mode = L9P_UNPACK;
	req->lr_req_msg.lm_niov = niov;
	memcpy(req->lr_req_msg.lm_iov, iov, sizeof (struct iovec) * niov);

	req->lr_resp_msg.lm_mode = L9P_PACK;

	if (l9p_pufcall(&req->lr_req_msg, &req->lr_req, conn->lc_version) != 0) {
		L9P_LOG(L9P_WARNING, "cannot unpack received message");
		l9p_freefcall(&req->lr_req);
		free(req);
		return;
	}

	if (ht_add(&conn->lc_requests, req->lr_req.hdr.tag, req)) {
		L9P_LOG(L9P_WARNING, "client reusing outstanding tag %d",
		    req->lr_req.hdr.tag);
		l9p_freefcall(&req->lr_req);
		free(req);
		return;
	}

	error = conn->lc_lt.lt_get_response_buffer(req,
	    req->lr_resp_msg.lm_iov,
	    &req->lr_resp_msg.lm_niov,
	    conn->lc_lt.lt_aux);
	if (error) {
		L9P_LOG(L9P_WARNING, "cannot obtain buffers for response");
		ht_remove(&conn->lc_requests, req->lr_req.hdr.tag);
		l9p_freefcall(&req->lr_req);
		free(req);
		return;
	}

	/*
	 * NB: it's up to l9p_threadpool_run to decide whether
	 * to queue the work or to run it immediately and wait
	 * (it must do the latter for Tflush requests).
	 */
	l9p_threadpool_run(&conn->lc_tp, req);
}

void
l9p_connection_close(struct l9p_connection *conn)
{
	struct ht_iter iter;
	struct l9p_fid *fid;
	struct l9p_request *req;

	L9P_LOG(L9P_DEBUG, "waiting for thread pool to shut down");
	l9p_threadpool_shutdown(&conn->lc_tp);

	/* Drain pending requests (if any) */
	L9P_LOG(L9P_DEBUG, "draining pending requests");
	ht_iter(&conn->lc_requests, &iter);
	while ((req = ht_next(&iter)) != NULL) {
#ifdef notyet
		/* XXX would be good to know if there is anyone listening */
		if (anyone listening) {
			/* XXX crude - ops like Tclunk should succeed */
			req->lr_error = EINTR;
			l9p_respond(req, false, false);
		} else
#endif
		l9p_respond(req, true, false);	/* use no-answer path */
		ht_remove_at_iter(&iter);
	}

	/* Close opened files (if any) */
	L9P_LOG(L9P_DEBUG, "closing opened files");
	ht_iter(&conn->lc_files, &iter);
	while ((fid = ht_next(&iter)) != NULL) {
		conn->lc_server->ls_backend->freefid(
		    conn->lc_server->ls_backend->softc, fid);
		free(fid);
		ht_remove_at_iter(&iter);
	}

	ht_destroy(&conn->lc_requests);
	ht_destroy(&conn->lc_files);
}

struct l9p_fid *
l9p_connection_alloc_fid(struct l9p_connection *conn, uint32_t fid)
{
	struct l9p_fid *file;

	file = l9p_calloc(1, sizeof (struct l9p_fid));
	file->lo_fid = fid;
	/*
	 * Note that the new fid is not marked valid yet.
	 * The insert here will fail if the fid number is
	 * in use, otherwise we have an invalid fid in the
	 * table (as desired).
	 */

	if (ht_add(&conn->lc_files, fid, file) != 0) {
		free(file);
		return (NULL);
	}

	return (file);
}

void
l9p_connection_remove_fid(struct l9p_connection *conn, struct l9p_fid *fid)
{
	struct l9p_backend *be;

	/* fid should be marked invalid by this point */
	assert(!l9p_fid_isvalid(fid));

	be = conn->lc_server->ls_backend;
	be->freefid(be->softc, fid);

	ht_remove(&conn->lc_files, fid->lo_fid);
	free(fid);
}
