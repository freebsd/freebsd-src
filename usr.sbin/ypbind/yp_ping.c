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
 *
 *	$Id: yp_ping.c,v 1.1 1997/05/25 19:49:29 wpaul Exp $
 */

/*
 * What follows is a special version of clntudp_call() that has been
 * hacked to send requests and receive replies asynchronously. Similar
 * magic is used inside rpc.nisd(8) for the special non-blocking,
 * non-fork()ing, non-threading callback support.
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
/*static char *sccsid = "from: @(#)clnt_udp.c 1.39 87/08/11 Copyr 1984 Sun Micro";*/
/*static char *sccsid = "from: @(#)clnt_udp.c	2.2 88/08/01 4.0 RPCSRC";*/
static const char rcsid[] = "@(#) $Id: yp_ping.c,v 1.1 1997/05/25 19:49:29 wpaul Exp $";
#endif

/*
 * clnt_udp.c, Implements a UDP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpcsvc/yp.h>
#include "yp_ping.h"

#ifndef timeradd
#ifndef KERNEL		/* use timevaladd/timevalsub in kernel */
/* NetBSD/OpenBSD compatable interfaces */
#define timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#define timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif
#endif

/*
 * Private data kept per client handle
 */
struct cu_data {
	int		   cu_sock;
	bool_t		   cu_closeit;
	struct sockaddr_in cu_raddr;
	int		   cu_rlen;
	struct timeval	   cu_wait;
	struct timeval     cu_total;
	struct rpc_err	   cu_error;
	XDR		   cu_outxdrs;
	u_int		   cu_xdrpos;
	u_int		   cu_sendsz;
	char		   *cu_outbuf;
	u_int		   cu_recvsz;
	char		   cu_inbuf[1];
};

static enum clnt_stat
clntudp_a_call(cl, proc, xargs, argsp, xresults, resultsp, utimeout)
	register CLIENT	*cl;		/* client handle */
	u_long		proc;		/* procedure number */
	xdrproc_t	xargs;		/* xdr routine for args */
	caddr_t		argsp;		/* pointer to args */
	xdrproc_t	xresults;	/* xdr routine for results */
	caddr_t		resultsp;	/* pointer to results */
	struct timeval	utimeout;	/* seconds to wait before giving up */
{
	register struct cu_data *cu = (struct cu_data *)cl->cl_private;
	register XDR *xdrs;
	register int outlen = 0;
	register int inlen;
	int fromlen;
	fd_set *fds, readfds;
	struct sockaddr_in from;
	struct rpc_msg reply_msg;
	XDR reply_xdrs;
	struct timeval time_waited, start, after, tmp1, tmp2, tv;
	bool_t ok;
	int nrefreshes = 2;	/* number of times to refresh cred */
	struct timeval timeout;

	if (cu->cu_total.tv_usec == -1)
		timeout = utimeout;     /* use supplied timeout */
	else
		timeout = cu->cu_total; /* use default timeout */

	if (cu->cu_sock + 1 > FD_SETSIZE) {
		int bytes = howmany(cu->cu_sock + 1, NFDBITS) * sizeof(fd_mask);
		fds = (fd_set *)malloc(bytes);
		if (fds == NULL)
			return (cu->cu_error.re_status = RPC_CANTSEND);
		memset(fds, 0, bytes);
	} else {
		fds = &readfds;
		FD_ZERO(fds);
	}

	timerclear(&time_waited);

call_again:
	xdrs = &(cu->cu_outxdrs);
	if (xargs == NULL)
		goto get_reply;
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, cu->cu_xdrpos);
	/*
	 * the transaction is the first thing in the out buffer
	 */
	(*(u_short *)(cu->cu_outbuf))++;
	if ((! XDR_PUTLONG(xdrs, (long *)&proc)) ||
	    (! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
	    (! (*xargs)(xdrs, argsp))) {
		if (fds != &readfds)
			free(fds);
		return (cu->cu_error.re_status = RPC_CANTENCODEARGS);
	}
	outlen = (int)XDR_GETPOS(xdrs);

send_again:
	if (sendto(cu->cu_sock, cu->cu_outbuf, outlen, 0,
	    (struct sockaddr *)&(cu->cu_raddr), cu->cu_rlen) != outlen) {
		cu->cu_error.re_errno = errno;
		if (fds != &readfds)
			free(fds);
		return (cu->cu_error.re_status = RPC_CANTSEND);
	}

	/*
	 * Hack to provide rpc-based message passing
	 */
	if (!timerisset(&timeout)) {
		if (fds != &readfds)
			free(fds);
		return (cu->cu_error.re_status = RPC_TIMEDOUT);
	}

get_reply:

	/*
	 * sub-optimal code appears here because we have
	 * some clock time to spare while the packets are in flight.
	 * (We assume that this is actually only executed once.)
	 */
	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = resultsp;
	reply_msg.acpted_rply.ar_results.proc = xresults;

	gettimeofday(&start, NULL);
	for (;;) {
		/* XXX we know the other bits are still clear */
		FD_SET(cu->cu_sock, fds);
		tv = cu->cu_wait;
		switch (select(cu->cu_sock+1, fds, NULL, NULL, &tv)) {

		case 0:
			timeradd(&time_waited, &cu->cu_wait, &tmp1);
			time_waited = tmp1;
			if (timercmp(&time_waited, &timeout, <))
				goto send_again;
			if (fds != &readfds)
				free(fds);
			return (cu->cu_error.re_status = RPC_TIMEDOUT);

		case -1:
			if (errno == EINTR) {
				gettimeofday(&after, NULL);
				timersub(&after, &start, &tmp1);
				timeradd(&time_waited, &tmp1, &tmp2);
				time_waited = tmp2;
				if (timercmp(&time_waited, &timeout, <))
					continue;
				if (fds != &readfds)
					free(fds);
				return (cu->cu_error.re_status = RPC_TIMEDOUT);
			}
			cu->cu_error.re_errno = errno;
			if (fds != &readfds)
				free(fds);
			return (cu->cu_error.re_status = RPC_CANTRECV);
		}

		do {
			fromlen = sizeof(struct sockaddr);
			inlen = recvfrom(cu->cu_sock, cu->cu_inbuf,
				(int) cu->cu_recvsz, 0,
				(struct sockaddr *)&from, &fromlen);
		} while (inlen < 0 && errno == EINTR);
		if (inlen < 0) {
			if (errno == EWOULDBLOCK)
				continue;
			cu->cu_error.re_errno = errno;
			if (fds != &readfds)
				free(fds);
			return (cu->cu_error.re_status = RPC_CANTRECV);
		}
		if (inlen < sizeof(u_int32_t))
			continue;
#ifdef dont_check_xid
		/* see if reply transaction id matches sent id */
		if (*((u_int32_t *)(cu->cu_inbuf)) != *((u_int32_t *)(cu->cu_outbuf)))
			continue;
#endif
		/* we now assume we have the proper reply */
		break;
	}

	/*
	 * now decode and validate the response
	 */
	xdrmem_create(&reply_xdrs, cu->cu_inbuf, (u_int)inlen, XDR_DECODE);
	ok = xdr_replymsg(&reply_xdrs, &reply_msg);
	/* XDR_DESTROY(&reply_xdrs);  save a few cycles on noop destroy */
	if (ok) {
		_seterr_reply(&reply_msg, &(cu->cu_error));
		if (cu->cu_error.re_status == RPC_SUCCESS) {
			if (! AUTH_VALIDATE(cl->cl_auth,
				&reply_msg.acpted_rply.ar_verf)) {
				cu->cu_error.re_status = RPC_AUTHERROR;
				cu->cu_error.re_why = AUTH_INVALIDRESP;
			}
			if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
				xdrs->x_op = XDR_FREE;
				(void)xdr_opaque_auth(xdrs,
				    &(reply_msg.acpted_rply.ar_verf));
			}
		}  /* end successful completion */
		else {
			/* maybe our credentials need to be refreshed ... */
			if (nrefreshes > 0 && AUTH_REFRESH(cl->cl_auth)) {
				nrefreshes--;
				goto call_again;
			}
		}  /* end of unsuccessful completion */
	}  /* end of valid reply message */
	else {
		cu->cu_error.re_status = RPC_CANTDECODERES;
	}
	if (fds != &readfds)
		free(fds);
	return (cu->cu_error.re_status);
}


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
	struct timeval		tv = { 5 , 0 };
	struct ping_req		**reqs;
	unsigned long		i;
	struct sockaddr_in	sin, *any;
	int			winner = -1;
	time_t			xid_seed, xid_lookup;
	int			sock, dontblock = 1;
	CLIENT			*clnt;
	char			*foo = dom;
	struct cu_data		*cu;
	enum clnt_stat		(*oldfunc)();
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
	clnt_control(clnt, CLSET_TIMEOUT, &tv);
	ioctl(sock, FIONBIO, &dontblock);
	oldfunc = clnt->cl_ops->cl_call;
	clnt->cl_ops->cl_call = clntudp_a_call;

	/* Transmit */
	for (i = 0; i < cnt; i++) {
		if (reqs[i] != NULL) {
			/* subtract one; clntudp_call() will increment */
			*((u_int32_t *)(cu->cu_outbuf)) = reqs[i]->xid - 1;
			bcopy((char *)&reqs[i]->sin, (char *)&cu->cu_raddr,
				sizeof(struct sockaddr_in));
			ypproc_domain_nonack_2_send(&foo, clnt);
		}
	}

	/* Receive reply */
	ypproc_domain_nonack_2_recv(&foo, clnt);

	/* Got a winner -- look him up. */
	xid_lookup = *((u_int32_t *)(cu->cu_inbuf));
	for (i = 0; i < cnt; i++) {
		if (reqs[i] != NULL && reqs[i]->xid == xid_lookup) {
			winner = i;
			*port = reqs[i]->sin.sin_port;
		}
	}

	/* Shut everything down */
	clnt->cl_ops->cl_call = oldfunc;
	auth_destroy(clnt->cl_auth);
	clnt_destroy(clnt);
	close(sock);

	for (i = 0; i < cnt; i++)
		if (reqs[i] != NULL)
			free(reqs[i]);
	free(reqs);

	return(winner);
}
