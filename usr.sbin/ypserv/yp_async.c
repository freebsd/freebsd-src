/*
 * Copyright (c) 1995, 1996
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yp_async.c,v 1.5 1996/11/30 21:22:48 wpaul Exp $
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>
#include <db.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/queue.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include "yp_extern.h"

#ifndef lint
static const char rcsid[] = "$Id: yp_async.c,v 1.5 1996/11/30 21:22:48 wpaul Exp $";
#endif

/*
 * This is the code that lets us handle yp_all() requests without
 * using fork(). The idea is that we steal away the transport and
 * XDR handles from the RPC library and handle the transactions
 * ourselves. Normally, for a TCP request, the RPC library will
 * send all the entire response at once, which means that if the
 * response is large (like, say, a huge password map), then we could
 * block for a long time inside the XDR filter. This would cause other
 * clients to block while waiting for us to finish, which is bad.
 *
 * Previously, we handled this by fork()ing off the request into a
 * child process, thereby allowing the parent (us) to service other
 * requests. But this can incurr a lot of overhead and we have to
 * limit the number of simultaneous children to avoid swapming the
 * system.
 *
 * What we do now is handle the request one record at a time. We send
 * the first record, then head back to the svc_run() loop to handle
 * more requests. If select() says we can still write data to the
 * socket, we send the next record. When we reach the end of the
 * map or the client stops receiving, we dequeue the request and
 * destroy the handle.
 *
 * The mechanism we use to steal the transport and XDR handles away
 * from the RPC library is quite evil. Basically, we call svc_sendreply()
 * and hand it a custom XDR filter that calls yp_add_async() to register
 * the request, then bails out via longjmp() back to the ypproc_all_2_svc()
 * routine before the library can shut down the pipe on us. We then
 * unregister the transport from the library (so that svc_getreqset()
 * will no longer talk to it) and handle it ourselves.
 */

extern int _rpc_dtablesize __P(( void ));

#ifndef QUEUE_TTL
#define QUEUE_TTL 255
#endif

static fd_set writefds;
static int fd_setsize;

static CIRCLEQ_HEAD(asynchead, asyncq_entry) ahead;

struct asyncq_entry {
	DB *dbp;	/* database handle */
	XDR *xdrs;	/* XDR handle */
	SVCXPRT *xprt;	/* transport handle */
	DBT key;
	int ttl;
	CIRCLEQ_ENTRY(asyncq_entry) links;
};

void yp_init_async()
{
	struct rlimit rlim;

	CIRCLEQ_INIT(&ahead);

	/*
	 * Jack up the number of file descriptors.
	 * We may need them if we end up with a lot
	 * of requests queued.
	 */

	if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
		yp_error("couldn't get filedesc limit: %s", strerror(errno));
		return;
	}
	rlim.rlim_cur = rlim.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rlim) == -1) {
		yp_error("couldn't set filedesc limit: %s", strerror(errno));
		return;
	}

	return;
}

static bool_t yp_xmit_ypresp_all(q)
	struct asyncq_entry *q;
{
	DBT data = { NULL, 0 };
	ypresp_all obj;

	/* Get a record */
	if ((obj.ypresp_all_u.val.stat =
		yp_next_record(q->dbp, &q->key, &data, 1, 0)) == YP_TRUE) {
		obj.ypresp_all_u.val.val.valdat_len = data.size;
		obj.ypresp_all_u.val.val.valdat_val = data.data;
		obj.ypresp_all_u.val.key.keydat_len = q->key.size;
		obj.ypresp_all_u.val.key.keydat_val = q->key.data;
		obj.more = TRUE;
	} else {
		obj.more = FALSE;
	}

	/* Serialize */
	q->xdrs->x_op = XDR_ENCODE;
	if (xdr_ypresp_all(q->xdrs, &obj) == FALSE)
		return(FALSE);

	return(obj.more);
}

static void yp_remove_async(q)
	struct asyncq_entry *q;
{
	xdrrec_endofrecord(q->xdrs, TRUE);
	svc_destroy(q->xprt);
	(void)(q->dbp->close)(q->dbp);
	CIRCLEQ_REMOVE(&ahead, q, links);
	free(q);

	return;
}

bool_t yp_add_async(xdrs, xprt, dbp)
	XDR *xdrs;
	SVCXPRT *xprt;
	DB *dbp;
{
	register struct asyncq_entry *q;

	q = (struct asyncq_entry *)calloc(1, sizeof(struct asyncq_entry));
	if (q == NULL) {
		yp_error("failed to malloc() asyncq entry: %s",
							strerror(errno));
		return(FALSE);
	}

	xprt_unregister(xprt);
	q->xdrs = xdrs;
	q->xdrs->x_op = XDR_ENCODE;
	q->dbp = dbp;
	q->xprt = xprt;
	q->key.size = 0;
	q->key.data = NULL;
	q->ttl = QUEUE_TTL;
	CIRCLEQ_INSERT_HEAD(&ahead, q, links);

	return(TRUE);
}

static void yp_handle_async()
{
	register struct asyncq_entry *q;

restart:

	for (q = ahead.cqh_first; q != (void *)&ahead; q = q->links.cqe_next) {
		if (FD_ISSET(q->xprt->xp_sock, &writefds)) {
			q->ttl = QUEUE_TTL;
			if (yp_xmit_ypresp_all(q) == FALSE) {
				yp_remove_async(q);
				goto restart;
			}
		}
	}
}

static int yp_set_async_fds()
{
	register struct asyncq_entry *q;
	int havefds;

restart:
	havefds = 0;
	FD_ZERO(&writefds);

	for (q = ahead.cqh_first; q != (void *)&ahead; q = q->links.cqe_next) {
		q->ttl--;
		if (q->ttl <= 0) {
			yp_remove_async(q);
			goto restart;
		} else {
			FD_SET(q->xprt->xp_sock, &writefds);
			havefds++;
		}
	}

	return(havefds);
}

void
yp_svc_run()
{
#ifdef FD_SETSIZE
	fd_set readfds;
#else
	int readfds;
#endif /* def FD_SETSIZE */
	extern int forked;
	int pid;
	int w;

	fd_setsize = _rpc_dtablesize();

	/* Establish the identity of the parent ypserv process. */
	pid = getpid();

	for (;;) {
#ifdef FD_SETSIZE
		readfds = svc_fdset;
#else
		readfds = svc_fds;
#endif /* def FD_SETSIZE */
		w = yp_set_async_fds();

		switch (select(fd_setsize, &readfds, w ? &writefds : NULL,
			       NULL, (struct timeval *)0)) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			perror("svc_run: - select failed");
			return;
		case 0:
			continue;
		default:
			yp_handle_async();
			svc_getreqset(&readfds);
			if (forked && pid != getpid())
				exit(0);
		}
	}
}
