/*
 * Copyright (c) 2005 Ammasso, Inc. All rights reserved.
 * Copyright (c) 2006 Open Grid Computing, Inc. All rights reserved.
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

#include <sys/ctype.h>

#include <sys/param.h>
#include <sys/condvar.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/syslog.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <contrib/rdma/rdma_cm.h>

#include "getopt.h"
#include "krping.h"

#define PFX "krping: "

static int debug = 0;
#define DEBUG_LOG if (debug) printf

static const struct krping_option krping_opts[] = {
	{"count", OPT_INT, 'C'},
	{"size", OPT_INT, 'S'},
	{"addr", OPT_STRING, 'a'},
	{"port", OPT_INT, 'p'},
	{"verbose", OPT_NOPARAM, 'v'},
	{"validate", OPT_NOPARAM, 'V'},
	{"server", OPT_NOPARAM, 's'},
	{"client", OPT_NOPARAM, 'c'},
	{"dmamr", OPT_NOPARAM, 'D'},
	{"debug", OPT_NOPARAM, 'd'},
	{"wlat", OPT_NOPARAM, 'l'},
	{"rlat", OPT_NOPARAM, 'L'},
	{"bw", OPT_NOPARAM, 'B'},
	{"tx-depth", OPT_INT, 't'},
  	{"poll", OPT_NOPARAM, 'P'},
	{NULL, 0, 0}
};

struct mtx krping_mutex;

/*
 * List of running krping threads.
 */
struct krping_cb_list krping_cbs;

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
 * Default max buffer size for IO...
 */
#define RPING_BUFSIZE 128*1024
#define RPING_SQ_DEPTH 32

static void krping_wait(struct krping_cb *cb, int state)
{
	int rc;
	mtx_lock(&cb->lock);
	while (cb->state < state) {
		rc = msleep(cb, &cb->lock, 0, "krping", 0);
		if (rc && rc != ERESTART) {
			cb->state = ERROR;
			break;
		}
	}
	mtx_unlock(&cb->lock);
}

static int krping_cma_event_handler(struct rdma_cm_id *cma_id,
				   struct rdma_cm_event *event)
{
	int ret;
	struct krping_cb *cb = cma_id->context;

	DEBUG_LOG(PFX "cma_event type %d cma_id %p (%s)\n", event->event, cma_id,
		  (cma_id == cb->cm_id) ? "parent" : "child");

	mtx_lock(&cb->lock);
	switch (event->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		cb->state = ADDR_RESOLVED;
		ret = rdma_resolve_route(cma_id, 2000);
		if (ret) {
			log(LOG_ERR, "rdma_resolve_route error %d\n", 
			       ret);
			wakeup(cb);
		}
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		cb->state = ROUTE_RESOLVED;
		wakeup(cb);
		break;

	case RDMA_CM_EVENT_CONNECT_REQUEST:
		cb->state = CONNECT_REQUEST;
		cb->child_cm_id = cma_id;
		DEBUG_LOG(PFX "child cma %p\n", cb->child_cm_id);
		wakeup(cb);
		break;

	case RDMA_CM_EVENT_ESTABLISHED:
		DEBUG_LOG(PFX "ESTABLISHED\n");
		if (!cb->server) {
			cb->state = CONNECTED;
			wakeup(cb);
		}
		break;

	case RDMA_CM_EVENT_ADDR_ERROR:
	case RDMA_CM_EVENT_ROUTE_ERROR:
	case RDMA_CM_EVENT_CONNECT_ERROR:
	case RDMA_CM_EVENT_UNREACHABLE:
	case RDMA_CM_EVENT_REJECTED:
		log(LOG_ERR, "cma event %d, error %d\n", event->event,
		       event->status);
		cb->state = ERROR;
		wakeup(cb);
		break;

	case RDMA_CM_EVENT_DISCONNECTED:
		DEBUG_LOG(PFX "DISCONNECT EVENT...\n");
		cb->state = ERROR;
		wakeup(cb);
		break;

	case RDMA_CM_EVENT_DEVICE_REMOVAL:
		DEBUG_LOG(PFX "cma detected device removal!!!!\n");
		break;

	default:
		log(LOG_ERR, "oof bad type!\n");
		wakeup(cb);
		break;
	}
	mtx_unlock(&cb->lock);
	return 0;
}

static int server_recv(struct krping_cb *cb, struct ib_wc *wc)
{
	if (wc->byte_len != sizeof(cb->recv_buf)) {
		log(LOG_ERR, "Received bogus data, size %d\n", 
		       wc->byte_len);
		return -1;
	}

	cb->remote_rkey = ntohl(cb->recv_buf.rkey);
	cb->remote_addr = ntohll(cb->recv_buf.buf);
	cb->remote_len  = ntohl(cb->recv_buf.size);
	DEBUG_LOG(PFX "Received rkey %x addr %llx len %d from peer\n",
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
		log(LOG_ERR, "Received bogus data, size %d\n", 
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

	mtx_lock(&cb->lock);
	KASSERT(cb->cq == cq, ("bad condition"));
	if (cb->state == ERROR) {
		log(LOG_ERR,  "cq completion in ERROR state\n");
		mtx_unlock(&cb->lock);
		return;
	}
	if (!cb->wlat && !cb->rlat && !cb->bw)
		ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
	while ((ret = ib_poll_cq(cb->cq, 1, &wc)) == 1) {
		if (wc.status) {
			if (wc.status != IB_WC_WR_FLUSH_ERR)
				log(LOG_ERR, "cq completion failed status %d\n",
					wc.status);
			goto error;
		}

		switch (wc.opcode) {
		case IB_WC_SEND:
			DEBUG_LOG(PFX "send completion\n");
			cb->stats.send_bytes += cb->send_sgl.length;
			cb->stats.send_msgs++;
			break;

		case IB_WC_RDMA_WRITE:
			DEBUG_LOG(PFX "rdma write completion\n");
			cb->stats.write_bytes += cb->rdma_sq_wr.sg_list->length;
			cb->stats.write_msgs++;
			cb->state = RDMA_WRITE_COMPLETE;
			wakeup(cb);
			break;

		case IB_WC_RDMA_READ:
			DEBUG_LOG(PFX "rdma read completion\n");
			cb->stats.read_bytes += cb->rdma_sq_wr.sg_list->length;
			cb->stats.read_msgs++;
			cb->state = RDMA_READ_COMPLETE;
			wakeup(cb);
			break;

		case IB_WC_RECV:
			DEBUG_LOG(PFX "recv completion\n");
			cb->stats.recv_bytes += sizeof(cb->recv_buf);
			cb->stats.recv_msgs++;
			if (cb->wlat || cb->rlat || cb->bw)
				ret = server_recv(cb, &wc);
			else
				ret = cb->server ? server_recv(cb, &wc) :
					   client_recv(cb, &wc);
			if (ret) {
				log(LOG_ERR, "recv wc error: %d\n", ret);
				goto error;
			}

			ret = ib_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
			if (ret) {
				log(LOG_ERR, "post recv error: %d\n", 
				       ret);
				goto error;
			}
			wakeup(cb);
			break;

		default:
			log(LOG_ERR, "unknown!!!!! completion\n");
			goto error;
		}
	}
	if (ret) {
		log(LOG_ERR, "poll error %d\n", ret);
		goto error;
	}
	mtx_unlock(&cb->lock);
	return;
error:
	cb->state = ERROR;
	wakeup(cb);
	mtx_unlock(&cb->lock);
}

static int krping_accept(struct krping_cb *cb)
{
	struct rdma_conn_param conn_param;
	int ret;

	DEBUG_LOG(PFX "accepting client connection request\n");

	memset(&conn_param, 0, sizeof conn_param);
	conn_param.responder_resources = 1;
	conn_param.initiator_depth = 1;

	ret = rdma_accept(cb->child_cm_id, &conn_param);
	if (ret) {
		log(LOG_ERR, "rdma_accept error: %d\n", ret);
		return ret;
	}

	if (!cb->wlat && !cb->rlat && !cb->bw) {
		krping_wait(cb, CONNECTED);
		if (cb->state == ERROR) {
			log(LOG_ERR,  "wait for CONNECTED state %d\n", cb->state);
			return -1;
		}
	}
	return 0;
}

static void krping_setup_wr(struct krping_cb *cb)
{
	/* XXX X86 only here... not mapping for dma! */
	cb->recv_sgl.addr = vtophys(&cb->recv_buf);
	cb->recv_sgl.length = sizeof cb->recv_buf;
	if (cb->use_dmamr)
		cb->recv_sgl.lkey = cb->dma_mr->lkey;
	else
		cb->recv_sgl.lkey = cb->recv_mr->lkey;
	cb->rq_wr.sg_list = &cb->recv_sgl;
	cb->rq_wr.num_sge = 1;

	cb->send_sgl.addr = vtophys(&cb->send_buf);
	cb->send_sgl.length = sizeof cb->send_buf;
	if (cb->use_dmamr)
		cb->send_sgl.lkey = cb->dma_mr->lkey;
	else
		cb->send_sgl.lkey = cb->send_mr->lkey;

	cb->sq_wr.opcode = IB_WR_SEND;
	cb->sq_wr.send_flags = IB_SEND_SIGNALED;
	cb->sq_wr.sg_list = &cb->send_sgl;
	cb->sq_wr.num_sge = 1;

	cb->rdma_addr = vtophys(cb->rdma_buf);
	cb->rdma_sgl.addr = cb->rdma_addr;
	if (cb->use_dmamr)
		cb->rdma_sgl.lkey = cb->dma_mr->lkey;
	else
		cb->rdma_sgl.lkey = cb->rdma_mr->lkey;
	cb->rdma_sq_wr.send_flags = IB_SEND_SIGNALED;
	cb->rdma_sq_wr.sg_list = &cb->rdma_sgl;
	cb->rdma_sq_wr.num_sge = 1;

	if (!cb->server || cb->wlat || cb->rlat || cb->bw) {
		cb->start_addr = vtophys(cb->start_buf);
	}
}

static int krping_setup_buffers(struct krping_cb *cb)
{
	int ret;
	struct ib_phys_buf buf;
	u64 iovbase;

	DEBUG_LOG(PFX "krping_setup_buffers called on cb %p\n", cb);

	if (cb->use_dmamr) {
		cb->dma_mr = ib_get_dma_mr(cb->pd, IB_ACCESS_LOCAL_WRITE|
					   IB_ACCESS_REMOTE_READ|
				           IB_ACCESS_REMOTE_WRITE);
		if (IS_ERR(cb->dma_mr)) {
			log(LOG_ERR, "reg_dmamr failed\n");
			return PTR_ERR(cb->dma_mr);
		}
	} else {

		buf.addr = vtophys(&cb->recv_buf);
		buf.size = sizeof cb->recv_buf;
		iovbase = vtophys(&cb->recv_buf);
		cb->recv_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
					     IB_ACCESS_LOCAL_WRITE, 
					     &iovbase);

		if (IS_ERR(cb->recv_mr)) {
			log(LOG_ERR, "recv_buf reg_mr failed\n");
			return PTR_ERR(cb->recv_mr);
		}

		buf.addr = vtophys(&cb->send_buf);
		buf.size = sizeof cb->send_buf;
		iovbase = vtophys(&cb->send_buf);
		cb->send_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
					     0, &iovbase);

		if (IS_ERR(cb->send_mr)) {
			log(LOG_ERR, "send_buf reg_mr failed\n");
			ib_dereg_mr(cb->recv_mr);
			return PTR_ERR(cb->send_mr);
		}
	}

	cb->rdma_buf = contigmalloc(cb->size, M_DEVBUF, M_WAITOK, 0, -1UL,
		PAGE_SIZE, 0);

	if (!cb->rdma_buf) {
		log(LOG_ERR, "rdma_buf malloc failed\n");
		ret = ENOMEM;
		goto err1;
	}
	if (!cb->use_dmamr) {

		buf.addr = vtophys(cb->rdma_buf);
		buf.size = cb->size;
		iovbase = vtophys(cb->rdma_buf);
		cb->rdma_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
					     IB_ACCESS_REMOTE_READ| 
					     IB_ACCESS_REMOTE_WRITE, 
					     &iovbase);

		if (IS_ERR(cb->rdma_mr)) {
			log(LOG_ERR, "rdma_buf reg_mr failed\n");
			ret = PTR_ERR(cb->rdma_mr);
			goto err2;
		}
	}

	if (!cb->server || cb->wlat || cb->rlat || cb->bw) {
		cb->start_buf = contigmalloc(cb->size, M_DEVBUF, M_WAITOK,
			0, -1UL, PAGE_SIZE, 0);
		if (!cb->start_buf) {
			log(LOG_ERR, "start_buf malloc failed\n");
			ret = ENOMEM;
			goto err2;
		}
		if (!cb->use_dmamr) {
			unsigned flags = IB_ACCESS_REMOTE_READ;

			if (cb->wlat || cb->rlat || cb->bw) 
				flags |= IB_ACCESS_REMOTE_WRITE;
			buf.addr = vtophys(cb->start_buf);
			buf.size = cb->size;
			iovbase = vtophys(cb->start_buf);
			cb->start_mr = ib_reg_phys_mr(cb->pd, &buf, 1, 
					     flags,
					     &iovbase);

			if (IS_ERR(cb->start_mr)) {
				log(LOG_ERR, "start_buf reg_mr failed\n");
				ret = PTR_ERR(cb->start_mr);
				goto err3;
			}
		}
	}

	krping_setup_wr(cb);
	DEBUG_LOG(PFX "allocated & registered buffers...\n");
	return 0;
err3:
	contigfree(cb->start_buf, cb->size, M_DEVBUF);

	if (!cb->use_dmamr)
		ib_dereg_mr(cb->rdma_mr);
err2:
	contigfree(cb->rdma_buf, cb->size, M_DEVBUF);
err1:
	if (cb->use_dmamr)
		ib_dereg_mr(cb->dma_mr);
	else {
		ib_dereg_mr(cb->recv_mr);
		ib_dereg_mr(cb->send_mr);
	}
	return ret;
}

static void krping_free_buffers(struct krping_cb *cb)
{
	DEBUG_LOG(PFX "krping_free_buffers called on cb %p\n", cb);
	
#if 0
	dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, recv_mapping),
			 sizeof(cb->recv_buf), DMA_BIDIRECTIONAL);
	dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, send_mapping),
			 sizeof(cb->send_buf), DMA_BIDIRECTIONAL);
	dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, rdma_mapping),
			 cb->size, DMA_BIDIRECTIONAL);
#endif
	contigfree(cb->rdma_buf, cb->size, M_DEVBUF);
	if (!cb->server || cb->wlat || cb->rlat || cb->bw) {
#if 0
		dma_unmap_single(cb->pd->device->dma_device,
			 pci_unmap_addr(cb, start_mapping),
			 cb->size, DMA_BIDIRECTIONAL);
#endif
		contigfree(cb->start_buf, cb->size, M_DEVBUF);
	}
	if (cb->use_dmamr)
		ib_dereg_mr(cb->dma_mr);
	else {
		ib_dereg_mr(cb->send_mr);
		ib_dereg_mr(cb->recv_mr);
		ib_dereg_mr(cb->rdma_mr);
		if (!cb->server)
			ib_dereg_mr(cb->start_mr);
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
		log(LOG_ERR, "ib_alloc_pd failed\n");
		return PTR_ERR(cb->pd);
	}
	DEBUG_LOG(PFX "created pd %p\n", cb->pd);

	cb->cq = ib_create_cq(cm_id->device, krping_cq_event_handler, NULL,
			      cb, cb->txdepth * 2, 0);
	if (IS_ERR(cb->cq)) {
		log(LOG_ERR, "ib_create_cq failed\n");
		ret = PTR_ERR(cb->cq);
		goto err1;
	}
	DEBUG_LOG(PFX "created cq %p\n", cb->cq);

	if (!cb->wlat && !cb->rlat && !cb->bw) {
		ret = ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
		if (ret) {
			log(LOG_ERR, "ib_create_cq failed\n");
			goto err2;
		}
	}

	ret = krping_create_qp(cb);
	if (ret) {
		log(LOG_ERR, "krping_create_qp failed: %d\n", ret);
		goto err2;
	}
	DEBUG_LOG(PFX "created qp %p\n", cb->qp);
	return 0;
err2:
	ib_destroy_cq(cb->cq);
err1:
	ib_dealloc_pd(cb->pd);
	return ret;
}

static void krping_format_send(struct krping_cb *cb, u64 buf, 
			       struct ib_mr *mr)
{
	struct krping_rdma_info *info = &cb->send_buf;

	info->buf = htonll(buf);
	info->rkey = htonl(mr->rkey);
	info->size = htonl(cb->size);

	DEBUG_LOG(PFX "RDMA addr %llx rkey %x len %d\n",
		  (unsigned long long)buf, mr->rkey, cb->size);
}

static void krping_test_server(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	int ret;

	while (1) {
		/* Wait for client's Start STAG/TO/Len */
		krping_wait(cb, RDMA_READ_ADV);
		if (cb->state != RDMA_READ_ADV) {
			DEBUG_LOG(PFX "wait for RDMA_READ_ADV state %d\n",
				cb->state);
			break;
		}

		DEBUG_LOG(PFX "server received sink adv\n");

		/* Issue RDMA Read. */
		cb->rdma_sq_wr.opcode = IB_WR_RDMA_READ;
		cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
		cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
		cb->rdma_sq_wr.sg_list->length = cb->remote_len;

		ret = ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
		if (ret) {
			log(LOG_ERR, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG(PFX "server posted rdma read req \n");

		/* Wait for read completion */
		krping_wait(cb, RDMA_READ_COMPLETE);
		if (cb->state != RDMA_READ_COMPLETE) {
			log(LOG_ERR,  
			       "wait for RDMA_READ_COMPLETE state %d\n",
			       cb->state);
			break;
		}
		DEBUG_LOG(PFX "server received read complete\n");

		/* Display data in recv buf */
		if (cb->verbose)
			DEBUG_LOG("server ping data: %s\n", cb->rdma_buf);

		/* Tell client to continue */
		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			log(LOG_ERR, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG(PFX "server posted go ahead\n");

		/* Wait for client's RDMA STAG/TO/Len */
		krping_wait(cb, RDMA_WRITE_ADV);
		if (cb->state != RDMA_WRITE_ADV) {
			log(LOG_ERR,  
			       "wait for RDMA_WRITE_ADV state %d\n",
			       cb->state);
			break;
		}
		DEBUG_LOG(PFX "server received sink adv\n");

		/* RDMA Write echo data */
		cb->rdma_sq_wr.opcode = IB_WR_RDMA_WRITE;
		cb->rdma_sq_wr.wr.rdma.rkey = cb->remote_rkey;
		cb->rdma_sq_wr.wr.rdma.remote_addr = cb->remote_addr;
		cb->rdma_sq_wr.sg_list->length = strlen(cb->rdma_buf) + 1;
		DEBUG_LOG(PFX "rdma write from lkey %x laddr %llx len %d\n",
			  cb->rdma_sq_wr.sg_list->lkey,
			  (unsigned long long)cb->rdma_sq_wr.sg_list->addr,
			  cb->rdma_sq_wr.sg_list->length);

		ret = ib_post_send(cb->qp, &cb->rdma_sq_wr, &bad_wr);
		if (ret) {
			log(LOG_ERR, "post send error %d\n", ret);
			break;
		}

		/* Wait for completion */
		krping_wait(cb, RDMA_WRITE_COMPLETE);
		if (cb->state != RDMA_WRITE_COMPLETE) {
			log(LOG_ERR,  
			       "wait for RDMA_WRITE_COMPLETE state %d\n",
			       cb->state);
			break;
		}
		DEBUG_LOG(PFX "server rdma write complete \n");

		cb->state = CONNECTED;

		/* Tell client to begin again */
		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			log(LOG_ERR, "post send error %d\n", ret);
			break;
		}
		DEBUG_LOG(PFX "server posted go ahead\n");
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
			log(LOG_ERR,  
				"Couldn't post send: ret=%d scnt %d\n",
				ret, scnt);
			return;
		}

		do {
			if (!cb->poll) {
				krping_wait(cb, RDMA_READ_COMPLETE);
				if (cb->state == RDMA_READ_COMPLETE) {
					ne = 1;
					ib_req_notify_cq(cb->cq, IB_CQ_NEXT_COMP);
				} else {
					ne = -1;
				}
			} else
				ne = ib_poll_cq(cb->cq, 1, &wc);
			if (cb->state == ERROR) {
				log(LOG_ERR, 
				       "state == ERROR...bailing scnt %d\n", scnt);
				return;
			}
		} while (ne == 0);

		if (ne < 0) {
			log(LOG_ERR, "poll CQ failed %d\n", ne);
			return;
		}
 		if (cb->poll && wc.status != IB_WC_SUCCESS) {
			log(LOG_ERR, "Completion wth error at %s:\n",
				cb->server ? "server" : "client");
			log(LOG_ERR, "Failed status %d: wr_id %d\n",
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

	log(LOG_ERR, "delta sec %zu delta usec %lu iter %d size %d\n",
		stop_tv.tv_sec - start_tv.tv_sec, 
		stop_tv.tv_usec - start_tv.tv_usec,
		scnt, cb->size);
}

static int alloc_cycle_mem(int cycle_iters,
				cycles_t **post_cycles_start,
				cycles_t **post_cycles_stop,
				cycles_t **poll_cycles_start,
				cycles_t **poll_cycles_stop,
				cycles_t **last_poll_cycles_start)
{
	*post_cycles_start = malloc(cycle_iters * sizeof(cycles_t), M_DEVBUF, M_WAITOK);
	if (!*post_cycles_start) {
		goto fail1;
	}
	*post_cycles_stop = malloc(cycle_iters * sizeof(cycles_t), M_DEVBUF, M_WAITOK);
	if (!*post_cycles_stop) {
		goto fail2;
	}
	*poll_cycles_start = malloc(cycle_iters * sizeof(cycles_t), M_DEVBUF, M_WAITOK);
	if (!*poll_cycles_start) {
		goto fail3;
	}
	*poll_cycles_stop = malloc(cycle_iters * sizeof(cycles_t), M_DEVBUF, M_WAITOK);
	if (!*poll_cycles_stop) {
		goto fail4;
	}
	*last_poll_cycles_start = malloc(cycle_iters * sizeof(cycles_t), M_DEVBUF, M_WAITOK);
	if (!*last_poll_cycles_start) {
		goto fail5;
	}
	return 0;
fail5:
	free(*poll_cycles_stop, M_DEVBUF);
fail4:
	free(*poll_cycles_start, M_DEVBUF);
fail3:
	free(*post_cycles_stop, M_DEVBUF);
fail2:
	free(*post_cycles_start, M_DEVBUF);
fail1:
	log(LOG_ERR, "%s malloc failed\n", __FUNCTION__);
	return ENOMEM;
}

static void free_cycle_mem(cycles_t *post_cycles_start,
				cycles_t *post_cycles_stop,
				cycles_t *poll_cycles_start,
				cycles_t *poll_cycles_stop,
				cycles_t *last_poll_cycles_start)
{
	free(last_poll_cycles_start, M_DEVBUF);
	free(poll_cycles_stop, M_DEVBUF);
	free(poll_cycles_start, M_DEVBUF);
	free(post_cycles_stop, M_DEVBUF);
	free(post_cycles_start, M_DEVBUF);
}

static void wlat_test(struct krping_cb *cb)
{
	int ccnt, scnt, rcnt;
	int iters=cb->count;
	volatile char *poll_buf = (char *) cb->start_buf;
	char *buf = (char *)cb->rdma_buf;
	ccnt = 0;
	scnt = 0;
	rcnt = 0;
	struct timeval start_tv, stop_tv;
	cycles_t *post_cycles_start, *post_cycles_stop;
	cycles_t *poll_cycles_start, *poll_cycles_stop;
	cycles_t *last_poll_cycles_start;
	cycles_t sum_poll = 0, sum_post = 0, sum_last_poll = 0;
	int i;
	int cycle_iters = 1000;
	int err;

	err = alloc_cycle_mem(cycle_iters, &post_cycles_start, &post_cycles_stop,
				&poll_cycles_start, &poll_cycles_stop, &last_poll_cycles_start);
			  
	if (err) {
		log(LOG_ERR, "%s malloc failed\n", __FUNCTION__);
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
					log(LOG_ERR, "state = ERROR, bailing\n");
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
				log(LOG_ERR,  "Couldn't post send: scnt=%d\n",
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
					last_poll_cycles_start[ccnt] = get_cycles();
				ne = ib_poll_cq(cb->cq, 1, &wc);
			} while (ne == 0);
			if (ccnt < cycle_iters)
				poll_cycles_stop[ccnt] = get_cycles();
			++ccnt;

			if (ne < 0) {
				log(LOG_ERR, "poll CQ failed %d\n", ne);
				return;
			}
			if (wc.status != IB_WC_SUCCESS) {
				log(LOG_ERR, "Completion wth error at %s:\n",
					cb->server ? "server" : "client");
				log(LOG_ERR, "Failed status %d: wr_id %d\n",
					wc.status, (int) wc.wr_id);
				log(LOG_ERR, "scnt=%d, rcnt=%d, ccnt=%d\n",
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
		sum_last_poll += poll_cycles_stop[i] - last_poll_cycles_start[i];
	}

	log(LOG_ERR, "delta sec %zu delta usec %lu iter %d size %d cycle_iters %d sum_post %llu sum_poll %llu sum_last_poll %llu\n",
		stop_tv.tv_sec - start_tv.tv_sec, 
		stop_tv.tv_usec - start_tv.tv_usec,
		scnt, cb->size, cycle_iters, 
		(unsigned long long)sum_post, (unsigned long long)sum_poll, 
		(unsigned long long)sum_last_poll);

	free_cycle_mem(post_cycles_start, post_cycles_stop, poll_cycles_start, 
			poll_cycles_stop, last_poll_cycles_start);
}

static void bw_test(struct krping_cb *cb)
{
	int ccnt, scnt, rcnt;
	int iters=cb->count;
	ccnt = 0;
	scnt = 0;
	rcnt = 0;
	struct timeval start_tv, stop_tv;
	cycles_t *post_cycles_start, *post_cycles_stop;
	cycles_t *poll_cycles_start, *poll_cycles_stop;
	cycles_t *last_poll_cycles_start;
	cycles_t sum_poll = 0, sum_post = 0, sum_last_poll = 0;
	int i;
	int cycle_iters = 1000;
	int err;

	err = alloc_cycle_mem(cycle_iters, &post_cycles_start, &post_cycles_stop,
				&poll_cycles_start, &poll_cycles_stop, &last_poll_cycles_start);
			  
	if (err) {
		log(LOG_ERR, "%s kmalloc failed\n", __FUNCTION__);
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
				log(LOG_ERR,  "Couldn't post send: scnt=%d\n",
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
					last_poll_cycles_start[ccnt] = get_cycles();
				ne = ib_poll_cq(cb->cq, 1, &wc);
			} while (ne == 0);
			if (ccnt < cycle_iters)
				poll_cycles_stop[ccnt] = get_cycles();
			ccnt += 1;

			if (ne < 0) {
				log(LOG_ERR, "poll CQ failed %d\n", ne);
				return;
			}
			if (wc.status != IB_WC_SUCCESS) {
				log(LOG_ERR, "Completion wth error at %s:\n",
					cb->server ? "server" : "client");
				log(LOG_ERR, "Failed status %d: wr_id %d\n",
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
		sum_last_poll += poll_cycles_stop[i] - last_poll_cycles_start[i];
	}

	log(LOG_ERR, "delta sec %zu delta usec %lu iter %d size %d cycle_iters %d sum_post %llu sum_poll %llu sum_last_poll %llu\n",
		stop_tv.tv_sec - start_tv.tv_sec, 
		stop_tv.tv_usec - start_tv.tv_usec,
		scnt, cb->size, cycle_iters, 
		(unsigned long long)sum_post, (unsigned long long)sum_poll, 
		(unsigned long long)sum_last_poll);

	free_cycle_mem(post_cycles_start, post_cycles_stop, poll_cycles_start, 
			poll_cycles_stop, last_poll_cycles_start);
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
	if (cb->dma_mr)
		krping_format_send(cb, cb->start_addr, cb->dma_mr);
	else
		krping_format_send(cb, cb->start_addr, cb->start_mr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		log(LOG_ERR, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		log(LOG_ERR, "send completiong error %d\n", wc.status);
		return;
	}

	krping_wait(cb, ERROR);
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
	if (cb->dma_mr)
		krping_format_send(cb, cb->start_addr, cb->dma_mr);
	else
		krping_format_send(cb, cb->start_addr, cb->start_mr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		log(LOG_ERR, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		log(LOG_ERR, "send completiong error %d\n", wc.status);
		return;
	}

	wlat_test(cb);

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
	if (cb->dma_mr)
		krping_format_send(cb, cb->start_addr, cb->dma_mr);
	else
		krping_format_send(cb, cb->start_addr, cb->start_mr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		log(LOG_ERR, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		log(LOG_ERR, "send completiong error %d\n", wc.status);
		return;
	}

	if (cb->duplex)
		bw_test(cb);
	krping_wait(cb, ERROR);
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
		log(LOG_ERR, "rdma_bind_addr error %d\n", ret);
		return ret;
	}
	DEBUG_LOG(PFX "rdma_bind_addr successful\n");

	DEBUG_LOG(PFX "rdma_listen\n");
	ret = rdma_listen(cb->cm_id, 3);
	if (ret) {
		log(LOG_ERR, "rdma_listen failed: %d\n", ret);
		return ret;
	}

	krping_wait(cb, CONNECT_REQUEST);
	if (cb->state != CONNECT_REQUEST) {
		log(LOG_ERR,  "wait for CONNECT_REQUEST state %d\n",
			cb->state);
		return -1;
	}

	return 0;
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
		log(LOG_ERR, "setup_qp failed: %d\n", ret);
		return;
	}

	ret = krping_setup_buffers(cb);
	if (ret) {
		log(LOG_ERR, "krping_setup_buffers failed: %d\n", ret);
		goto err1;
	}

	ret = ib_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "ib_post_recv failed: %d\n", ret);
		goto err2;
	}

	ret = krping_accept(cb);
	if (ret) {
		log(LOG_ERR, "connect error %d\n", ret);
		goto err2;
	}

	if (cb->wlat)
		krping_wlat_test_server(cb);
	else if (cb->rlat)
		krping_rlat_test_server(cb);
	else if (cb->bw)
		krping_bw_test_server(cb);
	else
		krping_test_server(cb);

	rdma_disconnect(cb->child_cm_id);
	rdma_destroy_id(cb->child_cm_id);
err2:
	krping_free_buffers(cb);
err1:
	krping_free_qp(cb);
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

		if (cb->dma_mr)
			krping_format_send(cb, cb->start_addr, cb->dma_mr);
		else
			krping_format_send(cb, cb->start_addr, cb->start_mr);

		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			log(LOG_ERR, "post send error %d\n", ret);
			break;
		}

		/* Wait for server to ACK */
		krping_wait(cb, RDMA_WRITE_ADV);
		if (cb->state != RDMA_WRITE_ADV) {
			log(LOG_ERR,  
			       "wait for RDMA_WRITE_ADV state %d\n",
			       cb->state);
			break;
		}

		if (cb->dma_mr)
			krping_format_send(cb, cb->rdma_addr, cb->dma_mr);
		else
			krping_format_send(cb, cb->rdma_addr, cb->rdma_mr);

		ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
		if (ret) {
			log(LOG_ERR, "post send error %d\n", ret);
			break;
		}

		/* Wait for the server to say the RDMA Write is complete. */
		krping_wait(cb, RDMA_WRITE_COMPLETE);
		if (cb->state != RDMA_WRITE_COMPLETE) {
			log(LOG_ERR,  
			       "wait for RDMA_WRITE_COMPLETE state %d\n",
			       cb->state);
			break;
		}

		if (cb->validate)
			if (memcmp(cb->start_buf, cb->rdma_buf, cb->size)) {
				log(LOG_ERR, "data mismatch!\n");
				break;
			}

		if (cb->verbose)
			DEBUG_LOG("ping data: %s\n", cb->rdma_buf);
	}
}

static void krping_rlat_test_client(struct krping_cb *cb)
{
	struct ib_send_wr *bad_wr;
	struct ib_wc wc;
	int ret;

	cb->state = RDMA_READ_ADV;

	/* Send STAG/TO/Len to client */
	if (cb->dma_mr)
		krping_format_send(cb, cb->start_addr, cb->dma_mr);
	else
		krping_format_send(cb, cb->start_addr, cb->rdma_mr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		log(LOG_ERR, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		log(LOG_ERR, "send completion error %d\n", wc.status);
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
			log(LOG_ERR,  "Couldn't post send\n");
			return;
		}
		do {
			ne = ib_poll_cq(cb->cq, 1, &wc);
		} while (ne == 0);
		if (ne < 0) {
			log(LOG_ERR, "poll CQ failed %d\n", ne);
			return;
		}
		if (wc.status != IB_WC_SUCCESS) {
			log(LOG_ERR, "Completion wth error at %s:\n",
				cb->server ? "server" : "client");
			log(LOG_ERR, "Failed status %d: wr_id %d\n",
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
	log(LOG_ERR, "0B-write-lat iters 100000 usec %llu\n", elapsed);
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
	if (cb->dma_mr)
		krping_format_send(cb, cb->start_addr, cb->dma_mr);
	else
		krping_format_send(cb, cb->start_addr, cb->start_mr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		log(LOG_ERR, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		log(LOG_ERR, "send completion error %d\n", wc.status);
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
	if (cb->dma_mr)
		krping_format_send(cb, cb->start_addr, cb->dma_mr);
	else
		krping_format_send(cb, cb->start_addr, cb->start_mr);
	ret = ib_post_send(cb->qp, &cb->sq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "post send error %d\n", ret);
		return;
	}

	/* Spin waiting for send completion */
	while ((ret = ib_poll_cq(cb->cq, 1, &wc) == 0));
	if (ret < 0) {
		log(LOG_ERR, "poll error %d\n", ret);
		return;
	}
	if (wc.status) {
		log(LOG_ERR, "send completion error %d\n", wc.status);
		return;
	}

	/* Spin waiting for server's Start STAG/TO/Len */
	while (cb->state < RDMA_WRITE_ADV) {
		krping_cq_event_handler(cb->cq, cb);
	}

	bw_test(cb);
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
		log(LOG_ERR, "rdma_connect error %d\n", ret);
		return ret;
	}

	krping_wait(cb, CONNECTED);
	if (cb->state == ERROR) {
		log(LOG_ERR,  "wait for CONNECTED state %d\n", cb->state);
		return -1;
	}

	DEBUG_LOG(PFX "rdma_connect successful\n");
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
		log(LOG_ERR, "rdma_resolve_addr error %d\n", ret);
		return ret;
	}

	krping_wait(cb, ROUTE_RESOLVED);
	if (cb->state != ROUTE_RESOLVED) {
		log(LOG_ERR,  
		       "addr/route resolution did not resolve: state %d\n",
		       cb->state);
		return EINTR;
	}

	DEBUG_LOG(PFX "rdma_resolve_addr - rdma_resolve_route successful\n");
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
		log(LOG_ERR, "setup_qp failed: %d\n", ret);
		return;
	}

	ret = krping_setup_buffers(cb);
	if (ret) {
		log(LOG_ERR, "krping_setup_buffers failed: %d\n", ret);
		goto err1;
	}

	ret = ib_post_recv(cb->qp, &cb->rq_wr, &bad_wr);
	if (ret) {
		log(LOG_ERR, "ib_post_recv failed: %d\n", ret);
		goto err2;
	}

	ret = krping_connect_client(cb);
	if (ret) {
		log(LOG_ERR, "connect error %d\n", ret);
		goto err2;
	}

	if (cb->wlat)
		krping_wlat_test_client(cb);
	else if (cb->rlat)
		krping_rlat_test_client(cb);
	else if (cb->bw)
		krping_bw_test_client(cb);
	else
		krping_test_client(cb);
	rdma_disconnect(cb->cm_id);
err2:
	krping_free_buffers(cb);
err1:
	krping_free_qp(cb);
}

int krping_doit(char *cmd)
{
	struct krping_cb *cb;
	int op;
	int ret = 0;
	char *optarg;
	unsigned long optint;
	debug = 0;

	cb = malloc(sizeof(*cb), M_DEVBUF, M_WAITOK);
	if (!cb)
		return ENOMEM;
	bzero(cb, sizeof *cb);

	mtx_lock(&krping_mutex);
	TAILQ_INSERT_TAIL(&krping_cbs, cb, list);
	mtx_unlock(&krping_mutex);

	cb->server = -1;
	cb->state = IDLE;
	cb->size = 64;
	cb->txdepth = RPING_SQ_DEPTH;
	mtx_init(&cb->lock, "krping mtx", NULL, MTX_DUPOK|MTX_DEF);

	while ((op = krping_getopt("krping", &cmd, krping_opts, NULL, &optarg,
			      &optint)) != 0) {
		switch (op) {
		case 'a':
			cb->addr_str = optarg;
			DEBUG_LOG(PFX "ipaddr (%s)\n", optarg);
			if (!inet_aton(optarg, &cb->addr)) {
				log(LOG_ERR, "bad addr string %s\n", optarg);
				ret = EINVAL;
			}
			break;
		case 'D':
			cb->use_dmamr = 1;
			DEBUG_LOG(PFX "using dma mr\n");
			break;
		case 'p':
			cb->port = htons(optint);
			DEBUG_LOG(PFX "port %d\n", (int)optint);
			break;
		case 'P':
			cb->poll = 1;
			DEBUG_LOG("server\n");
			break;
		case 's':
			cb->server = 1;
			DEBUG_LOG(PFX "server\n");
			break;
		case 'c':
			cb->server = 0;
			DEBUG_LOG(PFX "client\n");
			break;
		case 'S':
			cb->size = optint;
			if ((cb->size < 1) ||
			    (cb->size > RPING_BUFSIZE)) {
				log(LOG_ERR, "Invalid size %d "
				       "(valid range is 1 to %d)\n",
				       cb->size, RPING_BUFSIZE);
				ret = EINVAL;
			} else
				DEBUG_LOG(PFX "size %d\n", (int)optint);
			break;
		case 'C':
			cb->count = optint;
			if (cb->count < 0) {
				log(LOG_ERR, "Invalid count %d\n",
					cb->count);
				ret = EINVAL;
			} else
				DEBUG_LOG(PFX "count %d\n", (int) cb->count);
			break;
		case 'v':
			cb->verbose++;
			DEBUG_LOG(PFX "verbose\n");
			break;
		case 'V':
			cb->validate++;
			DEBUG_LOG(PFX "validate data\n");
			break;
		case 'L':
			cb->rlat++;
			break;
		case 'l':
			cb->wlat++;
			break;
		case 'B':
			cb->bw++;
			break;
		case 't':
			cb->txdepth = optint;
			DEBUG_LOG(PFX "txdepth %d\n", cb->txdepth);
			break;
		case 'd':
			debug++;
			break;
		default:
			log(LOG_ERR, "unknown opt %s\n", optarg);
			ret = EINVAL;
			break;
		}
	}
	if (ret)
		goto out;

	if (cb->server == -1) {
		log(LOG_ERR, "must be either client or server\n");
		ret = EINVAL;
		goto out;
	}
	if ((cb->bw + cb->rlat + cb->wlat) > 1) {
		log(LOG_ERR, "Pick only one test: bw, rlat, wlat\n");
		ret = EINVAL;
		goto out;
	}


	cb->cm_id = rdma_create_id(krping_cma_event_handler, cb, RDMA_PS_TCP);
	if (IS_ERR(cb->cm_id)) {
		ret = PTR_ERR(cb->cm_id);
		log(LOG_ERR, "rdma_create_id error %d\n", ret);
		goto out;
	}
	DEBUG_LOG(PFX "created cm_id %p\n", cb->cm_id);
	if (cb->server)
		krping_run_server(cb);
	else
		krping_run_client(cb);
	DEBUG_LOG(PFX "destroy cm_id %p\n", cb->cm_id);
	rdma_destroy_id(cb->cm_id);
out:
	mtx_lock(&krping_mutex);
	TAILQ_REMOVE(&krping_cbs, cb, list);
	mtx_unlock(&krping_mutex);
	free(cb, M_DEVBUF);
	return ret;
}

void krping_init(void)
{
	mtx_init(&krping_mutex, "krping lock", NULL, MTX_DEF);
	TAILQ_INIT(&krping_cbs);
}
