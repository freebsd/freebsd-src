/*
 * Copyright (c) 2007-2009 Intel Corporation.  All rights reserved.
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
#include "dapl_ib_util.h"
#include "dapl_ep_util.h"
#include "dapl_cookie.h"
#include <stdarg.h>

DAT_RETURN
dapli_post_ext(IN DAT_EP_HANDLE ep_handle,
	       IN DAT_UINT64 cmp_add,
	       IN DAT_UINT64 swap,
	       IN DAT_UINT32 immed_data,
	       IN DAT_COUNT segments,
	       IN DAT_LMR_TRIPLET * local_iov,
	       IN DAT_DTO_COOKIE user_cookie,
	       IN const DAT_RMR_TRIPLET * remote_iov,
	       IN int op_type,
	       IN DAT_COMPLETION_FLAGS flags, IN DAT_IB_ADDR_HANDLE * ah);

/*
 * dapl_extensions
 *
 * Process extension requests
 *
 * Input:
 *	ext_type,
 *	...
 *
 * Output:
 * 	Depends....
 *
 * Returns:
 * 	DAT_SUCCESS
 *	DAT_NOT_IMPLEMENTED
 *      .....
 *
 */
DAT_RETURN
dapl_extensions(IN DAT_HANDLE dat_handle,
		IN DAT_EXTENDED_OP ext_op, IN va_list args)
{
	DAT_EP_HANDLE ep;
	DAT_IB_ADDR_HANDLE *ah = NULL;
	DAT_LMR_TRIPLET *lmr_p;
	DAT_DTO_COOKIE cookie;
	const DAT_RMR_TRIPLET *rmr_p;
	DAT_UINT64 dat_uint64a, dat_uint64b;
	DAT_UINT32 dat_uint32;
	DAT_COUNT segments = 1;
	DAT_COMPLETION_FLAGS comp_flags;
	DAT_RETURN status = DAT_NOT_IMPLEMENTED;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     "dapl_extensions(hdl %p operation %d, ...)\n",
		     dat_handle, ext_op);

	switch ((int)ext_op) {

	case DAT_IB_RDMA_WRITE_IMMED_OP:
		dapl_dbg_log(DAPL_DBG_TYPE_RTN,
			     " WRITE_IMMED_DATA extension call\n");

		ep = dat_handle;	/* ep_handle */
		segments = va_arg(args, DAT_COUNT);	/* num segments */
		lmr_p = va_arg(args, DAT_LMR_TRIPLET *);
		cookie = va_arg(args, DAT_DTO_COOKIE);
		rmr_p = va_arg(args, const DAT_RMR_TRIPLET *);
		dat_uint32 = va_arg(args, DAT_UINT32);	/* immed data */
		comp_flags = va_arg(args, DAT_COMPLETION_FLAGS);

		status = dapli_post_ext(ep, 0, 0, dat_uint32, segments, lmr_p,
					cookie, rmr_p, OP_RDMA_WRITE_IMM,
					comp_flags, ah);
		break;

	case DAT_IB_CMP_AND_SWAP_OP:
		dapl_dbg_log(DAPL_DBG_TYPE_RTN,
			     " CMP_AND_SWAP extension call\n");

		ep = dat_handle;	/* ep_handle */
		dat_uint64a = va_arg(args, DAT_UINT64);	/* cmp_value */
		dat_uint64b = va_arg(args, DAT_UINT64);	/* swap_value */
		lmr_p = va_arg(args, DAT_LMR_TRIPLET *);
		cookie = va_arg(args, DAT_DTO_COOKIE);
		rmr_p = va_arg(args, const DAT_RMR_TRIPLET *);
		comp_flags = va_arg(args, DAT_COMPLETION_FLAGS);

		status = dapli_post_ext(ep, dat_uint64a, dat_uint64b,
					0, segments, lmr_p, cookie, rmr_p,
					OP_COMP_AND_SWAP, comp_flags, ah);
		break;

	case DAT_IB_FETCH_AND_ADD_OP:
		dapl_dbg_log(DAPL_DBG_TYPE_RTN,
			     " FETCH_AND_ADD extension call\n");

		ep = dat_handle;	/* ep_handle */
		dat_uint64a = va_arg(args, DAT_UINT64);	/* add value */
		lmr_p = va_arg(args, DAT_LMR_TRIPLET *);
		cookie = va_arg(args, DAT_DTO_COOKIE);
		rmr_p = va_arg(args, const DAT_RMR_TRIPLET *);
		comp_flags = va_arg(args, DAT_COMPLETION_FLAGS);

		status = dapli_post_ext(ep, dat_uint64a, 0, 0, segments,
					lmr_p, cookie, rmr_p,
					OP_FETCH_AND_ADD, comp_flags, ah);
		break;

	case DAT_IB_UD_SEND_OP:
		dapl_dbg_log(DAPL_DBG_TYPE_RTN,
			     " UD post_send extension call\n");

		ep = dat_handle;	/* ep_handle */
		segments = va_arg(args, DAT_COUNT);	/* segments */
		lmr_p = va_arg(args, DAT_LMR_TRIPLET *);
		ah = va_arg(args, DAT_IB_ADDR_HANDLE *);
		cookie = va_arg(args, DAT_DTO_COOKIE);
		comp_flags = va_arg(args, DAT_COMPLETION_FLAGS);

		status = dapli_post_ext(ep, 0, 0, 0, segments,
					lmr_p, cookie, NULL,
					OP_SEND_UD, comp_flags, ah);
		break;

#ifdef DAPL_COUNTERS
	case DAT_QUERY_COUNTERS_OP:
		{
			int cntr, reset;
			DAT_UINT64 *p_cntr_out;

			dapl_dbg_log(DAPL_DBG_TYPE_RTN,
				     " Query counter extension call\n");

			cntr = va_arg(args, int);
			p_cntr_out = va_arg(args, DAT_UINT64 *);
			reset = va_arg(args, int);

			status = dapl_query_counter(dat_handle, cntr,
						    p_cntr_out, reset);
			break;
		}
	case DAT_PRINT_COUNTERS_OP:
		{
			int cntr, reset;

			dapl_dbg_log(DAPL_DBG_TYPE_RTN,
				     " Print counter extension call\n");

			cntr = va_arg(args, int);
			reset = va_arg(args, int);

			dapl_print_counter(dat_handle, cntr, reset);
			status = DAT_SUCCESS;
			break;
		}
#endif				/* DAPL_COUNTERS */

	default:
		dapl_dbg_log(DAPL_DBG_TYPE_ERR,
			     "unsupported extension(%d)\n", (int)ext_op);
	}

	return (status);
}

DAT_RETURN
dapli_post_ext(IN DAT_EP_HANDLE ep_handle,
	       IN DAT_UINT64 cmp_add,
	       IN DAT_UINT64 swap,
	       IN DAT_UINT32 immed_data,
	       IN DAT_COUNT segments,
	       IN DAT_LMR_TRIPLET * local_iov,
	       IN DAT_DTO_COOKIE user_cookie,
	       IN const DAT_RMR_TRIPLET * remote_iov,
	       IN int op_type,
	       IN DAT_COMPLETION_FLAGS flags, IN DAT_IB_ADDR_HANDLE * ah)
{
	DAPL_EP *ep_ptr;
	ib_qp_handle_t qp_ptr;
	DAPL_COOKIE *cookie = NULL;
	DAT_RETURN dat_status = DAT_SUCCESS;

	dapl_dbg_log(DAPL_DBG_TYPE_API,
		     " post_ext_op: ep %p cmp_val %d "
		     "swap_val %d cookie 0x%x, r_iov %p, flags 0x%x, ah %p\n",
		     ep_handle, (unsigned)cmp_add, (unsigned)swap,
		     (unsigned)user_cookie.as_64, remote_iov, flags, ah);

	if (DAPL_BAD_HANDLE(ep_handle, DAPL_MAGIC_EP))
		return (DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP));

	ep_ptr = (DAPL_EP *) ep_handle;
	qp_ptr = ep_ptr->qp_handle;

	/*
	 * Synchronization ok since this buffer is only used for send
	 * requests, which aren't allowed to race with each other.
	 */
	dat_status = dapls_dto_cookie_alloc(&ep_ptr->req_buffer,
					    DAPL_DTO_TYPE_EXTENSION,
					    user_cookie, &cookie);
	if (dat_status != DAT_SUCCESS)
		goto bail;

	/*
	 * Take reference before posting to avoid race conditions with
	 * completions
	 */
	dapl_os_atomic_inc(&ep_ptr->req_count);

	/*
	 * Invoke provider specific routine to post DTO
	 */
	dat_status = dapls_ib_post_ext_send(ep_ptr, op_type, cookie, segments,	/* data segments */
					    local_iov, remote_iov, immed_data,	/* immed data */
					    cmp_add,	/* compare or add */
					    swap,	/* swap */
					    flags, ah);

	if (dat_status != DAT_SUCCESS) {
		dapl_os_atomic_dec(&ep_ptr->req_count);
		dapls_cookie_dealloc(&ep_ptr->req_buffer, cookie);
	}

      bail:
	return dat_status;

}

/* 
 * New provider routine to process extended DTO events 
 */
void
dapls_cqe_to_event_extension(IN DAPL_EP * ep_ptr,
			     IN DAPL_COOKIE * cookie,
			     IN ib_work_completion_t * cqe_ptr,
			     IN DAT_EVENT * event_ptr)
{
	uint32_t ibtype;
	DAT_DTO_COMPLETION_EVENT_DATA *dto =
	    &event_ptr->event_data.dto_completion_event_data;
	DAT_IB_EXTENSION_EVENT_DATA *ext_data = (DAT_IB_EXTENSION_EVENT_DATA *)
	    & event_ptr->event_extension_data[0];
	DAT_DTO_COMPLETION_STATUS dto_status;

	/* Get status from cqe */
	dto_status = dapls_ib_get_dto_status(cqe_ptr);

	dapl_dbg_log(DAPL_DBG_TYPE_EVD,
		     " cqe_to_event_ext: dto_ptr %p ext_ptr %p status %d\n",
		     dto, ext_data, dto_status);

	event_ptr->event_number = DAT_IB_DTO_EVENT;
	dto->ep_handle = cookie->ep;
	dto->user_cookie = cookie->val.dto.cookie;
	dto->operation = DAPL_GET_CQE_DTOS_OPTYPE(cqe_ptr);	/* new for 2.0 */
	dto->status = ext_data->status = dto_status;

	if (dto_status != DAT_DTO_SUCCESS)
		return;

	/* 
	 * Get operation type from CQ work completion entry and
	 * if extented operation then set extended event data
	 */
	ibtype = DAPL_GET_CQE_OPTYPE(cqe_ptr);

	switch (ibtype) {

	case OP_RDMA_WRITE_IMM:
		dapl_dbg_log(DAPL_DBG_TYPE_EVD,
			     " cqe_to_event_ext: OP_RDMA_WRITE_IMMED\n");

		/* type and outbound rdma write transfer size */
		dto->transfered_length = cookie->val.dto.size;
		ext_data->type = DAT_IB_RDMA_WRITE_IMMED;
		break;
	case OP_RECEIVE_IMM:
		dapl_dbg_log(DAPL_DBG_TYPE_EVD,
			     " cqe_to_event_ext: OP_RECEIVE_RDMA_IMMED\n");

		/* immed recvd, type and inbound rdma write transfer size */
		dto->transfered_length = DAPL_GET_CQE_BYTESNUM(cqe_ptr);
		ext_data->type = DAT_IB_RDMA_WRITE_IMMED_DATA;
		ext_data->val.immed.data = DAPL_GET_CQE_IMMED_DATA(cqe_ptr);
		break;
	case OP_RECEIVE_MSG_IMM:
		dapl_dbg_log(DAPL_DBG_TYPE_EVD,
			     " cqe_to_event_ext: OP_RECEIVE_MSG_IMMED\n");

		/* immed recvd, type and inbound recv message transfer size */
		dto->transfered_length = DAPL_GET_CQE_BYTESNUM(cqe_ptr);
		ext_data->type = DAT_IB_RECV_IMMED_DATA;
		ext_data->val.immed.data = DAPL_GET_CQE_IMMED_DATA(cqe_ptr);
		break;
	case OP_COMP_AND_SWAP:
		dapl_dbg_log(DAPL_DBG_TYPE_EVD,
			     " cqe_to_event_ext: COMP_AND_SWAP_RESP\n");

		/* original data is returned in LMR provided with post */
		ext_data->type = DAT_IB_CMP_AND_SWAP;
		dto->transfered_length = DAPL_GET_CQE_BYTESNUM(cqe_ptr);
		break;
	case OP_FETCH_AND_ADD:
		dapl_dbg_log(DAPL_DBG_TYPE_EVD,
			     " cqe_to_event_ext: FETCH_AND_ADD_RESP\n");

		/* original data is returned in LMR provided with post */
		ext_data->type = DAT_IB_FETCH_AND_ADD;
		dto->transfered_length = DAPL_GET_CQE_BYTESNUM(cqe_ptr);
		break;
	case OP_SEND_UD:
		dapl_dbg_log(DAPL_DBG_TYPE_EVD, " cqe_to_event_ext: UD_SEND\n");

		/* type and outbound send transfer size */
		ext_data->type = DAT_IB_UD_SEND;
		dto->transfered_length = cookie->val.dto.size;
		break;
	case OP_RECV_UD:
		dapl_dbg_log(DAPL_DBG_TYPE_EVD, " cqe_to_event_ext: UD_RECV\n");

		/* type and inbound recv message transfer size */
		ext_data->type = DAT_IB_UD_RECV;
		dto->transfered_length = DAPL_GET_CQE_BYTESNUM(cqe_ptr);
		break;

	default:
		/* not extended operation */
		ext_data->status = DAT_IB_OP_ERR;
		dto->status = DAT_DTO_ERR_TRANSPORT;
		break;
	}
}
