/* @(#)svc_udp.c	2.2 88/07/29 4.0 RPCSRC */
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
static char sccsid[] = "@(#)svc_udp.c 1.24 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * svc_udp.c,
 * Server side for UDP/IP based RPC.  (Does some caching in the hopes of
 * achieving execute-at-most-once semantics.)
 */

#include "k5-platform.h"
#include <unistd.h>
#include <gssrpc/rpc.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif
#include <port-sockets.h>
#include <socket-utils.h>


#ifndef GETSOCKNAME_ARG3_TYPE
#define GETSOCKNAME_ARG3_TYPE int
#endif

#define rpc_buffer(xprt) ((xprt)->xp_p1)
#ifndef MAX
#define MAX(a, b)     ((a > b) ? a : b)
#endif

static bool_t		svcudp_recv(SVCXPRT *, struct rpc_msg *);
static bool_t		svcudp_reply(SVCXPRT *, struct rpc_msg *);
static enum xprt_stat	svcudp_stat(SVCXPRT *);
static bool_t		svcudp_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t		svcudp_freeargs(SVCXPRT *, xdrproc_t, void *);
static void		svcudp_destroy(SVCXPRT *);

static void cache_set(SVCXPRT *, uint32_t);
static int cache_get(SVCXPRT *, struct rpc_msg *, char **, uint32_t *);

static struct xp_ops svcudp_op = {
	svcudp_recv,
	svcudp_stat,
	svcudp_getargs,
	svcudp_reply,
	svcudp_freeargs,
	svcudp_destroy
};


/*
 * kept in xprt->xp_p2
 */
struct svcudp_data {
	u_int   su_iosz;	/* byte size of send.recv buffer */
	uint32_t	su_xid;		/* transaction id */
	XDR	su_xdrs;	/* XDR handle */
	char	su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	void * 	su_cache;	/* cached data, NULL if no cache */
};
#define	su_data(xprt)	((struct svcudp_data *)(xprt->xp_p2))

/*
 * Usage:
 *	xprt = svcudp_create(sock);
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svcudp_create
 * binds it to an arbitrary port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 * Once *xprt is initialized, it is registered as a transporter;
 * see (svc.h, xprt_register).
 * The routines returns NULL if a problem occurred.
 */
SVCXPRT *
svcudp_bufcreate(
	int sock,
	u_int sendsz,
	u_int recvsz)
{
	bool_t madesock = FALSE;
	SVCXPRT *xprt;
	struct svcudp_data *su;
	struct sockaddr_storage ss;
	struct sockaddr *sa = (struct sockaddr *)&ss;
	socklen_t len;

	if (sock == RPC_ANYSOCK) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("svcudp_create: socket creation problem");
			return ((SVCXPRT *)NULL);
		}
		set_cloexec_fd(sock);
		madesock = TRUE;
		memset(&ss, 0, sizeof(ss));
		sa->sa_family = AF_INET;
	} else {
		len = sizeof(struct sockaddr_storage);
		if (getsockname(sock, sa, &len) < 0) {
			perror("svcudp_create - cannot getsockname");
			return ((SVCXPRT *)NULL);
		}
	}

	if (bindresvport_sa(sock, sa)) {
		sa_setport(sa, 0);
		(void)bind(sock, sa, sa_socklen(sa));
	}
	len = sizeof(struct sockaddr_storage);
	if (getsockname(sock, sa, &len) != 0) {
		perror("svcudp_create - cannot getsockname");
		if (madesock)
			(void)close(sock);
		return ((SVCXPRT *)NULL);
	}
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		(void)fprintf(stderr, "svcudp_create: out of memory\n");
		return (NULL);
	}
	su = (struct svcudp_data *)mem_alloc(sizeof(*su));
	if (su == NULL) {
		(void)fprintf(stderr, "svcudp_create: out of memory\n");
		return (NULL);
	}
	su->su_iosz = ((MAX(sendsz, recvsz) + 3) / 4) * 4;
	if ((rpc_buffer(xprt) = mem_alloc(su->su_iosz)) == NULL) {
		(void)fprintf(stderr, "svcudp_create: out of memory\n");
		return (NULL);
	}
	xdrmem_create(
	    &(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_DECODE);
	su->su_cache = NULL;
	xprt->xp_p2 = (caddr_t)su;
	xprt->xp_auth = NULL;
	xprt->xp_verf.oa_base = su->su_verfbody;
	xprt->xp_ops = &svcudp_op;
	xprt->xp_port = sa_getport(sa);
	xprt->xp_sock = sock;
	xprt_register(xprt);
	return (xprt);
}

SVCXPRT *
svcudp_create(int sock)
{

	return(svcudp_bufcreate(sock, UDPMSGSIZE, UDPMSGSIZE));
}

static enum xprt_stat
svcudp_stat(SVCXPRT *xprt)
{

	return (XPRT_IDLE);
}

static bool_t
svcudp_recv(
	SVCXPRT *xprt,
	struct rpc_msg *msg)
{
        struct msghdr dummy;
	struct iovec dummy_iov[1];
	struct svcudp_data *su = su_data(xprt);
	XDR *xdrs = &su->su_xdrs;
	int rlen;
	char *reply;
	uint32_t replylen;
	socklen_t addrlen;

    again:
	memset(&dummy, 0, sizeof(dummy));
	dummy_iov[0].iov_base = rpc_buffer(xprt);
	dummy_iov[0].iov_len = (int) su->su_iosz;
	dummy.msg_iov = dummy_iov;
	dummy.msg_iovlen = 1;
	dummy.msg_namelen = xprt->xp_laddrlen = sizeof(struct sockaddr_in);
	dummy.msg_name = (char *) &xprt->xp_laddr;
	rlen = recvmsg(xprt->xp_sock, &dummy, MSG_PEEK);
	if (rlen == -1) {
	     if (errno == EINTR)
		  goto again;
	     else
		  return (FALSE);
	}

	addrlen = sizeof(struct sockaddr_in);
	rlen = recvfrom(xprt->xp_sock, rpc_buffer(xprt), (int) su->su_iosz,
	    0, (struct sockaddr *)&(xprt->xp_raddr), &addrlen);
	if (rlen == -1 && errno == EINTR)
		goto again;
	if (rlen < (int) (4*sizeof(uint32_t)))
		return (FALSE);
	xprt->xp_addrlen = addrlen;
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg))
		return (FALSE);
	su->su_xid = msg->rm_xid;
	if (su->su_cache != NULL) {
		if (cache_get(xprt, msg, &reply, &replylen)) {
			(void) sendto(xprt->xp_sock, reply, (int) replylen, 0,
			  (struct sockaddr *) &xprt->xp_raddr, xprt->xp_addrlen);
			return (TRUE);
		}
	}
	return (TRUE);
}

static bool_t svcudp_reply(
	SVCXPRT *xprt,
	struct rpc_msg *msg)
{
     struct svcudp_data *su = su_data(xprt);
     XDR *xdrs = &su->su_xdrs;
     int slen;
     bool_t stat = FALSE;

     xdrproc_t xdr_results = NULL;
     caddr_t xdr_location = 0;
     bool_t has_args;

     if (msg->rm_reply.rp_stat == MSG_ACCEPTED &&
	 msg->rm_reply.rp_acpt.ar_stat == SUCCESS) {
	  has_args = TRUE;
	  xdr_results = msg->acpted_rply.ar_results.proc;
	  xdr_location = msg->acpted_rply.ar_results.where;

	  msg->acpted_rply.ar_results.proc = xdr_void;
	  msg->acpted_rply.ar_results.where = NULL;
     } else
	  has_args = FALSE;

     xdrs->x_op = XDR_ENCODE;
     XDR_SETPOS(xdrs, 0);
     msg->rm_xid = su->su_xid;
     if (xdr_replymsg(xdrs, msg) &&
	 (!has_args ||
	  (SVCAUTH_WRAP(xprt->xp_auth, xdrs, xdr_results, xdr_location)))) {
	  slen = (int)XDR_GETPOS(xdrs);
	  if (sendto(xprt->xp_sock, rpc_buffer(xprt), slen, 0,
		     (struct sockaddr *)&(xprt->xp_raddr), xprt->xp_addrlen)
	      == slen) {
	       stat = TRUE;
	       if (su->su_cache && slen >= 0) {
		    cache_set(xprt, (uint32_t) slen);
	       }
	  }
     }
     return (stat);
}

static bool_t
svcudp_getargs(
	SVCXPRT *xprt,
	xdrproc_t xdr_args,
	void * args_ptr)
{
	if (! SVCAUTH_UNWRAP(xprt->xp_auth, &(su_data(xprt)->su_xdrs),
			     xdr_args, args_ptr)) {
		(void)svcudp_freeargs(xprt, xdr_args, args_ptr);
		return FALSE;
	}
	return TRUE;
}

static bool_t
svcudp_freeargs(
	SVCXPRT *xprt,
	xdrproc_t xdr_args,
	void * args_ptr)
{
	XDR *xdrs = &su_data(xprt)->su_xdrs;

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static void
svcudp_destroy(SVCXPRT *xprt)
{
	struct svcudp_data *su = su_data(xprt);

	xprt_unregister(xprt);
        if (xprt->xp_sock != INVALID_SOCKET)
                (void)closesocket(xprt->xp_sock);
        xprt->xp_sock = INVALID_SOCKET;
	if (xprt->xp_auth != NULL) {
		SVCAUTH_DESTROY(xprt->xp_auth);
		xprt->xp_auth = NULL;
	}
	XDR_DESTROY(&(su->su_xdrs));
	mem_free(rpc_buffer(xprt), su->su_iosz);
	mem_free((caddr_t)su, sizeof(struct svcudp_data));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}


/***********this could be a separate file*********************/

/*
 * Fifo cache for udp server
 * Copies pointers to reply buffers into fifo cache
 * Buffers are sent again if retransmissions are detected.
 */

#define SPARSENESS 4	/* 75% sparse */

#define CACHE_PERROR(msg)	\
	(void) fprintf(stderr,"%s\n", msg)

#define ALLOC(type, size)	\
	(type *) mem_alloc((unsigned) (sizeof(type) * (size)))

#define BZERO(addr, type, size)	 \
	memset(addr, 0, sizeof(type) * (int) (size))

/*
 * An entry in the cache
 */
typedef struct cache_node *cache_ptr;
struct cache_node {
	/*
	 * Index into cache is xid, proc, vers, prog and address
	 */
	uint32_t cache_xid;
	rpcproc_t cache_proc;
	rpcvers_t cache_vers;
	rpcprog_t cache_prog;
	struct sockaddr_in cache_addr;
	/*
	 * The cached reply and length
	 */
	char * cache_reply;
	uint32_t cache_replylen;
	/*
 	 * Next node on the list, if there is a collision
	 */
	cache_ptr cache_next;
};



/*
 * The entire cache
 */
struct udp_cache {
	uint32_t uc_size;		/* size of cache */
	cache_ptr *uc_entries;	/* hash table of entries in cache */
	cache_ptr *uc_fifo;	/* fifo list of entries in cache */
	uint32_t uc_nextvictim;	/* points to next victim in fifo list */
	rpcprog_t uc_prog;		/* saved program number */
	rpcvers_t uc_vers;		/* saved version number */
	rpcproc_t uc_proc;		/* saved procedure number */
	struct sockaddr_in uc_addr; /* saved caller's address */
};


/*
 * the hashing function
 */
#define CACHE_LOC(transp, xid)	\
 (xid % (SPARSENESS*((struct udp_cache *) su_data(transp)->su_cache)->uc_size))


/*
 * Enable use of the cache.
 * Note: there is no disable.
 */
int
svcudp_enablecache(
	SVCXPRT *transp,
	uint32_t size)
{
	struct svcudp_data *su = su_data(transp);
	struct udp_cache *uc;

	if (su->su_cache != NULL) {
		CACHE_PERROR("enablecache: cache already enabled");
		return(0);
	}
	uc = ALLOC(struct udp_cache, 1);
	if (uc == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache");
		return(0);
	}
	uc->uc_size = size;
	uc->uc_nextvictim = 0;
	uc->uc_entries = ALLOC(cache_ptr, size * SPARSENESS);
	if (uc->uc_entries == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache data");
		return(0);
	}
	BZERO(uc->uc_entries, cache_ptr, size * SPARSENESS);
	uc->uc_fifo = ALLOC(cache_ptr, size);
	if (uc->uc_fifo == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache fifo");
		return(0);
	}
	BZERO(uc->uc_fifo, cache_ptr, size);
	su->su_cache = (char *) uc;
	return(1);
}


/*
 * Set an entry in the cache
 */
static void
cache_set(
	SVCXPRT *xprt,
	uint32_t replylen)
{
	cache_ptr victim;
	cache_ptr *vicp;
	struct svcudp_data *su = su_data(xprt);
	struct udp_cache *uc = (struct udp_cache *) su->su_cache;
	u_int loc;
	char *newbuf;

	/*
 	 * Find space for the new entry, either by
	 * reusing an old entry, or by mallocing a new one
	 */
	victim = uc->uc_fifo[uc->uc_nextvictim];
	if (victim != NULL) {
		loc = CACHE_LOC(xprt, victim->cache_xid);
		for (vicp = &uc->uc_entries[loc];
		  *vicp != NULL && *vicp != victim;
		  vicp = &(*vicp)->cache_next)
				;
		if (*vicp == NULL) {
			CACHE_PERROR("cache_set: victim not found");
			return;
		}
		*vicp = victim->cache_next;	/* remote from cache */
		newbuf = victim->cache_reply;
	} else {
		victim = ALLOC(struct cache_node, 1);
		if (victim == NULL) {
			CACHE_PERROR("cache_set: victim alloc failed");
			return;
		}
		newbuf = mem_alloc(su->su_iosz);
		if (newbuf == NULL) {
			CACHE_PERROR("cache_set: could not allocate new rpc_buffer");
			free(victim);
			return;
		}
	}

	/*
	 * Store it away
	 */
	victim->cache_replylen = replylen;
	victim->cache_reply = rpc_buffer(xprt);
	rpc_buffer(xprt) = newbuf;
	xdrmem_create(&(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_ENCODE);
	victim->cache_xid = su->su_xid;
	victim->cache_proc = uc->uc_proc;
	victim->cache_vers = uc->uc_vers;
	victim->cache_prog = uc->uc_prog;
	victim->cache_addr = uc->uc_addr;
	loc = CACHE_LOC(xprt, victim->cache_xid);
	victim->cache_next = uc->uc_entries[loc];
	uc->uc_entries[loc] = victim;
	uc->uc_fifo[uc->uc_nextvictim++] = victim;
	uc->uc_nextvictim %= uc->uc_size;
}

/*
 * Try to get an entry from the cache
 * return 1 if found, 0 if not found
 */
static int
cache_get(
	SVCXPRT *xprt,
	struct rpc_msg *msg,
	char **replyp,
	uint32_t *replylenp)
{
	u_int loc;
	cache_ptr ent;
	struct svcudp_data *su = su_data(xprt);
	struct udp_cache *uc = su->su_cache;

#	define EQADDR(a1, a2) (memcmp((char*)&a1, (char*)&a2, sizeof(a1)) == 0)

	loc = CACHE_LOC(xprt, su->su_xid);
	for (ent = uc->uc_entries[loc]; ent != NULL; ent = ent->cache_next) {
		if (ent->cache_xid == su->su_xid &&
		  ent->cache_proc == uc->uc_proc &&
		  ent->cache_vers == uc->uc_vers &&
		  ent->cache_prog == uc->uc_prog &&
		  EQADDR(ent->cache_addr, uc->uc_addr)) {
			*replyp = ent->cache_reply;
			*replylenp = ent->cache_replylen;
			return(1);
		}
	}
	/*
	 * Failed to find entry
	 * Remember a few things so we can do a set later
	 */
	uc->uc_proc = msg->rm_call.cb_proc;
	uc->uc_vers = msg->rm_call.cb_vers;
	uc->uc_prog = msg->rm_call.cb_prog;
	uc->uc_addr = xprt->xp_raddr;
	return(0);
}
