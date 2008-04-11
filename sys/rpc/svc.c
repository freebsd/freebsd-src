/*	$NetBSD: svc.c,v 1.21 2000/07/06 03:10:35 christos Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid2 = "@(#)svc.c 1.44 88/02/08 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc.c	2.4 88/08/11 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * svc.c, Server-side remote procedure call interface.
 *
 * There are two sets of procedures here.  The xprt routines are
 * for handling transport handles.  The svc routines handle the
 * list of service routines.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/ucred.h>

#include <rpc/rpc.h>
#include <rpc/rpcb_clnt.h>

#include <rpc/rpc_com.h>

#define SVC_VERSQUIET 0x0001		/* keep quiet about vers mismatch */
#define version_keepquiet(xp) ((u_long)(xp)->xp_p3 & SVC_VERSQUIET)

static struct svc_callout *svc_find(SVCPOOL *pool, rpcprog_t, rpcvers_t,
    char *);
static void __xprt_do_unregister (SVCXPRT *xprt, bool_t dolock);

/* ***************  SVCXPRT related stuff **************** */

SVCPOOL*
svcpool_create(void)
{
	SVCPOOL *pool;

	pool = malloc(sizeof(SVCPOOL), M_RPC, M_WAITOK|M_ZERO);
	
	mtx_init(&pool->sp_lock, "sp_lock", NULL, MTX_DEF);
	TAILQ_INIT(&pool->sp_xlist);
	TAILQ_INIT(&pool->sp_active);
	TAILQ_INIT(&pool->sp_callouts);

	return pool;
}

void
svcpool_destroy(SVCPOOL *pool)
{
	SVCXPRT *xprt;
	struct svc_callout *s;

	mtx_lock(&pool->sp_lock);

	while (TAILQ_FIRST(&pool->sp_xlist)) {
		xprt = TAILQ_FIRST(&pool->sp_xlist);
		mtx_unlock(&pool->sp_lock);
		SVC_DESTROY(xprt);
		mtx_lock(&pool->sp_lock);
	}

	while (TAILQ_FIRST(&pool->sp_callouts)) {
		s = TAILQ_FIRST(&pool->sp_callouts);
		mtx_unlock(&pool->sp_lock);
		svc_unreg(pool, s->sc_prog, s->sc_vers);
		mtx_lock(&pool->sp_lock);
	}

	mtx_destroy(&pool->sp_lock);
	free(pool, M_RPC);
}

/*
 * Activate a transport handle.
 */
void
xprt_register(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	mtx_lock(&pool->sp_lock);
	xprt->xp_registered = TRUE;
	xprt->xp_active = FALSE;
	TAILQ_INSERT_TAIL(&pool->sp_xlist, xprt, xp_link);
	mtx_unlock(&pool->sp_lock);
}

void
xprt_unregister(SVCXPRT *xprt)
{
	__xprt_do_unregister(xprt, TRUE);
}

void
__xprt_unregister_unlocked(SVCXPRT *xprt)
{
	__xprt_do_unregister(xprt, FALSE);
}

/*
 * De-activate a transport handle.
 */
static void
__xprt_do_unregister(SVCXPRT *xprt, bool_t dolock)
{
	SVCPOOL *pool = xprt->xp_pool;

	//__svc_generic_cleanup(xprt);

	if (dolock)
		mtx_lock(&pool->sp_lock);

	if (xprt->xp_active) {
		TAILQ_REMOVE(&pool->sp_active, xprt, xp_alink);
		xprt->xp_active = FALSE;
	}
	TAILQ_REMOVE(&pool->sp_xlist, xprt, xp_link);
	xprt->xp_registered = FALSE;

	if (dolock)
		mtx_unlock(&pool->sp_lock);
}

void
xprt_active(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	mtx_lock(&pool->sp_lock);

	if (!xprt->xp_active) {
		TAILQ_INSERT_TAIL(&pool->sp_active, xprt, xp_alink);
		xprt->xp_active = TRUE;
	}
	wakeup(&pool->sp_active);

	mtx_unlock(&pool->sp_lock);
}

void
xprt_inactive(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;

	mtx_lock(&pool->sp_lock);

	if (xprt->xp_active) {
		TAILQ_REMOVE(&pool->sp_active, xprt, xp_alink);
		xprt->xp_active = FALSE;
	}
	wakeup(&pool->sp_active);

	mtx_unlock(&pool->sp_lock);
}

/*
 * Add a service program to the callout list.
 * The dispatch routine will be called when a rpc request for this
 * program number comes in.
 */
bool_t
svc_reg(SVCXPRT *xprt, const rpcprog_t prog, const rpcvers_t vers,
    void (*dispatch)(struct svc_req *, SVCXPRT *),
    const struct netconfig *nconf)
{
	SVCPOOL *pool = xprt->xp_pool;
	struct svc_callout *s;
	char *netid = NULL;
	int flag = 0;

/* VARIABLES PROTECTED BY svc_lock: s, svc_head */

	if (xprt->xp_netid) {
		netid = strdup(xprt->xp_netid, M_RPC);
		flag = 1;
	} else if (nconf && nconf->nc_netid) {
		netid = strdup(nconf->nc_netid, M_RPC);
		flag = 1;
	} /* must have been created with svc_raw_create */
	if ((netid == NULL) && (flag == 1)) {
		return (FALSE);
	}

	mtx_lock(&pool->sp_lock);
	if ((s = svc_find(pool, prog, vers, netid)) != NULL) {
		if (netid)
			free(netid, M_RPC);
		if (s->sc_dispatch == dispatch)
			goto rpcb_it; /* he is registering another xptr */
		mtx_unlock(&pool->sp_lock);
		return (FALSE);
	}
	s = malloc(sizeof (struct svc_callout), M_RPC, M_NOWAIT);
	if (s == NULL) {
		if (netid)
			free(netid, M_RPC);
		mtx_unlock(&pool->sp_lock);
		return (FALSE);
	}

	s->sc_prog = prog;
	s->sc_vers = vers;
	s->sc_dispatch = dispatch;
	s->sc_netid = netid;
	TAILQ_INSERT_TAIL(&pool->sp_callouts, s, sc_link);

	if ((xprt->xp_netid == NULL) && (flag == 1) && netid)
		((SVCXPRT *) xprt)->xp_netid = strdup(netid, M_RPC);

rpcb_it:
	mtx_unlock(&pool->sp_lock);
	/* now register the information with the local binder service */
	if (nconf) {
		bool_t dummy;
		struct netconfig tnc;
		tnc = *nconf;
		dummy = rpcb_set(prog, vers, &tnc,
		    &((SVCXPRT *) xprt)->xp_ltaddr);
		return (dummy);
	}
	return (TRUE);
}

/*
 * Remove a service program from the callout list.
 */
void
svc_unreg(SVCPOOL *pool, const rpcprog_t prog, const rpcvers_t vers)
{
	struct svc_callout *s;

	/* unregister the information anyway */
	(void) rpcb_unset(prog, vers, NULL);
	mtx_lock(&pool->sp_lock);
	while ((s = svc_find(pool, prog, vers, NULL)) != NULL) {
		TAILQ_REMOVE(&pool->sp_callouts, s, sc_link);
		if (s->sc_netid)
			mem_free(s->sc_netid, sizeof (s->sc_netid) + 1);
		mem_free(s, sizeof (struct svc_callout));
	}
	mtx_unlock(&pool->sp_lock);
}

/* ********************** CALLOUT list related stuff ************* */

/*
 * Search the callout list for a program number, return the callout
 * struct.
 */
static struct svc_callout *
svc_find(SVCPOOL *pool, rpcprog_t prog, rpcvers_t vers, char *netid)
{
	struct svc_callout *s;

	mtx_assert(&pool->sp_lock, MA_OWNED);
	TAILQ_FOREACH(s, &pool->sp_callouts, sc_link) {
		if (s->sc_prog == prog && s->sc_vers == vers
		    && (netid == NULL || s->sc_netid == NULL ||
			strcmp(netid, s->sc_netid) == 0))
			break;
	}

	return (s);
}

/* ******************* REPLY GENERATION ROUTINES  ************ */

/*
 * Send a reply to an rpc request
 */
bool_t
svc_sendreply(SVCXPRT *xprt, xdrproc_t xdr_results, void * xdr_location)
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
svcerr_auth(SVCXPRT *xprt, enum auth_stat why)
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
svcerr_progvers(SVCXPRT *xprt, rpcvers_t low_vers, rpcvers_t high_vers)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROG_MISMATCH;
	rply.acpted_rply.ar_vers.low = (uint32_t)low_vers;
	rply.acpted_rply.ar_vers.high = (uint32_t)high_vers;

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
 * In-kernel, we represent non-trivial cooked creds with struct ucred.
 * In all events, all three parameters are freed upon exit from this routine.
 * The storage is trivially management on the call stack in user land, but
 * is mallocated in kernel land.
 */

static void
svc_getreq(SVCXPRT *xprt)
{
	SVCPOOL *pool = xprt->xp_pool;
	struct svc_req r;
	struct rpc_msg msg;
	int prog_found;
	rpcvers_t low_vers;
	rpcvers_t high_vers;
	enum xprt_stat stat;
	char cred_area[2*MAX_AUTH_BYTES + sizeof(struct xucred)];

	msg.rm_call.cb_cred.oa_base = cred_area;
	msg.rm_call.cb_verf.oa_base = &cred_area[MAX_AUTH_BYTES];
	r.rq_clntcred = &cred_area[2*MAX_AUTH_BYTES];

	/* now receive msgs from xprtprt (support batch calls) */
	do {
		if (SVC_RECV(xprt, &msg)) {

			/* now find the exported program and call it */
			struct svc_callout *s;
			enum auth_stat why;

			r.rq_xprt = xprt;
			r.rq_prog = msg.rm_call.cb_prog;
			r.rq_vers = msg.rm_call.cb_vers;
			r.rq_proc = msg.rm_call.cb_proc;
			r.rq_cred = msg.rm_call.cb_cred;
			/* first authenticate the message */
			if ((why = _authenticate(&r, &msg)) != AUTH_OK) {
				svcerr_auth(xprt, why);
				goto call_done;
			}
			/* now match message with a registered service*/
			prog_found = FALSE;
			low_vers = (rpcvers_t) -1L;
			high_vers = (rpcvers_t) 0L;
			TAILQ_FOREACH(s, &pool->sp_callouts, sc_link) {
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
				svcerr_progvers(xprt, low_vers, high_vers);
			else
				svcerr_noprog(xprt);
			/* Fall through to ... */
		}
		/*
		 * Check if the xprt has been disconnected in a
		 * recursive call in the service dispatch routine.
		 * If so, then break.
		 */
		mtx_lock(&pool->sp_lock);
		if (!xprt->xp_registered) {
			mtx_unlock(&pool->sp_lock);
			break;
		}
		mtx_unlock(&pool->sp_lock);
call_done:
		if ((stat = SVC_STAT(xprt)) == XPRT_DIED) {
			SVC_DESTROY(xprt);
			break;
		}
	} while (stat == XPRT_MOREREQS);
}

void
svc_run(SVCPOOL *pool)
{
	SVCXPRT *xprt;
	int error;

	mtx_lock(&pool->sp_lock);

	pool->sp_exited = FALSE;

	while (!pool->sp_exited) {
		xprt = TAILQ_FIRST(&pool->sp_active);
		if (!xprt) {
			error = msleep(&pool->sp_active, &pool->sp_lock, PCATCH,
			    "rpcsvc", 0);
			if (error)
				break;
			continue;
		}

		/*
		 * Move this transport to the end to ensure fairness
		 * when multiple transports are active. If this was
		 * the last queued request, svc_getreq will end up
		 * calling xprt_inactive to remove from the active
		 * list.
		 */
		TAILQ_REMOVE(&pool->sp_active, xprt, xp_alink);
		TAILQ_INSERT_TAIL(&pool->sp_active, xprt, xp_alink);

		mtx_unlock(&pool->sp_lock);
		svc_getreq(xprt);
		mtx_lock(&pool->sp_lock);
	}

	mtx_unlock(&pool->sp_lock);
}

void
svc_exit(SVCPOOL *pool)
{
	mtx_lock(&pool->sp_lock);
	pool->sp_exited = TRUE;
	wakeup(&pool->sp_active);
	mtx_unlock(&pool->sp_lock);
}
