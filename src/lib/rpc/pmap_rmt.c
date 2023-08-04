/* @(#)pmap_rmt.c	2.2 88/08/01 4.0 RPCSRC */
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
static char sccsid[] = "@(#)pmap_rmt.c 1.21 87/08/27 Copyr 1984 Sun Micro";
#endif

/*
 * pmap_rmt.c
 * Client interface to pmap rpc service.
 * remote call and broadcast service
 */

#include "k5-platform.h"
#include <unistd.h>
#include <gssrpc/rpc.h>
#include <gssrpc/pmap_prot.h>
#include <gssrpc/pmap_clnt.h>
#include <gssrpc/pmap_rmt.h>
#include <sys/socket.h>
#ifdef sun
#include <sys/sockio.h>
#endif
#ifdef OSF1
#include <net/route.h>
#include <sys/mbuf.h>
#endif
#include <net/if.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#define MAX_BROADCAST_SIZE 1400
#include <port-sockets.h>
#include "socket-utils.h"

static struct timeval timeout = { 3, 0 };

#ifndef GETSOCKNAME_ARG3_TYPE
#define GETSOCKNAME_ARG3_TYPE int
#endif

/*
 * pmapper remote-call-service interface.
 * This routine is used to call the pmapper remote call service
 * which will look up a service program in the port maps, and then
 * remotely call that routine with the given parameters.  This allows
 * programs to do a lookup and call in one step.
*/
enum clnt_stat
pmap_rmtcall(
	struct sockaddr_in *addr,
	rpcprog_t prog,
	rpcvers_t vers,
	rpcproc_t proc,
	xdrproc_t xdrargs,
	caddr_t argsp,
	xdrproc_t xdrres,
	caddr_t resp,
	struct timeval tout,
	rpcport_t *port_ptr)
{
        SOCKET sock = INVALID_SOCKET;
	CLIENT *client;
	struct rmtcallargs a;
	struct rmtcallres r;
	enum clnt_stat stat;

	addr->sin_port = htons(PMAPPORT);
	client = clntudp_create(addr, PMAPPROG, PMAPVERS, timeout, &sock);
	if (client != (CLIENT *)NULL) {
		a.prog = prog;
		a.vers = vers;
		a.proc = proc;
		a.args_ptr = argsp;
		a.xdr_args = xdrargs;
		r.port_ptr = port_ptr;
		r.results_ptr = resp;
		r.xdr_results = xdrres;
		stat = CLNT_CALL(client, PMAPPROC_CALLIT, xdr_rmtcall_args, &a,
		    xdr_rmtcallres, &r, tout);
		CLNT_DESTROY(client);
	} else {
		stat = RPC_FAILED;
	}
        (void)closesocket(sock);
	addr->sin_port = 0;
	return (stat);
}


/*
 * XDR remote call arguments
 * written for XDR_ENCODE direction only
 */
bool_t
xdr_rmtcall_args(
	XDR *xdrs,
	struct rmtcallargs *cap)
{
	u_int lenposition, argposition, position;

	if (xdr_u_int32(xdrs, &(cap->prog)) &&
	    xdr_u_int32(xdrs, &(cap->vers)) &&
	    xdr_u_int32(xdrs, &(cap->proc))) {
		lenposition = XDR_GETPOS(xdrs);
		if (! xdr_u_int32(xdrs, &(cap->arglen)))
		    return (FALSE);
		argposition = XDR_GETPOS(xdrs);
		if (! (*(cap->xdr_args))(xdrs, cap->args_ptr))
		    return (FALSE);
		position = XDR_GETPOS(xdrs);
		cap->arglen = (uint32_t)position - (uint32_t)argposition;
		XDR_SETPOS(xdrs, lenposition);
		if (! xdr_u_int32(xdrs, &(cap->arglen)))
		    return (FALSE);
		XDR_SETPOS(xdrs, position);
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR remote call results
 * written for XDR_DECODE direction only
 */
bool_t
xdr_rmtcallres(
	XDR *xdrs,
	struct rmtcallres *crp)
{
	caddr_t port_ptr;

	port_ptr = (caddr_t)(void *)crp->port_ptr;
	if (xdr_reference(xdrs, &port_ptr, sizeof (uint32_t),
	    xdr_u_int32) && xdr_u_int32(xdrs, &crp->resultslen)) {
		crp->port_ptr = (uint32_t *)(void *)port_ptr;
		return ((*(crp->xdr_results))(xdrs, crp->results_ptr));
	}
	return (FALSE);
}


/*
 * The following is kludged-up support for simple rpc broadcasts.
 * Someday a large, complicated system will replace these trivial
 * routines which only support udp/ip .
 */

#define GIFCONF_BUFSIZE (256 * sizeof (struct ifconf))

static int
getbroadcastnets(
	struct in_addr *addrs,
	int sock,  /* any valid socket will do */
	char *buf  /* why allocxate more when we can use existing... */
	)
{
	struct ifconf ifc;
        struct ifreq ifreq, *ifr;
        int n, i;

        ifc.ifc_len = GIFCONF_BUFSIZE;
        ifc.ifc_buf = buf;
	memset (buf, 0, GIFCONF_BUFSIZE);
        if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0) {
                perror("broadcast: ioctl (get interface configuration)");
                return (0);
        }
        ifr = ifc.ifc_req;
        for (i = 0, n = ifc.ifc_len/sizeof (struct ifreq); n > 0; n--, ifr++) {
                ifreq = *ifr;
                if (ioctl(sock, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
                        perror("broadcast: ioctl (get interface flags)");
                        continue;
                }
                if ((ifreq.ifr_flags & IFF_BROADCAST) &&
		    (ifreq.ifr_flags & IFF_UP) &&
		    ifr->ifr_addr.sa_family == AF_INET) {
#ifdef SIOCGIFBRDADDR   /* 4.3BSD */
			if (ioctl(sock, SIOCGIFBRDADDR, (char *)&ifreq) < 0) {
				addrs[i++].s_addr = INADDR_ANY;
			} else {
				addrs[i++] = sa2sin(&ifreq.ifr_addr)->sin_addr;
			}
#else /* 4.2 BSD */
			struct sockaddr_in *sockin;
			sockin = sa2sin(&ifr->ifr_addr);
			addrs[i++] = inet_makeaddr(inet_netof
			  (sockin->sin_addr.s_addr), INADDR_ANY);
#endif
		}
	}
	return (i);
}

enum clnt_stat
clnt_broadcast(
	rpcprog_t	prog,		/* program number */
	rpcvers_t	vers,		/* version number */
	rpcproc_t	proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	caddr_t		argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	caddr_t		resultsp,	/* pointer to results */
	resultproc_t	eachresult	/* call with each result obtained */
	)
{
	enum clnt_stat stat;
	AUTH *unix_auth = authunix_create_default();
	XDR xdr_stream;
	XDR *xdrs = &xdr_stream;
	int outlen, nets;
	ssize_t inlen;
	GETSOCKNAME_ARG3_TYPE fromlen;
        SOCKET sock;
	int on = 1;
#ifdef FD_SETSIZE
	fd_set mask;
	fd_set readfds;
#else
	int readfds;
	int mask;
#endif /* def FD_SETSIZE */
	int i;
	bool_t done = FALSE;
	uint32_t xid;
	rpcport_t port;
	struct in_addr addrs[20];
	struct sockaddr_in baddr, raddr; /* broadcast and response addresses */
	struct rmtcallargs a;
	struct rmtcallres r;
	struct rpc_msg msg;
	struct timeval t, t2;
	char outbuf[MAX_BROADCAST_SIZE];
#ifndef MAX
#define MAX(A,B) ((A)<(B)?(B):(A))
#endif
	char inbuf[MAX (UDPMSGSIZE, GIFCONF_BUFSIZE)];

	/*
	 * initialization: create a socket, a broadcast address, and
	 * preserialize the arguments into a send buffer.
	 */
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("Cannot create socket for broadcast rpc");
		stat = RPC_CANTSEND;
		goto done_broad;
	}
	set_cloexec_fd(sock);
#ifdef SO_BROADCAST
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *) &on,
		       sizeof (on)) < 0) {
		perror("Cannot set socket option SO_BROADCAST");
		stat = RPC_CANTSEND;
		goto done_broad;
	}
#endif /* def SO_BROADCAST */
#ifdef FD_SETSIZE
	FD_ZERO(&mask);
	FD_SET(sock, &mask);
#else
	mask = (1 << sock);
#endif /* def FD_SETSIZE */
	nets = getbroadcastnets(addrs, sock, inbuf);
	memset(&baddr, 0, sizeof (baddr));
	baddr.sin_family = AF_INET;
	baddr.sin_port = htons(PMAPPORT);
	baddr.sin_addr.s_addr = htonl(INADDR_ANY);
/*	baddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY); */
	(void)gettimeofday(&t, (struct timezone *)0);
	msg.rm_xid = xid = getpid() ^ t.tv_sec ^ t.tv_usec;
	t.tv_usec = 0;
	msg.rm_direction = CALL;
	msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	msg.rm_call.cb_prog = PMAPPROG;
	msg.rm_call.cb_vers = PMAPVERS;
	msg.rm_call.cb_proc = PMAPPROC_CALLIT;
	msg.rm_call.cb_cred = unix_auth->ah_cred;
	msg.rm_call.cb_verf = unix_auth->ah_verf;
	a.prog = prog;
	a.vers = vers;
	a.proc = proc;
	a.xdr_args = xargs;
	a.args_ptr = argsp;
	r.port_ptr = &port;
	r.xdr_results = xresults;
	r.results_ptr = resultsp;
	xdrmem_create(xdrs, outbuf, MAX_BROADCAST_SIZE, XDR_ENCODE);
	if ((! xdr_callmsg(xdrs, &msg)) || (! xdr_rmtcall_args(xdrs, &a))) {
		stat = RPC_CANTENCODEARGS;
		goto done_broad;
	}
	outlen = (int)xdr_getpos(xdrs);
	xdr_destroy(xdrs);
	/*
	 * Basic loop: broadcast a packet and wait a while for response(s).
	 * The response timeout grows larger per iteration.
	 */
	for (t.tv_sec = 4; t.tv_sec <= 14; t.tv_sec += 2) {
		for (i = 0; i < nets; i++) {
			baddr.sin_addr = addrs[i];
			if (sendto(sock, outbuf, outlen, 0,
				(struct sockaddr *)&baddr,
				sizeof (struct sockaddr)) != outlen) {
				perror("Cannot send broadcast packet");
				stat = RPC_CANTSEND;
				goto done_broad;
			}
		}
		if (eachresult == NULL) {
			stat = RPC_SUCCESS;
			goto done_broad;
		}
	recv_again:
		msg.acpted_rply.ar_verf = gssrpc__null_auth;
		msg.acpted_rply.ar_results.where = (caddr_t)&r;
                msg.acpted_rply.ar_results.proc = xdr_rmtcallres;
		readfds = mask;
		t2 = t;
		switch (select(gssrpc__rpc_dtablesize(), &readfds, (fd_set *)NULL,
			       (fd_set *)NULL, &t2)) {

		case 0:  /* timed out */
			stat = RPC_TIMEDOUT;
			continue;

		case -1:  /* some kind of error */
			if (errno == EINTR)
				goto recv_again;
			perror("Broadcast select problem");
			stat = RPC_CANTRECV;
			goto done_broad;

		}  /* end of select results switch */
	try_again:
		fromlen = sizeof(struct sockaddr);
		inlen = recvfrom(sock, inbuf, UDPMSGSIZE, 0,
			(struct sockaddr *)&raddr, &fromlen);
		if (inlen < 0) {
			if (errno == EINTR)
				goto try_again;
			perror("Cannot receive reply to broadcast");
			stat = RPC_CANTRECV;
			goto done_broad;
		}
		if ((size_t)inlen < sizeof(uint32_t))
			goto recv_again;
		/*
		 * see if reply transaction id matches sent id.
		 * If so, decode the results.
		 */
		xdrmem_create(xdrs, inbuf, (u_int)inlen, XDR_DECODE);
		if (xdr_replymsg(xdrs, &msg)) {
			if ((msg.rm_xid == xid) &&
				(msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
				(msg.acpted_rply.ar_stat == SUCCESS)) {
				raddr.sin_port = htons((u_short)port);
				done = (*eachresult)(resultsp, &raddr);
			}
			/* otherwise, we just ignore the errors ... */
		} else {
#ifdef notdef
			/* some kind of deserialization problem ... */
			if (msg.rm_xid == xid)
				fprintf(stderr, "Broadcast deserialization problem");
			/* otherwise, just random garbage */
#endif
		}
		xdrs->x_op = XDR_FREE;
		msg.acpted_rply.ar_results.proc = xdr_void;
		(void)xdr_replymsg(xdrs, &msg);
		(void)(*xresults)(xdrs, resultsp);
		xdr_destroy(xdrs);
		if (done) {
			stat = RPC_SUCCESS;
			goto done_broad;
		} else {
			goto recv_again;
		}
	}
done_broad:
        (void)closesocket(sock);
	AUTH_DESTROY(unix_auth);
	return (stat);
}
