/*	$NetBSD: rpc_soc.c,v 1.6 2000/07/06 03:10:35 christos Exp $	*/
/*	$FreeBSD$ */

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

/* #ident	"@(#)rpc_soc.c	1.17	94/04/24 SMI" */

/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 */

#if 0
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)rpc_soc.c 1.41 89/05/02 Copyr 1988 Sun Micro";
#endif
#endif

#ifdef PORTMAP
/*
 * rpc_soc.c
 *
 * The backward compatibility routines for the earlier implementation
 * of RPC, where the only transports supported were tcp/ip and udp/ip.
 * Based on berkeley socket abstraction, now implemented on the top
 * of TLI/Streams
 */

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpc/nettype.h>
#include <syslog.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "un-namespace.h"

#include "rpc_com.h"

extern mutex_t	rpcsoc_lock;

static CLIENT *clnt_com_create __P((struct sockaddr_in *, rpcprog_t, rpcvers_t,
				    int *, u_int, u_int, char *));
static SVCXPRT *svc_com_create __P((int, u_int, u_int, char *));
static bool_t rpc_wrap_bcast __P((char *, struct netbuf *, struct netconfig *));

/* XXX */
#define IN4_LOCALHOST_STRING    "127.0.0.1"
#define IN6_LOCALHOST_STRING    "::1"

/*
 * A common clnt create routine
 */
static CLIENT *
clnt_com_create(raddr, prog, vers, sockp, sendsz, recvsz, tp)
	struct sockaddr_in *raddr;
	rpcprog_t prog;
	rpcvers_t vers;
	int *sockp;
	u_int sendsz;
	u_int recvsz;
	char *tp;
{
	CLIENT *cl;
	int madefd = FALSE;
	int fd = *sockp;
	struct netconfig *nconf;
	struct netbuf bindaddr;

	mutex_lock(&rpcsoc_lock);
	if ((nconf = __rpc_getconfip(tp)) == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNPROTO;
		mutex_unlock(&rpcsoc_lock);
		return (NULL);
	}
	if (fd == RPC_ANYSOCK) {
		fd = __rpc_nconf2fd(nconf);
		if (fd == -1)
			goto syserror;
		madefd = TRUE;
	}

	if (raddr->sin_port == 0) {
		u_int proto;
		u_short sport;

		mutex_unlock(&rpcsoc_lock);	/* pmap_getport is recursive */
		proto = strcmp(tp, "udp") == 0 ? IPPROTO_UDP : IPPROTO_TCP;
		sport = pmap_getport(raddr, (u_long)prog, (u_long)vers,
		    proto);
		if (sport == 0) {
			goto err;
		}
		raddr->sin_port = htons(sport);
		mutex_lock(&rpcsoc_lock);	/* pmap_getport is recursive */
	}

	/* Transform sockaddr_in to netbuf */
	bindaddr.maxlen = bindaddr.len =  sizeof (struct sockaddr_in);
	bindaddr.buf = raddr;

	bindresvport(fd, NULL);
	cl = clnt_tli_create(fd, nconf, &bindaddr, prog, vers,
				sendsz, recvsz);
	if (cl) {
		if (madefd == TRUE) {
			/*
			 * The fd should be closed while destroying the handle.
			 */
			(void) CLNT_CONTROL(cl, CLSET_FD_CLOSE, NULL);
			*sockp = fd;
		}
		(void) freenetconfigent(nconf);
		mutex_unlock(&rpcsoc_lock);
		return (cl);
	}
	goto err;

syserror:
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;

err:	if (madefd == TRUE)
		(void)_close(fd);
	(void) freenetconfigent(nconf);
	mutex_unlock(&rpcsoc_lock);
	return (NULL);
}

CLIENT *
clntudp_bufcreate(raddr, prog, vers, wait, sockp, sendsz, recvsz)
	struct sockaddr_in *raddr;
	u_long prog;
	u_long vers;
	struct timeval wait;
	int *sockp;
	u_int sendsz;
	u_int recvsz;
{
	CLIENT *cl;

	cl = clnt_com_create(raddr, (rpcprog_t)prog, (rpcvers_t)vers, sockp,
	    sendsz, recvsz, "udp");
	if (cl == NULL) {
		return (NULL);
	}
	(void) CLNT_CONTROL(cl, CLSET_RETRY_TIMEOUT, (char *)(void *)&wait);
	return (cl);
}

CLIENT *
clntudp_create(raddr, program, version, wait, sockp)
	struct sockaddr_in *raddr;
	u_long program;
	u_long version;
	struct timeval wait;
	int *sockp;
{

	return clntudp_bufcreate(raddr, program, version, wait, sockp,
					UDPMSGSIZE, UDPMSGSIZE);
}

CLIENT *
clnttcp_create(raddr, prog, vers, sockp, sendsz, recvsz)
	struct sockaddr_in *raddr;
	u_long prog;
	u_long vers;
	int *sockp;
	u_int sendsz;
	u_int recvsz;
{

	return clnt_com_create(raddr, (rpcprog_t)prog, (rpcvers_t)vers, sockp,
	    sendsz, recvsz, "tcp");
}

CLIENT *
clntraw_create(prog, vers)
	u_long prog;
	u_long vers;
{

	return clnt_raw_create((rpcprog_t)prog, (rpcvers_t)vers);
}

/*
 * A common server create routine
 */
static SVCXPRT *
svc_com_create(fd, sendsize, recvsize, netid)
	int fd;
	u_int sendsize;
	u_int recvsize;
	char *netid;
{
	struct netconfig *nconf;
	SVCXPRT *svc;
	int madefd = FALSE;
	int port;
	struct sockaddr_in sin;

	if ((nconf = __rpc_getconfip(netid)) == NULL) {
		(void) syslog(LOG_ERR, "Could not get %s transport", netid);
		return (NULL);
	}
	if (fd == RPC_ANYSOCK) {
		fd = __rpc_nconf2fd(nconf);
		if (fd == -1) {
			(void) freenetconfigent(nconf);
			(void) syslog(LOG_ERR,
			"svc%s_create: could not open connection", netid);
			return (NULL);
		}
		madefd = TRUE;
	}

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	bindresvport(fd, &sin);
	_listen(fd, SOMAXCONN);
	svc = svc_tli_create(fd, nconf, NULL, sendsize, recvsize);
	(void) freenetconfigent(nconf);
	if (svc == NULL) {
		if (madefd)
			(void)_close(fd);
		return (NULL);
	}
	port = (((struct sockaddr_in *)svc->xp_ltaddr.buf)->sin_port);
	svc->xp_port = ntohs(port);
	return (svc);
}

SVCXPRT *
svctcp_create(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{

	return svc_com_create(fd, sendsize, recvsize, "tcp");
}

SVCXPRT *
svcudp_bufcreate(fd, sendsz, recvsz)
	int fd;
	u_int sendsz, recvsz;
{

	return svc_com_create(fd, sendsz, recvsz, "udp");
}

SVCXPRT *
svcfd_create(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{

	return svc_fd_create(fd, sendsize, recvsize);
}


SVCXPRT *
svcudp_create(fd)
	int fd;
{

	return svc_com_create(fd, UDPMSGSIZE, UDPMSGSIZE, "udp");
}

SVCXPRT *
svcraw_create()
{

	return svc_raw_create();
}

int
get_myaddress(addr)
	struct sockaddr_in *addr;
{

	memset((void *) addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(PMAPPORT);
	addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	return (0);
}

/*
 * For connectionless "udp" transport. Obsoleted by rpc_call().
 */
int
callrpc(host, prognum, versnum, procnum, inproc, in, outproc, out)
	char *host;
	int prognum, versnum, procnum;
	xdrproc_t inproc, outproc;
	void *in, *out;
{

	return (int)rpc_call(host, (rpcprog_t)prognum, (rpcvers_t)versnum,
	    (rpcproc_t)procnum, inproc, in, outproc, out, "udp");
}

/*
 * For connectionless kind of transport. Obsoleted by rpc_reg()
 */
int
registerrpc(prognum, versnum, procnum, progname, inproc, outproc)
	int prognum, versnum, procnum;
	char *(*progname) __P((char [UDPMSGSIZE]));
	xdrproc_t inproc, outproc;
{

	return rpc_reg((rpcprog_t)prognum, (rpcvers_t)versnum,
	    (rpcproc_t)procnum, progname, inproc, outproc, "udp");
}

/*
 * All the following clnt_broadcast stuff is convulated; it supports
 * the earlier calling style of the callback function
 */
static thread_key_t	clnt_broadcast_key;
static resultproc_t	clnt_broadcast_result_main;

/*
 * Need to translate the netbuf address into sockaddr_in address.
 * Dont care about netid here.
 */
/* ARGSUSED */
static bool_t
rpc_wrap_bcast(resultp, addr, nconf)
	char *resultp;		/* results of the call */
	struct netbuf *addr;	/* address of the guy who responded */
	struct netconfig *nconf; /* Netconf of the transport */
{
	resultproc_t clnt_broadcast_result;

	if (strcmp(nconf->nc_netid, "udp"))
		return (FALSE);
	if (thr_main())
		clnt_broadcast_result = clnt_broadcast_result_main;
	else
		clnt_broadcast_result = (resultproc_t)thr_getspecific(clnt_broadcast_key);
	return (*clnt_broadcast_result)(resultp,
				(struct sockaddr_in *)addr->buf);
}

/*
 * Broadcasts on UDP transport. Obsoleted by rpc_broadcast().
 */
enum clnt_stat
clnt_broadcast(prog, vers, proc, xargs, argsp, xresults, resultsp, eachresult)
	u_long		prog;		/* program number */
	u_long		vers;		/* version number */
	u_long		proc;		/* procedure number */
	xdrproc_t	xargs;		/* xdr routine for args */
	caddr_t		argsp;		/* pointer to args */
	xdrproc_t	xresults;	/* xdr routine for results */
	caddr_t		resultsp;	/* pointer to results */
	resultproc_t	eachresult;	/* call with each result obtained */
{
	extern mutex_t tsd_lock;

	if (thr_main())
		clnt_broadcast_result_main = eachresult;
	else {
		if (clnt_broadcast_key == 0) {
			mutex_lock(&tsd_lock);
			if (clnt_broadcast_key == 0)
				thr_keycreate(&clnt_broadcast_key, free);
			mutex_unlock(&tsd_lock);
		}
		thr_setspecific(clnt_broadcast_key, (void *) eachresult);
	}
	return rpc_broadcast((rpcprog_t)prog, (rpcvers_t)vers,
	    (rpcproc_t)proc, xargs, argsp, xresults, resultsp,
	    (resultproc_t) rpc_wrap_bcast, "udp");
}

/*
 * Create the client des authentication object. Obsoleted by
 * authdes_seccreate().
 */
AUTH *
authdes_create(servername, window, syncaddr, ckey)
	char *servername;		/* network name of server */
	u_int window;			/* time to live */
	struct sockaddr *syncaddr;	/* optional hostaddr to sync with */
	des_block *ckey;		/* optional conversation key to use */
{
	AUTH *dummy;
	AUTH *nauth;
	char hostname[NI_MAXHOST];

	if (syncaddr) {
		/*
		 * Change addr to hostname, because that is the way
		 * new interface takes it.
		 */
		if (getnameinfo(syncaddr, syncaddr->sa_len, hostname,
		    sizeof hostname, NULL, 0, 0) != 0)
			goto fallback;

		nauth = authdes_seccreate(servername, window, hostname, ckey);
		return (nauth);
	}
fallback:
	dummy = authdes_seccreate(servername, window, NULL, ckey);
	return (dummy);
}

#endif /* PORTMAP */
