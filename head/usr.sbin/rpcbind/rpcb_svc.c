/*	$NetBSD: rpcb_svc.c,v 1.1 2000/06/02 23:15:41 fvdl Exp $	*/
/*	$FreeBSD$ */

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
/*
 * Copyright (c) 1986 - 1991 by Sun Microsystems, Inc.
 */

/* #ident	"@(#)rpcb_svc.c	1.16	93/07/05 SMI" */

/*
 * rpcb_svc.c
 * The server procedure for the version 3 rpcbind (TLI).
 *
 * It maintains a separate list of all the registered services with the
 * version 3 of rpcbind.
 */
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpc/rpcb_prot.h>
#include <netconfig.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rpcbind.h"

static void *rpcbproc_getaddr_3_local(void *, struct svc_req *, SVCXPRT *,
					   rpcvers_t);
static void *rpcbproc_dump_3_local(void *, struct svc_req *, SVCXPRT *,
					rpcvers_t);

/*
 * Called by svc_getreqset. There is a separate server handle for
 * every transport that it waits on.
 */
void
rpcb_service_3(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		RPCB rpcbproc_set_3_arg;
		RPCB rpcbproc_unset_3_arg;
		RPCB rpcbproc_getaddr_3_local_arg;
		struct rpcb_rmtcallargs rpcbproc_callit_3_arg;
		char *rpcbproc_uaddr2taddr_3_arg;
		struct netbuf rpcbproc_taddr2uaddr_3_arg;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	void *(*local)(void *, struct svc_req *, SVCXPRT *, rpcvers_t);

	rpcbs_procinfo(RPCBVERS_3_STAT, rqstp->rq_proc);

	switch (rqstp->rq_proc) {
	case NULLPROC:
		/*
		 * Null proc call
		 */
#ifdef RPCBIND_DEBUG
		if (debugging)
			fprintf(stderr, "RPCBPROC_NULL\n");
#endif
		/* This call just logs, no actual checks */
		check_access(transp, rqstp->rq_proc, NULL, RPCBVERS);
		(void) svc_sendreply(transp, (xdrproc_t)xdr_void, (char *)NULL);
		return;

	case RPCBPROC_SET:
		xdr_argument = (xdrproc_t )xdr_rpcb;
		xdr_result = (xdrproc_t )xdr_bool;
		local = rpcbproc_set_com;
		break;

	case RPCBPROC_UNSET:
		xdr_argument = (xdrproc_t)xdr_rpcb;
		xdr_result = (xdrproc_t)xdr_bool;
		local = rpcbproc_unset_com;
		break;

	case RPCBPROC_GETADDR:
		xdr_argument = (xdrproc_t)xdr_rpcb;
		xdr_result = (xdrproc_t)xdr_wrapstring;
		local = rpcbproc_getaddr_3_local;
		break;

	case RPCBPROC_DUMP:
#ifdef RPCBIND_DEBUG
		if (debugging)
			fprintf(stderr, "RPCBPROC_DUMP\n");
#endif
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_rpcblist_ptr;
		local = rpcbproc_dump_3_local;
		break;

	case RPCBPROC_CALLIT:
		rpcbproc_callit_com(rqstp, transp, rqstp->rq_proc, RPCBVERS);
		return;

	case RPCBPROC_GETTIME:
#ifdef RPCBIND_DEBUG
		if (debugging)
			fprintf(stderr, "RPCBPROC_GETTIME\n");
#endif
		xdr_argument = (xdrproc_t)xdr_void;
		xdr_result = (xdrproc_t)xdr_u_long;
		local = rpcbproc_gettime_com;
		break;

	case RPCBPROC_UADDR2TADDR:
#ifdef RPCBIND_DEBUG
		if (debugging)
			fprintf(stderr, "RPCBPROC_UADDR2TADDR\n");
#endif
		xdr_argument = (xdrproc_t)xdr_wrapstring;
		xdr_result = (xdrproc_t)xdr_netbuf;
		local = rpcbproc_uaddr2taddr_com;
		break;

	case RPCBPROC_TADDR2UADDR:
#ifdef RPCBIND_DEBUG
		if (debugging)
			fprintf(stderr, "RPCBPROC_TADDR2UADDR\n");
#endif
		xdr_argument = (xdrproc_t)xdr_netbuf;
		xdr_result = (xdrproc_t)xdr_wrapstring;
		local = rpcbproc_taddr2uaddr_com;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, (xdrproc_t) xdr_argument,
				(char *) &argument)) {
		svcerr_decode(transp);
		if (debugging)
			(void) fprintf(stderr, "rpcbind: could not decode\n");
		return;
	}
	if (!check_access(transp, rqstp->rq_proc, &argument, RPCBVERS)) {
		svcerr_weakauth(transp);
		goto done;
	}
	result = (*local)(&argument, rqstp, transp, RPCBVERS);
	if (result != NULL && !svc_sendreply(transp, (xdrproc_t)xdr_result,
						result)) {
		svcerr_systemerr(transp);
		if (debugging) {
			(void) fprintf(stderr, "rpcbind: svc_sendreply\n");
			if (doabort) {
				rpcbind_abort();
			}
		}
	}
done:
	if (!svc_freeargs(transp, (xdrproc_t)xdr_argument, (char *)
				&argument)) {
		if (debugging) {
			(void) fprintf(stderr, "unable to free arguments\n");
			if (doabort) {
				rpcbind_abort();
			}
		}
	}
}

/*
 * Lookup the mapping for a program, version and return its
 * address. Assuming that the caller wants the address of the
 * server running on the transport on which the request came.
 *
 * We also try to resolve the universal address in terms of
 * address of the caller.
 */
/* ARGSUSED */
static void *
rpcbproc_getaddr_3_local(void *arg, struct svc_req *rqstp __unused,
			 SVCXPRT *transp __unused, rpcvers_t versnum __unused)
{
	RPCB *regp = (RPCB *)arg;
#ifdef RPCBIND_DEBUG
	if (debugging) {
		char *uaddr;

		uaddr = taddr2uaddr(rpcbind_get_conf(transp->xp_netid),
			    svc_getrpccaller(transp));
		fprintf(stderr, "RPCB_GETADDR req for (%lu, %lu, %s) from %s: ",
		    (unsigned long)regp->r_prog, (unsigned long)regp->r_vers,
		    regp->r_netid, uaddr);
		free(uaddr);
	}
#endif
	return (rpcbproc_getaddr_com(regp, rqstp, transp, RPCBVERS,
	    RPCB_ALLVERS));
}

/* ARGSUSED */
static void *
rpcbproc_dump_3_local(void *arg __unused, struct svc_req *rqstp __unused,
		      SVCXPRT *transp __unused, rpcvers_t versnum __unused)
{
	return ((void *)&list_rbl);
}
