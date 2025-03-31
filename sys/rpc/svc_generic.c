/*	$NetBSD: svc_generic.c,v 1.3 2000/07/06 03:10:35 christos Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2009, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * - Neither the name of Sun Microsystems, Inc. nor the names of its 
 *   contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

/*
 * svc_generic.c, Server side for RPC.
 *
 */

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/sx.h>
#include <sys/ucred.h>

#include <net/vnet.h>

#include <rpc/rpc.h>
#include <rpc/rpcb_clnt.h>
#include <rpc/nettype.h>

#include <rpc/rpc_com.h>

extern int __svc_vc_setflag(SVCXPRT *, int);

/*
 * The high level interface to svc_tli_create().
 * It tries to create a server for "nconf" and registers the service
 * with the rpcbind. It calls svc_tli_create();
 */
SVCXPRT *
svc_tp_create(
	SVCPOOL *pool,
	void (*dispatch)(struct svc_req *, SVCXPRT *),
	rpcprog_t prognum,		/* Program number */
	rpcvers_t versnum,		/* Version number */
	const char *uaddr,		/* Address (or null for default) */
	const struct netconfig *nconf) /* Netconfig structure for the network */
{
	struct netconfig nconfcopy;
	struct netbuf *taddr;
	struct t_bind bind;
	SVCXPRT *xprt;

	if (nconf == NULL) {
		printf(
	"svc_tp_create: invalid netconfig structure for prog %u vers %u\n",
				(unsigned)prognum, (unsigned)versnum);
		return (NULL);
	}
	if (uaddr) {
		taddr = uaddr2taddr(nconf, uaddr);
		bind.addr = *taddr;
		free(taddr, M_RPC);
		bind.qlen = -1;
		xprt = svc_tli_create(pool, nconf, &bind, 0, 0);
		free(bind.addr.buf, M_RPC);
	} else {
		xprt = svc_tli_create(pool, nconf, NULL, 0, 0);
	}
	if (xprt == NULL) {
		return (NULL);
	}
	/*LINTED const castaway*/
	nconfcopy = *nconf;
	(void) rpcb_unset(prognum, versnum, &nconfcopy);
	if (svc_reg(xprt, prognum, versnum, dispatch, nconf) == FALSE) {
		printf(
		"svc_tp_create: Could not register prog %u vers %u on %s\n",
				(unsigned)prognum, (unsigned)versnum,
				nconf->nc_netid);
		xprt_unregister(xprt);
		SVC_RELEASE(xprt);
		return (NULL);
	}
	return (xprt);
}

/*
 * If so is NULL, then it opens a socket for the given transport
 * provider (nconf cannot be NULL then). If the t_state is T_UNBND and
 * bindaddr is NON-NULL, it performs a t_bind using the bindaddr. For
 * NULL bindadr and Connection oriented transports, the value of qlen
 * is set to 8.
 *
 * If sendsz or recvsz are zero, their default values are chosen.
 */
SVCXPRT *
svc_tli_create(
	SVCPOOL *pool,
	const struct netconfig *nconf,	/* Netconfig struct for nettoken */
	const struct t_bind *bindaddr,	/* Local bind address */
	size_t sendsz,			/* Max sendsize */
	size_t recvsz)			/* Max recvsize */
{
	struct socket *so;
	SVCXPRT *xprt = NULL;		/* service handle */
	struct __rpc_sockinfo si;
	struct sockaddr_storage ss;

	if (nconf == NULL) {
		printf("svc_tli_create: invalid netconfig\n");
		return (NULL);
	}
	so = __rpc_nconf2socket(nconf);
	if (!so) {
		printf(
		    "svc_tli_create: could not open connection for %s\n",
				nconf->nc_netid);
		return (NULL);
	}
	__rpc_nconf2sockinfo(nconf, &si);

	if (bindaddr == NULL) {
		if (bindresvport(so, NULL)) {
			memset(&ss, 0, sizeof ss);
			ss.ss_family = si.si_af;
			ss.ss_len = si.si_alen;
			if (sobind(so, (struct sockaddr *)&ss,
				curthread)) {
				printf(
		"svc_tli_create: could not bind to anonymous port\n");
				goto freedata;
			}
		}
		solisten(so, -1, curthread);
	} else {
		if (bindresvport(so,
			(struct sockaddr *)bindaddr->addr.buf)) {
			printf(
	"svc_tli_create: could not bind to requested address\n");
			goto freedata;
		}
		solisten(so, (int)bindaddr->qlen, curthread);
	}

	/*
	 * call transport specific function.
	 */
	switch (si.si_socktype) {
		case SOCK_STREAM:
#if 0
			slen = sizeof ss;
			if (_getpeername(fd, (struct sockaddr *)(void *)&ss, &slen)
			    == 0) {
				/* accepted socket */
				xprt = svc_fd_create(fd, sendsz, recvsz);
			} else
#endif
				xprt = svc_vc_create(pool, so, sendsz, recvsz);
			if (!nconf || !xprt)
				break;
#if 0
			/* XXX fvdl */
			if (strcmp(nconf->nc_protofmly, "inet") == 0 ||
			    strcmp(nconf->nc_protofmly, "inet6") == 0)
				(void) __svc_vc_setflag(xprt, TRUE);
#endif
			break;
		case SOCK_DGRAM:
			xprt = svc_dg_create(pool, so, sendsz, recvsz);
			break;
		default:
			printf("svc_tli_create: bad service type");
			goto freedata;
	}

	if (xprt == NULL)
		/*
		 * The error messages here are spitted out by the lower layers:
		 * svc_vc_create(), svc_fd_create() and svc_dg_create().
		 */
		goto freedata;

	/* Fill in type of service */
	xprt->xp_type = __rpc_socktype2seman(si.si_socktype);

	if (nconf) {
		xprt->xp_netid = strdup(nconf->nc_netid, M_RPC);
	}
	return (xprt);

freedata:
	(void)soclose(so);
	if (xprt)
		xprt_unregister(xprt);
	return (NULL);
}
