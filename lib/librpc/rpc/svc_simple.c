/* @(#)svc_simple.c	2.2 88/08/01 4.0 RPCSRC */
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
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)svc_simple.c 1.18 87/08/11 Copyr 1984 Sun Micro";
#endif

/* 
 * svc_simple.c
 * Simplified front end to rpc.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <netdb.h>

static struct proglst {
	char *(*p_progname)();
	int  p_prognum;
	int  p_procnum;
	xdrproc_t p_inproc, p_outproc;
	struct proglst *p_nxt;
} *proglst;
static void universal();
static SVCXPRT *transp;
struct proglst *pl;

registerrpc(prognum, versnum, procnum, progname, inproc, outproc)
	char *(*progname)();
	xdrproc_t inproc, outproc;
{
	
	if (procnum == NULLPROC) {
		(void) fprintf(stderr,
		    "can't reassign procedure number %d\n", NULLPROC);
		return (-1);
	}
	if (transp == 0) {
		transp = svcudp_create(RPC_ANYSOCK);
		if (transp == NULL) {
			(void) fprintf(stderr, "couldn't create an rpc server\n");
			return (-1);
		}
	}
	(void) pmap_unset((u_long)prognum, (u_long)versnum);
	if (!svc_register(transp, (u_long)prognum, (u_long)versnum, 
	    universal, IPPROTO_UDP)) {
	    	(void) fprintf(stderr, "couldn't register prog %d vers %d\n",
		    prognum, versnum);
		return (-1);
	}
	pl = (struct proglst *)malloc(sizeof(struct proglst));
	if (pl == NULL) {
		(void) fprintf(stderr, "registerrpc: out of memory\n");
		return (-1);
	}
	pl->p_progname = progname;
	pl->p_prognum = prognum;
	pl->p_procnum = procnum;
	pl->p_inproc = inproc;
	pl->p_outproc = outproc;
	pl->p_nxt = proglst;
	proglst = pl;
	return (0);
}

static void
universal(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	int prog, proc;
	char *outdata;
	char xdrbuf[UDPMSGSIZE];
	struct proglst *pl;

	/* 
	 * enforce "procnum 0 is echo" convention
	 */
	if (rqstp->rq_proc == NULLPROC) {
		if (svc_sendreply(transp, xdr_void, (char *)NULL) == FALSE) {
			(void) fprintf(stderr, "xxx\n");
			exit(1);
		}
		return;
	}
	prog = rqstp->rq_prog;
	proc = rqstp->rq_proc;
	for (pl = proglst; pl != NULL; pl = pl->p_nxt)
		if (pl->p_prognum == prog && pl->p_procnum == proc) {
			/* decode arguments into a CLEAN buffer */
			bzero(xdrbuf, sizeof(xdrbuf)); /* required ! */
			if (!svc_getargs(transp, pl->p_inproc, xdrbuf)) {
				svcerr_decode(transp);
				return;
			}
			outdata = (*(pl->p_progname))(xdrbuf);
			if (outdata == NULL && pl->p_outproc != xdr_void)
				/* there was an error */
				return;
			if (!svc_sendreply(transp, pl->p_outproc, outdata)) {
				(void) fprintf(stderr,
				    "trouble replying to prog %d\n",
				    pl->p_prognum);
				exit(1);
			}
			/* free the decoded arguments */
			(void)svc_freeargs(transp, pl->p_inproc, xdrbuf);
			return;
		}
	(void) fprintf(stderr, "never registered prog %d\n", prog);
	exit(1);
}

