// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (c) 2014-2017 Oracle.  All rights reserved.
 * Copyright (c) 2003-2007 Network Appliance, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of the Network Appliance, Inc. nor the names of
 *      its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file is loosely based on net/sunrpc/xprtrdma/verbs.c.
 * in Linux.
 * It is significantly different, due to FreeBSD's rdma_XXX()
 * functions having different arguments, among other things.
 */
/*
 * verbs.c
 *
 * Encapsulates the major functions managing:
 *  o adapters
 *  o endpoints
 *  o connections
 *  o buffer memory
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <rpc/clnt_stat.h>
#include <rpc/clntrdma.h>
#include <rdma/rdma_cm.h>
#include "xprt_rdma.h"

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netinet/in_fib.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_fib.h>
#include <netinet6/scope6_var.h>

#include "opt_inet.h"
#include "opt_inet6.h"

MALLOC_DEFINE(M_RPCRDMA, "Krpc RDMA", "Krpc RDMA");

struct rpcrdma_send {
	void			*buf;
	struct ib_cqe		cqe;
	struct ib_send_wr	wr;
	struct ib_sge		sge;
	struct rpcrdma_ep	*ep;
	int			async_rc;
	struct completion	done;
};

struct rpcrdma_recv {
	void			*buf;
	struct ib_cqe		cqe;
	struct ib_recv_wr	wr;
	struct ib_sge		sge;
	struct rpcrdma_ep	*ep;
	int			ind;
	int			async_rc;
	struct completion	done;
};

/* Structure for a connection endpoint. */
struct rpcrdma_ep {
	struct rpcrdma_xprt	*re_xp;
	int			re_async_rc;
	struct completion	re_done;
	struct ib_qp_init_attr	re_attr;
	xprt_rdma_upcall	re_upcall;
#define	re_startzero		re_id
	struct rdma_cm_id	*re_id;
	struct ib_pd		*re_pd;
	struct ib_cq		*re_recv_cq;
	struct ib_cq		*re_send_cq;
	uint64_t		re_busy;
	bool			re_draining;
	bool			re_drained;
	struct rpcrdma_send	re_send[RPCRDMA_IO_NUMBUFS];
	struct rpcrdma_recv	re_recv[RPCRDMA_IO_NUMBUFS];
	struct ib_mr	       *re_mr[RPCRDMA_IO_NUMBUFS][RPCRDMA_MAX_SEGMENTS];
	struct ib_mr	*re_reply[RPCRDMA_IO_NUMBUFS][RPCRDMA_MAX_SEGMENTS];
	struct rpcrdma_regwr re_regwr[RPCRDMA_IO_NUMBUFS][RPCRDMA_MAX_SEGMENTS];
	struct rpcrdma_regwr re_reply_regwr[RPCRDMA_IO_NUMBUFS][RPCRDMA_MAX_SEGMENTS];
	size_t			re_buflen;
	unsigned int		re_send_count;
	int			re_receive_count;
	int			re_connect_status;
	struct rdma_conn_param	re_remote_cma;
#define	re_endzero		re_mrtype
	enum ib_mr_type		re_mrtype;
};

typedef void (*rpcrdma_done)(struct ib_cq *cq, struct ib_wc *wc);

extern u_int rpcrdma_maxrpc;

/* Serialize posting with the transition to a drained endpoint. */
static int
rpcrdma_post_send(struct rpcrdma_ep *ep, const struct ib_send_wr *wr,
    const struct ib_send_wr **bad_wr)
{
	int ret;

	mtx_lock(&ep->re_xp->mtx);
	if (ep->re_connect_status != 0)
		ret = -ENOTCONN;
	else
		ret = ib_post_send(ep->re_id->qp, wr, bad_wr);
	mtx_unlock(&ep->re_xp->mtx);
	return (ret);
}

/* Called in a request or close thread, never from a CQ or CM callback. */
static void
rpcrdma_drain_ep(struct rpcrdma_ep *ep)
{
	struct rpcrdma_xprt *xp;

	xp = ep->re_xp;
	mtx_lock(&xp->mtx);
	ep->re_connect_status = -ECONNABORTED;
	wakeup(&ep->re_busy);
	while (ep->re_draining)
		msleep(&ep->re_draining, &xp->mtx, PVFS, "rdmadr", 0);
	if (ep->re_drained) {
		mtx_unlock(&xp->mtx);
		return;
	}
	ep->re_draining = true;
	mtx_unlock(&xp->mtx);
	if (ep->re_id != NULL && ep->re_id->qp != NULL)
		ib_drain_qp(ep->re_id->qp);
	mtx_lock(&xp->mtx);
	ep->re_drained = true;
	ep->re_draining = false;
	wakeup(&ep->re_draining);
	mtx_unlock(&xp->mtx);
	if (ep->re_upcall != NULL)
		ep->re_upcall(xp, NULL);
}

static void
rpcrdma_free_ep(struct rpcrdma_ep *ep, struct rpcrdma_xprt *xp)
{
	int i, j;

	for (i = 0; i < xp->maxrpc + xp->maxbck; i++) {
		if (ep->re_recv[i].sge.addr != 0)
			ib_dma_unmap_single(ep->re_id->device,
			    ep->re_recv[i].sge.addr, RPCRDMA_MAX_INLINE,
			    DMA_FROM_DEVICE);
		if (ep->re_recv[i].buf != NULL)
			free(ep->re_recv[i].buf, M_RPCRDMA);
		if (ep->re_send[i].sge.addr != 0)
			ib_dma_unmap_single(ep->re_id->device,
			    ep->re_send[i].sge.addr, RPCRDMA_MAX_INLINE,
			    DMA_TO_DEVICE);
		if (ep->re_send[i].buf != NULL)
			free(ep->re_send[i].buf, M_RPCRDMA);
	}

	for (i = 0; i < xp->maxrpc; i++) {
		for (j = 0; j < xp->maxsegs; j++) {
			if (ep->re_mr[i][j] != NULL)
				ib_dereg_mr(ep->re_mr[i][j]);
			if (ep->re_reply[i][j] != NULL)
				ib_dereg_mr(ep->re_reply[i][j]);
		}
	}
	if (ep->re_pd != NULL)
		ib_dealloc_pd(ep->re_pd);
}

static int
rpcrdma_cm_event_handler(struct rdma_cm_id *id, struct rdma_cm_event *event)
{
	struct rpcrdma_ep *ep = id->context;
	int rc;

	might_sleep();

	RPCRDMA_DEBUG(3, "rpcrdma_cm_event_handler: event=%d\n", event->event);
	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
	case RDMA_CM_EVENT_ROUTE_RESOLVED:
	case RDMA_CM_EVENT_ESTABLISHED:
		ep->re_async_rc = 0;
		complete(&ep->re_done);
		return (0);
	case RDMA_CM_EVENT_ADDR_ERROR:
		ep->re_async_rc = -EPROTO;
		complete(&ep->re_done);
		return (0);
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_REJECTED:
	case RDMA_CM_EVENT_UNREACHABLE:
		/*
		 * This error matters.  When ENETUNREACH is returned by
		 * xprt_rdma_connect(), clnt_rdma_create() sets
		 * RPC_SYSTEMERROR in rc_err->re_status.  This results in
		 * clnt_reconnect_connect() returning that status and that
		 * causes reconnection retries in clnt_reconnect_call().
		 */
		ep->re_async_rc = -ENETUNREACH;
		complete(&ep->re_done);
		return (0);
	case RDMA_CM_EVENT_DISCONNECTED:
		mtx_lock(&ep->re_xp->mtx);
		ep->re_connect_status = -ECONNABORTED;
		wakeup(&ep->re_busy);
		mtx_unlock(&ep->re_xp->mtx);
		if (ep->re_xp != NULL && ep->re_upcall != NULL)
			ep->re_upcall(ep->re_xp, NULL);
		rc = rdma_disconnect(id);
		RPCRDMA_DEBUG(4, "rpcrdma_cm_event_handler: "
		    "rdma_disconnect=%d\n", rc);
		return (0);
	case RDMA_CM_EVENT_TIMEWAIT_EXIT:
		/* Active requests still need the ID, QP and DMA mappings. */
		mtx_lock(&ep->re_xp->mtx);
		ep->re_connect_status = -ECONNABORTED;
		wakeup(&ep->re_busy);
		mtx_unlock(&ep->re_xp->mtx);
		ep->re_async_rc = 0;
		complete(&ep->re_done);
		return (0);
	default:
		printf("rpcrdma_cm_event_handler: Unknown event=%d\n",
		    event->event);
		ep->re_async_rc = -EIO;
		complete(&ep->re_done);
		return (0);
	}

	return (0);
}

void
xprt_rdma_init(struct rpcrdma_xprt *xp, xprt_rdma_upcall rdma_upcall)
{
	struct rpcrdma_ep *ep;

	KASSERT(xp->ep == NULL, ("xprt_rdma_init: ep not NULL"));
	ep = malloc(sizeof(*ep), M_RPCRDMA, M_WAITOK | M_ZERO);

	/* And set the upcall. */
	ep->re_upcall = rdma_upcall;

	xp->ep = ep;
	ep->re_xp = xp;
}

static struct rpcrdma_ep *
rpcrdma_acquire_ep(struct rpcrdma_xprt *xp, bool fail_disconnect)
{
	struct rpcrdma_ep *ep;

	mtx_lock(&xp->mtx);
	ep = xp->ep;
	if (ep == NULL || (ep->re_connect_status < 0 && fail_disconnect)) {
		mtx_unlock(&xp->mtx);
		RPCRDMA_DEBUG(4, "rpcrdma_acquire_ep: failed ep=%p\n", ep);
		return (NULL);
	}
	mtx_unlock(&xp->mtx);
	return (ep);
}

/*
 * Acquire a buffer, waiting until one is available.
 * *busyp points to a uint64_t bitmap, so it can handle up to 64 buffers.
 */
int
xprt_rdma_acquire_buf(struct rpcrdma_xprt *xp, int start, int end)
{
	struct rpcrdma_ep *ep;
	uint32_t credits;
	int i;

	mtx_assert(&xp->mtx, MA_OWNED);
	for (;;) {
		ep = (struct rpcrdma_ep *)xp->ep;
		if (ep == NULL || ep->re_connect_status != 0)
			return (-1);
		credits = UINT32_MAX;
		if (start == 0)
			credits = xp->credits;
		for (i = start; i < credits && i < end; i++) {
			if (((1ull << i) & ep->re_busy) == 0) {
				ep->re_busy |= (1ull << i);
				return (i);
			}
		}
		RPCRDMA_DEBUG(4, "xprt_rdma_acquire_buf: sleeping credits=%u "
		    "busy=0x%ju start=%d end=%d\n", credits,
		    (uintmax_t)ep->re_busy, start, end);
		msleep(&ep->re_busy, &xp->mtx, PVFS, "rpcrdma", hz);
	}
}

/*
 * Release a buffer.
 */
static void
rpcrdma_release_buf(struct rpcrdma_xprt *xp, uint64_t *busyp, u_int index)
{

	mtx_lock(&xp->mtx);
	*busyp &= ~(1ull << index);
	wakeup(busyp);
	mtx_unlock(&xp->mtx);
}

static void
rpcrdma_send_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct rpcrdma_send *rs;

	rs = container_of(wc->wr_cqe, struct rpcrdma_send, cqe);
	rs->async_rc = wc->status == IB_WC_SUCCESS ? 0 : EPIPE;
	if (wc->status != IB_WC_SUCCESS)
		printf("rpcrdma_send_done: failed status=%u\n", wc->status);
	complete(&rs->done);
}

/* An error completion need not contain a valid opcode. */
static void
rpcrdma_reg_wake(struct ib_cq *cq, struct ib_wc *wc)
{
	struct rpcrdma_regwr *regwr;

	regwr = container_of(wc->wr_cqe, struct rpcrdma_regwr, cqe);
	regwr->async_rc = wc->status == IB_WC_SUCCESS ? 0 : EPIPE;
	if (wc->status != IB_WC_SUCCESS)
		printf("rpcrdma_reg_wake: failed status=%u\n", wc->status);
	complete(&regwr->done);
}

/* Null done function for registrations without signalling. */
static void
rpcrdma_reg_done(struct ib_cq *cq, struct ib_wc *wc)
{

	RPCRDMA_DEBUG(4, "rpcrdma_reg_done: op=%d stat=%d\n", wc->opcode,
	    wc->status);
	if (wc->status != IB_WC_SUCCESS && wc->status != IB_WC_WR_FLUSH_ERR)
		printf("rpcrdma_reg_done: failed opcode=%u status=%u\n",
		    wc->opcode, wc->status);
}

static int
rpcrdma_post_recv(struct rpcrdma_ep *ep, int ind)
{
	const struct ib_recv_wr *bad_rwr;
	int ret;

	KASSERT(ind >= 0 && ind <= RPCRDMA_IO_NUMBUFS,
	    ("rpcrdma_post_recv: bogus ind: %d", ind));
	ib_dma_sync_single_for_device(ep->re_id->device,
	    ep->re_recv[ind].sge.addr, RPCRDMA_MAX_INLINE, DMA_FROM_DEVICE);
	init_completion(&ep->re_recv[ind].done);
	ep->re_recv[ind].async_rc = -ETIMEDOUT;
	mtx_lock(&ep->re_xp->mtx);
	if (ep->re_connect_status != 0)
		ret = -ENOTCONN;
	else
		ret = ib_post_recv(ep->re_id->qp, &ep->re_recv[ind].wr,
		    &bad_rwr);
	mtx_unlock(&ep->re_xp->mtx);
	RPCRDMA_DEBUG(1, "rpcrdma_post_recv: ind=%d ib_post_recv=%d\n", ind,
	    ret);
	if (ret < 0)
		printf("rpcrdma_post_recv: failed ret=%d ind=%d\n", ret, ind);
	return (ret);
}

static void
rpcrdma_recv_done(struct ib_cq *cq, struct ib_wc *wc)
{
	struct uio uio;
	struct iovec iov;
	struct rpcrdma_recv *rs;
	struct rpcrdma_ep *ep;
	struct mbuf *m;
	int rc;

	if (cq == NULL || wc == NULL) {
		printf("rpcrdma_recv_done: cq=%p wc=%p\n", cq, wc);
		return;
	}
	RPCRDMA_DEBUG(4, "rpcrdma_recv_done: code=%d stat=%d len=%d\n",
	    wc->opcode, wc->status, wc->byte_len);
	rs = container_of(wc->wr_cqe, struct rpcrdma_recv, cqe);
	if (rs == NULL) {
		printf("rpcrdma_recv_done: rs NULL\n");
		return;
	}
	ep = rs->ep;
	if (ep == NULL) {
		printf("rpcrdma_recv_done: ep NULL\n");
		return;
	}

	if (wc->opcode != IB_WC_RECV || wc->status != IB_WC_SUCCESS ||
	    wc->byte_len == 0 || wc->byte_len > RPCRDMA_MAX_INLINE) {
		if (wc->status != IB_WC_WR_FLUSH_ERR)
			printf("rpcrdma_recv_done: opcode=%d status=%d "
			    "len=%d\n", wc->opcode, wc->status, wc->byte_len);
		if (wc->status != IB_WC_WR_FLUSH_ERR)
			ep->re_upcall(ep->re_xp, NULL);
		return;
	}

	KASSERT(rs->ind >= 0 && rs->ind <= RPCRDMA_IO_NUMBUFS,
	    ("rpcrdma_recv_done: Bogus ind: %d", rs->ind));
	if (wc->byte_len > ep->re_buflen) {
		printf("rpcrdma_recv_done: byte_len=%d too large\n",
		    wc->byte_len);
		ep->re_upcall(ep->re_xp, NULL);
		return;
	}

	ib_dma_sync_single_for_cpu(ep->re_id->device, rs->sge.addr,
	    RPCRDMA_MAX_INLINE, DMA_FROM_DEVICE);

	/* fill in an mbuf chain with the data. */
	iov.iov_base = rs->buf;
	iov.iov_len = wc->byte_len;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_resid = wc->byte_len;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_offset = 0;
	uio.uio_td = NULL;
	m = m_uiotombuf(&uio, M_WAITOK, 0, 0, M_PKTHDR);
	if (m == NULL) {
		printf("rpcrdma_recv_done: m_uiotombuf failed\n");
		ep->re_upcall(ep->re_xp, NULL);
		return;
	}

	/* And re-post the recv buffer. */
	rc = rpcrdma_post_recv(ep, rs->ind);
	if (rc) {
		RPCRDMA_DEBUG(1, "rpcrdma_recv_done: rpcrdma_post_recv ind=%d "
		    "failed=%d\n", rs->ind, rc);
		m_freem(m);
		ep->re_upcall(ep->re_xp, NULL);
		return;
	}
	/* Re-enable the receive buffer. */
	RPCRDMA_DEBUG(4, "rpcrdma_recv_done: calling upcall m=%p\n", m);
	if (ep->re_xp != NULL)
		ep->re_upcall(ep->re_xp, m);
}

/*
 * Try and figure out the srcaddr.
 * Return 0 if a link local INET6 address.
 * Return 1 if the address is resolved.
 * Return -1 for failure.
 */
static int
_rpcrdma_get_srcaddr(struct sockaddr *saddr, struct sockaddr_storage *srcaddr)
{
#if defined(INET) || defined(INET6)
	struct nhop_object *nh = NULL;
	struct epoch_tracker et;
#endif
#ifdef INET
	struct sockaddr_in *sin;
#endif
#ifdef INET6
	struct sockaddr_in6 *sin6;
	struct in6_addr kdst;
	uint32_t scopeid;
#endif

	CURVNET_SET_QUIET(TD_TO_VNET(curthread));
	switch (saddr->sa_family) {
#ifdef INET
	case AF_INET:
		sin = (struct sockaddr_in *)saddr;
		NET_EPOCH_ENTER(et);
		nh = fib4_lookup(curthread->td_proc->p_fibnum, sin->sin_addr, 0,
		    NHR_NONE, 0);
		if (nh == NULL || nh->nh_ifa == NULL ||
		    nh->nh_ifa->ifa_addr == NULL) {
			NET_EPOCH_EXIT(et);
			CURVNET_RESTORE();
			return (-1);
		}
		memcpy(srcaddr, nh->nh_ifa->ifa_addr, sizeof(*sin));
		RPCRDMA_DEBUG(3, "_rpcrdma_get_srcaddr: NIC to be used %s\n",
		    nh->nh_ifp->if_xname);
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();
		return (0);
#endif
#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)saddr;
		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
			CURVNET_RESTORE();
			return (0);
		}
		in6_splitscope(&sin6->sin6_addr, &kdst, &scopeid);
		NET_EPOCH_ENTER(et);
		nh = fib6_lookup(curthread->td_proc->p_fibnum, &kdst, scopeid,
		    NHR_NONE, 0);
		if (nh == NULL || nh->nh_ifa == NULL ||
		    nh->nh_ifa->ifa_addr == NULL) {
			NET_EPOCH_EXIT(et);
			CURVNET_RESTORE();
			return (-1);
		}
		memcpy(srcaddr, nh->nh_ifa->ifa_addr, sizeof(*sin6));
		RPCRDMA_DEBUG(3, "_rpcrdma_get_srcaddr: NIC to be used %s\n",
		    nh->nh_ifp->if_xname);
		NET_EPOCH_EXIT(et);
		CURVNET_RESTORE();
		return (0);
#endif
	default:
		CURVNET_RESTORE();
		RPCRDMA_DEBUG(3, "_rpcrdma_get_srcaddr: No NIC found\n");
		return (-1);
	}
	CURVNET_RESTORE();
	return (-1);
}

static struct rdma_cm_id *
rpcrdma_create_id(struct vnet *net, struct sockaddr *saddr,
    struct rpcrdma_ep *ep, struct rpcrdma_xprt *xp, uint32_t cbslots)
{
	struct rdma_cm_id *id;
	const struct ib_device_attr *attr;
	struct sockaddr_storage ss;
	struct sockaddr *srcaddr;
	int val, numsge, rc;

	/* Try and figure out what the srcaddr is via routing. */
	rc = _rpcrdma_get_srcaddr(saddr, &ss);
	switch (rc) {
	case 0:
		srcaddr = NULL;
		break;
	case 1:
		srcaddr = (struct sockaddr *)&ss;
		break;
	case -1:
		RPCRDMA_DEBUG(3, "rpcrdma_create_id: No NIC addr found\n");
		return (ERR_PTR(-EADDRNOTAVAIL));
	}

	id = rdma_create_id(net, rpcrdma_cm_event_handler, ep,
	    RDMA_PS_TCP, IB_QPT_RC);
	RPCRDMA_DEBUG(4, "rpcrdma_create_id: rdma_create_id=%p\n", id);
	if (IS_ERR(id))
		return (id);

	init_completion(&ep->re_done);
	ep->re_async_rc = -ETIMEDOUT;
	rc = rdma_resolve_addr(id, srcaddr, saddr, RDMA_RESOLVE_TIMEOUT);
	if (rc)
		goto out;
	rc = wait_for_completion(&ep->re_done);
	if (rc < 0)
		goto out;
	rc = ep->re_async_rc;
	RPCRDMA_DEBUG(4, "rpcrdma_create_id: rdma_resolve_addr=%d\n", rc);
	if (rc)
		goto out;

	init_completion(&ep->re_done);
	ep->re_async_rc = -ETIMEDOUT;
	rc = rdma_resolve_route(id, RDMA_RESOLVE_TIMEOUT);
	if (rc)
		goto out;
	rc = wait_for_completion(&ep->re_done);
	if (rc < 0)
		goto out;
	rc = ep->re_async_rc;
	RPCRDMA_DEBUG(4, "rpcrdma_create_id: rdma_resolve_route=%d\n", rc);
	if (rc)
		goto out;

	if (id->device == NULL) {
		/* I don't think this should ever happen? */
		printf("rpcrdma_create_id: No valid device\n");
		rc = -EPROTONOSUPPORT;
		goto out;
	}

	attr = &id->device->attrs;
	/* The device attrs should now be set, so check them. */
	if ((attr->device_cap_flags & IB_DEVICE_MEM_MGT_EXTENSIONS) == 0) {
		printf("rpcrdma_create_id: No memory extensions\n");
		rc = -EPROTONOSUPPORT;
		goto out;
	}

	ep->re_mrtype = IB_MR_TYPE_MEM_REG;
	if ((attr->device_cap_flags & IB_DEVICE_SG_GAPS_REG) != 0)
		ep->re_mrtype = IB_MR_TYPE_SG_GAPS;

	/*
	 * I am still pretty sketchy w.r.t. ofed/rdma, but I think..
	 * - MIN(max_sg, max_sg_rd) is the max # of pages per segment (MR).
	 * - I assume it also sets the limit on the # of send/recv buffers per
	 *   QP. Some drivers may allow larger values for the max_send_sge
	 *   and max_recv_sge but assuming only one MR seems to work ok.
	 * - RFC-8267 sets a limit of 16 segments (MRs) per chunk and 1 chunk
	 *   per RPC.
	 * For a worse case of all RPCs (Read/Write/Readdir) needing a chunk and
	 * NFS I/O sizes (nm_rsize, nm_wsize) are always a power of 2, so
	 * round MIN(max_sg, max_sg_rd) down to a power of 2.
	 * - Add 1 for the rest of the RPC message other than the data.
	 * - Add 1 for the minimum send/recv buffers needed for backchannel msg.
	 * maxrpc - Set to the maximum # of concurrent RPCs.
	 * maxbck - Set to the maximum # of concurrent back channel RPCs.
	 * maxio - Set to the maximum I/O size allowed.
	 * maxsge - Set to the maximum sge size.
	 * maxsegs - Maximum number of segments (MRs) in a chunk.
	 * maxreads - Maximum number of RPCs with a Read chunk.
	 * For now, it appears that the device attributes max_sge, max_sge_rd
	 * and max_qp_rd_atom set the limits.
	 */
#define	MELLANOX_ID	0x02c9
#define	INTEL_ID	0x8086
#define	CHELSIO_ID	0x1425

#define	CONNECTX4_ID	4115

	numsge = MIN(attr->max_sge, attr->max_sge_rd);
	if (numsge < 5) {
		/* If numsge < 5, NFS over RDMA cannot be done. */
		xp->maxsge = xp->maxrpc = xp->maxbck = xp->maxio = 0;
		xp->maxsegs = 0;
		RPCRDMA_DEBUG(1, "rpcrdma_create_id: numsge=0 id=%p\n", id);
		return (id);
	}
	/* Must be a power of 2 less than max_sge and max_sge_rd. */
	val = 1 << (fls(numsge) - 1);
	if (val < numsge)
		numsge = val;
	else
		numsge /= 2;
	xp->maxsge = MIN(numsge + 1, RPCRDMA_MAX_SGE);
	if (xp->maxsge < numsge + 1)
		numsge = xp->maxsge - 1;
	xp->maxbck = cbslots;
	xp->maxrpc = RPCRDMA_IO_NUMBUFS - cbslots;

	/*
	 * For ConnectX4, if maxrpc > 12 or maxsegs > 1, there are intermittent
	 * QP disconnects caused by "mlx5_1: WARN: dump_cqe..".  Until this is
	 * resolved, set these limits to avoid the QP reconnects.
	 * Setting kern.rpc.rdma.maxrpc to < 56 overrides this.
	 */
	if (attr->vendor_id == MELLANOX_ID &&
	    attr->vendor_part_id <= CONNECTX4_ID)
		xp->maxrpc = 8;
	if (rpcrdma_maxrpc < RPCRDMA_IO_NUMBUFS - cbslots)
		xp->maxrpc = rpcrdma_maxrpc;
	xp->maxreads = MIN(xp->maxrpc, attr->max_qp_rd_atom);
	xp->maxsegs = howmany(maxbcachebuf, xp->maxsge * PAGE_SIZE);
	xp->maxsegs = MIN(xp->maxsegs, RPCRDMA_MAX_SEGMENTS);
	if (attr->vendor_id == MELLANOX_ID &&
	    attr->vendor_part_id <= CONNECTX4_ID)
		xp->maxsegs = 1;
	xp->maxio = numsge * xp->maxsegs * PAGE_SIZE;
	RPCRDMA_DEBUG(1, "rpcrdma_create_id: maxrpc=%u maxbck=%u maxsge=%u "
	    "maxsegs=%u maxio=%u maxreads=%u id=%p\n", xp->maxrpc, xp->maxbck,
	    xp->maxsge, xp->maxsegs, xp->maxio, xp->maxreads, id);
	return (id);

out:
	rdma_destroy_id(id);
	RPCRDMA_DEBUG(1, "rpcrdma_create_id: Failed %d\n", rc);
	return (ERR_PTR(rc));
}

static int
rpcrdma_ep_create(struct vnet *net, struct sockaddr *saddr,
    struct rpcrdma_ep *ep, struct rpcrdma_xprt *xp, size_t buflen,
    uint32_t cbslots)
{
	struct ib_device *device;
	struct rdma_cm_id *id;
	struct ib_pd *pd;
	struct ib_cq *recv_cq, *send_cq;
	int i, j, rc;
	u_int maxvec, nextvec;
	static volatile u_int vec = 0;

	pd = NULL;
	memset(&ep->re_startzero, 0,
	    __rangeof(struct rpcrdma_ep, re_startzero, re_endzero));
	id = rpcrdma_create_id(net, saddr, ep, xp, cbslots);
	if (IS_ERR(id))
		return (PTR_ERR(id));
	ep->re_id = id;
	device = id->device;
	reinit_completion(&ep->re_done);

	pd = ib_alloc_pd(device, 0);
	if (IS_ERR(pd)) {
		rc = PTR_ERR(pd);
		pd = NULL;
		goto out_destroy;
	}
	ep->re_pd = pd;

	maxvec = (device->num_comp_vectors > 0) ? device->num_comp_vectors : 1;
	nextvec = atomic_fetchadd_int(&vec, 2) % maxvec;
	recv_cq = ib_alloc_cq(device, xp, xp->maxrpc + xp->maxbck + 1, nextvec,
	    IB_POLL_WORKQUEUE);
	if (IS_ERR(recv_cq)) {
		rc = PTR_ERR(recv_cq);
		recv_cq = NULL;
		goto out_destroy;
	}
	ep->re_recv_cq = recv_cq;

	send_cq = ib_alloc_cq(device, xp, xp->maxrpc + xp->maxbck + xp->maxrpc *
	    xp->maxsegs + 1, (nextvec + 1) % maxvec, IB_POLL_WORKQUEUE);
	if (IS_ERR(send_cq)) {
		rc = PTR_ERR(send_cq);
		send_cq = NULL;
		goto out_destroy;
	}
	ep->re_send_cq = send_cq;

	/* The cap fields of re_attr should be filled in, the rest zeroed. */
	memset(&ep->re_attr, 0, sizeof(ep->re_attr));
	ep->re_attr.qp_type = IB_QPT_RC;
	ep->re_attr.sq_sig_type = IB_SIGNAL_REQ_WR;
	ep->re_attr.qp_context = ep;
	ep->re_attr.recv_cq = recv_cq;
	ep->re_attr.send_cq = send_cq;

	/* Leave one entry in each queue for ib_drain_qp(). */
	ep->re_attr.cap.max_send_wr = xp->maxrpc + xp->maxbck + xp->maxrpc *
	    xp->maxsegs + 1;
	ep->re_attr.cap.max_recv_wr = xp->maxrpc + xp->maxbck + xp->maxrpc *
	    xp->maxsegs + 1;
	ep->re_attr.cap.max_recv_sge = xp->maxsge;
	ep->re_attr.cap.max_send_sge = xp->maxsge;
	rc = rdma_create_qp(id, pd, &ep->re_attr);
	if (rc) {
		RPCRDMA_DEBUG(1, "rpcrdma_ep_create: rdma_create_qp "
		    "failed=%d\n", rc);
		goto out_destroy;
	}

	ep->re_buflen = buflen;
	RPCRDMA_DEBUG(4, "rpcrdma_ep_create: Initialize %u send/recv buffers "
	    "len=%ju\n", xp->maxrpc + xp->maxbck, (uintmax_t)buflen);
	for (i = 0; i < xp->maxrpc + xp->maxbck; i++) {
		ep->re_send[i].buf = malloc(RPCRDMA_MAX_INLINE, M_RPCRDMA,
		    M_WAITOK);
		ep->re_send[i].sge.addr = ib_dma_map_single(device,
		    ep->re_send[i].buf, RPCRDMA_MAX_INLINE, DMA_TO_DEVICE);
		ep->re_send[i].sge.length = RPCRDMA_MAX_INLINE;
		ep->re_send[i].sge.lkey = pd->local_dma_lkey;
		rc = ib_dma_mapping_error(device, ep->re_send[i].sge.addr);
		if (rc) {
			ep->re_send[i].sge.addr = 0;
			RPCRDMA_DEBUG(4, "rpcrdma_ep_create: dma mapping "
			    "failed=%d\n", rc);
			goto out_destroy_qp;
		}
		ep->re_send[i].ep = ep;
		ep->re_send[i].wr.next = NULL;
		ep->re_send[i].wr.num_sge = 1;
		ep->re_send[i].wr.wr_cqe = &ep->re_send[i].cqe;
		ep->re_send[i].wr.sg_list = &ep->re_send[i].sge;
		ep->re_send[i].cqe.done = rpcrdma_send_done;
		ep->re_send[i].wr.send_flags = IB_SEND_SIGNALED;
		ep->re_send[i].wr.opcode = IB_WR_SEND;

		ep->re_recv[i].buf = malloc(RPCRDMA_MAX_INLINE, M_RPCRDMA,
		    M_WAITOK);
		ep->re_recv[i].sge.addr = ib_dma_map_single(device,
		    ep->re_recv[i].buf, RPCRDMA_MAX_INLINE, DMA_FROM_DEVICE);
		ep->re_recv[i].sge.length = RPCRDMA_MAX_INLINE;
		ep->re_recv[i].sge.lkey = pd->local_dma_lkey;
		rc = ib_dma_mapping_error(device, ep->re_recv[i].sge.addr);
		if (rc) {
			ep->re_recv[i].sge.addr = 0;
			RPCRDMA_DEBUG(4, "rpcrdma_ep_create: dma mapping "
			    "failed=%d\n", rc);
			goto out_destroy_qp;
		}
		ep->re_recv[i].ep = ep;
		ep->re_recv[i].wr.next = NULL;
		ep->re_recv[i].wr.num_sge = 1;
		ep->re_recv[i].wr.sg_list = &ep->re_recv[i].sge;
		ep->re_recv[i].wr.wr_cqe = &ep->re_recv[i].cqe;
		ep->re_recv[i].cqe.done = rpcrdma_recv_done;
		ep->re_recv[i].ind = i;

	}

	for (i = 0; i < xp->maxrpc; i++) {
		for (j = 0; j < xp->maxsegs; j++) {
			ep->re_mr[i][j] = ib_alloc_mr(pd, ep->re_mrtype,
			    xp->maxsge);
			if (IS_ERR(ep->re_mr[i][j])) {
				rc = PTR_ERR(ep->re_mr[i][j]);
				RPCRDMA_DEBUG(4, "rpcrdma_ep_create: alloc mr "
				    "failed=%p\n", ep->re_mr[i][j]);
				ep->re_mr[i][j] = NULL;
				goto out_destroy_qp;
			}
			ep->re_reply[i][j] = ib_alloc_mr(pd, ep->re_mrtype,
			    xp->maxsge);
			if (IS_ERR(ep->re_reply[i][j])) {
				rc = PTR_ERR(ep->re_reply[i][j]);
				RPCRDMA_DEBUG(4, "rpcrdma_ep_create: alloc mr "
				    "failed=%p\n", ep->re_mr[i][j]);
				ep->re_reply[i][j] = NULL;
				goto out_destroy_qp;
			}
		}
	}
	ep->re_pd = pd;
	ep->re_id = id;
	ep->re_recv_cq = recv_cq;
	ep->re_send_cq = send_cq;
	RPCRDMA_DEBUG(1, "rpcrdma_ep_create succeeded\n");
	return (0);

out_destroy_qp:
out_destroy:
	xprt_rdma_disconnect(xp);
	RPCRDMA_DEBUG(1, "rpcrdma_ep_create: failed=%d\n", rc);
	return (rc);
}

int
xprt_rdma_check_route(struct vnet *net, struct sockaddr *saddr,
    uint32_t cbslots)
{
	struct rpcrdma_xprt xpr;
	struct rdma_cm_id *id;

	memset(&xpr, 0, sizeof(xpr));
	xprt_rdma_init(&xpr, NULL);
	mtx_init(&xpr.mtx, "xprt_check_mtx", NULL, MTX_DEF);
	id = rpcrdma_create_id(net, saddr, xpr.ep, &xpr, cbslots);
	if (IS_ERR(id)) {
		free(xpr.ep, M_RPCRDMA);
		mtx_destroy(&xpr.mtx);
		return (EPROTONOSUPPORT);
	}
	rdma_disconnect(id);
	rdma_destroy_id(id);
	free(xpr.ep, M_RPCRDMA);
	mtx_destroy(&xpr.mtx);
	if (xpr.maxrpc < 1 || xpr.maxbck < 1)
		return (EPROTONOSUPPORT);
	return (0);
}

int
xprt_rdma_connect(struct vnet *net, struct sockaddr *saddr,
    struct rpcrdma_xprt *xp, size_t buflen, uint32_t cbslots)
{
	struct rpcrdma_ep *ep;
	struct ib_device *device;
	int i, rc;

	ep = rpcrdma_acquire_ep(xp, true);
	if (ep == NULL)
		return (ECONNREFUSED);
	rc = rpcrdma_ep_create(net, saddr, ep, xp, buflen, cbslots);
	if (rc)
		goto out;

	/* Initialize cma parameters */
	/* Client offers RDMA Read but does not initiate */
	ep->re_remote_cma.initiator_depth = 0;
	device = ep->re_id->device;
	/*
	 * responder_resources is a uint8_t, but Intel reports 256 for
	 * max_qp_rd_atom, which is why it is the minimum of max_qp_rd_atom
	 * and UINT8_MAX.
	 */
	ep->re_remote_cma.responder_resources =
	    min_t(int, U8_MAX, device->attrs.max_qp_rd_atom);

	/*
	 * Try an infinite retry count.  The clnt_reconnect layer should
	 * handle the case where the connection is hung.
	 */
	ep->re_remote_cma.retry_count = 7;

	/*
	 * RPC-over-RDMA handles its own flow control. In addition,
	 * make all RNR NAKs visible so we know that RPC-over-RDMA
	 * flow control is working correctly (no NAKs should be seen).
	 */
	ep->re_remote_cma.flow_control = 0;
	ep->re_remote_cma.rnr_retry_count = 0;

	init_completion(&ep->re_done);
	ep->re_async_rc = -ETIMEDOUT;
	rc = rdma_connect(ep->re_id, &ep->re_remote_cma);
	if (rc)
		goto out_destroy;
	rc = wait_for_completion(&ep->re_done);
	if (rc < 0)
		goto out_destroy;
	rc = ep->re_async_rc;
	RPCRDMA_DEBUG(1, "xprt_rdma_connect: rdma_connect=%d\n", rc);
	if (rc)
		goto out_destroy;

	for (i = 0; i < xp->maxrpc + xp->maxbck; i++) {
		/* And post the receive buffer now. */
		rc = rpcrdma_post_recv(ep, i);
		if (rc) {
			RPCRDMA_DEBUG(4, "rpcrdma_ep_create: ind=%d "
			    "rpcrdma_post_recv=%d\n", i, rc);
			goto out_destroy;
		}
	}
	return (rc);
out_destroy:
	xprt_rdma_disconnect(xp);
out:
	if (rc < 0)
		rc = -rc;
	RPCRDMA_DEBUG(1, "xprt_rdma_connect: failed=%d\n", rc);
	return (rc);
}

void
xprt_rdma_disconnect(struct rpcrdma_xprt *xp)
{
	struct rpcrdma_ep *ep;
	struct rdma_cm_id *id;

	mtx_lock(&xp->mtx);
	ep = xp->ep;
	if (ep == NULL) {
		mtx_unlock(&xp->mtx);
		return;
	}
	ep->re_connect_status = -ECONNABORTED;
	wakeup(&ep->re_busy);
	/* Include backchannel sends and the last request's DMA cleanup. */
	while (ep->re_busy != 0)
		msleep(&ep->re_busy, &xp->mtx, PVFS, "rdmacl", 0);
	id = ep->re_id;
	mtx_unlock(&xp->mtx);
	if (id == NULL)
		return;
	(void)rdma_disconnect(id);
	rpcrdma_drain_ep(ep);
	if (id->qp != NULL)
		rdma_destroy_qp(id);
	if (ep->re_recv_cq != NULL)
		ib_free_cq(ep->re_recv_cq);
	if (ep->re_send_cq != NULL)
		ib_free_cq(ep->re_send_cq);
	rpcrdma_free_ep(ep, xp);
	/* Wait for CM callbacks before releasing their context. */
	rdma_destroy_id(id);
	ep->re_id = NULL;
	ep->re_xp = NULL;
}

static void
rpcrdma_free_mr(struct rpcrdma_ep *ep, struct _rpcrdma_chunk_priv *chp)
{
	unsigned long wtimeout = msecs_to_jiffies(RDMA_RESOLVE_TIMEOUT) + 1;
	struct rpcrdma_regwr *regwr, *tmp_regwr;
	const struct ib_send_wr *bad_wr;
	int i, ret;

	for (i = 0; i < chp->ch.num_segment; i++) {
		if (chp->ch.mextpg != NULL)
			regwr = &ep->re_reply_regwr[chp->ch.ind][i];
		else
			regwr = &ep->re_regwr[chp->ch.ind][i];
		regwr->wr.wr.next = NULL;
		regwr->wr.wr.sg_list = NULL;
		regwr->wr.wr.num_sge = 0;
		regwr->wr.wr.opcode = IB_WR_LOCAL_INV;
		regwr->wr.wr.ex.invalidate_rkey = regwr->wr.key;
		RPCRDMA_DEBUG(4, "rpcrdma_free_mr: Invalidating key=0x%x\n",
		    regwr->wr.key);
		regwr->wr.wr.send_flags = 0;
		regwr->wr.wr.wr_cqe = &regwr->cqe;
		regwr->cqe.done = rpcrdma_reg_wake;
	}
	for (i = 0; i < chp->ch.num_segment - 1; i++) {
		if (chp->ch.mextpg != NULL) {
			regwr = &ep->re_reply_regwr[chp->ch.ind][i];
			tmp_regwr = &ep->re_reply_regwr[chp->ch.ind][i + 1];
		} else {
			regwr = &ep->re_regwr[chp->ch.ind][i];
			tmp_regwr = &ep->re_regwr[chp->ch.ind][i + 1];
		}
		regwr->wr.wr.next = &tmp_regwr->wr.wr;
		regwr->cqe.done = rpcrdma_reg_done;
	}
	if (chp->ch.mextpg != NULL) {
		regwr = &ep->re_reply_regwr[chp->ch.ind][chp->ch.num_segment-1];
		tmp_regwr = &ep->re_reply_regwr[chp->ch.ind][0];
	} else {
		regwr = &ep->re_regwr[chp->ch.ind][chp->ch.num_segment-1];
		tmp_regwr = &ep->re_regwr[chp->ch.ind][0];
	}
	regwr->wr.wr.send_flags = IB_SEND_SIGNALED;
	init_completion(&regwr->done);
	regwr->async_rc = -ETIMEDOUT;
	ret = rpcrdma_post_send(ep, &tmp_regwr->wr.wr, &bad_wr);
	if (ret != 0) {
		printf("rpcrdma_free_mr: Failed to post send=%d\n", ret);
		rpcrdma_drain_ep(ep);
		return;
	}
	ret = wait_for_completion_timeout(&regwr->done, wtimeout);
	if (ret <= 0 || regwr->async_rc != 0) {
		printf("rpcrdma_free_mr: Failed to complete local key "
		    "invalidation ret=%d rc=%d\n", ret, regwr->async_rc);
		rpcrdma_drain_ep(ep);
	}
}

/* Unmap a chunk. */
static void
rpcrdma_unmap(struct rpcrdma_ep *ep, struct _rpcrdma_chunk_priv *chp,
    enum clnt_stat stat)
{
	enum dma_data_direction dma_dir;
	int i, j;
	uint32_t plen;

	KASSERT(chp->ch.ind >= 0 && chp->ch.ind <= RPCRDMA_IO_NUMBUFS,
	    ("rpcrdma_unmap: Bogus ind: %d", chp->ch.ind));
	dma_dir = (chp->ch.into_mem) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	plen = PAGE_SIZE;
	RPCRDMA_DEBUG(4, "rpcrdma_unmap: Unmapping nseg=%d\n",
	    chp->ch.num_segment);
	if (stat == RPC_SUCCESS)
		rpcrdma_free_mr(ep, chp);
	else
		rpcrdma_drain_ep(ep);
	for (i = 0; i < chp->ch.num_segment; i++) {
		for (j = 0; j < chp->ch.sge_cnt[i]; j++) {
			if (j + 1 == chp->ch.sge_cnt[i] &&
			    i + 1 == chp->ch.num_segment)
				plen = chp->ch.last_len;
			ib_dma_unmap_page(ep->re_id->device,
			    sg_dma_address(&chp->sge[i][j]), plen, dma_dir);
		}
	}
}

/* Unmap a single chunk. */
void
xprt_rdma_unmap_chunk(struct rpcrdma_xprt *xp, struct rpcrdma_chunk *extern_chp)
{
	struct rpcrdma_ep *ep;
	struct _rpcrdma_chunk_priv *chp;

	ep = rpcrdma_acquire_ep(xp, false);

	chp = (struct _rpcrdma_chunk_priv *)extern_chp;
	if (ep != NULL)
		rpcrdma_unmap(ep, chp, RPC_SUCCESS);
	kfree(chp);
}

void
xprt_rdma_release_send(struct rpcrdma_xprt *xp, int ind,
    struct rpcrdma_chunk *extern_chp, struct rpcrdma_chunk *extern_reply_chp,
    struct rpcrdma_chunk *extern_request_chp, enum clnt_stat stat)
{
	struct rpcrdma_ep *ep;
	struct _rpcrdma_chunk_priv *chp;

	ep = rpcrdma_acquire_ep(xp, false);
	KASSERT(ind >= 0 && ind <= xp->maxrpc + xp->maxbck,
	    ("xprt_rdma_release_send: ind out of range"));
	RPCRDMA_DEBUG(4, "xprt_rdma_release_send: ind=%d rwlist_chp=%p "
	    "reply_chp=%p request_chp=%p\n", ind, extern_chp, extern_reply_chp,
	    extern_request_chp);
	if (extern_chp != NULL) {
		chp = (struct _rpcrdma_chunk_priv *)extern_chp;
		if (ep != NULL)
			rpcrdma_unmap(ep, chp, stat);
		kfree(chp);
	}
	if (extern_reply_chp != NULL) {
		chp = (struct _rpcrdma_chunk_priv *)extern_reply_chp;
		if (ep != NULL)
			rpcrdma_unmap(ep, chp, stat);
		m_freem(chp->ch.mextpg);
		kfree(chp);
	}
	if (extern_request_chp != NULL) {
		chp = (struct _rpcrdma_chunk_priv *)extern_request_chp;
		if (ep != NULL)
			rpcrdma_unmap(ep, chp, stat);
		kfree(chp);
	}
	if (ep != NULL)
		rpcrdma_release_buf(ep->re_xp, &ep->re_busy, ind);
}

int
xprt_rdma_send(struct rpcrdma_xprt *xp, struct mbuf *mreq, int ind)
{
	unsigned long wtimeout = msecs_to_jiffies(RDMA_RESOLVE_TIMEOUT) + 1;
	struct rpcrdma_ep *ep;
	const struct ib_send_wr *bad_wr;
	int ret;

	KASSERT(ind >= 0 && ind < RPCRDMA_IO_NUMBUFS,
	    ("xprt_rdma_send: ind out of range: %d", ind));
	ep = rpcrdma_acquire_ep(xp, true);
	RPCRDMA_DEBUG(1, "xprt_rdma_send: ep=%p\n", ep);
	if (ep == NULL) {
		m_freem(mreq);
		return (EPIPE);
	}

	if (mreq->m_pkthdr.len > RPCRDMA_MAX_INLINE) {
		m_freem(mreq);
		return (EMSGSIZE);
	}

	/*
	 * The smallest request is an RPC_NOMSG with 1 Read chunk for the
	 * RPC request message.
	 */
	KASSERT(mreq->m_pkthdr.len >= RPCRDMA_HEADER_LEN(1, 0, 0),
	    ("xprt_rdma_send: mbuf list len out of range: %d",
	    mreq->m_pkthdr.len));
	RPCRDMA_DEBUG(1, "xprt_rdma_send: Send data len=%d ind=%d\n",
	    mreq->m_pkthdr.len, ind);
	ib_dma_sync_single_for_cpu(ep->re_id->device,
	    ep->re_send[ind].sge.addr, RPCRDMA_MAX_INLINE, DMA_TO_DEVICE);
	m_copydata(mreq, 0, mreq->m_pkthdr.len, ep->re_send[ind].buf);
	ib_dma_sync_single_for_device(ep->re_id->device,
	    ep->re_send[ind].sge.addr, RPCRDMA_MAX_INLINE, DMA_TO_DEVICE);

	/* Fill in the ib_send_wr. */
	ep->re_send[ind].sge.length = mreq->m_pkthdr.len;
	m_freem(mreq);

	KASSERT(ep->re_send[ind].sge.addr != 0 &&
	    ep->re_send[ind].wr.num_sge == 1 && ep->re_send[ind].sge.lkey != 0,
	    ("xprt_rdma_send: buf=%p sgeaddr=0x%jx len=%d lkey=0x%x num_sge=%d "
	    " send_flags=0x%x\n", ep->re_send[ind].buf,
	    (uintmax_t)ep->re_send[ind].sge.addr, ep->re_send[ind].sge.length,
	    ep->re_send[ind].sge.lkey, ep->re_send[ind].wr.num_sge,
	    ep->re_send[ind].wr.send_flags));
	init_completion(&ep->re_send[ind].done);
	ep->re_send[ind].async_rc = -ETIMEDOUT;
	ret = rpcrdma_post_send(ep, &ep->re_send[ind].wr, &bad_wr);
	RPCRDMA_DEBUG(1, "xprt_rdma_send: Post a send buffer\n");
	if (ret) {
		WARN_ONCE(ret, "failed to drain send queue: %d\n", ret);
		rpcrdma_drain_ep(ep);
		return (EPIPE);
	}
	ret = wait_for_completion_timeout(&ep->re_send[ind].done, wtimeout);
	if (ret <= 0 || ep->re_send[ind].async_rc != 0) {
		printf("xprt_rdma_send: Send failed ret=%d async=%d\n",
		    ret, ep->re_send[ind].async_rc);
		rpcrdma_drain_ep(ep);
		return (EPIPE);
	}

	return (0);
}

/*
 * FRWR map a chunk.
 */
static int
rpcrdma_map_chunk(struct rpcrdma_ep *ep, struct _rpcrdma_chunk_priv *chp,
    bool map_seg)
{
	unsigned long wtimeout = msecs_to_jiffies(RDMA_RESOLVE_TIMEOUT) + 1;
	const struct ib_send_wr *badwr;
	struct rpcrdma_regwr *regwr, *tmp_regwr;
	struct ib_mr *mr;
	enum dma_data_direction dma_dir;
	int i, j, k, l, ret;
	uint32_t plen;
	uint8_t key;
	unsigned int mapped[RPCRDMA_MAX_SEGMENTS] = { 0 };

	RPCRDMA_DEBUG(3, "rpcrdma_map_chunk: map_seg=%d\n", map_seg);
	dma_dir = (chp->ch.into_mem) ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	/* An ib_reg_wr has a ib_send_wr in it. */
	plen = PAGE_SIZE;
	for (i = 0; i < chp->ch.num_segment; i++) {
		if (chp->ch.mextpg != NULL) {
			regwr = &ep->re_reply_regwr[chp->ch.ind][i];
			mr = ep->re_reply[chp->ch.ind][i];
		} else {
			regwr = &ep->re_regwr[chp->ch.ind][i];
			mr = ep->re_mr[chp->ch.ind][i];
		}
		RPCRDMA_DEBUG(4, "rpcrdma_map_chunk: ind=%d seg=%d mextpg=%p\n",
		    chp->ch.ind, i, chp->ch.mextpg);
		memset(&regwr->wr.wr, 0, sizeof(regwr->wr.wr));

		/* Map the sge(s) into bus dma space. */
		if (map_seg) {
			for (j = 0; j < chp->ch.sge_cnt[i]; j++) {
				if (j + 1 == chp->ch.sge_cnt[i] &&
				    i + 1 == chp->ch.num_segment)
					plen = chp->ch.last_len;
				for (k = 0; k < 1000; k++) {
					sg_dma_address(&chp->sge[i][j]) =
					    ib_dma_map_page(ep->re_id->device,
					    (vm_page_t)chp->sge[i][j].page_link,
					    0, plen, dma_dir);
					if (ib_dma_mapping_error(
					    ep->re_id->device,
					    sg_dma_address(&chp->sge[i][j]))==0)
						break;
					printf("rpcrdma_map_chunk: ib dma "
					    "mapping failed, will be "
					    "retried\n");
				}
				if (k == 1000)
					goto reg_err;
				mapped[i]++;
			}
			RPCRDMA_DEBUG(4, "rpcrdma_map_chunk: mapped seg%d "
			    "sgecnt=%d\n", i, chp->ch.sge_cnt[i]);
			ret = ib_map_mr_sg_zbva(mr, &chp->sge[i][0],
			    chp->ch.sge_cnt[i], NULL, PAGE_SIZE);
			RPCRDMA_DEBUG(4, "rpcrdma_map_chunk: ib_map_mr_sg_zbva"
			    " ret=%d\n", ret);
			if (ret != chp->ch.sge_cnt[i]) {
				RPCRDMA_DEBUG(4, "rpcrdma_map_chunk: "
				    "ib_map_mr_sg_zbva failed sgecnt=%d "
				    "ret=%d\n", chp->ch.sge_cnt[i], ret);
				goto reg_err;
			}
		}

		/* Set up keys, etc. */
		key = mr->rkey;
		ib_update_fast_reg_key(mr, ++key);
		chp->ch.handle[i] = mr->rkey;
		chp->ch.offset[i] = mr->iova;
		RPCRDMA_DEBUG(4, "rpcrdma_map_chunk: frwr handle=0x%x "
		    "offset=0x%jx length=%d\n", chp->ch.handle[i],
		    (uintmax_t)chp->ch.offset[i], chp->ch.length[i]);

		/* Register this mr. */
		regwr->wr.wr.next = NULL;
		regwr->wr.wr.wr_cqe = &regwr->cqe;
		regwr->cqe.done = rpcrdma_reg_wake;
		regwr->wr.wr.opcode = IB_WR_REG_MR;
		regwr->wr.mr = mr;
		regwr->wr.key = mr->rkey;
		if (chp->ch.into_mem)
			regwr->wr.access = IB_ACCESS_REMOTE_WRITE |
			    IB_ACCESS_LOCAL_WRITE;
		else
			regwr->wr.access = IB_ACCESS_REMOTE_READ;
		regwr->wr.wr.send_flags = 0;
	}
	for (i = 0; i < chp->ch.num_segment - 1; i++) {
		if (chp->ch.mextpg != NULL) {
			regwr = &ep->re_reply_regwr[chp->ch.ind][i];
			tmp_regwr = &ep->re_reply_regwr[chp->ch.ind][i + 1];
		} else {
			regwr = &ep->re_regwr[chp->ch.ind][i];
			tmp_regwr = &ep->re_regwr[chp->ch.ind][i + 1];
		}
		regwr->wr.wr.next = &tmp_regwr->wr.wr;
		regwr->cqe.done = rpcrdma_reg_done;
	}
	if (chp->ch.mextpg != NULL) {
		regwr = &ep->re_reply_regwr[chp->ch.ind][chp->ch.num_segment-1];
		tmp_regwr = &ep->re_reply_regwr[chp->ch.ind][0];
	} else {
		regwr = &ep->re_regwr[chp->ch.ind][chp->ch.num_segment-1];
		tmp_regwr = &ep->re_regwr[chp->ch.ind][0];
	}
	regwr->wr.wr.send_flags = IB_SEND_SIGNALED;
	init_completion(&regwr->done);
	regwr->async_rc = -ETIMEDOUT;
	ret = rpcrdma_post_send(ep, &tmp_regwr->wr.wr, &badwr);
	RPCRDMA_DEBUG(4, "rpcrdma_map_chunk: ib_post_send=%d\n", ret);
	if (ret != 0)
		goto reg_err;

	/* Now, wait for all the registration(s) to complete. */
	ret = wait_for_completion_timeout(&regwr->done, wtimeout);
	if (ret <= 0 || regwr->async_rc != 0) {
		printf("rpcrdma_map_chunk: frwr failed "
		    "ret=%d async_rc=%d\n", ret, regwr->async_rc);
		goto reg_err;
	}
	RPCRDMA_DEBUG(1, "rpcrdma_map_chunk: successful\n");
	return (0);
reg_err:
	/* A partial post or timeout can leave work using these mappings. */
	rpcrdma_drain_ep(ep);
	if (map_seg) {
		for (k = 0; k < chp->ch.num_segment; k++) {
			for (l = 0; l < mapped[k]; l++) {
				ib_dma_unmap_page(ep->re_id->device,
				    sg_dma_address(&chp->sge[k][l]),
				    chp->sge[k][l].length, dma_dir);
			}
		}
	}
	RPCRDMA_DEBUG(1, "rpcrdma_map_chunk: failed\n");
	return (-1);
}

/*
 * Create a chunk for RDMA, dma map it and associate it with an MR.
 * The chunk can be created from pages from any of these sources:
 * mextpg - If not NULL, it is a list of M_EXTPG mbufs with pages in them.
 * rb - If not NULL, this structure has a list of pages in it.
 */
struct rpcrdma_chunk *
xprt_rdma_create_chunk(struct rpcrdma_xprt *xp, uint32_t num_pg,
    struct rpcrdma_reduce_pg *rb, struct mbuf *mextpg, bool into_mem, int ind)
{
	struct _rpcrdma_chunk_priv *chp;
	struct rpcrdma_ep *ep;
	struct mbuf *m2;
	vm_page_t pg;
	uint32_t plen;
	int i, j, k;

	ep = rpcrdma_acquire_ep(xp, true);
	if (ep == NULL) {
		if (mextpg != NULL)
			m_freem(mextpg);
		return (NULL);
	}

	RPCRDMA_DEBUG(1, "xprt_rdma_create_chunk: num_pg=%d rb=%p mextpg=%p "
	    "into_mem=%d ind=%d\n", num_pg, rb, mextpg, into_mem, ind);
	chp = kzalloc(sizeof(*chp), GFP_KERNEL);
	chp->ch.ind = ind;

	/* Fill in the scatter list(s). */
	if (rb != NULL) {
		chp->ch.last_len = rb->len % PAGE_SIZE;
		if (chp->ch.last_len == 0)
			chp->ch.last_len = PAGE_SIZE;
		chp->ch.first_off = 0;
		chp->ch.into_mem = rb->into_mem;
		/* Fill in the scatter list(s) from the page vector. */
		for (i = 0, j = 0, k = 0; k < rb->npg; k++) {
			KASSERT(i < xp->maxsegs,
			    ("xprt_rdma_create_chunk: exceeds maxsegs rb"));
			if (k + 1 == rb->npg)
				plen = chp->ch.last_len;
			else
				plen = PAGE_SIZE;
			sg_set_page(&chp->sge[i][j], rb->pg[k], plen, 0);
			chp->ch.length[i] += plen;
			if (++j == xp->maxsge) {
				chp->ch.sge_cnt[i] = j;
				j = 0;
				i++;
			}
		}
		if (j != 0) {
			chp->ch.sge_cnt[i] = j;
			i++;
		}
		chp->ch.num_segment = i;
		chp->rbpages = rb->npg;
		RPCRDMA_DEBUG(3, "xprt_rdma_create_chunk: num_seg=%d "
		    "rb->npg=%d\n", i, rb->npg);
	} else if (mextpg != NULL) {
		for (i = 0, j = 0, m2 = mextpg; m2 != NULL; m2 = m2->m_next) {
			KASSERT((m2->m_flags & M_EXTPG) != 0,
			    ("clnt_rmda_create_chunk: non-m_extpg mbuf"));
			if (mextpg == m2)
				chp->ch.first_off = m2->m_epg_1st_off;
			plen = PAGE_SIZE;
			for (k = 0; k < m2->m_epg_npgs; k++) {
				KASSERT(i < xp->maxsegs, ("xprt_rdma_create_"
				    "chunk: exceeds maxsegs mextpg"));
				if (m2->m_next == NULL &&
				    k == m2->m_epg_npgs - 1) {
					chp->ch.last_len = plen =
					    m2->m_epg_last_len;
				}
				if (k == 0)
					chp->ch.length[i] += (plen -
					    chp->ch.first_off);
				else
					chp->ch.length[i] += plen;
				pg = PHYS_TO_VM_PAGE(m2->m_epg_pa[k]);
				sg_set_page(&chp->sge[i][j], pg, plen, 0);
				if (++j == xp->maxsge) {
					chp->ch.sge_cnt[i] = j;
					j = 0;
					i++;
				}
				plen = PAGE_SIZE;
			}
		}
		if (j != 0) {
			chp->ch.sge_cnt[i] = j;
			i++;
		}
		chp->ch.num_segment = i;
		chp->ch.into_mem = (into_mem) ? TRUE : FALSE;
		chp->ch.mextpg = mextpg;
		RPCRDMA_DEBUG(3, "xprt_rdma_create_chunk: num_seg=%d "
		    "mextpg=%p into_mem=%d\n", i, mextpg, into_mem);
	} else {
		goto errout2;
	}

	i = rpcrdma_map_chunk(ep, chp, true);
	RPCRDMA_DEBUG(3, "xprt_rdma_create_chunk: rpcrdma_map_chunk=%d\n", i);
	if (i != 0)
		goto errout;
	/* The caller keeps its send slot until all chunks are released. */
	RPCRDMA_DEBUG(1, "xprt_rdma_create_chunk: succeeded\n");
	return (&chp->ch);
errout:
	RPCRDMA_DEBUG(1, "xprt_rdma_create_chunk: failed\n");
	if (mextpg != NULL)
		m_freem(mextpg);
errout2:
	kfree(chp);
	return (NULL);
}

/* Check the re_connect_status. */
int
xprt_rdma_isdisconnected(struct rpcrdma_xprt *xp)
{
	struct rpcrdma_ep *ep;

	mtx_assert(&xp->mtx, MA_OWNED);
	ep = (struct rpcrdma_ep *)xp->ep;
	if (ep == NULL || ep->re_connect_status != 0)
		return (ECONNABORTED);
	return (0);
}

/* Mark the connection broken. */
void
xprt_rdma_mark_disconnected(struct rpcrdma_xprt *xp)
{
	struct rpcrdma_ep *ep;

	mtx_assert(&xp->mtx, MA_OWNED);
	ep = (struct rpcrdma_ep *)xp->ep;
	if (ep == NULL)
		return;
	ep->re_connect_status = -ECONNABORTED;
	wakeup(&ep->re_busy);
}

