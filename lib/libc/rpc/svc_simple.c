/*	$NetBSD: svc_simple.c,v 1.20 2000/07/06 03:10:35 christos Exp $	*/

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
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

/* #pragma ident	"@(#)svc_simple.c	1.18	94/04/24 SMI" */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * svc_simple.c
 * Simplified front end to rpc.
 */

/*
 * This interface creates a virtual listener for all the services
 * started thru rpc_reg(). It listens on the same endpoint for
 * all the services and then executes the corresponding service
 * for the given prognum and procnum.
 */

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpc/nettype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include "un-namespace.h"

#include "rpc_com.h"

static void universal(struct svc_req *, SVCXPRT *);

static struct proglst {
	char *(*p_progname)(char *);
	rpcprog_t p_prognum;
	rpcvers_t p_versnum;
	rpcproc_t p_procnum;
	SVCXPRT *p_transp;
	char *p_netid;
	char *p_xdrbuf;
	int p_recvsz;
	xdrproc_t p_inproc, p_outproc;
	struct proglst *p_nxt;
} *proglst;

static const char rpc_reg_err[] = "%s: %s";
static const char rpc_reg_msg[] = "rpc_reg: ";
static const char __reg_err1[] = "can't find appropriate transport";
static const char __reg_err2[] = "can't get protocol info";
static const char __reg_err3[] = "unsupported transport size";
static const char __no_mem_str[] = "out of memory";

/*
 * For simplified, easy to use kind of rpc interfaces.
 * nettype indicates the type of transport on which the service will be
 * listening. Used for conservation of the system resource. Only one
 * handle is created for all the services (actually one of each netid)
 * and same xdrbuf is used for same netid. The size of the arguments
 * is also limited by the recvsize for that transport, even if it is
 * a COTS transport. This may be wrong, but for cases like these, they
 * should not use the simplified interfaces like this.
 */

int
rpc_reg(prognum, versnum, procnum, progname, inproc, outproc, nettype)
	rpcprog_t prognum;			/* program number */
	rpcvers_t versnum;			/* version number */
	rpcproc_t procnum;			/* procedure number */
	char *(*progname)(char *); /* Server routine */
	xdrproc_t inproc, outproc;	/* in/out XDR procedures */
	char *nettype;			/* nettype */
{
	struct netconfig *nconf;
	int done = FALSE;
	void *handle;
	extern mutex_t proglst_lock;



	if (procnum == NULLPROC) {
		warnx("%s can't reassign procedure number %u", rpc_reg_msg,
			NULLPROC);
		return (-1);
	}

	if (nettype == NULL)
		nettype = "netpath";		/* The default behavior */
	if ((handle = __rpc_setconf(nettype)) == NULL) {
		warnx(rpc_reg_err, rpc_reg_msg, __reg_err1);
		return (-1);
	}
/* VARIABLES PROTECTED BY proglst_lock: proglst */
	mutex_lock(&proglst_lock);
	while ((nconf = __rpc_getconf(handle)) != NULL) {
		struct proglst *pl;
		SVCXPRT *svcxprt;
		int madenow;
		u_int recvsz;
		char *xdrbuf;
		char *netid;

		madenow = FALSE;
		svcxprt = NULL;
		recvsz = 0;
		xdrbuf = netid = NULL;
		for (pl = proglst; pl; pl = pl->p_nxt) {
			if (strcmp(pl->p_netid, nconf->nc_netid) == 0) {
				svcxprt = pl->p_transp;
				xdrbuf = pl->p_xdrbuf;
				recvsz = pl->p_recvsz;
				netid = pl->p_netid;
				break;
			}
		}

		if (svcxprt == NULL) {
			struct __rpc_sockinfo si;

			svcxprt = svc_tli_create(RPC_ANYFD, nconf, NULL, 0, 0);
			if (svcxprt == NULL)
				continue;
			if (!__rpc_fd2sockinfo(svcxprt->xp_fd, &si)) {
				warnx(rpc_reg_err, rpc_reg_msg, __reg_err2);
				SVC_DESTROY(svcxprt);
				continue;
			}
			recvsz = __rpc_get_t_size(si.si_af, si.si_proto, 0);
			if (recvsz == 0) {
				warnx(rpc_reg_err, rpc_reg_msg, __reg_err3);
				SVC_DESTROY(svcxprt);
				continue;
			}
			if (((xdrbuf = malloc((unsigned)recvsz)) == NULL) ||
				((netid = strdup(nconf->nc_netid)) == NULL)) {
				warnx(rpc_reg_err, rpc_reg_msg, __no_mem_str);
				SVC_DESTROY(svcxprt);
				break;
			}
			madenow = TRUE;
		}
		/*
		 * Check if this (program, version, netid) had already been
		 * registered.  The check may save a few RPC calls to rpcbind
		 */
		for (pl = proglst; pl; pl = pl->p_nxt)
			if ((pl->p_prognum == prognum) &&
				(pl->p_versnum == versnum) &&
				(strcmp(pl->p_netid, netid) == 0))
				break;
		if (pl == NULL) { /* Not yet */
			(void) rpcb_unset(prognum, versnum, nconf);
		} else {
			/* so that svc_reg does not call rpcb_set() */
			nconf = NULL;
		}

		if (!svc_reg(svcxprt, prognum, versnum, universal, nconf)) {
			warnx("%s couldn't register prog %u vers %u for %s",
				rpc_reg_msg, (unsigned)prognum,
				(unsigned)versnum, netid);
			if (madenow) {
				SVC_DESTROY(svcxprt);
				free(xdrbuf);
				free(netid);
			}
			continue;
		}

		pl = malloc(sizeof (struct proglst));
		if (pl == NULL) {
			warnx(rpc_reg_err, rpc_reg_msg, __no_mem_str);
			if (madenow) {
				SVC_DESTROY(svcxprt);
				free(xdrbuf);
				free(netid);
			}
			break;
		}
		pl->p_progname = progname;
		pl->p_prognum = prognum;
		pl->p_versnum = versnum;
		pl->p_procnum = procnum;
		pl->p_inproc = inproc;
		pl->p_outproc = outproc;
		pl->p_transp = svcxprt;
		pl->p_xdrbuf = xdrbuf;
		pl->p_recvsz = recvsz;
		pl->p_netid = netid;
		pl->p_nxt = proglst;
		proglst = pl;
		done = TRUE;
	}
	__rpc_endconf(handle);
	mutex_unlock(&proglst_lock);

	if (done == FALSE) {
		warnx("%s cant find suitable transport for %s",
			rpc_reg_msg, nettype);
		return (-1);
	}
	return (0);
}

/*
 * The universal handler for the services registered using registerrpc.
 * It handles both the connectionless and the connection oriented cases.
 */

static void
universal(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	rpcprog_t prog;
	rpcvers_t vers;
	rpcproc_t proc;
	char *outdata;
	char *xdrbuf;
	struct proglst *pl;
	extern mutex_t proglst_lock;

	/*
	 * enforce "procnum 0 is echo" convention
	 */
	if (rqstp->rq_proc == NULLPROC) {
		if (svc_sendreply(transp, (xdrproc_t) xdr_void, NULL) ==
		    FALSE) {
			warnx("svc_sendreply failed");
		}
		return;
	}
	prog = rqstp->rq_prog;
	vers = rqstp->rq_vers;
	proc = rqstp->rq_proc;
	mutex_lock(&proglst_lock);
	for (pl = proglst; pl; pl = pl->p_nxt)
		if (pl->p_prognum == prog && pl->p_procnum == proc &&
			pl->p_versnum == vers &&
			(strcmp(pl->p_netid, transp->xp_netid) == 0)) {
			/* decode arguments into a CLEAN buffer */
			xdrbuf = pl->p_xdrbuf;
			/* Zero the arguments: reqd ! */
			(void) memset(xdrbuf, 0, sizeof (pl->p_recvsz));
			/*
			 * Assuming that sizeof (xdrbuf) would be enough
			 * for the arguments; if not then the program
			 * may bomb. BEWARE!
			 */
			if (!svc_getargs(transp, pl->p_inproc, xdrbuf)) {
				svcerr_decode(transp);
				mutex_unlock(&proglst_lock);
				return;
			}
			outdata = (*(pl->p_progname))(xdrbuf);
			if (outdata == NULL &&
				pl->p_outproc != (xdrproc_t) xdr_void){
				/* there was an error */
				mutex_unlock(&proglst_lock);
				return;
			}
			if (!svc_sendreply(transp, pl->p_outproc, outdata)) {
				warnx(
			"rpc: rpc_reg trouble replying to prog %u vers %u",
				(unsigned)prog, (unsigned)vers);
				mutex_unlock(&proglst_lock);
				return;
			}
			/* free the decoded arguments */
			(void)svc_freeargs(transp, pl->p_inproc, xdrbuf);
			mutex_unlock(&proglst_lock);
			return;
		}
	mutex_unlock(&proglst_lock);
	/* This should never happen */
	warnx("rpc: rpc_reg: never registered prog %u vers %u",
		(unsigned)prog, (unsigned)vers);
	return;
}
