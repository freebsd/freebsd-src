/*
 * Copyright (c) 1996, 1997
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
 */

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

#ifndef lint
#if 0
static char *sccsid = "@(#)from: clnt_udp.c 1.39 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)from: clnt_udp.c	2.2 88/08/01 4.0 RPCSRC";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif

/*
 * clnt_udp.c, Implements a UDP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpcsvc/yp.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include "yp_ping.h"

struct cu_data {
	int			cu_fd;		/* connections fd */
	bool_t			cu_closeit;	/* opened by library */
	struct sockaddr_storage	cu_raddr;	/* remote address */
	int			cu_rlen;
	struct timeval		cu_wait;	/* retransmit interval */
	struct timeval		cu_total;	/* total time for the call */
	struct rpc_err		cu_error;
	XDR			cu_outxdrs;
	u_int			cu_xdrpos;
	u_int			cu_sendsz;	/* send size */
	char			*cu_outbuf;
	u_int			cu_recvsz;	/* recv size */
	struct pollfd		pfdp;
	int			cu_async;
	char			cu_inbuf[1];
};

/*
 * pmap_getport.c
 * Client interface to pmap rpc service.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */


static struct timeval timeout = { 1, 0 };
static struct timeval tottimeout = { 1, 0 };

/*
 * Find the mapped port for program,version.
 * Calls the pmap service remotely to do the lookup.
 * Returns 0 if no map exists.
 */
static u_short
__pmap_getport(address, program, version, protocol)
	struct sockaddr_in *address;
	u_long program;
	u_long version;
	u_int protocol;
{
	u_short port = 0;
	int sock = -1;
	register CLIENT *client;
	struct pmap parms;

	address->sin_port = htons(PMAPPORT);

	client = clntudp_bufcreate(address, PMAPPROG,
	    PMAPVERS, timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client != (CLIENT *)NULL) {
		parms.pm_prog = program;
		parms.pm_vers = version;
		parms.pm_prot = protocol;
		parms.pm_port = 0;  /* not needed or used */
		if (CLNT_CALL(client, PMAPPROC_GETPORT, xdr_pmap, &parms,
		    xdr_u_short, &port, tottimeout) != RPC_SUCCESS){
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
		} else if (port == 0) {
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		}
		CLNT_DESTROY(client);
	}
	if (sock != -1)
		(void)close(sock);
	address->sin_port = 0;
	return (port);
}

/*
 * Transmit to YPPROC_DOMAIN_NONACK, return immediately.
 */
static bool_t *
ypproc_domain_nonack_2_send(domainname *argp, CLIENT *clnt)
{
	static bool_t clnt_res;
	struct timeval TIMEOUT = { 0, 0 };

	memset((char *)&clnt_res, 0, sizeof (clnt_res));
	if (clnt_call(clnt, YPPROC_DOMAIN_NONACK,
		(xdrproc_t) xdr_domainname, (caddr_t) argp,
		(xdrproc_t) xdr_bool, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

/*
 * Receive response from YPPROC_DOMAIN_NONACK asynchronously.
 */
static bool_t *
ypproc_domain_nonack_2_recv(domainname *argp, CLIENT *clnt)
{
	static bool_t clnt_res;
	struct timeval TIMEOUT = { 0, 0 };

	memset((char *)&clnt_res, 0, sizeof (clnt_res));
	if (clnt_call(clnt, YPPROC_DOMAIN_NONACK,
		(xdrproc_t) NULL, (caddr_t) argp,
		(xdrproc_t) xdr_bool, (caddr_t) &clnt_res,
		TIMEOUT) != RPC_SUCCESS) {
		return (NULL);
	}
	return (&clnt_res);
}

/*
 * "We have the machine that goes 'ping!'" -- Monty Python
 *
 * This function blasts packets at the YPPROC_DOMAIN_NONACK procedures
 * of the NIS servers listed in restricted_addrs structure.
 * Whoever replies the fastest becomes our chosen server.
 *
 * Note: THIS IS NOT A BROADCAST OPERATION! We could use clnt_broadcast()
 * for this, but that has the following problems:
 * - We only get the address of the machine that replied in the
 *   'eachresult' callback, and on multi-homed machines this can
 *   lead to confusion.
 * - clnt_broadcast() only transmits to local networks, whereas with
 *   NIS+ you can have a perfectly good server located anywhere on or
 *   off the local network.
 * - clnt_broadcast() blocks for an arbitrary amount of time which the
 *   caller can't control -- we want to avoid that.
 *
 * Also note that this has nothing to do with the NIS_PING procedure used
 * for replica updates.
 */

struct ping_req {
	struct sockaddr_in	sin;
	unsigned long		xid;
};

int __yp_ping(restricted_addrs, cnt, dom, port)
	struct in_addr		*restricted_addrs;
	int			cnt;
	char			*dom;
	short			*port;
{
	struct timeval		tv = { 5, 0 };
	struct ping_req		**reqs;
	unsigned long		i;
	int			async;
	struct sockaddr_in	sin, *any = NULL;
	int			winner = -1;
	time_t			xid_seed, xid_lookup;
	int			sock, dontblock = 1;
	CLIENT			*clnt;
	char			*foo = dom;
	struct cu_data		*cu;
	int			validsrvs = 0;

	/* Set up handles. */
	reqs = calloc(1, sizeof(struct ping_req *) * cnt);
	xid_seed = time(NULL) ^ getpid();

	for (i = 0; i < cnt; i++) {
		bzero((char *)&sin, sizeof(sin));
		sin.sin_family = AF_INET;
		bcopy((char *)&restricted_addrs[i],
			(char *)&sin.sin_addr, sizeof(struct in_addr));
		sin.sin_port = htons(__pmap_getport(&sin, YPPROG,
					YPVERS, IPPROTO_UDP));
		if (sin.sin_port == 0)
			continue;
		reqs[i] = calloc(1, sizeof(struct ping_req));
		bcopy((char *)&sin, (char *)&reqs[i]->sin, sizeof(sin));
		any = &reqs[i]->sin;
		reqs[i]->xid = xid_seed;
		xid_seed++;
		validsrvs++;
	}

	/* Make sure at least one server was assigned */
	if (!validsrvs) {
		free(reqs);
		return(-1);
	}

	/* Create RPC handle */
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	clnt = clntudp_create(any, YPPROG, YPVERS, tv, &sock);
	if (clnt == NULL) {
		close(sock);
		for (i = 0; i < cnt; i++)
			if (reqs[i] != NULL)
				free(reqs[i]);
		free(reqs);
		return(-1);
	}
	clnt->cl_auth = authunix_create_default();
	cu = (struct cu_data *)clnt->cl_private;
	tv.tv_sec = 0;

	clnt_control(clnt, CLSET_TIMEOUT, (char *)&tv);
	async = TRUE;
	clnt_control(clnt, CLSET_ASYNC, (char *)&async);
	ioctl(sock, FIONBIO, &dontblock);

	/* Transmit */
	for (i = 0; i < cnt; i++) {
		if (reqs[i] != NULL) {
			clnt_control(clnt, CLSET_XID, (char *)&reqs[i]->xid);
			bcopy((char *)&reqs[i]->sin, (char *)&cu->cu_raddr,
				sizeof(struct sockaddr_in));
			ypproc_domain_nonack_2_send(&foo, clnt);
		}
	}

	/* Receive reply */
	ypproc_domain_nonack_2_recv(&foo, clnt);

	/* Got a winner -- look him up. */
	clnt_control(clnt, CLGET_XID, (char *)&xid_lookup);
	for (i = 0; i < cnt; i++) {
		if (reqs[i] != NULL && reqs[i]->xid == xid_lookup) {
			winner = i;
			*port = reqs[i]->sin.sin_port;
		}
	}

	/* Shut everything down */
	auth_destroy(clnt->cl_auth);
	clnt_destroy(clnt);
	close(sock);

	for (i = 0; i < cnt; i++)
		if (reqs[i] != NULL)
			free(reqs[i]);
	free(reqs);

	return(winner);
}
