/*
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

/*
 * dapl_ib_qp_alloc
 *
 * Alloc a QP
 *
 * Input:
 *	*ep_ptr		pointer to EP INFO
 *	ib_hca_handle	provider HCA handle
 *	ib_pd_handle	provider protection domain handle
 *	cq_recv		provider recv CQ handle
 *	cq_send		provider send CQ handle
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
dapls_ib_qp_alloc(IN DAPL_IA * ia_ptr,
		  IN DAPL_EP * ep_ptr, IN DAPL_EP * ep_ctx_ptr)
{
	DAT_EP_ATTR *attr;
	DAPL_EVD *rcv_evd, *req_evd;
	ib_cq_handle_t rcv_cq, req_cq;
	ib_pd_handle_t ib_pd_handle;
	struct ibv_qp_init_attr qp_create;
#ifdef _OPENIB_CMA_
	dp_ib_cm_handle_t conn;
#endif
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " qp_alloc: ia_ptr %p ep_ptr %p ep_ctx_ptr %p\n",
		     ia_ptr, ep_ptr, ep_ctx_ptr);

	attr = &ep_ptr->param.ep_attr;
	ib_pd_handle = ((DAPL_PZ *) ep_ptr->param.pz_handle)->pd_handle;
	rcv_evd = (DAPL_EVD *) ep_ptr->param.recv_evd_handle;
	req_evd = (DAPL_EVD *) ep_ptr->param.request_evd_handle;

	/* 
	 * DAT allows usage model of EP's with no EVD's but IB does not. 
	 * Create a CQ with zero entries under the covers to support and 
	 * catch any invalid posting. 
	 */
	if (rcv_evd != DAT_HANDLE_NULL)
		rcv_cq = rcv_evd->ib_cq_handle;
	else if (!ia_ptr->hca_ptr->ib_trans.ib_cq_empty)
		rcv_cq = ia_ptr->hca_ptr->ib_trans.ib_cq_empty;
	else {
		struct ibv_comp_channel *channel;

		channel = ibv_create_comp_channel(ia_ptr->hca_ptr->ib_hca_handle);
		if (!channel)
			return (dapl_convert_errno(ENOMEM, "create_cq"));
		  
		/* Call IB verbs to create CQ */
		rcv_cq = ibv_create_cq(ia_ptr->hca_ptr->ib_hca_handle,
				       0, NULL, channel, 0);

		if (rcv_cq == IB_INVALID_HANDLE) {
			ibv_destroy_comp_channel(channel);
			return (dapl_convert_errno(ENOMEM, "create_cq"));
		}

		ia_ptr->hca_ptr->ib_trans.ib_cq_empty = rcv_cq;
	}
	if (req_evd != DAT_HANDLE_NULL)
		req_cq = req_evd->ib_cq_handle;
	else
		req_cq = ia_ptr->hca_ptr->ib_trans.ib_cq_empty;

	/* 
	 * IMPLEMENTATION NOTE:
	 * uDAPL allows consumers to post buffers on the EP after creation
	 * and before a connect request (outbound and inbound). This forces
	 * a binding to a device during the hca_open call and requires the
	 * consumer to predetermine which device to listen on or connect from.
	 * This restriction eliminates any option of listening or connecting 
	 * over multiple devices. uDAPL should add API's to resolve addresses 
	 * and bind to the device at the approriate time (before connect 
	 * and after CR arrives). Discovery should happen at connection time 
	 * based on addressing and not on static configuration during open.
	 */

#ifdef _OPENIB_CMA_
	/* Allocate CM and initialize lock */
	if ((conn = dapls_ib_cm_create(ep_ptr)) == NULL)
		return (dapl_convert_errno(ENOMEM, "create_cq"));

	/* open identifies the local device; per DAT specification */
	if (rdma_bind_addr(conn->cm_id,
			   (struct sockaddr *)&ia_ptr->hca_ptr->hca_address))
		return (dapl_convert_errno(EAFNOSUPPORT, "create_cq"));
#endif
	/* Setup attributes and create qp */
	dapl_os_memzero((void *)&qp_create, sizeof(qp_create));
	qp_create.send_cq = req_cq;
	qp_create.cap.max_send_wr = attr->max_request_dtos;
	qp_create.cap.max_send_sge = attr->max_request_iov;
	qp_create.cap.max_inline_data =
	    ia_ptr->hca_ptr->ib_trans.max_inline_send;
	qp_create.qp_type = IBV_QPT_RC;
	qp_create.qp_context = (void *)ep_ptr;

#ifdef DAT_EXTENSIONS 
	if (attr->service_type == DAT_IB_SERVICE_TYPE_UD) {
#ifdef _OPENIB_CMA_
		return (DAT_NOT_IMPLEMENTED);
#endif
		qp_create.qp_type = IBV_QPT_UD;
		if (attr->max_message_size >
		    (128 << ia_ptr->hca_ptr->ib_trans.mtu)) {
			return (DAT_INVALID_PARAMETER | DAT_INVALID_ARG6);
		}
	}
#endif
	
	/* ibv assumes rcv_cq is never NULL, set to req_cq */
	if (rcv_cq == NULL) {
		qp_create.recv_cq = req_cq;
		qp_create.cap.max_recv_wr = 0;
		qp_create.cap.max_recv_sge = 0;
	} else {
		qp_create.recv_cq = rcv_cq;
		qp_create.cap.max_recv_wr = attr->max_recv_dtos;
		qp_create.cap.max_recv_sge = attr->max_recv_iov;
	}

#ifdef _OPENIB_CMA_
	if (rdma_create_qp(conn->cm_id, ib_pd_handle, &qp_create)) {
		dapls_ib_cm_free(conn, ep_ptr);
		return (dapl_convert_errno(errno, "create_qp"));
	}
	ep_ptr->qp_handle = conn->cm_id->qp;
	ep_ptr->cm_handle = conn;
	ep_ptr->qp_state = IBV_QPS_INIT;
		
	/* setup up ep->param to reference the bound local address and port */
	ep_ptr->param.local_ia_address_ptr = 
		&conn->cm_id->route.addr.src_addr;
	ep_ptr->param.local_port_qual = rdma_get_src_port(conn->cm_id);
#else
	ep_ptr->qp_handle = ibv_create_qp(ib_pd_handle, &qp_create);
	if (!ep_ptr->qp_handle)
		return (dapl_convert_errno(ENOMEM, "create_qp"));
		
	/* Setup QP attributes for INIT state on the way out */
	if (dapls_modify_qp_state(ep_ptr->qp_handle,
				  IBV_QPS_INIT, 0, 0, 0) != DAT_SUCCESS) {
		ibv_destroy_qp(ep_ptr->qp_handle);
		ep_ptr->qp_handle = IB_INVALID_HANDLE;
		return DAT_INTERNAL_ERROR;
	}
#endif
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " qp_alloc: qpn %p sq %d,%d rq %d,%d\n",
		     ep_ptr->qp_handle->qp_num,
		     qp_create.cap.max_send_wr, qp_create.cap.max_send_sge,
		     qp_create.cap.max_recv_wr, qp_create.cap.max_recv_sge);

	return DAT_SUCCESS;
}

/*
 * dapl_ib_qp_free
 *
 * Free a QP
 *
 * Input:
 *	ia_handle	IA handle
 *	*ep_ptr		pointer to EP INFO
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	DAT_SUCCESS
 *  dapl_convert_errno
 *
 */
DAT_RETURN dapls_ib_qp_free(IN DAPL_IA * ia_ptr, IN DAPL_EP * ep_ptr)
{
	dapl_dbg_log(DAPL_DBG_TYPE_EP, " qp_free:  ep_ptr %p qp %p\n",
		     ep_ptr, ep_ptr->qp_handle);

	if (ep_ptr->cm_handle != NULL) {
		dapls_ib_cm_free(ep_ptr->cm_handle, ep_ptr);
	}
	
	if (ep_ptr->qp_handle != NULL) {
		/* force error state to flush queue, then destroy */
		dapls_modify_qp_state(ep_ptr->qp_handle, IBV_QPS_ERR, 0,0,0);

		if (ibv_destroy_qp(ep_ptr->qp_handle))
			return (dapl_convert_errno(errno, "destroy_qp"));

		ep_ptr->qp_handle = NULL;
	}

#ifdef DAT_EXTENSIONS
	/* UD endpoints can have many CR associations and will not
	 * set ep->cm_handle. Call provider with cm_ptr null to incidate
	 * UD type multi CR's for this EP. It will parse internal list
	 * and cleanup all associations.
	 */
	if (ep_ptr->param.ep_attr.service_type == DAT_IB_SERVICE_TYPE_UD) 
		dapls_ib_cm_free(NULL, ep_ptr);
#endif

	return DAT_SUCCESS;
}

/*
 * dapl_ib_qp_modify
 *
 * Set the QP to the parameters specified in an EP_PARAM
 *
 * The EP_PARAM structure that is provided has been
 * sanitized such that only non-zero values are valid.
 *
 * Input:
 *	ib_hca_handle		HCA handle
 *	qp_handle		QP handle
 *	ep_attr		        Sanitized EP Params
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
dapls_ib_qp_modify(IN DAPL_IA * ia_ptr,
		   IN DAPL_EP * ep_ptr, IN DAT_EP_ATTR * attr)
{
	struct ibv_qp_attr qp_attr;

	if (ep_ptr->qp_handle == IB_INVALID_HANDLE)
		return DAT_INVALID_PARAMETER;

	/* 
	 * EP state, qp_handle state should be an indication
	 * of current state but the only way to be sure is with
	 * a user mode ibv_query_qp call which is NOT available 
	 */

	/* move to error state if necessary */
	if ((ep_ptr->qp_state == IB_QP_STATE_ERROR) &&
	    (ep_ptr->qp_handle->state != IBV_QPS_ERR)) {
		return (dapls_modify_qp_state(ep_ptr->qp_handle, 
					      IBV_QPS_ERR, 0, 0, 0));
	}

	/*
	 * Check if we have the right qp_state to modify attributes
	 */
	if ((ep_ptr->qp_handle->state != IBV_QPS_RTR) &&
	    (ep_ptr->qp_handle->state != IBV_QPS_RTS))
		return DAT_INVALID_STATE;

	/* Adjust to current EP attributes */
	dapl_os_memzero((void *)&qp_attr, sizeof(qp_attr));
	qp_attr.cap.max_send_wr = attr->max_request_dtos;
	qp_attr.cap.max_recv_wr = attr->max_recv_dtos;
	qp_attr.cap.max_send_sge = attr->max_request_iov;
	qp_attr.cap.max_recv_sge = attr->max_recv_iov;

	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     "modify_qp: qp %p sq %d,%d, rq %d,%d\n",
		     ep_ptr->qp_handle,
		     qp_attr.cap.max_send_wr, qp_attr.cap.max_send_sge,
		     qp_attr.cap.max_recv_wr, qp_attr.cap.max_recv_sge);

	if (ibv_modify_qp(ep_ptr->qp_handle, &qp_attr, IBV_QP_CAP)) {
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "modify_qp: modify ep %p qp %p failed\n",
			     ep_ptr, ep_ptr->qp_handle);
		return (dapl_convert_errno(errno, "modify_qp_state"));
	}

	return DAT_SUCCESS;
}

/*
 * dapls_ib_reinit_ep
 *
 * Move the QP to INIT state again.
 *
 * Input:
 *	ep_ptr		DAPL_EP
 *
 * Output:
 * 	none
 *
 * Returns:
 * 	void
 *
 */
#if defined(_WIN32) || defined(_WIN64) || defined(_OPENIB_CMA_)
void dapls_ib_reinit_ep(IN DAPL_EP * ep_ptr)
{
	/* work around bug in low level driver - 3/24/09 */
	/* RTS -> RESET -> INIT -> ERROR QP transition crashes system */
	if (ep_ptr->qp_handle != IB_INVALID_HANDLE) {
		dapls_ib_qp_free(ep_ptr->header.owner_ia, ep_ptr);
		dapls_ib_qp_alloc(ep_ptr->header.owner_ia, ep_ptr, ep_ptr);
	}
}
#else				// _WIN32 || _WIN64
void dapls_ib_reinit_ep(IN DAPL_EP * ep_ptr)
{
	if (ep_ptr->qp_handle != IB_INVALID_HANDLE &&
	    ep_ptr->qp_handle->qp_type != IBV_QPT_UD) {
		/* move to RESET state and then to INIT */
		dapls_modify_qp_state(ep_ptr->qp_handle, IBV_QPS_RESET,0,0,0);
		dapls_modify_qp_state(ep_ptr->qp_handle, IBV_QPS_INIT,0,0,0);
	}
}
#endif				// _WIN32 || _WIN64

/* 
 * Generic QP modify for init, reset, error, RTS, RTR
 * For UD, create_ah on RTR, qkey on INIT
 * CM msg provides QP attributes, info in network order
 */
DAT_RETURN
dapls_modify_qp_state(IN ib_qp_handle_t		qp_handle,
		      IN ib_qp_state_t		qp_state, 
		      IN uint32_t		qpn,
		      IN uint16_t		lid,
		      IN ib_gid_handle_t	gid)
{
	struct ibv_qp_attr qp_attr;
	enum ibv_qp_attr_mask mask = IBV_QP_STATE;
	DAPL_EP *ep_ptr = (DAPL_EP *) qp_handle->qp_context;
	DAPL_IA *ia_ptr = ep_ptr->header.owner_ia;
	int ret;

	dapl_os_memzero((void *)&qp_attr, sizeof(qp_attr));
	qp_attr.qp_state = qp_state;
	
	switch (qp_state) {
	case IBV_QPS_RTR:
		dapl_dbg_log(DAPL_DBG_TYPE_EP,
				" QPS_RTR: type %d qpn 0x%x gid %p (%d) lid 0x%x"
				" port %d ep %p qp_state %d \n",
				qp_handle->qp_type, ntohl(qpn), gid, 
				ia_ptr->hca_ptr->ib_trans.global,
				ntohs(lid), ia_ptr->hca_ptr->port_num,
				ep_ptr, ep_ptr->qp_state);

		mask |= IBV_QP_AV |
			IBV_QP_PATH_MTU |
			IBV_QP_DEST_QPN |
			IBV_QP_RQ_PSN |
			IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

		qp_attr.dest_qp_num = ntohl(qpn);
		qp_attr.rq_psn = 1;
		qp_attr.path_mtu = ia_ptr->hca_ptr->ib_trans.mtu;
		qp_attr.max_dest_rd_atomic =
			ep_ptr->param.ep_attr.max_rdma_read_out;
		qp_attr.min_rnr_timer =
			ia_ptr->hca_ptr->ib_trans.rnr_timer;

		/* address handle. RC and UD */
		qp_attr.ah_attr.dlid = ntohs(lid);
		if (gid && ia_ptr->hca_ptr->ib_trans.global) {
			dapl_dbg_log(DAPL_DBG_TYPE_EP, 
				     " QPS_RTR: GID Subnet 0x" F64x " ID 0x" F64x "\n", 
				     (unsigned long long)htonll(gid->global.subnet_prefix),
				     (unsigned long long)htonll(gid->global.interface_id));

			qp_attr.ah_attr.is_global = 1;
			qp_attr.ah_attr.grh.dgid.global.subnet_prefix = 
				gid->global.subnet_prefix;
			qp_attr.ah_attr.grh.dgid.global.interface_id = 
				gid->global.interface_id;
			qp_attr.ah_attr.grh.hop_limit =
				ia_ptr->hca_ptr->ib_trans.hop_limit;
			qp_attr.ah_attr.grh.traffic_class =
				ia_ptr->hca_ptr->ib_trans.tclass;
		}
		qp_attr.ah_attr.sl = 0;
		qp_attr.ah_attr.src_path_bits = 0;
		qp_attr.ah_attr.port_num = ia_ptr->hca_ptr->port_num;

		/* UD: already in RTR, RTS state */
		if (qp_handle->qp_type == IBV_QPT_UD) {
			mask = IBV_QP_STATE;
			if (ep_ptr->qp_state == IBV_QPS_RTR ||
				ep_ptr->qp_state == IBV_QPS_RTS)
				return DAT_SUCCESS;
		}
		break;
	case IBV_QPS_RTS:
		if (qp_handle->qp_type == IBV_QPT_RC) {
			mask |= IBV_QP_SQ_PSN |
				IBV_QP_TIMEOUT |
				IBV_QP_RETRY_CNT |
				IBV_QP_RNR_RETRY | IBV_QP_MAX_QP_RD_ATOMIC;
			qp_attr.timeout =
				ia_ptr->hca_ptr->ib_trans.ack_timer;
			qp_attr.retry_cnt =
				ia_ptr->hca_ptr->ib_trans.ack_retry;
			qp_attr.rnr_retry =
				ia_ptr->hca_ptr->ib_trans.rnr_retry;
			qp_attr.max_rd_atomic =
				ep_ptr->param.ep_attr.max_rdma_read_out;
		}
		/* RC and UD */
		qp_attr.qp_state = IBV_QPS_RTS;
		qp_attr.sq_psn = 1;

		dapl_dbg_log(DAPL_DBG_TYPE_EP,
				" QPS_RTS: psn %x rd_atomic %d ack %d "
				" retry %d rnr_retry %d ep %p qp_state %d\n",
				qp_attr.sq_psn, qp_attr.max_rd_atomic,
				qp_attr.timeout, qp_attr.retry_cnt,
				qp_attr.rnr_retry, ep_ptr,
				ep_ptr->qp_state);

		if (qp_handle->qp_type == IBV_QPT_UD) {
			/* already RTS, multi remote AH's on QP */
			if (ep_ptr->qp_state == IBV_QPS_RTS)
				return DAT_SUCCESS;
			else
				mask = IBV_QP_STATE | IBV_QP_SQ_PSN;
		}
		break;
	case IBV_QPS_INIT:
		mask |= IBV_QP_PKEY_INDEX | IBV_QP_PORT;
		if (qp_handle->qp_type == IBV_QPT_RC) {
			mask |= IBV_QP_ACCESS_FLAGS;
			qp_attr.qp_access_flags =
				IBV_ACCESS_LOCAL_WRITE |
				IBV_ACCESS_REMOTE_WRITE |
				IBV_ACCESS_REMOTE_READ |
				IBV_ACCESS_REMOTE_ATOMIC |
				IBV_ACCESS_MW_BIND;
		}

		if (qp_handle->qp_type == IBV_QPT_UD) {
			/* already INIT, multi remote AH's on QP */
			if (ep_ptr->qp_state == IBV_QPS_INIT)
				return DAT_SUCCESS;
			mask |= IBV_QP_QKEY;
			qp_attr.qkey = DAT_UD_QKEY;
		}

		qp_attr.pkey_index = 0;
		qp_attr.port_num = ia_ptr->hca_ptr->port_num;

		dapl_dbg_log(DAPL_DBG_TYPE_EP,
				" QPS_INIT: pi %x port %x acc %x qkey 0x%x\n",
				qp_attr.pkey_index, qp_attr.port_num,
				qp_attr.qp_access_flags, qp_attr.qkey);
		break;
	default:
		break;
	}

	ret = ibv_modify_qp(qp_handle, &qp_attr, mask);
	if (ret == 0) {
		ep_ptr->qp_state = qp_state;
		return DAT_SUCCESS;
	} else {
		return (dapl_convert_errno(errno, "modify_qp_state"));
	}
}

/* Modify UD type QP from init, rtr, rts, info network order */
DAT_RETURN 
dapls_modify_qp_ud(IN DAPL_HCA *hca, IN ib_qp_handle_t qp)
{
	struct ibv_qp_attr qp_attr;

	/* modify QP, setup and prepost buffers */
	dapl_os_memzero((void *)&qp_attr, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_INIT;
        qp_attr.pkey_index = 0;
        qp_attr.port_num = hca->port_num;
        qp_attr.qkey = DAT_UD_QKEY;
	if (ibv_modify_qp(qp, &qp_attr, 
			  IBV_QP_STATE		|
			  IBV_QP_PKEY_INDEX	|
                          IBV_QP_PORT		|
                          IBV_QP_QKEY)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			" modify_ud_qp INIT: ERR %s\n", strerror(errno));
		return (dapl_convert_errno(errno, "modify_qp"));
	}
	dapl_os_memzero((void *)&qp_attr, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_RTR;
	if (ibv_modify_qp(qp, &qp_attr,IBV_QP_STATE)) {
		dapl_log(DAPL_DBG_TYPE_ERR, 
			" modify_ud_qp RTR: ERR %s\n", strerror(errno));
		return (dapl_convert_errno(errno, "modify_qp"));
	}
	dapl_os_memzero((void *)&qp_attr, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.sq_psn = 1;
	if (ibv_modify_qp(qp, &qp_attr, 
			  IBV_QP_STATE | IBV_QP_SQ_PSN)) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			" modify_ud_qp RTS: ERR %s\n", strerror(errno));
		return (dapl_convert_errno(errno, "modify_qp"));
	}
	return DAT_SUCCESS;
}

/* Create address handle for remote QP, info in network order */
ib_ah_handle_t 
dapls_create_ah(IN DAPL_HCA		*hca,
		IN ib_pd_handle_t	pd,
		IN ib_qp_handle_t	qp,
		IN uint16_t		lid,
		IN ib_gid_handle_t	gid)
{
	struct ibv_qp_attr qp_attr;
	ib_ah_handle_t	ah;

	if (qp->qp_type != IBV_QPT_UD)
		return NULL;

	dapl_os_memzero((void *)&qp_attr, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QP_STATE;

	/* address handle. RC and UD */
	qp_attr.ah_attr.dlid = ntohs(lid);
	if (gid != NULL) {
		dapl_log(DAPL_DBG_TYPE_CM, "dapl_create_ah: with GID\n");
		qp_attr.ah_attr.is_global = 1;
		qp_attr.ah_attr.grh.dgid.global.subnet_prefix = 
				ntohll(gid->global.subnet_prefix);
		qp_attr.ah_attr.grh.dgid.global.interface_id = 
				ntohll(gid->global.interface_id);
		qp_attr.ah_attr.grh.hop_limit =	hca->ib_trans.hop_limit;
		qp_attr.ah_attr.grh.traffic_class = hca->ib_trans.tclass;
	}
	qp_attr.ah_attr.sl = 0;
	qp_attr.ah_attr.src_path_bits = 0;
	qp_attr.ah_attr.port_num = hca->port_num;

	dapl_log(DAPL_DBG_TYPE_CM, 
			" dapls_create_ah: port %x lid %x pd %p ctx %p handle 0x%x\n", 
			hca->port_num,qp_attr.ah_attr.dlid, pd, pd->context, pd->handle);

	/* UD: create AH for remote side */
	ah = ibv_create_ah(pd, &qp_attr.ah_attr);
	if (!ah) {
		dapl_log(DAPL_DBG_TYPE_ERR,
			" create_ah: ERR %s\n", strerror(errno));
		return NULL;
	}

	dapl_log(DAPL_DBG_TYPE_CM, 
			" dapls_create_ah: AH %p for lid %x\n", 
			ah, qp_attr.ah_attr.dlid);

	return ah;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
