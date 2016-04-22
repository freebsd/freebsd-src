/*
 * Copyright (c) 2005 Ammasso, Inc. All rights reserved.
 * Copyright (c) 2006-2009 Open Grid Computing, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/in.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/sched.h>

#include <asm/atomic.h>

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#include "krping.h"
#include "getopt.h"

extern int krping_debug;
#define DEBUG_LOG(cb, x...) if (krping_debug) log(LOG_INFO, x)
#define PRINTF(cb, x...) log(LOG_INFO, x)
#define BIND_INFO 1

MODULE_AUTHOR("Steve Wise");
MODULE_DESCRIPTION("RDMA ping client/server");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(krping, 1);
MODULE_DEPEND(krping, linuxkpi, 1, 1, 1);

static __inline uint64_t
get_cycles(void)
{
	uint32_t low, high;
	__asm __volatile("rdtsc" : "=a" (low), "=d" (high));
	return (low | ((u_int64_t)high << 32));
}

typedef uint64_t cycles_t;

enum mem_type {
	DMA = 1,
	FASTREG = 2,
	MW = 3,
	MR = 4
};

static const struct krping_option krping_opts[] = {
	{"count", OPT_INT, 'C'},
	{"size", OPT_INT, 'S'},
	{"addr", OPT_STRING, 'a'},
	{"port", OPT_INT, 'p'},
	{"verbose", OPT_NOPARAM, 'v'},
	{"validate", OPT_NOPARAM, 'V'},
	{"server", OPT_NOPARAM, 's'},
	{"client", OPT_NOPARAM, 'c'},
	{"mem_mode", OPT_STRING, 'm'},
	{"server_inv", OPT_NOPARAM, 'I'},
 	{"wlat", OPT_NOPARAM, 'l'},
 	{"rlat", OPT_NOPARAM, 'L'},
 	{"bw", OPT_NOPARAM, 'B'},
 	{"duplex", OPT_NOPARAM, 'd'},
 	{"txdepth", OPT_INT, 'T'},
 	{"poll", OPT_NOPARAM, 'P'},
 	{"local_dma_lkey", OPT_NOPARAM, 'Z'},
 	{"read_inv", OPT_NOPARAM, 'R'},
 	{"fr", OPT_INT, 'f'},
	{NULL, 0, 0}
};

#define htonll(x) cpu_to_be64((x))
#define ntohll(x) cpu_to_be64((x))

static struct mutex krping_mutex;

/*
 * List of running krping threads.
 */
static LIST_HEAD(krping_cbs);

/*
 * krping "ping/pong" loop:
 * 	client sends source rkey/addr/len
 *	server receives source rkey/add/len
 *	server rdma reads "ping" data from source
 * 	server sends "go ahead" on rdma read completion
 *	client sends sink rkey/addr/len
 * 	server receives sink rkey/addr/len
 * 	server rdma writes "pong" data to sink
 * 	server sends "go ahead" on rdma write completion
 * 	<repeat loop>
 */

/*
 * These states are used to signal events between the completion handler
 * and the main client or server thread.
 *
 * Once CONNECTED, they cycle through RDMA_READ_ADV, RDMA_WRITE_ADV,
 * and RDMA_WRITE_COMPLETE for each ping.
 */
enum test_state {
	IDLE = 1,
	CONNECT_REQUEST,
	ADDR_RESOLVED,
	ROUTE_RESOLVED,
	CONNECTED,
	RDMA_READ_ADV,
	RDMA_READ_COMPLETE,
	RDMA_WRITE_ADV,
	RDMA_WRITE_COMPLETE,
	ERROR
};

struct krping_rdma_info {
	uint64_t buf;
	uint32_t rkey;
	uint32_t size;
};

/*
 * Default max buffer size for IO...
 */
#define RPING_BUFSIZE 128*1024
#define RPING_SQ_DEPTH 64

/*
 * Control block struct.
 */
struct krping_cb {
	void *cookie;
	int server;			/* 0 iff client */
	struct ib_cq *cq;
	struct ib_pd *pd;
	struct ib_qp *qp;

	enum mem_type mem;
	struct ib_mr *dma_mr;

	struct ib_fast_reg_page_list *page_list;
	int page_list_len;
	struct ib_send_wr fastreg_wr;
	struct ib_send_wr invalidate_wr;
	struct ib_mr *fastreg_mr;
	int server_invalidate;
	int read_inv;
	u8 key;

	struct ib_mw *mw;
	struct ib_mw_bind bind_attr;

	struct ib_recv_wr rq_wr;	/* recv work request record */
	struct ib_sge recv_sgl;		/* recv single SGE */
	struct krping_rdma_info recv_buf;/* malloc'd buffer */
	u64 recv_dma_addr;
	DECLARE_PCI_UNMAP_ADDR(recv_mapping)
	struct ib_mr *recv_mr;

	struct ib_send_wr sq_wr;	/* send work requrest record */
	struct ib_sge send_sgl;
	struct krping_rdma_info send_buf;/* single send buf */
	u64 send_dma_addr;
	DECLARE_PCI_UNMAP_ADDR(send_mapping)
	struct ib_mr *send_mr;

	struct ib_send_wr rdma_sq_wr;	/* rdma work request record */
	struct ib_sge rdma_sgl;		/* rdma single SGE */
	char *rdma_buf;			/* used as rdma sink */
	u64  rdma_dma_addr;
	DECLARE_PCI_UNMAP_ADDR(rdma_mapping)
	struct ib_mr *rdma_mr;

	uint32_t remote_rkey;		/* remote guys RKEY */
	uint64_t remote_addr;		/* remote guys TO */
	uint32_t remote_len;		/* remote guys LEN */

	char *start_buf;		/* rdma read src */
	u64  start_dma_addr;
	DECLARE_PCI_UNMAP_ADDR(start_mapping)
	struct ib_mr *start_mr;

	enum test_state state;		/* used for cond/signalling */
	wait_queue_head_t sem;
	struct krping_stats stats;

	uint16_t port;			/* dst port in NBO */
	struct in_addr addr;		/* dst addr in NBO */
	char *addr_str;			/* dst addr string */
	int verbose;			/* verbose logging */
	int count;			/* ping count */
	int size;			/* ping data size */
	int validate;			/* validate ping data */
	int wlat;			/* run wlat test */
	int rlat;			/* run rlat test */
	int bw;				/* run bw test */
	int duplex;			/* run bw full duplex test */
	int poll;			/* poll or block for rlat test */
	int txdepth;			/* SQ depth */
	int local_dma_lkey;		/* use 0 for lkey */
	int frtest;			/* fastreg test */
	int testnum;

	/* CM stuff */
	struct rdma_cm_id *cm_id;	/* connection on client side,*/
					/* listener on server side. */
	struct rdma_cm_id *child_cm_id;	/* connection on server side */
	struct list_head list;
};

static int krping_cma_event_handler(struct rdma_cm_id *cma_id,
				   struct rdma_cm_event *event)
{
	int ret;
	struct krping_cb *cb = cma_id->context;

	DEBUG_LOG(cb, "cma_event type %d cma_id %p (%s)\n", event->event,
	    cma_id, (cma_id == cb->cm_id) ? "parent" : "child");

	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		cb->state = ADDR_RESOLVED;
		ret = rdma_resolve_route(cma_id, 2000);
		if (ret) {
			PRINTF(cb, "rdma_resolve_route error %d\n", ret);
			wake_up_interruptible(&cb->sem);
		}
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		cb->state = ROUTE_RESOLVED;
		cb->child_cm_id = cma_id;
		wake_up_interruptible(&cb->sem);
		break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
		if (cb->state == IDLE) {
			cb->state = CONNECT_REQUEST;
			cb->child_cm_id = cma_id;
		} else {
			PRINTF(cb, "Received connection request in wrong state"
			    " (%d)\n", cb->state);
		}
		DEBUG_LOG(cb, "child cma %p\n", cb->child_cm_id);
		wake_up_interruptible(&cb->sem);
		break;

	case RDMA_CM_EVENT_ESTABLISHED:
		DEBUG_LOG(cb, "ESTABLISHED\n");
		if (!cb->server) {
			cb->state = CONNECTED;
		}
		wake_up_interruptible(&cb->sem);
		break;

	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
	case RDMA_CM_EVENT_REJECTED:
		PRINTF(cb, "cma event %d, error %d\n", event->event,
		       event->status);
		cb->state = ERROR;
		wake_up_interruptible(&cb->sem);
		break;

	case RDMA_CM_EVENT_DISCONNECTED:
		PRINTF(cb, "DISCONNECT EVENT...\n");
		cb->state = ERROR;
		wake_up_interruptible(&cb->sem);
		break;

	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		PRINTF(cb, "cma detected device removal!!!!\n");
		break;

	default:
		PRINTF(cb, "oof bad type!\n");
		wake_up_interruptible(&cb->sem);
		break;
	}
	return 0;
}

static int server_recv(struct krping_cb *cb, struct ib_wc *wc)
{
	if (wc->byte_len != sizeof(cb->recv_buf)) {
		PRINTF(cb, "Received bogus data, size %d\n", 
		       wc->byte_len);
		return -1;
	}

	cb->remote_rkey = ntohl(cb->recv_buf.rkey);
	cb->remote_addr = ntohll(cb->recv_buf.buf);
	cb->remote_len  = ntohl(cb->recv_buf.size);
	DEBUG_LOG(cb, "Received rkey %x addr %llx len %d from peer\n",
		  cb->remote_rkey, (unsigned long long)cb->remote_addr, 
		  cb->remote_len);

	if (cb->state <= CONNECTED || cb->state == RDMA_WRITE_COMPLETE)
		cb->state = RDMA_READ_ADV;
	else
		cb->state = RDMA_WRITE_ADV;

	return 0;
}

static int client_recv(struct krping_cb *cb, struct ib_wc *wc)
{
	if (wc->byte_len != sizeof(cb->recv_buf)) {
		PRINTF(cb, "Received bogus data, size %d\n", 
		       wc->byte_len);
		return -1;
	}

	if (cb->state == RDMA_READ_ADV)
		cb->state = RDMA_WRITE_ADV;
	else
		cb->state = RDMA_WRITE_COMPLETE;

	return 0;
}

static void krping_cq_event_handler(struct ib_cq *cq, void *ctx)
{
	struct krping_cb *cb = ctx;
	struct ib_wc wc;
	struct ib_recv_wr *bad_wr;
	int ret;

	BUG_ON(cb->cq != cq);
	if (cb->state == ERROR) {
		PRINTF(cb, "cq completion in ERROR state\n");
		return;
	}
	if (!cb->wlat && !cb->rlat && !cb->bw && !cb->frtest)
		ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
	while ((ret = ib_poll_cq(cb->cq, 1, &wc)) == 1) {
		if (wc.status) {
			if (wc.status == IB_WC_WR_FLUSH_ERR) {
				DEBUG_LOG(cb, "cq flushed\n");
				continue;
			} else {
				PRINTF(cb, "cq completion failed with "
				       "wr_id %jx status %d opcode %d vender_err %x\n",
					(uintmax_t)wc.wr_id, wc.status, wc.opcode, wc.vendor_err);
				goto error;
			}
		}

		switch (wc.opcode) {
		case IB_WC_SEND:
			DEBUG_LOG(cb, "send completion\n");
			cb->stats.send_bytes += cb->send_sgl.length;
			cb->stats.send_msgs++;
			break;

		case IB_WC_RDMA_WRITE:
			DEBUG_LOG(cb, "rdma write completion\n");
			cb->stats.write_bytes += cb->rdma_sq_wr.sg_list->length;
			cb->stats.write_msgs++;
			cb->state = RDMA_WRITE_COMPLETE;
			wake_up_interruptible(&cb->sem);
			break;

		case IB_WC_RDMA_READ:
			DEBUG_LOG(cb, "rdma read completion\n");
			cb->stats.read_bytes += cb->rdma_sq_wr.sg_list->length;
			cb->stats.read_msgs++;
			cb->state = RDMA_READ_COMPLETE;
			wake_up_interruptible(&cb->sem);
			break;

		case IB_WC_RECV:
			DEBUG_LOG(cb, "recv completion\n");
			cb->stats.recv_bytes += sizeof(cb->recv_buf);
			cb->stats.recv_msgs++;
			if (cb->wlat || cb->rlat || cb->bw || cb->frtest)
				ret = server_recv(cb, &wc);
			else
				ret = cb->server ? server_recv(cb, &wc) :
						   client_recv(cb, &wc);
			if (ret) {
				PRINTF(cb, "recv wc error: %d\n", ret);
				goto error;
			}

			ret = ib_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
			if (ret) {
				PRINTF(cb, "post recv error: %d\n", 
				       ret);
				goto error;
			}
			wake_up_interruptible(&cb->sem);
			break;

		default:
			PRINTF(cb, 
			       "%s:%d Unexpected opcode %d, Shutting down\n",
			       __func__, __LINE__, wc.opcode);
			goto error;
		}
	}
	if (ret) {
		PRINTF(cb, "poll error %d\n", ret);
		goto error;
	}
	return;
error:
	cb->state = ERROR;
	wake_up_interruptible(&cb->sem);
}

static int krping_accept(struct krping_cb *cb)
{
	struct rdma_conn_param conn_param;
	int ret;

	DEBUG_LOG(cb, "accepting client connection request\n");

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

	ret = rdma_accept(cb->child_cm_id, &conn_param);
	if (ret) {
		PRINTF(cb, "rdma_accept error: %d\n", ret);
		return ret;
	}

	if (!cb->wlat && !cb->rlat && !cb->bw && !cb->frtest) {
		wait_event_interruptible(cb->sem, cb->state >= CONNECTED);
		if (cb->state == ERROR) {
			PRINTF(cb, "wait for CONNECTED state %d\n", 
				cb->state);
			return -1;
		}
	}
	return 0;
}

static void krping_setup_wr(struct krping_cb *cb)
{
	cb->recv_sgl.addr = cb->recv_dma_addr;
	cb->recv_sgl.length = sizeof cb->recv_buf;
	if (cb->local_dma_lkey)
		cb->recv_sgl.lkey = cb->qp->device->local_dma_lkey;
	else if (cb->mem == DMA)
		cb->recv_sgl.lkey = cb->dma_mr->lkey;
	else
		cb->recv_sgl.lkey = cb->recv_mr->lkey;
	cb->rq_wr.sg_list = &cb->recv_sgl;
	cb->rq_wr.num_sge = 1;

	cb->send_sgl.addr = cb->send_dma_addr;
	cb->send_sgl.length = sizeof cb->send_buf;
	if (cb->local_dma_lkey)
		cb->send_sgl.lkey = cb->qp->device->local_dma_lkey;
	else if (cb->mem == DMA)
		cb->send_sgl.lkey = cb->dma_mr->lkey;
	else
		cb->send_sgl.lkey = cb->send_mr->lkey;

	cb->sq_wr.opcode = IB_WR_SEND;
	cb->sq_wr.send_flags = IB_SEND_SIGNALED;
	cb->sq_wr.sg_list = &cb->send_sgl;
	cb->sq_wr.num_sge = 1;

	if (cb->server || cb->wlat || cb->rlat || cb->bw || cb->frtest) {
		cb->rdma_sgl.addr = cb->rdma_dma_addr;
		if (cb->mem == MR)
			cb->rdma_sgl.lkey = cb->rdma_mr->lkey;
		cb->rdma_sq_wr.send_flags = IB_SEND_SIGNALED;
		cb->rdma_sq_wr.sg_list = &cb->rdma_sgl;
		cb->rdma_sq_wr.num_sge = 1;
	}

	switch(cb->mem) {
	case FASTREG:

		/* 
		 * A chain of 2 WRs, INVALDATE_MR + FAST_REG_MR.
		 * both unsignaled.  The client uses them to reregister
		 * the rdma buffers with a new key each iteration.
		 */
		cb->fastreg_wr.opcode = IB_WR_FAST_REG_MR;
		cb->fastreg_wr.wr.fast_reg.page_shift = PAGE_SHIFT;
		cb->fastreg_wr.wr.fast_reg.length = cb->size;
		cb->fastreg_wr.wr.fast_reg.page_list = cb->page_list;
		cb->fastreg_wr.wr.fast_reg.page_list_len = cb->page_list_len;

		cb->invalidate_wr.next = &cb->fastreg_wr;
		cb->invalidate_wr.opcode = IB_WR_LOCAL_INV;
		break;
	case MW:
		cb->bind_attr.wr_id = 0xabbaabba;
		cb->bind_attr.send_flags = 0; /* unsignaled */
#ifdef BIND_INFO
		cb->bind_attr.bind_info.length = cb->size;
#else
		cb->bind_attr.length = cb->size;
#endif
		break;
	default:
		break;
	}
}

static int krping_setup_buffers(struct krping_cb *cb)
{
	int ret;
	struct ib_phys_buf buf;
	u64 iovbase;

	DEBUG_LOG(cb, "krping_setup_buffers called on cb %p\n", cb);

	cb->recv_dma_addr = dma_map_single(cb->pd->device->dma_device, 
				   &cb->recv_buf, 
				   sizeof(cb->recv_buf), DMA_BIDIRECTIONAL);
	pci_unmap_addr_set(cb, recv_mapping, cb->recv_dma_addr);
	cb->send_dma_addr = dma_map_single(cb->pd->device->dma_device, 
					   &cb->send_buf, sizeof(cb->send_buf),
					   DMA_BIDIRECTIONAL);
	pci_unmap_addr_set(cb, send_mapping, cb->send_dma_addr);

	if (cb->mem == DMA) {
		cb->dma_mr = ib_get_dma_mr(cb->pd, IB_ACCESS_LOCAL_WRITE|
					   IB_ACCESS_REMOTE_READ|
				           IB_ACCESS_REMOTE_WRITE);
		if (IS_ERR(cb->dma_mr)) {
			DEBUG_LOG(cb, "reg_dmamr failed\n");
			ret = PTR_ERR(cb->dma_mr);
			goto bail;
		}
	} else {
		if (!cb->local_dma_lkey) {
			buf.addr = cb->recv_dma_addr;
			buf.size = sizeof cb->recv_buf;
			DEBUG_LOG(cb, "recv buf dma_addr %jx size %d\n",
			    (uintmax_t)buf.addr, (int)buf.size);
			iovbase = cb->recv_dma_addr;
			cb->recv_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
						     IB_ACCESS_LOCAL_WRITE, 
						     &iovbase);

			if (IS_ERR(cb->recv_mr)) {
				DEBUG_LOG(cb, "recv_buf reg_mr failed\n");
				ret = PTR_ERR(cb->recv_mr);
				goto bail;
			}

			buf.addr = cb->send_dma_addr;
			buf.size = sizeof cb->send_buf;
			DEBUG_LOG(cb, "send buf dma_addr %jx size %d\n",
			    (uintmax_t)buf.addr, (int)buf.size);
			iovbase = cb->send_dma_addr;
			cb->send_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
						     0, &iovbase);

			if (IS_ERR(cb->send_mr)) {
				DEBUG_LOG(cb, "send_buf reg_mr failed\n");
				ret = PTR_ERR(cb->send_mr);
				goto bail;
			}
		}
	}

	cb->rdma_buf = kmalloc(cb->size, GFP_KERNEL);
	if (!cb->rdma_buf) {
		DEBUG_LOG(cb, "rdma_buf malloc failed\n");
		ret = -ENOMEM;
		goto bail;
	}

	cb->rdma_dma_addr = dma_map_single(cb->pd->device->dma_device, 
			       cb->rdma_buf, cb->size, 
			       DMA_BIDIRECTIONAL);
	pci_unmap_addr_set(cb, rdma_mapping, cb->rdma_dma_addr);
	if (cb->mem != DMA) {
		switch (cb->mem) {
		case FASTREG:
			cb->page_list_len = (((cb->size - 1) & PAGE_MASK) +
				PAGE_SIZE) >> PAGE_SHIFT;
			cb->page_list = ib_alloc_fast_reg_page_list(
						cb->pd->device, 
						cb->page_list_len);
			if (IS_ERR(cb->page_list)) {
				DEBUG_LOG(cb, "recv_buf reg_mr failed\n");
				ret = PTR_ERR(cb->page_list);
				goto bail;
			}
			cb->fastreg_mr = ib_alloc_fast_reg_mr(cb->pd, 
					cb->page_list->max_page_list_len);
			if (IS_ERR(cb->fastreg_mr)) {
				DEBUG_LOG(cb, "recv_buf reg_mr failed\n");
				ret = PTR_ERR(cb->fastreg_mr);
				goto bail;
			}
			DEBUG_LOG(cb, "fastreg rkey 0x%x page_list %p"
				" page_list_len %u\n", cb->fastreg_mr->rkey, 
				cb->page_list, cb->page_list_len);
			break;
		case MW:
			cb->mw = ib_alloc_mw(cb->pd,IB_MW_TYPE_1);
			if (IS_ERR(cb->mw)) {
				DEBUG_LOG(cb, "recv_buf alloc_mw failed\n");
				ret = PTR_ERR(cb->mw);
				goto bail;
			}
			DEBUG_LOG(cb, "mw rkey 0x%x\n", cb->mw->rkey);
			/*FALLTHROUGH*/
		case MR:
			buf.addr = cb->rdma_dma_addr;
			buf.size = cb->size;
			iovbase = cb->rdma_dma_addr;
			cb->rdma_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
						IB_ACCESS_LOCAL_WRITE|
					     IB_ACCESS_REMOTE_READ| 
					     IB_ACCESS_REMOTE_WRITE, 
					     &iovbase);
			if (IS_ERR(cb->rdma_mr)) {
				DEBUG_LOG(cb, "rdma_buf reg_mr failed\n");
				ret = PTR_ERR(cb->rdma_mr);
				goto bail;
			}
			DEBUG_LOG(cb, "rdma buf dma_addr %jx size %d mr rkey 0x%x\n",
				(uintmax_t)buf.addr, (int)buf.size, cb->rdma_mr->rkey);
			break;
		default:
			ret = -EINVAL;
			goto bail;
			break;
		}
	}

	if (!cb->server || cb->wlat || cb->rlat || cb->bw || cb->frtest) {

		cb->start_buf = kmalloc(cb->size, GFP_KERNEL);
		if (!cb->start_buf) {
			DEBUG_LOG(cb, "start_buf malloc failed\n");
			ret = -ENOMEM;
			goto bail;
		}

		cb->start_dma_addr = dma_map_single(cb->pd->device->dma_device, 
						   cb->start_buf, cb->size, 
						   DMA_BIDIRECTIONAL);
		pci_unmap_addr_set(cb, start_mapping, cb->start_dma_addr);

		if (cb->mem == MR || cb->mem == MW) {
			unsigned flags = IB_ACCESS_REMOTE_READ;

			if (cb->wlat || cb->rlat || cb->bw || cb->frtest) {
				flags |= IB_ACCESS_LOCAL_WRITE |
					IB_ACCESS_REMOTE_WRITE;
			}

			buf.addr = cb->start_dma_addr;
			buf.size = cb->size;
			DEBUG_LOG(cb, "start buf dma_addr %jx size %d\n",
				(uintmax_t)buf.addr, (int)buf.size);
			iovbase = cb->start_dma_addr;
			cb->start_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
					     flags,
					     &iovbase);

			if (IS_ERR(cb->start_mr)) {
				DEBUG_LOG(cb, "start_buf reg_mr failed\n");
				ret = PTR_ERR(cb->start_mr);
				goto bail;
			}
		}
	}

	krping_setup_wr(cb);
	DEBUG_LOG(cb, "allocated & registered buffers...\n");
	return 0;
bail:
	if (cb->fastreg_mr && !IS_ERR(cb->fastreg_mr))
		ib_dereg_mr(cb->fastreg_mr);
	if (cb->mw && !IS_ERR(cb->mw))
		ib_dealloc_mw(cb->mw);
	if (cb->rdma_mr && !IS_ERR(cb->rdma_mr))
		ib_dereg_mr(cb->rdma_mr);
	if (cb->page_list && !IS_ERR(cb->page_list))
		ib_free_fast_reg_page_list(cb->page_list);
	if (cb->dma_mr && !IS_ERR(cb->dma_mr))
		ib_dereg_mr(cb->dma_mr);
	if (cb->recv_mr && !IS_ERR(cb->recv_mr))
		ib_dereg_mr(cb->recv_mr);
	if (cb->send_mr && !IS_ERR(cb->send_mr))
		ib_dereg_mr(cb->send_mr);
	if (cb->rdma_buf)
		kfree(cb->rdma_buf);
	if (cb->start_buf)
		kfree(cb->start_buf);
	return ret;
}

static void krping_free_buffers(struct krping_cb *cb)
{
	DEBUG_LOG(cb, "krping_free_buffers called on cb %p\n", cb);
	
	if (cb->dma_mr)
		ib_dereg_mr(cb->dma_mr);
	if (cb->send_mr)
		ib_dereg_mr(cb->send_mr);
	if (cb->recv_mr)
		ib_dereg_mr(cb->recv_mr);
	if (cb->rdma_mr)
		ib_dereg_mr(cb->rdma_mr);
	if (cb->start_mr)
		ib_dereg_mr(cb->start_mr);
	if (cb->fastreg_mr)
		ib_dereg_mr(cb->fastreg_mr);
	if (cb->mw)
		ib_dealloc_mw(cb->mw);

	dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, recv_mapping),
			 sizeof(cb->recv_buf), DMA_BIDIRECTIONAL);
	dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, send_mapping),
			 sizeof(cb->send_buf), DMA_BIDIRECTIONAL);
	dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, rdma_mapping),
			 cb->size, DMA_BIDIRECTIONAL);
	kfree(cb->rdma_buf);
	if (cb->start_buf) {
		dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, start_mapping),
			 cb->size, DMA_BIDIRECTIONAL);
		kfree(cb->start_buf);
	}
}

static int krping_create_qp(struct krping_cb *cb)
{
	struct ib_qp_init_attr init_attr;
	int ret;

	memset(&init_attr, 0, sizeof(init_attr));
	init_attr.cap.max_send_wr = cb->txdepth;
	init_attr.cap.max_recv_wr = 2;
	init_attr.cap.max_recv_sge = 1;
	init_attr.cap.max_send_sge = 1;
	init_attr.qp_type = IB_QPT_RC;
	init_attr.send_cq = cb->cq;
	init_attr.recv_cq = cb->cq;
	init_attr.sq_sig_type = IB_SIGNAL_REQ_WR;

	if (cb->server) {
		ret = rdma_create_qp(cb->child_cm_id, cb->pd, &init_attr);
		if (!ret)
			cb->qp = cb->child_cm_id->qp;
	} else {
		ret = rdma_create_qp(cb->cm_id, cb->pd, &init_attr);
		if (!ret)
			cb->qp = cb->cm_id->qp;
	}

	return ret;
}

static void krping_free_qp(struct krping_cb *cb)
{
	ib_destroy_qp(cb->qp);
	ib_destroy_cq(cb->cq);
	ib_dealloc_pd(cb->pd);
}

static int krping_setup_qp(struct krping_cb *cb, struct rdma_cm_id *cm_id)
{
	int ret;
	cb->pd = ib_alloc_pd(cm_id->device);
	if (IS_ERR(cb->pd)) {
		PRINTF(cb, "ib_alloc_pd failed\n");
		return PTR_ERR(cb->pd);
	}
	DEBUG_LOG(cb, "created pd %p\n", cb->pd);

	strlcpy(cb->stats.name, cb->pd->device->name, sizeof(cb->stats.name));

	cb->cq = ib_create_cq(cm_id->device, krping_cq_event_handler, NULL,
			      cb, cb->txdepth * 2, 0);
	if (IS_ERR(cb->cq)) {
		PRINTF(cb, "ib_create_cq failed\n");
		ret = PTR_ERR(cb->cq);
		goto err1;
	}
	DEBUG_LOG(cb, "created cq %p\n", cb->cq);

	if (!cb->wlat && !cb->rlat && !cb->bw && !cb->frtest) {
		ret = ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
		if (ret) {
			PRINTF(cb, "ib_create_cq failed\n");
			goto err2;
		}
	}

	ret = krping_create_qp(cb);
	if (ret) {
		PRINTF(cb, "krping_create_qp failed: %d\n", ret);
		goto err2;
	}
	DEBUG_LOG(cb, "created qp %p\n", cb->qp);
	return 0;
err2:
	ib_destroy_cq(cb->cq);
err1:
	ib_dealloc_pd(cb->pd);
	return ret;
}

/*
 * return the (possibly rebound) rkey for the rdma buffer.
 * FASTREG mode: invalidate and rebind via fastreg wr.
 * MW mode: rebind the MW.
 * other modes: just return the mr rkey.
 */
static u32 krping_rdma_rkey(struct krping_cb *cb, u64 buf, int post_inv)
{
	u32 rkey = 0xffffffff;
	u64 p;
	struct ib_send_wr *bad_wr;
	int i;
	int ret;

	switch (cb->mem) {
	case FASTREG:
		cb->invalidate_wr.ex.invalidate_rkey = cb->fastreg_mr->rkey;

		/*
		 * Update the fastreg key.
		 */
		ib_update_fast_reg_key(cb->fastreg_mr, ++cb->key);
		cb->fastreg_wr.wr.fast_reg.rkey = cb->fastreg_mr->rkey;

		/*
		 * Update the fastreg WR with new buf info.
		 */
		if (buf == (u64)cb->start_dma_addr)
			cb->fastreg_wr.wr.fast_reg.access_flags = IB_ACCESS_REMOTE_READ;
		else
			cb->fastreg_wr.wr.fast_reg.access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
		cb->fastreg_wr.wr.fast_reg.iova_start = buf;
		p = (u64)(buf & PAGE_MASK);
		for (i=0; i < cb->fastreg_wr.wr.fast_reg.page_list_len; 
		     i++, p += PAGE_SIZE) {
			cb->page_list->page_list[i] = p;
			DEBUG_LOG(cb, "page_list[%d] 0x%jx\n", i, (uintmax_t)p);
		}

		DEBUG_LOG(cb, "post_inv = %d, fastreg new rkey 0x%x shift %u len %u"
			" iova_start %jx page_list_len %u\n",
			post_inv,
			cb->fastreg_wr.wr.fast_reg.rkey,
			cb->fastreg_wr.wr.fast_reg.page_shift,
			(unsigned)cb->fastreg_wr.wr.fast_reg.length,
			(uintmax_t)cb->fastreg_wr.wr.fast_reg.iova_start,
			cb->fastreg_wr.wr.fast_reg.page_list_len);

		if (post_inv)
			ret = ib_post_send(cb->qp, &cb->invalidate_wr, &bad_wr);
		else
			ret = ib_post_send(cb->qp, &cb->fastreg_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, "post send error %d\n", ret);
			cb->state = ERROR;
		}
		rkey = cb->fastreg_mr->rkey;
		break;
	case MW:
		/*
		 * Update the MW with new buf info.
		 */
		if (buf == (u64)cb->start_dma_addr) {
#ifdef BIND_INFO
			cb->bind_attr.bind_info.mw_access_flags = IB_ACCESS_REMOTE_READ;
			cb->bind_attr.bind_info.mr = cb->start_mr;
#else
			cb->bind_attr.mw_access_flags = IB_ACCESS_REMOTE_READ;
			cb->bind_attr.mr = cb->start_mr;
#endif
		} else {
#ifdef BIND_INFO
			cb->bind_attr.bind_info.mw_access_flags = IB_ACCESS_REMOTE_WRITE;
			cb->bind_attr.bind_info.mr = cb->rdma_mr;
#else
			cb->bind_attr.mw_access_flags = IB_ACCESS_REMOTE_WRITE;
			cb->bind_attr.mr = cb->rdma_mr;
#endif
		}
#ifdef BIND_INFO
		cb->bind_attr.bind_info.addr = buf;
#else
		cb->bind_attr.addr = buf;
#endif
		DEBUG_LOG(cb, "binding mw rkey 0x%x to buf %jx mr rkey 0x%x\n",
#ifdef BIND_INFO
			cb->mw->rkey, (uintmax_t)buf, cb->bind_attr.bind_info.mr->rkey);
#else
			cb->mw->rkey, buf, cb->bind_attr.mr->rkey);
#endif
		ret = ib_bind_mw(cb->qp, cb->mw, &cb->bind_attr);
		if (ret) {
			PRINTF(cb, "bind mw error %d\n", ret);
			cb->state = ERROR;
		} else
			rkey = cb->mw->rkey;
		break;
	case MR:
		if (buf == (u64)cb->start_dma_addr)
			rkey = cb->start_mr->rkey;
		else
			rkey = cb->rdma_mr->rkey;
		break;
	case DMA:
		rkey = cb->dma_mr->rkey;
		break;
	default:
		PRINTF(cb, "%s:%d case ERROR\n", __func__, __LINE__);
		cb->state = ERROR;
		break;
	}
	return rkey;
}

static void krping_format_send(struct krping_cb *cb, u64 buf)
{
	struct krping_rdma_info *info = &cb->send_buf;
	u32 rkey;

	/*
	 * Client side will do fastreg or mw bind before
	 * advertising the rdma buffer.  Server side
	 * sends have no data.
	 */
	if (!cb->server || cb->wlat || cb->rlat || cb->bw || cb->frtest) {
		rkey = krping_rdma_rkey(cb, buf, !cb->server_invalidate);
		info->buf = htonll(buf);
		info->rkey = htonl(rkey);
		info->size = htonl(cb->size);
		DEBUG_LOG(cb, "RDMA addr %llx rkey %x len %d\n",
			  (unsigned long long)buf, rkey, cb->size);
	}
}

static void krping_test_server(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr, inv;
	int ret;

	while (1) {
		/* Wait for client's Start STAG/TO/Len */
		wait_event_interruptible(cb->sem, cb->state >= RDMA_READ_ADV);
		if (cb->state != RDMA_READ_ADV) {
			PRINTF(cb, "wait for RDMA_READ_ADV state %d\n",
				cb->state);
			break;
		}

		DEBUG_LOG(cb, "server received sink adv\n");

		cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
		cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
		cb->rdma_sq_wr.sg_list->length = cb->remote_len;
		cb->rdma_sgl.lkey = krping_rdma_rkey(cb, cb->rdma_dma_addr, 1);

		/* Issue RDMA Read. */
		if (cb->read_inv)
			cb->rdma_sq_wr.opcode = IB_WR_RDMA_READ_WITH_INV;
		else {

			cb->rdma_sq_wr.opcode = IB_WR_RDMA_READ;
			if (cb->mem == FASTREG) {
				/* 
				 * Immediately follow the read with a 
				 * fenced LOCAL_INV.
				 */
				cb->rdma_sq_wr.next = &inv;
				memset(&inv, 0, sizeof inv);
				inv.opcode = IB_WR_LOCAL_INV;
				inv.ex.invalidate_rkey = cb->fastreg_mr->rkey;
				inv.send_flags = IB_SEND_FENCE;
			}
		}

		ret = ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, "post send error %d\n", ret);
			break;
		}
		cb->rdma_sq_wr.next = NULL;

		DEBUG_LOG(cb, "server posted rdma read req \n");

		/* Wait for read completion */
		wait_event_interruptible(cb->sem, 
					 cb->state >= RDMA_READ_COMPLETE);
		if (cb->state != RDMA_READ_COMPLETE) {
			PRINTF(cb, 
			       "wait for RDMA_READ_COMPLETE state %d\n",
			       cb->state);
			break;
		}
		DEBUG_LOG(cb, "server received read complete\n");

		/* Display data in recv buf */
		if (cb->verbose) {
			if (strlen(cb->rdma_buf) > 128) {
				char msgbuf[128];

				strlcpy(msgbuf, cb->rdma_buf, sizeof(msgbuf));
				PRINTF(cb, "server ping data stripped: %s\n",
				       msgbuf);
			} else
				PRINTF(cb, "server ping data: %s\n",
				       cb->rdma_buf);
		}

		/* Tell client to continue */
		if (cb->server && cb->server_invalidate) {
			cb->sq_wr.ex.invalidate_rkey = cb->remote_rkey;
			cb->sq_wr.opcode = IB_WR_SEND_WITH_INV;
			DEBUG_LOG(cb, "send-w-inv rkey 0x%x\n", cb->remote_rkey);
		} 
		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG(cb, "server posted go ahead\n");

		/* Wait for client's RDMA STAG/TO/Len */
		wait_event_interruptible(cb->sem, cb->state >= RDMA_WRITE_ADV);
		if (cb->state != RDMA_WRITE_ADV) {
			PRINTF(cb, 
			       "wait for RDMA_WRITE_ADV state %d\n",
			       cb->state);
			break;
		}
		DEBUG_LOG(cb, "server received sink adv\n");

		/* RDMA Write echo data */
		cb->rdma_sq_wr.opcode = IB_WR_RDMA_WRITE;
		cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
		cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
		cb->rdma_sq_wr.sg_list->length = strlen(cb->rdma_buf) + 1;
		if (cb->local_dma_lkey)
			cb->rdma_sgl.lkey = cb->qp->device->local_dma_lkey;
		else 
			cb->rdma_sgl.lkey = krping_rdma_rkey(cb, cb->rdma_dma_addr, 0);
			
		DEBUG_LOG(cb, "rdma write from lkey %x laddr %llx len %d\n",
			  cb->rdma_sq_wr.sg_list->lkey,
			  (unsigned long long)cb->rdma_sq_wr.sg_list->addr,
			  cb->rdma_sq_wr.sg_list->length);

		ret = ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, "post send error %d\n", ret);
			break;
		}

		/* Wait for completion */
		ret = wait_event_interruptible(cb->sem, cb->state >= 
							 RDMA_WRITE_COMPLETE);
		if (cb->state != RDMA_WRITE_COMPLETE) {
			PRINTF(cb, 
			       "wait for RDMA_WRITE_COMPLETE state %d\n",
			       cb->state);
			break;
		}
		DEBUG_LOG(cb, "server rdma write complete \n");

		cb->state = CONNECTED;

		/* Tell client to begin again */
		if (cb->server && cb->server_invalidate) {
			cb->sq_wr.ex.invalidate_rkey = cb->remote_rkey;
			cb->sq_wr.opcode = IB_WR_SEND_WITH_INV;
			DEBUG_LOG(cb, "send-w-inv rkey 0x%x\n", cb->remote_rkey);
		} 
		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG(cb, "server posted go ahead\n");
	}
}

static void rlat_test(struct krping_cb *cb)
{
	int scnt;
	int iters = cb->count;
	struct timeval start_tv, stop_tv;
	int ret;
	struct ib_wc wc;
	struct ib_send_wr *bad_wr;
	int ne;

	scnt = 0;
	cb->rdma_sq_wr.opcode = IB_WR_RDMA_READ;
	cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
	cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
	cb->rdma_sq_wr.sg_list->length = cb->size;

	microtime(&start_tv);
	if (!cb->poll) {
		cb->state = RDMA_READ_ADV;
		ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
	}
	while (scnt < iters) {

		cb->state = RDMA_READ_ADV;
		ret = ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, 
				"Couldn't post send: ret=%d scnt %d\n",
				ret, scnt);
			return;
		}

		do {
			if (!cb->poll) {
				wait_event_interruptible(cb->sem, 
					cb->state != RDMA_READ_ADV);
				if (cb->state == RDMA_READ_COMPLETE) {
					ne = 1;
					ib_req_notify_cq(cb->cq, 
						IB_CQ_NEXT_COMP);
				} else {
					ne = -1;
				}
			} else
				ne = ib_poll_cq(cb->cq, 1, &wc);
			if (cb->state == ERROR) {
				PRINTF(cb, 
					"state == ERROR...bailing scnt %d\n", 
					scnt);
				return;
			}
		} while (ne == 0);

		if (ne < 0) {
			PRINTF(cb, "poll CQ failed %d\n", ne);
			return;
		}
		if (cb->poll && wc.status != IB_WC_SUCCESS) {
			PRINTF(cb, "Completion wth error at %s:\n",
				cb->server ? "server" : "client");
			PRINTF(cb, "Failed status %d: wr_id %d\n",
				wc.status, (int) wc.wr_id);
			return;
		}
		++scnt;
	}
	microtime(&stop_tv);

        if (stop_tv.tv_usec < start_tv.tv_usec) {
                stop_tv.tv_usec += 1000000;
                stop_tv.tv_sec  -= 1;
        }

	PRINTF(cb, "delta sec %lu delta usec %lu iter %d size %d\n",
		(unsigned long)(stop_tv.tv_sec - start_tv.tv_sec),
		(unsigned long)(stop_tv.tv_usec - start_tv.tv_usec),
		scnt, cb->size);
}

static void wlat_test(struct krping_cb *cb)
{
	int ccnt, scnt, rcnt;
	int iters=cb->count;
	volatile char *poll_buf = (char *) cb->start_buf;
	char *buf = (char *)cb->rdma_buf;
	struct timeval start_tv, stop_tv;
	cycles_t *post_cycles_start, *post_cycles_stop;
	cycles_t *poll_cycles_start, *poll_cycles_stop;
	cycles_t *last_poll_cycles_start;
	cycles_t sum_poll = 0, sum_post = 0, sum_last_poll = 0;
	int i;
	int cycle_iters = 1000;

	ccnt = 0;
	scnt = 0;
	rcnt = 0;

	post_cycles_start = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!post_cycles_start) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	post_cycles_stop = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!post_cycles_stop) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	poll_cycles_start = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!poll_cycles_start) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	poll_cycles_stop = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!poll_cycles_stop) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	last_poll_cycles_start = kmalloc(cycle_iters * sizeof(cycles_t), 
		GFP_KERNEL);
	if (!last_poll_cycles_start) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	cb->rdma_sq_wr.opcode = IB_WR_RDMA_WRITE;
	cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
	cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
	cb->rdma_sq_wr.sg_list->length = cb->size;

	if (cycle_iters > iters)
		cycle_iters = iters;
	microtime(&start_tv);
	while (scnt < iters || ccnt < iters || rcnt < iters) {

		/* Wait till buffer changes. */
		if (rcnt < iters && !(scnt < 1 && !cb->server)) {
			++rcnt;
			while (*poll_buf != (char)rcnt) {
				if (cb->state == ERROR) {
					PRINTF(cb, 
						"state = ERROR, bailing\n");
					return;
				}
			}
		}

		if (scnt < iters) {
			struct ib_send_wr *bad_wr;

			*buf = (char)scnt+1;
			if (scnt < cycle_iters)
				post_cycles_start[scnt] = get_cycles();
			if (ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr)) {
				PRINTF(cb, 
					"Couldn't post send: scnt=%d\n",
					scnt);
				return;
			}
			if (scnt < cycle_iters)
				post_cycles_stop[scnt] = get_cycles();
			scnt++;
		}

		if (ccnt < iters) {
			struct ib_wc wc;
			int ne;

			if (ccnt < cycle_iters)
				poll_cycles_start[ccnt] = get_cycles();
			do {
				if (ccnt < cycle_iters)
					last_poll_cycles_start[ccnt] = 
						get_cycles();
				ne = ib_poll_cq(cb->cq, 1, &wc);
			} while (ne == 0);
			if (ccnt < cycle_iters)
				poll_cycles_stop[ccnt] = get_cycles();
			++ccnt;

			if (ne < 0) {
				PRINTF(cb, "poll CQ failed %d\n", ne);
				return;
			}
			if (wc.status != IB_WC_SUCCESS) {
				PRINTF(cb, 
					"Completion wth error at %s:\n",
					cb->server ? "server" : "client");
				PRINTF(cb, 
					"Failed status %d: wr_id %d\n",
					wc.status, (int) wc.wr_id);
				PRINTF(cb, 
					"scnt=%d, rcnt=%d, ccnt=%d\n",
					scnt, rcnt, ccnt);
				return;
			}
		}
	}
	microtime(&stop_tv);

        if (stop_tv.tv_usec < start_tv.tv_usec) {
                stop_tv.tv_usec += 1000000;
                stop_tv.tv_sec  -= 1;
        }

	for (i=0; i < cycle_iters; i++) {
		sum_post += post_cycles_stop[i] - post_cycles_start[i];
		sum_poll += poll_cycles_stop[i] - poll_cycles_start[i];
		sum_last_poll += poll_cycles_stop[i]-last_poll_cycles_start[i];
	}
	PRINTF(cb,
		"delta sec %lu delta usec %lu iter %d size %d cycle_iters %d"
		" sum_post %llu sum_poll %llu sum_last_poll %llu\n",
		(unsigned long)(stop_tv.tv_sec - start_tv.tv_sec),
		(unsigned long)(stop_tv.tv_usec - start_tv.tv_usec),
		scnt, cb->size, cycle_iters,
		(unsigned long long)sum_post, (unsigned long long)sum_poll, 
		(unsigned long long)sum_last_poll);
	kfree(post_cycles_start);
	kfree(post_cycles_stop);
	kfree(poll_cycles_start);
	kfree(poll_cycles_stop);
	kfree(last_poll_cycles_start);
}

static void bw_test(struct krping_cb *cb)
{
	int ccnt, scnt, rcnt;
	int iters=cb->count;
	struct timeval start_tv, stop_tv;
	cycles_t *post_cycles_start, *post_cycles_stop;
	cycles_t *poll_cycles_start, *poll_cycles_stop;
	cycles_t *last_poll_cycles_start;
	cycles_t sum_poll = 0, sum_post = 0, sum_last_poll = 0;
	int i;
	int cycle_iters = 1000;

	ccnt = 0;
	scnt = 0;
	rcnt = 0;

	post_cycles_start = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!post_cycles_start) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	post_cycles_stop = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!post_cycles_stop) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	poll_cycles_start = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!poll_cycles_start) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	poll_cycles_stop = kmalloc(cycle_iters * sizeof(cycles_t), GFP_KERNEL);
	if (!poll_cycles_stop) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	last_poll_cycles_start = kmalloc(cycle_iters * sizeof(cycles_t), 
		GFP_KERNEL);
	if (!last_poll_cycles_start) {
		PRINTF(cb, "%s kmalloc failed\n", __FUNCTION__);
		return;
	}
	cb->rdma_sq_wr.opcode = IB_WR_RDMA_WRITE;
	cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
	cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
	cb->rdma_sq_wr.sg_list->length = cb->size;

	if (cycle_iters > iters)
		cycle_iters = iters;
	microtime(&start_tv);
	while (scnt < iters || ccnt < iters) {

		while (scnt < iters && scnt - ccnt < cb->txdepth) {
			struct ib_send_wr *bad_wr;

			if (scnt < cycle_iters)
				post_cycles_start[scnt] = get_cycles();
			if (ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr)) {
				PRINTF(cb, 
					"Couldn't post send: scnt=%d\n",
					scnt);
				return;
			}
			if (scnt < cycle_iters)
				post_cycles_stop[scnt] = get_cycles();
			++scnt;
		}

		if (ccnt < iters) {
			int ne;
			struct ib_wc wc;

			if (ccnt < cycle_iters)
				poll_cycles_start[ccnt] = get_cycles();
			do {
				if (ccnt < cycle_iters)
					last_poll_cycles_start[ccnt] = 
						get_cycles();
				ne = ib_poll_cq(cb->cq, 1, &wc);
			} while (ne == 0);
			if (ccnt < cycle_iters)
				poll_cycles_stop[ccnt] = get_cycles();
			ccnt += 1;

			if (ne < 0) {
				PRINTF(cb, "poll CQ failed %d\n", ne);
				return;
			}
			if (wc.status != IB_WC_SUCCESS) {
				PRINTF(cb, 
					"Completion wth error at %s:\n",
					cb->server ? "server" : "client");
				PRINTF(cb, 
					"Failed status %d: wr_id %d\n",
					wc.status, (int) wc.wr_id);
				return;
			}
		}
	}
	microtime(&stop_tv);

        if (stop_tv.tv_usec < start_tv.tv_usec) {
                stop_tv.tv_usec += 1000000;
                stop_tv.tv_sec  -= 1;
        }

	for (i=0; i < cycle_iters; i++) {
		sum_post += post_cycles_stop[i] - post_cycles_start[i];
		sum_poll += poll_cycles_stop[i] - poll_cycles_start[i];
		sum_last_poll += poll_cycles_stop[i]-last_poll_cycles_start[i];
	}
	PRINTF(cb,
		"delta sec %lu delta usec %lu iter %d size %d cycle_iters %d"
		" sum_post %llu sum_poll %llu sum_last_poll %llu\n",
		(unsigned long)(stop_tv.tv_sec - start_tv.tv_sec),
		(unsigned long)(stop_tv.tv_usec - start_tv.tv_usec),
		scnt, cb->size, cycle_iters, 
		(unsigned long long)sum_post, (unsigned long long)sum_poll, 
		(unsigned long long)sum_last_poll);
	kfree(post_cycles_start);
	kfree(post_cycles_stop);
	kfree(poll_cycles_start);
	kfree(poll_cycles_stop);
	kfree(last_poll_cycles_start);
}

static void krping_rlat_test_server(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	/* Spin waiting for client's Start STAG/TO/Len */
	while (cb->state < RDMA_READ_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completiong error %d\n", wc.status);
		return;
	}
	wait_event_interruptible(cb->sem, cb->state == ERROR);
}

static void krping_wlat_test_server(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	/* Spin waiting for client's Start STAG/TO/Len */
	while (cb->state < RDMA_READ_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completiong error %d\n", wc.status);
		return;
	}

	wlat_test(cb);
	wait_event_interruptible(cb->sem, cb->state == ERROR);
}

static void krping_bw_test_server(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	/* Spin waiting for client's Start STAG/TO/Len */
	while (cb->state < RDMA_READ_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completiong error %d\n", wc.status);
		return;
	}

	if (cb->duplex)
		bw_test(cb);
	wait_event_interruptible(cb->sem, cb->state == ERROR);
}

static int fastreg_supported(struct krping_cb *cb, int server)
{
	struct ib_device *dev = server?cb->child_cm_id->device:
					cb->cm_id->device;
	struct ib_device_attr attr;
	int ret;

	ret = ib_query_device(dev, &attr);
	if (ret) {
		PRINTF(cb, "ib_query_device failed ret %d\n", ret);
		return 0;
	}
	if (!(attr.device_cap_flags & IB_DEVICE_MEM_MGT_EXTENSIONS)) {
		PRINTF(cb, "Fastreg not supported - device_cap_flags 0x%llx\n",
		    (unsigned long long)attr.device_cap_flags);
		return 0;
	}
	DEBUG_LOG(cb, "Fastreg supported - device_cap_flags 0x%jx\n",
		(uintmax_t)attr.device_cap_flags);
	return 1;
}

static int krping_bind_server(struct krping_cb *cb)
{
	struct sockaddr_in sin;
	int ret;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = cb->addr.s_addr;
	sin.sin_port = cb->port;

	ret = rdma_bind_addr(cb->cm_id, (struct sockaddr *) &sin);
	if (ret) {
		PRINTF(cb, "rdma_bind_addr error %d\n", ret);
		return ret;
	}
	DEBUG_LOG(cb, "rdma_bind_addr successful\n");

	DEBUG_LOG(cb, "rdma_listen\n");
	ret = rdma_listen(cb->cm_id, 3);
	if (ret) {
		PRINTF(cb, "rdma_listen failed: %d\n", ret);
		return ret;
	}

	wait_event_interruptible(cb->sem, cb->state >= CONNECT_REQUEST);
	if (cb->state != CONNECT_REQUEST) {
		PRINTF(cb, "wait for CONNECT_REQUEST state %d\n",
			cb->state);
		return -1;
	}

	if (cb->mem == FASTREG && !fastreg_supported(cb, 1))
		return -EINVAL;

	return 0;
}

/*
 * sq-depth worth of fastreg + 0B read-inv pairs, reposting them as the reads
 * complete.
 * NOTE: every 9 seconds we sleep for 1 second to keep the kernel happy.
 */
static void krping_fr_test5(struct krping_cb *cb)
{
	struct ib_fast_reg_page_list **pl;
	struct ib_send_wr *fr, *read, *bad;
	struct ib_wc wc;
	struct ib_sge *sgl;
	u8 key = 0;
	struct ib_mr **mr;
	u8 **buf;
	dma_addr_t *dma_addr;
	int i;
	int ret;
	int plen = (((cb->size - 1) & PAGE_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
	time_t start;
	int count = 0;
	int scnt;
	int depth = cb->txdepth >> 1;

	if (!depth) {
		PRINTF(cb, "txdepth must be > 1 for this test!\n");
		return;
	}

	pl = kzalloc(sizeof *pl * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s pl %p size %zu\n", __func__, pl, sizeof *pl * depth);
	mr = kzalloc(sizeof *mr * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s mr %p size %zu\n", __func__, mr, sizeof *mr * depth);
	fr = kzalloc(sizeof *fr * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s fr %p size %zu\n", __func__, fr, sizeof *fr * depth);
	sgl = kzalloc(sizeof *sgl * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s sgl %p size %zu\n", __func__, sgl, sizeof *sgl * depth);
	read = kzalloc(sizeof *read * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s read %p size %zu\n", __func__, read, sizeof *read * depth);
	buf = kzalloc(sizeof *buf * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s buf %p size %zu\n", __func__, buf, sizeof *buf * depth);
	dma_addr = kzalloc(sizeof *dma_addr * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s dma_addr %p size %zu\n", __func__, dma_addr, sizeof *dma_addr * depth);
	if (!pl || !mr || !fr || !read || !sgl || !buf || !dma_addr) {
		PRINTF(cb, "kzalloc failed\n");
		goto err1;
	}

	for (scnt = 0; scnt < depth; scnt++) {
		pl[scnt] = ib_alloc_fast_reg_page_list(cb->qp->device, plen);
		if (IS_ERR(pl[scnt])) {
			PRINTF(cb, "alloc_fr_page_list failed %ld\n",
			       PTR_ERR(pl[scnt]));
			goto err2;
		}
		DEBUG_LOG(cb, "%s pl[%u] %p\n", __func__, scnt, pl[scnt]);

		mr[scnt] = ib_alloc_fast_reg_mr(cb->pd, plen);
		if (IS_ERR(mr[scnt])) {
			PRINTF(cb, "alloc_fr failed %ld\n",
			       PTR_ERR(mr[scnt]));
			goto err2;
		}
		DEBUG_LOG(cb, "%s mr[%u] %p\n", __func__, scnt, mr[scnt]);
		ib_update_fast_reg_key(mr[scnt], ++key);

		buf[scnt] = kmalloc(cb->size, GFP_KERNEL);
		if (!buf[scnt]) {
			PRINTF(cb, "kmalloc failed\n");
			ret = -ENOMEM;
			goto err2;
		}
		DEBUG_LOG(cb, "%s buf[%u] %p\n", __func__, scnt, buf[scnt]);
		dma_addr[scnt] = dma_map_single(cb->pd->device->dma_device,
						   buf[scnt], cb->size,
						   DMA_BIDIRECTIONAL);
		if (dma_mapping_error(cb->pd->device->dma_device,
		    dma_addr[scnt])) {
			PRINTF(cb, "dma_map failed\n");
			ret = -ENOMEM;
			goto err2;
		}
		DEBUG_LOG(cb, "%s dma_addr[%u] %p\n", __func__, scnt, (void *)dma_addr[scnt]);
		for (i=0; i<plen; i++) {
			pl[scnt]->page_list[i] = ((unsigned long)dma_addr[scnt] & PAGE_MASK) + (i * PAGE_SIZE);
			DEBUG_LOG(cb, "%s pl[%u]->page_list[%u] 0x%jx\n",
				  __func__, scnt, i,  (uintmax_t)pl[scnt]->page_list[i]);
		}

		sgl[scnt].lkey = mr[scnt]->rkey;
		sgl[scnt].length = cb->size;
		sgl[scnt].addr = (u64)buf[scnt];
		DEBUG_LOG(cb, "%s sgl[%u].lkey 0x%x length %u addr 0x%jx\n",
			  __func__, scnt,  sgl[scnt].lkey, sgl[scnt].length,
			  (uintmax_t)sgl[scnt].addr);

		fr[scnt].opcode = IB_WR_FAST_REG_MR;
		fr[scnt].wr_id = scnt;
		fr[scnt].send_flags = 0;
		fr[scnt].wr.fast_reg.page_shift = PAGE_SHIFT;
		fr[scnt].wr.fast_reg.length = cb->size;
		fr[scnt].wr.fast_reg.page_list = pl[scnt];
		fr[scnt].wr.fast_reg.page_list_len = plen;
		fr[scnt].wr.fast_reg.iova_start = (u64)buf[scnt];
		fr[scnt].wr.fast_reg.access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
		fr[scnt].wr.fast_reg.rkey = mr[scnt]->rkey;
		fr[scnt].next = &read[scnt];
		read[scnt].opcode = IB_WR_RDMA_READ_WITH_INV;
		read[scnt].wr_id = scnt;
		read[scnt].send_flags = IB_SEND_SIGNALED;
		read[scnt].wr.rdma.rkey = cb->remote_rkey;
		read[scnt].wr.rdma.remote_addr = cb->remote_addr;
		read[scnt].num_sge = 1;
		read[scnt].sg_list = &sgl[scnt];
		ret = ib_post_send(cb->qp, &fr[scnt], &bad);
		if (ret) {
			PRINTF(cb, "ib_post_send failed %d\n", ret);
			goto err2;
		}
	}

	start = time_uptime;
	DEBUG_LOG(cb, "%s starting IO.\n", __func__);
	while (!cb->count || cb->server || count < cb->count) {
		if ((time_uptime - start) >= 9) {
			DEBUG_LOG(cb, "%s pausing 1 tick! count %u\n", __func__,
				  count);
			wait_event_interruptible_timeout(cb->sem,
							 cb->state == ERROR,
							 1);
			if (cb->state == ERROR)
				break;
			start = time_uptime;
		}
		do {
			ret = ib_poll_cq(cb->cq, 1, &wc);
			if (ret < 0) {
				PRINTF(cb, "ib_poll_cq failed %d\n",
				       ret);
				goto err2;
			}
			if (ret == 1) {
				if (wc.status) {
					PRINTF(cb,
					       "completion error %u wr_id %ju "
					       "opcode %d\n", wc.status,
					       (uintmax_t)wc.wr_id, wc.opcode);
					goto err2;
				}
				count++;
				if (count == cb->count)
					break;
				ib_update_fast_reg_key(mr[wc.wr_id], ++key);
				fr[wc.wr_id].wr.fast_reg.rkey =
					mr[wc.wr_id]->rkey;
				sgl[wc.wr_id].lkey = mr[wc.wr_id]->rkey;
				ret = ib_post_send(cb->qp, &fr[wc.wr_id], &bad);
				if (ret) {
					PRINTF(cb,
					       "ib_post_send failed %d\n", ret);
					goto err2;
				}
			} else if (krping_sigpending()) {
				PRINTF(cb, "signal!\n");
				goto err2;
			}
		} while (ret == 1);
	}
	DEBUG_LOG(cb, "%s done!\n", __func__);
err2:
	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	DEBUG_LOG(cb, "draining the cq...\n");
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			break;
		}
		if (ret == 1) {
			if (wc.status) {
				PRINTF(cb, "completion error %u "
				       "opcode %u\n", wc.status, wc.opcode);
			}
		}
	} while (ret == 1);

	DEBUG_LOG(cb, "destroying fr mrs!\n");
	for (scnt = 0; scnt < depth; scnt++) {
		if (mr[scnt]) {
			ib_dereg_mr(mr[scnt]);
			DEBUG_LOG(cb, "%s dereg mr %p\n", __func__, mr[scnt]);
		}
	}
	DEBUG_LOG(cb, "unmapping/freeing bufs!\n");
	for (scnt = 0; scnt < depth; scnt++) {
		if (buf[scnt]) {
			dma_unmap_single(cb->pd->device->dma_device,
					 dma_addr[scnt], cb->size,
					 DMA_BIDIRECTIONAL);
			kfree(buf[scnt]);
			DEBUG_LOG(cb, "%s unmap/free buf %p dma_addr %p\n", __func__, buf[scnt], (void *)dma_addr[scnt]);
		}
	}
	DEBUG_LOG(cb, "destroying fr page lists!\n");
	for (scnt = 0; scnt < depth; scnt++) {
		if (pl[scnt]) {
			DEBUG_LOG(cb, "%s free pl %p\n", __func__, pl[scnt]);
			ib_free_fast_reg_page_list(pl[scnt]);
		}
	}
err1:
	if (pl)
		kfree(pl);
	if (mr)
		kfree(mr);
	if (fr)
		kfree(fr);
	if (read)
		kfree(read);
	if (sgl)
		kfree(sgl);
	if (buf)
		kfree(buf);
	if (dma_addr)
		kfree(dma_addr);
}
static void krping_fr_test_server(struct krping_cb *cb)
{
	DEBUG_LOG(cb, "%s waiting for disconnect...\n", __func__);
	wait_event_interruptible(cb->sem, cb->state == ERROR);
}

static void krping_fr_test5_server(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	/* Spin waiting for client's Start STAG/TO/Len */
	while (cb->state < RDMA_READ_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}
	DEBUG_LOG(cb, "%s client STAG %x TO 0x%jx\n", __func__,
		  cb->remote_rkey, (uintmax_t)cb->remote_addr);

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completiong error %d\n", wc.status);
		return;
	}

	if (cb->duplex)
		krping_fr_test5(cb);
	DEBUG_LOG(cb, "%s waiting for disconnect...\n", __func__);
	wait_event_interruptible(cb->sem, cb->state == ERROR);
}

static void krping_fr_test5_client(struct krping_cb *cb)
{
	struct ib_send_wr *bad;
	struct ib_wc wc;
	int ret;

	cb->state = RDMA_READ_ADV;

	/* Send STAG/TO/Len to server */
	krping_format_send(cb, cb->start_dma_addr);
	if (cb->state == ERROR) {
		PRINTF(cb, "krping_format_send failed\n");
		return;
	}
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completion error %d\n", wc.status);
		return;
	}

	/* Spin waiting for server's Start STAG/TO/Len */
	while (cb->state < RDMA_WRITE_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}
	DEBUG_LOG(cb, "%s server STAG %x TO 0x%jx\n", __func__, cb->remote_rkey,
	    (uintmax_t)cb->remote_addr);

	return krping_fr_test5(cb);
}

/*
 * sq-depth worth of write + fastreg + inv, reposting them as the invs
 * complete.
 * NOTE: every 9 seconds we sleep for 1 second to keep the kernel happy.
 * If a count is given, then the last IO will have a bogus lkey in the
 * write work request.  This reproduces a fw bug where the connection
 * will get stuck if a fastreg is processed while the ulptx is failing
 * the bad write.
 */
static void krping_fr_test6(struct krping_cb *cb)
{
	struct ib_fast_reg_page_list **pl;
	struct ib_send_wr *fr, *write, *inv, *bad;
	struct ib_wc wc;
	struct ib_sge *sgl;
	u8 key = 0;
	struct ib_mr **mr;
	u8 **buf;
	dma_addr_t *dma_addr;
	int i;
	int ret;
	int plen = (((cb->size - 1) & PAGE_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
	unsigned long start;
	int count = 0;
	int scnt;
	int depth = cb->txdepth  / 3;

	if (!depth) {
		PRINTF(cb, "txdepth must be > 3 for this test!\n");
		return;
	}

	pl = kzalloc(sizeof *pl * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s pl %p size %zu\n", __func__, pl, sizeof *pl * depth);

	mr = kzalloc(sizeof *mr * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s mr %p size %zu\n", __func__, mr, sizeof *mr * depth);

	fr = kzalloc(sizeof *fr * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s fr %p size %zu\n", __func__, fr, sizeof *fr * depth);

	sgl = kzalloc(sizeof *sgl * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s sgl %p size %zu\n", __func__, sgl, sizeof *sgl * depth);

	write = kzalloc(sizeof *write * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s read %p size %zu\n", __func__, write, sizeof *write * depth);

	inv = kzalloc(sizeof *inv * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s inv %p size %zu\n", __func__, inv, sizeof *inv * depth);

	buf = kzalloc(sizeof *buf * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s buf %p size %zu\n", __func__, buf, sizeof *buf * depth);

	dma_addr = kzalloc(sizeof *dma_addr * depth, GFP_KERNEL);
	DEBUG_LOG(cb, "%s dma_addr %p size %zu\n", __func__, dma_addr, sizeof *dma_addr * depth);

	if (!pl || !mr || !fr || !write || !sgl || !buf || !dma_addr) {
		PRINTF(cb, "kzalloc failed\n");
		goto err1;
	}

	for (scnt = 0; scnt < depth; scnt++) {
		pl[scnt] = ib_alloc_fast_reg_page_list(cb->qp->device, plen);
		if (IS_ERR(pl[scnt])) {
			PRINTF(cb, "alloc_fr_page_list failed %ld\n",
			       PTR_ERR(pl[scnt]));
			goto err2;
		}
		DEBUG_LOG(cb, "%s pl[%u] %p\n", __func__, scnt, pl[scnt]);

		mr[scnt] = ib_alloc_fast_reg_mr(cb->pd, plen);
		if (IS_ERR(mr[scnt])) {
			PRINTF(cb, "alloc_fr failed %ld\n",
			       PTR_ERR(mr[scnt]));
			goto err2;
		}
		DEBUG_LOG(cb, "%s mr[%u] %p\n", __func__, scnt, mr[scnt]);
		ib_update_fast_reg_key(mr[scnt], ++key);

		buf[scnt] = kmalloc(cb->size, GFP_KERNEL);
		if (!buf[scnt]) {
			PRINTF(cb, "kmalloc failed\n");
			ret = -ENOMEM;
			goto err2;
		}
		DEBUG_LOG(cb, "%s buf[%u] %p\n", __func__, scnt, buf[scnt]);
		dma_addr[scnt] = dma_map_single(cb->pd->device->dma_device,
						   buf[scnt], cb->size,
						   DMA_BIDIRECTIONAL);
		if (dma_mapping_error(cb->pd->device->dma_device,
		    dma_addr[scnt])) {
			PRINTF(cb, "dma_map failed\n");
			ret = -ENOMEM;
			goto err2;
		}
		DEBUG_LOG(cb, "%s dma_addr[%u] %p\n", __func__, scnt, (void *)dma_addr[scnt]);
		for (i=0; i<plen; i++) {
			pl[scnt]->page_list[i] = ((unsigned long)dma_addr[scnt] & PAGE_MASK) + (i * PAGE_SIZE);
			DEBUG_LOG(cb, "%s pl[%u]->page_list[%u] 0x%jx\n",
				  __func__, scnt, i,  (uintmax_t)pl[scnt]->page_list[i]);
		}

		write[scnt].opcode = IB_WR_RDMA_WRITE;
		write[scnt].wr_id = scnt;
		write[scnt].wr.rdma.rkey = cb->remote_rkey;
		write[scnt].wr.rdma.remote_addr = cb->remote_addr;
		write[scnt].num_sge = 1;
		write[scnt].sg_list = &cb->rdma_sgl;
		write[scnt].sg_list->length = cb->size;
		write[scnt].next = &fr[scnt];

		fr[scnt].opcode = IB_WR_FAST_REG_MR;
		fr[scnt].wr_id = scnt;
		fr[scnt].wr.fast_reg.page_shift = PAGE_SHIFT;
		fr[scnt].wr.fast_reg.length = cb->size;
		fr[scnt].wr.fast_reg.page_list = pl[scnt];
		fr[scnt].wr.fast_reg.page_list_len = plen;
		fr[scnt].wr.fast_reg.iova_start = (u64)buf[scnt];
		fr[scnt].wr.fast_reg.access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
		fr[scnt].wr.fast_reg.rkey = mr[scnt]->rkey;
		fr[scnt].next = &inv[scnt];

		inv[scnt].opcode = IB_WR_LOCAL_INV;
		inv[scnt].send_flags = IB_SEND_SIGNALED;
		inv[scnt].ex.invalidate_rkey = mr[scnt]->rkey;

		ret = ib_post_send(cb->qp, &write[scnt], &bad);
		if (ret) {
			PRINTF(cb, "ib_post_send failed %d\n", ret);
			goto err2;
		}
	}

	start = time_uptime;
	DEBUG_LOG(cb, "%s starting IO.\n", __func__);
	while (!cb->count || cb->server || count < cb->count) {
		if ((time_uptime - start) >= 9) {
			DEBUG_LOG(cb, "%s pausing 1 tick! count %u\n", __func__,
				  count);
			wait_event_interruptible_timeout(cb->sem,
							 cb->state == ERROR,
							 1);
			if (cb->state == ERROR)
				break;
			start = time_uptime;
		}
		do {
			ret = ib_poll_cq(cb->cq, 1, &wc);
			if (ret < 0) {
				PRINTF(cb, "ib_poll_cq failed %d\n",
				       ret);
				goto err2;
			}
			if (ret == 1) {
				if (wc.status) {
					PRINTF(cb,
					       "completion error %u wr_id %ju "
					       "opcode %d\n", wc.status,
					       (uintmax_t)wc.wr_id, wc.opcode);
					goto err2;
				}
				count++;
				if (count == (cb->count -1))
					cb->rdma_sgl.lkey = 0x00dead;
				if (count == cb->count)
					break;
				ib_update_fast_reg_key(mr[wc.wr_id], ++key);
				fr[wc.wr_id].wr.fast_reg.rkey =
					mr[wc.wr_id]->rkey;
				inv[wc.wr_id].ex.invalidate_rkey =
					mr[wc.wr_id]->rkey;
				ret = ib_post_send(cb->qp, &write[wc.wr_id], &bad);
				if (ret) {
					PRINTF(cb,
					       "ib_post_send failed %d\n", ret);
					goto err2;
				}
			} else if (krping_sigpending()){
				PRINTF(cb, "signal!\n");
				goto err2;
			}
		} while (ret == 1);
	}
	DEBUG_LOG(cb, "%s done!\n", __func__);
err2:
	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	DEBUG_LOG(cb, "draining the cq...\n");
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			break;
		}
		if (ret == 1) {
			if (wc.status) {
				PRINTF(cb, "completion error %u "
				       "opcode %u\n", wc.status, wc.opcode);
			}
		}
	} while (ret == 1);

	DEBUG_LOG(cb, "destroying fr mrs!\n");
	for (scnt = 0; scnt < depth; scnt++) {
		if (mr[scnt]) {
			ib_dereg_mr(mr[scnt]);
			DEBUG_LOG(cb, "%s dereg mr %p\n", __func__, mr[scnt]);
		}
	}
	DEBUG_LOG(cb, "unmapping/freeing bufs!\n");
	for (scnt = 0; scnt < depth; scnt++) {
		if (buf[scnt]) {
			dma_unmap_single(cb->pd->device->dma_device,
					 dma_addr[scnt], cb->size,
					 DMA_BIDIRECTIONAL);
			kfree(buf[scnt]);
			DEBUG_LOG(cb, "%s unmap/free buf %p dma_addr %p\n", __func__, buf[scnt], (void *)dma_addr[scnt]);
		}
	}
	DEBUG_LOG(cb, "destroying fr page lists!\n");
	for (scnt = 0; scnt < depth; scnt++) {
		if (pl[scnt]) {
			DEBUG_LOG(cb, "%s free pl %p\n", __func__, pl[scnt]);
			ib_free_fast_reg_page_list(pl[scnt]);
		}
	}
err1:
	if (pl)
		kfree(pl);
	if (mr)
		kfree(mr);
	if (fr)
		kfree(fr);
	if (write)
		kfree(write);
	if (inv)
		kfree(inv);
	if (sgl)
		kfree(sgl);
	if (buf)
		kfree(buf);
	if (dma_addr)
		kfree(dma_addr);
}

static void krping_fr_test6_server(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	/* Spin waiting for client's Start STAG/TO/Len */
	while (cb->state < RDMA_READ_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}
	DEBUG_LOG(cb, "%s client STAG %x TO 0x%jx\n", __func__,
		  cb->remote_rkey, (uintmax_t)cb->remote_addr);

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completiong error %d\n", wc.status);
		return;
	}

	if (cb->duplex)
		krping_fr_test6(cb);
	DEBUG_LOG(cb, "%s waiting for disconnect...\n", __func__);
	wait_event_interruptible(cb->sem, cb->state == ERROR);
}

static void krping_fr_test6_client(struct krping_cb *cb)
{
	struct ib_send_wr *bad;
	struct ib_wc wc;
	int ret;

	cb->state = RDMA_READ_ADV;

	/* Send STAG/TO/Len to server */
	krping_format_send(cb, cb->start_dma_addr);
	if (cb->state == ERROR) {
		PRINTF(cb, "krping_format_send failed\n");
		return;
	}
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completion error %d\n", wc.status);
		return;
	}

	/* Spin waiting for server's Start STAG/TO/Len */
	while (cb->state < RDMA_WRITE_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}
	DEBUG_LOG(cb, "%s server STAG %x TO 0x%jx\n", __func__, cb->remote_rkey,
	    (uintmax_t)cb->remote_addr);

	return krping_fr_test6(cb);
}

static void krping_run_server(struct krping_cb *cb)
{
	struct ib_recv_wr *bad_wr;
	int ret;

	ret = krping_bind_server(cb);
	if (ret)
		return;

	ret = krping_setup_qp(cb, cb->child_cm_id);
	if (ret) {
		PRINTF(cb, "setup_qp failed: %d\n", ret);
		goto err0;
	}

	ret = krping_setup_buffers(cb);
	if (ret) {
		PRINTF(cb, "krping_setup_buffers failed: %d\n", ret);
		goto err1;
	}

	ret = ib_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "ib_post_recv failed: %d\n", ret);
		goto err2;
	}

	ret = krping_accept(cb);
	if (ret) {
		PRINTF(cb, "connect error %d\n", ret);
		goto err2;
	}

	if (cb->wlat)
		krping_wlat_test_server(cb);
	else if (cb->rlat)
		krping_rlat_test_server(cb);
	else if (cb->bw)
		krping_bw_test_server(cb);
	else if (cb->frtest) {
		switch (cb->testnum) {
		case 1:
		case 2:
		case 3:
		case 4:
			krping_fr_test_server(cb);
			break;
		case 5:
			krping_fr_test5_server(cb);
			break;
		case 6:
			krping_fr_test6_server(cb);
			break;
		default:
			PRINTF(cb, "unknown fr test %d\n", cb->testnum);
			goto err2;
			break;
		}
	} else
		krping_test_server(cb);
	rdma_disconnect(cb->child_cm_id);
err2:
	krping_free_buffers(cb);
err1:
	krping_free_qp(cb);
err0:
	rdma_destroy_id(cb->child_cm_id);
}

static void krping_test_client(struct krping_cb *cb)
{
	int ping, start, cc, i, ret;
	struct ib_send_wr *bad_wr;
	unsigned char c;

	start = 65;
	for (ping = 0; !cb->count || ping < cb->count; ping++) {
		cb->state = RDMA_READ_ADV;

		/* Put some ascii text in the buffer. */
		cc = sprintf(cb->start_buf, "rdma-ping-%d: ", ping);
		for (i = cc, c = start; i < cb->size; i++) {
			cb->start_buf[i] = c;
			c++;
			if (c > 122)
				c = 65;
		}
		start++;
		if (start > 122)
			start = 65;
		cb->start_buf[cb->size - 1] = 0;

		krping_format_send(cb, cb->start_dma_addr);
		if (cb->state == ERROR) {
			PRINTF(cb, "krping_format_send failed\n");
			break;
		}
		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, "post send error %d\n", ret);
			break;
		}

		/* Wait for server to ACK */
		wait_event_interruptible(cb->sem, cb->state >= RDMA_WRITE_ADV);
		if (cb->state != RDMA_WRITE_ADV) {
			PRINTF(cb, 
			       "wait for RDMA_WRITE_ADV state %d\n",
			       cb->state);
			break;
		}

		krping_format_send(cb, cb->rdma_dma_addr);
		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			PRINTF(cb, "post send error %d\n", ret);
			break;
		}

		/* Wait for the server to say the RDMA Write is complete. */
		wait_event_interruptible(cb->sem, 
					 cb->state >= RDMA_WRITE_COMPLETE);
		if (cb->state != RDMA_WRITE_COMPLETE) {
			PRINTF(cb, 
			       "wait for RDMA_WRITE_COMPLETE state %d\n",
			       cb->state);
			break;
		}

		if (cb->validate)
			if (memcmp(cb->start_buf, cb->rdma_buf, cb->size)) {
				PRINTF(cb, "data mismatch!\n");
				break;
			}

		if (cb->verbose) {
			if (strlen(cb->rdma_buf) > 128) {
				char msgbuf[128];

				strlcpy(msgbuf, cb->rdma_buf, sizeof(msgbuf));
				PRINTF(cb, "ping data stripped: %s\n",
				       msgbuf);
			} else
				PRINTF(cb, "ping data: %s\n", cb->rdma_buf);
		}
#ifdef SLOW_KRPING
		wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
#endif
	}
}

static void krping_rlat_test_client(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	cb->state = RDMA_READ_ADV;

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	if (cb->state == ERROR) {
		PRINTF(cb, "krping_format_send failed\n");
		return;
	}
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completion error %d\n", wc.status);
		return;
	}

	/* Spin waiting for server's Start STAG/TO/Len */
	while (cb->state < RDMA_WRITE_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}

#if 0
{
	int i;
	struct timeval start, stop;
	time_t sec;
	suseconds_t usec;
	unsigned long long elapsed;
	struct ib_wc wc;
	struct ib_send_wr *bad_wr;
	int ne;
	
	cb->rdma_sq_wr.opcode = IB_WR_RDMA_WRITE;
	cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
	cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
	cb->rdma_sq_wr.sg_list->length = 0;
	cb->rdma_sq_wr.num_sge = 0;

	microtime(&start);
	for (i=0; i < 100000; i++) {
		if (ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr)) {
			PRINTF(cb, "Couldn't post send\n");
			return;
		}
		do {
			ne = ib_poll_cq(cb->cq, 1, &wc);
		} while (ne == 0);
		if (ne < 0) {
			PRINTF(cb, "poll CQ failed %d\n", ne);
			return;
		}
		if (wc.status != IB_WC_SUCCESS) {
			PRINTF(cb, "Completion wth error at %s:\n",
				cb->server ? "server" : "client");
			PRINTF(cb, "Failed status %d: wr_id %d\n",
				wc.status, (int) wc.wr_id);
			return;
		}
	}
	microtime(&stop);
	
	if (stop.tv_usec < start.tv_usec) {
		stop.tv_usec += 1000000;
		stop.tv_sec  -= 1;
	}
	sec     = stop.tv_sec - start.tv_sec;
	usec    = stop.tv_usec - start.tv_usec;
	elapsed = sec * 1000000 + usec;
	PRINTF(cb, "0B-write-lat iters 100000 usec %llu\n", elapsed);
}
#endif

	rlat_test(cb);
}

static void krping_wlat_test_client(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	cb->state = RDMA_READ_ADV;

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	if (cb->state == ERROR) {
		PRINTF(cb, "krping_format_send failed\n");
		return;
	}
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completion error %d\n", wc.status);
		return;
	}

	/* Spin waiting for server's Start STAG/TO/Len */
	while (cb->state < RDMA_WRITE_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}

	wlat_test(cb);
}

static void krping_bw_test_client(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	cb->state = RDMA_READ_ADV;

	/* Send STAG/TO/Len to client */
	krping_format_send(cb, cb->start_dma_addr);
	if (cb->state == ERROR) {
		PRINTF(cb, "krping_format_send failed\n");
		return;
	}
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		PRINTF(cb, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		PRINTF(cb, "send completion error %d\n", wc.status);
		return;
	}

	/* Spin waiting for server's Start STAG/TO/Len */
	while (cb->state < RDMA_WRITE_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}

	bw_test(cb);
}


/*
 * fastreg 2 valid different mrs and verify the completions.
 */
static void krping_fr_test1(struct krping_cb *cb)
{
	struct ib_fast_reg_page_list *pl;
	struct ib_send_wr fr, *bad;
	struct ib_wc wc;
	struct ib_mr *mr1, *mr2;
	int i;
	int ret;
	int size = cb->size;
	int plen = (((size - 1) & PAGE_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
	int count = 0;

	pl = ib_alloc_fast_reg_page_list(cb->qp->device, plen);
	if (IS_ERR(pl)) {
		PRINTF(cb, "ib_alloc_fast_reg_page_list failed %ld\n", PTR_ERR(pl));
		return;
	}

	mr1 = ib_alloc_fast_reg_mr(cb->pd, plen);
	if (IS_ERR(mr1)) {
		PRINTF(cb, "ib_alloc_fast_reg_mr failed %ld\n", PTR_ERR(pl));
		goto err1;
	}
	mr2 = ib_alloc_fast_reg_mr(cb->pd, plen);
	if (IS_ERR(mr2)) {
		PRINTF(cb, "ib_alloc_fast_reg_mr failed %ld\n", PTR_ERR(pl));
		goto err2;
	}


	for (i=0; i<plen; i++)
		pl->page_list[i] = i * PAGE_SIZE;

	memset(&fr, 0, sizeof fr);
	fr.opcode = IB_WR_FAST_REG_MR;
	fr.wr_id = 1;
	fr.wr.fast_reg.page_shift = PAGE_SHIFT;
	fr.wr.fast_reg.length = size;
	fr.wr.fast_reg.page_list = pl;
	fr.wr.fast_reg.page_list_len = plen;
	fr.wr.fast_reg.iova_start = 0;
	fr.wr.fast_reg.access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
	fr.send_flags = IB_SEND_SIGNALED;
	fr.wr.fast_reg.rkey = mr1->rkey;
	DEBUG_LOG(cb, "%s fr1: stag 0x%x plen %u size %u depth %u\n", __func__, fr.wr.fast_reg.rkey, plen, cb->size, cb->txdepth);
	ret = ib_post_send(cb->qp, &fr, &bad);
	if (ret) {
		PRINTF(cb, "ib_post_send failed %d\n", ret);
		goto err3;
	}
	fr.wr.fast_reg.rkey = mr2->rkey;
	DEBUG_LOG(cb, "%s fr2: stag 0x%x plen %u size %u depth %u\n", __func__, fr.wr.fast_reg.rkey, plen, cb->size, cb->txdepth);
	ret = ib_post_send(cb->qp, &fr, &bad);
	if (ret) {
		PRINTF(cb, "ib_post_send failed %d\n", ret);
		goto err3;
	}

	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			goto err3;
		}
		if (ret == 1) {
			DEBUG_LOG(cb, "completion status %u wr %s\n",
				  wc.status, wc.wr_id == 1 ? "fr" : "inv");
			count++;
		} else if (krping_sigpending()) {
			PRINTF(cb, "signal!\n");
			goto err3;
		}

		wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	} while (count != 2);
err3:
	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	DEBUG_LOG(cb, "draining the cq...\n");
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			break;
		}
		if (ret == 1) {
			PRINTF(cb, "completion %u opcode %u\n", wc.status, wc.opcode);
		}
	} while (ret == 1);
	DEBUG_LOG(cb, "destroying fr mr2!\n");

	ib_dereg_mr(mr2);
err2:
	DEBUG_LOG(cb, "destroying fr mr1!\n");
	ib_dereg_mr(mr1);
err1:
	DEBUG_LOG(cb, "destroying fr page list!\n");
	ib_free_fast_reg_page_list(pl);
	DEBUG_LOG(cb, "%s done!\n", __func__);
}

/*
 * fastreg the same mr twice, 2nd one should produce error cqe.
 */
static void krping_fr_test2(struct krping_cb *cb)
{
	struct ib_fast_reg_page_list *pl;
	struct ib_send_wr fr, *bad;
	struct ib_wc wc;
	struct ib_mr *mr1;
	int i;
	int ret;
	int size = cb->size;
	int plen = (((size - 1) & PAGE_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
	int count = 0;

	pl = ib_alloc_fast_reg_page_list(cb->qp->device, plen);
	if (IS_ERR(pl)) {
		PRINTF(cb, "ib_alloc_fast_reg_page_list failed %ld\n", PTR_ERR(pl));
		return;
	}

	mr1 = ib_alloc_fast_reg_mr(cb->pd, plen);
	if (IS_ERR(mr1)) {
		PRINTF(cb, "ib_alloc_fast_reg_mr failed %ld\n", PTR_ERR(pl));
		goto err1;
	}

	for (i=0; i<plen; i++)
		pl->page_list[i] = i * PAGE_SIZE;

	memset(&fr, 0, sizeof fr);
	fr.opcode = IB_WR_FAST_REG_MR;
	fr.wr_id = 1;
	fr.wr.fast_reg.page_shift = PAGE_SHIFT;
	fr.wr.fast_reg.length = size;
	fr.wr.fast_reg.page_list = pl;
	fr.wr.fast_reg.page_list_len = plen;
	fr.wr.fast_reg.iova_start = 0;
	fr.wr.fast_reg.access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
	fr.send_flags = IB_SEND_SIGNALED;
	fr.wr.fast_reg.rkey = mr1->rkey;
	DEBUG_LOG(cb, "%s fr1: stag 0x%x plen %u size %u depth %u\n", __func__, fr.wr.fast_reg.rkey, plen, cb->size, cb->txdepth);
	ret = ib_post_send(cb->qp, &fr, &bad);
	if (ret) {
		PRINTF(cb, "ib_post_send failed %d\n", ret);
		goto err3;
	}
	DEBUG_LOG(cb, "%s fr2: stag 0x%x plen %u size %u depth %u\n", __func__, fr.wr.fast_reg.rkey, plen, cb->size, cb->txdepth);
	ret = ib_post_send(cb->qp, &fr, &bad);
	if (ret) {
		PRINTF(cb, "ib_post_send failed %d\n", ret);
		goto err3;
	}

	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			goto err3;
		}
		if (ret == 1) {
			DEBUG_LOG(cb, "completion status %u wr %s\n",
				  wc.status, wc.wr_id == 1 ? "fr" : "inv");
			count++;
		} else if (krping_sigpending()) {
			PRINTF(cb, "signal!\n");
			goto err3;
		}
		wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	} while (count != 2);
err3:
	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	DEBUG_LOG(cb, "draining the cq...\n");
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			break;
		}
		if (ret == 1) {
			PRINTF(cb, "completion %u opcode %u\n", wc.status, wc.opcode);
		}
	} while (ret == 1);
	DEBUG_LOG(cb, "destroying fr mr1!\n");
	ib_dereg_mr(mr1);
err1:
	DEBUG_LOG(cb, "destroying fr page list!\n");
	ib_free_fast_reg_page_list(pl);
	DEBUG_LOG(cb, "%s done!\n", __func__);
}

/*
 * fastreg pipelined in a loop as fast as we can until the user interrupts.
 * NOTE: every 9 seconds we sleep for 1 second to keep the kernel happy.
 */
static void krping_fr_test3(struct krping_cb *cb)
{
	struct ib_fast_reg_page_list *pl;
	struct ib_send_wr fr, inv, *bad;
	struct ib_wc wc;
	u8 key = 0;
	struct ib_mr *mr;
	int i;
	int ret;
	int size = cb->size;
	int plen = (((size - 1) & PAGE_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
	unsigned long start;
	int count = 0;
	int scnt = 0;


	pl = ib_alloc_fast_reg_page_list(cb->qp->device, plen);
	if (IS_ERR(pl)) {
		PRINTF(cb, "ib_alloc_fast_reg_page_list failed %ld\n", PTR_ERR(pl));
		return;
	}
	
	mr = ib_alloc_fast_reg_mr(cb->pd, plen);
	if (IS_ERR(mr)) {
		PRINTF(cb, "ib_alloc_fast_reg_mr failed %ld\n", PTR_ERR(pl));
		goto err1;
	}

	for (i=0; i<plen; i++)
		pl->page_list[i] = i * PAGE_SIZE;
	
	memset(&fr, 0, sizeof fr);
	fr.opcode = IB_WR_FAST_REG_MR;
	fr.wr.fast_reg.page_shift = PAGE_SHIFT;
	fr.wr.fast_reg.length = size;
	fr.wr.fast_reg.page_list = pl;
	fr.wr.fast_reg.page_list_len = plen;
	fr.wr.fast_reg.iova_start = 0;
	fr.send_flags = IB_SEND_SIGNALED;
	fr.wr.fast_reg.access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
	fr.next = &inv;
	memset(&inv, 0, sizeof inv);
	inv.opcode = IB_WR_LOCAL_INV;
	inv.send_flags = IB_SEND_SIGNALED;
	
	DEBUG_LOG(cb, "fr_test: stag index 0x%x plen %u size %u depth %u\n", mr->rkey >> 8, plen, cb->size, cb->txdepth);
	start = time_uptime;
	while (1) {
		if ((time_uptime - start) >= 9) {
			DEBUG_LOG(cb, "fr_test: pausing 1 second! count %u latest size %u plen %u\n", count, size, plen);
			wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
			if (cb->state == ERROR)
				break;
			start = time_uptime;
		}	
		while (scnt < (cb->txdepth>>1)) {
			ib_update_fast_reg_key(mr, ++key);
			fr.wr.fast_reg.rkey = mr->rkey;
			inv.ex.invalidate_rkey = mr->rkey;
			size = arc4random() % cb->size;
			if (size == 0)
				size = cb->size;
			plen = (((size - 1) & PAGE_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
			fr.wr.fast_reg.length = size;
			fr.wr.fast_reg.page_list_len = plen;
			ret = ib_post_send(cb->qp, &fr, &bad);
			if (ret) {
				PRINTF(cb, "ib_post_send failed %d\n", ret);
				goto err2;	
			}
			scnt+=2;
		}

		do {
			ret = ib_poll_cq(cb->cq, 1, &wc);
			if (ret < 0) {
				PRINTF(cb, "ib_poll_cq failed %d\n", ret);
				goto err2;	
			}
			if (ret == 1) {
				if (wc.status) {
					PRINTF(cb, "completion error %u\n", wc.status);
					goto err2;
				}
				count++;
				scnt--;
			}
			else if (krping_sigpending()) {
				PRINTF(cb, "signal!\n");
				goto err2;
			}
		} while (ret == 1);
	}
err2:
	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	DEBUG_LOG(cb, "draining the cq...\n");
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			break;
		}
		if (ret == 1) {
			if (wc.status) {
				PRINTF(cb, "completion error %u opcode %u\n", wc.status, wc.opcode);
			}
		}
	} while (ret == 1);
	DEBUG_LOG(cb, "fr_test: done!\n");
	ib_dereg_mr(mr);
err1:
	DEBUG_LOG(cb, "destroying fr page list!\n");
	ib_free_fast_reg_page_list(pl);
	DEBUG_LOG(cb, "%s done!\n", __func__);
}

/*
 * fastreg 1 and invalidate 1 mr and verify completion.
 */
static void krping_fr_test4(struct krping_cb *cb)
{
	struct ib_fast_reg_page_list *pl;
	struct ib_send_wr fr, inv, *bad;
	struct ib_wc wc;
	struct ib_mr *mr1;
	int i;
	int ret;
	int size = cb->size;
	int plen = (((size - 1) & PAGE_MASK) + PAGE_SIZE) >> PAGE_SHIFT;
	int count = 0;

	pl = ib_alloc_fast_reg_page_list(cb->qp->device, plen);
	if (IS_ERR(pl)) {
		PRINTF(cb, "ib_alloc_fast_reg_page_list failed %ld\n", PTR_ERR(pl));
		return;
	}

	mr1 = ib_alloc_fast_reg_mr(cb->pd, plen);
	if (IS_ERR(mr1)) {
		PRINTF(cb, "ib_alloc_fast_reg_mr failed %ld\n", PTR_ERR(pl));
		goto err1;
	}

	for (i=0; i<plen; i++)
		pl->page_list[i] = i * PAGE_SIZE;

	memset(&fr, 0, sizeof fr);
	fr.opcode = IB_WR_FAST_REG_MR;
	fr.wr_id = 1;
	fr.wr.fast_reg.page_shift = PAGE_SHIFT;
	fr.wr.fast_reg.length = size;
	fr.wr.fast_reg.page_list = pl;
	fr.wr.fast_reg.page_list_len = plen;
	fr.wr.fast_reg.iova_start = 0;
	fr.wr.fast_reg.access_flags = IB_ACCESS_REMOTE_WRITE | IB_ACCESS_LOCAL_WRITE;
	fr.send_flags = IB_SEND_SIGNALED;
	fr.wr.fast_reg.rkey = mr1->rkey;
	fr.next = &inv;
	memset(&inv, 0, sizeof inv);
	inv.opcode = IB_WR_LOCAL_INV;
	inv.ex.invalidate_rkey = mr1->rkey;

	DEBUG_LOG(cb, "%s fr1: stag 0x%x plen %u size %u depth %u\n", __func__, fr.wr.fast_reg.rkey, plen, cb->size, cb->txdepth);
	ret = ib_post_send(cb->qp, &fr, &bad);
	if (ret) {
		PRINTF(cb, "ib_post_send failed %d\n", ret);
		goto err3;
	}
	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			goto err3;
		}
		if (ret == 1) {
			DEBUG_LOG(cb, "completion status %u wr %s\n",
				  wc.status, wc.wr_id == 1 ? "fr" : "inv");
			count++;
		} else if (krping_sigpending()) {
			PRINTF(cb, "signal!\n");
			goto err3;
		}
		wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	} while (count != 1);
err3:
	DEBUG_LOG(cb, "sleeping 1 second\n");
	wait_event_interruptible_timeout(cb->sem, cb->state == ERROR, HZ);
	DEBUG_LOG(cb, "draining the cq...\n");
	do {
		ret = ib_poll_cq(cb->cq, 1, &wc);
		if (ret < 0) {
			PRINTF(cb, "ib_poll_cq failed %d\n", ret);
			break;
		}
		if (ret == 1) {
			PRINTF(cb, "completion %u opcode %u\n", wc.status, wc.opcode);
		}
	} while (ret == 1);
	DEBUG_LOG(cb, "destroying fr mr1!\n");
	ib_dereg_mr(mr1);
err1:
	DEBUG_LOG(cb, "destroying fr page list!\n");
	ib_free_fast_reg_page_list(pl);
	DEBUG_LOG(cb, "%s done!\n", __func__);
}

static void krping_fr_test(struct krping_cb *cb)
{
	switch (cb->testnum) {
	case 1:
		krping_fr_test1(cb);
		break;
	case 2:
		krping_fr_test2(cb);
		break;
	case 3:
		krping_fr_test3(cb);
		break;
	case 4:
		krping_fr_test4(cb);
		break;
	case 5:
		krping_fr_test5_client(cb);
		break;
	case 6:
		krping_fr_test6_client(cb);
		break;
	default:
		PRINTF(cb, "Unkown frtest num %u\n", cb->testnum);
		break;
	}
}

static int krping_connect_client(struct krping_cb *cb)
{
	struct rdma_conn_param conn_param;
	int ret;

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;
	conn_param.retry_count = 10;

	ret = rdma_connect(cb->cm_id, &conn_param);
	if (ret) {
		PRINTF(cb, "rdma_connect error %d\n", ret);
		return ret;
	}

	wait_event_interruptible(cb->sem, cb->state >= CONNECTED);
	if (cb->state == ERROR) {
		PRINTF(cb, "wait for CONNECTED state %d\n", cb->state);
		return -1;
	}

	DEBUG_LOG(cb, "rdma_connect successful\n");
	return 0;
}

static int krping_bind_client(struct krping_cb *cb)
{
	struct sockaddr_in sin;
	int ret;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = cb->addr.s_addr;
	sin.sin_port = cb->port;

	ret = rdma_resolve_addr(cb->cm_id, NULL, (struct sockaddr *) &sin,
				2000);
	if (ret) {
		PRINTF(cb, "rdma_resolve_addr error %d\n", ret);
		return ret;
	}

	wait_event_interruptible(cb->sem, cb->state >= ROUTE_RESOLVED);
	if (cb->state != ROUTE_RESOLVED) {
		PRINTF(cb, 
		       "addr/route resolution did not resolve: state %d\n",
		       cb->state);
		return -EINTR;
	}

	if (cb->mem == FASTREG && !fastreg_supported(cb, 0))
		return -EINVAL;

	DEBUG_LOG(cb, "rdma_resolve_addr - rdma_resolve_route successful\n");
	return 0;
}

static void krping_run_client(struct krping_cb *cb)
{
	struct ib_recv_wr *bad_wr;
	int ret;

	ret = krping_bind_client(cb);
	if (ret)
		return;

	ret = krping_setup_qp(cb, cb->cm_id);
	if (ret) {
		PRINTF(cb, "setup_qp failed: %d\n", ret);
		return;
	}

	ret = krping_setup_buffers(cb);
	if (ret) {
		PRINTF(cb, "krping_setup_buffers failed: %d\n", ret);
		goto err1;
	}

	ret = ib_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
	if (ret) {
		PRINTF(cb, "ib_post_recv failed: %d\n", ret);
		goto err2;
	}

	ret = krping_connect_client(cb);
	if (ret) {
		PRINTF(cb, "connect error %d\n", ret);
		goto err2;
	}

	if (cb->wlat)
		krping_wlat_test_client(cb);
	else if (cb->rlat)
		krping_rlat_test_client(cb);
	else if (cb->bw)
		krping_bw_test_client(cb);
	else if (cb->frtest)
		krping_fr_test(cb);
	else
		krping_test_client(cb);
	rdma_disconnect(cb->cm_id);
err2:
	krping_free_buffers(cb);
err1:
	krping_free_qp(cb);
}

int krping_doit(char *cmd, void *cookie)
{
	struct krping_cb *cb;
	int op;
	int ret = 0;
	char *optarg;
	unsigned long optint;

	cb = kzalloc(sizeof(*cb), GFP_KERNEL);
	if (!cb)
		return -ENOMEM;

	mutex_lock(&krping_mutex);
	list_add_tail(&cb->list, &krping_cbs);
	mutex_unlock(&krping_mutex);

	cb->cookie = cookie;
	cb->server = -1;
	cb->state = IDLE;
	cb->size = 64;
	cb->txdepth = RPING_SQ_DEPTH;
	cb->mem = DMA;
	init_waitqueue_head(&cb->sem);

	while ((op = krping_getopt("krping", &cmd, krping_opts, NULL, &optarg,
			      &optint)) != 0) {
		switch (op) {
		case 'a':
			cb->addr_str = optarg;
			DEBUG_LOG(cb, "ipaddr (%s)\n", optarg);
			if (!inet_aton(optarg, &cb->addr)) {
				PRINTF(cb, "bad addr string %s\n",
				    optarg);
				ret = EINVAL;
			}
			break;
		case 'p':
			cb->port = htons(optint);
			DEBUG_LOG(cb, "port %d\n", (int)optint);
			break;
		case 'P':
			cb->poll = 1;
			DEBUG_LOG(cb, "server\n");
			break;
		case 's':
			cb->server = 1;
			DEBUG_LOG(cb, "server\n");
			break;
		case 'c':
			cb->server = 0;
			DEBUG_LOG(cb, "client\n");
			break;
		case 'S':
			cb->size = optint;
			if ((cb->size < 1) ||
			    (cb->size > RPING_BUFSIZE)) {
				PRINTF(cb, "Invalid size %d "
				       "(valid range is 1 to %d)\n",
				       cb->size, RPING_BUFSIZE);
				ret = EINVAL;
			} else
				DEBUG_LOG(cb, "size %d\n", (int)optint);
			break;
		case 'C':
			cb->count = optint;
			if (cb->count < 0) {
				PRINTF(cb, "Invalid count %d\n",
					cb->count);
				ret = EINVAL;
			} else
				DEBUG_LOG(cb, "count %d\n", (int) cb->count);
			break;
		case 'v':
			cb->verbose++;
			DEBUG_LOG(cb, "verbose\n");
			break;
		case 'V':
			cb->validate++;
			DEBUG_LOG(cb, "validate data\n");
			break;
		case 'l':
			cb->wlat++;
			break;
		case 'L':
			cb->rlat++;
			break;
		case 'B':
			cb->bw++;
			break;
		case 'd':
			cb->duplex++;
			break;
		case 'm':
			if (!strncmp(optarg, "dma", 3))
				cb->mem = DMA;
			else if (!strncmp(optarg, "fastreg", 7))
				cb->mem = FASTREG;
			else if (!strncmp(optarg, "mw", 2))
				cb->mem = MW;
			else if (!strncmp(optarg, "mr", 2))
				cb->mem = MR;
			else {
				PRINTF(cb, "unknown mem mode %s.  "
					"Must be dma, fastreg, mw, or mr\n",
					optarg);
				ret = -EINVAL;
				break;
			}
			break;
		case 'I':
			cb->server_invalidate = 1;
			break;
		case 'T':
			cb->txdepth = optint;
			DEBUG_LOG(cb, "txdepth %d\n", (int) cb->txdepth);
			break;
		case 'Z':
			cb->local_dma_lkey = 1;
			DEBUG_LOG(cb, "using local dma lkey\n");
			break;
		case 'R':
			cb->read_inv = 1;
			DEBUG_LOG(cb, "using read-with-inv\n");
			break;
		case 'f':
			cb->frtest = 1;
			cb->testnum = optint;
			DEBUG_LOG(cb, "fast-reg test!\n");
			break;
		default:
			PRINTF(cb, "unknown opt %s\n", optarg);
			ret = -EINVAL;
			break;
		}
	}
	if (ret)
		goto out;

	if (cb->server == -1) {
		PRINTF(cb, "must be either client or server\n");
		ret = -EINVAL;
		goto out;
	}

	if ((cb->frtest + cb->bw + cb->rlat + cb->wlat) > 1) {
		PRINTF(cb, "Pick only one test: fr, bw, rlat, wlat\n");
		ret = -EINVAL;
		goto out;
	}
	if (cb->server_invalidate && cb->mem != FASTREG) {
		PRINTF(cb, "server_invalidate only valid with fastreg mem_mode\n");
		ret = -EINVAL;
		goto out;
	}

	if (cb->read_inv && cb->mem != FASTREG) {
		PRINTF(cb, "read_inv only valid with fastreg mem_mode\n");
		ret = -EINVAL;
		goto out;
	}

	if (cb->mem != MR && (cb->wlat || cb->rlat || cb->bw || cb->frtest)) {
		PRINTF(cb, "wlat, rlat, and bw tests only support mem_mode MR\n");
		ret = -EINVAL;
		goto out;
	}

	cb->cm_id = rdma_create_id(krping_cma_event_handler, cb, RDMA_PS_TCP, IB_QPT_RC);
	if (IS_ERR(cb->cm_id)) {
		ret = PTR_ERR(cb->cm_id);
		PRINTF(cb, "rdma_create_id error %d\n", ret);
		goto out;
	}
	DEBUG_LOG(cb, "created cm_id %p\n", cb->cm_id);

	if (cb->server)
		krping_run_server(cb);
	else
		krping_run_client(cb);

	DEBUG_LOG(cb, "destroy cm_id %p\n", cb->cm_id);
	rdma_destroy_id(cb->cm_id);
out:
	mutex_lock(&krping_mutex);
	list_del(&cb->list);
	mutex_unlock(&krping_mutex);
	kfree(cb);
	return ret;
}

void
krping_walk_cb_list(void (*f)(struct krping_stats *, void *), void *arg)
{
	struct krping_cb *cb;

	mutex_lock(&krping_mutex);
	list_for_each_entry(cb, &krping_cbs, list)
	    (*f)(cb->pd ? &cb->stats : NULL, arg);
	mutex_unlock(&krping_mutex);
}

void krping_init(void)
{

	mutex_init(&krping_mutex);
}
