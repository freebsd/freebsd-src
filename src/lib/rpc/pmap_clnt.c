/* @(#)pmap_clnt.c	2.2 88/08/01 4.0 RPCSRC */
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
static char sccsid[] = "@(#)pmap_clnt.c 1.37 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * pmap_clnt.c
 * Client interface to pmap rpc service.
 */

#include <unistd.h>
#include <gssrpc/rpc.h>
#include <gssrpc/pmap_prot.h>
#include <gssrpc/pmap_clnt.h>

#if TARGET_OS_MAC
#include <sys/un.h>
#include <string.h>
#include <syslog.h>
#endif

static struct timeval timeout = { 5, 0 };
static struct timeval tottimeout = { 60, 0 };

void clnt_perror();

/*
 * Set a mapping between program,version and port.
 * Calls the pmap service remotely to do the mapping.
 */
bool_t
pmap_set(
	rpcprog_t program,
	rpcvers_t version,
	rpcprot_t protocol,
	u_int port)
{
	struct sockaddr_in myaddress;
	int sock = -1;
	CLIENT *client;
	struct pmap parms;
	bool_t rslt;

	get_myaddress(&myaddress);
	client = clntudp_bufcreate(&myaddress, PMAPPROG, PMAPVERS,
	    timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client == (CLIENT *)NULL)
		return (FALSE);
	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_prot = protocol;
	parms.pm_port = port;
#if TARGET_OS_MAC
	{
	    /*
	     * Poke launchd, then wait for portmap to start up.
	     *
	     * My impression is that the protocol involves getting
	     * something back from the server.  So wait, briefly, to
	     * see if it's going to send us something.  Then continue
	     * on, regardless.  I don't actually check what the data
	     * is, because I have no idea what sort of validation, if
	     * any, is needed.
	     *
	     * However, for whatever reason, the socket seems to be
	     * mode 700 owner root on my system, so if you don't
	     * change its ownership or mode, and if the program isn't
	     * running as root, you still lose.
	     */
#define TICKLER_SOCKET "/var/run/portmap.socket"
	    int tickle;
	    struct sockaddr_un a = {
		.sun_family = AF_UNIX,
		.sun_path = TICKLER_SOCKET,
		.sun_len = (sizeof(TICKLER_SOCKET)
			    + offsetof(struct sockaddr_un, sun_path)),
	    };

	    if (sizeof(TICKLER_SOCKET) <= sizeof(a.sun_path)) {
		tickle = socket(AF_UNIX, SOCK_STREAM, 0);
		if (tickle >= 0) {
		    if (connect(tickle, (struct sockaddr *)&a, a.sun_len) == 0
			&& tickle < FD_SETSIZE) {
			fd_set readfds;
			struct timeval tv;

			FD_ZERO(&readfds);
			/* XXX Range check.  */
			FD_SET(tickle, &readfds);
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			(void) select(tickle+1, &readfds, 0, 0, &tv);
		    }
		    close(tickle);
		}
	    }
	}
#endif
	if (CLNT_CALL(client, PMAPPROC_SET, xdr_pmap, &parms, xdr_bool, &rslt,
	    tottimeout) != RPC_SUCCESS) {
		clnt_perror(client, "Cannot register service");
		return (FALSE);
	}
	CLNT_DESTROY(client);
	(void)close(sock);
	return (rslt);
}

/*
 * Remove the mapping between program,version and port.
 * Calls the pmap service remotely to do the un-mapping.
 */
bool_t
pmap_unset(
	rpcprog_t program,
	rpcvers_t version)
{
	struct sockaddr_in myaddress;
	int sock = -1;
	CLIENT *client;
	struct pmap parms;
	bool_t rslt;

	get_myaddress(&myaddress);
	client = clntudp_bufcreate(&myaddress, PMAPPROG, PMAPVERS,
	    timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client == (CLIENT *)NULL)
		return (FALSE);
	parms.pm_prog = program;
	parms.pm_vers = version;
	parms.pm_port = parms.pm_prot = 0;
	CLNT_CALL(client, PMAPPROC_UNSET, xdr_pmap, &parms, xdr_bool, &rslt,
	    tottimeout);
	CLNT_DESTROY(client);
	(void)close(sock);
	return (rslt);
}
