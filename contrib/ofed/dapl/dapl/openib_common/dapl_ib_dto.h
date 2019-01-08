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
#ifndef _DAPL_IB_DTO_H_
#define _DAPL_IB_DTO_H_

#include "dapl_ib_util.h"

#ifdef DAT_EXTENSIONS
#include <dat2/dat_ib_extensions.h>
#endif

STATIC _INLINE_ int dapls_cqe_opcode(ib_work_completion_t *cqe_p);

#define CQE_WR_TYPE_UD(id) \
	(((DAPL_COOKIE *)(uintptr_t)id)->ep->qp_handle->qp_type == IBV_QPT_UD)

/*
 * dapls_ib_post_recv
 *
 * Provider specific Post RECV function
 */
STATIC _INLINE_ DAT_RETURN 
dapls_ib_post_recv (
	IN  DAPL_EP		*ep_ptr,
	IN  DAPL_COOKIE		*cookie,
	IN  DAT_COUNT		segments,
	IN  DAT_LMR_TRIPLET	*local_iov )
{
	struct ibv_recv_wr wr;
	struct ibv_recv_wr *bad_wr;
	ib_data_segment_t *ds = (ib_data_segment_t *)local_iov;
	DAT_COUNT i, total_len;
	int ret;
	
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " post_rcv: ep %p cookie %p segs %d l_iov %p\n",
		     ep_ptr, cookie, segments, local_iov);

	/* setup work request */
	total_len = 0;
	wr.next = 0;
	wr.num_sge = segments;
	wr.wr_id = (uint64_t)(uintptr_t)cookie;
	wr.sg_list = ds;

	if (cookie != NULL) {
		for (i = 0; i < segments; i++) {
			dapl_dbg_log(DAPL_DBG_TYPE_EP, 
				     " post_rcv: l_key 0x%x va %p len %d\n",
				     ds->lkey, ds->addr, ds->length );
			total_len += ds->length;
			ds++;
		}
		cookie->val.dto.size = total_len;
	}

	ret = ibv_post_recv(ep_ptr->qp_handle, &wr, &bad_wr);
	
	if (ret)
		return(dapl_convert_errno(errno,"ibv_recv"));

	DAPL_CNTR(ep_ptr, DCNT_EP_POST_RECV);
	DAPL_CNTR_DATA(ep_ptr, DCNT_EP_POST_RECV_DATA, total_len);

	return DAT_SUCCESS;
}

/*
 * dapls_ib_post_send
 *
 * Provider specific Post SEND function
 */
STATIC _INLINE_ DAT_RETURN 
dapls_ib_post_send (
	IN  DAPL_EP			*ep_ptr,
	IN  ib_send_op_type_t		op_type,
	IN  DAPL_COOKIE			*cookie,
	IN  DAT_COUNT			segments,
	IN  DAT_LMR_TRIPLET		*local_iov,
	IN  const DAT_RMR_TRIPLET	*remote_iov,
	IN  DAT_COMPLETION_FLAGS	completion_flags)
{
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	ib_data_segment_t *ds = (ib_data_segment_t *)local_iov;
	ib_hca_transport_t *ibt_ptr = 
		&ep_ptr->header.owner_ia->hca_ptr->ib_trans;
	DAT_COUNT i, total_len;
	int ret;
	
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " post_snd: ep %p op %d ck %p sgs",
		     "%d l_iov %p r_iov %p f %d\n",
		     ep_ptr, op_type, cookie, segments, local_iov, 
		     remote_iov, completion_flags);

#ifdef DAT_EXTENSIONS	
	if (ep_ptr->qp_handle->qp_type != IBV_QPT_RC)
		return(DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP));
#endif
	/* setup the work request */
	wr.next = 0;
	wr.opcode = op_type;
	wr.num_sge = segments;
	wr.send_flags = 0;
	wr.wr_id = (uint64_t)(uintptr_t)cookie;
	wr.sg_list = ds;
	total_len = 0;

	if (cookie != NULL) {
		for (i = 0; i < segments; i++ ) {
			dapl_dbg_log(DAPL_DBG_TYPE_EP, 
				     " post_snd: lkey 0x%x va %p len %d\n",
				     ds->lkey, ds->addr, ds->length );
			total_len += ds->length;
			ds++;
		}
		cookie->val.dto.size = total_len;
	}

	if (wr.num_sge && 
	    (op_type == OP_RDMA_WRITE || op_type == OP_RDMA_READ)) {
		wr.wr.rdma.remote_addr = remote_iov->virtual_address;
		wr.wr.rdma.rkey = remote_iov->rmr_context;
		dapl_dbg_log(DAPL_DBG_TYPE_EP, 
			     " post_snd_rdma: rkey 0x%x va %#016Lx\n",
			     wr.wr.rdma.rkey, wr.wr.rdma.remote_addr);
	}


	/* inline data for send or write ops */
	if ((total_len <= ibt_ptr->max_inline_send) && 
	   ((op_type == OP_SEND) || (op_type == OP_RDMA_WRITE))) 
		wr.send_flags |= IBV_SEND_INLINE;
	
	/* set completion flags in work request */
	wr.send_flags |= (DAT_COMPLETION_SUPPRESS_FLAG & 
				completion_flags) ? 0 : IBV_SEND_SIGNALED;
	wr.send_flags |= (DAT_COMPLETION_BARRIER_FENCE_FLAG & 
				completion_flags) ? IBV_SEND_FENCE : 0;
	wr.send_flags |= (DAT_COMPLETION_SOLICITED_WAIT_FLAG & 
				completion_flags) ? IBV_SEND_SOLICITED : 0;

	dapl_dbg_log(DAPL_DBG_TYPE_EP, 
		     " post_snd: op 0x%x flags 0x%x sglist %p, %d\n", 
		     wr.opcode, wr.send_flags, wr.sg_list, wr.num_sge);

	ret = ibv_post_send(ep_ptr->qp_handle, &wr, &bad_wr);

	if (ret)
		return(dapl_convert_errno(errno,"ibv_send"));

#ifdef DAPL_COUNTERS
	switch (op_type) {
	case OP_SEND:
		DAPL_CNTR(ep_ptr, DCNT_EP_POST_SEND);
		DAPL_CNTR_DATA(ep_ptr, DCNT_EP_POST_SEND_DATA,total_len);
		break;
	case OP_RDMA_WRITE:
		DAPL_CNTR(ep_ptr, DCNT_EP_POST_WRITE);
		DAPL_CNTR_DATA(ep_ptr, DCNT_EP_POST_WRITE_DATA,total_len);
		break;	
	case OP_RDMA_READ:
		DAPL_CNTR(ep_ptr, DCNT_EP_POST_READ);
		DAPL_CNTR_DATA(ep_ptr, DCNT_EP_POST_READ_DATA,total_len);
		break;
	default:
		break;
	}
#endif /* DAPL_COUNTERS */

	dapl_dbg_log(DAPL_DBG_TYPE_EP," post_snd: returned\n");
	return DAT_SUCCESS;
}

/* map Work Completions to DAPL WR operations */
STATIC _INLINE_ DAT_DTOS dapls_cqe_dtos_opcode(ib_work_completion_t *cqe_p)
{
	switch (cqe_p->opcode) {

	case IBV_WC_SEND:
#ifdef DAT_EXTENSIONS
		if (CQE_WR_TYPE_UD(cqe_p->wr_id))
			return (DAT_IB_DTO_SEND_UD);
		else
#endif			
		return (DAT_DTO_SEND);
	case IBV_WC_RDMA_READ:
		return (DAT_DTO_RDMA_READ);
	case IBV_WC_BIND_MW:
		return (DAT_DTO_BIND_MW);
#ifdef DAT_EXTENSIONS
	case IBV_WC_RDMA_WRITE:
		if (cqe_p->wc_flags & IBV_WC_WITH_IMM)
			return (DAT_IB_DTO_RDMA_WRITE_IMMED);
		else
			return (DAT_DTO_RDMA_WRITE);
	case IBV_WC_COMP_SWAP:
		return (DAT_IB_DTO_CMP_SWAP);
	case IBV_WC_FETCH_ADD:
		return (DAT_IB_DTO_FETCH_ADD);
	case IBV_WC_RECV_RDMA_WITH_IMM:
		return (DAT_IB_DTO_RECV_IMMED);
#else
	case IBV_WC_RDMA_WRITE:
		return (DAT_DTO_RDMA_WRITE);
#endif
	case IBV_WC_RECV:
#ifdef DAT_EXTENSIONS
		if (CQE_WR_TYPE_UD(cqe_p->wr_id)) 
			return (DAT_IB_DTO_RECV_UD);
		else if (cqe_p->wc_flags & IBV_WC_WITH_IMM)
			return (DAT_IB_DTO_RECV_MSG_IMMED);
		else
#endif	
		return (DAT_DTO_RECEIVE);
	default:
		return (0xff);
	}
}
#define DAPL_GET_CQE_DTOS_OPTYPE(cqe_p) dapls_cqe_dtos_opcode(cqe_p)


#ifdef DAT_EXTENSIONS
/*
 * dapls_ib_post_ext_send
 *
 * Provider specific extended Post SEND function for atomics
 *	OP_COMP_AND_SWAP and OP_FETCH_AND_ADD
 */
STATIC _INLINE_ DAT_RETURN 
dapls_ib_post_ext_send (
	IN  DAPL_EP			*ep_ptr,
	IN  ib_send_op_type_t		op_type,
	IN  DAPL_COOKIE			*cookie,
	IN  DAT_COUNT			segments,
	IN  DAT_LMR_TRIPLET		*local_iov,
	IN  const DAT_RMR_TRIPLET	*remote_iov,
	IN  DAT_UINT32			immed_data,
	IN  DAT_UINT64			compare_add,
	IN  DAT_UINT64			swap,
	IN  DAT_COMPLETION_FLAGS	completion_flags,
	IN  DAT_IB_ADDR_HANDLE		*remote_ah)
{
	struct ibv_send_wr wr;
	struct ibv_send_wr *bad_wr;
	ib_data_segment_t *ds = (ib_data_segment_t *)local_iov;
	DAT_COUNT i, total_len;
	int ret;
	
	dapl_dbg_log(DAPL_DBG_TYPE_EP,
		     " post_ext_snd: ep %p op %d ck %p sgs",
		     "%d l_iov %p r_iov %p f %d\n",
		     ep_ptr, op_type, cookie, segments, local_iov, 
		     remote_iov, completion_flags, remote_ah);

	/* setup the work request */
	wr.next = 0;
	wr.opcode = op_type;
	wr.num_sge = segments;
	wr.send_flags = 0;
	wr.wr_id = (uint64_t)(uintptr_t)cookie;
	wr.sg_list = ds;
	total_len = 0;

	if (cookie != NULL) {
		for (i = 0; i < segments; i++ ) {
			dapl_dbg_log(DAPL_DBG_TYPE_EP, 
				     " post_snd: lkey 0x%x va %p len %d\n",
				     ds->lkey, ds->addr, ds->length );
			total_len += ds->length;
			ds++;
		}
		cookie->val.dto.size = total_len;
	}

	switch (op_type) {
	case OP_RDMA_WRITE_IMM:
		/* OP_RDMA_WRITE)IMMED has direct IB wr_type mapping */
		dapl_dbg_log(DAPL_DBG_TYPE_EP, 
			     " post_ext: rkey 0x%x va %#016Lx immed=0x%x\n",
			     remote_iov?remote_iov->rmr_context:0, 
			     remote_iov?remote_iov->virtual_address:0,
			     immed_data);

		wr.imm_data = immed_data;
	        if (wr.num_sge) {
			wr.wr.rdma.remote_addr = remote_iov->virtual_address;
			wr.wr.rdma.rkey = remote_iov->rmr_context;
		}
		break;
	case OP_COMP_AND_SWAP:
		/* OP_COMP_AND_SWAP has direct IB wr_type mapping */
		dapl_dbg_log(DAPL_DBG_TYPE_EP, 
			     " post_ext: OP_COMP_AND_SWAP=%lx,"
			     "%lx rkey 0x%x va %#016Lx\n",
			     compare_add, swap, remote_iov->rmr_context,
			     remote_iov->virtual_address);
		
		wr.wr.atomic.compare_add = compare_add;
		wr.wr.atomic.swap = swap;
		wr.wr.atomic.remote_addr = remote_iov->virtual_address;
		wr.wr.atomic.rkey = remote_iov->rmr_context;
		break;
	case OP_FETCH_AND_ADD:
		/* OP_FETCH_AND_ADD has direct IB wr_type mapping */
		dapl_dbg_log(DAPL_DBG_TYPE_EP, 
			     " post_ext: OP_FETCH_AND_ADD=%lx,"
			     "%lx rkey 0x%x va %#016Lx\n",
			     compare_add, remote_iov->rmr_context,
			     remote_iov->virtual_address);

		wr.wr.atomic.compare_add = compare_add;
		wr.wr.atomic.remote_addr = remote_iov->virtual_address;
		wr.wr.atomic.rkey = remote_iov->rmr_context;
		break;
	case OP_SEND_UD:
		/* post must be on EP with service_type of UD */
		if (ep_ptr->qp_handle->qp_type != IBV_QPT_UD)
			return(DAT_ERROR(DAT_INVALID_HANDLE, DAT_INVALID_HANDLE_EP));

		dapl_dbg_log(DAPL_DBG_TYPE_EP, 
			     " post_ext: OP_SEND_UD ah=%p"
			     " qp_num=0x%x\n",
			     remote_ah->ah, remote_ah->qpn);
		
		wr.opcode = OP_SEND;
		wr.wr.ud.ah = remote_ah->ah;
		wr.wr.ud.remote_qpn = remote_ah->qpn;
		wr.wr.ud.remote_qkey = DAT_UD_QKEY;
		break;
	default:
		break;
	}

	/* set completion flags in work request */
	wr.send_flags |= (DAT_COMPLETION_SUPPRESS_FLAG & 
				completion_flags) ? 0 : IBV_SEND_SIGNALED;
	wr.send_flags |= (DAT_COMPLETION_BARRIER_FENCE_FLAG & 
				completion_flags) ? IBV_SEND_FENCE : 0;
	wr.send_flags |= (DAT_COMPLETION_SOLICITED_WAIT_FLAG & 
				completion_flags) ? IBV_SEND_SOLICITED : 0;

	dapl_dbg_log(DAPL_DBG_TYPE_EP, 
		     " post_snd: op 0x%x flags 0x%x sglist %p, %d\n", 
		     wr.opcode, wr.send_flags, wr.sg_list, wr.num_sge);

	ret = ibv_post_send(ep_ptr->qp_handle, &wr, &bad_wr);

	if (ret)
		return( dapl_convert_errno(errno,"ibv_send") );
	
#ifdef DAPL_COUNTERS
	switch (op_type) {
	case OP_RDMA_WRITE_IMM:
		DAPL_CNTR(ep_ptr, DCNT_EP_POST_WRITE_IMM);
		DAPL_CNTR_DATA(ep_ptr, 
			       DCNT_EP_POST_WRITE_IMM_DATA, total_len);
		break;
	case OP_COMP_AND_SWAP:
		DAPL_CNTR(ep_ptr, DCNT_EP_POST_CMP_SWAP);
		break;	
	case OP_FETCH_AND_ADD:
		DAPL_CNTR(ep_ptr, DCNT_EP_POST_FETCH_ADD);
		break;
	case OP_SEND_UD:
		DAPL_CNTR(ep_ptr, DCNT_EP_POST_SEND_UD);
		DAPL_CNTR_DATA(ep_ptr, DCNT_EP_POST_SEND_UD_DATA, total_len);
		break;
	default:
		break;
	}
#endif /* DAPL_COUNTERS */

	dapl_dbg_log(DAPL_DBG_TYPE_EP," post_snd: returned\n");
        return DAT_SUCCESS;
}
#endif

STATIC _INLINE_ DAT_RETURN 
dapls_ib_optional_prv_dat(
	IN  DAPL_CR		*cr_ptr,
	IN  const void		*event_data,
	OUT   DAPL_CR		**cr_pp)
{
    return DAT_SUCCESS;
}


/* map Work Completions to DAPL WR operations */
STATIC _INLINE_ int dapls_cqe_opcode(ib_work_completion_t *cqe_p)
{
#ifdef DAPL_COUNTERS
	DAPL_COOKIE *cookie = (DAPL_COOKIE *)(uintptr_t)cqe_p->wr_id;
#endif /* DAPL_COUNTERS */

	switch (cqe_p->opcode) {
	case IBV_WC_SEND:
		if (CQE_WR_TYPE_UD(cqe_p->wr_id))
			return(OP_SEND_UD);
		else
			return (OP_SEND);
	case IBV_WC_RDMA_WRITE:
		if (cqe_p->wc_flags & IBV_WC_WITH_IMM)
			return (OP_RDMA_WRITE_IMM);
		else
			return (OP_RDMA_WRITE);
	case IBV_WC_RDMA_READ:
		return (OP_RDMA_READ);
	case IBV_WC_COMP_SWAP:
		return (OP_COMP_AND_SWAP);
	case IBV_WC_FETCH_ADD:
		return (OP_FETCH_AND_ADD);
	case IBV_WC_BIND_MW:
		return (OP_BIND_MW);
	case IBV_WC_RECV:
		if (CQE_WR_TYPE_UD(cqe_p->wr_id)) {
			DAPL_CNTR(cookie->ep, DCNT_EP_RECV_UD);
			DAPL_CNTR_DATA(cookie->ep, DCNT_EP_RECV_UD_DATA, 
				       cqe_p->byte_len);
			return (OP_RECV_UD);
		}
		else if (cqe_p->wc_flags & IBV_WC_WITH_IMM) {
			DAPL_CNTR(cookie->ep, DCNT_EP_RECV_IMM);
			DAPL_CNTR_DATA(cookie->ep, DCNT_EP_RECV_IMM_DATA, 
				       cqe_p->byte_len);
			return (OP_RECEIVE_IMM);
		} else {
			DAPL_CNTR(cookie->ep, DCNT_EP_RECV);
			DAPL_CNTR_DATA(cookie->ep, DCNT_EP_RECV_DATA, 
				       cqe_p->byte_len);
			return (OP_RECEIVE);
		}
	case IBV_WC_RECV_RDMA_WITH_IMM:
		DAPL_CNTR(cookie->ep, DCNT_EP_RECV_RDMA_IMM);
		DAPL_CNTR_DATA(cookie->ep, DCNT_EP_RECV_RDMA_IMM_DATA, 
			       cqe_p->byte_len);
		return (OP_RECEIVE_IMM);
	default:
		return (OP_INVALID);
	}
}

#define DAPL_GET_CQE_OPTYPE(cqe_p) dapls_cqe_opcode(cqe_p)
#define DAPL_GET_CQE_WRID(cqe_p) ((ib_work_completion_t*)cqe_p)->wr_id
#define DAPL_GET_CQE_STATUS(cqe_p) ((ib_work_completion_t*)cqe_p)->status
#define DAPL_GET_CQE_VENDOR_ERR(cqe_p) ((ib_work_completion_t*)cqe_p)->vendor_err
#define DAPL_GET_CQE_BYTESNUM(cqe_p) ((ib_work_completion_t*)cqe_p)->byte_len
#define DAPL_GET_CQE_IMMED_DATA(cqe_p) ((ib_work_completion_t*)cqe_p)->imm_data

STATIC _INLINE_ char * dapls_dto_op_str(int op)
{
    static char *optable[] =
    {
        "OP_RDMA_WRITE",
        "OP_RDMA_WRITE_IMM",
        "OP_SEND",
        "OP_SEND_IMM",
        "OP_RDMA_READ",
        "OP_COMP_AND_SWAP",
        "OP_FETCH_AND_ADD",
        "OP_RECEIVE",
        "OP_RECEIVE_MSG_IMM",
	"OP_RECEIVE_RDMA_IMM",
        "OP_BIND_MW"
	"OP_SEND_UD"
	"OP_RECV_UD"
    };
    return ((op < 0 || op > 12) ? "Invalid CQE OP?" : optable[op]);
}

static _INLINE_ char *
dapls_cqe_op_str(IN ib_work_completion_t *cqe_ptr)
{
    return dapls_dto_op_str(DAPL_GET_CQE_OPTYPE(cqe_ptr));
}

#define DAPL_GET_CQE_OP_STR(cqe) dapls_cqe_op_str(cqe)

#endif	/*  _DAPL_IB_DTO_H_ */
