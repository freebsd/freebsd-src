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

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/krpc.h>
#include <rpc/clnt_nl.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_generic.h>

/*
 * Kernel RPC client over netlink(4), where kernel is RPC client and an
 * application is a server.  See svc_nl.c in the libc/rpc as the counterpart.
 *
 * The module registers itself within generic netlink families list under name
 * "rpc".  Every new client creates a new multicast group belonging to this
 * family.  When a client starts RPC, the module will multicast the call to
 * potential netlink listeners and sleep/retry until receiving a result.  The
 * framing of the request:
 *
 * [netlink message header, type = "rpc" ID, seq == xid]
 * [generic netlink header, cmd = RPCNL_REQUEST]
 * [netlink attribute RPCNL_REQUEST_GROUP]
 * [group ID]
 * [netlink attribute RPCNL_REQUEST_BODY]
 * [XDR encoded payload]
 *
 * Note: the generic netlink header and attributes aren't really necessary
 * for successful communication, since the netlink multicast membership already
 * guarantees us all needed filtering.  The working prototype was putting the
 * XDR encoded payload right after netlink message header.  But we will provide
 * this framing to allow for any future extensions.
 *
 * The expected RPC result from the userland shall be framed like this:
 *
 * [netlink message header, type = "rpc" ID, seq == xid]
 * [generic netlink header, cmd = RPCNL_REPLY]
 * [netlink attribute RPCNL_REPLY_GROUP]
 * [group ID]
 * [netlink attribute RPCNL_REPLY_BODY]
 * [XDR encoded payload]
 *
 * Disclaimer: has been designed and tested only for the NFS related kernel
 * RPC clients: kgssapi, RPC binding for NLM, TLS client and TLS server.
 *
 * Caveats:
 * 1) Now the privilege checking is hardcoded to PRIV_NFS_DAEMON at the netlink
 *    command and multicast layers.  If any new client in addition to NFS
 *    service emerges, we may want to rewrite privelege checking at the client
 *    level somehow.
 * 2) Since we are using netlink attribute for the payload, payload size is
 *    limited to UINT16_MAX.  Today it is smaller than RPC_MAXDATASIZE of 9000.
 *    What if a future RPC wants more?
 */

static enum clnt_stat clnt_nl_call(CLIENT *, struct rpc_callextra *,
    rpcproc_t, struct mbuf *, struct mbuf **, struct timeval);
static void clnt_nl_close(CLIENT *);
static void clnt_nl_destroy(CLIENT *);
static bool_t clnt_nl_control(CLIENT *, u_int, void *);

static const struct clnt_ops clnt_nl_ops = {
	.cl_call =	clnt_nl_call,
	.cl_close =	clnt_nl_close,
	.cl_destroy =	clnt_nl_destroy,
	.cl_control =	clnt_nl_control,
};

static int clnt_nl_reply(struct nlmsghdr *, struct nl_pstate *);

static const struct genl_cmd clnt_cmds[] = {
	{
		.cmd_num = RPCNL_REPLY,
		.cmd_name = "request",
		.cmd_cb = clnt_nl_reply,
		.cmd_priv = PRIV_NFS_DAEMON,
	},
};

struct nl_reply_parsed {
	uint32_t	group;
	struct nlattr	*data;
};
static const struct nlattr_parser rpcnl_attr_parser[] = {
#define	OUT(field)	offsetof(struct nl_reply_parsed, field)
    { .type = RPCNL_REPLY_GROUP, .off = OUT(group), .cb = nlattr_get_uint32 },
    { .type = RPCNL_REPLY_BODY, .off = OUT(data), .cb = nlattr_get_nla },
#undef OUT
};
NL_DECLARE_PARSER(rpcnl_parser, struct genlmsghdr, nlf_p_empty,
    rpcnl_attr_parser);

struct nl_data {
	struct mtx	nl_lock;
	RB_ENTRY(nl_data) nl_tree;
	TAILQ_HEAD(, ct_request) nl_pending;
	uint32_t	nl_xid;
	u_int		nl_mpos;
	u_int		nl_authlen;
	u_int		nl_retries;
	struct {
		struct genlmsghdr ghdr;
		struct nlattr gattr;
		uint32_t group;
	} nl_hdr;	/* pre-initialized header */
	char		nl_mcallc[MCALL_MSG_SIZE]; /* marshalled callmsg */
	/* msleep(9) arguments */
	const char *	nl_wchan;
	int		nl_prio;
	int		nl_timo;
};

static RB_HEAD(nl_data_t, nl_data) rpcnl_clients;
static int32_t
nl_data_compare(const struct nl_data *a, const struct nl_data *b)
{
	return ((int32_t)(a->nl_hdr.group - b->nl_hdr.group));
}
RB_GENERATE_STATIC(nl_data_t, nl_data, nl_tree, nl_data_compare);
static struct rwlock rpcnl_global_lock;

static const char rpcnl_family_name[] = "rpc";
static uint16_t rpcnl_family_id;

void
rpcnl_init(void)
{
	bool rv __diagused;

	rpcnl_family_id = genl_register_family(rpcnl_family_name, 0, 1, 1);
	MPASS(rpcnl_family_id != 0);
	rv = genl_register_cmds(rpcnl_family_id, clnt_cmds, nitems(clnt_cmds));
	MPASS(rv);
	rw_init(&rpcnl_global_lock, rpcnl_family_name);
}

CLIENT *
client_nl_create(const char *name, const rpcprog_t program,
    const rpcvers_t version)
{
	CLIENT *cl;
	struct nl_data *nl;
	struct timeval now;
	struct rpc_msg call_msg;
	XDR xdrs;
	uint32_t group;
	bool rv __diagused;

	if ((group = genl_register_group(rpcnl_family_id, name)) == 0)
		return (NULL);

	nl = malloc(sizeof(*nl), M_RPC, M_WAITOK);
	*nl = (struct nl_data){
		.nl_pending = TAILQ_HEAD_INITIALIZER(nl->nl_pending),
		.nl_hdr = {
			.ghdr.cmd = RPCNL_REQUEST,
			.gattr.nla_type = RPCNL_REQUEST_GROUP,
			.gattr.nla_len = sizeof(struct nlattr) +
			    sizeof(uint32_t),
			.group = group,
		},
		.nl_wchan = rpcnl_family_name,
		.nl_prio = PSOCK | PCATCH,
		.nl_timo = 60 * hz,
		.nl_retries = 1,
	};
	mtx_init(&nl->nl_lock, "rpc_clnt_nl", NULL, MTX_DEF);

	/*
	 * Initialize and pre-serialize the static part of the call message.
	 */
	getmicrotime(&now);
	nl->nl_xid = __RPC_GETXID(&now);
	call_msg = (struct rpc_msg ){
		.rm_xid = nl->nl_xid,
		.rm_direction = CALL,
		.rm_call = {
			.cb_rpcvers = RPC_MSG_VERSION,
			.cb_prog = (uint32_t)program,
			.cb_vers = (uint32_t)version,
		},
	};

	cl = malloc(sizeof(*cl), M_RPC, M_WAITOK);
	*cl = (CLIENT){
		.cl_refs = 1,
		.cl_ops = &clnt_nl_ops,
		.cl_private = nl,
		.cl_auth = authnone_create(),
	};

	/*
	 * Experimentally learn how many bytes does procedure name plus
	 * authnone header needs.  Use nl_mcallc as temporary scratch space.
	 */
	xdrmem_create(&xdrs, nl->nl_mcallc, MCALL_MSG_SIZE, XDR_ENCODE);
	rv = xdr_putint32(&xdrs, &(rpcproc_t){0});
	MPASS(rv);
	rv = AUTH_MARSHALL(cl->cl_auth, 0, &xdrs, NULL);
	MPASS(rv);
	nl->nl_authlen = xdr_getpos(&xdrs);
	xdr_destroy(&xdrs);

	xdrmem_create(&xdrs, nl->nl_mcallc, MCALL_MSG_SIZE, XDR_ENCODE);
	rv = xdr_callhdr(&xdrs, &call_msg);
	MPASS(rv);
	nl->nl_mpos = xdr_getpos(&xdrs);
	xdr_destroy(&xdrs);

	rw_wlock(&rpcnl_global_lock);
	RB_INSERT(nl_data_t, &rpcnl_clients, nl);
	rw_wunlock(&rpcnl_global_lock);

	return (cl);
}

static enum clnt_stat
clnt_nl_call(CLIENT *cl, struct rpc_callextra *ext, rpcproc_t proc,
    struct mbuf *args, struct mbuf **resultsp, struct timeval utimeout)
{
	struct nl_writer nw;
	struct nl_data *nl = cl->cl_private;
	struct ct_request *cr;
	struct rpc_err *errp, err;
	enum clnt_stat stat;
	AUTH *auth;
	XDR xdrs;
	void *mem;
	uint32_t len, xlen;
	u_int retries = 0;
	bool rv __diagused;

	CURVNET_ASSERT_SET();

	cr = malloc(sizeof(struct ct_request), M_RPC, M_WAITOK);
	*cr = (struct ct_request){
		.cr_xid = atomic_fetchadd_32(&nl->nl_xid, 1),
		.cr_error = ETIMEDOUT,
#ifdef VIMAGE
		.cr_vnet = curvnet,
#endif
	};

	if (ext) {
		auth = ext->rc_auth;
		errp = &ext->rc_err;
		len = RPC_MAXDATASIZE;	/* XXXGL: can be improved */
	} else {
		auth = cl->cl_auth;
		errp = &err;
		len = nl->nl_mpos + nl->nl_authlen + m_length(args, NULL);
	}

	mem = malloc(len, M_RPC, M_WAITOK);
retry:
	xdrmem_create(&xdrs, mem, len, XDR_ENCODE);

	rv = xdr_putbytes(&xdrs, nl->nl_mcallc, nl->nl_mpos);
	MPASS(rv);
	rv = xdr_putint32(&xdrs, &proc);
	MPASS(rv);
	if (!AUTH_MARSHALL(auth, cr->cr_xid, &xdrs, args)) {
		stat = errp->re_status = RPC_CANTENCODEARGS;
		goto out;
	} else
		stat = errp->re_status = RPC_SUCCESS;

	/* XXX: XID is the first thing in the request. */
	*(uint32_t *)mem = htonl(cr->cr_xid);

	xlen = xdr_getpos(&xdrs);
	rv = nl_writer_group(&nw, xlen, NETLINK_GENERIC, nl->nl_hdr.group,
	    PRIV_NFS_DAEMON, true);
	MPASS(rv);

	rv = nlmsg_add(&nw, 0, cr->cr_xid, rpcnl_family_id, 0,
	    sizeof(nl->nl_hdr) + sizeof(struct nlattr) + xlen);
	MPASS(rv);

	memcpy(nlmsg_reserve_data_raw(&nw, sizeof(nl->nl_hdr)), &nl->nl_hdr,
	    sizeof(nl->nl_hdr));

	rv = nlattr_add(&nw, RPCNL_REQUEST_BODY, xlen, mem);
	MPASS(rv);

	rv = nlmsg_end(&nw);
	MPASS(rv);

	mtx_lock(&nl->nl_lock);
	TAILQ_INSERT_TAIL(&nl->nl_pending, cr, cr_link);
	mtx_unlock(&nl->nl_lock);

	nlmsg_flush(&nw);

	mtx_lock(&nl->nl_lock);
	if (__predict_true(cr->cr_error == ETIMEDOUT))
		(void)msleep(cr, &nl->nl_lock, nl->nl_prio, nl->nl_wchan,
		    (nl->nl_timo ? nl->nl_timo : tvtohz(&utimeout)) /
		    nl->nl_retries);
	TAILQ_REMOVE(&nl->nl_pending, cr, cr_link);
	mtx_unlock(&nl->nl_lock);

	if (__predict_true(cr->cr_error == 0)) {
		struct rpc_msg reply_msg = {
			.acpted_rply.ar_verf.oa_base = cr->cr_verf,
			.acpted_rply.ar_results.proc = (xdrproc_t)xdr_void,
		};

		MPASS(cr->cr_mrep);
		if (ext && ext->rc_feedback)
			ext->rc_feedback(FEEDBACK_OK, proc,
			    ext->rc_feedback_arg);
		xdrmbuf_create(&xdrs, cr->cr_mrep, XDR_DECODE);
		rv = xdr_replymsg(&xdrs, &reply_msg);
		if (__predict_false(!rv)) {
			stat = errp->re_status = RPC_CANTDECODERES;
			goto out;
		}
		if ((reply_msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
		    (reply_msg.acpted_rply.ar_stat == SUCCESS)) {
			struct mbuf *results;

                        stat = errp->re_status = RPC_SUCCESS;
			results = xdrmbuf_getall(&xdrs);
			if (__predict_true(AUTH_VALIDATE(auth, cr->cr_xid,
			    &reply_msg.acpted_rply.ar_verf, &results))) {
                                MPASS(results);
                                *resultsp = results;
				/* end successful completion */
			} else {
				stat = errp->re_status = RPC_AUTHERROR;
				errp->re_why = AUTH_INVALIDRESP;
			}
		} else {
			stat = _seterr_reply(&reply_msg, errp);
		}
		xdr_destroy(&xdrs);	/* frees cr->cr_mrep */
	} else {
		MPASS(cr->cr_mrep == NULL);
		errp->re_errno = cr->cr_error;
		stat = errp->re_status = RPC_CANTRECV;
		if (cr->cr_error == ETIMEDOUT && ++retries < nl->nl_retries) {
			cr->cr_xid = atomic_fetchadd_32(&nl->nl_xid, 1);
			goto retry;
		}
	}
out:
	free(cr, M_RPC);
	free(mem, M_RPC);

	return (stat);
}

static int
clnt_nl_reply(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct nl_reply_parsed attrs = {};
	struct nl_data *nl;
	struct ct_request *cr;
	struct mchain mc;
	int error;

	CURVNET_ASSERT_SET();

	if ((error = nl_parse_nlmsg(hdr, &rpcnl_parser, npt, &attrs)) != 0)
		return (error);
	if (attrs.data == NULL)
		return (EINVAL);

	error = mc_get(&mc, NLA_DATA_LEN(attrs.data), M_WAITOK, MT_DATA, 0);
	MPASS(error == 0);
	m_copyback(mc_first(&mc), 0, NLA_DATA_LEN(attrs.data),
	    NLA_DATA(attrs.data));

	rw_rlock(&rpcnl_global_lock);
	if ((nl = RB_FIND(nl_data_t, &rpcnl_clients,
	    &(struct nl_data){ .nl_hdr.group = attrs.group })) == NULL) {
		rw_runlock(&rpcnl_global_lock);
		mc_freem(&mc);
		return (EPROGUNAVAIL);
	};
	mtx_lock(&nl->nl_lock);
	rw_runlock(&rpcnl_global_lock);

	TAILQ_FOREACH(cr, &nl->nl_pending, cr_link)
		if (cr->cr_xid == hdr->nlmsg_seq
#ifdef VIMAGE
		    && cr->cr_vnet == curvnet
#endif
		    )
			break;
	if (cr == NULL) {
		mtx_unlock(&nl->nl_lock);
		mc_freem(&mc);
		return (EPROCUNAVAIL);
	}
	cr->cr_mrep = mc_first(&mc);
	cr->cr_error = 0;
	wakeup(cr);
	mtx_unlock(&nl->nl_lock);

	return (0);
}

static void
clnt_nl_close(CLIENT *cl)
{
	struct nl_data *nl =  cl->cl_private;
	struct ct_request *cr;

	mtx_lock(&nl->nl_lock);
	TAILQ_FOREACH(cr, &nl->nl_pending, cr_link) {
		cr->cr_error = ESHUTDOWN;
		wakeup(cr);
	}
	mtx_unlock(&nl->nl_lock);
}

static void
clnt_nl_destroy(CLIENT *cl)
{
	struct nl_data *nl = cl->cl_private;

	MPASS(TAILQ_EMPTY(&nl->nl_pending));

	genl_unregister_group(rpcnl_family_id, nl->nl_hdr.group);
	rw_wlock(&rpcnl_global_lock);
	RB_REMOVE(nl_data_t, &rpcnl_clients, nl);
	rw_wlock(&rpcnl_global_lock);

	mtx_destroy(&nl->nl_lock);
	free(nl, M_RPC);
	free(cl, M_RPC);
}

static bool_t
clnt_nl_control(CLIENT *cl, u_int request, void *info)
{
	struct nl_data *nl = (struct nl_data *)cl->cl_private;

	mtx_lock(&nl->nl_lock);
	switch (request) {
	case CLSET_TIMEOUT:
		nl->nl_timo = tvtohz((struct timeval *)info);
		break;

	case CLGET_TIMEOUT:
		*(struct timeval *)info =
		    (struct timeval){.tv_sec = nl->nl_timo / hz};
		break;

	case CLSET_RETRIES:
		nl->nl_retries = *(u_int *)info;
		break;

	case CLSET_WAITCHAN:
		nl->nl_wchan = (const char *)info;
		break;

	case CLGET_WAITCHAN:
		*(const char **)info = nl->nl_wchan;
		break;

	case CLSET_INTERRUPTIBLE:
		if (*(int *)info)
			nl->nl_prio |= PCATCH;
		else
			nl->nl_prio &= ~PCATCH;
		break;

	case CLGET_INTERRUPTIBLE:
		*(int *)info = (nl->nl_prio & PCATCH) ? TRUE : FALSE;
		break;

	default:
		mtx_unlock(&nl->nl_lock);
		printf("%s: unsupported request %u\n", __func__, request);
		return (FALSE);
	}

	mtx_unlock(&nl->nl_lock);
	return (TRUE);
}
