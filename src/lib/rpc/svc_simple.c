/* @(#)svc_simple.c	2.2 88/08/01 4.0 RPCSRC */
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
static char sccsid[] = "@(#)svc_simple.c 1.18 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * svc_simple.c
 * Simplified front end to rpc.
 */

#include <stdio.h>
#include <string.h>
#include <gssrpc/rpc.h>
#include <gssrpc/pmap_clnt.h>
#include <sys/socket.h>
#include <netdb.h>

static struct proglst {
	char *(*p_progname)();
	int  p_prognum;
	int  p_procnum;
	xdrproc_t p_inproc, p_outproc;
	struct proglst *p_nxt;
} *proglst;
static void universal(struct svc_req *, SVCXPRT *);
static SVCXPRT *transp;

int
registerrpc(
	rpcprog_t prognum,
	rpcvers_t versnum,
	rpcproc_t procnum,
	char *(*progname)(),
	xdrproc_t inproc,
	xdrproc_t outproc)
{
        struct proglst *pl;

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
	(void) pmap_unset(prognum, versnum);
	if (!svc_register(transp, prognum, versnum, universal, IPPROTO_UDP)) {
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
universal(
	struct svc_req *rqstp,
	SVCXPRT *s_transp)
{
	int prog, proc;
	char *outdata;
	char xdrbuf[UDPMSGSIZE];
	struct proglst *pl;

	/*
	 * enforce "procnum 0 is echo" convention
	 */
	if (rqstp->rq_proc == NULLPROC) {
		if (svc_sendreply(s_transp, xdr_void, (char *)NULL) == FALSE) {
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
			memset(xdrbuf, 0, sizeof(xdrbuf)); /* required ! */
			if (!svc_getargs(s_transp, pl->p_inproc, xdrbuf)) {
				svcerr_decode(s_transp);
				return;
			}
			outdata = (*(pl->p_progname))(xdrbuf);
			if (outdata == NULL && pl->p_outproc != xdr_void)
				/* there was an error */
				return;
			if (!svc_sendreply(s_transp, pl->p_outproc, outdata)) {
				(void) fprintf(stderr,
				    "trouble replying to prog %d\n",
				    pl->p_prognum);
				exit(1);
			}
			/* free the decoded arguments */
			(void)svc_freeargs(s_transp, pl->p_inproc, xdrbuf);
			return;
		}
	(void) fprintf(stderr, "never registered prog %d\n", prog);
	exit(1);
}
