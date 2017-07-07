/* @(#)clnt_simple.c	2.2 88/08/01 4.0 RPCSRC */
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
static char sccsid[] = "@(#)clnt_simple.c 1.35 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * clnt_simple.c
 * Simplified front end to rpc.
 */

#include "autoconf.h"
#include <stdio.h>
/* for close() */
#include <unistd.h>
#include <gssrpc/rpc.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <port-sockets.h>

static struct callrpc_private {
	CLIENT	*client;
        SOCKET  socket;
	rpcprog_t oldprognum;
	rpcvers_t oldversnum;
	int	valid;
	char	*oldhost;
} *callrpc_private;

int
callrpc(
	char *host,
	rpcprog_t prognum,
	rpcvers_t versnum,
	rpcproc_t procnum,
	xdrproc_t inproc,
	char *in,
	xdrproc_t outproc,
	char *out)
{
	register struct callrpc_private *crp = callrpc_private;
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct timeval timeout, tottimeout;

	if (crp == 0) {
		crp = (struct callrpc_private *)calloc(1, sizeof (*crp));
		if (crp == 0)
			return (0);
		callrpc_private = crp;
	}
	if (crp->oldhost == NULL) {
		crp->oldhost = mem_alloc(256);
		if (crp->oldhost == 0)
		    return 0;
		crp->oldhost[0] = 0;
		crp->socket = RPC_ANYSOCK;
	}
	if (crp->valid && crp->oldprognum == prognum && crp->oldversnum == versnum
		&& strcmp(crp->oldhost, host) == 0) {
		/* reuse old client */
	} else {
		crp->valid = 0;
                (void)closesocket(crp->socket);
		crp->socket = RPC_ANYSOCK;
		if (crp->client) {
			clnt_destroy(crp->client);
			crp->client = NULL;
		}
		if ((hp = gethostbyname(host)) == NULL)
			return ((int) RPC_UNKNOWNHOST);
		timeout.tv_usec = 0;
		timeout.tv_sec = 5;
		memset(&server_addr, 0, sizeof(server_addr));
		memmove((char *)&server_addr.sin_addr, hp->h_addr,
			sizeof(server_addr.sin_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port =  0;
		if ((crp->client = clntudp_create(&server_addr, prognum,
		    versnum, timeout, &crp->socket)) == NULL)
			return ((int) rpc_createerr.cf_stat);
		crp->valid = 1;
		crp->oldprognum = prognum;
		crp->oldversnum = versnum;
		(void) strncpy(crp->oldhost, host, 255);
		crp->oldhost[255] = '\0';
	}
	tottimeout.tv_sec = 25;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(crp->client, procnum, inproc, in,
	    outproc, out, tottimeout);
	/*
	 * if call failed, empty cache
	 */
	if (clnt_stat != RPC_SUCCESS)
		crp->valid = 0;
	return ((int) clnt_stat);
}
