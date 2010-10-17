/*
 * Copyright (c) 2002-2003, Network Appliance, Inc. All rights reserved.
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

/**********************************************************************
 *
 * MODULE: dapl_ep_modify.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the DAPL 1.0 API, Chapter 6, section 5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_cookie.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"

/*
 * Internal prototypes
 */

static _INLINE_ DAT_RETURN
dapli_ep_modify_validate_parameters(IN DAT_EP_HANDLE ep_handle,
				    IN DAT_EP_PARAM_MASK ep_param_mask,
				    IN const DAT_EP_PARAM * ep_param,
				    OUT DAPL_IA ** ia_ptr,
				    OUT DAPL_EP ** ep_ptr,
				    OUT DAT_EP_ATTR * ep_attr_ptr);

/*
 * dapl_ep_modify
 *
 * DAPL Requirements Version xxx, 6.5.6
 *
 * Provide the consumer parameters, including attributes and status of
 * the Endpoint.
 *
 * Input:
 *	ep_handle
 *	ep_args_mask
 *
 * Output:
 *	ep_args
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_ATTRIBUTE
 *	DAT_INVALID_STATE
 */
DAT_RETURN DAT_API
dapl_ep_modify(IN DAT_EP_HANDLE ep_handle,
	       IN DAT_EP_PARAM_MASK ep_param_mask,
	       IN const DAT_EP_PARAM * ep_param)
{
	DAPL_IA *ia;
	DAPL_EP *ep1, *ep2;
	DAT_EP_ATTR ep_attr1 = { 0 }, ep_attr2 = {
	0};
	DAPL_EP new_ep, copy_of_old_ep;
	DAPL_EP alloc_ep;	/* Holder for resources.  */
	DAPL_PZ *tmp_pz;
	DAPL_EVD *tmp_evd;
	DAT_RETURN dat_status;

	/* Flag indicating we've allocated a new one of these.  */
	DAT_BOOLEAN qp_allocated = DAT_FALSE;
	DAT_BOOLEAN rqst_cb_allocated = DAT_FALSE;
	DAT_BOOLEAN recv_cb_allocated = DAT_FALSE;

	/* Flag indicating we've used (assigned to QP) a new one of these.  */
	DAT_BOOLEAN qp_used = DAT_FALSE;
	DAT_BOOLEAN rqst_cb_used = DAT_FALSE;
	DAT_BOOLEAN recv_cb_used = DAT_FALSE;

	dat_status = dapli_ep_modify_validate_parameters(ep_handle,
							 ep_param_mask,
							 ep_param,
							 &ia, &ep1, &ep_attr1);
	if (DAT_SUCCESS != dat_status) {
		goto bail;
	}

	/*
	 * Setup the alloc_ep with the appropriate parameters (primarily
	 * for allocating the QP.
	 */
	alloc_ep = *ep1;
	alloc_ep.param.ep_attr = ep_attr1;
	if (ep_param_mask & DAT_EP_FIELD_PZ_HANDLE) {
		alloc_ep.param.pz_handle = ep_param->pz_handle;
	}

	if (ep_param_mask & DAT_EP_FIELD_RECV_EVD_HANDLE) {
		alloc_ep.param.recv_evd_handle = ep_param->recv_evd_handle;
	}

	if (ep_param_mask & DAT_EP_FIELD_REQUEST_EVD_HANDLE) {
		alloc_ep.param.request_evd_handle =
		    ep_param->request_evd_handle;
	}

	if (ep_param_mask & DAT_EP_FIELD_CONNECT_EVD_HANDLE) {
		alloc_ep.param.connect_evd_handle =
		    ep_param->connect_evd_handle;
	}

	/*
	 * Allocate everything that might be needed.
	 * We allocate separately, and into a different "holding"
	 * ep, since we a) want the copy of the old ep into the new ep to
	 * be atomic with the assignment back (under lock), b) want the
	 * assignment of the allocated materials to be after the copy of the
	 * old ep into the new ep, and c) don't want the allocation done
	 * under lock.
	 */
	dat_status = dapls_cb_create(&alloc_ep.req_buffer, ep1,	/* For pointer in buffer bool.  */
				     ep_attr1.max_request_dtos);
	if (DAT_SUCCESS != dat_status) {
		goto bail;
	}
	rqst_cb_allocated = DAT_TRUE;

	dat_status = dapls_cb_create(&alloc_ep.recv_buffer, ep1,	/* For pointer in buffer bool.  */
				     ep_attr1.max_recv_dtos);
	if (DAT_SUCCESS != dat_status) {
		goto bail;
	}
	recv_cb_allocated = DAT_TRUE;

	dat_status = dapls_ib_qp_alloc(ia, &alloc_ep, ep1);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}
	qp_allocated = DAT_TRUE;

	/*
	 * Now we atomically modify the EP, under lock
	 * There's a lot of work done here, but there should be no
	 * allocation or blocking.
	 */
	dapl_os_lock(&ep1->header.lock);

	/*
	 * Revalidate parameters; make sure that races haven't
	 * changed anything important.
	 */
	dat_status = dapli_ep_modify_validate_parameters(ep_handle,
							 ep_param_mask,
							 ep_param,
							 &ia, &ep2, &ep_attr2);
	if (DAT_SUCCESS != dat_status) {
		dapl_os_unlock(&ep2->header.lock);
		goto bail;
	}

	/*
	 * All of the following should be impossible, if validation
	 * occurred.  But they're important to the logic of this routine,
	 * so we check.
	 */
	dapl_os_assert(ep1 == ep2);
	dapl_os_assert(ep_attr2.max_recv_dtos == ep_attr1.max_recv_dtos);
	dapl_os_assert(ep_attr2.max_request_dtos == ep_attr1.max_request_dtos);
	dapl_os_assert(ep_attr2.max_recv_iov == ep_attr1.max_recv_iov);
	dapl_os_assert(ep_attr2.max_request_iov == ep_attr1.max_request_iov);

	copy_of_old_ep = *ep2;

	/*
	 * Setup new ep.
	 */
	new_ep = *ep2;
	new_ep.param.ep_attr = ep_attr2;

	/*
	 * We can initialize the PZ and EVD handles from the alloc_ep because
	 * the only thing that could have changed since we setup the alloc_ep
	 * is stuff changed by dapl_cr_accept, and neither PZ nor EVD is in that
	 * list.
	 */
	new_ep.param.pz_handle = alloc_ep.param.pz_handle;
	new_ep.param.recv_evd_handle = alloc_ep.param.recv_evd_handle;
	new_ep.param.request_evd_handle = alloc_ep.param.request_evd_handle;
	new_ep.param.connect_evd_handle = alloc_ep.param.connect_evd_handle;

	/* Deal with each of the allocation fields.  */
	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_RECV_DTOS
	    && (ep_param->ep_attr.max_recv_dtos
		!= ep2->param.ep_attr.max_recv_dtos)) {
		new_ep.recv_buffer = alloc_ep.recv_buffer;
		recv_cb_used = DAT_TRUE;
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_REQUEST_DTOS
	    && (ep_param->ep_attr.max_request_dtos
		!= ep2->param.ep_attr.max_request_dtos)) {
		new_ep.req_buffer = alloc_ep.req_buffer;
		rqst_cb_used = DAT_TRUE;
	}

	/*
	 * We need to change the QP only if there already was a QP
	 * (leave things the way you found them!) and one of the
	 * following has changed: send/recv EVD, send/recv reqs/IOV max.
	 */
	if (DAPL_QP_STATE_UNATTACHED != new_ep.qp_state
	    && (ep_param_mask
		& (DAT_EP_FIELD_EP_ATTR_MAX_REQUEST_IOV
		   | DAT_EP_FIELD_EP_ATTR_MAX_RECV_IOV
		   | DAT_EP_FIELD_EP_ATTR_MAX_REQUEST_DTOS
		   | DAT_EP_FIELD_EP_ATTR_MAX_RECV_DTOS
		   | DAT_EP_FIELD_RECV_EVD_HANDLE
		   | DAT_EP_FIELD_REQUEST_EVD_HANDLE))) {
		/*
		 * We shouldn't be racing with connection establishment
		 * because the parameter validate routine should protect us,
		 * but it's an important enough point that we assert it.
		 */
		dapl_os_assert((ep2->param.ep_state
				!= DAT_EP_STATE_PASSIVE_CONNECTION_PENDING)
			       && (ep2->param.ep_state
				   != DAT_EP_STATE_ACTIVE_CONNECTION_PENDING));

		new_ep.qp_handle = alloc_ep.qp_handle;
		new_ep.qpn = alloc_ep.qpn;
	}

	/*
	 * The actual assignment, including modifying QP parameters.
	 * Modifying QP parameters needs to come first, as if it fails
	 * we need to exit. 
	 */
	if (DAPL_QP_STATE_UNATTACHED != new_ep.qp_state) {
		dat_status = dapls_ib_qp_modify(ia, ep2, &ep_attr2);
		if (dat_status != DAT_SUCCESS) {
			dapl_os_unlock(&ep2->header.lock);
			goto bail;
		}
	}
	*ep2 = new_ep;

	dapl_os_unlock(&ep2->header.lock);

	/*
	 * Modify reference counts, incrementing new ones
	 * and then decrementing old ones (so if they're the same
	 * the refcount never drops to zero).
	 */
	tmp_pz = (DAPL_PZ *) new_ep.param.pz_handle;
	if (NULL != tmp_pz) {
		dapl_os_atomic_inc(&tmp_pz->pz_ref_count);
	}

	tmp_evd = (DAPL_EVD *) new_ep.param.recv_evd_handle;
	if (NULL != tmp_evd) {
		dapl_os_atomic_inc(&tmp_evd->evd_ref_count);
	}

	tmp_evd = (DAPL_EVD *) new_ep.param.request_evd_handle;
	if (NULL != tmp_evd) {
		dapl_os_atomic_inc(&tmp_evd->evd_ref_count);
	}

	tmp_evd = (DAPL_EVD *) new_ep.param.connect_evd_handle;
	if (NULL != tmp_evd) {
		dapl_os_atomic_inc(&tmp_evd->evd_ref_count);
	}

	/* decreament the old reference counts */
	tmp_pz = (DAPL_PZ *) copy_of_old_ep.param.pz_handle;
	if (NULL != tmp_pz) {
		dapl_os_atomic_dec(&tmp_pz->pz_ref_count);
	}

	tmp_evd = (DAPL_EVD *) copy_of_old_ep.param.recv_evd_handle;
	if (NULL != tmp_evd) {
		dapl_os_atomic_dec(&tmp_evd->evd_ref_count);
	}

	tmp_evd = (DAPL_EVD *) copy_of_old_ep.param.request_evd_handle;
	if (NULL != tmp_evd) {
		dapl_os_atomic_dec(&tmp_evd->evd_ref_count);
	}

	tmp_evd = (DAPL_EVD *) copy_of_old_ep.param.connect_evd_handle;
	if (NULL != tmp_evd) {
		dapl_os_atomic_dec(&tmp_evd->evd_ref_count);
	}

      bail:
	if (qp_allocated) {
		DAT_RETURN local_dat_status;
		if (dat_status != DAT_SUCCESS || !qp_used) {
			local_dat_status = dapls_ib_qp_free(ia, &alloc_ep);
		} else {
			local_dat_status =
			    dapls_ib_qp_free(ia, &copy_of_old_ep);
		}
		if (local_dat_status != DAT_SUCCESS) {
			dapl_dbg_log(DAPL_DBG_TYPE_WARN,
				     "ep_modify: Failed to free QP; status %x\n",
				     local_dat_status);
		}
	}

	if (rqst_cb_allocated) {
		if (dat_status != DAT_SUCCESS || !rqst_cb_used) {
			dapls_cb_free(&alloc_ep.req_buffer);
		} else {
			dapls_cb_free(&copy_of_old_ep.req_buffer);
		}
	}

	if (recv_cb_allocated) {
		if (dat_status != DAT_SUCCESS || !recv_cb_used) {
			dapls_cb_free(&alloc_ep.recv_buffer);
		} else {
			dapls_cb_free(&copy_of_old_ep.recv_buffer);
		}
	}

	return dat_status;
}

/*
 * dapli_ep_modify_validate_parameters
 *
 * Validate parameters
 *
 * The space for the ep_attr_ptr parameter should be allocated by the
 * consumer. Upon success, this parameter will contain the current ep
 * attribute values with the requested modifications made.
 *
 */

static DAT_RETURN
dapli_ep_modify_validate_parameters(IN DAT_EP_HANDLE ep_handle,
				    IN DAT_EP_PARAM_MASK ep_param_mask,
				    IN const DAT_EP_PARAM * ep_param,
				    OUT DAPL_IA ** ia_ptr,
				    OUT DAPL_EP ** ep_ptr,
				    OUT DAT_EP_ATTR * ep_attr_ptr)
{
	DAPL_IA *ia;
	DAPL_EP *ep;
	DAT_EP_ATTR ep_attr;
	DAT_EP_ATTR ep_attr_limit;
	DAT_EP_ATTR ep_attr_request;
	DAT_RETURN dat_status;

	*ia_ptr = NULL;
	*ep_ptr = NULL;
	dat_status = DAT_SUCCESS;

	if (DAPL_BAD_HANDLE(ep_handle, DAPL_MAGIC_EP)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
		goto bail;
	}

	ep = (DAPL_EP *) ep_handle;
	ia = ep->header.owner_ia;

	/*
	 * Verify parameters valid in current EP state
	 */
	if (ep_param_mask & (DAT_EP_FIELD_IA_HANDLE |
			     DAT_EP_FIELD_EP_STATE |
			     DAT_EP_FIELD_LOCAL_IA_ADDRESS_PTR |
			     DAT_EP_FIELD_LOCAL_PORT_QUAL |
			     DAT_EP_FIELD_REMOTE_IA_ADDRESS_PTR |
			     DAT_EP_FIELD_REMOTE_PORT_QUAL)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	/*
	 * Can only change the PZ handle if we are UNCONNECTED or
	 * TENTATIVE_CONNECTION_PENDING (psp PROVIDER allocated EP)
	 */
	if ((ep_param_mask & DAT_EP_FIELD_PZ_HANDLE) &&
	    (ep->param.ep_state != DAT_EP_STATE_UNCONNECTED &&
	     ep->param.ep_state != DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, dapls_ep_state_subtype(ep));
		goto bail;
	}

	if ((ep_param_mask & (DAT_EP_FIELD_RECV_EVD_HANDLE |
			      DAT_EP_FIELD_REQUEST_EVD_HANDLE |
			      DAT_EP_FIELD_CONNECT_EVD_HANDLE |
			      DAT_EP_FIELD_EP_ATTR_SERVICE_TYPE |
			      DAT_EP_FIELD_EP_ATTR_MAX_MESSAGE_SIZE |
			      DAT_EP_FIELD_EP_ATTR_MAX_RDMA_SIZE |
			      DAT_EP_FIELD_EP_ATTR_QOS |
			      DAT_EP_FIELD_EP_ATTR_REQUEST_COMPLETION_FLAGS |
			      DAT_EP_FIELD_EP_ATTR_RECV_COMPLETION_FLAGS |
			      DAT_EP_FIELD_EP_ATTR_MAX_RECV_DTOS |
			      DAT_EP_FIELD_EP_ATTR_MAX_REQUEST_DTOS |
			      DAT_EP_FIELD_EP_ATTR_MAX_RECV_IOV |
			      DAT_EP_FIELD_EP_ATTR_MAX_REQUEST_IOV)) &&
	    (ep->param.ep_state != DAT_EP_STATE_UNCONNECTED &&
	     ep->param.ep_state != DAT_EP_STATE_RESERVED &&
	     ep->param.ep_state != DAT_EP_STATE_TENTATIVE_CONNECTION_PENDING)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE, dapls_ep_state_subtype(ep));
		goto bail;
	}

	/*
	 * Validate handles being modified
	 */
	if (ep_param_mask & DAT_EP_FIELD_PZ_HANDLE) {
		if (ep_param->pz_handle != NULL &&
		    DAPL_BAD_HANDLE(ep_param->pz_handle, DAPL_MAGIC_PZ)) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_RECV_EVD_HANDLE) {
		if (ep_param->recv_evd_handle != NULL &&
		    (DAPL_BAD_HANDLE(ep_param->recv_evd_handle, DAPL_MAGIC_EVD)
		     || !((DAPL_EVD *) ep_param->recv_evd_handle)->
		     evd_flags & DAT_EVD_DTO_FLAG))
		{
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_REQUEST_EVD_HANDLE) {
		if (ep_param->request_evd_handle != NULL &&
		    DAPL_BAD_HANDLE(ep_param->request_evd_handle,
				    DAPL_MAGIC_EVD)) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_CONNECT_EVD_HANDLE) {
		if (ep_param->connect_evd_handle != NULL &&
		    DAPL_BAD_HANDLE(ep_param->connect_evd_handle,
				    DAPL_MAGIC_EVD)
		    && !(((DAPL_EVD *) ep_param->connect_evd_handle)->
			 evd_flags & DAT_EVD_CONNECTION_FLAG))
		{
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		}
	}

	/*
	 * Validate the attributes against the HCA limits
	 */
	ep_attr = ep->param.ep_attr;

	dapl_os_memzero(&ep_attr_limit, sizeof(DAT_EP_ATTR));
	dat_status =
	    dapls_ib_query_hca(ia->hca_ptr, NULL, &ep_attr_limit, NULL);
	if (dat_status != DAT_SUCCESS) {
		goto bail;
	}

	ep_attr_request = ep_param->ep_attr;

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_SERVICE_TYPE) {
		if (ep_attr_request.service_type != DAT_SERVICE_TYPE_RC) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_MESSAGE_SIZE) {
		if (ep_attr_request.max_mtu_size > ep_attr_limit.max_mtu_size) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		} else {
			ep_attr.max_mtu_size = ep_attr_request.max_mtu_size;
		}
	}

	/* Do nothing if the DAT_EP_FIELD_EP_ATTR_MAX_RDMA_SIZE flag is   */
	/* set. Each RDMA transport/provider may or may not have a limit  */
	/* on the size of an RDMA DTO. For InfiniBand, this parameter is  */
	/* validated in the implementation of the dapls_ib_qp_modify()    */
	/* function.                                                      */
	/*                                                                */
	/*          if ( ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_RDMA_SIZE ) */
	/*  {                                                         */
	/*                                                                */
	/*  }                                                         */

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_QOS) {
		/* Do nothing, not defined in the spec yet */
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_RECV_COMPLETION_FLAGS) {
		dat_status =
		    dapl_ep_check_recv_completion_flags(ep_attr_request.
							recv_completion_flags);
		if (dat_status != DAT_SUCCESS)
		{
			goto bail;
		} else {
			ep_attr.recv_completion_flags =
			    ep_attr_request.recv_completion_flags;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_REQUEST_COMPLETION_FLAGS) {
		dat_status =
		    dapl_ep_check_request_completion_flags(ep_attr_request.
							   request_completion_flags);
		if (dat_status != DAT_SUCCESS) {
			goto bail;
		} else {
			ep_attr.request_completion_flags =
			    ep_attr_request.request_completion_flags;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_RECV_DTOS) {
		if (ep_attr_request.max_recv_dtos > ep_attr_limit.max_recv_dtos
		    || (ep_param->recv_evd_handle == DAT_HANDLE_NULL
			&& (ep_attr_request.max_recv_dtos > 0))) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		} else {
			ep_attr.max_recv_dtos = ep_attr_request.max_recv_dtos;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_REQUEST_DTOS) {
		if (ep_attr_request.max_request_dtos >
		    ep_attr_limit.max_request_dtos
		    || (ep_param->request_evd_handle == DAT_HANDLE_NULL
			&& (ep_attr_request.max_request_dtos > 0))) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		} else {
			ep_attr.max_request_dtos =
			    ep_attr_request.max_request_dtos;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_RECV_IOV) {
		if (ep_attr_request.max_recv_iov > ep_attr_limit.max_recv_iov) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		} else {
			ep_attr.max_recv_iov = ep_attr_request.max_recv_iov;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_REQUEST_IOV) {
		if (ep_attr_request.max_request_iov >
		    ep_attr_limit.max_request_iov) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		} else {
			ep_attr.max_request_iov =
			    ep_attr_request.max_request_iov;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_RDMA_READ_IOV) {
		if (ep_attr_request.max_rdma_read_iov >
		    ep_attr_limit.max_rdma_read_iov) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		} else {
			ep_attr.max_rdma_read_iov =
			    ep_attr_request.max_rdma_read_iov;
		}
	}

	if (ep_param_mask & DAT_EP_FIELD_EP_ATTR_MAX_RDMA_WRITE_IOV) {
		if (ep_attr_request.max_rdma_write_iov >
		    ep_attr_limit.max_rdma_write_iov) {
			dat_status =
			    DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG3);
			goto bail;
		} else {
			ep_attr.max_rdma_write_iov =
			    ep_attr_request.max_rdma_write_iov;
		}
	}

	*ia_ptr = ia;
	*ep_ptr = ep;
	*ep_attr_ptr = ep_attr;

      bail:
	return dat_status;
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 8
 * End:
 */
