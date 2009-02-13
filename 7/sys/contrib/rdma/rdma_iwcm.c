/*
 * Copyright (c) 2004, 2005 Intel Corporation.  All rights reserved.
 * Copyright (c) 2004 Topspin Corporation.  All rights reserved.
 * Copyright (c) 2004, 2005 Voltaire Corporation.  All rights reserved.
 * Copyright (c) 2005 Sun Microsystems, Inc. All rights reserved.
 * Copyright (c) 2005 Open Grid Computing, Inc. All rights reserved.
 * Copyright (c) 2005 Network Appliance, Inc. All rights reserved.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/priv.h>
#include <sys/syslog.h>
#include <sys/malloc.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <contrib/rdma/iw_cm.h>

enum iw_cm_state {
	IW_CM_STATE_IDLE,             /* unbound, inactive */
	IW_CM_STATE_LISTEN,           /* listen waiting for connect */
	IW_CM_STATE_CONN_RECV,        /* inbound waiting for user accept */
	IW_CM_STATE_CONN_SENT,        /* outbound waiting for peer accept */
	IW_CM_STATE_ESTABLISHED,      /* established */
	IW_CM_STATE_CLOSING,	      /* disconnect */
	IW_CM_STATE_DESTROYING        /* object being deleted */
};

struct iwcm_id_private {
	struct iw_cm_id	id;
	enum iw_cm_state state;
	unsigned long flags;
	struct ib_qp *qp;
	void * destroy_comp;
	void * connect_wait;
	TAILQ_HEAD(, iwcm_work) work_list;
	struct mtx lock;
	volatile int refcount;
	TAILQ_HEAD(, iwcm_work) work_free_list;
};

#define IWCM_F_CALLBACK_DESTROY   1
#define IWCM_F_CONNECT_WAIT       2

static struct taskqueue *iwcm_wq;
struct iwcm_work {
	struct task task;
	struct iwcm_id_private *cm_id;
	TAILQ_ENTRY(iwcm_work) list;
	struct iw_cm_event event;
	TAILQ_ENTRY(iwcm_work) free_list;
};

/*
 * The following services provide a mechanism for pre-allocating iwcm_work
 * elements.  The design pre-allocates them  based on the cm_id type:
 *	LISTENING IDS: 	Get enough elements preallocated to handle the
 *			listen backlog.
 *	ACTIVE IDS:	4: CONNECT_REPLY, ESTABLISHED, DISCONNECT, CLOSE
 *	PASSIVE IDS:	3: ESTABLISHED, DISCONNECT, CLOSE
 *
 * Allocating them in connect and listen avoids having to deal
 * with allocation failures on the event upcall from the provider (which
 * is called in the interrupt context).
 *
 * One exception is when creating the cm_id for incoming connection requests.
 * There are two cases:
 * 1) in the event upcall, cm_event_handler(), for a listening cm_id.  If
 *    the backlog is exceeded, then no more connection request events will
 *    be processed.  cm_event_handler() returns ENOMEM in this case.  Its up
 *    to the provider to reject the connection request.
 * 2) in the connection request workqueue handler, cm_conn_req_handler().
 *    If work elements cannot be allocated for the new connect request cm_id,
 *    then IWCM will call the provider reject method.  This is ok since
 *    cm_conn_req_handler() runs in the workqueue thread context.
 */

static struct iwcm_work *get_work(struct iwcm_id_private *cm_id_priv)
{
	struct iwcm_work *work;

	if (TAILQ_EMPTY(&cm_id_priv->work_free_list))
		return NULL;
	work = TAILQ_FIRST(&cm_id_priv->work_free_list);
	TAILQ_REMOVE(&cm_id_priv->work_free_list, work, free_list);
	return work;
}

static void put_work(struct iwcm_work *work)
{
	TAILQ_INSERT_HEAD(&work->cm_id->work_free_list, work, free_list);
}

static void dealloc_work_entries(struct iwcm_id_private *cm_id_priv)
{
	struct iwcm_work *e, *tmp;

	TAILQ_FOREACH_SAFE(e, &cm_id_priv->work_free_list, free_list, tmp)
		free(e, M_DEVBUF);
}

static int alloc_work_entries(struct iwcm_id_private *cm_id_priv, int count)
{
	struct iwcm_work *work;

	PANIC_IF(!TAILQ_EMPTY(&cm_id_priv->work_free_list));
	while (count--) {
		work = malloc(sizeof(struct iwcm_work), M_DEVBUF, M_NOWAIT);
		if (!work) {
			dealloc_work_entries(cm_id_priv);
			return (ENOMEM);
		}
		work->cm_id = cm_id_priv;
		put_work(work);
	}
	return 0;
}

/*
 * Save private data from incoming connection requests to
 * iw_cm_event, so the low level driver doesn't have to. Adjust
 * the event ptr to point to the local copy.
 */
static int copy_private_data(struct iw_cm_event *event)
{
	void *p;

	p = malloc(event->private_data_len, M_DEVBUF, M_NOWAIT);
	if (!p)
		return (ENOMEM);
	bcopy(event->private_data, p, event->private_data_len);
	event->private_data = p;
	return 0;
}

static void free_cm_id(struct iwcm_id_private *cm_id_priv)
{
	dealloc_work_entries(cm_id_priv);
	free(cm_id_priv, M_DEVBUF);
}

/*
 * Release a reference on cm_id. If the last reference is being
 * released, enable the waiting thread (in iw_destroy_cm_id) to
 * get woken up, and return 1 if a thread is already waiting.
 */
static int iwcm_deref_id(struct iwcm_id_private *cm_id_priv)
{
	mtx_lock(&cm_id_priv->lock);
	PANIC_IF(atomic_load_acq_int(&cm_id_priv->refcount)==0);
	if (atomic_fetchadd_int(&cm_id_priv->refcount, -1) == 1) {
		PANIC_IF(!TAILQ_EMPTY(&cm_id_priv->work_list));
		wakeup(&cm_id_priv->destroy_comp);
		mtx_unlock(&cm_id_priv->lock);
		return 1;
	}
	mtx_unlock(&cm_id_priv->lock);

	return 0;
}

static void add_ref(struct iw_cm_id *cm_id)
{
	struct iwcm_id_private *cm_id_priv;
	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	mtx_lock(&cm_id_priv->lock);
	atomic_add_int(&cm_id_priv->refcount, 1);
	mtx_unlock(&cm_id_priv->lock);
}

static void rem_ref(struct iw_cm_id *cm_id)
{
	struct iwcm_id_private *cm_id_priv;
	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	if (iwcm_deref_id(cm_id_priv) &&
	    isset(&cm_id_priv->flags, IWCM_F_CALLBACK_DESTROY)) {
		PANIC_IF(!TAILQ_EMPTY(&cm_id_priv->work_list));
		free_cm_id(cm_id_priv);
	}
}

static int cm_event_handler(struct iw_cm_id *cm_id, struct iw_cm_event *event);

struct iw_cm_id *iw_create_cm_id(struct ib_device *device,
				 struct socket *so,
				 iw_cm_handler cm_handler,
				 void *context)
{
	struct iwcm_id_private *cm_id_priv;

	KASSERT(so, ("iw_create_cm_id called with NULL socket!"));
	cm_id_priv = malloc(sizeof(*cm_id_priv), M_DEVBUF, M_NOWAIT);
	if (!cm_id_priv)
		return ERR_PTR(ENOMEM);
	bzero(cm_id_priv, sizeof *cm_id_priv);

	cm_id_priv->state = IW_CM_STATE_IDLE;
	cm_id_priv->id.device = device;
	cm_id_priv->id.cm_handler = cm_handler;
	cm_id_priv->id.context = context;
	cm_id_priv->id.event_handler = cm_event_handler;
	cm_id_priv->id.add_ref = add_ref;
	cm_id_priv->id.rem_ref = rem_ref;
	cm_id_priv->id.so = so;
	mtx_init(&cm_id_priv->lock, "cm_id_priv", NULL, MTX_DUPOK|MTX_DEF);
	atomic_store_rel_int(&cm_id_priv->refcount, 1);
	TAILQ_INIT(&cm_id_priv->work_list);
	TAILQ_INIT(&cm_id_priv->work_free_list);

	return &cm_id_priv->id;
}


static int iwcm_modify_qp_err(struct ib_qp *qp)
{
	struct ib_qp_attr qp_attr;

	if (!qp)
		return (EINVAL);

	qp_attr.qp_state = IB_QPS_ERR;
	return ib_modify_qp(qp, &qp_attr, IB_QP_STATE);
}

/*
 * This is really the RDMAC CLOSING state. It is most similar to the
 * IB SQD QP state.
 */
static int iwcm_modify_qp_sqd(struct ib_qp *qp)
{
	struct ib_qp_attr qp_attr;

	PANIC_IF(qp == NULL);
	qp_attr.qp_state = IB_QPS_SQD;
	return ib_modify_qp(qp, &qp_attr, IB_QP_STATE);
}

/*
 * CM_ID <-- CLOSING
 *
 * Block if a passive or active connection is currently being processed. Then
 * process the event as follows:
 * - If we are ESTABLISHED, move to CLOSING and modify the QP state
 *   based on the abrupt flag
 * - If the connection is already in the CLOSING or IDLE state, the peer is
 *   disconnecting concurrently with us and we've already seen the
 *   DISCONNECT event -- ignore the request and return 0
 * - Disconnect on a listening endpoint returns EINVAL
 */
int iw_cm_disconnect(struct iw_cm_id *cm_id, int abrupt)
{
	struct iwcm_id_private *cm_id_priv;
	int ret = 0;
	struct ib_qp *qp = NULL;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	/* Wait if we're currently in a connect or accept downcall */
	mtx_lock(&cm_id_priv->lock);
	if (isset(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT))
		msleep(&cm_id_priv->connect_wait, &cm_id_priv->lock, 0, "iwcm connect1", 0);

	switch (cm_id_priv->state) {
	case IW_CM_STATE_ESTABLISHED:
		cm_id_priv->state = IW_CM_STATE_CLOSING;

		/* QP could be <nul> for user-mode client */
		if (cm_id_priv->qp)
			qp = cm_id_priv->qp;
		else
			ret = EINVAL;
		break;
	case IW_CM_STATE_LISTEN:
		ret = EINVAL;
		break;
	case IW_CM_STATE_CLOSING:
		/* remote peer closed first */
	case IW_CM_STATE_IDLE:
		/* accept or connect returned !0 */
		break;
	case IW_CM_STATE_CONN_RECV:
		/*
		 * App called disconnect before/without calling accept after
		 * connect_request event delivered.
		 */
		break;
	case IW_CM_STATE_CONN_SENT:
		/* Can only get here if wait above fails */
	default:
		panic("just cuz");
	}
	mtx_unlock(&cm_id_priv->lock);

	if (qp) {
		if (abrupt)
			ret = iwcm_modify_qp_err(qp);
		else
			ret = iwcm_modify_qp_sqd(qp);

		/*
		 * If both sides are disconnecting the QP could
		 * already be in ERR or SQD states
		 */
		ret = 0;
	}

	return ret;
}

/*
 * CM_ID <-- DESTROYING
 *
 * Clean up all resources associated with the connection and release
 * the initial reference taken by iw_create_cm_id.
 */
static void destroy_cm_id(struct iw_cm_id *cm_id)
{
	struct iwcm_id_private *cm_id_priv;
	int ret;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	/*
	 * Wait if we're currently in a connect or accept downcall. A
	 * listening endpoint should never block here.
	 */
	mtx_lock(&cm_id_priv->lock);
	if (isset(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT))
		msleep(&cm_id_priv->connect_wait, &cm_id_priv->lock, 0, "iwcm connect2", 0);

	switch (cm_id_priv->state) {
	case IW_CM_STATE_LISTEN:
		cm_id_priv->state = IW_CM_STATE_DESTROYING;
		mtx_unlock(&cm_id_priv->lock);
		/* destroy the listening endpoint */
		ret = cm_id->device->iwcm->destroy_listen(cm_id);
		mtx_lock(&cm_id_priv->lock);
		break;
	case IW_CM_STATE_ESTABLISHED:
		cm_id_priv->state = IW_CM_STATE_DESTROYING;
		mtx_unlock(&cm_id_priv->lock);
		/* Abrupt close of the connection */
		(void)iwcm_modify_qp_err(cm_id_priv->qp);
		mtx_lock(&cm_id_priv->lock);
		break;
	case IW_CM_STATE_IDLE:
	case IW_CM_STATE_CLOSING:
		cm_id_priv->state = IW_CM_STATE_DESTROYING;
		break;
	case IW_CM_STATE_CONN_RECV:
		/*
		 * App called destroy before/without calling accept after
		 * receiving connection request event notification or
		 * returned non zero from the event callback function.
		 * In either case, must tell the provider to reject.
		 */
		cm_id_priv->state = IW_CM_STATE_DESTROYING;
		break;
	case IW_CM_STATE_CONN_SENT:
	case IW_CM_STATE_DESTROYING:
	default:
		panic("just cuz");
		break;
	}
	if (cm_id_priv->qp) {
		cm_id_priv->id.device->iwcm->rem_ref(cm_id_priv->qp);
		cm_id_priv->qp = NULL;
	}
	mtx_unlock(&cm_id_priv->lock);

	(void)iwcm_deref_id(cm_id_priv);
}

/*
 * This function is only called by the application thread and cannot
 * be called by the event thread. The function will wait for all
 * references to be released on the cm_id and then free the cm_id
 * object.
 */
void iw_destroy_cm_id(struct iw_cm_id *cm_id)
{
	struct iwcm_id_private *cm_id_priv;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	PANIC_IF(isset(&cm_id_priv->flags, IWCM_F_CALLBACK_DESTROY));

	destroy_cm_id(cm_id);

	mtx_lock(&cm_id_priv->lock);
	if (atomic_load_acq_int(&cm_id_priv->refcount))
		msleep(&cm_id_priv->destroy_comp, &cm_id_priv->lock, 0, "iwcm destroy", 0);
	mtx_unlock(&cm_id_priv->lock);

	free_cm_id(cm_id_priv);
}

/*
 * CM_ID <-- LISTEN
 *
 * Start listening for connect requests. Generates one CONNECT_REQUEST
 * event for each inbound connect request.
 */
int iw_cm_listen(struct iw_cm_id *cm_id, int backlog)
{
	struct iwcm_id_private *cm_id_priv;
	int ret;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);

	ret = alloc_work_entries(cm_id_priv, backlog);
	if (ret)
		return ret;

	mtx_lock(&cm_id_priv->lock);
	switch (cm_id_priv->state) {
	case IW_CM_STATE_IDLE:
		cm_id_priv->state = IW_CM_STATE_LISTEN;
		mtx_unlock(&cm_id_priv->lock);
		ret = cm_id->device->iwcm->create_listen(cm_id, backlog);
		if (ret)
			cm_id_priv->state = IW_CM_STATE_IDLE;
		mtx_lock(&cm_id_priv->lock);
		break;
	default:
		ret = EINVAL;
	}
	mtx_unlock(&cm_id_priv->lock);

	return ret;
}

/*
 * CM_ID <-- IDLE
 *
 * Rejects an inbound connection request. No events are generated.
 */
int iw_cm_reject(struct iw_cm_id *cm_id,
		 const void *private_data,
		 u8 private_data_len)
{
	struct iwcm_id_private *cm_id_priv;
	int ret;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	setbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);

	mtx_lock(&cm_id_priv->lock);
	if (cm_id_priv->state != IW_CM_STATE_CONN_RECV) {
		clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
		wakeup(&cm_id_priv->connect_wait);
		mtx_unlock(&cm_id_priv->lock);
		return (EINVAL);
	}
	cm_id_priv->state = IW_CM_STATE_IDLE;
	mtx_unlock(&cm_id_priv->lock);

	ret = cm_id->device->iwcm->reject(cm_id, private_data,
					  private_data_len);

	mtx_lock(&cm_id_priv->lock);
	clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
	wakeup(&cm_id_priv->connect_wait);
	mtx_unlock(&cm_id_priv->lock);

	return ret;
}

/*
 * CM_ID <-- ESTABLISHED
 *
 * Accepts an inbound connection request and generates an ESTABLISHED
 * event. Callers of iw_cm_disconnect and iw_destroy_cm_id will block
 * until the ESTABLISHED event is received from the provider.
 */
int iw_cm_accept(struct iw_cm_id *cm_id,
		 struct iw_cm_conn_param *iw_param)
{
	struct iwcm_id_private *cm_id_priv;
	struct ib_qp *qp;
	int ret;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	setbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);

	mtx_lock(&cm_id_priv->lock);
	if (cm_id_priv->state != IW_CM_STATE_CONN_RECV) {
		clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
		wakeup(&cm_id_priv->connect_wait);
		mtx_unlock(&cm_id_priv->lock);
		
		return (EINVAL);
	}
	/* Get the ib_qp given the QPN */
	qp = cm_id->device->iwcm->get_qp(cm_id->device, iw_param->qpn);
	if (!qp) {
		mtx_unlock(&cm_id_priv->lock);
		return (EINVAL);
	}
	cm_id->device->iwcm->add_ref(qp);
	cm_id_priv->qp = qp;
	mtx_unlock(&cm_id_priv->lock);

	ret = cm_id->device->iwcm->accept(cm_id, iw_param);
	if (ret) {
		/* An error on accept precludes provider events */
		PANIC_IF(cm_id_priv->state != IW_CM_STATE_CONN_RECV);
		cm_id_priv->state = IW_CM_STATE_IDLE;
		mtx_lock(&cm_id_priv->lock);
		if (cm_id_priv->qp) {
			cm_id->device->iwcm->rem_ref(qp);
			cm_id_priv->qp = NULL;
		}
		clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
		wakeup(&cm_id_priv->connect_wait);
		mtx_unlock(&cm_id_priv->lock);
	}

	return ret;
}

/*
 * Active Side: CM_ID <-- CONN_SENT
 *
 * If successful, results in the generation of a CONNECT_REPLY
 * event. iw_cm_disconnect and iw_cm_destroy will block until the
 * CONNECT_REPLY event is received from the provider.
 */
int iw_cm_connect(struct iw_cm_id *cm_id, struct iw_cm_conn_param *iw_param)
{
	struct iwcm_id_private *cm_id_priv;
	int ret;
	struct ib_qp *qp;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);

	ret = alloc_work_entries(cm_id_priv, 4);
	if (ret)
		return ret;

	setbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
	mtx_lock(&cm_id_priv->lock);

	if (cm_id_priv->state != IW_CM_STATE_IDLE) {
		clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT); 
		wakeup(&cm_id_priv->connect_wait);
		mtx_unlock(&cm_id_priv->lock);
		
		return (EINVAL);
	}

	/* Get the ib_qp given the QPN */
	qp = cm_id->device->iwcm->get_qp(cm_id->device, iw_param->qpn);
	if (!qp) {
		mtx_unlock(&cm_id_priv->lock);
		return (EINVAL);
	}
	cm_id->device->iwcm->add_ref(qp);
	cm_id_priv->qp = qp;
	cm_id_priv->state = IW_CM_STATE_CONN_SENT;
	mtx_unlock(&cm_id_priv->lock);

	ret = cm_id->device->iwcm->connect(cm_id, iw_param);
	if (ret) {
		mtx_lock(&cm_id_priv->lock);
		if (cm_id_priv->qp) {
			cm_id->device->iwcm->rem_ref(qp);
			cm_id_priv->qp = NULL;
		}
		PANIC_IF(cm_id_priv->state != IW_CM_STATE_CONN_SENT);
		cm_id_priv->state = IW_CM_STATE_IDLE;
		clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
		wakeup(&cm_id_priv->connect_wait);
		mtx_unlock(&cm_id_priv->lock);
		
	}

	return ret;
}

/*
 * Passive Side: new CM_ID <-- CONN_RECV
 *
 * Handles an inbound connect request. The function creates a new
 * iw_cm_id to represent the new connection and inherits the client
 * callback function and other attributes from the listening parent.
 *
 * The work item contains a pointer to the listen_cm_id and the event. The
 * listen_cm_id contains the client cm_handler, context and
 * device. These are copied when the device is cloned. The event
 * contains the new four tuple.
 *
 * An error on the child should not affect the parent, so this
 * function does not return a value.
 */
static void cm_conn_req_handler(struct iwcm_id_private *listen_id_priv,
				struct iw_cm_event *iw_event)
{
	struct iw_cm_id *cm_id;
	struct iwcm_id_private *cm_id_priv;
	int ret;

	/*
	 * The provider should never generate a connection request
	 * event with a bad status.
	 */
	PANIC_IF(iw_event->status);

	/*
	 * We could be destroying the listening id. If so, ignore this
	 * upcall.
	 */
	mtx_lock(&listen_id_priv->lock);
	if (listen_id_priv->state != IW_CM_STATE_LISTEN) {
		mtx_unlock(&listen_id_priv->lock);
		goto out;
	}
	mtx_unlock(&listen_id_priv->lock);

	cm_id = iw_create_cm_id(listen_id_priv->id.device,
				iw_event->so,
				listen_id_priv->id.cm_handler,
				listen_id_priv->id.context);
	/* If the cm_id could not be created, ignore the request */
	if (IS_ERR(cm_id))
		goto out;

	cm_id->provider_data = iw_event->provider_data;
	cm_id->local_addr = iw_event->local_addr;
	cm_id->remote_addr = iw_event->remote_addr;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	cm_id_priv->state = IW_CM_STATE_CONN_RECV;

	ret = alloc_work_entries(cm_id_priv, 3);
	if (ret) {
		iw_cm_reject(cm_id, NULL, 0);
		iw_destroy_cm_id(cm_id);
		goto out;
	}

	/* Call the client CM handler */
	ret = cm_id->cm_handler(cm_id, iw_event);
	if (ret) {
		iw_cm_reject(cm_id, NULL, 0);
		setbit(&cm_id_priv->flags, IWCM_F_CALLBACK_DESTROY);
		
		destroy_cm_id(cm_id);
		if (atomic_load_acq_int(&cm_id_priv->refcount)==0)
			free_cm_id(cm_id_priv);
	}

out:
	if (iw_event->private_data_len)
		free(iw_event->private_data, M_DEVBUF);
}

/*
 * Passive Side: CM_ID <-- ESTABLISHED
 *
 * The provider generated an ESTABLISHED event which means that
 * the MPA negotion has completed successfully and we are now in MPA
 * FPDU mode.
 *
 * This event can only be received in the CONN_RECV state. If the
 * remote peer closed, the ESTABLISHED event would be received followed
 * by the CLOSE event. If the app closes, it will block until we wake
 * it up after processing this event.
 */
static int cm_conn_est_handler(struct iwcm_id_private *cm_id_priv,
			       struct iw_cm_event *iw_event)
{
	int ret;

	mtx_lock(&cm_id_priv->lock);

	/*
	 * We clear the CONNECT_WAIT bit here to allow the callback
	 * function to call iw_cm_disconnect. Calling iw_destroy_cm_id
	 * from a callback handler is not allowed.
	 */
	clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
	PANIC_IF(cm_id_priv->state != IW_CM_STATE_CONN_RECV);
	cm_id_priv->state = IW_CM_STATE_ESTABLISHED;
	ret = cm_id_priv->id.cm_handler(&cm_id_priv->id, iw_event);
	wakeup(&cm_id_priv->connect_wait);
	mtx_unlock(&cm_id_priv->lock);

	return ret;
}

/*
 * Active Side: CM_ID <-- ESTABLISHED
 *
 * The app has called connect and is waiting for the established event to
 * post it's requests to the server. This event will wake up anyone
 * blocked in iw_cm_disconnect or iw_destroy_id.
 */
static int cm_conn_rep_handler(struct iwcm_id_private *cm_id_priv,
			       struct iw_cm_event *iw_event)
{
	int ret;

	mtx_lock(&cm_id_priv->lock);
	/*
	 * Clear the connect wait bit so a callback function calling
	 * iw_cm_disconnect will not wait and deadlock this thread
	 */
	clrbit(&cm_id_priv->flags, IWCM_F_CONNECT_WAIT);
	PANIC_IF(cm_id_priv->state != IW_CM_STATE_CONN_SENT);
	if (iw_event->status == IW_CM_EVENT_STATUS_ACCEPTED) {
		cm_id_priv->id.local_addr = iw_event->local_addr;
		cm_id_priv->id.remote_addr = iw_event->remote_addr;
		cm_id_priv->state = IW_CM_STATE_ESTABLISHED;
	} else {
		/* REJECTED or RESET */
		cm_id_priv->id.device->iwcm->rem_ref(cm_id_priv->qp);
		cm_id_priv->qp = NULL;
		cm_id_priv->state = IW_CM_STATE_IDLE;
	}
	mtx_unlock(&cm_id_priv->lock);
	ret = cm_id_priv->id.cm_handler(&cm_id_priv->id, iw_event);

	mtx_lock(&cm_id_priv->lock);
	if (iw_event->private_data_len)
		free(iw_event->private_data, M_DEVBUF);

	/* Wake up waiters on connect complete */
	wakeup(&cm_id_priv->connect_wait);
	mtx_unlock(&cm_id_priv->lock);

	return ret;
}

/*
 * CM_ID <-- CLOSING
 *
 * If in the ESTABLISHED state, move to CLOSING.
 */
static void cm_disconnect_handler(struct iwcm_id_private *cm_id_priv,
				  struct iw_cm_event *iw_event)
{

	mtx_lock(&cm_id_priv->lock);
	if (cm_id_priv->state == IW_CM_STATE_ESTABLISHED)
		cm_id_priv->state = IW_CM_STATE_CLOSING;
	mtx_unlock(&cm_id_priv->lock);
}

/*
 * CM_ID <-- IDLE
 *
 * If in the ESTBLISHED or CLOSING states, the QP will have have been
 * moved by the provider to the ERR state. Disassociate the CM_ID from
 * the QP,  move to IDLE, and remove the 'connected' reference.
 *
 * If in some other state, the cm_id was destroyed asynchronously.
 * This is the last reference that will result in waking up
 * the app thread blocked in iw_destroy_cm_id.
 */
static int cm_close_handler(struct iwcm_id_private *cm_id_priv,
				  struct iw_cm_event *iw_event)
{
	int ret = 0;
	mtx_lock(&cm_id_priv->lock);

	if (cm_id_priv->qp) {
		cm_id_priv->id.device->iwcm->rem_ref(cm_id_priv->qp);
		cm_id_priv->qp = NULL;
	}
	switch (cm_id_priv->state) {
	case IW_CM_STATE_ESTABLISHED:
	case IW_CM_STATE_CLOSING:
		cm_id_priv->state = IW_CM_STATE_IDLE;
		mtx_unlock(&cm_id_priv->lock);
		ret = cm_id_priv->id.cm_handler(&cm_id_priv->id, iw_event);
		mtx_lock(&cm_id_priv->lock);
		break;
	case IW_CM_STATE_DESTROYING:
		break;
	default:
		panic("just cuz");
	}
	mtx_unlock(&cm_id_priv->lock);

	return ret;
}

static int process_event(struct iwcm_id_private *cm_id_priv,
			 struct iw_cm_event *iw_event)
{
	int ret = 0;

	switch (iw_event->event) {
	case IW_CM_EVENT_CONNECT_REQUEST:
		cm_conn_req_handler(cm_id_priv, iw_event);
		break;
	case IW_CM_EVENT_CONNECT_REPLY:
		ret = cm_conn_rep_handler(cm_id_priv, iw_event);
		break;
	case IW_CM_EVENT_ESTABLISHED:
		ret = cm_conn_est_handler(cm_id_priv, iw_event);
		break;
	case IW_CM_EVENT_DISCONNECT:
		cm_disconnect_handler(cm_id_priv, iw_event);
		break;
	case IW_CM_EVENT_CLOSE:
		ret = cm_close_handler(cm_id_priv, iw_event);
		break;
	default:
		panic("just cuz");
	}

	return ret;
}

/*
 * Process events on the work_list for the cm_id. If the callback
 * function requests that the cm_id be deleted, a flag is set in the
 * cm_id flags to indicate that when the last reference is
 * removed, the cm_id is to be destroyed. This is necessary to
 * distinguish between an object that will be destroyed by the app
 * thread asleep on the destroy_comp list vs. an object destroyed
 * here synchronously when the last reference is removed.
 */
static void cm_work_handler(void *context, int pending)
{
	struct iwcm_work *work = context;
	struct iw_cm_event levent;
	struct iwcm_id_private *cm_id_priv = work->cm_id;
	int empty;
	int ret = 0;

	mtx_lock(&cm_id_priv->lock);
	empty = TAILQ_EMPTY(&cm_id_priv->work_list);
	while (!empty) {
		work = TAILQ_FIRST(&cm_id_priv->work_list);
		TAILQ_REMOVE(&cm_id_priv->work_list, work, list);
		empty = TAILQ_EMPTY(&cm_id_priv->work_list);
		levent = work->event;
		put_work(work);
		mtx_unlock(&cm_id_priv->lock);

		ret = process_event(cm_id_priv, &levent);
		if (ret) {
			setbit(&cm_id_priv->flags, IWCM_F_CALLBACK_DESTROY);
			destroy_cm_id(&cm_id_priv->id);
		}
		PANIC_IF(atomic_load_acq_int(&cm_id_priv->refcount)==0);
		if (iwcm_deref_id(cm_id_priv)) {
			if (isset(&cm_id_priv->flags,
				IWCM_F_CALLBACK_DESTROY)) {
				PANIC_IF(!TAILQ_EMPTY(&cm_id_priv->work_list));
				free_cm_id(cm_id_priv);
			}
			return;
		}
		mtx_lock(&cm_id_priv->lock);
	}
	mtx_unlock(&cm_id_priv->lock);
}

/*
 * This function is called on interrupt context. Schedule events on
 * the iwcm_wq thread to allow callback functions to downcall into
 * the CM and/or block.  Events are queued to a per-CM_ID
 * work_list. If this is the first event on the work_list, the work
 * element is also queued on the iwcm_wq thread.
 *
 * Each event holds a reference on the cm_id. Until the last posted
 * event has been delivered and processed, the cm_id cannot be
 * deleted.
 *
 * Returns:
 * 	      0	- the event was handled.
 *	ENOMEM	- the event was not handled due to lack of resources.
 */
static int cm_event_handler(struct iw_cm_id *cm_id,
			     struct iw_cm_event *iw_event)
{
	struct iwcm_work *work;
	struct iwcm_id_private *cm_id_priv;
	int ret = 0;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);

	mtx_lock(&cm_id_priv->lock);
	work = get_work(cm_id_priv);
	if (!work) {
		ret = ENOMEM;
		goto out;
	}

        TASK_INIT(&work->task, 0, cm_work_handler, work);
	work->cm_id = cm_id_priv;
	work->event = *iw_event;

	if ((work->event.event == IW_CM_EVENT_CONNECT_REQUEST ||
	     work->event.event == IW_CM_EVENT_CONNECT_REPLY) &&
	    work->event.private_data_len) {
		ret = copy_private_data(&work->event);
		if (ret) {
			put_work(work);
			goto out;
		}
	}

	atomic_add_acq_int(&cm_id_priv->refcount, 1);
	if (TAILQ_EMPTY(&cm_id_priv->work_list)) {
		TAILQ_INSERT_TAIL(&cm_id_priv->work_list, work, list);
		taskqueue_enqueue(iwcm_wq, &work->task);
	} else
		TAILQ_INSERT_TAIL(&cm_id_priv->work_list, work, list);
out:
	mtx_unlock(&cm_id_priv->lock);
	return ret;
}

static int iwcm_init_qp_init_attr(struct iwcm_id_private *cm_id_priv,
				  struct ib_qp_attr *qp_attr,
				  int *qp_attr_mask)
{
	int ret;

	mtx_lock(&cm_id_priv->lock);
	switch (cm_id_priv->state) {
	case IW_CM_STATE_IDLE:
	case IW_CM_STATE_CONN_SENT:
	case IW_CM_STATE_CONN_RECV:
	case IW_CM_STATE_ESTABLISHED:
		*qp_attr_mask = IB_QP_STATE | IB_QP_ACCESS_FLAGS;
		qp_attr->qp_access_flags = IB_ACCESS_LOCAL_WRITE |
					   IB_ACCESS_REMOTE_WRITE|
					   IB_ACCESS_REMOTE_READ;
		ret = 0;
		break;
	default:
		ret = EINVAL;
		break;
	}
	mtx_unlock(&cm_id_priv->lock);
	return ret;
}

static int iwcm_init_qp_rts_attr(struct iwcm_id_private *cm_id_priv,
				  struct ib_qp_attr *qp_attr,
				  int *qp_attr_mask)
{
	int ret;

	mtx_lock(&cm_id_priv->lock);
	switch (cm_id_priv->state) {
	case IW_CM_STATE_IDLE:
	case IW_CM_STATE_CONN_SENT:
	case IW_CM_STATE_CONN_RECV:
	case IW_CM_STATE_ESTABLISHED:
		*qp_attr_mask = 0;
		ret = 0;
		break;
	default:
		ret = EINVAL;
		break;
	}
	mtx_unlock(&cm_id_priv->lock);
	return ret;
}

int iw_cm_init_qp_attr(struct iw_cm_id *cm_id,
		       struct ib_qp_attr *qp_attr,
		       int *qp_attr_mask)
{
	struct iwcm_id_private *cm_id_priv;
	int ret;

	cm_id_priv = container_of(cm_id, struct iwcm_id_private, id);
	switch (qp_attr->qp_state) {
	case IB_QPS_INIT:
	case IB_QPS_RTR:
		ret = iwcm_init_qp_init_attr(cm_id_priv,
					     qp_attr, qp_attr_mask);
		break;
	case IB_QPS_RTS:
		ret = iwcm_init_qp_rts_attr(cm_id_priv,
					    qp_attr, qp_attr_mask);
		break;
	default:
		ret = EINVAL;
		break;
	}
	return ret;
}

static int iw_cm_init(void)
{
	iwcm_wq = taskqueue_create("iw_cm_wq", M_NOWAIT, taskqueue_thread_enqueue, &iwcm_wq);
	if (!iwcm_wq)
		return (ENOMEM);

	taskqueue_start_threads(&iwcm_wq, 1, PI_NET, "iw_cm_wq thread");
	return 0;
}

static void iw_cm_cleanup(void)
{
	taskqueue_free(iwcm_wq);
}

static int 
iw_cm_load(module_t mod, int cmd, void *arg)
{
        int err = 0;

        switch (cmd) {
        case MOD_LOAD:
                printf("Loading rdma_iwcm.\n");

                iw_cm_init();
                break;
        case MOD_QUIESCE:
                break;
        case MOD_UNLOAD:
                printf("Unloading rdma_iwcm.\n");
		iw_cm_cleanup();
                break;
        case MOD_SHUTDOWN:
                break;
        default:
                err = EOPNOTSUPP;
                break;
        }

        return (err);
}

static moduledata_t mod_data = {
	"rdma_iwcm",
	iw_cm_load,
	0
};

MODULE_VERSION(rdma_iwcm, 1);
MODULE_DEPEND(rdma_iwcm, rdma_core, 1, 1, 1);
DECLARE_MODULE(rdma_iwcm, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
