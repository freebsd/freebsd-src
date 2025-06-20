/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <rpc/rpc.h>
#include <rpc/clnt_nl.h>

#include <netlink/netlink.h>
#include <netlink/netlink_generic.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_generic.h>

#include "rpc_com.h"
#include "libc_private.h"

/*
 * RPC server to serve a kernel RPC client(s) over netlink(4).  See clnt_nl.c
 * in sys/rpc as the counterpart.
 *
 * Upon creation the client will seek for specified multicast group within the
 * generic netlink family named "rpc".  Then it would listen for incoming
 * messages, process them and send replies over the same netlink socket.
 * See clnt_nl.c for more transport protocol implementation details.
 */

static void svc_nl_destroy(SVCXPRT *);
static bool_t svc_nl_recv(SVCXPRT *, struct rpc_msg *);
static bool_t svc_nl_reply(SVCXPRT *, struct rpc_msg *);
static enum xprt_stat svc_nl_stat(SVCXPRT *);
static bool_t svc_nl_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t svc_nl_freeargs(SVCXPRT *, xdrproc_t, void *);
static bool_t svc_nl_control(SVCXPRT *, const u_int, void *);

static struct xp_ops nl_ops = {
	.xp_recv = svc_nl_recv,
	.xp_reply = svc_nl_reply,
	.xp_stat = svc_nl_stat,
	.xp_getargs = svc_nl_getargs,
	.xp_freeargs = svc_nl_freeargs,
	.xp_destroy = svc_nl_destroy,
};
static struct xp_ops2 nl_ops2 = {
	.xp_control = svc_nl_control,
};

struct nl_softc {
	struct snl_state snl;
	XDR		xdrs;
	struct nlmsghdr	*hdr;
	pthread_key_t	xidkey;
	size_t		mlen;
	enum xprt_stat	stat;
	uint32_t	xid;
	uint32_t	group;
	uint16_t	family;
	u_int		errline;
	int		error;
};

SVCXPRT *
svc_nl_create(const char *service)
{
	static struct sockaddr_nl snl_null = {
		.nl_len = sizeof(struct sockaddr_nl),
		.nl_family = PF_NETLINK,
	};
	struct nl_softc *sc;
	SVCXPRT *xprt = NULL;
	void *buf = NULL;
	uint16_t family;
	ssize_t len = 1024;

	if ((sc = calloc(1, sizeof(struct nl_softc))) == NULL)
		return (NULL);
	if (!snl_init(&sc->snl, NETLINK_GENERIC) || (sc->group =
	    snl_get_genl_mcast_group(&sc->snl, "rpc", service, &family)) == 0)
		goto fail;
	if (setsockopt(sc->snl.fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
	    &sc->group, sizeof(sc->group)) == -1)
		goto fail;
	if ((buf = malloc(len)) == NULL)
		goto fail;
	if ((xprt = svc_xprt_alloc()) == NULL)
		goto fail;

	sc->hdr = buf,
	sc->mlen = len,
	sc->stat = XPRT_IDLE,
	sc->family = family;

	if (__isthreaded &&
	    (pthread_key_create(&sc->xidkey, NULL) != 0 ||
	    pthread_setspecific(sc->xidkey, &sc->xid) != 0))
		goto fail;

	xprt->xp_fd = sc->snl.fd,
	xprt->xp_p1 = sc,
	xprt->xp_ops = &nl_ops,
	xprt->xp_ops2 = &nl_ops2,
	xprt->xp_rtaddr = (struct netbuf){
		.maxlen = sizeof(struct sockaddr_nl),
		.len = sizeof(struct sockaddr_nl),
		.buf = &snl_null,
	};
	xprt_register(xprt);

	return (xprt);
fail:
	free(xprt);
	free(buf);
	snl_free(&sc->snl);
	free(sc);
	return (NULL);
}

static void
svc_nl_destroy(SVCXPRT *xprt)
{
	struct nl_softc *sc = xprt->xp_p1;

	snl_free(&sc->snl);
	free(sc->hdr);
	free(xprt->xp_p1);
	svc_xprt_free(xprt);
}

#define	DIE(sc) do {							\
	(sc)->stat = XPRT_DIED;						\
	(sc)->errline = __LINE__;					\
	(sc)->error = errno;						\
	return (FALSE);							\
} while (0)

struct nl_request_parsed {
	uint32_t	group;
	struct nlattr	*data;
};
static const struct snl_attr_parser rpcnl_attr_parser[] = {
#define	OUT(field)	offsetof(struct nl_request_parsed, field)
    { .type = RPCNL_REQUEST_GROUP, .off = OUT(group),
       .cb = snl_attr_get_uint32 },
    { .type = RPCNL_REQUEST_BODY, .off = OUT(data), .cb = snl_attr_get_nla },
#undef OUT
};
SNL_DECLARE_GENL_PARSER(request_parser, rpcnl_attr_parser);

static bool_t
svc_nl_recv(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct nl_request_parsed req;
	struct nl_softc *sc = xprt->xp_p1;
	struct nlmsghdr *hdr = sc->hdr;

	switch (sc->stat) {
	case XPRT_IDLE:
		if (recv(xprt->xp_fd, hdr, sizeof(struct nlmsghdr),
		    MSG_PEEK) != sizeof(struct nlmsghdr))
			DIE(sc);
		break;
	case XPRT_MOREREQS:
		sc->stat = XPRT_IDLE;
		break;
	case XPRT_DIED:
		return (FALSE);
	}

	if (sc->mlen < hdr->nlmsg_len) {
		if ((hdr = sc->hdr = realloc(hdr, hdr->nlmsg_len)) == NULL)
			DIE(sc);
		else
			sc->mlen = hdr->nlmsg_len;
	}
	if (read(xprt->xp_fd, hdr, hdr->nlmsg_len) != hdr->nlmsg_len)
		DIE(sc);

	if (hdr->nlmsg_type != sc->family)
		return (FALSE);

	if (((struct genlmsghdr *)(hdr + 1))->cmd != RPCNL_REQUEST)
		return (FALSE);

	if (!snl_parse_nlmsg(NULL, hdr, &request_parser, &req))
		return (FALSE);

	if (req.group != sc->group)
		return (FALSE);

	xdrmem_create(&sc->xdrs, NLA_DATA(req.data), NLA_DATA_LEN(req.data),
	    XDR_DECODE);
	if (xdr_callmsg(&sc->xdrs, msg)) {
		/* XXX: assert that xid == nlmsg_seq? */
		sc->xid = msg->rm_xid;
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * Reenterable reply method.  Note that both the softc and xprt are declared
 * const.  The qualifier for xprt is commented out to match the library
 * prototype.  If doing any substantial changes to the function please
 * temporarily uncomment the const for xprt and check your changes.
 *
 * Applications that want to use svc_nl_reply in a spawned thread context
 * should do the following hacks in self:
 * 1) - Create xprt with svc_nl_create() with libc in threaded mode, e.g.
 *      at least one pthread_create() shall happen before svc_nl_create().
 *    - After xprt creation query it for the pthread_key_t with the
 *      SVCNL_GET_XIDKEY control and save this key.
 * 2) In the RPC function body that wants to become multithreaded:
 *    - Make a copy of the arguments and of the xid with help of
 *      pthread_getspecific() using the key.
 *    - pthread_create() the worker function, pointing it at the copy of
 *      arguments and xid.
 *    - return FALSE, so that RPC generated code doesn't do anything.
 * 3) In the spawned thread:
 *    - Use arguments provided in the copy by the parent.
 *    - Allocate appropriately typed result on stack.
 *    - *** do the actual work ***
 *    - Populate the on-stack result same way as pointed result is populated
 *      in a regular RPC function.
 *    - Point the thread specific storage to the copy of xid provided by the
 *      parent with help of pthread_setspecific().
 *    - Call svc_sendreply() just like the rpcgen(1) generated code does.
 *
 * If all done correctly svc_nl_reply() will use thread specific xid for
 * a call that was processed asynchronously and most recent xid when entered
 * synchronously.  So you can make only some methods of your application
 * reentrable, keeping others as is.
 */

static bool_t
svc_nl_reply(/* const */ SVCXPRT *xprt, struct rpc_msg *msg)
{
	const struct nl_softc *sc = xprt->xp_p1;
	struct snl_state snl;
	struct snl_writer nw;
	XDR xdrs;
	struct nlattr *body;
	bool_t rv;

	msg->rm_xid = __isthreaded ?
	    *(uint32_t *)pthread_getspecific(sc->xidkey) :
	    sc->xid;

	if (__predict_false(!snl_clone(&snl, &sc->snl)))
		return (FALSE);
	snl_init_writer(&snl, &nw);
	snl_create_genl_msg_request(&nw, sc->family, RPCNL_REPLY);
	snl_add_msg_attr_u32(&nw, RPCNL_REPLY_GROUP, sc->group);
	body = snl_reserve_msg_attr_raw(&nw, RPCNL_REPLY_BODY, RPC_MAXDATASIZE);

	xdrmem_create(&xdrs, (char *)(body + 1), RPC_MAXDATASIZE, XDR_ENCODE);

	if (msg->rm_reply.rp_stat == MSG_ACCEPTED &&
	    msg->rm_reply.rp_acpt.ar_stat == SUCCESS) {
		xdrproc_t xdr_proc;
		char *xdr_where;
		u_int pos;

		xdr_proc = msg->acpted_rply.ar_results.proc;
		xdr_where = msg->acpted_rply.ar_results.where;
		msg->acpted_rply.ar_results.proc = (xdrproc_t) xdr_void;
		msg->acpted_rply.ar_results.where = NULL;

		pos = xdr_getpos(&xdrs);
		if (!xdr_replymsg(&xdrs, msg) ||
		    !SVCAUTH_WRAP(&SVC_AUTH(xprt), &xdrs, xdr_proc,
		    xdr_where)) {
			xdr_setpos(&xdrs, pos);
			rv = FALSE;
		} else
			rv = TRUE;
	} else
		rv = xdr_replymsg(&xdrs, msg);

	if (rv) {
		/* snl_finalize_msg() really doesn't work for us */
		body->nla_len = sizeof(struct nlattr) + xdr_getpos(&xdrs);
		nw.hdr->nlmsg_len = ((char *)body - (char *)nw.hdr) +
		    body->nla_len;
		nw.hdr->nlmsg_type = sc->family;
		nw.hdr->nlmsg_flags = NLM_F_REQUEST;
		nw.hdr->nlmsg_seq = msg->rm_xid;
		if (write(xprt->xp_fd, nw.hdr, nw.hdr->nlmsg_len) !=
		    nw.hdr->nlmsg_len)
			DIE(__DECONST(struct nl_softc *, sc));
	}

	snl_free(&snl);

	return (rv);
}

static bool_t
svc_nl_control(SVCXPRT *xprt, const u_int req, void *v)
{
	struct nl_softc *sc = xprt->xp_p1;

	switch (req) {
	case SVCNL_GET_XIDKEY:
		if (!__isthreaded) {
			/*
			 * Report to application that it had created xprt not
			 * in threaded mode, but definitly plans to use it with
			 * threads.  If it tries so, it would very likely crash.
			 */
			errno = EDOOFUS;
			DIE(sc);
		};
		*(pthread_key_t *)v = sc->xidkey;
		return (TRUE);
	default:
		return (FALSE);
	}
}

static enum xprt_stat
svc_nl_stat(SVCXPRT *xprt)
{
	struct nl_softc *sc = xprt->xp_p1;

	if (sc->stat == XPRT_IDLE &&
	    recv(xprt->xp_fd, sc->hdr, sizeof(struct nlmsghdr),
	    MSG_PEEK | MSG_DONTWAIT) == sizeof(struct nlmsghdr))
		sc->stat = XPRT_MOREREQS;

	return (sc->stat);
}

static bool_t
svc_nl_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
{
	struct nl_softc *sc = xprt->xp_p1;

	return (SVCAUTH_UNWRAP(&SVC_AUTH(xprt), &sc->xdrs, xdr_args, args_ptr));
}

static bool_t
svc_nl_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
{
	struct nl_softc *sc = xprt->xp_p1;

	sc->xdrs.x_op = XDR_FREE;
	return ((*xdr_args)(&sc->xdrs, args_ptr));
}
