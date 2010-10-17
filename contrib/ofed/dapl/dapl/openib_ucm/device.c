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

#include "openib_osd.h"
#include "dapl.h"
#include "dapl_adapter_util.h"
#include "dapl_ib_util.h"
#include "dapl_osd.h"

#include <stdlib.h>

static void ucm_service_destroy(IN DAPL_HCA *hca);
static int  ucm_service_create(IN DAPL_HCA *hca);

#if defined (_WIN32)
#include "..\..\..\..\..\etc\user\comp_channel.cpp"
#include <rdma\winverbs.h>

static int32_t create_os_signal(IN DAPL_HCA * hca_ptr)
{
	return CompSetInit(&hca_ptr->ib_trans.signal.set);
}

static void destroy_os_signal(IN DAPL_HCA * hca_ptr)
{
	CompSetCleanup(&hca_ptr->ib_trans.signal.set);
}

static int dapls_config_verbs(struct ibv_context *verbs)
{
	verbs->channel.Milliseconds = 0;
	return 0;
}

static int dapls_config_comp_channel(struct ibv_comp_channel *channel)
{
	channel->comp_channel.Milliseconds = 0;
	return 0;
}

#else // _WIN32

static int32_t create_os_signal(IN DAPL_HCA * hca_ptr)
{
	DAPL_SOCKET listen_socket;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(addr);
	int ret;

	listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_socket == DAPL_INVALID_SOCKET)
		return 1;

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(0x7f000001);
	ret = bind(listen_socket, (struct sockaddr *)&addr, sizeof addr);
	if (ret)
		goto err1;

	ret = getsockname(listen_socket, (struct sockaddr *)&addr, &addrlen);
	if (ret)
		goto err1;

	ret = listen(listen_socket, 0);
	if (ret)
		goto err1;

	hca_ptr->ib_trans.signal.scm[1] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (hca_ptr->ib_trans.signal.scm[1] == DAPL_INVALID_SOCKET)
		goto err1;

	ret = connect(hca_ptr->ib_trans.signal.scm[1], 
		      (struct sockaddr *)&addr, sizeof(addr));
	if (ret)
		goto err2;

	hca_ptr->ib_trans.signal.scm[0] = accept(listen_socket, NULL, NULL);
	if (hca_ptr->ib_trans.signal.scm[0] == DAPL_INVALID_SOCKET)
		goto err2;

	closesocket(listen_socket);
	return 0;

      err2:
	closesocket(hca_ptr->ib_trans.signal.scm[1]);
      err1:
	closesocket(listen_socket);
	return 1;
}

static void destroy_os_signal(IN DAPL_HCA * hca_ptr)
{
	closesocket(hca_ptr->ib_trans.signal.scm[0]);
	closesocket(hca_ptr->ib_trans.signal.scm[1]);
}

static int dapls_config_fd(int fd)
{
	int opts;

	opts = fcntl(fd, F_GETFL);
	if (opts < 0 || fcntl(fd, F_SETFL, opts | O_NONBLOCK) < 0) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " dapls_config_fd: fcntl on fd %d ERR %d %s\n",
			 fd, opts, strerror(errno));
		return errno;
	}

	return 0;
}

static int dapls_config_verbs(struct ibv_context *verbs)
{
	return dapls_config_fd(verbs->async_fd);
}

static int dapls_config_comp_channel(struct ibv_comp_channel *channel)
{
	return dapls_config_fd(channel->fd);
}

#endif

/*
 * dapls_ib_init, dapls_ib_release
 *
 * Initialize Verb related items for device open
 *
 * Input:
 * 	none
 *
 * Output:
 *	none
 *
 * Returns:
 * 	0 success, -1 error
 *
 */
int32_t dapls_ib_init(void)
{
	return 0;
}

int32_t dapls_ib_release(void)
{
	return 0;
}

/*
 * dapls_ib_open_hca
 *
 * Open HCA
 *
 * Input:
 *      *hca_name         pointer to provider device name
 *      *ib_hca_handle_p  pointer to provide HCA handle
 *
 * Output:
 *      none
 *
 * Return:
 *      DAT_SUCCESS
 *      dapl_convert_errno
 *
 */
DAT_RETURN dapls_ib_open_hca(IN IB_HCA_NAME hca_name, IN DAPL_HCA * hca_ptr)
{
	struct ibv_device **dev_list;
	struct ibv_port_attr port_attr;
	int i;
	DAT_RETURN dat_status;

	/* Get list of all IB devices, find match, open */
	dev_list = ibv_get_device_list(NULL);
	if (!dev_list) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     " open_hca: ibv_get_device_list() failed\n",
			     hca_name);
		return DAT_INTERNAL_ERROR;
	}

	for (i = 0; dev_list[i]; ++i) {
		hca_ptr->ib_trans.ib_dev = dev_list[i];
		if (!strcmp(ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			    hca_name))
			goto found;
	}

	dapl_log(DAPL_DBG_TYPE_ERR,
		 " open_hca: device %s not found\n", hca_name);
	goto err;

found:

	hca_ptr->ib_hca_handle = ibv_open_device(hca_ptr->ib_trans.ib_dev);
	if (!hca_ptr->ib_hca_handle) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: dev open failed for %s, err=%s\n",
			 ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			 strerror(errno));
		goto err;
	}
	hca_ptr->ib_trans.ib_ctx = hca_ptr->ib_hca_handle;
	dapls_config_verbs(hca_ptr->ib_hca_handle);
	
	/* get lid for this hca-port, network order */
	if (ibv_query_port(hca_ptr->ib_hca_handle,
			   (uint8_t)hca_ptr->port_num, &port_attr)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: get lid ERR for %s, err=%s\n",
			 ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			 strerror(errno));
		goto err;
	} else {
		hca_ptr->ib_trans.addr.ib.lid = htons(port_attr.lid);
	}

	/* get gid for this hca-port, network order */
	if (ibv_query_gid(hca_ptr->ib_hca_handle,
			  (uint8_t) hca_ptr->port_num, 0,
			  (union ibv_gid *)&hca_ptr->ib_trans.addr.ib.gid)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: query GID ERR for %s, err=%s\n",
			 ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
			 strerror(errno));
		goto err;
	}

	/* set RC tunables via enviroment or default */
	hca_ptr->ib_trans.max_inline_send =
	    dapl_os_get_env_val("DAPL_MAX_INLINE", INLINE_SEND_IB_DEFAULT);
	hca_ptr->ib_trans.ack_retry =
	    dapl_os_get_env_val("DAPL_ACK_RETRY", DCM_ACK_RETRY);
	hca_ptr->ib_trans.ack_timer =
	    dapl_os_get_env_val("DAPL_ACK_TIMER", DCM_ACK_TIMER);
	hca_ptr->ib_trans.rnr_retry =
	    dapl_os_get_env_val("DAPL_RNR_RETRY", DCM_RNR_RETRY);
	hca_ptr->ib_trans.rnr_timer =
	    dapl_os_get_env_val("DAPL_RNR_TIMER", DCM_RNR_TIMER);
	hca_ptr->ib_trans.global =
	    dapl_os_get_env_val("DAPL_GLOBAL_ROUTING", DCM_GLOBAL);
	hca_ptr->ib_trans.hop_limit =
	    dapl_os_get_env_val("DAPL_HOP_LIMIT", DCM_HOP_LIMIT);
	hca_ptr->ib_trans.tclass =
	    dapl_os_get_env_val("DAPL_TCLASS", DCM_TCLASS);
	hca_ptr->ib_trans.mtu =
	    dapl_ib_mtu(dapl_os_get_env_val("DAPL_IB_MTU", DCM_IB_MTU));

	/* initialize CM list, LISTEN, SND queue, PSP array, locks */
	if ((dapl_os_lock_init(&hca_ptr->ib_trans.lock)) != DAT_SUCCESS)
		goto err;
	
	if ((dapl_os_lock_init(&hca_ptr->ib_trans.llock)) != DAT_SUCCESS)
		goto err;
	
	if ((dapl_os_lock_init(&hca_ptr->ib_trans.slock)) != DAT_SUCCESS)
		goto err;

	if ((dapl_os_lock_init(&hca_ptr->ib_trans.plock)) != DAT_SUCCESS)
		goto err;

	/* EVD events without direct CQ channels, CNO support */
	hca_ptr->ib_trans.ib_cq =
	    ibv_create_comp_channel(hca_ptr->ib_hca_handle);
	if (hca_ptr->ib_trans.ib_cq == NULL) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: ibv_create_comp_channel ERR %s\n",
			 strerror(errno));
		goto bail;
	}
	dapls_config_comp_channel(hca_ptr->ib_trans.ib_cq);

	/* initialize CM and listen lists on this HCA uCM QP */
	dapl_llist_init_head(&hca_ptr->ib_trans.list);
	dapl_llist_init_head(&hca_ptr->ib_trans.llist);

	/* create uCM qp services */
	if (ucm_service_create(hca_ptr))
		goto bail;

	if (create_os_signal(hca_ptr)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: failed to init cr pipe - %s\n",
			 strerror(errno));
		goto bail;
	}

	/* create thread to process inbound connect request */
	hca_ptr->ib_trans.cm_state = IB_THREAD_INIT;
	dat_status = dapl_os_thread_create(cm_thread,
					   (void *)hca_ptr,
					   &hca_ptr->ib_trans.thread);
	if (dat_status != DAT_SUCCESS) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			 " open_hca: failed to create thread\n");
		goto bail;
	}

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: devname %s, ctx %p port %d, hostname_IP %s\n",
		     ibv_get_device_name(hca_ptr->ib_trans.ib_dev),
		     hca_ptr->ib_hca_handle,
		     hca_ptr->port_num, 
		     inet_ntoa(((struct sockaddr_in *)
			       &hca_ptr->hca_address)->sin_addr));
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
		     " open_hca: QPN 0x%x LID 0x%x GID Subnet 0x" F64x ""
		     " ID 0x" F64x "\n", 
		     ntohl(hca_ptr->ib_trans.addr.ib.qpn),
		     ntohs(hca_ptr->ib_trans.addr.ib.lid), 
		     (unsigned long long)
		     ntohll(*(uint64_t*)&hca_ptr->ib_trans.addr.ib.gid[0]),
		     (unsigned long long)
		     ntohll(*(uint64_t*)&hca_ptr->ib_trans.addr.ib.gid[8]));

	/* save LID, GID, QPN, PORT address information, for ia_queries */
	/* Set AF_INET6 to insure callee address storage of 28 bytes */
	hca_ptr->ib_trans.hca = hca_ptr;
	hca_ptr->ib_trans.addr.ib.family = AF_INET6; 
	hca_ptr->ib_trans.addr.ib.qp_type = IBV_QPT_UD;
	memcpy(&hca_ptr->hca_address, 
	       &hca_ptr->ib_trans.addr, 
	       sizeof(union dcm_addr));

	ibv_free_device_list(dev_list);

	/* wait for cm_thread */
	while (hca_ptr->ib_trans.cm_state != IB_THREAD_RUN) 
		dapl_os_sleep_usec(1000);

	return dat_status;

bail:
	ucm_service_destroy(hca_ptr);
	ibv_close_device(hca_ptr->ib_hca_handle);
	hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
      
err:
	ibv_free_device_list(dev_list);
	return DAT_INTERNAL_ERROR;
}

/*
 * dapls_ib_close_hca
 *
 * Open HCA
 *
 * Input:
 *      DAPL_HCA   provide CA handle
 *
 * Output:
 *      none
 *
 * Return:
 *      DAT_SUCCESS
 *	dapl_convert_errno 
 *
 */
DAT_RETURN dapls_ib_close_hca(IN DAPL_HCA * hca_ptr)
{
	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " close_hca: %p\n", hca_ptr);

	if (hca_ptr->ib_trans.cm_state == IB_THREAD_RUN) {
		hca_ptr->ib_trans.cm_state = IB_THREAD_CANCEL;
		dapls_thread_signal(&hca_ptr->ib_trans.signal);
		while (hca_ptr->ib_trans.cm_state != IB_THREAD_EXIT) {
			dapl_dbg_log(DAPL_DBG_TYPE_UTIL,
				" close_hca: waiting for cr_thread\n");
			dapls_thread_signal(&hca_ptr->ib_trans.signal);
			dapl_os_sleep_usec(1000);
		}
	}

	dapl_os_lock_destroy(&hca_ptr->ib_trans.lock);
	dapl_os_lock_destroy(&hca_ptr->ib_trans.llock);
	destroy_os_signal(hca_ptr);
	ucm_service_destroy(hca_ptr);

	if (hca_ptr->ib_hca_handle != IB_INVALID_HANDLE) {
		if (ibv_close_device(hca_ptr->ib_hca_handle))
			return (dapl_convert_errno(errno, "ib_close_device"));
		hca_ptr->ib_hca_handle = IB_INVALID_HANDLE;
	}

	return (DAT_SUCCESS);
}

/* Create uCM endpoint services, allocate remote_ah's array */
static void ucm_service_destroy(IN DAPL_HCA *hca)
{
	ib_hca_transport_t *tp = &hca->ib_trans;
	int msg_size = sizeof(ib_cm_msg_t);

	if (tp->mr_sbuf)
		ibv_dereg_mr(tp->mr_sbuf);

	if (tp->mr_rbuf)
		ibv_dereg_mr(tp->mr_rbuf);

	if (tp->qp)
		ibv_destroy_qp(tp->qp);

	if (tp->scq)
		ibv_destroy_cq(tp->scq);

	if (tp->rcq)
		ibv_destroy_cq(tp->rcq);

	if (tp->rch)
		ibv_destroy_comp_channel(tp->rch);

 	if (tp->ah) {
		int i;

		for (i = 0;i < 0xffff; i++) {
			if (tp->ah[i])
				ibv_destroy_ah(tp->ah[i]);
		}
		dapl_os_free(tp->ah, (sizeof(*tp->ah) * 0xffff));
	}

	if (tp->pd)
		ibv_dealloc_pd(tp->pd);

	if (tp->sid)
		dapl_os_free(tp->sid, (sizeof(*tp->sid) * 0xffff));

	if (tp->rbuf)
		dapl_os_free(tp->rbuf, (msg_size * tp->qpe));

	if (tp->sbuf)
		dapl_os_free(tp->sbuf, (msg_size * tp->qpe));
}

static int ucm_service_create(IN DAPL_HCA *hca)
{
        struct ibv_qp_init_attr qp_create;
	ib_hca_transport_t *tp = &hca->ib_trans;
	struct ibv_recv_wr recv_wr, *recv_err;
        struct ibv_sge sge;
	int i, mlen = sizeof(ib_cm_msg_t);
	int hlen = sizeof(struct ibv_grh); /* hdr included with UD recv */

	dapl_dbg_log(DAPL_DBG_TYPE_UTIL, " ucm_create: \n");

	/* setup CM timers and queue sizes */
	tp->retries = dapl_os_get_env_val("DAPL_UCM_RETRY", DCM_RETRY_CNT);
	tp->rep_time = dapl_os_get_env_val("DAPL_UCM_REP_TIME", DCM_REP_TIME);
	tp->rtu_time = dapl_os_get_env_val("DAPL_UCM_RTU_TIME", DCM_RTU_TIME);
	tp->cm_timer = DAPL_MIN(tp->rep_time,tp->rtu_time);
	tp->qpe = dapl_os_get_env_val("DAPL_UCM_QP_SIZE", DCM_QP_SIZE);
	tp->cqe = dapl_os_get_env_val("DAPL_UCM_CQ_SIZE", DCM_CQ_SIZE);
	tp->pd = ibv_alloc_pd(hca->ib_hca_handle);
        if (!tp->pd) 
                goto bail;
        
        dapl_log(DAPL_DBG_TYPE_UTIL,
                        " create_service: pd %p ctx %p handle 0x%x\n",
                         tp->pd, tp->pd->context, tp->pd->handle);

    	tp->rch = ibv_create_comp_channel(hca->ib_hca_handle);
	if (!tp->rch) 
		goto bail;

	tp->scq = ibv_create_cq(hca->ib_hca_handle, tp->cqe, hca, NULL, 0);
	if (!tp->scq) 
		goto bail;
        
	tp->rcq = ibv_create_cq(hca->ib_hca_handle, tp->cqe, hca, tp->rch, 0);
	if (!tp->rcq) 
		goto bail;

	if(ibv_req_notify_cq(tp->rcq, 0))
		goto bail; 
 
	dapl_os_memzero((void *)&qp_create, sizeof(qp_create));
	qp_create.qp_type = IBV_QPT_UD;
	qp_create.send_cq = tp->scq;
	qp_create.recv_cq = tp->rcq;
	qp_create.cap.max_send_wr = qp_create.cap.max_recv_wr = tp->qpe;
	qp_create.cap.max_send_sge = qp_create.cap.max_recv_sge = 1;
	qp_create.cap.max_inline_data = tp->max_inline_send;
	qp_create.qp_context = (void *)hca;

	tp->qp = ibv_create_qp(tp->pd, &qp_create);
	if (!tp->qp) 
                goto bail;

	tp->ah = (ib_ah_handle_t*) dapl_os_alloc(sizeof(ib_ah_handle_t) * 0xffff);
	tp->sid = (uint8_t*) dapl_os_alloc(sizeof(uint8_t) * 0xffff);
	tp->rbuf = (void*) dapl_os_alloc((mlen + hlen) * tp->qpe);
	tp->sbuf = (void*) dapl_os_alloc(mlen * tp->qpe);

	if (!tp->ah || !tp->rbuf || !tp->sbuf || !tp->sid)
		goto bail;

	(void)dapl_os_memzero(tp->ah, (sizeof(ib_ah_handle_t) * 0xffff));
	(void)dapl_os_memzero(tp->sid, (sizeof(uint8_t) * 0xffff));
	tp->sid[0] = 1; /* resv slot 0, 0 == no ports available */
	(void)dapl_os_memzero(tp->rbuf, ((mlen + hlen) * tp->qpe));
	(void)dapl_os_memzero(tp->sbuf, (mlen * tp->qpe));

	tp->mr_sbuf = ibv_reg_mr(tp->pd, tp->sbuf, 
				 (mlen * tp->qpe),
				 IBV_ACCESS_LOCAL_WRITE);
	if (!tp->mr_sbuf)
		goto bail;

	tp->mr_rbuf = ibv_reg_mr(tp->pd, tp->rbuf, 
				 ((mlen + hlen) * tp->qpe),
				 IBV_ACCESS_LOCAL_WRITE);
	if (!tp->mr_rbuf)
		goto bail;
	
	/* modify UD QP: init, rtr, rts */
	if ((dapls_modify_qp_ud(hca, tp->qp)) != DAT_SUCCESS)
		goto bail;

	/* post receive buffers, setup head, tail pointers */
	recv_wr.next = NULL;
	recv_wr.sg_list = &sge;
	recv_wr.num_sge = 1;
	sge.length = mlen + hlen;
	sge.lkey = tp->mr_rbuf->lkey;

	for (i = 0; i < tp->qpe; i++) {
		recv_wr.wr_id = 
			(uintptr_t)((char *)&tp->rbuf[i] + 
				    sizeof(struct ibv_grh));
		sge.addr = (uintptr_t) &tp->rbuf[i];
		if (ibv_post_recv(tp->qp, &recv_wr, &recv_err))
			goto bail;
	}

	/* save qp_num as part of ia_address, network order */
	tp->addr.ib.qpn = htonl(tp->qp->qp_num);
        return 0;
bail:
	dapl_log(DAPL_DBG_TYPE_ERR,
		 " ucm_create_services: ERR %s\n", strerror(errno));
	ucm_service_destroy(hca);
	return -1;
}

void ucm_async_event(struct dapl_hca *hca)
{
	struct ibv_async_event event;
	struct _ib_hca_transport *tp = &hca->ib_trans;

	dapl_log(DAPL_DBG_TYPE_WARN, " async_event(%p)\n", hca);

	if (!ibv_get_async_event(hca->ib_hca_handle, &event)) {

		switch (event.event_type) {
		case IBV_EVENT_CQ_ERR:
		{
			struct dapl_ep *evd_ptr =
				event.element.cq->cq_context;

			dapl_log(DAPL_DBG_TYPE_ERR,
				 "dapl async_event CQ (%p) ERR %d\n",
				 evd_ptr, event.event_type);

			/* report up if async callback still setup */
			if (tp->async_cq_error)
				tp->async_cq_error(hca->ib_hca_handle,
						   event.element.cq,
						   &event, (void *)evd_ptr);
			break;
		}
		case IBV_EVENT_COMM_EST:
		{
			/* Received msgs on connected QP before RTU */
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " async_event COMM_EST(%p) rdata beat RTU\n",
				 event.element.qp);

			break;
		}
		case IBV_EVENT_QP_FATAL:
		case IBV_EVENT_QP_REQ_ERR:
		case IBV_EVENT_QP_ACCESS_ERR:
		case IBV_EVENT_QP_LAST_WQE_REACHED:
		case IBV_EVENT_SRQ_ERR:
		case IBV_EVENT_SRQ_LIMIT_REACHED:
		case IBV_EVENT_SQ_DRAINED:
		{
			struct dapl_ep *ep_ptr =
				event.element.qp->qp_context;

			dapl_log(DAPL_DBG_TYPE_ERR,
				 "dapl async_event QP (%p) ERR %d\n",
				 ep_ptr, event.event_type);

			/* report up if async callback still setup */
			if (tp->async_qp_error)
				tp->async_qp_error(hca->ib_hca_handle,
						   ep_ptr->qp_handle,
						   &event, (void *)ep_ptr);
			break;
		}
		case IBV_EVENT_PATH_MIG:
		case IBV_EVENT_PATH_MIG_ERR:
		case IBV_EVENT_DEVICE_FATAL:
		case IBV_EVENT_PORT_ACTIVE:
		case IBV_EVENT_PORT_ERR:
		case IBV_EVENT_LID_CHANGE:
		case IBV_EVENT_PKEY_CHANGE:
		case IBV_EVENT_SM_CHANGE:
		{
			dapl_log(DAPL_DBG_TYPE_WARN,
				 "dapl async_event: DEV ERR %d\n",
				 event.event_type);

			/* report up if async callback still setup */
			if (tp->async_unafiliated)
				tp->async_unafiliated(hca->ib_hca_handle,
						      &event,
						      tp->async_un_ctx);
			break;
		}
		case IBV_EVENT_CLIENT_REREGISTER:
			/* no need to report this event this time */
			dapl_log(DAPL_DBG_TYPE_UTIL,
				 " async_event: IBV_CLIENT_REREGISTER\n");
			break;

		default:
			dapl_log(DAPL_DBG_TYPE_WARN,
				 "dapl async_event: %d UNKNOWN\n",
				 event.event_type);
			break;

		}
		ibv_ack_async_event(&event);
	}
}

