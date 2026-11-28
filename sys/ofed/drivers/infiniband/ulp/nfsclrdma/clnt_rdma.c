/*	$NetBSD: clnt_vc.c,v 1.4 2000/07/14 08:40:42 fvdl Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2009, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Sun Microsystems, Inc. nor the names of its
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
 * clnt_tcp.c, Implements a TCP/IP based, client side RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * TCP based RPC supports 'batched calls'.
 * A sequence of calls may be batched-up in a send buffer.  The rpc call
 * return immediately to the client even though the call was not necessarily
 * sent.  The batching occurs if the results' xdr routine is NULL (0) AND
 * the rpc timeout value is zero (see clnt.h, rpc).
 *
 * Clients should NOT casually batch calls that in fact return results; that is,
 * the server side should be aware that a call is batched and not produce any
 * return message.  Batched calls that produce many result messages can
 * deadlock (netlock) the client and the server....
 *
 * Now go hang yourself.
 */

/*
 * The code in this file handles RPC over RDMA as described by
 * RFC-8166 and RFC-8267 and is loosely based on code that the above
 * comment discusses.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktls.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <net/vnet.h>

#include <netinet/tcp.h>

#include <rpc/rpc.h>
#include <rpc/rpc_com.h>
#include <rpc/krpc.h>
#include <rpc/clntrdma.h>

/*
 * This structure stores the internals for an RDMA connection endpoint.
 */
struct crdma_data {
	int		crdma_threads;	/* # of threads in clnt_rdma_call */
	int		crdma_upcallrefs; /* Cnt of upcalls in progress */
	bool_t		crdma_closing;	/* TRUE if we are closing */
	bool_t		crdma_closed;	/* TRUE if we are closed */
	struct rpcrdma_xprt crdma_xp;	/* RDMA endpoint */
	bool_t		crdma_closeit;	/* close it on destroy */
	struct timeval	crdma_wait;	/* wait interval in milliseconds */
	struct sockaddr_storage	crdma_addr; /* remote addr */
	struct rpc_err	crdma_error;
	uint32_t	crdma_xid;
	uint32_t	crdma_prog;
	uint32_t	crdma_vers;
	char		crdma_mcallc[MCALL_MSG_SIZE]; /* call msg */
	size_t		crdma_mpos;	/* pos after marshal */
	const char	*crdma_waitchan;
	int		crdma_waitflag;
	struct mbuf	*crdma_record;	/* current reply record */
	size_t		crdma_record_resid; /* how much left of reply to read */
	bool_t		crdma_record_eor; /* true if reading last fragment */
	struct ct_request_list crdma_pending;
	SVCXPRT		*crdma_backchannelxprt; /* xprt for backchannel */
	struct mbuf	*crdma_raw;	/* Raw mbufs recv'd */
	uint32_t	crdma_small_reply; /* Max small reply M_PROTO8 */
	uint32_t	crdma_max_io;	/* Max size of an I/O M_PROTO9 */
	uint32_t	crdma_readcnt;	/* Count of RPCs with a Read RDMA */
};
#define	crdma_lock	crdma_xp.mtx

MALLOC_DECLARE(M_RPCRDMA);

SYSCTL_DECL(_kern_rpc);
SYSCTL_NODE(_kern_rpc, OID_AUTO, rdma, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "RPC over RDMA");
int rpcrdma_debuglevel = 0;
SYSCTL_INT(_kern_rpc_rdma, OID_AUTO, debuglevel, CTLFLAG_RW,
    &rpcrdma_debuglevel, 0, "Debug level for RPC over RDMA client");

u_int rpcrdma_maxrpc = RPCRDMA_IO_NUMBUFS - 8;
static int
sysctl_maxrpc(SYSCTL_HANDLER_ARGS)
{
	int error, newmaxrpc;

	newmaxrpc = rpcrdma_maxrpc;
	error = sysctl_handle_int(oidp, &newmaxrpc, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (newmaxrpc < 4 || newmaxrpc > RPCRDMA_IO_NUMBUFS - 8)
		return (EINVAL);
	rpcrdma_maxrpc = newmaxrpc;
	return (0);
}
SYSCTL_PROC(_kern_rpc_rdma, OID_AUTO, maxrpc,
    CTLTYPE_UINT | CTLFLAG_MPSAFE | CTLFLAG_RW, 0, sizeof(rpcrdma_maxrpc),
    sysctl_maxrpc, "IU", "Maximum # of concurrent rpcs");

struct cmessage {
        struct cmsghdr cmsg;
        struct cmsgcred cmcred;
};

static enum clnt_stat clnt_rdma_call(CLIENT *, struct rpc_callextra *,
    rpcproc_t, struct mbuf *, struct mbuf **, struct timeval);
static void clnt_rdma_geterr(CLIENT *, struct rpc_err *);
static bool_t clnt_rdma_freeres(CLIENT *, xdrproc_t, void *);
static void clnt_rdma_abort(CLIENT *);
static bool_t clnt_rdma_control(CLIENT *, u_int, void *);
static void clnt_rdma_close(CLIENT *);
static void clnt_rdma_destroy(CLIENT *);
static bool_t time_not_ok(struct timeval *);
static void clnt_rdma_upcall(struct rpcrdma_xprt *xp, struct mbuf *);
static bool_t clnt_rdma_bcksend(SVCXPRT *, struct mbuf *);

static const struct clnt_ops clnt_rdma_ops = {
	.cl_call =	clnt_rdma_call,
	.cl_abort =	clnt_rdma_abort,
	.cl_geterr =	clnt_rdma_geterr,
	.cl_freeres =	clnt_rdma_freeres,
	.cl_close =	clnt_rdma_close,
	.cl_destroy =	clnt_rdma_destroy,
	.cl_control =	clnt_rdma_control
};


/*
 * Create a client handle for an RDMA connection.
 */
CLIENT *
clnt_rdma_create(
	struct sockaddr *raddr,		/* servers address */
	const rpcprog_t prog,		/* program number */
	const rpcvers_t vers,		/* version number */
	int intrflag,			/* interruptible */
	uint32_t small_reply,		/* Max. medium reply */
	uint32_t max_io,		/* size of largest RPC I/O */
	uint32_t cbslots,		/* Max # of callbacks */
	struct rpc_err *err)		/* Return error and stat */
{
	CLIENT *cl;			/* client handle */
	struct crdma_data *crdma = NULL;	/* client handle */
	struct timeval now;
	int error;
	static uint32_t disrupt;

	KASSERT(raddr->sa_family != AF_LOCAL,
	    ("%s: kernel RPC over unix(4) not supported", __func__));

	if (disrupt == 0)
		disrupt = (uint32_t)(long)raddr;

	cl = (CLIENT *)mem_alloc(sizeof (*cl));
	crdma = (struct crdma_data *)mem_alloc(sizeof (*crdma));

	mtx_init(&crdma->crdma_lock, "crdma->crdma_lock", NULL, MTX_DEF);
	crdma->crdma_threads = 0;
	crdma->crdma_upcallrefs = 0;
	crdma->crdma_closing = FALSE;
	crdma->crdma_closed = FALSE;
	crdma->crdma_closeit = FALSE;
	TAILQ_INIT(&crdma->crdma_pending);

	/*
	 * Set up private data struct
	 */
	crdma->crdma_xp.credits = 1;
	xprt_rdma_init(&crdma->crdma_xp, clnt_rdma_upcall);
	crdma->crdma_wait.tv_sec = -1;
	crdma->crdma_wait.tv_usec = -1;
	memcpy(&crdma->crdma_addr, raddr, raddr->sa_len);
	crdma->crdma_small_reply = round_page(small_reply);
	crdma->crdma_max_io = round_page(max_io + 2048);

	/* Attempt to connect.. */
#ifdef VIMAGE
	error = xprt_rdma_connect(vnet0, raddr, &crdma->crdma_xp,
	    RPCRDMA_MAX_SMALL_MSG, cbslots);
#else
	error = xprt_rdma_connect(NULL, raddr, &crdma->crdma_xp,
	    RPCRDMA_MAX_SMALL_MSG, cbslots);
#endif
	RPCRDMA_DEBUG(1, "clnt_rdma_create: xprt_rdma_connect=%d\n", error);
	if (error != 0) {
		err->re_errno = error;
		if (error == ENETUNREACH)
			err->re_status = RPC_SYSTEMERROR;
		else if (error == EPROTO)
			err->re_status = RPC_UNKNOWNADDR;
		else
			err->re_status = RPC_UNKNOWNPROTO;
		RPCRDMA_DEBUG(1, "clnt_rdma_create: Fail stat=%d\n",
		    err->re_status);
		goto fail;
	}

	/*
	 * Initialize the xid.
	 */
	getmicrotime(&now);
	crdma->crdma_xid = ((uint32_t)++disrupt) ^ __RPC_GETXID(&now);

	crdma->crdma_prog = prog;
	crdma->crdma_vers = vers;

	crdma->crdma_waitchan = "rdmarecv";
	crdma->crdma_waitflag = 0;

	cl->cl_refs = 1;
	cl->cl_ops = &clnt_rdma_ops;
	cl->cl_private = crdma;
	cl->cl_auth = authnone_create();

	crdma->crdma_raw = NULL;
	crdma->crdma_record = NULL;
	crdma->crdma_record_resid = 0;
	TAILQ_INIT(&crdma->crdma_pending);
	RPCRDMA_DEBUG(1, "clnt_rdma_create: client=%p\n", cl);
	return (cl);
fail:
	RPCRDMA_DEBUG(1, "clnt_rdma_create: failed err=%d\n", error);
	free(crdma->crdma_xp.ep, M_RPCRDMA);
	mtx_destroy(&crdma->crdma_lock);
	mem_free(crdma, sizeof (struct crdma_data));
	mem_free(cl, sizeof (CLIENT));

	return ((CLIENT *)NULL);
}

#define	READLIST	0
#define	WRITELIST	1
#define	WRITECHUNK	2

/*
 * Create an M_EXTPG mbuf list of numpg pages.
 */
static struct mbuf *
_rpc_create_mextpg(int numpg)
{
	struct mbuf *m, *mhead, **mprev;
	int i, j;

	mprev = &mhead;
	mhead = NULL;
	for (i = numpg; i > 0; i -= j) {
		j = MIN(i, MBUF_PEXT_MAX_PGS);
		m = mb_alloc_ext_plus_pages(j * PAGE_SIZE, M_WAITOK);
		if (m == NULL) {
			if (mhead != NULL)
				m_freem(mhead);
			return (NULL);
		}
		m->m_epg_last_len = PAGE_SIZE;
		m->m_len = j * PAGE_SIZE;
		*mprev = m;
		mprev = &m->m_next;
	}
	return (mhead);
}

/*
 * Trim the tail of an M_EXTPG mbuf chain created by _rpc_create_mextpg().
 */
static void
_rpc_trim_mextpg(struct mbuf *mextpg, uint32_t newlen)
{
	struct mbuf *m, *m2;
	vm_page_t pg;
	uint32_t i, j;

	i = 0;
	for (m = mextpg; m != NULL; m = m->m_next) {
		/* At last mbuf. */
		if (i + m->m_len >= newlen)
			break;
		i += m->m_len;
	}
	if (m == NULL)
		return;

	/* Throw away any additional mbufs. */
	m2 = m->m_next;
	m->m_next = NULL;
	if (m2 != NULL)
		m_freem(m2);

	/* Now trim the last mbuf. */
	newlen -= i;
	if (newlen == m->m_len)
		return;
	i = howmany(newlen, PAGE_SIZE);
	for (j = i; j < m->m_epg_npgs; j++) {
		pg = PHYS_TO_VM_PAGE(m->m_epg_pa[j]);
		vm_page_unwire_noq(pg);
		vm_page_free(pg);
	}
	m->m_epg_npgs = i;
	m->m_len = newlen;
	m->m_epg_last_len = newlen % PAGE_SIZE;
	if (m->m_epg_last_len == 0)
		m->m_epg_last_len = PAGE_SIZE;
}

/*
 * A local failure may retire an endpoint without a CM disconnect event.
 * Wake its other callers so they can retry on the replacement connection.
 * NFSv4 reply waits otherwise have a nearly indefinite timeout.
 */
static void
clnt_rdma_fail_locked(struct crdma_data *crdma, int error)
{
	struct ct_request *cr;

	mtx_assert(&crdma->crdma_lock, MA_OWNED);
	xprt_rdma_mark_disconnected(&crdma->crdma_xp);
	TAILQ_FOREACH(cr, &crdma->crdma_pending, cr_link) {
		cr->cr_error = error;
		wakeup(cr);
	}
	wakeup(&crdma->crdma_readcnt);
}

/*
 * Some M_PROTOnn flags are used in "args" for RDMA chunks.
 * M_PROTO11 - Indicates that the RPC gets reduced.
 *             (Read/Write/Symlink for NFS.)
 * M_PROTO10 - On an mbuf in the list (marked by a M_PROTO11) that has the
 *             reduction information, such as the pages.  This mbuf is removed
 *             from the list before being sent.
 * M_PROTO9  - This RPC needs a large rdma_reply write chunk.
 * M_PROTO8  - This RPC needs a small sized rdma_reply write chunk.
 *             (Readdir for NFS.)
 * M_PROTO7  - This RPC needs a single page rdma_reply write chunk.
 *             (Readlink for NFS.)
 * M_PROTO6  - Set in reply to indicate reduction was done.
 * Except for M_PROTO10, these flags are always set in the first mbuf in "args".
 *
 * At this time, only one read or write chunk is provided for an RPC,
 * which implies only one reduction and no rdma_reply write chunk for
 * RPCs that have a reduction.
 */
static enum clnt_stat
clnt_rdma_call(
	CLIENT		*cl,		/* client handle */
	struct rpc_callextra *ext,	/* call metadata */
	rpcproc_t	proc,		/* procedure number */
	struct mbuf	*args,		/* pointer to args */
	struct mbuf	**resultsp,	/* pointer to results */
	struct timeval	utimeout)
{
	struct crdma_data *crdma = (struct crdma_data *) cl->cl_private;
	AUTH *auth;
	struct rpc_err *errp;
	enum clnt_stat stat;
	XDR xdrs;
	struct rpc_msg call_msg;
	struct rpc_msg reply_msg;
	uint32_t chunks[3], reply_chunks[3];
	bool_t bval, falseval, ok;
	bool readcounted;
	int nrefreshes = 2;		/* number of times to refresh cred */
	struct timeval timeout;
	uint32_t xid, rdma_xid, rdma_vers, rdma_credit, rdma_body, rpos;
	uint32_t tlen, tlen2, length, handle, reduce_pos, rpchdr_len;
	uint32_t reply_max;
	uint64_t offset;
	struct mbuf *m1, *m2, *mreq, *results, *mr, *mreduce, *mreduce_prev;
	struct ct_request *cr;
	struct rpcrdma_chunk *reduce_chp, *reply_chp, *request_chp;
	int error, i, ind, reduce_ind;

	cr = malloc(sizeof(struct ct_request), M_RPC, M_WAITOK);

	ind = -1;
	reduce_chp = NULL;
	reply_chp = NULL;
	request_chp = NULL;
	reduce_pos = 0;
	reduce_ind = 0;
	mr = NULL;
	mreq = NULL;
	mreduce = NULL;
	mreduce_prev = NULL;
	readcounted = false;
	chunks[READLIST] = chunks[WRITELIST] = chunks[WRITECHUNK] = 0;
	mtx_lock(&crdma->crdma_lock);

	if (crdma->crdma_closing || crdma->crdma_closed ||
	    xprt_rdma_isdisconnected(&crdma->crdma_xp) != 0) {
		mtx_unlock(&crdma->crdma_lock);
		free(cr, M_RPC);
		RPCRDMA_DEBUG(2, "clnt_rdma_call: First closing, return "
		    "RPC_CANTSEND\n");
		return (RPC_CANTSEND);
	}
	crdma->crdma_threads++;

	if (ext) {
		auth = ext->rc_auth;
		errp = &ext->rc_err;
	} else {
		auth = cl->cl_auth;
		errp = &crdma->crdma_error;
	}

	cr->cr_mrep = NULL;
	cr->cr_error = 0;

	if (crdma->crdma_wait.tv_usec == -1) {
		timeout = utimeout;	/* use supplied timeout */
	} else {
		timeout = crdma->crdma_wait;	/* use default timeout */
	}

	/*
	 * After 15sec of looping, allow it to return RPC_CANTSEND, which will
	 * cause the clnt_reconnect layer to create a new RDMA connection.
	 */
call_again:
	mtx_assert(&crdma->crdma_lock, MA_OWNED);
	if (crdma->crdma_closing || crdma->crdma_closed) {
closing:
		xprt_rdma_mark_disconnected(&crdma->crdma_xp);
		mtx_unlock(&crdma->crdma_lock);
		if (ind >= 0)
			xprt_rdma_release_send(&crdma->crdma_xp, ind,
			    reduce_chp, reply_chp, request_chp, RPC_CANTSEND);
		mtx_lock(&crdma->crdma_lock);
		crdma->crdma_threads--;
		wakeup(crdma);
		mtx_unlock(&crdma->crdma_lock);
		free(cr, M_RPC);
		RPCRDMA_DEBUG(2, "clnt_rdma_call: Second closing, return "
		    "RPC_CANTSEND\n");
		return (RPC_CANTSEND);
	}

	crdma->crdma_xid++;
	xid = crdma->crdma_xid;

	if (ind < 0)
		ind = xprt_rdma_acquire_buf(&crdma->crdma_xp, 0,
		    crdma->crdma_xp.maxrpc);
	RPCRDMA_DEBUG(4, "clnt_rdma_call: buffer xid=0x%x ind=%d maxrpc=%d\n",
	    xid, ind, crdma->crdma_xp.maxrpc);
	if (ind < 0)
		goto closing;
	mtx_unlock(&crdma->crdma_lock);

	m2 = NULL;
	if ((args->m_flags & M_PROTO9) != 0 && reply_chp == NULL) {
		/* Create a large rdma_reply chunk, as required. */
		tlen = howmany(crdma->crdma_max_io, PAGE_SIZE);
		m1 = _rpc_create_mextpg(tlen);
		reply_chp = xprt_rdma_create_chunk(&crdma->crdma_xp, tlen,
		    NULL, m1, true, ind);
		if (reply_chp != NULL)
			chunks[WRITECHUNK] = reply_chp->num_segment;
		else
			goto cannotchunk;
		RPCRDMA_DEBUG(4, "clnt_rdma_call: Large (M_PROTO9) reply chunk"
		    " reply_chp=%p num_pg=%d\n", reply_chp, tlen);
	} else if ((args->m_flags & M_PROTO8) != 0 && reply_chp == NULL) {
		/* Create a small (Readdir) sized rdma_reply chunk. */
		tlen = howmany(crdma->crdma_small_reply, PAGE_SIZE);
		m1 = _rpc_create_mextpg(tlen);
		reply_chp = xprt_rdma_create_chunk(&crdma->crdma_xp, tlen,
		    NULL, m1, true, ind);
		if (reply_chp != NULL)
			chunks[WRITECHUNK] = reply_chp->num_segment;
		else
			goto cannotchunk;

		RPCRDMA_DEBUG(4, "clnt_rdma_call: Small (M_PROTO8) reply chunk"
		    " reply_chp=%p num_pg=%d\n", reply_chp, tlen);
	} else if ((args->m_flags & M_PROTO7) != 0 && reply_chp == NULL) {
		/* Create a single page sized rdma_reply chunk. (Readlink) */
		m1 = _rpc_create_mextpg(1);
		reply_chp = xprt_rdma_create_chunk(&crdma->crdma_xp, 1,
		    NULL, m1, true, ind);
		if (reply_chp != NULL)
			chunks[WRITECHUNK] = reply_chp->num_segment;
		else
			goto cannotchunk;
		RPCRDMA_DEBUG(4, "clnt_rdma_call: Single page (M_PROTO7) reply"
		    " chunk reply_chp=%p num_pg=%d\n", reply_chp, tlen);
	}
	if ((args->m_flags & M_PROTO11) != 0 && reduce_chp == NULL) {
		KASSERT((args->m_flags & M_PROTO10) == 0,
		    ("clnt_rdma_call: first args mbuf marked M_PROTO10"));
		RPCRDMA_DEBUG(4, "clnt_rdma_call: M_PROTO11 to be reduced\n");
		/*
		 * This RPC needs to be reduced.  The M_PROTO10 mbuf provides
		 * the information to create the chunk for reduction.
		 */
		mreduce_prev = NULL;
		for (mreduce = args; mreduce != NULL &&
		    (mreduce->m_flags & M_PROTO10) == 0;
		     mreduce = mreduce->m_next)
			mreduce_prev = mreduce;
		if (mreduce != NULL && mreduce_prev != NULL) {
			struct rpcrdma_reduce_pg *rb;

			/* Use mreduce mbuf to create chunk. */
			rb = mtod(mreduce, struct rpcrdma_reduce_pg *);
			reduce_chp = xprt_rdma_create_chunk(&crdma->crdma_xp,
			    0, rb, NULL, (rb->into_mem != 0) ? true : false,
			    ind);
			if (reduce_chp != NULL) {
				if (rb->into_mem != 0)
					reduce_ind = WRITELIST;
				else
					reduce_ind = READLIST;
				chunks[reduce_ind] = reduce_chp->num_segment;
				tlen = rb->len;
				reduce_pos = rb->pos;
			} else
				goto cannotchunk;
			RPCRDMA_DEBUG(4, "clnt_rdma_call: reduction pos=%d "
			    "len=%d into_mem=%d reduce_chp=%p\n", reduce_pos,
			    tlen, rb->into_mem, reduce_chp);
		} else
			printf("clnt_rdma_call: Bogus M_PROTO10\n");
	}

	rdma_vers = 1;
	rdma_credit = crdma->crdma_xp.maxrpc;
	rdma_body = RDMA_MSG;

	/* Wait until read cnt < maxreads. */
	mtx_lock(&crdma->crdma_lock);
	if (chunks[READLIST] > 0) {
		while (crdma->crdma_readcnt >= crdma->crdma_xp.maxreads)
			(void)msleep(&crdma->crdma_readcnt, &crdma->crdma_lock,
			    PVFS, "crmrd", hz);
		crdma->crdma_readcnt++;
		readcounted = true;
	}
	mtx_unlock(&crdma->crdma_lock);

	/*
	 * Create the RPC header now in m2, so we can size it and adjust the
	 * rdma_readlist positions.
	 */
	m2 = m_get(M_WAITOK, MT_DATA);
	m2->m_len = 0;
	xdrmbuf_create(&xdrs, m2, XDR_ENCODE);
	/*
	 * Put the RPC header in the XDR stream.
	 */
	call_msg.rm_xid = xid;
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = crdma->crdma_prog;
	call_msg.rm_call.cb_vers = crdma->crdma_vers;
	if (! xdr_callhdr(&xdrs, &call_msg))
		goto cannotencode;
	/* Keep the caller's metadata linked on every failure and retry. */
	if (mreduce != NULL && mreduce_prev != NULL)
		mreduce_prev->m_next = mreduce->m_next;
	m1 = m_copym(args, 0, M_COPYALL, M_WAITOK);
	if (mreduce != NULL && mreduce_prev != NULL) {
		/* Put mreduce back in args list so that a retry will work. */
		mreduce->m_next = mreduce_prev->m_next;
		mreduce_prev->m_next = mreduce;
	}
	if (m1 == NULL)
		goto cannotencode;

	rpchdr_len = m_length(m1, NULL);   /* Set to length of the NFS args. */
	if ((! XDR_PUTINT32(&xdrs, &proc)) ||
	    (! AUTH_MARSHALL(auth, xid, &xdrs, m1))) {
cannotencode:
		if (m2)
			m_freem(m2);
		m2 = NULL;
		if (mreq)
			m_freem(mreq);
		mreq = NULL;
		errp->re_status = stat = RPC_CANTENCODEARGS;
		RPCRDMA_DEBUG(1, "xprt_rdma_cal: cannot encode args\n");
		mtx_lock(&crdma->crdma_lock);
		goto out;
	}
	tlen2 = m_length(m2, NULL);
	rpchdr_len = tlen2 - rpchdr_len;   /* Subtract out the NFS args len. */
	RPCRDMA_DEBUG(4, "clnt_rdma_call: RPC header length=%d\n", tlen2);
	tlen2 += RPCRDMA_HEADER_LEN(chunks[READLIST], chunks[WRITELIST],
	    chunks[WRITECHUNK]);
	RPCRDMA_DEBUG(4, "clnt_rdma_call: RPC header plus RDMA header "
	    "length=%d\n", tlen2);

	/*
	 * If the RPC request message is too large
	 * create a large enough buffer and copy the mbuf
	 * list into it.
	 */
	if (rdma_body == RDMA_MSG && tlen2 > RPCRDMA_MAX_INLINE) {
		struct rpcrdma_reduce_pg *rb;

		KASSERT(request_chp == NULL, ("clnt_rdma_call: request_chp not "
		    "NULL"));
		tlen2 -= RPCRDMA_HEADER_LEN(chunks[READLIST], chunks[WRITELIST],
		    chunks[WRITECHUNK]);
		if (reduce_chp != NULL) {
			printf("clnt_rdma_call: Request msg too big with chunk"
			    " %d\n", tlen2);
			goto cannotencode;
		}
		RPCRDMA_DEBUG(4, "clnt_rdma_call: Large request len=%d\n",
		    tlen2);
		mr = rpc_reduce_pg(NULL, tlen2, 0, false);
		rb = mtod(mr, struct rpcrdma_reduce_pg *);
		request_chp = xprt_rdma_create_chunk(&crdma->crdma_xp,
		    0, rb, NULL, false, ind);
		if (request_chp != NULL) {
			rpc_copy_mbuf_to_rb(m2, rb);
			m_freem(m2);
			m2 = NULL;
			chunks[READLIST] = request_chp->num_segment;
			rdma_body = RDMA_NOMSG;
			RPCRDMA_DEBUG(4, "clnt_rdma_call: Got request_chp, "
			    "num_segment=%d\n", request_chp->num_segment);
		} else {
cannotchunk:
			if (m2)
				m_freem(m2);
			m2 = NULL;
			errp->re_status = stat = RPC_CANTSEND;
			RPCRDMA_DEBUG(1, "xprt_rdma_cal: cannot create "
			    "chunk\n");
			mtx_lock(&crdma->crdma_lock);
			goto out;
		}
		RPCRDMA_DEBUG(1, "clnt_rdma_call: Creating large RPC request "
		    "chunk length=%d request_chp=%p\n", tlen2, request_chp);
	}

	mreq = m_gethdr(M_WAITOK, MT_DATA);
	mreq->m_len = 0;
	xdrmbuf_create(&xdrs, mreq, XDR_ENCODE);

	errp->re_status = stat = RPC_SUCCESS;

	/*
	 * Put the RDMA header in the XDR stream.
	 */
	falseval = FALSE;
	bval = (chunks[READLIST] > 0) ? TRUE : FALSE;
	if ((! XDR_PUTINT32(&xdrs, &xid)) ||
	    (! XDR_PUTINT32(&xdrs, &rdma_vers)) ||
	    (! XDR_PUTINT32(&xdrs, &rdma_credit)) ||
	    (! XDR_PUTINT32(&xdrs, &rdma_body)) ||
	    (! XDR_PUTINT32(&xdrs, &bval)))
		goto cannotencode;
	RPCRDMA_DEBUG(4, "clnt_rdma_call: xid=0x%x vers=%d credits=%d "
	    "rdma_body=%d readlist=%d\n", xid, rdma_vers, rdma_credit,
	    rdma_body, bval);
	if (bval) {
		struct rpcrdma_chunk *tchp;

		KASSERT(reduce_pos > 0 || rdma_body == RDMA_NOMSG,
		    ("clnt_rdma_call: reduce_pos zero for RDMA_MSG"));
		rpos = reduce_pos + rpchdr_len;
		tchp = reduce_chp;
		if (rdma_body == RDMA_NOMSG) {
			rpos = 0;	/* 0 for a long call message. */
			tchp = request_chp;
		}
		bval = TRUE;
		for (i = 0; i < chunks[READLIST]; i++) {
			if (i == chunks[READLIST] - 1)
				bval = FALSE;
			if ((! XDR_PUTINT32(&xdrs, &rpos)) ||
			    (! XDR_PUTINT32(&xdrs, &tchp->handle[i])) ||
			    (! XDR_PUTINT32(&xdrs, &tchp->length[i])) ||
			    (! xdr_uint64_t(&xdrs, &tchp->offset[i])) ||
			    (! XDR_PUTINT32(&xdrs, &bval)))
				goto cannotencode;
			RPCRDMA_DEBUG(4, "clnt_rdma_call: readlist chunk "
			    "handle=0x%x length=%d offset=%ju numseg=%d\n",
			    tchp->handle[i], tchp->length[i],
			    (uintmax_t)tchp->offset[i], chunks[READLIST]);
		}
		RPCRDMA_DEBUG(2, "clnt_rdma_call: Readlist rdma_body=%d "
		    "numseg=%d\n", rdma_body, chunks[READLIST]);
	}
	bval = (chunks[WRITELIST] > 0) ? TRUE : FALSE;
	if (bval) {
		if ((! XDR_PUTINT32(&xdrs, &bval)) ||
		    (! XDR_PUTINT32(&xdrs, &chunks[WRITELIST])))
			goto cannotencode;
		for (i = 0; i < chunks[WRITELIST]; i++) {
			if ((! XDR_PUTINT32(&xdrs, &reduce_chp->handle[i])) ||
			    (! XDR_PUTINT32(&xdrs, &reduce_chp->length[i])) ||
			    (! xdr_uint64_t(&xdrs, &reduce_chp->offset[i])))
				goto cannotencode;
			RPCRDMA_DEBUG(4, "clnt_rdma_call: writelist chunk "
			    "handle=0x%x length=%d offset=%ju numseg=%d\n",
			    reduce_chp->handle[i], reduce_chp->length[i],
			    (uintmax_t)reduce_chp->offset[i],
			    chunks[WRITELIST]);
		}
		if (! XDR_PUTINT32(&xdrs, &falseval))
			goto cannotencode;
		RPCRDMA_DEBUG(2, "clnt_rdma_call: Writelist numseg=%d\n",
		    chunks[WRITELIST]);
	} else {
		if (! XDR_PUTINT32(&xdrs, &bval))
			goto cannotencode;
	}
	bval = (chunks[WRITECHUNK] > 0) ? TRUE : FALSE;
	if (bval) {
		if ((! XDR_PUTINT32(&xdrs, &bval)) ||
		    (! XDR_PUTINT32(&xdrs, &chunks[WRITECHUNK])))
			goto cannotencode;
		for (i = 0; i < chunks[WRITECHUNK]; i++) {
			if ((! XDR_PUTINT32(&xdrs, &reply_chp->handle[i])) ||
			    (! XDR_PUTINT32(&xdrs, &reply_chp->length[i])) ||
			    (! xdr_uint64_t(&xdrs, &reply_chp->offset[i])))
				goto cannotencode;
			RPCRDMA_DEBUG(4, "clnt_rdma_call: replylist chunk "
			    "handle=0x%x length=%d offset=%ju numseg=%d\n",
			    reply_chp->handle[i], reply_chp->length[i],
			    (uintmax_t)reply_chp->offset[i],
			    chunks[WRITECHUNK]);
		}
		RPCRDMA_DEBUG(2, "clnt_rdma_call: Replylist numseg=%d\n",
		    chunks[WRITECHUNK]);
	} else {
		if (! XDR_PUTINT32(&xdrs, &bval))
			goto cannotencode;
	}
	RPCRDMA_DEBUG(1, "clnt_rdma_call: RDMA header len=%d rdma_body=%d\n",
	    m_length(mreq, NULL), rdma_body);
	if (rdma_body == RDMA_MSG) {
		if (! XDR_PUTMBUF(&xdrs, m2))
			goto cannotencode;
		m2 = NULL;
	}
	mreq->m_pkthdr.len = m_length(mreq, NULL);
	RPCRDMA_DEBUG(1, "clnt_rdma_call: RDMA header plus RPC message "
	    "len=%d\n", mreq->m_pkthdr.len);

	cr->cr_xid = xid;
	mtx_lock(&crdma->crdma_lock);
	/*
	 * Check to see if the other end has already started to close down
	 * the connection. The upcall will have set crdma_error.re_status
	 * to RPC_CANTRECV if this is the case.
	 * If the other end starts to close down the connection after this
	 * point, it will be detected later when cr_error is checked,
	 * since the request is in the crdma_pending queue.
	 */
	if (crdma->crdma_error.re_status == RPC_CANTRECV) {
		if (errp != &crdma->crdma_error) {
			errp->re_errno = crdma->crdma_error.re_errno;
			errp->re_status = RPC_CANTRECV;
		}
		stat = RPC_CANTRECV;
		RPCRDMA_DEBUG(1, "clnt_rdma_call: return RPC_CANTRECV\n");
		goto out;
	}

	TAILQ_INSERT_TAIL(&crdma->crdma_pending, cr, cr_link);
	mtx_unlock(&crdma->crdma_lock);

	/*
	 * Send the message on the RDMA payload stream.
	 */
	error = xprt_rdma_send(&crdma->crdma_xp, mreq, ind);
	RPCRDMA_DEBUG(1, "clnt_rdma_call: xprt_rdma_send index=%d error=%d\n",
	    ind, error);
	mreq = NULL;
	reply_msg.acpted_rply.ar_verf.oa_flavor = AUTH_NULL;
	reply_msg.acpted_rply.ar_verf.oa_base = cr->cr_verf;
	reply_msg.acpted_rply.ar_verf.oa_length = 0;
	reply_msg.acpted_rply.ar_results.where = NULL;
	reply_msg.acpted_rply.ar_results.proc = (xdrproc_t)xdr_void;

	mtx_lock(&crdma->crdma_lock);
	if (error) {
		TAILQ_REMOVE(&crdma->crdma_pending, cr, cr_link);
		errp->re_errno = error;
		errp->re_status = stat = RPC_CANTSEND;
		RPCRDMA_DEBUG(1, "clnt_rdma_call: Third RPC_CANTSEND\n");
		goto out;
	}

	/*
	 * Check to see if we got an upcall while waiting for the
	 * lock. In both these cases, the request has been removed
	 * from crdma->crdma_pending.
	 */
	if (cr->cr_error) {
		TAILQ_REMOVE(&crdma->crdma_pending, cr, cr_link);
		errp->re_errno = cr->cr_error;
		RPCRDMA_DEBUG(1, "clnt_rdma_call: First RPC_CANTRECV\n");
		errp->re_status = stat = RPC_CANTRECV;
		goto out;
	}
	if (cr->cr_mrep) {
		TAILQ_REMOVE(&crdma->crdma_pending, cr, cr_link);
		RPCRDMA_DEBUG(1, "clnt_rdma_call: Got reply to RPC\n");
		goto got_reply;
	}

	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		TAILQ_REMOVE(&crdma->crdma_pending, cr, cr_link);
		errp->re_status = stat = RPC_TIMEDOUT;
		RPCRDMA_DEBUG(1, "clnt_rdma_call: Weird zero timeout\n");
		goto out;
	}

	error = msleep(cr, &crdma->crdma_lock, crdma->crdma_waitflag,
	    crdma->crdma_waitchan, tvtohz(&timeout));
	RPCRDMA_DEBUG(1, "clnt_rdma_call: msleep=%d\n", error);

	TAILQ_REMOVE(&crdma->crdma_pending, cr, cr_link);

	if (error) {
		/*
		 * The sleep returned an error so our request is still
		 * on the list. Turn the error code into an
		 * appropriate client status.
		 */
		errp->re_errno = error;
		switch (error) {
		case EINTR:
			stat = RPC_INTR;
			break;
		case EWOULDBLOCK:
			stat = RPC_TIMEDOUT;
			break;
		default:
			stat = RPC_CANTRECV;
		}
		errp->re_status = stat;
		RPCRDMA_DEBUG(1, "clnt_rdma_call: msleep error return "
		    "stat=%d\n", stat);
		goto out;
	} else {
		/*
		 * We were woken up by the upcall.  If the
		 * upcall had a receive error, report that,
		 * otherwise we have a reply.
		 */
		if (cr->cr_error) {
			errp->re_errno = cr->cr_error;
			errp->re_status = stat = RPC_CANTRECV;
			RPCRDMA_DEBUG(1, "clnt_rdma_call: Upcall recv=%d, "
			    "second RPC_CANTRECV\n", errp->re_errno);
			goto out;
		}
	}

got_reply:
	/*
	 * Now decode and validate the response. We need to drop the
	 * lock since xdr_replymsg may end up sleeping in malloc.
	 */
	mtx_unlock(&crdma->crdma_lock);

	if (ext && ext->rc_feedback)
		ext->rc_feedback(FEEDBACK_OK, proc, ext->rc_feedback_arg);

	xdrmbuf_create(&xdrs, cr->cr_mrep, XDR_DECODE);
	cr->cr_mrep = NULL;

	/* Decode the RDMA header. */
	if ((! XDR_GETINT32(&xdrs, &rdma_xid)) ||
	    (! XDR_GETINT32(&xdrs, &rdma_vers)) ||
	    (! XDR_GETINT32(&xdrs, &rdma_credit)) ||
	    (! XDR_GETINT32(&xdrs, &rdma_body)))
		goto cannotdecode;
	RPCRDMA_DEBUG(2, "clnt_rdma_call: Got reply RDMA header xid=0x%x "
	    "vers=%d rdma_credit=%d rdma_body=%d\n", rdma_xid, rdma_vers,
	    rdma_credit, rdma_body);
	if (xid != rdma_xid || rdma_vers != 1) {
		printf("clnt_rdma_call: xid=%u rdma_xid=%u rdma_vers=%d\n",
		    xid, rdma_xid, rdma_vers);
		goto cannotdecode;
	}
	if (rdma_credit == 0) {
		/*
		 * I do not think this should ever happen, but Linux just
		 * sets the credit to 1 for this case, so we'll do the
		 * same.
		 */
		rdma_credit = 1;
	}
	mtx_lock(&crdma->crdma_lock);
	crdma->crdma_xp.credits = rdma_credit;
	mtx_unlock(&crdma->crdma_lock);
	reply_max = reply_chp != NULL ?
	    m_length(reply_chp->mextpg, NULL) : 0;
	reply_chunks[READLIST] = chunks[READLIST];
	reply_chunks[WRITELIST] = chunks[WRITELIST];
	reply_chunks[WRITECHUNK] = chunks[WRITECHUNK];
	if (rdma_body == RDMA_MSG || rdma_body == RDMA_NOMSG) {
		/* Check the 3 chunks in the reply. */
		for (i = 0; i < 3; i++) {
			tlen = 0;
			if (! XDR_GETINT32(&xdrs, &bval))
				goto cannotdecode;
			if (bval) {
				RPCRDMA_DEBUG(4, "clnt_rdma_call: Got chunk "
				    "in reply, type=%d\n", i);
				if (chunks[i] == 0) {
					printf("clnt_rdma_call: chunks%d not "
					    "set\n", i);
					goto cannotdecode;
				}
				/*
				 * Set reply_chunks[i] to 0 to indicate used.
				 */
				reply_chunks[i] = 0;

				rpos = 0;
				/*
				 * For i > 0, rpos is the count of segments
				 * in the xdr_write_chunk.
				 */
				if (i > 0) {
					if (! XDR_GETINT32(&xdrs, &rpos)) {
						printf("clnt_rdma_call: No "
						    "chunk cnt\n");
						goto cannotdecode;
					}
					if (rpos < 1 ||
					    rpos > RPCRDMA_MAX_SEGMENTS) {
						printf("clnt_rdma_call: Bad "
						    "write chunk cnt\n");
						goto cannotdecode;
					}
				}
				while (bval && ((rpos == 0 && i == 0) ||
				    (rpos > 0 && i > 0))) {
					if (i == 0 &&
					    (! XDR_GETINT32(&xdrs, &rpos))) {
						printf("clnt_rdma_call: No "
						    "chunk pos\n");
						goto cannotdecode;
					}
					if ((! XDR_GETINT32(&xdrs, &handle)) ||
					    (! XDR_GETINT32(&xdrs, &length)) ||
					    (! xdr_int64_t(&xdrs, &offset))) {
						printf("clnt_rdma_call: chunk "
						    "bogus\n");
						goto cannotdecode;
					}
					if (i == WRITECHUNK &&
					    rdma_body == RDMA_NOMSG &&
					    length > reply_max - tlen)
						goto cannotdecode;
					tlen += length;
					RPCRDMA_DEBUG(4, "clnt_rdma_call: Got "
					    "segment handle=0x%x length=%d "
					    "offset=%ju\n", handle, length,
					    (uintmax_t)offset);
					if (i > 0)
						rpos--;
					if (i == 0 || (i == 1 && rpos == 0)) {
						if (!XDR_GETINT32(&xdrs, &bval))
							goto cannotdecode;
					}
				}
				if (reduce_chp == NULL && reply_chp == NULL) {
					printf("clnt_rdma_call: No chunk\n");
					goto cannotdecode;
				}
			}
		}
		if (rdma_body == RDMA_NOMSG) {
			struct mbuf *mextpg;

			if (reply_chp == NULL || tlen == 0) {
				printf("clnt_rdma_call: Bad reply chunk chp=%p"
				    " len=%d\n", reply_chp, tlen);
				goto cannotdecode;
			}
			RPCRDMA_DEBUG(2, "clnt_rdma_call: Got RDMA_NOMSG for "
			    "large reply, length=%d\n", tlen);
			mextpg = reply_chp->mextpg;
			xprt_rdma_unmap_chunk(&crdma->crdma_xp, reply_chp);
			reply_chp = NULL;
			_rpc_trim_mextpg(mextpg, tlen);

			/*
			 * Until nfsm_dissect() and friends know how to handle
			 * an M_EXTPG mbuf list, we need a cluster copy.
			 */
			error = mb_unmapped_to_ext(mextpg, &m1);
			RPCRDMA_DEBUG(4, "clnt_rdma_call: mb_unmapped_to_ext"
			    " error=%d\n", error);
			if (error != 0)
				goto cannotdecode;
			RPCRDMA_DEBUG(4, "clnt_rdma_call: length=%d mbuf "
			    "len=%d\n", tlen, m_length(m1, NULL));

			XDR_DESTROY(&xdrs);
			xdrmbuf_create(&xdrs, m1, XDR_DECODE);
		}
	} else if (rdma_body == RDMA_ERROR) {
		RPCRDMA_DEBUG(1, "clnt_rdma_call: Got RDMA_ERROR reply\n");
		if ((! XDR_GETINT32(&xdrs, &rpos)))
			goto cannotdecode;
		if (rpos == RDMA_ERR_VERS) {
			if ((! XDR_GETINT32(&xdrs, &rpos)) ||
			    (! XDR_GETINT32(&xdrs, &rpos)))
				goto cannotdecode;
			printf("clnt_rdma_call: Wrong RDMA verion\n");
			goto cannotdecode;
		} else if (rpos == RDMA_ERR_CHUNK) {
			printf("clnt_rdma_call: ERR_CHUNK\n");
			clnt_rdma_upcall(&crdma->crdma_xp, NULL);
			goto cannotdecode;
		}
	}

	ok = xdr_replymsg(&xdrs, &reply_msg);
	if (ok) {
		if ((reply_msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
		    (reply_msg.acpted_rply.ar_stat == SUCCESS)) {
			errp->re_status = stat = RPC_SUCCESS;
		} else {
			stat = _seterr_reply(&reply_msg, errp);
			if (stat == RPC_SYSTEMERROR || stat == RPC_FAILED)
				stat = RPC_CANTRECV;	/* Make it retry. */
		}

		if (stat == RPC_SUCCESS) {
			results = xdrmbuf_getall(&xdrs);
			if (!AUTH_VALIDATE(auth, xid,
				&reply_msg.acpted_rply.ar_verf,
				&results)) {
				errp->re_status = stat = RPC_AUTHERROR;
				errp->re_why = AUTH_INVALIDRESP;
				RPCRDMA_DEBUG(1, "clnt_rdma_call: Auth err, "
				    "invalid response\n");
			} else {
				KASSERT(results,
				    ("auth validated but no result"));
				if (reduce_chp != NULL &&
				    reply_chunks[reduce_ind] == 0)
					results->m_flags |= M_PROTO6;
				*resultsp = results;
				RPCRDMA_DEBUG(1, "clnt_rdma_call: Got RPC reply"
				    " (NFS) : reduce_chp=%p reduced=%d\n",
				    reduce_chp, reply_chunks[reduce_ind]);
			}
		}		/* end successful completion */
		/*
		 * If unsuccessful AND error is an authentication error
		 * then refresh credentials and try again, else break
		 */
		else if (stat == RPC_AUTHERROR)
			/* maybe our credentials need to be refreshed ... */
			if (nrefreshes > 0 &&
			    AUTH_REFRESH(auth, &reply_msg)) {
				nrefreshes--;
				XDR_DESTROY(&xdrs);
				if (request_chp != NULL) {
					KASSERT(mr != NULL, ("clnt_rdma_call: "
					    "request_chp, but no mr"));
					xprt_rdma_unmap_chunk(&crdma->crdma_xp,
					    request_chp);
					request_chp = NULL;
					chunks[READLIST] = 0;
					rpc_free_rdma_reduction(mr);
					mr = NULL;
				}
				RPCRDMA_DEBUG(4, "clnt_rdma_call: Auth retry "
				    "xid=0x%x\n", xid);
				mtx_lock(&crdma->crdma_lock);
				if (readcounted) {
					crdma->crdma_readcnt--;
					readcounted = false;
					wakeup(&crdma->crdma_readcnt);
				}
				goto call_again;
			}
		/* end of unsuccessful completion */
	}	/* end of valid reply message */
	else {
cannotdecode:
		RPCRDMA_DEBUG(1, "clnt_rdma_call: RPC_CANTDECODERES\n");
		errp->re_status = stat = RPC_CANTDECODERES;
	}
	/* Done with the Send/Receive buffer, so release it. */
	xprt_rdma_release_send(&crdma->crdma_xp, ind, reduce_chp, reply_chp,
	    request_chp, stat);
	RPCRDMA_DEBUG(1, "clnt_rdma_call: xprt_rdma_release send done\n");
	ind = -1;
	XDR_DESTROY(&xdrs);
	mtx_lock(&crdma->crdma_lock);
out:
	mtx_assert(&crdma->crdma_lock, MA_OWNED);

	if (readcounted) {
		crdma->crdma_readcnt--;
		wakeup(&crdma->crdma_readcnt);
	}

	KASSERT(stat != RPC_SUCCESS || *resultsp,
	    ("RPC_SUCCESS without reply"));

	if (mreq)
		m_freem(mreq);
	if (cr->cr_mrep)
		m_freem(cr->cr_mrep);

	if (stat == RPC_CANTSEND || stat == RPC_CANTRECV) {
		clnt_rdma_fail_locked(crdma, ECONNRESET);
		args->m_flags |= M_PROTO4;
		printf("set M_PROTO4: xid=0x%x rlch=%d wlch=%d rpch=%d\n", xid,
		    chunks[READLIST], chunks[WRITELIST], chunks[WRITECHUNK]);
	} else if ((args->m_flags & M_PROTO4) != 0) {
		printf("retry stat=%d  M_PROTO4: xid=0x%x rlch=%d wlch=%d "
		    "rpch=%d\n", stat, xid, chunks[READLIST], chunks[WRITELIST],
		    chunks[WRITECHUNK]);
	}

	mtx_unlock(&crdma->crdma_lock);

	if (ind >= 0)
		xprt_rdma_release_send(&crdma->crdma_xp, ind, reduce_chp,
		    reply_chp, request_chp, stat);
	if (mr)
		rpc_free_rdma_reduction(mr);

	if (auth && stat != RPC_SUCCESS)
		AUTH_VALIDATE(auth, xid, NULL, NULL);
	mtx_lock(&crdma->crdma_lock);
	crdma->crdma_threads--;
	if (crdma->crdma_closing)
		wakeup(crdma);
	mtx_unlock(&crdma->crdma_lock);

	free(cr, M_RPC);

	RPCRDMA_DEBUG(1, "clnt_rdma_call: Returning stat=%d\n", stat);
	return (stat);
}

static void
clnt_rdma_geterr(CLIENT *cl, struct rpc_err *errp)
{
	struct crdma_data *crdma = (struct crdma_data *) cl->cl_private;

	*errp = crdma->crdma_error;
}

static bool_t
clnt_rdma_freeres(CLIENT *cl, xdrproc_t xdr_res, void *res_ptr)
{
	XDR xdrs;
	bool_t dummy;

	xdrs.x_op = XDR_FREE;
	dummy = (*xdr_res)(&xdrs, res_ptr);

	return (dummy);
}

/*ARGSUSED*/
static void
clnt_rdma_abort(CLIENT *cl)
{
}

static bool_t
clnt_rdma_control(CLIENT *cl, u_int request, void *info)
{
	struct crdma_data *crdma = (struct crdma_data *)cl->cl_private;
	void *infop = info;
	uint32_t max_io;
	SVCXPRT *xprt;

	mtx_lock(&crdma->crdma_lock);

	switch (request) {
	case CLSET_FD_CLOSE:
		crdma->crdma_closeit = TRUE;
		mtx_unlock(&crdma->crdma_lock);
		return (TRUE);
	case CLSET_FD_NCLOSE:
		crdma->crdma_closeit = FALSE;
		mtx_unlock(&crdma->crdma_lock);
		return (TRUE);
	default:
		break;
	}

	/* for other requests which use info */
	if (info == NULL) {
		mtx_unlock(&crdma->crdma_lock);
		return (FALSE);
	}
	switch (request) {
	case CLSET_TIMEOUT:
		if (time_not_ok((struct timeval *)info)) {
			mtx_unlock(&crdma->crdma_lock);
			return (FALSE);
		}
		crdma->crdma_wait = *(struct timeval *)infop;
		break;
	case CLGET_TIMEOUT:
		*(struct timeval *)infop = crdma->crdma_wait;
		break;
	case CLGET_SERVER_ADDR:
		memcpy(info, &crdma->crdma_addr, crdma->crdma_addr.ss_len);
		break;
	case CLGET_SVC_ADDR:
		/*
		 * Slightly different semantics to userland - we use
		 * sockaddr instead of netbuf.
		 */
		memcpy(info, &crdma->crdma_addr, crdma->crdma_addr.ss_len);
		break;
	case CLSET_SVC_ADDR:		/* set to new address */
		mtx_unlock(&crdma->crdma_lock);
		return (FALSE);
	case CLGET_XID:
		*(uint32_t *)info = crdma->crdma_xid;
		break;
	case CLSET_XID:
		/* This will set the xid of the NEXT call */
		/* decrement by 1 as clnt_rdma_call() increments once */
		crdma->crdma_xid = *(uint32_t *)info - 1;
		break;
	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * beginning of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		*(uint32_t *)info =
		    ntohl(*(uint32_t *)(void *)(crdma->crdma_mcallc +
		    4 * BYTES_PER_XDR_UNIT));
		break;
	case CLSET_VERS:
		*(uint32_t *)(void *)(crdma->crdma_mcallc +
		    4 * BYTES_PER_XDR_UNIT) =
		    htonl(*(uint32_t *)info);
		break;
	case CLGET_PROG:
		/*
		 * This RELIES on the information that, in the call body,
		 * the program number field is the fourth field from the
		 * beginning of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		*(uint32_t *)info =
		    ntohl(*(uint32_t *)(void *)(crdma->crdma_mcallc +
		    3 * BYTES_PER_XDR_UNIT));
		break;
	case CLSET_PROG:
		*(uint32_t *)(void *)(crdma->crdma_mcallc +
		    3 * BYTES_PER_XDR_UNIT) =
		    htonl(*(uint32_t *)info);
		break;
	case CLSET_WAITCHAN:
		crdma->crdma_waitchan = (const char *)info;
		break;
	case CLGET_WAITCHAN:
		*(const char **) info = crdma->crdma_waitchan;
		break;
	case CLSET_INTERRUPTIBLE:
		if (*(int *) info)
			crdma->crdma_waitflag = PCATCH;
		else
			crdma->crdma_waitflag = 0;
		break;
	case CLGET_INTERRUPTIBLE:
		if (crdma->crdma_waitflag)
			*(int *) info = TRUE;
		else
			*(int *) info = FALSE;
		break;
	case CLSET_RDMAMAX_IO:
		max_io = *(uint32_t *)info;
		crdma->crdma_max_io = round_page(max_io + 2048);
		break;
	case CLGET_RDMAMAX_IO:
		*(uint32_t *)info = crdma->crdma_xp.maxio;
		break;
	case CLSET_BACKCHANNEL:
		xprt = (SVCXPRT *)info;
		if (crdma->crdma_backchannelxprt == NULL) {
			SVC_ACQUIRE(xprt);
			xprt->xp_p2 = crdma;
			crdma->crdma_backchannelxprt = xprt;
		}
		break;
	default:
		mtx_unlock(&crdma->crdma_lock);
		return (FALSE);
	}

	mtx_unlock(&crdma->crdma_lock);
	return (TRUE);
}

static void
clnt_rdma_close(CLIENT *cl)
{
	struct crdma_data *crdma = (struct crdma_data *) cl->cl_private;
	struct ct_request *cr;

	mtx_lock(&crdma->crdma_lock);

	if (crdma->crdma_closed) {
		mtx_unlock(&crdma->crdma_lock);
		return;
	}

	if (crdma->crdma_closing) {
		while (crdma->crdma_closing)
			(void)msleep(crdma, &crdma->crdma_lock, PVFS, "rdmacl",
			    0);
		KASSERT(crdma->crdma_closed, ("client should be closed"));
		mtx_unlock(&crdma->crdma_lock);
		return;
	}

	crdma->crdma_closing = TRUE;
	xprt_rdma_mark_disconnected(&crdma->crdma_xp);

	/* Wait for upcalls to be done. */
	while (crdma->crdma_upcallrefs > 0)
		(void)msleep(&crdma->crdma_upcallrefs, &crdma->crdma_lock,
		    PVFS, "rdmaup", 0);

	/*
	 * Abort any pending requests and wait until everyone
	 * has finished with clnt_rdma_call.
	 */
	TAILQ_FOREACH(cr, &crdma->crdma_pending, cr_link) {
		cr->cr_xid = 0;
		cr->cr_error = ESHUTDOWN;
		wakeup(cr);
	}

	while (crdma->crdma_threads)
		msleep(crdma, &crdma->crdma_lock, PVFS, "rdmacl2", 0);

	mtx_unlock(&crdma->crdma_lock);
	RPCRDMA_DEBUG(1, "clnt_rdma_close: Close of connection at "
	    "disconnect\n");
	xprt_rdma_disconnect(&crdma->crdma_xp);
	mtx_lock(&crdma->crdma_lock);
	crdma->crdma_closing = FALSE;
	crdma->crdma_closed = TRUE;
	wakeup(crdma);
	mtx_unlock(&crdma->crdma_lock);
}

static void
clnt_rdma_destroy(CLIENT *cl)
{
	struct crdma_data *crdma = (struct crdma_data *) cl->cl_private;
	SVCXPRT *xprt;
	bool_t rlse;

	clnt_rdma_close(cl);

	mtx_lock(&crdma->crdma_lock);
	xprt = crdma->crdma_backchannelxprt;
	crdma->crdma_backchannelxprt = NULL;
	if (xprt != NULL) {
		mtx_unlock(&crdma->crdma_lock);	/* To avoid a LOR. */
		rlse = FALSE;
		sx_xlock(&xprt->xp_lock);
		mtx_lock(&crdma->crdma_lock);
		if (xprt->xp_p2 == crdma) {
			xprt->xp_p2 = NULL;
			rlse = TRUE;
		}
		sx_xunlock(&xprt->xp_lock);
		if (rlse)
			SVC_RELEASE(xprt);
	}
	mtx_unlock(&crdma->crdma_lock);

	RPCRDMA_DEBUG(1, "clnt_rdma_destroy: after clnt_rdma_close() call\n");
	mtx_destroy(&crdma->crdma_lock);

	if (crdma->crdma_xp.ep != NULL)
		free(crdma->crdma_xp.ep, M_RPCRDMA);
	m_freem(crdma->crdma_record);
	m_freem(crdma->crdma_raw);
	mem_free(crdma, sizeof(struct crdma_data));
	if (cl->cl_netid && cl->cl_netid[0])
		mem_free(cl->cl_netid, strlen(cl->cl_netid) +1);
	if (cl->cl_tp && cl->cl_tp[0])
		mem_free(cl->cl_tp, strlen(cl->cl_tp) +1);
	mem_free(cl, sizeof(CLIENT));
}

/*
 * Make sure that the time is not garbage.   -1 value is disallowed.
 * Note this is different from time_not_ok in clnt_dg.c
 */
static bool_t
time_not_ok(struct timeval *t)
{
	return (t->tv_sec <= -1 || t->tv_sec > 100000000 ||
		t->tv_usec <= -1 || t->tv_usec > 1000000);
}

/* First 9 words of an RDMA message with no chunks. */
struct first_rdma {
	uint32_t	xid;
	uint32_t	vers;
	uint32_t	credits;
	uint32_t	body;
	uint32_t	readlist;
	uint32_t	writelist;
	uint32_t	reply;
	uint32_t	rpcxid;
	uint32_t	call;
};

static void
clnt_rdma_upcall(struct rpcrdma_xprt *xp, struct mbuf *m)
{
	struct first_rdma first_rdma;
	struct ct_request *cr;
	int error, foundreq;
	u_int rawlen;
	struct crdma_data *crdma;
	struct mbuf *m2;
	SVCXPRT *xprt;
	struct cf_conn *cd;

	RPCRDMA_DEBUG(1, "clnt_rdma_upcall: Recv upcall m=%p\n", m);
	crdma = (struct crdma_data *)__containerof((void *)xp,
	    struct crdma_data, crdma_xp);

	/* Put the RPC message onto crdma_raw. */
	error = 0;

	mtx_lock(&crdma->crdma_lock);
	crdma->crdma_upcallrefs++;
	if (m != NULL) {
		if (crdma->crdma_raw != NULL)
			m_last(crdma->crdma_raw)->m_next = m;
		else
			crdma->crdma_raw = m;
		rawlen = m_length(crdma->crdma_raw, NULL);
	} else {
		/*
		 * We must have got EOF trying
		 * to read from the stream.
		 */
		error = ECONNRESET;
		RPCRDMA_DEBUG(1, "clnt_rdma_upcall: Connection reset\n");
	}

	/* Now, process as much of crdma_raw as possible. */
	for (; error == 0;) {
		/*
		 * Move a record, if possible.
		 */
		if (rawlen == 0)
			break;
		if (crdma->crdma_record != NULL)
			m_last(crdma->crdma_record)->m_next =
			    crdma->crdma_raw;
		else
			crdma->crdma_record = crdma->crdma_raw;
		RPCRDMA_DEBUG(2, "clnt_rdma_call: Received a record\n");
		crdma->crdma_raw = NULL;
		rawlen = 0;

		/*
		 * There are always more than 9 words in an
		 * RDMA message.  For callbacks, I assume
		 * that there are no chunks, so the RDMA
		 * header is 7 words + the first two words
		 * of the RPC header.
		 */
		if (crdma->crdma_record->m_len < sizeof(first_rdma) &&
		    m_length(crdma->crdma_record, NULL) < sizeof(first_rdma)) {
			/*
			 * What to do now?
			 * The data in the RDMA stream is
			 * corrupted such that there is no
			 * valid RPC message to parse.
			 * I think it best to close this
			 * connection and allow
			 * clnt_reconnect_call() to try
			 * and establish a new one.
			 */
			printf("clnt_rdma_upcall: "
			    "connection data corrupted\n");
			error = ECONNRESET;
			goto wakeup_all;
		}
		m_copydata(crdma->crdma_record, 0, sizeof(first_rdma),
		    (char *)&first_rdma);
		first_rdma.xid = ntohl(first_rdma.xid);
		first_rdma.call = ntohl(first_rdma.call);
		/*
		 * RDMA_MSG and FALSE are both 0, so ntohl()
		 * is not needed for the comparisons.
		 * Check message direction.
		 */
		if (first_rdma.body == RDMA_MSG &&
		    first_rdma.readlist == FALSE &&
		    first_rdma.writelist == FALSE &&
		    first_rdma.reply == FALSE &&
		    first_rdma.call == CALL) {
			/*
			 * This is a backchannel request.
			 * Strip off the RDMA header.
			 */
			m_adj(crdma->crdma_record, 7 * BYTES_PER_XDR_UNIT);
			xprt = crdma->crdma_backchannelxprt;
			if (xprt == NULL) {
				/* Just throw it away. */
				m_freem(crdma->crdma_record);
				crdma->crdma_record = NULL;
			} else {
				cd = (struct cf_conn *)xprt->xp_p1;
				m2 = cd->mreq;
				/*
				 * The requests are chained
				 * in the m_nextpkt list.
				 */
				while (m2 != NULL && m2->m_nextpkt != NULL)
					/* Find end of list. */
					m2 = m2->m_nextpkt;
				if (m2 != NULL)
					m2->m_nextpkt = crdma->crdma_record;
				else
					cd->mreq = crdma->crdma_record;
				crdma->crdma_record->m_nextpkt = NULL;
				crdma->crdma_record = NULL;
				xprt_active(xprt);
			}
		} else {
			/* This is a RPC reply. */
			RPCRDMA_DEBUG(1, "clnt_rdma_upcall: Got an RPC "
			    "reply\n");
			foundreq = 0;
			TAILQ_FOREACH(cr, &crdma->crdma_pending, cr_link) {
				if (cr->cr_xid == first_rdma.xid) {
					/*
					 * This one
					 * matches. We leave
					 * the reply mbuf in
					 * cr->cr_mrep. Set
					 * the XID to zero so
					 * that we will ignore
					 * any duplicated
					 * replies.
					 */
					cr->cr_xid = 0;
					cr->cr_mrep = crdma->crdma_record;
					cr->cr_error = 0;
					foundreq = 1;
					wakeup(cr);
					RPCRDMA_DEBUG(1, "clnt_rdma_upcall: "
					    "Found reply for record\n");
					break;
				}
			}

			if (!foundreq) {
				RPCRDMA_DEBUG(1, "clnt_rdma_upcall: No reply for "
				    "record\n");
				m_freem(crdma->crdma_record);
			}
			crdma->crdma_record = NULL;
		}
	}

	if (error != 0) {
	wakeup_all:
		/*
		 * This endpoint is broken, so mark that it cannot
		 * receive and fail all RPCs waiting for a reply
		 * on it, so that they will be retried on a new
		 * RDMA connection created by clnt_reconnect_connect().
		 */
		RPCRDMA_DEBUG(1, "clnt_rdma_upcall: Wakeup all RPC_CANTRECV\n");
		crdma->crdma_error.re_status = RPC_CANTRECV;
		crdma->crdma_error.re_errno = error;
		clnt_rdma_fail_locked(crdma, error);
	}

	crdma->crdma_upcallrefs--;
	if (crdma->crdma_upcallrefs < 0)
		panic("rdma upcall refcnt");
	if (crdma->crdma_upcallrefs == 0)
		wakeup(&crdma->crdma_upcallrefs);
	mtx_unlock(&crdma->crdma_lock);
	RPCRDMA_DEBUG(1, "End of clnt_rdma_upcall\n");
}

/*
 * Do a send of a backchannel reply.
 */
static bool_t
clnt_rdma_bcksend(SVCXPRT *xprt, struct mbuf *m)
{
	struct crdma_data *crdma;
	struct mbuf *mrep;
	XDR xdrs;
	uint32_t xid, rdma_vers, rdma_credit, rdma_body;
	int error, ind;
	bool_t bval;

	/*
	 * The XID is in the first uint32_t of the reply.
	 */
	if (m->m_len < sizeof(xid) && m_length(m, NULL) < sizeof(xid)) {
		m_freem(m);
		return (FALSE);
	}
	m_copydata(m, 0, sizeof(xid), (char *)&xid);
	xid = ntohl(xid);

	mrep = m_gethdr(M_WAITOK, MT_DATA);
	mrep->m_len = 0;
	xdrmbuf_create(&xdrs, mrep, XDR_ENCODE);

	sx_xlock(&xprt->xp_lock);
	crdma = (struct crdma_data *)xprt->xp_p2;
	if (crdma != NULL) {
		rdma_vers = 1;
		rdma_credit = crdma->crdma_xp.maxbck;
		rdma_body = RDMA_MSG;
		bval = FALSE;
		if ((! XDR_PUTINT32(&xdrs, &xid)) ||
		    (! XDR_PUTINT32(&xdrs, &rdma_vers)) ||
		    (! XDR_PUTINT32(&xdrs, &rdma_credit)) ||
		    (! XDR_PUTINT32(&xdrs, &rdma_body)) ||
		    (! XDR_PUTINT32(&xdrs, &bval)) ||
		    (! XDR_PUTINT32(&xdrs, &bval)) ||
		    (! XDR_PUTINT32(&xdrs, &bval))) {
			sx_xunlock(&xprt->xp_lock);
			m_freem(m);
			return (FALSE);
		}

		if (! XDR_PUTMBUF(&xdrs, m)) {
			sx_xunlock(&xprt->xp_lock);
			return (FALSE);
		}

		mtx_lock(&crdma->crdma_lock);
		ind = xprt_rdma_acquire_buf(&crdma->crdma_xp,
		    crdma->crdma_xp.maxrpc, crdma->crdma_xp.maxrpc +
		    crdma->crdma_xp.maxbck);
		mtx_unlock(&crdma->crdma_lock);
		if (ind < 0) {
			sx_xunlock(&xprt->xp_lock);
			m_freem(mrep);
			return (FALSE);
		}
		mrep->m_pkthdr.len = m_length(mrep, NULL);
		error = xprt_rdma_send(&crdma->crdma_xp, mrep, ind);
		xprt_rdma_release_send(&crdma->crdma_xp, ind, NULL, NULL, NULL,
		    RPC_SUCCESS);
		sx_xunlock(&xprt->xp_lock);
	} else {
		sx_xunlock(&xprt->xp_lock);
		m_freem(m);
		m_free(mrep);
		error = EIO;	/* Anything non-zero. */
	}
	if (error != 0)
		return (FALSE);
	return (TRUE);
}

/* These are very similar to the ones in svc_vc.c, but just different enough. */
static void svc_rdma_backchannel_destroy(SVCXPRT *);
static enum xprt_stat svc_rdma_backchannel_stat(SVCXPRT *);
static bool_t svc_rdma_backchannel_recv(SVCXPRT *, struct rpc_msg *,
    struct sockaddr **, struct mbuf **);
static bool_t svc_rdma_backchannel_reply(SVCXPRT *, struct rpc_msg *,
    struct sockaddr *, struct mbuf *, uint32_t *);
static bool_t svc_rdma_backchannel_control(SVCXPRT *xprt, const u_int rq,
    void *in);

static const struct xp_ops svc_rdma_backchannel_ops = {
	.xp_recv =	svc_rdma_backchannel_recv,
	.xp_stat =	svc_rdma_backchannel_stat,
	.xp_reply =	svc_rdma_backchannel_reply,
	.xp_destroy =	svc_rdma_backchannel_destroy,
	.xp_control =	svc_rdma_backchannel_control
};

/*
 * Create a new transport for a backchannel on a clnt_rdma socket.
 */
SVCXPRT *
svc_rdma_create_backchannel(SVCPOOL *pool)
{
	SVCXPRT *xprt = NULL;
	struct cf_conn *cd = NULL;

	cd = mem_alloc(sizeof(*cd));
	cd->strm_stat = XPRT_IDLE;

	xprt = svc_xprt_alloc();
	sx_init(&xprt->xp_lock, "xprt->xp_lock");
	xprt->xp_pool = pool;
	xprt->xp_socket = NULL;
	xprt->xp_p1 = cd;
	xprt->xp_p2 = NULL;
	xprt->xp_ops = &svc_rdma_backchannel_ops;
	return (xprt);
}

static void
svc_rdma_backchannel_destroy(SVCXPRT *xprt)
{
	struct cf_conn *cd = (struct cf_conn *)xprt->xp_p1;
	struct mbuf *m, *m2;

	svc_xprt_free(xprt);
	m = cd->mreq;
	while (m != NULL) {
		m2 = m;
		m = m->m_nextpkt;
		m_freem(m2);
	}
	mem_free(cd, sizeof(*cd));
}

static bool_t
svc_rdma_backchannel_control(SVCXPRT *xprt, const u_int rq, void *in)
{

	return (FALSE);
}

static enum xprt_stat
svc_rdma_backchannel_stat(SVCXPRT *xprt)
{
	struct cf_conn *cd;

	if (xprt->xp_p2 == NULL)
		return (XPRT_DIED);

	cd = (struct cf_conn *)(xprt->xp_p1);

	if (cd->mreq != NULL)
		return (XPRT_MOREREQS);

	return (XPRT_IDLE);
}

static bool_t
svc_rdma_backchannel_recv(SVCXPRT *xprt, struct rpc_msg *msg,
    struct sockaddr **addrp, struct mbuf **mp)
{
	struct cf_conn *cd = (struct cf_conn *) xprt->xp_p1;
	struct crdma_data *crdma;
	struct mbuf *m;
	XDR xdrs;

	sx_xlock(&xprt->xp_lock);
	crdma = (struct crdma_data *)xprt->xp_p2;
	if (crdma == NULL) {
		sx_xunlock(&xprt->xp_lock);
		return (FALSE);
	}
	mtx_lock(&crdma->crdma_lock);
	m = cd->mreq;
	if (m == NULL) {
		xprt_inactive_self(xprt);
		mtx_unlock(&crdma->crdma_lock);
		sx_xunlock(&xprt->xp_lock);
		return (FALSE);
	}
	cd->mreq = m->m_nextpkt;
	mtx_unlock(&crdma->crdma_lock);
	sx_xunlock(&xprt->xp_lock);

	xdrmbuf_create(&xdrs, m, XDR_DECODE);
	if (! xdr_callmsg(&xdrs, msg)) {
		XDR_DESTROY(&xdrs);
		return (FALSE);
	}
	*addrp = NULL;
	*mp = xdrmbuf_getall(&xdrs);
	XDR_DESTROY(&xdrs);
	return (TRUE);
}

static bool_t
svc_rdma_backchannel_reply(SVCXPRT *xprt, struct rpc_msg *msg,
    struct sockaddr *addr, struct mbuf *m, uint32_t *seq)
{
	XDR xdrs;
	struct mbuf *mrep;
	bool_t stat = TRUE;

	mrep = m_gethdr(M_WAITOK, MT_DATA);

	xdrmbuf_create(&xdrs, mrep, XDR_ENCODE);

	if (msg->rm_reply.rp_stat == MSG_ACCEPTED &&
	    msg->rm_reply.rp_acpt.ar_stat == SUCCESS) {
		if (!xdr_replymsg(&xdrs, msg))
			stat = FALSE;
		else
			(void)xdr_putmbuf(&xdrs, m);
	} else {
		stat = xdr_replymsg(&xdrs, msg);
	}

	if (stat)
		stat = clnt_rdma_bcksend(xprt, mrep);
	else
		m_freem(mrep);

	XDR_DESTROY(&xdrs);

	return (stat);
}

/*
 * Kernel module glue
 */
static int
nfsclrdma_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		rdma_check_route = xprt_rdma_check_route;
		clnt_rdma_create_call = clnt_rdma_create;
		svc_rdma_create_backchannel_call = svc_rdma_create_backchannel;
		break;
	case MOD_UNLOAD:
		/*
		 * Cannot be unloaded.
		 */
		/* FALLTHROUGH */
	default:
		error = EOPNOTSUPP;
	}
	return (error);
}
static moduledata_t nfsclrdma_mod = {
	"nfsclrdma",
	nfsclrdma_modevent,
	NULL,
};
DECLARE_MODULE(nfsclrdma, nfsclrdma_mod, SI_SUB_VFS, SI_ORDER_FIRST);

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_VERSION(nfsclrdma, 1);
MODULE_DEPEND(nfsclrdma, krpc, 1, 1, 1);
MODULE_DEPEND(nfsclrdma, nfscommon, 1, 1, 1);
MODULE_DEPEND(nfsclrdma, nfscl, 1, 1, 1);
MODULE_DEPEND(nfsclrdma, ibcore, 1, 1, 1);
MODULE_DEPEND(nfsclrdma, linuxkpi, 1, 1, 1);
