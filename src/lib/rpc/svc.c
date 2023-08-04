/* @(#)svc.c	2.4 88/08/11 4.0 RPCSRC; from 1.44 88/02/08 SMI */
/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the "Oracle America, Inc." nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)svc.c 1.41 87/10/13 Copyr 1984 Sun Micro";
#endif

/*
 * svc.c, Server-side remote procedure call interface.
 *
 * There are two sets of procedures here.  The xprt routines are
 * for handling transport handles.  The svc routines handle the
 * list of service routines.
 */

#include "autoconf.h"
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <gssrpc/rpc.h>
#include <gssrpc/pmap_clnt.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef FD_SETSIZE
static SVCXPRT **xports;
extern int gssrpc_svc_fdset_init;
#else

#ifdef NBBY
#define NOFILE (sizeof(int) * NBBY)
#else
#define NOFILE (sizeof(int) * 8)
#endif

static SVCXPRT *xports[NOFILE];
#endif /* def FD_SETSIZE */

#define NULL_SVC ((struct svc_callout *)0)
#define	RQCRED_SIZE	1024		/* this size is excessive */

/*
 * The services list
 * Each entry represents a set of procedures (an rpc program).
 * The dispatch routine takes request structs and runs the
 * appropriate procedure.
 */
static struct svc_callout {
	struct svc_callout *sc_next;
	rpcprog_t		    sc_prog;
	rpcprog_t		    sc_vers;
	void		    (*sc_dispatch)();
} *svc_head;

static struct svc_callout *svc_find(rpcprog_t, rpcvers_t,
				    struct svc_callout **);

static void svc_do_xprt(SVCXPRT *xprt);

/* ***************  SVCXPRT related stuff **************** */

/*
 * Activate a transport handle.
 */
void
xprt_register(SVCXPRT *xprt)
{
	int sock = xprt->xp_sock;

#ifdef FD_SETSIZE
	if (gssrpc_svc_fdset_init == 0) {
		FD_ZERO(&svc_fdset);
		gssrpc_svc_fdset_init++;
	}
	if (xports == NULL) {
		xports = (SVCXPRT **)
			mem_alloc(FD_SETSIZE * sizeof(SVCXPRT *));
		memset(xports, 0, FD_SETSIZE * sizeof(SVCXPRT *));
	}
	if (sock < FD_SETSIZE) {
		xports[sock] = xprt;
		FD_SET(sock, &svc_fdset);
		if (sock > svc_maxfd)
			svc_maxfd = sock;
	}
#else
	if (sock < NOFILE) {
		xports[sock] = xprt;
		svc_fds |= (1 << sock);
		if (sock > svc_maxfd)
			svc_maxfd = sock;
	}
#endif /* def FD_SETSIZE */
}

/*
 * De-activate a transport handle.
 */
void
xprt_unregister(SVCXPRT *xprt)
{
	int sock = xprt->xp_sock;

#ifdef FD_SETSIZE
	if ((sock < FD_SETSIZE) && (xports[sock] == xprt)) {
		xports[sock] = (SVCXPRT *)0;
		FD_CLR(sock, &svc_fdset);
	}
#else
	if ((sock < NOFILE) && (xports[sock] == xprt)) {
		xports[sock] = (SVCXPRT *)0;
		svc_fds &= ~(1 << sock);
	}
#endif /* def FD_SETSIZE */
	if (svc_maxfd <= sock) {
		while ((svc_maxfd > 0) && xports[svc_maxfd] == 0)
			svc_maxfd--;
	}
}


/* ********************** CALLOUT list related stuff ************* */

/*
 * Add a service program to the callout list.
 * The dispatch routine will be called when a rpc request for this
 * program number comes in.
 */
bool_t
svc_register(
	SVCXPRT *xprt,
	rpcprog_t prog,
	rpcvers_t vers,
	void (*dispatch)(),
	int protocol)
{
	struct svc_callout *prev;
	struct svc_callout *s;

	if ((s = svc_find(prog, vers, &prev)) != NULL_SVC) {
		if (s->sc_dispatch == dispatch)
			goto pmap_it;  /* he is registering another xptr */
		return (FALSE);
	}
	s = (struct svc_callout *)mem_alloc(sizeof(struct svc_callout));
	if (s == (struct svc_callout *)0) {
		return (FALSE);
	}
	s->sc_prog = prog;
	s->sc_vers = vers;
	s->sc_dispatch = dispatch;
	s->sc_next = svc_head;
	svc_head = s;
pmap_it:
	/* now register the information with the local binder service */
	if (protocol) {
		return (pmap_set(prog, vers, protocol, xprt->xp_port));
	}
	return (TRUE);
}

/*
 * Remove a service program from the callout list.
 */
void
svc_unregister(
	rpcprog_t prog,
	rpcvers_t vers)
{
	struct svc_callout *prev;
	struct svc_callout *s;

	if ((s = svc_find(prog, vers, &prev)) == NULL_SVC)
		return;
	if (prev == NULL_SVC) {
		svc_head = s->sc_next;
	} else {
		prev->sc_next = s->sc_next;
	}
	s->sc_next = NULL_SVC;
	mem_free((char *) s, (u_int) sizeof(struct svc_callout));
	/* now unregister the information with the local binder service */
	(void)pmap_unset(prog, vers);
}

/*
 * Search the callout list for a program number, return the callout
 * struct.
 */
static struct svc_callout *
svc_find(
	rpcprog_t prog,
	rpcvers_t vers,
	struct svc_callout **prev)
{
	struct svc_callout *s, *p;

	p = NULL_SVC;
	for (s = svc_head; s != NULL_SVC; s = s->sc_next) {
		if ((s->sc_prog == prog) && (s->sc_vers == vers))
			goto done;
		p = s;
	}
done:
	*prev = p;
	return (s);
}

/* ******************* REPLY GENERATION ROUTINES  ************ */

/*
 * Send a reply to an rpc request
 */
bool_t
svc_sendreply(
	SVCXPRT *xprt,
	xdrproc_t xdr_results,
	caddr_t xdr_location)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = SUCCESS;
	rply.acpted_rply.ar_results.where = xdr_location;
	rply.acpted_rply.ar_results.proc = xdr_results;
	return (SVC_REPLY(xprt, &rply));
}

/*
 * No procedure error reply
 */
void
svcerr_noproc(SVCXPRT *xprt)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROC_UNAVAIL;
	SVC_REPLY(xprt, &rply);
}

/*
 * Can't decode args error reply
 */
void
svcerr_decode(SVCXPRT *xprt)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = GARBAGE_ARGS;
	SVC_REPLY(xprt, &rply);
}

/*
 * Some system error
 */
void
svcerr_systemerr(SVCXPRT *xprt)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = SYSTEM_ERR;
	SVC_REPLY(xprt, &rply);
}

/*
 * Authentication error reply
 */
void
svcerr_auth(
	SVCXPRT *xprt,
	enum auth_stat why)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_DENIED;
	rply.rjcted_rply.rj_stat = AUTH_ERROR;
	rply.rjcted_rply.rj_why = why;
	SVC_REPLY(xprt, &rply);
}

/*
 * Auth too weak error reply
 */
void
svcerr_weakauth(SVCXPRT *xprt)
{

	svcerr_auth(xprt, AUTH_TOOWEAK);
}

/*
 * Program unavailable error reply
 */
void
svcerr_noprog(SVCXPRT *xprt)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROG_UNAVAIL;
	SVC_REPLY(xprt, &rply);
}

/*
 * Program version mismatch error reply
 */
void
svcerr_progvers(
	SVCXPRT *xprt,
	rpcvers_t low_vers,
	rpcvers_t high_vers)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROG_MISMATCH;
	rply.acpted_rply.ar_vers.low = low_vers;
	rply.acpted_rply.ar_vers.high = high_vers;
	SVC_REPLY(xprt, &rply);
}

/* ******************* SERVER INPUT STUFF ******************* */

/*
 * Get server side input from some transport.
 *
 * Statement of authentication parameters management:
 * This function owns and manages all authentication parameters, specifically
 * the "raw" parameters (msg.rm_call.cb_cred and msg.rm_call.cb_verf) and
 * the "cooked" credentials (rqst->rq_clntcred).
 * However, this function does not know the structure of the cooked
 * credentials, so it make the following assumptions:
 *   a) the structure is contiguous (no pointers), and
 *   b) the cred structure size does not exceed RQCRED_SIZE bytes.
 * In all events, all three parameters are freed upon exit from this routine.
 * The storage is trivially management on the call stack in user land, but
 * is mallocated in kernel land.
 */

void
svc_getreq(int rdfds)
{
#ifdef FD_SETSIZE
	fd_set readfds;
	int	i, mask;

	FD_ZERO(&readfds);
	for (i=0, mask=1; rdfds; i++, mask <<=1) {
		if (rdfds & mask)
			FD_SET(i, &readfds);
		rdfds &= ~mask;
	}
	svc_getreqset(&readfds);
#else
	int readfds = rdfds & svc_fds;

	svc_getreqset(&readfds);
#endif /* def FD_SETSIZE */
}

#ifdef FD_SETSIZE
#define FDSET_TYPE fd_set
#else
#define FDSET_TYPE int
#endif

void
svc_getreqset(FDSET_TYPE *readfds)
{
#ifndef FD_SETSIZE
	int readfds_local = *readfds;
#endif
	SVCXPRT *xprt;
	int sock;

#ifdef FD_SETSIZE
	for (sock = 0; sock <= svc_maxfd; sock++) {
		if (!FD_ISSET(sock, readfds))
			continue;
		/* sock has input waiting */
		xprt = xports[sock];
		/* now receive msgs from xprtprt (support batch calls) */
		svc_do_xprt(xprt);
	}
#else
	for (sock = 0; readfds_local != 0; sock++, readfds_local >>= 1) {
		if ((readfds_local & 1) == 0)
			continue;
		/* sock has input waiting */
		xprt = xports[sock];
		/* now receive msgs from xprtprt (support batch calls) */
		svc_do_xprt(xprt);
	}
#endif
}

extern struct svc_auth_ops svc_auth_gss_ops;

static void
svc_do_xprt(SVCXPRT *xprt)
{
	caddr_t rawcred, rawverf, cookedcred;
	struct rpc_msg msg;
	struct svc_req r;
        bool_t no_dispatch;
	int prog_found;
	rpcvers_t low_vers;
	rpcvers_t high_vers;
	enum xprt_stat stat;

	rawcred = mem_alloc(MAX_AUTH_BYTES);
	rawverf = mem_alloc(MAX_AUTH_BYTES);
	cookedcred = mem_alloc(RQCRED_SIZE);

	if (rawcred == NULL || rawverf == NULL || cookedcred == NULL)
		return;

	msg.rm_call.cb_cred.oa_base = rawcred;
	msg.rm_call.cb_verf.oa_base = rawverf;
	r.rq_clntcred = cookedcred;

	do {
		struct svc_callout *s;
		enum auth_stat why;

		if (!SVC_RECV(xprt, &msg))
			goto call_done;

		/* now find the exported program and call it */

		r.rq_xprt = xprt;
		r.rq_prog = msg.rm_call.cb_prog;
		r.rq_vers = msg.rm_call.cb_vers;
		r.rq_proc = msg.rm_call.cb_proc;
		r.rq_cred = msg.rm_call.cb_cred;

		no_dispatch = FALSE;

		/* first authenticate the message */
		why = gssrpc__authenticate(&r, &msg, &no_dispatch);
		if (why != AUTH_OK) {
			svcerr_auth(xprt, why);
			goto call_done;
		} else if (no_dispatch) {
			goto call_done;
		}

		/* now match message with a registered service*/
		prog_found = FALSE;
		low_vers = (rpcvers_t) -1L;
		high_vers = 0;
		for (s = svc_head; s != NULL_SVC; s = s->sc_next) {
			if (s->sc_prog == r.rq_prog) {
				if (s->sc_vers == r.rq_vers) {
					(*s->sc_dispatch)(&r, xprt);
					goto call_done;
				}  /* found correct version */
				prog_found = TRUE;
				if (s->sc_vers < low_vers)
					low_vers = s->sc_vers;
				if (s->sc_vers > high_vers)
					high_vers = s->sc_vers;
			}   /* found correct program */
		}
		/*
		 * if we got here, the program or version
		 * is not served ...
		 */
		if (prog_found)
			svcerr_progvers(xprt,
					low_vers, high_vers);
		else
			svcerr_noprog(xprt);
		/* Fall through to ... */

	call_done:
		if ((stat = SVC_STAT(xprt)) == XPRT_DIED){
			SVC_DESTROY(xprt);
			break;
		} else if ((xprt->xp_auth != NULL) &&
			   (xprt->xp_auth->svc_ah_ops != &svc_auth_gss_ops)) {
			xprt->xp_auth = NULL;
		}
	} while (stat == XPRT_MOREREQS);

	mem_free(rawcred, MAX_AUTH_BYTES);
	mem_free(rawverf, MAX_AUTH_BYTES);
	mem_free(cookedcred, RQCRED_SIZE);
}
