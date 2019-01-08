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
 * MODULE: dapl_rmr_bind.c
 *
 * PURPOSE: Memory management
 *
 * $Id:$
 **********************************************************************/

#include "dapl.h"
#include "dapl_rmr_util.h"
#include "dapl_ep_util.h"
#include "dapl_cookie.h"
#include "dapl_adapter_util.h"

/*********************************************************************
 *                                                                   *
 * Function Prototypes                                               *
 *                                                                   *
 *********************************************************************/

STATIC _INLINE_ DAT_RETURN
dapli_rmr_bind_fuse(IN DAPL_RMR * rmr,
		    IN const DAT_LMR_TRIPLET * lmr_triplet,
		    IN DAT_MEM_PRIV_FLAGS mem_priv,
		    IN DAPL_EP * ep_ptr,
		    IN DAT_RMR_COOKIE user_cookie,
		    IN DAT_COMPLETION_FLAGS completion_flags,
		    OUT DAT_RMR_CONTEXT * rmr_context);

STATIC _INLINE_ DAT_RETURN
dapli_rmr_bind_unfuse(IN DAPL_RMR * rmr,
		      IN DAPL_EP * ep_ptr,
		      IN DAT_RMR_COOKIE user_cookie,
		      IN DAT_COMPLETION_FLAGS completion_flags);

/*********************************************************************
 *                                                                   *
 * Function Definitions                                              *
 *                                                                   *
 *********************************************************************/

DAT_RETURN
dapli_rmr_bind_fuse(IN DAPL_RMR * rmr,
		    IN const DAT_LMR_TRIPLET * lmr_triplet,
		    IN DAT_MEM_PRIV_FLAGS mem_priv,
		    IN DAPL_EP * ep_ptr,
		    IN DAT_RMR_COOKIE user_cookie,
		    IN DAT_COMPLETION_FLAGS completion_flags,
		    OUT DAT_RMR_CONTEXT * rmr_context)
{
	DAPL_LMR *lmr;
	DAPL_COOKIE *cookie;
	DAT_RETURN dat_status;
	DAT_BOOLEAN is_signaled;
	DAPL_HASH_DATA hash_lmr;

	dat_status =
	    dapls_hash_search(rmr->header.owner_ia->hca_ptr->lmr_hash_table,
			      lmr_triplet->lmr_context, &hash_lmr);
	if (DAT_SUCCESS != dat_status) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}
	lmr = (DAPL_LMR *) hash_lmr;

	/* if the ep in unconnected return an error. IB requires that the */
	/* QP be connected to change a memory window binding since:       */
	/*                                                                */
	/* - memory window bind operations are WQEs placed on a QP's      */
	/*   send queue                                                   */
	/*                                                                */
	/* - QP's only process WQEs on the send queue when the QP is in   */
	/*   the RTS state                                                */
	if (DAT_EP_STATE_CONNECTED != ep_ptr->param.ep_state) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      dapls_ep_state_subtype(ep_ptr));
		goto bail;
	}

	if (DAT_FALSE ==
	    dapl_mr_bounds_check(dapl_mr_get_address
				 (lmr->param.region_desc, lmr->param.mem_type),
				 lmr->param.length,
				 lmr_triplet->virtual_address,
				 lmr_triplet->segment_length)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail;
	}

	/* If the LMR, RMR, and EP are not in the same PZ, there is an error */
	if ((ep_ptr->param.pz_handle != lmr->param.pz_handle) ||
	    (ep_ptr->param.pz_handle != rmr->param.pz_handle)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}

	if (!dapl_rmr_validate_completion_flag(DAT_COMPLETION_SUPPRESS_FLAG,
					       ep_ptr->param.ep_attr.
					       request_completion_flags,
					       completion_flags)
	    ||
	    !dapl_rmr_validate_completion_flag(DAT_COMPLETION_UNSIGNALLED_FLAG,
					       ep_ptr->param.ep_attr.
					       request_completion_flags,
					       completion_flags)
	    ||
	    !dapl_rmr_validate_completion_flag
	    (DAT_COMPLETION_BARRIER_FENCE_FLAG,
	     ep_ptr->param.ep_attr.request_completion_flags,
	     completion_flags)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG4);
		goto bail;
	}

	dat_status = dapls_rmr_cookie_alloc(&ep_ptr->req_buffer,
					    rmr, user_cookie, &cookie);
	if (DAT_SUCCESS != dat_status) {
		goto bail;
	}

	is_signaled =
	    (completion_flags & DAT_COMPLETION_SUPPRESS_FLAG) ? DAT_FALSE :
	    DAT_TRUE;

	dat_status = dapls_ib_mw_bind(rmr,
				      lmr,
				      ep_ptr,
				      cookie,
				      lmr_triplet->virtual_address,
				      lmr_triplet->segment_length,
				      mem_priv, is_signaled);
	if (DAT_SUCCESS != dat_status) {
		dapls_cookie_dealloc(&ep_ptr->req_buffer, cookie);
		goto bail;
	}

	dapl_os_atomic_inc(&lmr->lmr_ref_count);

	/* if the RMR was previously bound */
	if (NULL != rmr->lmr) {
		dapl_os_atomic_dec(&rmr->lmr->lmr_ref_count);
	}

	rmr->param.mem_priv = mem_priv;
	rmr->param.lmr_triplet = *lmr_triplet;
	rmr->ep = ep_ptr;
	rmr->lmr = lmr;

	if (NULL != rmr_context) {
		*rmr_context = rmr->param.rmr_context;
	}

      bail:
	return dat_status;
}

DAT_RETURN
dapli_rmr_bind_unfuse(IN DAPL_RMR * rmr,
		      IN DAPL_EP * ep_ptr,
		      IN DAT_RMR_COOKIE user_cookie,
		      IN DAT_COMPLETION_FLAGS completion_flags)
{
	DAPL_COOKIE *cookie;
	DAT_RETURN dat_status;
	DAT_BOOLEAN is_signaled;

	dat_status = DAT_SUCCESS;
	/*
	 * if the ep in unconnected return an error. IB requires that the
	 * QP be connected to change a memory window binding since:
	 *
	 * - memory window bind operations are WQEs placed on a QP's
	 *   send queue
	 *
	 * - QP's only process WQEs on the send queue when the QP is in
	 *   the RTS state
	 */
	if (DAT_EP_STATE_CONNECTED != ep_ptr->param.ep_state) {
		dat_status =
		    DAT_ERROR(DAT_INVALID_STATE,
			      dapls_ep_state_subtype(ep_ptr));
		goto bail1;
	}

	/* If the RMR and EP are not in the same PZ, there is an error */
	if (ep_ptr->param.pz_handle != rmr->param.pz_handle) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail1;
	}

	if (!dapl_rmr_validate_completion_flag(DAT_COMPLETION_SUPPRESS_FLAG,
					       ep_ptr->param.ep_attr.
					       request_completion_flags,
					       completion_flags)
	    ||
	    !dapl_rmr_validate_completion_flag(DAT_COMPLETION_UNSIGNALLED_FLAG,
					       ep_ptr->param.ep_attr.
					       request_completion_flags,
					       completion_flags)
	    ||
	    !dapl_rmr_validate_completion_flag
	    (DAT_COMPLETION_BARRIER_FENCE_FLAG,
	     ep_ptr->param.ep_attr.request_completion_flags,
	     completion_flags)) {
		dat_status = DAT_ERROR(DAT_INVALID_PARAMETER, DAT_INVALID_ARG2);
		goto bail1;
	}

	dat_status = dapls_rmr_cookie_alloc(&ep_ptr->req_buffer,
					    rmr, user_cookie, &cookie);
	if (DAT_SUCCESS != dat_status) {
		goto bail1;
	}

	is_signaled =
	    (completion_flags & DAT_COMPLETION_UNSIGNALLED_FLAG) ? DAT_FALSE :
	    DAT_TRUE;

	dat_status = dapls_ib_mw_unbind(rmr, ep_ptr, cookie, is_signaled);
	if (DAT_SUCCESS != dat_status) {
		dapls_cookie_dealloc(&ep_ptr->req_buffer, cookie);
		goto bail1;
	}

	/* if the RMR was previously bound */
	if (NULL != rmr->lmr) {
		dapl_os_atomic_dec(&rmr->lmr->lmr_ref_count);
	}

	rmr->param.mem_priv = DAT_MEM_PRIV_NONE_FLAG;
	rmr->param.lmr_triplet.lmr_context = 0;
	rmr->param.lmr_triplet.virtual_address = 0;
	rmr->param.lmr_triplet.segment_length = 0;
	rmr->ep = ep_ptr;
	rmr->lmr = NULL;

      bail1:
	return dat_status;
}

/*
 * dapl_rmr_bind
 *
 * DAPL Requirements Version xxx, 6.6.4.4
 *
 * Bind the RMR to the specified memory region within the LMR and
 * provide a new rmr_context value.
 *
 * Input:
 * Output:
 */
DAT_RETURN DAT_API
dapl_rmr_bind(IN DAT_RMR_HANDLE rmr_handle,
	      IN DAT_LMR_HANDLE lmr_handle,
	      IN const DAT_LMR_TRIPLET * lmr_triplet,
	      IN DAT_MEM_PRIV_FLAGS mem_priv,
	      IN DAT_VA_TYPE va_type,
	      IN DAT_EP_HANDLE ep_handle,
	      IN DAT_RMR_COOKIE user_cookie,
	      IN DAT_COMPLETION_FLAGS completion_flags,
	      OUT DAT_RMR_CONTEXT * rmr_context)
{
	DAPL_RMR *rmr;
	DAPL_EP *ep_ptr;

	if (DAPL_BAD_HANDLE(rmr_handle, DAPL_MAGIC_RMR)) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_RMR);
	}
	if (DAPL_BAD_HANDLE(ep_handle, DAPL_MAGIC_EP)) {
		return DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP);
	}

	rmr = (DAPL_RMR *) rmr_handle;
	ep_ptr = (DAPL_EP *) ep_handle;

	/* if the rmr should be bound */
	if (0 != lmr_triplet->segment_length) {
		return dapli_rmr_bind_fuse(rmr,
					   lmr_triplet,
					   mem_priv,
					   ep_ptr,
					   user_cookie,
					   completion_flags, rmr_context);
	} else {		/* the rmr should be unbound */

		return dapli_rmr_bind_unfuse(rmr,
					     ep_ptr,
					     user_cookie, completion_flags);
	}
}
