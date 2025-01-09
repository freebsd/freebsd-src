/*	$NetBSD: rpcb_clnt.c,v 1.6 2000/07/16 06:41:43 itojun Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010, Oracle America, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the "Oracle America, Inc." nor the names of its
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

#include <sys/cdefs.h>
/*
 * rpcb_clnt.c
 * interface to rpcbind rpc service.
 *
 * Copyright (C) 1988, Sun Microsystems, Inc.
 */

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <rpc/rpc.h>
#include <rpc/rpcb_clnt.h>
#include <rpc/rpcb_prot.h>

#include <rpc/rpc_com.h>

static struct timeval tottimeout = { 60, 0 };
static const char nullstring[] = "\000";

static CLIENT *local_rpcb(void);

/* XXX */
#define IN4_LOCALHOST_STRING	"127.0.0.1"
#define IN6_LOCALHOST_STRING	"::1"

/*
 * This routine will return a client handle that is connected to the local
 * rpcbind. Returns NULL on error and free's everything.
 */
static CLIENT *
local_rpcb(void)
{
	CLIENT *client;
	struct socket *so;
	size_t tsize;
	struct sockaddr_un sun;
	int error;

	/*
	 * Try connecting to the local rpcbind through a local socket
	 * first. If this doesn't work, try all transports defined in
	 * the netconfig file.
	 */
	memset(&sun, 0, sizeof sun);
	so = NULL;
	error = socreate(AF_LOCAL, &so, SOCK_STREAM, 0, curthread->td_ucred,
	    curthread);
	if (error)
		return (NULL);
	sun.sun_family = AF_LOCAL;
	strcpy(sun.sun_path, _PATH_RPCBINDSOCK);
	sun.sun_len = SUN_LEN(&sun);

	tsize = __rpc_get_t_size(AF_LOCAL, 0, 0);
	client = clnt_vc_create(so, (struct sockaddr *)&sun, (rpcprog_t)RPCBPROG,
	    (rpcvers_t)RPCBVERS, tsize, tsize, 1);

	if (client != NULL) {
		/* Mark the socket to be closed in destructor */
		(void) CLNT_CONTROL(client, CLSET_FD_CLOSE, NULL);
		return client;
	}

	/* Nobody needs this socket anymore; free the descriptor. */
	soclose(so);

	return (NULL);
}

/*
 * Set a mapping between program, version and address.
 * Calls the rpcbind service to do the mapping.
 */
bool_t
rpcb_set(rpcprog_t program, rpcvers_t version,
    const struct netconfig *nconf,	/* Network structure of transport */
    const struct netbuf *address)	/* Services netconfig address */
{
	CLIENT *client;
	bool_t rslt = FALSE;
	RPCB parms;
#if 0
	char uidbuf[32];
#endif
	struct netconfig nconfcopy;
	struct netbuf addresscopy;

	/* parameter checking */
	if (nconf == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		return (FALSE);
	}
	if (address == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (FALSE);
	}
	client = local_rpcb();
	if (! client) {
		return (FALSE);
	}

	/* convert to universal */
	/*LINTED const castaway*/
	nconfcopy = *nconf;
	addresscopy = *address;
	parms.r_addr = taddr2uaddr(&nconfcopy, &addresscopy);
	if (!parms.r_addr) {
		CLNT_DESTROY(client);
		rpc_createerr.cf_stat = RPC_N2AXLATEFAILURE;
		return (FALSE); /* no universal address */
	}
	parms.r_prog = program;
	parms.r_vers = version;
	parms.r_netid = nconf->nc_netid;
#if 0
	/*
	 * Though uid is not being used directly, we still send it for
	 * completeness.  For non-unix platforms, perhaps some other
	 * string or an empty string can be sent.
	 */
	(void) snprintf(uidbuf, sizeof uidbuf, "%d", geteuid());
	parms.r_owner = uidbuf;
#else
	parms.r_owner = "";
#endif

	CLNT_CALL(client, (rpcproc_t)RPCBPROC_SET, (xdrproc_t) xdr_rpcb,
	    (char *)(void *)&parms, (xdrproc_t) xdr_bool,
	    (char *)(void *)&rslt, tottimeout);

	CLNT_DESTROY(client);
	free(parms.r_addr, M_RPC);
	return (rslt);
}

/*
 * Remove the mapping between program, version and netbuf address.
 * Calls the rpcbind service to do the un-mapping.
 * If netbuf is NULL, unset for all the transports, otherwise unset
 * only for the given transport.
 */
bool_t
rpcb_unset(rpcprog_t program, rpcvers_t version, const struct netconfig *nconf)
{
	CLIENT *client;
	bool_t rslt = FALSE;
	RPCB parms;
#if 0
	char uidbuf[32];
#endif

	client = local_rpcb();
	if (! client) {
		return (FALSE);
	}

	parms.r_prog = program;
	parms.r_vers = version;
	if (nconf)
		parms.r_netid = nconf->nc_netid;
	else {
		/*LINTED const castaway*/
		parms.r_netid = (char *)(uintptr_t) &nullstring[0]; /* unsets  all */
	}
	/*LINTED const castaway*/
	parms.r_addr = (char *)(uintptr_t) &nullstring[0];
#if 0
	(void) snprintf(uidbuf, sizeof uidbuf, "%d", geteuid());
	parms.r_owner = uidbuf;
#else
	parms.r_owner = "";
#endif

	CLNT_CALL(client, (rpcproc_t)RPCBPROC_UNSET, (xdrproc_t) xdr_rpcb,
	    (char *)(void *)&parms, (xdrproc_t) xdr_bool,
	    (char *)(void *)&rslt, tottimeout);

	CLNT_DESTROY(client);
	return (rslt);
}
