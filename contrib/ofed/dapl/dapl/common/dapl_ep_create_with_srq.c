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
 * MODULE: dapl_ep_create_with_srq.c
 *
 * PURPOSE: Endpoint management
 * Description: Interfaces in this file are completely described in
 *		the kDAPL 1.2 API, Chapter 6, section 6.5
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_ia_util.h"
#include "dapl_ep_util.h"
#include "dapl_adapter_util.h"

/*
 * dapl_ep_create_with_srq
 *
 * uDAPL Version 1.2, 6.6.5
 *
 * Create an instance of an Endpoint that is provided to the
 * consumer at ep_handle.
 *
 * Input:
 *	ia_handle
 *	pz_handle
 *	recv_evd_handle (recv DTOs)
 *	request_evd_handle (xmit DTOs)
 *	connect_evd_handle
 *	srq_handle
 *	ep_attrs
 *
 * Output:
 *	ep_handle
 *
 * Returns:
 *	DAT_SUCCESS
 *	DAT_INSUFFICIENT_RESOURCES
 *	DAT_INVALID_HANDLE
 *	DAT_INVALID_PARAMETER
 *	DAT_INVALID_ATTRIBUTE
 *	DAT_MODEL_NOT_SUPPORTED
 */
DAT_RETURN DAT_API
dapl_ep_create_with_srq(IN DAT_IA_HANDLE ia_handle,
			IN DAT_PZ_HANDLE pz_handle,
			IN DAT_EVD_HANDLE recv_evd_handle,
			IN DAT_EVD_HANDLE request_evd_handle,
			IN DAT_EVD_HANDLE connect_evd_handle,
			IN DAT_SRQ_HANDLE srq_handle,
			IN const DAT_EP_ATTR * ep_attr,
			OUT DAT_EP_HANDLE * ep_handle)
{
	DAPL_IA *ia_ptr;
	DAPL_EP *ep_ptr;
	DAT_EP_ATTR ep_attr_limit;
	DAPL_EVD *evd_ptr;
	DAT_RETURN dat_status;

	dat_status = DAT_SUCCESS;
	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_ep_create_with_srq (%p, %p, %p, %p, %p, %p, %p %p)\n",
		     ia_handle,
		     pz_handle,
		     recv_evd_handle,
		     request_evd_handle,
		     connect_evd_handle, srq_handle, ep_attr, ep_handle);

	ia_ptr = (DAPL_IA *) ia_handle;

	/*
	 * Verify parameters
	 */
	if (DAPL_BAD_HANDLE(ia_ptr, DAPL_MAGIC_IA)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_IA);
		goto bail;
	}

	/*
	 * Verify non-required parameters.
	 * N.B. Assumption: any parameter that can be
	 *      modified by dat_ep_modify() is not strictly
	 *      required when the EP is created
	 */
	if (pz_handle != NULL && DAPL_BAD_HANDLE(pz_handle, DAPL_MAGIC_PZ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_PZ);
		goto bail;
	}

	/* If connect handle is !NULL verify handle is good  */
	if (connect_evd_handle != DAT_HANDLE_NULL &&
	    (DAPL_BAD_HANDLE(connect_evd_handle, DAPL_MAGIC_EVD) ||
	     !(((DAPL_EVD *) connect_evd_handle)->
	       evd_flags & DAT_EVD_CONNECTION_FLAG))) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EVD_CONN);
		goto bail;
	}
	/* If recv_evd is !NULL, verify handle is good and flags are valid */
	if (recv_evd_handle != DAT_HANDLE_NULL &&
	    (DAPL_BAD_HANDLE(recv_evd_handle, DAPL_MAGIC_EVD) ||
	     !(((DAPL_EVD *) recv_evd_handle)->evd_flags & DAT_EVD_DTO_FLAG))) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EVD_RECV);
		goto bail;
	}

	/* If req_evd is !NULL, verify handle is good and flags are valid */
	if (request_evd_handle != DAT_HANDLE_NULL &&
	    (DAPL_BAD_HANDLE(request_evd_handle, DAPL_MAGIC_EVD) ||
	     !(((DAPL_EVD *) request_evd_handle)->
	       evd_flags & DAT_EVD_DTO_FLAG))) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE,
			      DAT_INVALID_HANDLE_EVD_REQUEST);
		goto bail;
	}

	/*
	 * Verify the SRQ handle. It is an error to invoke this call with
	 * a NULL handle
	 */
	if (DAPL_BAD_HANDLE(srq_handle, DAPL_MAGIC_SRQ)) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_SRQ);
		goto bail;
	}

	if (ep_handle == NULL) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG7);
		goto bail;
	}

	if (DAPL_BAD_PTR(ep_attr)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG6);
		goto bail;
	}

	/*
	 * Qualify EP Attributes are legal and make sense.  Note that if one
	 * or both of the DTO handles are NULL, then the corresponding
	 * max_*_dtos must 0 as the user will not be able to post dto ops on
	 * the respective queue.
	 */
	if (ep_attr != NULL &&
	    (ep_attr->service_type != DAT_SERVICE_TYPE_RC ||
	     (recv_evd_handle == DAT_HANDLE_NULL && ep_attr->max_recv_dtos != 0)
	     || (recv_evd_handle != DAT_HANDLE_NULL
		 && ep_attr->max_recv_dtos == 0)
	     || (request_evd_handle == DAT_HANDLE_NULL
		 && ep_attr->max_request_dtos != 0)
	     || (request_evd_handle != DAT_HANDLE_NULL
		 && ep_attr->max_request_dtos == 0)
	     || ep_attr->max_recv_iov == 0 || ep_attr->max_request_iov == 0
	     || (DAT_SUCCESS !=
		 dapl_ep_check_recv_completion_flags(ep_attr->
						     recv_completion_flags)))) {
		dat_status = DAT_INVALID_PARAMETER | DAT_INVALID_ARG6;
		goto bail;
	}

	/* Verify the attributes against the transport */
	if (ep_attr != NULL) {
		dapl_os_memzero(&ep_attr_limit, sizeof(DAT_EP_ATTR));
		dat_status = dapls_ib_query_hca(ia_ptr->hca_ptr,
						NULL, &ep_attr_limit, NULL);
		if (dat_status != DAT_SUCCESS) {
			goto bail;
		}
		if (ep_attr->max_mtu_size > ep_attr_limit.max_mtu_size ||
		    ep_attr->max_rdma_size > ep_attr_limit.max_rdma_size ||
		    ep_attr->max_recv_dtos > ep_attr_limit.max_recv_dtos ||
		    ep_attr->max_request_dtos > ep_attr_limit.max_request_dtos
		    || ep_attr->max_recv_iov > ep_attr_limit.max_recv_iov
		    || ep_attr->max_request_iov > ep_attr_limit.max_request_iov
		    || ep_attr->max_rdma_read_in >
		    ep_attr_limit.max_rdma_read_in
		    || ep_attr->max_rdma_read_out >
		    ep_attr_limit.max_rdma_read_out)
		{
			dat_status = DAT_INVALID_PARAMETER | DAT_INVALID_ARG6;
			goto bail;
		}
	}

	/*
	 * Verify the completion flags for the EVD and the EP
	 */
	/*
	 * XXX FIXME
	 * XXX Need to make assign the EVD to the right completion type
	 * XXX depending on the EP attributes. Fail if the types don't
	 * XXX match, they are mutually exclusive.
	 */
	evd_ptr = (DAPL_EVD *) recv_evd_handle;
	if (evd_ptr != NULL && evd_ptr->completion_type == DAPL_EVD_STATE_INIT) {
		if (ep_attr != NULL &&
		    ep_attr->recv_completion_flags ==
		    DAT_COMPLETION_DEFAULT_FLAG) {
			evd_ptr->completion_type = DAPL_EVD_STATE_THRESHOLD;
		} else {
			evd_ptr->completion_type =
			    ep_attr->recv_completion_flags;
		}
	}

	evd_ptr = (DAPL_EVD *) request_evd_handle;
	if (evd_ptr != NULL && evd_ptr->completion_type == DAPL_EVD_STATE_INIT) {
		if (ep_attr != NULL &&
		    ep_attr->recv_completion_flags ==
		    DAT_COMPLETION_DEFAULT_FLAG) {
			evd_ptr->completion_type = DAPL_EVD_STATE_THRESHOLD;
		} else {
			evd_ptr->completion_type =
			    ep_attr->recv_completion_flags;
		}
	}

	dat_status = DAT_NOT_IMPLEMENTED;

	/*
	 * XXX The rest of the EP code is useful in this case too,
	 * XXX but need to complete the SRQ implementation before
	 * XXX committing resources
	 */
	*ep_handle = ep_ptr = NULL;
	goto bail;
#ifdef notdef

	/* Allocate EP */
	ep_ptr = dapl_ep_alloc(ia_ptr, ep_attr);
	if (ep_ptr == NULL) {
		dat_status =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto bail;
	}

	/*
	 * Fill in the EP
	 */
	ep_ptr->param.ia_handle = ia_handle;
	ep_ptr->param.ep_state = DAT_EP_STATE_UNCONNECTED;
	ep_ptr->param.local_ia_address_ptr =
	    (DAT_IA_ADDRESS_PTR) & ia_ptr->hca_ptr->hca_address;
	/* Set the remote address pointer to the end of the EP struct */
	ep_ptr->param.remote_ia_address_ptr = (DAT_IA_ADDRESS_PTR) (ep_ptr + 1);

	ep_ptr->param.pz_handle = pz_handle;
	ep_ptr->param.recv_evd_handle = recv_evd_handle;
	ep_ptr->param.request_evd_handle = request_evd_handle;
	ep_ptr->param.connect_evd_handle = connect_evd_handle;

	/*
	 * Make sure we handle the NULL DTO EVDs
	 */
	if (recv_evd_handle == DAT_HANDLE_NULL && ep_attr == NULL) {
		ep_ptr->param.ep_attr.max_recv_dtos = 0;
	}

	if (request_evd_handle == DAT_HANDLE_NULL && ep_attr == NULL) {
		ep_ptr->param.ep_attr.max_request_dtos = 0;
	}

	/*
	 * If the user has specified a PZ handle we allocate a QP for
	 * this EP; else we defer until it is assigned via ep_modify().
	 * As much as possible we try to keep QP creation out of the
	 * connect path to avoid resource errors in strange places.
	 */
	if (pz_handle != DAT_HANDLE_NULL) {
		/* Take a reference on the PZ handle */
		dapl_os_atomic_inc(&((DAPL_PZ *) pz_handle)->pz_ref_count);

		/*
		 * Get a QP from the IB provider
		 */
		dat_status = dapls_ib_qp_alloc(ia_ptr, ep_ptr, ep_ptr);

		if (dat_status != DAT_SUCCESS) {
			dapl_os_atomic_dec(&((DAPL_PZ *) pz_handle)->
					   pz_ref_count);
			dapl_ep_dealloc(ep_ptr);
			goto bail;
		}
	} else {
		ep_ptr->qp_state = DAPL_QP_STATE_UNATTACHED;
	}

	/*
	 * Update ref counts. See the spec where the endpoint marks
	 * a data object as 'in use'
	 *   pz_handle: dat_pz_free, uDAPL Document, 6.6.1.2
	 *   evd_handles:
	 *
	 * N.B. This should really be done by a util routine.
	 */
	dapl_os_atomic_inc(&((DAPL_EVD *) connect_evd_handle)->evd_ref_count);
	/* Optional handles */
	if (recv_evd_handle != DAT_HANDLE_NULL) {
		dapl_os_atomic_inc(&((DAPL_EVD *) recv_evd_handle)->
				   evd_ref_count);
	}
	if (request_evd_handle != DAT_HANDLE_NULL) {
		dapl_os_atomic_inc(&((DAPL_EVD *) request_evd_handle)->
				   evd_ref_count);
	}

	/* Link it onto the IA */
	dapl_ia_link_ep(ia_ptr, ep_ptr);

	*ep_handle = ep_ptr;

#endif				/* notdef */

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
