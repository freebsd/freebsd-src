/*
 * Copyright (c) 2009 Intel Corporation.  All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a
 *    copy of which is available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_evd_util.h"
#include "dapl_cr_util.h"
#include "dapl_name_service.h"
#include "dapl_ib_util.h"
#include "dapl_osd.h"


#if defined(_WIN32)
#include <rdma\winverbs.h>
#else				// _WIN32
enum DAPL_FD_EVENTS {
	DAPL_FD_READ = POLLIN,
	DAPL_FD_WRITE = POLLOUT,
	DAPL_FD_ERROR = POLLERR
};

struct dapl_fd_set {
	int index;
	struct pollfd set[DAPL_FD_SETSIZE];
};

static struct dapl_fd_set *dapl_alloc_fd_set(void)
{
	return dapl_os_alloc(sizeof(struct dapl_fd_set));
}

static void dapl_fd_zero(struct dapl_fd_set *set)
{
	set->index = 0;
}

static int dapl_fd_set(DAPL_SOCKET s, struct dapl_fd_set *set,
		       enum DAPL_FD_EVENTS event)
{
	if (set->index == DAPL_FD_SETSIZE - 1) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 "SCM ERR: cm_thread exceeded FD_SETSIZE %d\n",
			 set->index + 1);
		return -1;
	}

	set->set[set->index].fd = s;
	set->set[set->index].revents = 0;
	set->set[set->index++].events = event;
	return 0;
}

static enum DAPL_FD_EVENTS dapl_poll(DAPL_SOCKET s, enum DAPL_FD_EVENTS event)
{
	struct pollfd fds;
	int ret;

	fds.fd = s;
	fds.events = event;
	fds.revents = 0;
	ret = poll(&fds, 1, 0);
	dapl_log(DAPL_DBG_TYPE_CM, " dapl_poll: fd=%d ret=%d, evnts=0x%x\n",
		 s, ret, fds.revents);
	if (ret == 0)
		return 0;
	else if (fds.revents & (POLLERR | POLLHUP | POLLNVAL)) 
		return DAPL_FD_ERROR;
	else 
		return fds.revents;
}

static int dapl_select(struct dapl_fd_set *set, int time_ms)
{
	int ret;

	dapl_dbg_log(DAPL_DBG_TYPE_CM, " dapl_select: sleep, fds=%d\n",
		     set->index);
	ret = poll(set->set, set->index, time_ms);
	dapl_dbg_log(DAPL_DBG_TYPE_CM, " dapl_select: wakeup, ret=0x%x\n", ret);
	return ret;
}
#endif

/* forward declarations */
static int ucm_reply(dp_ib_cm_handle_t cm);
static void ucm_accept(ib_cm_srvc_handle_t cm, ib_cm_msg_t *msg);
static void ucm_connect_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg);
static void ucm_accept_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg);
static int ucm_send(ib_hca_transport_t *tp, ib_cm_msg_t *msg, DAT_PVOID p_data, DAT_COUNT p_size);
static void ucm_disconnect_final(dp_ib_cm_handle_t cm);
DAT_RETURN dapli_cm_disconnect(dp_ib_cm_handle_t cm);
DAT_RETURN dapli_cm_connect(DAPL_EP *ep, dp_ib_cm_handle_t cm);

#define UCM_SND_BURST	50	

/* Service ids - port space */
static uint16_t ucm_get_port(ib_hca_transport_t *tp, uint16_t port)
{
	int i = 0;
	
	dapl_os_lock(&tp->plock);
	/* get specific ID */
	if (port) {
		if (tp->sid[port] == 0) {
			tp->sid[port] = 1;
			i = port;
		}
		goto done;
	} 
	
	/* get any free ID */
	for (i = 0xffff; i > 0; i--) {
		if (tp->sid[i] == 0) {
			tp->sid[i] = 1;
			break;
		}
	}
done:
	dapl_os_unlock(&tp->plock);
	return i;
}

static void ucm_free_port(ib_hca_transport_t *tp, uint16_t port)
{
	dapl_os_lock(&tp->plock);
	tp->sid[port] = 0;
	dapl_os_unlock(&tp->plock);
}

static void ucm_check_timers(dp_ib_cm_handle_t cm, int *timer)
{
	DAPL_OS_TIMEVAL time;

        dapl_os_lock(&cm->lock);
	dapl_os_get_time(&time); 
	switch (cm->state) {
	case DCM_REP_PENDING: 
		*timer = cm->hca->ib_trans.cm_timer; 
		/* wait longer each retry */
		if ((time - cm->timer)/1000 > 
		    (cm->hca->ib_trans.rep_time << cm->retries)) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				 " CM_REQ retry %d [lid, port, qpn]:"
				 " %x %x %x -> %x %x %x Time(ms) %llu > %llu\n", 
				 cm->retries, ntohs(cm->msg.saddr.ib.lid), 
				 ntohs(cm->msg.sport), ntohl(cm->msg.saddr.ib.qpn), 
				 ntohs(cm->msg.daddr.ib.lid), ntohs(cm->msg.dport),
				 ntohl(cm->msg.dqpn), (time - cm->timer)/1000, 
				 cm->hca->ib_trans.rep_time << cm->retries);
			cm->retries++;
			dapl_os_unlock(&cm->lock);
			dapli_cm_connect(cm->ep, cm);
			return;
		}
		break;
	case DCM_RTU_PENDING: 
		*timer = cm->hca->ib_trans.cm_timer;  
		if ((time - cm->timer)/1000 > 
		    (cm->hca->ib_trans.rtu_time << cm->retries)) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				 " CM_REPLY retry %d [lid, port, qpn]:"
				 " %x %x %x -> %x %x %x r_pid %x,%d Time(ms) %llu > %llu\n", 
				 cm->retries,
				 ntohs(cm->msg.saddr.ib.lid), 
				 ntohs(cm->msg.sport),
				 ntohl(cm->msg.saddr.ib.qpn), 
				 ntohs(cm->msg.daddr.ib.lid), 
				 ntohs(cm->msg.dport),
				 ntohl(cm->msg.daddr.ib.qpn),  
				 ntohl(*(DAT_UINT32*)cm->msg.resv),
				 ntohl(*(DAT_UINT32*)cm->msg.resv), 
				 (time - cm->timer)/1000, cm->hca->ib_trans.rtu_time << cm->retries);
			cm->retries++;
			dapl_os_unlock(&cm->lock);
			ucm_reply(cm);
			return;
		}
		break;
	case DCM_DISC_PENDING: 
		*timer = cm->hca->ib_trans.cm_timer; 
		/* wait longer each retry */
		if ((time - cm->timer)/1000 > 
		    (cm->hca->ib_trans.rtu_time << cm->retries)) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				 " CM_DREQ retry %d [lid, port, qpn]:"
				 " %x %x %x -> %x %x %x r_pid %x,%d Time(ms) %llu > %llu\n", 
				 cm->retries,
				 ntohs(cm->msg.saddr.ib.lid), 
				 ntohs(cm->msg.sport),
				 ntohl(cm->msg.saddr.ib.qpn), 
				 ntohs(cm->msg.daddr.ib.lid), 
				 ntohs(cm->msg.dport),
				 ntohl(cm->msg.dqpn), 
				 ntohl(*(DAT_UINT32*)cm->msg.resv),
				 ntohl(*(DAT_UINT32*)cm->msg.resv), 
				 (time - cm->timer)/1000, cm->hca->ib_trans.rtu_time << cm->retries);
			cm->retries++;
			dapl_os_unlock(&cm->lock);
			dapli_cm_disconnect(cm);
                        return;
		}
		break;
	default:
		break;
	}
	dapl_os_unlock(&cm->lock);
}

/* SEND CM MESSAGE PROCESSING */

/* Get CM UD message from send queue, called with s_lock held */
static ib_cm_msg_t *ucm_get_smsg(ib_hca_transport_t *tp)
{
	ib_cm_msg_t *msg = NULL; 
	int ret, polled = 0, hd = tp->s_hd;

	hd++;
retry:
	if (hd == tp->qpe)
		hd = 0;

	if (hd == tp->s_tl)
		msg = NULL;
	else {
		msg = &tp->sbuf[hd];
		tp->s_hd = hd; /* new hd */
	}

	/* if empty, process some completions */
	if ((msg == NULL) && (!polled)) {
		struct ibv_wc wc;

		/* process completions, based on UCM_SND_BURST */
		ret = ibv_poll_cq(tp->scq, 1, &wc);
		if (ret < 0) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				" get_smsg: cq %p %s\n", 
				tp->scq, strerror(errno));
		}
		/* free up completed sends, update tail */
		if (ret > 0) {
			tp->s_tl = (int)wc.wr_id;
			dapl_log(DAPL_DBG_TYPE_CM,
				" get_smsg: wr_cmp (%d) s_tl=%d\n", 
				wc.status, tp->s_tl);
		}
		polled++;
		goto retry;
	}
	return msg;
}

/* RECEIVE CM MESSAGE PROCESSING */

static int ucm_post_rmsg(ib_hca_transport_t *tp, ib_cm_msg_t *msg)
{	
	struct ibv_recv_wr recv_wr, *recv_err;
	struct ibv_sge sge;
        
	recv_wr.next = NULL;
	recv_wr.sg_list = &sge;
	recv_wr.num_sge = 1;
	recv_wr.wr_id = (uint64_t)(uintptr_t) msg;
	sge.length = sizeof(ib_cm_msg_t) + sizeof(struct ibv_grh);
	sge.lkey = tp->mr_rbuf->lkey;
	sge.addr = (uintptr_t)((char *)msg - sizeof(struct ibv_grh));
	
	return (ibv_post_recv(tp->qp, &recv_wr, &recv_err));
}

static int ucm_reject(ib_hca_transport_t *tp, ib_cm_msg_t *msg)
{
	ib_cm_msg_t	smsg;

	/* setup op, rearrange the src, dst cm and addr info */
	(void)dapl_os_memzero(&smsg, sizeof(smsg));
	smsg.ver = htons(DCM_VER);
	smsg.op = htons(DCM_REJ_CM);
	smsg.dport = msg->sport;
	smsg.dqpn = msg->sqpn;
	smsg.sport = msg->dport; 
	smsg.sqpn = msg->dqpn;

	dapl_os_memcpy(&smsg.daddr, &msg->saddr, sizeof(union dcm_addr));
	
	/* no dst_addr IB info in REQ, init lid, gid, get type from saddr */
	smsg.saddr.ib.lid = tp->addr.ib.lid; 
	smsg.saddr.ib.qp_type = msg->saddr.ib.qp_type;
	dapl_os_memcpy(&smsg.saddr.ib.gid[0],
		       &tp->addr.ib.gid, 16); 

	dapl_os_memcpy(&smsg.saddr, &msg->daddr, sizeof(union dcm_addr));

	dapl_dbg_log(DAPL_DBG_TYPE_CM, 
		     " CM reject -> LID %x, QPN %x PORT %d\n", 
		     ntohs(smsg.daddr.ib.lid),
		     ntohl(smsg.dqpn), ntohs(smsg.dport));

	return (ucm_send(tp, &smsg, NULL, 0));
}

static void ucm_process_recv(ib_hca_transport_t *tp, 
			     ib_cm_msg_t *msg, 
			     dp_ib_cm_handle_t cm)
{
	dapl_os_lock(&cm->lock);
	switch (cm->state) {
	case DCM_LISTEN: /* passive */
		dapl_os_unlock(&cm->lock);
		ucm_accept(cm, msg);
		break;
	case DCM_RTU_PENDING: /* passive */
		dapl_os_unlock(&cm->lock);
		ucm_accept_rtu(cm, msg);
		break;
	case DCM_REP_PENDING: /* active */
		dapl_os_unlock(&cm->lock);
		ucm_connect_rtu(cm, msg);
		break;
	case DCM_CONNECTED: /* active and passive */
		/* DREQ, change state and process */
		if (ntohs(msg->op) == DCM_DREQ) {
			cm->state = DCM_DISC_RECV;
			dapl_os_unlock(&cm->lock);
			dapli_cm_disconnect(cm);
			break;
		} 
		/* active: RTU was dropped, resend */
		if (ntohs(msg->op) == DCM_REP) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				" RESEND RTU: op %s st %s [lid, port, qpn]:"
				" 0x%x %d 0x%x -> 0x%x %d 0x%x\n", 
				dapl_cm_op_str(ntohs(msg->op)), 
				dapl_cm_state_str(cm->state),
				ntohs(msg->saddr.ib.lid), 
				ntohs(msg->sport),
				ntohl(msg->saddr.ib.qpn), 
				ntohs(msg->daddr.ib.lid), 
				ntohs(msg->dport),
				ntohl(msg->daddr.ib.qpn));  

			cm->msg.op = htons(DCM_RTU);
			ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0); 		
		}
		dapl_os_unlock(&cm->lock);
		break;
	case DCM_DISC_PENDING: /* active and passive */
		/* DREQ or DREP, finalize */
		dapl_os_unlock(&cm->lock);
		ucm_disconnect_final(cm);
		break;
	case DCM_DISCONNECTED:
	case DCM_DESTROY:
		/* DREQ dropped, resend */
		if (ntohs(msg->op) == DCM_DREQ) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				" RESEND DREP: op %s st %s [lid, port, qpn]:"
				" 0x%x %d 0x%x -> 0x%x %d 0x%x\n", 
				dapl_cm_op_str(ntohs(msg->op)), 
				dapl_cm_state_str(cm->state),
				ntohs(msg->saddr.ib.lid), 
				ntohs(msg->sport),
				ntohl(msg->saddr.ib.qpn), 
				ntohs(msg->daddr.ib.lid), 
				ntohs(msg->dport),
				ntohl(msg->daddr.ib.qpn));  
			cm->msg.op = htons(DCM_DREP);
			ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0); 
			
		}
		dapl_os_unlock(&cm->lock);
		break;
	case DCM_RELEASED:
		/* UD reply retried, ignore */
		if (ntohs(msg->op) != DCM_REP) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				" ucm_recv: UNKNOWN operation"
				" <- op %d, %s spsp %d sqpn %d\n", 
				ntohs(msg->op), dapl_cm_state_str(cm->state),
				ntohs(msg->sport), ntohl(msg->sqpn));
		}
		dapl_os_unlock(&cm->lock);
		break;
	default:
		dapl_log(DAPL_DBG_TYPE_WARN,
				" ucm_recv: UNKNOWN state"
				" <- op %d, %s spsp %d sqpn %d\n", 
				ntohs(msg->op), dapl_cm_state_str(cm->state), 
				ntohs(msg->sport), ntohl(msg->sqpn));
		dapl_os_unlock(&cm->lock);
		break;
	}
}

/* Find matching CM object for this receive message, return CM reference, timer */
dp_ib_cm_handle_t ucm_cm_find(ib_hca_transport_t *tp, ib_cm_msg_t *msg)
{
	dp_ib_cm_handle_t cm, next, found = NULL;
	struct dapl_llist_entry	**list;
	DAPL_OS_LOCK *lock;
	int listenq = 0;

	/* conn list first, duplicate requests for DCM_REQ */
	list = &tp->list;
	lock = &tp->lock;

retry_listenq:
	dapl_os_lock(lock);
        if (!dapl_llist_is_empty(list))
		next = dapl_llist_peek_head(list);
	else
		next = NULL;

	while (next) {
		cm = next;
		next = dapl_llist_next_entry(list,
					     (DAPL_LLIST_ENTRY *)&cm->entry);
		if (cm->state == DCM_DESTROY)
			continue;
		
		/* CM sPORT + QPN, match is good enough for listenq */
		if (listenq && 
		    cm->msg.sport == msg->dport && 
		    cm->msg.sqpn == msg->dqpn) {
			found = cm;
			break;
		}	 
		/* connectq, check src and dst, check duplicate conn_reqs */
		if (!listenq && 
		    cm->msg.sport == msg->dport && cm->msg.sqpn == msg->dqpn && 
		    cm->msg.dport == msg->sport && cm->msg.dqpn == msg->sqpn && 
		    cm->msg.daddr.ib.lid == msg->saddr.ib.lid) {
			if (ntohs(msg->op) != DCM_REQ) {
				found = cm;
				break; 
			} else {
				/* duplicate; bail and throw away */
				dapl_os_unlock(lock);
				dapl_log(DAPL_DBG_TYPE_WARN,
					 " DUPLICATE: op %s st %s [lid, port, qpn]:"
					 " 0x%x %d 0x%x <- 0x%x %d 0x%x\n", 
					 dapl_cm_op_str(ntohs(msg->op)), 
					 dapl_cm_state_str(cm->state),
					 ntohs(msg->daddr.ib.lid), 
					 ntohs(msg->dport),
					 ntohl(msg->daddr.ib.qpn), 
					 ntohs(msg->saddr.ib.lid), 
					 ntohs(msg->sport),
					 ntohl(msg->saddr.ib.qpn));  
				return NULL;
			}
		}
	}
	dapl_os_unlock(lock);

	/* no duplicate request on connq, check listenq for new request */
	if (ntohs(msg->op) == DCM_REQ && !listenq && !found) {
		listenq = 1;
		list = &tp->llist;
		lock = &tp->llock;
		goto retry_listenq;
	}

	/* not match on listenq for valid request, send reject */
	if (ntohs(msg->op) == DCM_REQ && !found)
		ucm_reject(tp, msg);

	if (!found) {
		dapl_log(DAPL_DBG_TYPE_WARN,
			" ucm_recv: NO MATCH op %s 0x%x %d i0x%x c0x%x"
			" < 0x%x %d 0x%x\n", 
			dapl_cm_op_str(ntohs(msg->op)), 
			ntohs(msg->daddr.ib.lid), ntohs(msg->dport), 
			ntohl(msg->daddr.ib.qpn), ntohl(msg->sqpn),
			ntohs(msg->saddr.ib.lid), ntohs(msg->sport), 
			ntohl(msg->saddr.ib.qpn));
	}

	return found;
}

/* Get rmsgs from CM completion queue, 10 at a time */
static void ucm_recv(ib_hca_transport_t *tp)
{
	struct ibv_wc wc[10];
	ib_cm_msg_t *msg;
	dp_ib_cm_handle_t cm;
	int i, ret, notify = 0;
	struct ibv_cq *ibv_cq = NULL;
	DAPL_HCA *hca;

	/* POLLIN on channel FD */
	ret = ibv_get_cq_event(tp->rch, &ibv_cq, (void *)&hca);
	if (ret == 0) {
		ibv_ack_cq_events(ibv_cq, 1);
	}
retry:	
	ret = ibv_poll_cq(tp->rcq, 10, wc);
	if (ret <= 0) {
		if (!ret && !notify) {
			ibv_req_notify_cq(tp->rcq, 0);
			notify = 1;
			goto retry;
		}
		return;
	} else 
		notify = 0;
	
	for (i = 0; i < ret; i++) {
		msg = (ib_cm_msg_t*) (uintptr_t) wc[i].wr_id;

		dapl_dbg_log(DAPL_DBG_TYPE_CM, 
			     " ucm_recv: wc status=%d, ln=%d id=%p sqp=%x\n", 
			     wc[i].status, wc[i].byte_len, 
			     (void*)wc[i].wr_id, wc[i].src_qp);

		/* validate CM message, version */
		if (ntohs(msg->ver) != DCM_VER) {
			dapl_log(DAPL_DBG_TYPE_WARN,
				 " ucm_recv: UNKNOWN msg %p, ver %d\n", 
				 msg, msg->ver);
			ucm_post_rmsg(tp, msg);
			continue;
		}
		if (!(cm = ucm_cm_find(tp, msg))) {
			ucm_post_rmsg(tp, msg);
			continue;
		}
		
		/* match, process it */
		ucm_process_recv(tp, msg, cm);
		ucm_post_rmsg(tp, msg);
	}
	
	/* finished this batch of WC's, poll and rearm */
	goto retry;
}

/* ACTIVE/PASSIVE: build and send CM message out of CM object */
static int ucm_send(ib_hca_transport_t *tp, ib_cm_msg_t *msg, DAT_PVOID p_data, DAT_COUNT p_size)
{
	ib_cm_msg_t *smsg = NULL;
	struct ibv_send_wr wr, *bad_wr;
	struct ibv_sge sge;
	int len, ret = -1;
	uint16_t dlid = ntohs(msg->daddr.ib.lid);

	/* Get message from send queue, copy data, and send */
	dapl_os_lock(&tp->slock);
	if ((smsg = ucm_get_smsg(tp)) == NULL)
		goto bail;

	len = (sizeof(*msg) - DCM_MAX_PDATA_SIZE);
	dapl_os_memcpy(smsg, msg, len);
	if (p_size) {
		smsg->p_size = ntohs(p_size);
		dapl_os_memcpy(&smsg->p_data, p_data, p_size);
	}

	wr.next = NULL;
        wr.sg_list = &sge;
        wr.num_sge = 1;
        wr.opcode = IBV_WR_SEND;
        wr.wr_id = (unsigned long)tp->s_hd;
	wr.send_flags = (wr.wr_id % UCM_SND_BURST) ? 0 : IBV_SEND_SIGNALED;
	if (len <= tp->max_inline_send)
		wr.send_flags |= IBV_SEND_INLINE; 

        sge.length = len + p_size;
        sge.lkey = tp->mr_sbuf->lkey;
        sge.addr = (uintptr_t)smsg;

	dapl_dbg_log(DAPL_DBG_TYPE_CM, 
		" ucm_send: op %d ln %d lid %x c_qpn %x rport %d\n", 
		ntohs(smsg->op), sge.length, htons(smsg->daddr.ib.lid), 
		htonl(smsg->dqpn), htons(smsg->dport));

	/* empty slot, then create AH */
	if (!tp->ah[dlid]) {
		tp->ah[dlid] = 	
			dapls_create_ah(tp->hca, tp->pd, tp->qp, 
					htons(dlid), NULL);
		if (!tp->ah[dlid])
			goto bail;
	}
		
	wr.wr.ud.ah = tp->ah[dlid];
	wr.wr.ud.remote_qpn = ntohl(smsg->dqpn);
	wr.wr.ud.remote_qkey = DAT_UD_QKEY;

	ret = ibv_post_send(tp->qp, &wr, &bad_wr);
bail:
	dapl_os_unlock(&tp->slock);	
	return ret;
}

/* ACTIVE/PASSIVE: CM objects */
dp_ib_cm_handle_t dapls_ib_cm_create(DAPL_EP *ep)
{
	dp_ib_cm_handle_t cm;

	/* Allocate CM, init lock, and initialize */
	if ((cm = dapl_os_alloc(sizeof(*cm))) == NULL)
		return NULL;

	(void)dapl_os_memzero(cm, sizeof(*cm));
	if (dapl_os_lock_init(&cm->lock))
		goto bail;

	cm->msg.ver = htons(DCM_VER);
	*(DAT_UINT32*)cm->msg.resv = htonl(dapl_os_getpid()); /* exchange PID for debugging */
	
	/* ACTIVE: init source address QP info from local EP */
	if (ep) {
		DAPL_HCA *hca = ep->header.owner_ia->hca_ptr;

		cm->msg.sport = htons(ucm_get_port(&hca->ib_trans, 0));
		if (!cm->msg.sport) 
			goto bail;

		/* IB info in network order */
		cm->ep = ep;
		cm->hca = hca;
		cm->msg.sqpn = htonl(hca->ib_trans.qp->qp_num); /* ucm */
		cm->msg.saddr.ib.qpn = htonl(ep->qp_handle->qp_num); /* ep */
		cm->msg.saddr.ib.qp_type = ep->qp_handle->qp_type;
                cm->msg.saddr.ib.lid = hca->ib_trans.addr.ib.lid; 
		dapl_os_memcpy(&cm->msg.saddr.ib.gid[0], 
			       &hca->ib_trans.addr.ib.gid, 16);
        }
	return cm;
bail:
	dapl_os_free(cm, sizeof(*cm));
	return NULL;
}

/* 
 * UD CR objects are kept active because of direct private data references
 * from CONN events. The cr->socket is closed and marked inactive but the 
 * object remains allocated and queued on the CR resource list. There can
 * be multiple CR's associated with a given EP. There is no way to determine 
 * when consumer is finished with event until the dat_ep_free.
 *
 * Schedule destruction for all CR's associated with this EP, cr_thread will
 * complete the cleanup with state == DCM_DESTROY. 
 */ 
static void ucm_ud_free(DAPL_EP *ep)
{
	DAPL_IA *ia = ep->header.owner_ia;
	DAPL_HCA *hca = NULL;
	ib_hca_transport_t *tp = &ia->hca_ptr->ib_trans;
	dp_ib_cm_handle_t cm, next;

	dapl_os_lock(&tp->lock);
	if (!dapl_llist_is_empty((DAPL_LLIST_HEAD*)&tp->list))
            next = dapl_llist_peek_head((DAPL_LLIST_HEAD*)&tp->list);
	else
	    next = NULL;

	while (next) {
		cm = next;
		next = dapl_llist_next_entry((DAPL_LLIST_HEAD*)&tp->list,
					     (DAPL_LLIST_ENTRY*)&cm->entry);
		if (cm->ep == ep)  {
			dapl_dbg_log(DAPL_DBG_TYPE_EP,
				     " qp_free CM: ep %p cm %p\n", ep, cm);
			dapl_os_lock(&cm->lock);
			hca = cm->hca;
			cm->ep = NULL;
			if (cm->ah) {
				ibv_destroy_ah(cm->ah);
				cm->ah = NULL;
			}
			cm->state = DCM_DESTROY;
			dapl_os_unlock(&cm->lock);
		}
	}
	dapl_os_unlock(&tp->lock);

	/* wakeup work thread if necessary */
	if (hca)
		dapls_thread_signal(&tp->signal);
}

/* mark for destroy, remove all references, schedule cleanup */
/* cm_ptr == NULL (UD), then multi CR's, kill all associated with EP */
void dapls_ib_cm_free(dp_ib_cm_handle_t cm, DAPL_EP *ep)
{
	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " cm_destroy: %s cm %p ep %p\n",
		     cm ? dapl_cm_state_str(cm->state) : "", cm, ep);

	if (!cm && ep) {
		ucm_ud_free(ep);
		return;
	}

	dapl_os_lock(&cm->lock);

	/* client, release local conn id port */
	if (!cm->sp && cm->msg.sport)
		ucm_free_port(&cm->hca->ib_trans, cm->msg.sport);

	/* cleanup, never made it to work queue */
	if (cm->state == DCM_INIT) {
		dapl_os_unlock(&cm->lock);
		dapl_os_free(cm, sizeof(*cm));
		return;
	}

	/* free could be called before disconnect, disc_clean will destroy */
	if (cm->state == DCM_CONNECTED) {
		dapl_os_unlock(&cm->lock);
		dapli_cm_disconnect(cm);
		return;
	}

	cm->state = DCM_DESTROY;
	if ((cm->ep) && (cm->ep->cm_handle == cm)) {
		cm->ep->cm_handle = IB_INVALID_HANDLE;
		cm->ep = NULL;
	}

	dapl_os_unlock(&cm->lock);

	/* wakeup work thread */
	dapls_thread_signal(&cm->hca->ib_trans.signal);
}

/* ACTIVE/PASSIVE: queue up connection object on CM list */
static void ucm_queue_conn(dp_ib_cm_handle_t cm)
{
	/* add to work queue, list, for cm thread processing */
	dapl_llist_init_entry((DAPL_LLIST_ENTRY *)&cm->entry);
	dapl_os_lock(&cm->hca->ib_trans.lock);
	dapl_llist_add_tail(&cm->hca->ib_trans.list,
			    (DAPL_LLIST_ENTRY *)&cm->entry, cm);
	dapl_os_unlock(&cm->hca->ib_trans.lock);
	dapls_thread_signal(&cm->hca->ib_trans.signal);
}

/* PASSIVE: queue up listen object on listen list */
static void ucm_queue_listen(dp_ib_cm_handle_t cm)
{
	/* add to work queue, llist, for cm thread processing */
	dapl_llist_init_entry((DAPL_LLIST_ENTRY *)&cm->entry);
	dapl_os_lock(&cm->hca->ib_trans.llock);
	dapl_llist_add_tail(&cm->hca->ib_trans.llist,
			    (DAPL_LLIST_ENTRY *)&cm->entry, cm);
	dapl_os_unlock(&cm->hca->ib_trans.llock);
}

static void ucm_dequeue_listen(dp_ib_cm_handle_t cm) {
	dapl_os_lock(&cm->hca->ib_trans.llock);
	dapl_llist_remove_entry(&cm->hca->ib_trans.llist, 
				(DAPL_LLIST_ENTRY *)&cm->entry);
	dapl_os_unlock(&cm->hca->ib_trans.llock);
}

static void ucm_disconnect_final(dp_ib_cm_handle_t cm) 
{
	/* no EP attachment or not RC, nothing to process */
	if (cm->ep == NULL ||
	    cm->ep->param.ep_attr.service_type != DAT_SERVICE_TYPE_RC) 
		return;

	dapl_os_lock(&cm->lock);
	if (cm->state == DCM_DISCONNECTED) {
		dapl_os_unlock(&cm->lock);
		return;
	}
		
	cm->state = DCM_DISCONNECTED;
	dapl_os_unlock(&cm->lock);

	if (cm->sp) 
		dapls_cr_callback(cm, IB_CME_DISCONNECTED, NULL, cm->sp);
	else
		dapl_evd_connection_callback(cm, IB_CME_DISCONNECTED, NULL, cm->ep);
}

/*
 * called from consumer thread via ep_disconnect/ep_free or 
 * from cm_thread when receiving DREQ
 */
DAT_RETURN dapli_cm_disconnect(dp_ib_cm_handle_t cm)
{
	int finalize = 1;

	dapl_os_lock(&cm->lock);
	switch (cm->state) {
	case DCM_CONNECTED:
		/* CONSUMER: move to err state to flush, if not UD */
		if (cm->ep->qp_handle->qp_type != IBV_QPT_UD) 
			dapls_modify_qp_state(cm->ep->qp_handle, IBV_QPS_ERR,0,0,0);

		/* send DREQ, event after DREP or DREQ timeout */
		cm->state = DCM_DISC_PENDING;
		cm->msg.op = htons(DCM_DREQ);
		finalize = 0; /* wait for DREP, wakeup timer thread */
		dapls_thread_signal(&cm->hca->ib_trans.signal);
		break;
	case DCM_DISC_PENDING:
		/* DREQ timeout, resend until retries exhausted */
		cm->msg.op = htons(DCM_DREQ);
		if (cm->retries >= cm->hca->ib_trans.retries) {
			dapl_log(DAPL_DBG_TYPE_ERR, 
				" CM_DREQ: RETRIES EXHAUSTED:"
				" 0x%x %d 0x%x -> 0x%x %d 0x%x\n",
				htons(cm->msg.saddr.ib.lid), 
				htonl(cm->msg.saddr.ib.qpn), 
				htons(cm->msg.sport), 
				htons(cm->msg.daddr.ib.lid), 
				htonl(cm->msg.dqpn), 
				htons(cm->msg.dport));
			finalize = 1;
		}
		break;
	case DCM_DISC_RECV:
		/* CM_THREAD: move to err state to flush, if not UD */
		if (cm->ep->qp_handle->qp_type != IBV_QPT_UD) 
			dapls_modify_qp_state(cm->ep->qp_handle, IBV_QPS_ERR,0,0,0);

		/* DREQ received, send DREP and schedule event */
		cm->msg.op = htons(DCM_DREP);
		break;
	default:
		dapl_os_unlock(&cm->lock);
		return DAT_SUCCESS;
	}
	
	dapl_os_get_time(&cm->timer); /* reply expected */
	ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0); 
	dapl_os_unlock(&cm->lock);

	if (finalize)
		ucm_disconnect_final(cm);
	
	return DAT_SUCCESS;
}

/*
 * ACTIVE: get remote CM SID server info from r_addr. 
 *         send, or resend CM msg via UD CM QP 
 */
DAT_RETURN
dapli_cm_connect(DAPL_EP *ep, dp_ib_cm_handle_t cm)
{
	dapl_log(DAPL_DBG_TYPE_EP, 
		 " connect: lid %x i_qpn %x lport %d p_sz=%d -> "
		 " lid %x c_qpn %x rport %d\n",
		 htons(cm->msg.saddr.ib.lid), htonl(cm->msg.saddr.ib.qpn),
		 htons(cm->msg.sport), htons(cm->msg.p_size),
		 htons(cm->msg.daddr.ib.lid), htonl(cm->msg.dqpn),
		 htons(cm->msg.dport));

	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_REP_PENDING) {
		dapl_os_unlock(&cm->lock);
		return DAT_INVALID_STATE;
	}
	
	if (cm->retries == cm->hca->ib_trans.retries) {
		dapl_log(DAPL_DBG_TYPE_ERR, 
			" CM_REQ: RETRIES EXHAUSTED:"
			 " 0x%x %d 0x%x -> 0x%x %d 0x%x\n",
			 htons(cm->msg.saddr.ib.lid), 
			 htonl(cm->msg.saddr.ib.qpn), 
			 htons(cm->msg.sport), 
			 htons(cm->msg.daddr.ib.lid), 
			 htonl(cm->msg.dqpn), 
			 htons(cm->msg.dport));

		/* update ep->cm reference so we get cleaned up on callback */
		if (cm->msg.saddr.ib.qp_type == IBV_QPT_RC);
			ep->cm_handle = cm;

		dapl_os_unlock(&cm->lock);

#ifdef DAPL_COUNTERS
		/* called from check_timers in cm_thread, cm lock held */
		if (g_dapl_dbg_type & DAPL_DBG_TYPE_CM_LIST) {
			dapl_os_unlock(&cm->hca->ib_trans.lock);
			dapls_print_cm_list(ep->header.owner_ia);
			dapl_os_lock(&cm->hca->ib_trans.lock);
		}
#endif
		dapl_evd_connection_callback(cm, 
					     IB_CME_DESTINATION_UNREACHABLE,
					     NULL, ep);
		
		return DAT_ERROR(DAT_INVALID_ADDRESS, 
				 DAT_INVALID_ADDRESS_UNREACHABLE);
	}
	dapl_os_unlock(&cm->lock);

	cm->msg.op = htons(DCM_REQ);
	dapl_os_get_time(&cm->timer); /* reply expected */
	if (ucm_send(&cm->hca->ib_trans, &cm->msg, 
		     &cm->msg.p_data, ntohs(cm->msg.p_size))) 		
		goto bail;

	/* first time through, put on work queue */
	if (!cm->retries)
		ucm_queue_conn(cm);

	return DAT_SUCCESS;

bail:
	dapl_log(DAPL_DBG_TYPE_WARN, 
		 " connect: ERR %s -> cm_lid %d cm_qpn %d r_psp %d p_sz=%d\n",
		 strerror(errno), htons(cm->msg.daddr.ib.lid), 
		 htonl(cm->msg.dqpn), htons(cm->msg.dport), 
		 htonl(cm->msg.p_size));

	/* close socket, free cm structure */
	dapls_ib_cm_free(cm, cm->ep);
	return DAT_INSUFFICIENT_RESOURCES;
}

/*
 * ACTIVE: exchange QP information, called from CR thread
 */
static void ucm_connect_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg)
{
	DAPL_EP *ep = cm->ep;
	ib_cm_events_t event = IB_CME_CONNECTED;

	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_REP_PENDING) {
		dapl_log(DAPL_DBG_TYPE_WARN, 
			 " CONN_RTU: UNEXPECTED state:"
			 " op %d, st %s <- lid %d sqpn %d sport %d\n", 
			 ntohs(msg->op), dapl_cm_state_str(cm->state), 
			 ntohs(msg->saddr.ib.lid), ntohl(msg->saddr.ib.qpn), 
			 ntohs(msg->sport));
		dapl_os_unlock(&cm->lock);
		return;
	}

	/* save remote address information to EP and CM */
	dapl_os_memcpy(&ep->remote_ia_address,
		       &msg->saddr, sizeof(union dcm_addr));
	dapl_os_memcpy(&cm->msg.daddr, 
		       &msg->saddr, sizeof(union dcm_addr));

	/* validate private data size, and copy if necessary */
	if (msg->p_size) {
		if (ntohs(msg->p_size) > DCM_MAX_PDATA_SIZE) {
			dapl_log(DAPL_DBG_TYPE_WARN, 
				 " CONN_RTU: invalid p_size %d:"
				 " st %s <- lid %d sqpn %d spsp %d\n", 
				 ntohs(msg->p_size), 
				 dapl_cm_state_str(cm->state), 
				 ntohs(msg->saddr.ib.lid), 
				 ntohl(msg->saddr.ib.qpn), 
				 ntohs(msg->sport));
			dapl_os_unlock(&cm->lock);
			goto bail;
		}
		dapl_os_memcpy(cm->msg.p_data, 
			       msg->p_data, ntohs(msg->p_size));
	}
		
	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " CONN_RTU: DST lid=%x,"
		     " iqp=%x, qp_type=%d, port=%d psize=%d\n",
		     ntohs(cm->msg.daddr.ib.lid),
		     ntohl(cm->msg.daddr.ib.qpn), cm->msg.daddr.ib.qp_type,
		     ntohs(msg->sport), ntohs(msg->p_size));

	if (ntohs(msg->op) == DCM_REP)
		event = IB_CME_CONNECTED;
	else if (ntohs(msg->op) == DCM_REJ_USER) 
		event = IB_CME_DESTINATION_REJECT_PRIVATE_DATA;
	else  
		event = IB_CME_DESTINATION_REJECT;
	
	if (event != IB_CME_CONNECTED) {
		cm->state = DCM_REJECTED;
		dapl_os_unlock(&cm->lock);

#ifdef DAT_EXTENSIONS
		if (cm->msg.daddr.ib.qp_type == IBV_QPT_UD) 
			goto ud_bail;
		else
#endif
		goto bail;
	}
	dapl_os_unlock(&cm->lock);

	/* modify QP to RTR and then to RTS with remote info */
	dapl_os_lock(&cm->ep->header.lock);
	if (dapls_modify_qp_state(cm->ep->qp_handle,
				  IBV_QPS_RTR, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  (ib_gid_handle_t)cm->msg.daddr.ib.gid) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU: QPS_RTR ERR %s <- lid %x iqp %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&cm->ep->header.lock);
		event = IB_CME_LOCAL_FAILURE;
		goto bail;
	}
	if (dapls_modify_qp_state(cm->ep->qp_handle,
				  IBV_QPS_RTS, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  NULL) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " CONN_RTU: QPS_RTS ERR %s <- lid %x iqp %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&cm->ep->header.lock);
		event = IB_CME_LOCAL_FAILURE;
		goto bail;
	}
	dapl_os_unlock(&cm->ep->header.lock);
	
	/* Send RTU, no private data */
	cm->msg.op = htons(DCM_RTU);
	
	dapl_os_lock(&cm->lock);
	cm->state = DCM_CONNECTED;
	dapl_os_unlock(&cm->lock);

	if (ucm_send(&cm->hca->ib_trans, &cm->msg, NULL, 0)) 		
		goto bail;

	/* init cm_handle and post the event with private data */
	dapl_dbg_log(DAPL_DBG_TYPE_EP, " ACTIVE: connected!\n");

#ifdef DAT_EXTENSIONS
ud_bail:
	if (cm->msg.daddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;
		uint16_t lid = ntohs(cm->msg.daddr.ib.lid);
		
		/* post EVENT, modify_qp, AH already created, ucm msg */
		xevent.status = 0;
		xevent.type = DAT_IB_UD_REMOTE_AH;
		xevent.remote_ah.qpn = ntohl(cm->msg.daddr.ib.qpn);
		xevent.remote_ah.ah = dapls_create_ah(cm->hca, 
						      cm->ep->qp_handle->pd, 
						      cm->ep->qp_handle, 
						      htons(lid), 
						      NULL);
		if (xevent.remote_ah.ah == NULL) {
			event = IB_CME_LOCAL_FAILURE;
			goto bail;
		}
		cm->ah = xevent.remote_ah.ah; /* keep ref to destroy */

		dapl_os_memcpy(&xevent.remote_ah.ia_addr,
			       &cm->msg.daddr,
			       sizeof(union dcm_addr));

		/* remote ia_addr reference includes ucm qpn, not IB qpn */
		((union dcm_addr*)
			&xevent.remote_ah.ia_addr)->ib.qpn = cm->msg.dqpn;

		dapl_dbg_log(DAPL_DBG_TYPE_EP,
			     " ACTIVE: UD xevent ah %p qpn 0x%x lid 0x%x\n",
			     xevent.remote_ah.ah, xevent.remote_ah.qpn, lid);
		dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     	     " ACTIVE: UD xevent ia_addr qp_type %d"
			     " lid 0x%x qpn 0x%x gid 0x"F64x" 0x"F64x" \n",
			     ((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qp_type,
			     ntohs(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.lid),
			     ntohl(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qpn),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[0]),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[8]));

		if (event == IB_CME_CONNECTED)
			event = DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED;
		else {
			xevent.type = DAT_IB_UD_CONNECT_REJECT;
			event = DAT_IB_UD_CONNECTION_REJECT_EVENT;
		}

		dapls_evd_post_connection_event_ext(
				(DAPL_EVD *)cm->ep->param.connect_evd_handle,
				event,
				(DAT_EP_HANDLE)ep,
				(DAT_COUNT)ntohs(cm->msg.p_size),
				(DAT_PVOID *)cm->msg.p_data,
				(DAT_PVOID *)&xevent);

		/* we are done, don't destroy cm_ptr, need pdata */
		dapl_os_lock(&cm->lock);
		cm->state = DCM_RELEASED;
		dapl_os_unlock(&cm->lock);
		
	} else
#endif
	{
		cm->ep->cm_handle = cm; /* only RC, multi CR's on UD */
		dapl_evd_connection_callback(cm,
					     IB_CME_CONNECTED,
					     cm->msg.p_data, cm->ep);
	}
	return;
bail:
	if (cm->msg.saddr.ib.qp_type != IBV_QPT_UD) 
		dapls_ib_reinit_ep(cm->ep); /* reset QP state */

	dapl_evd_connection_callback(NULL, event, cm->msg.p_data, cm->ep);
	dapls_ib_cm_free(cm, NULL); 
}

/*
 * PASSIVE: Accept on listen CM PSP.
 *          create new CM object for this CR, 
 *	    receive peer QP information, private data, 
 *	    and post cr_event 
 */
static void ucm_accept(ib_cm_srvc_handle_t cm, ib_cm_msg_t *msg)
{
	dp_ib_cm_handle_t acm;

	/* Allocate accept CM and setup passive references */
	if ((acm = dapls_ib_cm_create(NULL)) == NULL) {
		dapl_log(DAPL_DBG_TYPE_WARN, " accept: ERR cm_create\n");
		return;
	}

	/* dest CM info from CR msg, source CM info from listen */
	acm->sp = cm->sp;
	acm->hca = cm->hca;
	acm->msg.dport = msg->sport;
	acm->msg.dqpn = msg->sqpn;
	acm->msg.sport = cm->msg.sport; 
	acm->msg.sqpn = cm->msg.sqpn;
	acm->msg.p_size = msg->p_size;

	/* CR saddr is CM daddr info, need EP for local saddr */
	dapl_os_memcpy(&acm->msg.daddr, &msg->saddr, sizeof(union dcm_addr));
	
	dapl_log(DAPL_DBG_TYPE_CM,
		 " accept: DST port=%d lid=%x, iqp=%x, psize=%d\n",
		 ntohs(acm->msg.dport), ntohs(acm->msg.daddr.ib.lid), 
		 htonl(acm->msg.daddr.ib.qpn), htons(acm->msg.p_size));

	/* validate private data size before reading */
	if (ntohs(msg->p_size) > DCM_MAX_PDATA_SIZE) {
		dapl_log(DAPL_DBG_TYPE_WARN, " accept: psize (%d) wrong\n",
			 ntohs(msg->p_size));
		goto bail;
	}

	/* read private data into cm_handle if any present */
	if (msg->p_size) 
		dapl_os_memcpy(acm->msg.p_data, 
			       msg->p_data, ntohs(msg->p_size));
		
	acm->state = DCM_ACCEPTING;
	ucm_queue_conn(acm);

#ifdef DAT_EXTENSIONS
	if (acm->msg.daddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;

		/* post EVENT, modify_qp created ah */
		xevent.status = 0;
		xevent.type = DAT_IB_UD_CONNECT_REQUEST;

		dapls_evd_post_cr_event_ext(acm->sp,
					    DAT_IB_UD_CONNECTION_REQUEST_EVENT,
					    acm,
					    (DAT_COUNT)ntohs(acm->msg.p_size),
					    (DAT_PVOID *)acm->msg.p_data,
					    (DAT_PVOID *)&xevent);
	} else
#endif
		/* trigger CR event and return SUCCESS */
		dapls_cr_callback(acm,
				  IB_CME_CONNECTION_REQUEST_PENDING,
				  acm->msg.p_data, acm->sp);
	return;

bail:
	/* free cm object */
	dapls_ib_cm_free(acm, NULL);
	return;
}

/*
 * PASSIVE: read RTU from active peer, post CONN event
 */
static void ucm_accept_rtu(dp_ib_cm_handle_t cm, ib_cm_msg_t *msg)
{
	dapl_os_lock(&cm->lock);
	if ((ntohs(msg->op) != DCM_RTU) || (cm->state != DCM_RTU_PENDING)) {
		dapl_log(DAPL_DBG_TYPE_WARN, 
			 " accept_rtu: UNEXPECTED op, state:"
			 " op %d, st %s <- lid %x iqp %x sport %d\n", 
			 ntohs(msg->op), dapl_cm_state_str(cm->state), 
			 ntohs(msg->saddr.ib.lid), ntohl(msg->saddr.ib.qpn), 
			 ntohs(msg->sport));
		dapl_os_unlock(&cm->lock);
		goto bail;
	}
	cm->state = DCM_CONNECTED;
	dapl_os_unlock(&cm->lock);
	
	/* final data exchange if remote QP state is good to go */
	dapl_dbg_log(DAPL_DBG_TYPE_CM, " PASSIVE: connected!\n");

#ifdef DAT_EXTENSIONS
	if (cm->msg.saddr.ib.qp_type == IBV_QPT_UD) {
		DAT_IB_EXTENSION_EVENT_DATA xevent;
		uint16_t lid = ntohs(cm->msg.daddr.ib.lid);
		
		/* post EVENT, modify_qp, AH already created, ucm msg */
		xevent.status = 0;
		xevent.type = DAT_IB_UD_PASSIVE_REMOTE_AH;
		xevent.remote_ah.qpn = ntohl(cm->msg.daddr.ib.qpn);
		xevent.remote_ah.ah = dapls_create_ah(cm->hca, 
						      cm->ep->qp_handle->pd, 
						      cm->ep->qp_handle, 
						      htons(lid), 
						      NULL);
		if (xevent.remote_ah.ah == NULL) {
			dapl_log(DAPL_DBG_TYPE_ERR,
				 " accept_rtu: ERR create_ah"
				 " for qpn 0x%x lid 0x%x\n",
				 xevent.remote_ah.qpn, lid);
			goto bail;
		}
		cm->ah = xevent.remote_ah.ah; /* keep ref to destroy */
		dapl_os_memcpy(&xevent.remote_ah.ia_addr,
			       &cm->msg.daddr,
			        sizeof(union dcm_addr));

		/* remote ia_addr reference includes ucm qpn, not IB qpn */
		((union dcm_addr*)
			&xevent.remote_ah.ia_addr)->ib.qpn = cm->msg.dqpn;

		dapl_dbg_log(DAPL_DBG_TYPE_EP,
			     " PASSIVE: UD xevent ah %p qpn 0x%x lid 0x%x\n",
			     xevent.remote_ah.ah, xevent.remote_ah.qpn, lid);
		dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     	     " PASSIVE: UD xevent ia_addr qp_type %d"
			     " lid 0x%x qpn 0x%x gid 0x"F64x" 0x"F64x" \n",
			     ((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qp_type,
			     ntohs(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.lid),
			     ntohl(((union dcm_addr*)
				&xevent.remote_ah.ia_addr)->ib.qpn),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[0]),
			     ntohll(*(uint64_t*)&cm->msg.daddr.ib.gid[8]));

		dapls_evd_post_connection_event_ext(
				(DAPL_EVD *)cm->ep->param.connect_evd_handle,
				DAT_IB_UD_CONNECTION_EVENT_ESTABLISHED,
				(DAT_EP_HANDLE)cm->ep,
				(DAT_COUNT)ntohs(cm->msg.p_size),
				(DAT_PVOID *)cm->msg.p_data,
				(DAT_PVOID *)&xevent);

                /* done with CM object, don't destroy cm, need pdata */
		dapl_os_lock(&cm->lock);
               	cm->state = DCM_RELEASED;
		dapl_os_unlock(&cm->lock);
	} else {
#endif
		cm->ep->cm_handle = cm; /* only RC, multi CR's on UD */
		dapls_cr_callback(cm, IB_CME_CONNECTED, NULL, cm->sp);
	}
	return;
bail:
	if (cm->msg.saddr.ib.qp_type != IBV_QPT_UD) 
		dapls_ib_reinit_ep(cm->ep);	/* reset QP state */
	dapls_ib_cm_free(cm, cm->ep);
	dapls_cr_callback(cm, IB_CME_LOCAL_FAILURE, NULL, cm->sp);
}

/*
 * PASSIVE: user accepted, send reply message with pdata
 */
static int ucm_reply(dp_ib_cm_handle_t cm)
{
	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_RTU_PENDING) {
		dapl_os_unlock(&cm->lock);
		return -1;
	}

	if (cm->retries == cm->hca->ib_trans.retries) {
		dapl_log(DAPL_DBG_TYPE_ERR, 
			 " CM_REPLY: RETRIES EXHAUSTED"
			 " 0x%x %d 0x%x -> 0x%x %d 0x%x\n",
			 htons(cm->msg.saddr.ib.lid), 
			 htons(cm->msg.sport), 
			 htonl(cm->msg.saddr.ib.qpn), 
			 htons(cm->msg.daddr.ib.lid), 
			 htons(cm->msg.dport), 
			 htonl(cm->msg.daddr.ib.qpn));
			
		dapl_os_unlock(&cm->lock);
#ifdef DAPL_COUNTERS
		/* called from check_timers in cm_thread, cm lock held */
		if (g_dapl_dbg_type & DAPL_DBG_TYPE_CM_LIST) {
			dapl_os_unlock(&cm->hca->ib_trans.lock);
			dapls_print_cm_list(dapl_llist_peek_head(&cm->hca->ia_list_head));
			dapl_os_lock(&cm->hca->ib_trans.lock);
		}
#endif
#ifdef DAT_EXTENSIONS
		if (cm->msg.saddr.ib.qp_type == IBV_QPT_UD) {
			DAT_IB_EXTENSION_EVENT_DATA xevent;
					
			/* post REJECT event with CONN_REQ p_data */
			xevent.status = 0;
			xevent.type = DAT_IB_UD_CONNECT_ERROR;
					
			dapls_evd_post_connection_event_ext(
				(DAPL_EVD *)cm->ep->param.connect_evd_handle,
				DAT_IB_UD_CONNECTION_ERROR_EVENT,
				(DAT_EP_HANDLE)cm->ep,
				(DAT_COUNT)ntohs(cm->msg.p_size),
				(DAT_PVOID *)cm->msg.p_data,
				(DAT_PVOID *)&xevent);
		} else 
#endif
			dapls_cr_callback(cm, IB_CME_LOCAL_FAILURE, 
					  NULL, cm->sp);
		return -1;
	}
	dapl_os_get_time(&cm->timer); /* RTU expected */
	dapl_os_unlock(&cm->lock);
	if (ucm_send(&cm->hca->ib_trans, &cm->msg, cm->p_data, cm->p_size)) 		
		return -1;

	return 0;
}


/*
 * PASSIVE: consumer accept, send local QP information, private data, 
 * queue on work thread to receive RTU information to avoid blocking
 * user thread. 
 */
DAT_RETURN
dapli_accept_usr(DAPL_EP *ep, DAPL_CR *cr, DAT_COUNT p_size, DAT_PVOID p_data)
{
	DAPL_IA *ia = ep->header.owner_ia;
	dp_ib_cm_handle_t cm = cr->ib_cm_handle;

	if (p_size > DCM_MAX_PDATA_SIZE)
		return DAT_LENGTH_ERROR;

	dapl_os_lock(&cm->lock);
	if (cm->state != DCM_ACCEPTING) {
		dapl_os_unlock(&cm->lock);
		return DAT_INVALID_STATE;
	}
	dapl_os_unlock(&cm->lock);

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT_USR: remote lid=%x"
		     " iqp=%x qp_type %d, psize=%d\n",
		     ntohs(cm->msg.daddr.ib.lid),
		     ntohl(cm->msg.daddr.ib.qpn), cm->msg.daddr.ib.qp_type, 
		     p_size);

	dapl_dbg_log(DAPL_DBG_TYPE_CM,
		     " ACCEPT_USR: remote GID subnet %016llx id %016llx\n",
		     (unsigned long long)
		     htonll(*(uint64_t*)&cm->msg.daddr.ib.gid[0]),
		     (unsigned long long)
		     htonll(*(uint64_t*)&cm->msg.daddr.ib.gid[8]));

#ifdef DAT_EXTENSIONS
	if (cm->msg.daddr.ib.qp_type == IBV_QPT_UD &&
	    ep->qp_handle->qp_type != IBV_QPT_UD) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			     " ACCEPT_USR: ERR remote QP is UD,"
			     ", but local QP is not\n");
		return (DAT_INVALID_HANDLE | DAT_INVALID_HANDLE_EP);
	}
#endif

	/* modify QP to RTR and then to RTS with remote info already read */
	dapl_os_lock(&ep->header.lock);
	if (dapls_modify_qp_state(ep->qp_handle,
				  IBV_QPS_RTR, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  (ib_gid_handle_t)cm->msg.daddr.ib.gid) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: QPS_RTR ERR %s -> lid %x qpn %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&ep->header.lock);
		goto bail;
	}
	if (dapls_modify_qp_state(ep->qp_handle,
				  IBV_QPS_RTS, 
				  cm->msg.daddr.ib.qpn,
				  cm->msg.daddr.ib.lid,
				  NULL) != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " ACCEPT_USR: QPS_RTS ERR %s -> lid %x qpn %x\n",
			 strerror(errno), ntohs(cm->msg.daddr.ib.lid),
			 ntohl(cm->msg.daddr.ib.qpn));
		dapl_os_unlock(&ep->header.lock);
		goto bail;
	}
	dapl_os_unlock(&ep->header.lock);

	/* save remote address information */
	dapl_os_memcpy(&ep->remote_ia_address,
		       &cm->msg.saddr, sizeof(union dcm_addr));

	/* setup local QP info and type from EP, copy pdata, for reply */
	cm->msg.op = htons(DCM_REP);
	cm->msg.saddr.ib.qpn = htonl(ep->qp_handle->qp_num);
	cm->msg.saddr.ib.qp_type = ep->qp_handle->qp_type;
	cm->msg.saddr.ib.lid = cm->hca->ib_trans.addr.ib.lid; 
	dapl_os_memcpy(&cm->msg.saddr.ib.gid[0],
		       &cm->hca->ib_trans.addr.ib.gid, 16); 

	/* 
	 * UD: deliver p_data with REQ and EST event, keep REQ p_data in 
	 * cm->msg.p_data and save REPLY accept data in cm->p_data for retries 
	 */
	cm->p_size = p_size;
	dapl_os_memcpy(&cm->p_data, p_data, p_size);

	/* save state and setup valid reference to EP, HCA */
	dapl_os_lock(&cm->lock);
	cm->ep = ep;
	cm->hca = ia->hca_ptr;
	cm->state = DCM_RTU_PENDING;
	dapl_os_get_time(&cm->timer); /* RTU expected */
	dapl_os_unlock(&cm->lock);

	if (ucm_reply(cm))
		goto bail;
	
	dapl_dbg_log(DAPL_DBG_TYPE_CM, " PASSIVE: accepted!\n");
	dapls_thread_signal(&cm->hca->ib_trans.signal);
	return DAT_SUCCESS;
bail:
	if (cm->msg.saddr.ib.qp_type != IBV_QPT_UD)
		dapls_ib_reinit_ep(ep);

	dapls_ib_cm_free(cm, ep);
	return DAT_INTERNAL_ERROR;
}


/*
 * dapls_ib_connect
 *
 * Initiate a connection with the passive listener on another node
 *
 * Input:
 *	ep_handle,
 *	remote_ia_address,
 *	remote_conn_qual,
 *	prd_size		size of private data and structure
 *	prd_prt			pointer to private data structure
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_PARAMETER
 *
 */
DAT_RETURN
dapls_ib_connect(IN DAT_EP_HANDLE ep_handle,
		 IN DAT_IA_ADDRESS_PTR r_addr,
		 IN DAT_CONN_QUAL r_psp,
		 IN DAT_COUNT p_size, IN void *p_data)
{
	DAPL_EP *ep = (DAPL_EP *)ep_handle;
	dp_ib_cm_handle_t cm;
	
	/* create CM object, initialize SRC info from EP */
	cm = dapls_ib_cm_create(ep);
	if (cm == NULL)
		return DAT_INSUFFICIENT_RESOURCES;

	/* remote hca and port: lid, gid, network order */
	dapl_os_memcpy(&cm->msg.daddr, r_addr, sizeof(union dcm_addr));

	/* remote uCM information, comes from consumer provider r_addr */
	cm->msg.dport = htons((uint16_t)r_psp);
	cm->msg.dqpn = cm->msg.daddr.ib.qpn;
	cm->msg.daddr.ib.qpn = 0; /* don't have a remote qpn until reply */
	
	if (p_size) {
		cm->msg.p_size = htons(p_size);
		dapl_os_memcpy(&cm->msg.p_data, p_data, p_size);
	}
	
	cm->state = DCM_REP_PENDING;

	/* build connect request, send to remote CM based on r_addr info */
	return(dapli_cm_connect(ep, cm));
}

/*
 * dapls_ib_disconnect
 *
 * Disconnect an EP
 *
 * Input:
 *	ep_handle,
 *	disconnect_flags
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 */
DAT_RETURN
dapls_ib_disconnect(IN DAPL_EP *ep, IN DAT_CLOSE_FLAGS close_flags)
{
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "dapls_ib_disconnect(ep_handle %p ....)\n", ep);

	if (ep->cm_handle == NULL ||
	    ep->param.ep_state == DAT_EP_STATE_DISCONNECTED)
		return DAT_SUCCESS;
	else
		return (dapli_cm_disconnect(ep->cm_handle));
}

/*
 * dapls_ib_disconnect_clean
 *
 * Clean up outstanding connection data. This routine is invoked
 * after the final disconnect callback has occurred. Only on the
 * ACTIVE side of a connection. It is also called if dat_ep_connect
 * times out using the consumer supplied timeout value.
 *
 * Input:
 *	ep_ptr		DAPL_EP
 *	active		Indicates active side of connection
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	void
 *
 */
void
dapls_ib_disconnect_clean(IN DAPL_EP *ep,
			  IN DAT_BOOLEAN active,
			  IN const ib_cm_events_t ib_cm_event)
{
	/* NOTE: SCM will only initialize cm_handle with RC type
	 * 
	 * For UD there can many in-flight CR's so you 
	 * cannot cleanup timed out CR's with EP reference 
	 * alone since they share the same EP. The common
	 * code that handles connection timeout logic needs 
	 * updated for UD support.
	 */
	if (ep->cm_handle)
		dapls_ib_cm_free(ep->cm_handle, ep);

	return;
}

/*
 * dapl_ib_setup_conn_listener
 *
 * Have the CM set up a connection listener.
 *
 * Input:
 *	ibm_hca_handle		HCA handle
 *	qp_handle			QP handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INTERNAL_ERROR
 *	DAT_CONN_QUAL_UNAVAILBLE
 *	DAT_CONN_QUAL_IN_USE
 *
 */
DAT_RETURN
dapls_ib_setup_conn_listener(IN DAPL_IA *ia, 
			     IN DAT_UINT64 sid, 
			     IN DAPL_SP *sp)
{
	ib_cm_srvc_handle_t cm = NULL;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " listen(ia %p ServiceID %d sp %p)\n",
		     ia, sid, sp);

	/* reserve local port, then allocate CM object */
	if (!ucm_get_port(&ia->hca_ptr->ib_trans, (uint16_t)sid)) {
		dapl_dbg_log(DAPL_DBG_TYPE_WARN,
			     " listen: ERROR %s on conn_qual 0x%x\n",
			     strerror(errno), sid);
		return DAT_CONN_QUAL_IN_USE;
	}

	/* cm_create will setup saddr for listen server */
	if ((cm = dapls_ib_cm_create(NULL)) == NULL)
		return DAT_INSUFFICIENT_RESOURCES;

	/* LISTEN: init DST address and QP info to local CM server info */
	cm->sp = sp;
	cm->hca = ia->hca_ptr;
	cm->msg.sport = htons((uint16_t)sid);
	cm->msg.sqpn = htonl(ia->hca_ptr->ib_trans.qp->qp_num);
	cm->msg.saddr.ib.qp_type = IBV_QPT_UD;
        cm->msg.saddr.ib.lid = ia->hca_ptr->ib_trans.addr.ib.lid; 
	dapl_os_memcpy(&cm->msg.saddr.ib.gid[0],
		       &cm->hca->ib_trans.addr.ib.gid, 16); 
	
	/* save cm_handle reference in service point */
	sp->cm_srvc_handle = cm;

	/* queue up listen socket to process inbound CR's */
	cm->state = DCM_LISTEN;
	ucm_queue_listen(cm);

	return DAT_SUCCESS;
}


/*
 * dapl_ib_remove_conn_listener
 *
 * Have the CM remove a connection listener.
 *
 * Input:
 *	ia_handle		IA handle
 *	ServiceID		IB Channel Service ID
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_STATE
 *
 */
DAT_RETURN
dapls_ib_remove_conn_listener(IN DAPL_IA *ia, IN DAPL_SP *sp)
{
	ib_cm_srvc_handle_t cm = sp->cm_srvc_handle;
	ib_hca_transport_t *tp = &ia->hca_ptr->ib_trans;

	/* free cm_srvc_handle and port, and mark CM for cleanup */
	if (cm) {
		dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " remove_listener(ia %p sp %p cm %p psp=%d)\n",
		     ia, sp, cm, ntohs(cm->msg.dport));

		sp->cm_srvc_handle = NULL;
		dapl_os_lock(&cm->lock);
		ucm_free_port(tp, ntohs(cm->msg.dport));
		cm->msg.dport = 0;
		cm->state = DCM_DESTROY;
		dapl_os_unlock(&cm->lock);
		ucm_dequeue_listen(cm);
		dapl_os_free(cm, sizeof(*cm));
	}
	return DAT_SUCCESS;
}

/*
 * dapls_ib_accept_connection
 *
 * Perform necessary steps to accept a connection
 *
 * Input:
 *	cr_handle
 *	ep_handle
 *	private_data_size
 *	private_data
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INTERNAL_ERROR
 *
 */
DAT_RETURN
dapls_ib_accept_connection(IN DAT_CR_HANDLE cr_handle,
			   IN DAT_EP_HANDLE ep_handle,
			   IN DAT_COUNT p_size, 
			   IN const DAT_PVOID p_data)
{
	DAPL_CR *cr = (DAPL_CR *)cr_handle;
	DAPL_EP *ep = (DAPL_EP *)ep_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " accept_connection(cr %p ep %p prd %p,%d)\n",
		     cr, ep, p_data, p_size);

	/* allocate and attach a QP if necessary */
	if (ep->qp_state == DAPL_QP_STATE_UNATTACHED) {
		DAT_RETURN status;
		status = dapls_ib_qp_alloc(ep->header.owner_ia,
					   ep, ep);
		if (status != DAT_SUCCESS)
			return status;
	}
	return (dapli_accept_usr(ep, cr, p_size, p_data));
}

/*
 * dapls_ib_reject_connection
 *
 * Reject a connection
 *
 * Input:
 *	cr_handle
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INTERNAL_ERROR
 *
 */
DAT_RETURN
dapls_ib_reject_connection(IN dp_ib_cm_handle_t cm,
			   IN int reason,
			   IN DAT_COUNT psize, IN const DAT_PVOID pdata)
{
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " reject(cm %p reason %x, pdata %p, psize %d)\n",
		     cm, reason, pdata, psize);

        if (psize > DCM_MAX_PDATA_SIZE)
                return DAT_LENGTH_ERROR;

	/* cr_thread will destroy CR, update saddr lid, gid, qp_type info */
	dapl_os_lock(&cm->lock);
	cm->state = DCM_REJECTED;
	cm->msg.saddr.ib.lid = cm->hca->ib_trans.addr.ib.lid; 
	cm->msg.saddr.ib.qp_type = cm->msg.daddr.ib.qp_type;
	dapl_os_memcpy(&cm->msg.saddr.ib.gid[0],
		       &cm->hca->ib_trans.addr.ib.gid, 16); 
	cm->msg.op = htons(DCM_REJ_USER);
	
	if (ucm_send(&cm->hca->ib_trans, &cm->msg, pdata, psize)) {
		dapl_log(DAPL_DBG_TYPE_WARN,
			 " cm_reject: ERR: %s\n", strerror(errno));
		return DAT_INTERNAL_ERROR;
	}
	dapl_os_unlock(&cm->lock);
		
	/* cleanup and destroy CM resources */ 
	dapls_ib_cm_free(cm, NULL);

	return DAT_SUCCESS;
}

/*
 * dapls_ib_cm_remote_addr
 *
 * Obtain the remote IP address given a connection
 *
 * Input:
 *	cr_handle
 *
 * Output:
 *	remote_ia_address: where to place the remote address
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_INVALID_HANDLE
 *
 */
DAT_RETURN
dapls_ib_cm_remote_addr(IN DAT_HANDLE dat_handle,
			OUT DAT_SOCK_ADDR6 * remote_ia_address)
{
	DAPL_HEADER *header;
	dp_ib_cm_handle_t ib_cm_handle;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "dapls_ib_cm_remote_addr(dat_handle %p, ....)\n",
		     dat_handle);

	header = (DAPL_HEADER *) dat_handle;

	if (header->magic == DAPL_MAGIC_EP)
		ib_cm_handle = ((DAPL_EP *) dat_handle)->cm_handle;
	else if (header->magic == DAPL_MAGIC_CR)
		ib_cm_handle = ((DAPL_CR *) dat_handle)->ib_cm_handle;
	else
		return DAT_INVALID_HANDLE;

	dapl_os_memcpy(remote_ia_address,
		       &ib_cm_handle->msg.daddr, sizeof(DAT_SOCK_ADDR6));

	return DAT_SUCCESS;
}

/*
 * dapls_ib_private_data_size
 *
 * Return the size of private data given a connection op type
 *
 * Input:
 *	prd_ptr		private data pointer
 *	conn_op		connection operation type
 *
 * If prd_ptr is NULL, this is a query for the max size supported by
 * the provider, otherwise it is the actual size of the private data
 * contained in prd_ptr.
 *
 *
 * Output:
 *	None
 *
 * Returns:
 * 	length of private data
 *
 */
int dapls_ib_private_data_size(IN DAPL_PRIVATE * prd_ptr,
			       IN DAPL_PDATA_OP conn_op, IN DAPL_HCA * hca_ptr)
{
	int size;

	switch (conn_op) {
	case DAPL_PDATA_CONN_REQ:
	case DAPL_PDATA_CONN_REP:
	case DAPL_PDATA_CONN_REJ:
	case DAPL_PDATA_CONN_DREQ:
	case DAPL_PDATA_CONN_DREP:
		size = DCM_MAX_PDATA_SIZE;
		break;
	default:
		size = 0;
	}			
	return size;
}

/*
 * Map all socket CM event codes to the DAT equivelent.
 */
#define DAPL_IB_EVENT_CNT	10

static struct ib_cm_event_map {
	const ib_cm_events_t ib_cm_event;
	DAT_EVENT_NUMBER dat_event_num;
} ib_cm_event_map[DAPL_IB_EVENT_CNT] = {
/* 00 */ {IB_CME_CONNECTED, 
	  DAT_CONNECTION_EVENT_ESTABLISHED},
/* 01 */ {IB_CME_DISCONNECTED, 
	  DAT_CONNECTION_EVENT_DISCONNECTED},
/* 02 */ {IB_CME_DISCONNECTED_ON_LINK_DOWN,
	  DAT_CONNECTION_EVENT_DISCONNECTED},
/* 03 */ {IB_CME_CONNECTION_REQUEST_PENDING, 
	  DAT_CONNECTION_REQUEST_EVENT},
/* 04 */ {IB_CME_CONNECTION_REQUEST_PENDING_PRIVATE_DATA,
	  DAT_CONNECTION_REQUEST_EVENT},
/* 05 */ {IB_CME_DESTINATION_REJECT,
	  DAT_CONNECTION_EVENT_NON_PEER_REJECTED},
/* 06 */ {IB_CME_DESTINATION_REJECT_PRIVATE_DATA,
	  DAT_CONNECTION_EVENT_PEER_REJECTED},
/* 07 */ {IB_CME_DESTINATION_UNREACHABLE, 
	  DAT_CONNECTION_EVENT_UNREACHABLE},
/* 08 */ {IB_CME_TOO_MANY_CONNECTION_REQUESTS,
	  DAT_CONNECTION_EVENT_NON_PEER_REJECTED},
/* 09 */ {IB_CME_LOCAL_FAILURE, 
	  DAT_CONNECTION_EVENT_BROKEN}
};

/*
 * dapls_ib_get_cm_event
 *
 * Return a DAT connection event given a provider CM event.
 *
 * Input:
 *	dat_event_num	DAT event we need an equivelent CM event for
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	ib_cm_event of translated DAPL value
 */
DAT_EVENT_NUMBER
dapls_ib_get_dat_event(IN const ib_cm_events_t ib_cm_event,
		       IN DAT_BOOLEAN active)
{
	DAT_EVENT_NUMBER dat_event_num;
	int i;

	if (ib_cm_event > IB_CME_LOCAL_FAILURE)
		return (DAT_EVENT_NUMBER) 0;

	dat_event_num = 0;
	for (i = 0; i < DAPL_IB_EVENT_CNT; i++) {
		if (ib_cm_event == ib_cm_event_map[i].ib_cm_event) {
			dat_event_num = ib_cm_event_map[i].dat_event_num;
			break;
		}
	}
	dapl_dbg_log(DAPL_DBG_TYPE_CALLBACK,
		     "dapls_ib_get_dat_event: event translate(%s) ib=0x%x dat=0x%x\n",
		     active ? "active" : "passive", ib_cm_event, dat_event_num);

	return dat_event_num;
}

/*
 * dapls_ib_get_dat_event
 *
 * Return a DAT connection event given a provider CM event.
 * 
 * Input:
 *	ib_cm_event	event provided to the dapl callback routine
 *	active		switch indicating active or passive connection
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_EVENT_NUMBER of translated provider value
 */
ib_cm_events_t dapls_ib_get_cm_event(IN DAT_EVENT_NUMBER dat_event_num)
{
	ib_cm_events_t ib_cm_event;
	int i;

	ib_cm_event = 0;
	for (i = 0; i < DAPL_IB_EVENT_CNT; i++) {
		if (dat_event_num == ib_cm_event_map[i].dat_event_num) {
			ib_cm_event = ib_cm_event_map[i].ib_cm_event;
			break;
		}
	}
	return ib_cm_event;
}

#if defined(_WIN32) || defined(_WIN64)

void cm_thread(void *arg)
{
	struct dapl_hca *hca = arg;
	dp_ib_cm_handle_t cm, next;
	DWORD time_ms;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread: ENTER hca %p\n", hca);
	dapl_os_lock(&hca->ib_trans.lock);
	for (hca->ib_trans.cm_state = IB_THREAD_RUN;
	     hca->ib_trans.cm_state == IB_THREAD_RUN ||
	     !dapl_llist_is_empty(&hca->ib_trans.list);
	     dapl_os_lock(&hca->ib_trans.lock)) {

		time_ms = INFINITE;
		CompSetZero(&hca->ib_trans.signal.set);
		CompSetAdd(&hca->ib_hca_handle->channel, &hca->ib_trans.signal.set);
		CompSetAdd(&hca->ib_trans.rch->comp_channel, &hca->ib_trans.signal.set);
		CompSetAdd(&hca->ib_trans.ib_cq->comp_channel, &hca->ib_trans.signal.set);

		next = dapl_llist_is_empty(&hca->ib_trans.list) ? NULL :
			dapl_llist_peek_head(&hca->ib_trans.list);

		while (next) {
			cm = next;
			next = dapl_llist_next_entry(&hca->ib_trans.list,
						     (DAPL_LLIST_ENTRY *)&cm->entry);
			dapl_os_lock(&cm->lock);
			if (cm->state == DCM_DESTROY || 
			    hca->ib_trans.cm_state != IB_THREAD_RUN) {
				dapl_llist_remove_entry(&hca->ib_trans.list,
							(DAPL_LLIST_ENTRY *)&cm->entry);
				dapl_os_unlock(&cm->lock);
				dapl_os_free(cm, sizeof(*cm));
				continue;
			}
			dapl_os_unlock(&cm->lock);
			ucm_check_timers(cm, &time_ms);
		}

		dapl_os_unlock(&hca->ib_trans.lock);

		hca->ib_hca_handle->channel.Milliseconds = time_ms;
		hca->ib_trans.rch->comp_channel.Milliseconds = time_ms;
		hca->ib_trans.ib_cq->comp_channel.Milliseconds = time_ms;
		CompSetPoll(&hca->ib_trans.signal.set, time_ms);

		hca->ib_hca_handle->channel.Milliseconds = 0;
		hca->ib_trans.rch->comp_channel.Milliseconds = 0;
		hca->ib_trans.ib_cq->comp_channel.Milliseconds = 0;

		ucm_recv(&hca->ib_trans);
		ucm_async_event(hca);
		dapli_cq_event_cb(&hca->ib_trans);
	}

	dapl_os_unlock(&hca->ib_trans.lock);
	hca->ib_trans.cm_state = IB_THREAD_EXIT;
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread(hca %p) exit\n", hca);
}

#else				// _WIN32 || _WIN64

void cm_thread(void *arg)
{
	struct dapl_hca *hca = arg;
	dp_ib_cm_handle_t cm, next;
	struct dapl_fd_set *set;
	char rbuf[2];
	int time_ms;

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread: ENTER hca %p\n", hca);
	set = dapl_alloc_fd_set();
	if (!set)
		goto out;

	dapl_os_lock(&hca->ib_trans.lock);
	hca->ib_trans.cm_state = IB_THREAD_RUN;

	while (1) {
		time_ms = -1; /* reset to blocking */
		dapl_fd_zero(set);
		dapl_fd_set(hca->ib_trans.signal.scm[0], set, DAPL_FD_READ);	
		dapl_fd_set(hca->ib_hca_handle->async_fd, set, DAPL_FD_READ);
		dapl_fd_set(hca->ib_trans.rch->fd, set, DAPL_FD_READ);
		dapl_fd_set(hca->ib_trans.ib_cq->fd, set, DAPL_FD_READ);
		
		if (!dapl_llist_is_empty(&hca->ib_trans.list))
			next = dapl_llist_peek_head(&hca->ib_trans.list);
		else
			next = NULL;

		while (next) {
			cm = next;
			next = dapl_llist_next_entry(
					&hca->ib_trans.list,
					(DAPL_LLIST_ENTRY *)&cm->entry);
			dapl_os_lock(&cm->lock);
			if (cm->state == DCM_DESTROY || 
			    hca->ib_trans.cm_state != IB_THREAD_RUN) {
				dapl_llist_remove_entry(
					&hca->ib_trans.list,
					(DAPL_LLIST_ENTRY *)&cm->entry);
				dapl_os_unlock(&cm->lock);
				dapl_os_free(cm, sizeof(*cm));
				continue;
			}
			dapl_os_unlock(&cm->lock);
			ucm_check_timers(cm, &time_ms);
		}

		/* set to exit and all resources destroyed */
		if ((hca->ib_trans.cm_state != IB_THREAD_RUN) &&
		    (dapl_llist_is_empty(&hca->ib_trans.list)))
			break;

		dapl_os_unlock(&hca->ib_trans.lock);
		dapl_select(set, time_ms);

		/* Process events: CM, ASYNC, NOTIFY THREAD */
		if (dapl_poll(hca->ib_trans.rch->fd, 
			      DAPL_FD_READ) == DAPL_FD_READ) {
			ucm_recv(&hca->ib_trans);
		}
		if (dapl_poll(hca->ib_hca_handle->async_fd, 
			      DAPL_FD_READ) == DAPL_FD_READ) {
			ucm_async_event(hca);
		}
		if (dapl_poll(hca->ib_trans.ib_cq->fd, 
			      DAPL_FD_READ) == DAPL_FD_READ) {
			dapli_cq_event_cb(&hca->ib_trans);
		}
		while (dapl_poll(hca->ib_trans.signal.scm[0], 
				 DAPL_FD_READ) == DAPL_FD_READ) {
			recv(hca->ib_trans.signal.scm[0], rbuf, 2, 0);
		}

		dapl_os_lock(&hca->ib_trans.lock);
		
		/* set to exit and all resources destroyed */
		if ((hca->ib_trans.cm_state != IB_THREAD_RUN) &&
		    (dapl_llist_is_empty(&hca->ib_trans.list)))
			break;
	}

	dapl_os_unlock(&hca->ib_trans.lock);
	free(set);
out:
	hca->ib_trans.cm_state = IB_THREAD_EXIT;
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " cm_thread(hca %p) exit\n", hca);
}
#endif

#ifdef DAPL_COUNTERS
/* Debug aid: List all Connections in process and state */
void dapls_print_cm_list(IN DAPL_IA *ia_ptr)
{
	/* Print in process CM's for this IA, if debug type set */
	int i = 0;
	dp_ib_cm_handle_t cm, next_cm;
	struct dapl_llist_entry	**list;
	DAPL_OS_LOCK *lock;
	
	/* LISTEN LIST */
	list = &ia_ptr->hca_ptr->ib_trans.llist;
	lock = &ia_ptr->hca_ptr->ib_trans.llock;

	dapl_os_lock(lock);
	if (!dapl_llist_is_empty((DAPL_LLIST_HEAD*)list))
		next_cm = dapl_llist_peek_head((DAPL_LLIST_HEAD*)list);
 	else
		next_cm = NULL;

        printf("\n DAPL IA LISTEN/CONNECTIONS IN PROCESS:\n");
	while (next_cm) {
		cm = next_cm;
		next_cm = dapl_llist_next_entry((DAPL_LLIST_HEAD*)list,
						(DAPL_LLIST_ENTRY*)&cm->entry);

		printf( "  LISTEN[%d]: sp %p %s uCM_QP: 0x%x %d 0x%x l_pid %x,%d\n",
			i, cm->sp, dapl_cm_state_str(cm->state),
			ntohs(cm->msg.saddr.ib.lid), ntohs(cm->msg.sport),
			ntohl(cm->msg.sqpn), ntohl(*(DAT_UINT32*)cm->msg.resv), 
			ntohl(*(DAT_UINT32*)cm->msg.resv)); 
		i++;
	}
	dapl_os_unlock(lock);

	/* CONNECTION LIST */
	list = &ia_ptr->hca_ptr->ib_trans.list;
	lock = &ia_ptr->hca_ptr->ib_trans.lock;

	dapl_os_lock(lock);
	if (!dapl_llist_is_empty((DAPL_LLIST_HEAD*)list))
		next_cm = dapl_llist_peek_head((DAPL_LLIST_HEAD*)list);
 	else
		next_cm = NULL;

        while (next_cm) {
		cm = next_cm;
		next_cm = dapl_llist_next_entry((DAPL_LLIST_HEAD*)list,
						(DAPL_LLIST_ENTRY*)&cm->entry);

		printf( "  CONN[%d]: ep %p cm %p %s %s"
			"  %x %x %x %s %x %x %x r_pid %x,%d\n",
			i, cm->ep, cm,
			cm->msg.saddr.ib.qp_type == IBV_QPT_RC ? "RC" : "UD",
			dapl_cm_state_str(cm->state),
			ntohs(cm->msg.saddr.ib.lid),
			ntohs(cm->msg.sport),
			ntohl(cm->msg.saddr.ib.qpn),	
			cm->sp ? "<-" : "->",
			ntohs(cm->msg.daddr.ib.lid),
			ntohs(cm->msg.dport),
			ntohl(cm->msg.daddr.ib.qpn),
			ntohs(cm->msg.op) == DCM_REQ ? 0 : ntohl(*(DAT_UINT32*)cm->msg.resv), 
			ntohs(cm->msg.op) == DCM_REQ ? 0 : ntohl(*(DAT_UINT32*)cm->msg.resv)); 
		i++;
	}
	printf("\n");
	dapl_os_unlock(lock);
}
#endif
