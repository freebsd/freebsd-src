/*	$NetBSD: pmap_svc.c,v 1.2 2000/10/20 11:49:40 fvdl Exp $	*/
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
 * Copyright (c) 1984 - 1991 by Sun Microsystems, Inc.
 */

/* #ident	"@(#)pmap_svc.c	1.14	93/07/05 SMI" */

#if 0
#ifndef lint
static	char sccsid[] = "@(#)pmap_svc.c 1.23 89/04/05 Copyr 1984 Sun Micro";
#endif
#endif

/*
 * pmap_svc.c
 * The server procedure for the version 2 portmaper.
 * All the portmapper related interface from the portmap side.
 */

#ifdef PORTMAP
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/rpcb_prot.h>
#ifdef RPCBIND_DEBUG
#include <stdlib.h>
#endif
#include "rpcbind.h"

static struct pmaplist *find_service_pmap __P((rpcprog_t, rpcvers_t,
					       rpcprot_t));
static bool_t pmapproc_change __P((struct svc_req *, SVCXPRT *, u_long));
static bool_t pmapproc_getport __P((struct svc_req *, SVCXPRT *));
static bool_t pmapproc_dump __P((struct svc_req *, SVCXPRT *));

/*
 * Called for all the version 2 inquiries.
 */
void
pmap_service(struct svc_req *rqstp, SVCXPRT *xprt)
{
	rpcbs_procinfo(RPCBVERS_2_STAT, rqstp->rq_proc);
	switch (rqstp->rq_proc) {
	case PMAPPROC_NULL:
		/*
		 * Null proc call
		 */
#ifdef RPCBIND_DEBUG
		if (debugging)
			fprintf(stderr, "PMAPPROC_NULL\n");
#endif
		check_access(xprt, rqstp->rq_proc, NULL, PMAPVERS);
		if ((!svc_sendreply(xprt, (xdrproc_t) xdr_void, NULL)) &&
			debugging) {
			if (doabort) {
				rpcbind_abort();
			}
		}
		break;

	case PMAPPROC_SET:
		/*
		 * Set a program, version to port mapping
		 */
		pmapproc_change(rqstp, xprt, rqstp->rq_proc);
		break;

	case PMAPPROC_UNSET:
		/*
		 * Remove a program, version to port mapping.
		 */
		pmapproc_change(rqstp, xprt, rqstp->rq_proc);
		break;

	case PMAPPROC_GETPORT:
		/*
		 * Lookup the mapping for a program, version and return its
		 * port number.
		 */
		pmapproc_getport(rqstp, xprt);
		break;

	case PMAPPROC_DUMP:
		/*
		 * Return the current set of mapped program, version
		 */
#ifdef RPCBIND_DEBUG
		if (debugging)
			fprintf(stderr, "PMAPPROC_DUMP\n");
#endif
		pmapproc_dump(rqstp, xprt);
		break;

	case PMAPPROC_CALLIT:
		/*
		 * Calls a procedure on the local machine. If the requested
		 * procedure is not registered this procedure does not return
		 * error information!!
		 * This procedure is only supported on rpc/udp and calls via
		 * rpc/udp. It passes null authentication parameters.
		 */
		rpcbproc_callit_com(rqstp, xprt, PMAPPROC_CALLIT, PMAPVERS);
		break;

	default:
		svcerr_noproc(xprt);
		break;
	}
}

/*
 * returns the item with the given program, version number. If that version
 * number is not found, it returns the item with that program number, so that
 * the port number is now returned to the caller. The caller when makes a
 * call to this program, version number, the call will fail and it will
 * return with PROGVERS_MISMATCH. The user can then determine the highest
 * and the lowest version number for this program using clnt_geterr() and
 * use those program version numbers.
 */
static struct pmaplist *
find_service_pmap(rpcprog_t prog, rpcvers_t vers, rpcprot_t prot)
{
	register struct pmaplist *hit = NULL;
	register struct pmaplist *pml;

	for (pml = list_pml; pml != NULL; pml = pml->pml_next) {
		if ((pml->pml_map.pm_prog != prog) ||
			(pml->pml_map.pm_prot != prot))
			continue;
		hit = pml;
		if (pml->pml_map.pm_vers == vers)
			break;
	}
	return (hit);
}

static bool_t
pmapproc_change(struct svc_req *rqstp, SVCXPRT *xprt, unsigned long op)
{
	struct pmap reg;
	RPCB rpcbreg;
	long ans;
	struct sockaddr_in *who;
	uid_t uid;
	char uidbuf[32];

#ifdef RPCBIND_DEBUG
	if (debugging)
		fprintf(stderr, "%s request for (%lu, %lu) : ",
		    op == PMAPPROC_SET ? "PMAP_SET" : "PMAP_UNSET",
		    reg.pm_prog, reg.pm_vers);
#endif

	if (!svc_getargs(xprt, (xdrproc_t) xdr_pmap, (char *)&reg)) {
		svcerr_decode(xprt);
		return (FALSE);
	}

	if (!check_access(xprt, op, &reg, PMAPVERS)) {
		svcerr_weakauth(xprt);
		return FALSE;
	}

	who = svc_getcaller(xprt);

	/*
	 * Can't use getpwnam here. We might end up calling ourselves
	 * and looping.
	 */
	if (__rpc_get_local_uid(xprt, &uid) < 0)
		rpcbreg.r_owner = "unknown";
	else if (uid == 0)
		rpcbreg.r_owner = "superuser";
	else {
		/* r_owner will be strdup-ed later */
		snprintf(uidbuf, sizeof uidbuf, "%d", uid);
		rpcbreg.r_owner = uidbuf;
	}

	rpcbreg.r_prog = reg.pm_prog;
	rpcbreg.r_vers = reg.pm_vers;

	if (op == PMAPPROC_SET) {
		char buf[32];

		snprintf(buf, sizeof buf, "0.0.0.0.%d.%d",
		    (int)((reg.pm_port >> 8) & 0xff),
		    (int)(reg.pm_port & 0xff));
		rpcbreg.r_addr = buf;
		if (reg.pm_prot == IPPROTO_UDP) {
			rpcbreg.r_netid = udptrans;
		} else if (reg.pm_prot == IPPROTO_TCP) {
			rpcbreg.r_netid = tcptrans;
		} else {
			ans = FALSE;
			goto done_change;
		}
		ans = map_set(&rpcbreg, rpcbreg.r_owner);
	} else if (op == PMAPPROC_UNSET) {
		bool_t ans1, ans2;

		rpcbreg.r_addr = NULL;
		rpcbreg.r_netid = tcptrans;
		ans1 = map_unset(&rpcbreg, rpcbreg.r_owner);
		rpcbreg.r_netid = udptrans;
		ans2 = map_unset(&rpcbreg, rpcbreg.r_owner);
		ans = ans1 || ans2;
	} else {
		ans = FALSE;
	}
done_change:
	if ((!svc_sendreply(xprt, (xdrproc_t) xdr_long, (caddr_t) &ans)) &&
	    debugging) {
		fprintf(stderr, "portmap: svc_sendreply\n");
		if (doabort) {
			rpcbind_abort();
		}
	}
#ifdef RPCBIND_DEBUG
	if (debugging)
		fprintf(stderr, "%s\n", ans == TRUE ? "succeeded" : "failed");
#endif
	if (op == PMAPPROC_SET)
		rpcbs_set(RPCBVERS_2_STAT, ans);
	else
		rpcbs_unset(RPCBVERS_2_STAT, ans);
	return (TRUE);
}

/* ARGSUSED */
static bool_t
pmapproc_getport(struct svc_req *rqstp, SVCXPRT *xprt)
{
	struct pmap reg;
	long lport;
	int port = 0;
	struct pmaplist *fnd;
#ifdef RPCBIND_DEBUG
	char *uaddr;
#endif

	if (!svc_getargs(xprt, (xdrproc_t) xdr_pmap, (char *)&reg)) {
		svcerr_decode(xprt);
		return (FALSE);
	}

	if (!check_access(xprt, PMAPPROC_GETPORT, &reg, PMAPVERS)) {
		svcerr_weakauth(xprt);
		return FALSE;
	}

#ifdef RPCBIND_DEBUG
	if (debugging) {
		uaddr =  taddr2uaddr(rpcbind_get_conf(xprt->xp_netid),
			    svc_getrpccaller(xprt));
		fprintf(stderr, "PMAP_GETPORT req for (%lu, %lu, %s) from %s :",
			reg.pm_prog, reg.pm_vers,
			reg.pm_prot == IPPROTO_UDP ? "udp" : "tcp", uaddr);
		free(uaddr);
	}
#endif
	fnd = find_service_pmap(reg.pm_prog, reg.pm_vers, reg.pm_prot);
	if (fnd) {
		char serveuaddr[32], *ua;
		int h1, h2, h3, h4, p1, p2;
		char *netid;

		if (reg.pm_prot == IPPROTO_UDP) {
			ua = udp_uaddr;
			netid = udptrans;
		} else {
			ua = tcp_uaddr; /* To get the len */
			netid = tcptrans;
		}
		if (ua == NULL) {
			goto sendreply;
		}
		if (sscanf(ua, "%d.%d.%d.%d.%d.%d", &h1, &h2, &h3,
				&h4, &p1, &p2) == 6) {
			p1 = (fnd->pml_map.pm_port >> 8) & 0xff;
			p2 = (fnd->pml_map.pm_port) & 0xff;
			snprintf(serveuaddr, sizeof serveuaddr,
			    "%d.%d.%d.%d.%d.%d", h1, h2, h3, h4, p1, p2);
			if (is_bound(netid, serveuaddr)) {
				port = fnd->pml_map.pm_port;
			} else { /* this service is dead; delete it */
				delete_prog(reg.pm_prog);
			}
		}
	}
sendreply:
	lport = port;
	if ((!svc_sendreply(xprt, (xdrproc_t) xdr_long, (caddr_t)&lport)) &&
			debugging) {
		(void) fprintf(stderr, "portmap: svc_sendreply\n");
		if (doabort) {
			rpcbind_abort();
		}
	}
#ifdef RPCBIND_DEBUG
	if (debugging)
		fprintf(stderr, "port = %d\n", port);
#endif
	rpcbs_getaddr(RPCBVERS_2_STAT, reg.pm_prog, reg.pm_vers,
		reg.pm_prot == IPPROTO_UDP ? udptrans : tcptrans,
		port ? udptrans : "");

	return (TRUE);
}

/* ARGSUSED */
static bool_t
pmapproc_dump(struct svc_req *rqstp, SVCXPRT *xprt)
{
	if (!svc_getargs(xprt, (xdrproc_t)xdr_void, NULL)) {
		svcerr_decode(xprt);
		return (FALSE);
	}

	if (!check_access(xprt, PMAPPROC_DUMP, NULL, PMAPVERS)) {
		svcerr_weakauth(xprt);
		return FALSE;
	}

	if ((!svc_sendreply(xprt, (xdrproc_t) xdr_pmaplist_ptr,
			(caddr_t)&list_pml)) && debugging) {
		if (debugging)
			(void) fprintf(stderr, "portmap: svc_sendreply\n");
		if (doabort) {
			rpcbind_abort();
		}
	}
	return (TRUE);
}

#endif /* PORTMAP */
