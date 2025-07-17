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
static CLIENT *rpcb_clnt;

static void
local_rpcb(void *v __unused)
{
	rpcb_clnt = client_nl_create("rpcbind", RPCBPROG, RPCBVERS);
	KASSERT(rpcb_clnt, ("%s: netlink client already exist", __func__));
	clnt_control(rpcb_clnt, CLSET_RETRIES, &(int){6});
	clnt_control(rpcb_clnt, CLSET_WAITCHAN, "rpcb");
}
SYSINIT(rpcb_clnt, SI_SUB_VFS, SI_ORDER_SECOND, local_rpcb, NULL);

/*
 * Set a mapping between program, version and address.
 * Calls the rpcbind service to do the mapping.
 */
bool_t
rpcb_set(rpcprog_t program, rpcvers_t version,
    const struct netconfig *nconf,	/* Network structure of transport */
    const struct netbuf *address)	/* Services netconfig address */
{
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

	/* convert to universal */
	/*LINTED const castaway*/
	nconfcopy = *nconf;
	addresscopy = *address;
	parms.r_addr = taddr2uaddr(&nconfcopy, &addresscopy);
	if (!parms.r_addr) {
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

	CLNT_CALL(rpcb_clnt, (rpcproc_t)RPCBPROC_SET, (xdrproc_t) xdr_rpcb,
	    (char *)(void *)&parms, (xdrproc_t) xdr_bool,
	    (char *)(void *)&rslt, tottimeout);

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
	bool_t rslt = FALSE;
	RPCB parms;
#if 0
	char uidbuf[32];
#endif

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

	CLNT_CALL(rpcb_clnt, (rpcproc_t)RPCBPROC_UNSET, (xdrproc_t) xdr_rpcb,
	    (char *)(void *)&parms, (xdrproc_t) xdr_bool,
	    (char *)(void *)&rslt, tottimeout);

	return (rslt);
}
