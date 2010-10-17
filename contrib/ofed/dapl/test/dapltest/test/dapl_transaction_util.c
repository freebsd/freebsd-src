/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
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

#include "dapl_proto.h"

#define DT_LOCAL_COMPLETION_VECTOR_SIZE 32

/* -----------------------------------------------------------
 * Post a recv buffer on each of this thread's EPs.
 */
bool
DT_handle_post_recv_buf(DT_Tdep_Print_Head * phead,
			Ep_Context_t * ep_context,
			unsigned int num_eps, int op_indx)
{
	unsigned int i, j;

	for (i = 0; i < num_eps; i++) {
		Transaction_Test_Op_t *op = &ep_context[i].op[op_indx];
		DAT_LMR_TRIPLET *iov = DT_Bpool_GetIOV(op->bp, 0);
		DAT_DTO_COOKIE cookie;
		DAT_RETURN ret;

		/* Prep the inputs */
		for (j = 0; j < op->num_segs; j++) {
			iov[j].virtual_address = (DAT_VADDR) (uintptr_t)
			    DT_Bpool_GetBuffer(op->bp, j);
			iov[j].segment_length = op->seg_size;
			iov[j].lmr_context = DT_Bpool_GetLMR(op->bp, j);
		}
		cookie.as_64 = ((((DAT_UINT64) i) << 32)
				| (((uintptr_t) DT_Bpool_GetBuffer(op->bp, 0)) &
				   0xffffffffUL));

		/* Post the recv */
		ret = dat_ep_post_recv(ep_context[i].ep_handle,
				       op->num_segs,
				       iov,
				       cookie, DAT_COMPLETION_DEFAULT_FLAG);

		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dat_ep_post_recv failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			return false;
		}
	}

	return true;
}

/* -----------------------------------------------------------
 * Post a send buffer on each of this thread's EPs.
 */
bool
DT_handle_send_op(DT_Tdep_Print_Head * phead,
		  Ep_Context_t * ep_context,
		  unsigned int num_eps, int op_indx, bool poll)
{
	unsigned int i, j;
	unsigned char *completion_reaped;
	unsigned char lcomp[DT_LOCAL_COMPLETION_VECTOR_SIZE];
	bool rc = false;

	if (num_eps <= DT_LOCAL_COMPLETION_VECTOR_SIZE) {
		completion_reaped = lcomp;
		bzero((void *)completion_reaped,
			sizeof(unsigned char) * num_eps);
	}
	else {
		completion_reaped = DT_Mdep_Malloc(num_eps * sizeof(unsigned char));
		if (!completion_reaped) {
			return false;
		}
	}

	for (i = 0; i < num_eps; i++) {
		Transaction_Test_Op_t *op = &ep_context[i].op[op_indx];
		DAT_LMR_TRIPLET *iov = DT_Bpool_GetIOV(op->bp, 0);
		DAT_DTO_COOKIE cookie;
		DAT_RETURN ret;

		/* Prep the inputs */
		for (j = 0; j < op->num_segs; j++) {
			iov[j].virtual_address = (DAT_VADDR) (uintptr_t)
			    DT_Bpool_GetBuffer(op->bp, j);
			iov[j].segment_length = op->seg_size;
			iov[j].lmr_context = DT_Bpool_GetLMR(op->bp, j);
		}
		cookie.as_64 = ((((DAT_UINT64) i) << 32)
				| (((uintptr_t) DT_Bpool_GetBuffer(op->bp, 0)) &
				   0xffffffffUL));

		/* Post the send */
		ret = dat_ep_post_send(ep_context[i].ep_handle,
				       op->num_segs,
				       iov,
				       cookie, DAT_COMPLETION_DEFAULT_FLAG);

		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dat_ep_post_send failed: %s\n",
					  DT_RetToString(ret));
			DT_Test_Error();
			goto xit;
		}
	}

	for (i = 0; i < num_eps; i++) {
		Transaction_Test_Op_t *op = &ep_context[i].op[op_indx];

		if (op->reap_send_on_recv && !op->server_initiated) {
			/* we will reap the send on the recv (Client SR) */
			rc = true;
			goto xit;
		}
	}

	/* reap the send completion */
	for (i = 0; i < num_eps; i++) {
		Transaction_Test_Op_t *op;
		DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
		DAT_DTO_COOKIE dto_cookie;
		unsigned int epnum;

		if (!DT_dto_event_reap
		    (phead, ep_context[i].reqt_evd_hdl, poll, &dto_stat)) {
			goto xit;
		}

		epnum = dto_stat.user_cookie.as_64 >> 32;
		if (epnum > num_eps) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: Send: Invalid endpoint completion reaped.\n"
					  "\tEndpoint: 0x%p, Cookie: 0x" F64x
					  ", Length: " F64u "\n",
					  dto_stat.ep_handle,
					  dto_stat.user_cookie.as_64,
					  dto_stat.transfered_length);
			DT_Test_Error();
			goto xit;
		}

		op = &ep_context[epnum].op[op_indx];

		dto_cookie.as_64 = ((((DAT_UINT64) epnum) << 32)
				    |
				    (((uintptr_t) DT_Bpool_GetBuffer(op->bp, 0))
				     & 0xffffffffUL));

		if (!DT_dto_check(phead,
				  &dto_stat,
				  ep_context[epnum].ep_handle,
				  op->num_segs * op->seg_size,
				  dto_cookie, "Send")) {
			goto xit;
		}

		if (completion_reaped[epnum]) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: Send: Secondary completion seen for endpoint 0x%p (%d)\n",
					  ep_context[epnum].ep_handle, epnum);
			DT_Test_Error();
			goto xit;
		}
		completion_reaped[epnum] = 1;
	}

	for (i = 0; i < num_eps; i++) {
		if (completion_reaped[i] == 0) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: Send: No completion seen for endpoint 0x%p (#%d)\n",
					  ep_context[i].ep_handle, i);
			DT_Test_Error();
			goto xit;
		}
	}

	rc = true;

xit:
	if (completion_reaped != lcomp)
		DT_Mdep_Free(completion_reaped);
	return rc;
}

/* -----------------------------------------------------------
 * Reap a recv op on each of this thread's EPs,
 * then if requested reap the corresponding send ops,
 * and re-post all of the recv buffers.
 */
bool
DT_handle_recv_op(DT_Tdep_Print_Head * phead,
		  Ep_Context_t * ep_context,
		  unsigned int num_eps,
		  int op_indx, bool poll, bool repost_recv)
{
	unsigned int i;
	unsigned char *recv_completion_reaped;
	unsigned char *send_completion_reaped;
	unsigned char rcomp[DT_LOCAL_COMPLETION_VECTOR_SIZE];
	unsigned char lcomp[DT_LOCAL_COMPLETION_VECTOR_SIZE];
	bool rc = false;

	if (num_eps <= DT_LOCAL_COMPLETION_VECTOR_SIZE ) {
		recv_completion_reaped = rcomp;
		send_completion_reaped = lcomp;
		bzero((void *)recv_completion_reaped,
			sizeof(unsigned char) * num_eps);
		bzero((void *)send_completion_reaped,
			sizeof(unsigned char) * num_eps);
	}
	else {
		recv_completion_reaped = DT_Mdep_Malloc(num_eps);
		if (recv_completion_reaped == NULL) {
			return false;
		}

		send_completion_reaped = DT_Mdep_Malloc(num_eps);
		if (send_completion_reaped == NULL) {
			DT_Mdep_Free(recv_completion_reaped);
			return false;
		}
	}

	/* Foreach EP, reap */
	for (i = 0; i < num_eps; i++) {
		Transaction_Test_Op_t *op;
		DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
		DAT_DTO_COOKIE dto_cookie;
		unsigned int epnum;

		/* First reap the recv DTO event */
		if (!DT_dto_event_reap
		    (phead, ep_context[i].recv_evd_hdl, poll, &dto_stat)) {
			goto xit;
		}

		epnum = dto_stat.user_cookie.as_64 >> 32;
		if (epnum > num_eps) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: Receive: Invalid endpoint completion reaped.\n"
					  "\tEndpoint: 0x%p, Cookie: 0x" F64x
					  ", Length: " F64u "\n",
					  dto_stat.ep_handle,
					  dto_stat.user_cookie.as_64,
					  dto_stat.transfered_length);
			DT_Test_Error();
			goto xit;
		}

		op = &ep_context[epnum].op[op_indx];
		dto_cookie.as_64 = ((((DAT_UINT64) epnum) << 32)
				    |
				    (((uintptr_t) DT_Bpool_GetBuffer(op->bp, 0))
				     & 0xffffffffUL));

		if (!DT_dto_check(phead,
				  &dto_stat,
				  ep_context[epnum].ep_handle,
				  op->num_segs * op->seg_size,
				  dto_cookie, "Recv")) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: recv DTO problem\n");
			DT_Test_Error();
			goto xit;
		}

		if (recv_completion_reaped[epnum]) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: Receive: Secondary completion seen for endpoint 0x%p (%d)\n",
					  ep_context[epnum].ep_handle, epnum);
			DT_Test_Error();
			goto xit;
		}
		recv_completion_reaped[epnum] = 1;

		/*
		 * Check the current op to see whether we are supposed
		 * to reap the previous send op now.
		 */
		if (op->reap_send_on_recv && op->server_initiated) {
			if (op_indx <= 0)
				/* shouldn't happen, but let's be certain */
			{
				DT_Tdep_PT_Printf(phead,
						  "Internal Error: reap_send_on_recv"
						  " but current op == #%d\n",
						  op_indx);
				goto xit;
			}

			if (!DT_dto_event_reap
			    (phead, ep_context[i].reqt_evd_hdl, poll,
			     &dto_stat)) {
				goto xit;
			}

			epnum = dto_stat.user_cookie.as_64 >> 32;
			if (epnum > num_eps) {
				DT_Tdep_PT_Printf(phead,
						  "Test Error: Send (ror): Invalid endpoint completion reaped.\n"
						  "\tEndpoint: 0x%p, Cookie: 0x"
						  F64x ", Length: " F64u "\n",
						  dto_stat.ep_handle,
						  dto_stat.user_cookie.as_64,
						  dto_stat.transfered_length);
				DT_Test_Error();
				goto xit;
			}

			/*
			 * We're reaping the last transaction, a
			 * send completion that we skipped when it was sent.
			 */
			op = &ep_context[epnum].op[op_indx - 1];

			dto_cookie.as_64 = ((((DAT_UINT64) epnum) << 32)
					    |
					    (((uintptr_t)
					      DT_Bpool_GetBuffer(op->bp, 0))
					     & 0xffffffffUL));

			/*
			 * If we have multiple EPs we can't guarantee the order of
			 * completions, so disable ep_handle check
			 */
			if (!DT_dto_check(phead,
					  &dto_stat,
					  num_eps ==
					  1 ? ep_context[i].ep_handle : NULL,
					  op->num_segs * op->seg_size,
					  dto_cookie, "Send-reaped-on-recv")) {
				DT_Tdep_PT_Printf(phead,
						  "Test Error: send DTO problem\n");
				DT_Test_Error();
				goto xit;
			}

			if (send_completion_reaped[epnum]) {
				DT_Tdep_PT_Printf(phead,
						  "Test Error: Send (ror): Secondary completion seen for endpoint 0x%p (%d)\n",
						  ep_context[epnum].ep_handle,
						  epnum);
				DT_Test_Error();
				goto xit;
			}
			send_completion_reaped[epnum] = 1;
		}
	}

	for (i = 0; i < num_eps; i++) {
		if (recv_completion_reaped[i] == 0) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: Receive: No completion seen for endpoint 0x%p (#%d)\n",
					  ep_context[i].ep_handle, i);
			DT_Test_Error();
			goto xit;
		}
	}

	if (ep_context[0].op[op_indx].reap_send_on_recv
	    && ep_context[0].op[op_indx].server_initiated) {
		for (i = 0; i < num_eps; i++) {
			if (send_completion_reaped[i] == 0) {
				DT_Tdep_PT_Printf(phead,
						  "Test Error: Send (ror): No completion seen for endpoint 0x%p (#%d)\n",
						  ep_context[i].ep_handle, i);
				DT_Test_Error();
				goto xit;
			}
		}
	}

	if (repost_recv) {
		/* repost the receive buffer */
		if (!DT_handle_post_recv_buf
		    (phead, ep_context, num_eps, op_indx)) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: recv re-post problem\n");
			DT_Test_Error();
			goto xit;
		}
	}
	rc = true;
xit:
	if (send_completion_reaped != lcomp) {
		DT_Mdep_Free(recv_completion_reaped);
		DT_Mdep_Free(send_completion_reaped);
	}
	return rc;
}

/* -----------------------------------------------------------
 * Initiate an RDMA op (synchronous) on each of this thread's EPs.
 */
bool
DT_handle_rdma_op(DT_Tdep_Print_Head * phead,
		  Ep_Context_t * ep_context,
		  unsigned int num_eps,
		  DT_Transfer_Type opcode, int op_indx, bool poll)
{
	unsigned int i, j;
	DAT_RETURN ret;
	unsigned char *completion_reaped;
	unsigned char lcomp[DT_LOCAL_COMPLETION_VECTOR_SIZE];
	bool rc = false;

	if (num_eps <= DT_LOCAL_COMPLETION_VECTOR_SIZE) {
		completion_reaped = lcomp;
		bzero((void *)completion_reaped, sizeof(unsigned char) * num_eps);
	}
	else {
		completion_reaped = DT_Mdep_Malloc(num_eps * sizeof(unsigned char));
		if (!completion_reaped) {
			return false;
		}
	}

	/* Initiate the operation */
	for (i = 0; i < num_eps; i++) {
		Transaction_Test_Op_t *op = &ep_context[i].op[op_indx];
		DAT_LMR_TRIPLET *iov = DT_Bpool_GetIOV(op->bp, 0);
		DAT_DTO_COOKIE cookie;
		DAT_RMR_TRIPLET rmr_triplet;

		/* Prep the inputs */
		for (j = 0; j < op->num_segs; j++) {
			iov[j].virtual_address = (DAT_VADDR) (uintptr_t)
			    DT_Bpool_GetBuffer(op->bp, j);
			iov[j].segment_length = op->seg_size;
			iov[j].lmr_context = DT_Bpool_GetLMR(op->bp, j);
		}
		cookie.as_64 = ((((DAT_UINT64) i) << 32)
				| (((uintptr_t) DT_Bpool_GetBuffer(op->bp, 0)) &
				   0xffffffffUL));

		rmr_triplet.virtual_address =
		    (DAT_VADDR) (uintptr_t) op->Rdma_Address;
		rmr_triplet.segment_length = op->seg_size * op->num_segs;
		rmr_triplet.rmr_context = op->Rdma_Context;

		DT_Tdep_PT_Debug(3, (phead,
				     "Call dat_ep_post_rdma_%s [" F64x ", sz="
				     F64x ", ctxt=%x]\n",
				     (opcode == RDMA_WRITE ? "write" : "read"),
				     rmr_triplet.virtual_address,
				     rmr_triplet.segment_length,
				     rmr_triplet.rmr_context));

		/* Post the operation */
		if (opcode == RDMA_WRITE) {

			ret = dat_ep_post_rdma_write(ep_context[i].ep_handle,
						     op->num_segs,
						     iov,
						     cookie,
						     &rmr_triplet,
						     DAT_COMPLETION_DEFAULT_FLAG);

		} else {	/* opcode == RDMA_READ */

			ret = dat_ep_post_rdma_read(ep_context[i].ep_handle,
						    op->num_segs,
						    iov,
						    cookie,
						    &rmr_triplet,
						    DAT_COMPLETION_DEFAULT_FLAG);

		}
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dat_ep_post_rdma_%s failed: %s\n",
					  (opcode ==
					   RDMA_WRITE ? "write" : "read"),
					  DT_RetToString(ret));
			DT_Test_Error();
			goto err;
		} else {
			DT_Tdep_PT_Debug(3, (phead,
					     "Done dat_ep_post_rdma_%s %s\n",
					     (opcode ==
					      RDMA_WRITE ? "write" : "read"),
					     " ()  Waiting ..."));
		}
	}

	/* Wait for it to happen */
	for (i = 0; i < num_eps; i++) {
		Transaction_Test_Op_t *op;
		DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
		DAT_DTO_COOKIE dto_cookie;
		unsigned int epnum;

		if (!DT_dto_event_reap
		    (phead, ep_context[i].reqt_evd_hdl, poll, &dto_stat)) {
			goto err;
		}

		epnum = dto_stat.user_cookie.as_64 >> 32;
		if (epnum > num_eps) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: %s: Invalid endpoint completion reaped.\n"
					  "\tEndpoint: 0x%p, Cookie: 0x" F64x
					  ", Length: " F64u "\n",
					  opcode ==
					  RDMA_WRITE ? "RDMA/WR" : "RDMA/RD",
					  dto_stat.ep_handle,
					  dto_stat.user_cookie.as_64,
					  dto_stat.transfered_length);
			DT_Test_Error();
			goto err;
		}
		op = &ep_context[epnum].op[op_indx];

		dto_cookie.as_64 = ((((DAT_UINT64) epnum) << 32)
				    |
				    (((uintptr_t) DT_Bpool_GetBuffer(op->bp, 0))
				     & 0xffffffffUL));

		if (!DT_dto_check(phead,
				  &dto_stat,
				  ep_context[epnum].ep_handle,
				  op->num_segs * op->seg_size,
				  dto_cookie,
				  (opcode ==
				   RDMA_WRITE ? "RDMA/WR" : "RDMA/RD"))) {
			goto err;
		}

		if (completion_reaped[epnum]) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: %s: Secondary completion seen for endpoint 0x%p (%d)\n",
					  opcode ==
					  RDMA_WRITE ? "RDMA/WR" : "RDMA/RD",
					  ep_context[epnum].ep_handle, epnum);
			DT_Test_Error();
			goto err;
		}
		completion_reaped[epnum] = 1;

		DT_Tdep_PT_Debug(3, (phead,
				     "dat_ep_post_rdma_%s OK\n",
				     (opcode ==
				      RDMA_WRITE ? "RDMA/WR" : "RDMA/RD")));
	}

	for (i = 0; i < num_eps; i++) {
		if (completion_reaped[i] == 0) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: %s: No completion seen for endpoint 0x%p (#%d)\n",
					  opcode ==
					  RDMA_WRITE ? "RDMA/WR" : "RDMA/RD",
					  ep_context[i].ep_handle, i);
			DT_Test_Error();
			goto err;
		}
	}

	rc = true;

err:
	if (completion_reaped != lcomp)
		DT_Mdep_Free(completion_reaped);

	return rc;
}

/* -----------------------------------------------------------
 * Verify whether we (the client side) can support
 * the requested 'T' test.
 */
bool DT_check_params(Per_Test_Data_t * pt_ptr, char *module)
{
	Transaction_Cmd_t *cmd = &pt_ptr->Params.u.Transaction_Cmd;
	unsigned long num_recvs = 0U;
	unsigned long num_sends = 0U;
	unsigned long num_rdma_rd = 0U;
	unsigned long num_rdma_wr = 0U;
	unsigned long max_size = 0U;
	unsigned long max_segs = 0U;
	bool rval = true;
	unsigned int i;
	DT_Tdep_Print_Head *phead;

	phead = pt_ptr->Params.phead;

	/* Count up what's requested (including -V appended sync points) */
	for (i = 0; i < cmd->num_ops; i++) {
		unsigned int xfer_size;

		xfer_size = cmd->op[i].num_segs * cmd->op[i].seg_size;
		if (xfer_size > max_size) {
			max_size = xfer_size;
		}
		if (cmd->op[i].num_segs > max_segs) {
			max_segs = cmd->op[i].num_segs;
		}

		switch (cmd->op[i].transfer_type) {
		case SEND_RECV:
			{
				if (cmd->op[i].server_initiated) {
					num_recvs++;
				} else {
					num_sends++;
				}
				break;
			}

		case RDMA_READ:
			{
				num_rdma_rd++;
				break;
			}

		case RDMA_WRITE:
			{
				num_rdma_wr++;
				break;
			}
		}
	}

	/*
	 * Now check the IA and EP attributes, and check for some of the
	 * more obvious resource problems.  This is hardly exhaustive,
	 * and some things will inevitably fall through to run-time.
	 *
	 * We don't compare
	 *      num_rdma_rd > pt_ptr->ia_attr.max_rdma_read_per_ep
	 *      num_rdma_wr > pt_ptr->ia_attr.max_dto_per_ep
	 * because each thread has its own EPs, and transfers are issued
	 * synchronously (across a thread's EPs, and ignoring -f, which allows
	 * a per-EP pipeline depth of at most 2 and applies only to SR ops),
	 * so dapltest actually attempts almost no pipelining on a single EP.
	 * But we do check that pre-posted recv buffers will all fit.
	 */
	if (num_recvs > pt_ptr->ia_attr.max_dto_per_ep ||
	    num_sends > pt_ptr->ia_attr.max_dto_per_ep) {
		DT_Tdep_PT_Printf(phead,
				  "%s: S/R: cannot supply %ld SR ops (maximum: %d)\n",
				  module,
				  num_recvs > num_sends ? num_recvs : num_sends,
				  pt_ptr->ia_attr.max_dto_per_ep);
		rval = false;
	}
	if (max_size > pt_ptr->ia_attr.max_lmr_block_size) {
		DT_Tdep_PT_Printf(phead,
				  "%s: buffer too large: 0x%lx (maximum: " F64x
				  " bytes)\n", module, max_size,
				  pt_ptr->ia_attr.max_lmr_block_size);
		rval = false;
	}
	if (max_segs > pt_ptr->ep_attr.max_recv_iov ||
	    max_segs > pt_ptr->ep_attr.max_request_iov) {
		/*
		 * In an ideal world, we'd just ask for more segments
		 * when creating the EPs for the test, rather than
		 * checking against default EP attributes.
		 */
		DT_Tdep_PT_Printf(phead,
				  "%s: cannot use %ld segments (maxima: S %d, R %d)\n",
				  module,
				  max_segs,
				  pt_ptr->ep_attr.max_request_iov,
				  pt_ptr->ep_attr.max_recv_iov);
		rval = false;
	}

	return (rval);
}

/* Empty function in which to set breakpoints.  */
void DT_Test_Error(void)
{
	;
}

void
DT_Transaction_Cmd_PT_Print(DT_Tdep_Print_Head * phead, Transaction_Cmd_t * cmd)
{
	unsigned int i;
	DT_Tdep_PT_Printf(phead, "-------------------------------------\n");
	DT_Tdep_PT_Printf(phead, "TransCmd.server_name              : %s\n",
			  cmd->server_name);
	DT_Tdep_PT_Printf(phead, "TransCmd.num_iterations           : %d\n",
			  cmd->num_iterations);
	DT_Tdep_PT_Printf(phead, "TransCmd.num_threads              : %d\n",
			  cmd->num_threads);
	DT_Tdep_PT_Printf(phead, "TransCmd.eps_per_thread           : %d\n",
			  cmd->eps_per_thread);
	DT_Tdep_PT_Printf(phead, "TransCmd.validate                 : %d\n",
			  cmd->validate);
	DT_Tdep_PT_Printf(phead, "TransCmd.dapl_name                : %s\n",
			  cmd->dapl_name);
	DT_Tdep_PT_Printf(phead, "TransCmd.num_ops                  : %d\n",
			  cmd->num_ops);

	for (i = 0; i < cmd->num_ops; i++) {
		DT_Tdep_PT_Printf(phead,
				  "TransCmd.op[%d].transfer_type      : %s %s\n",
				  i,
				  cmd->op[i].transfer_type ==
				  0 ? "RDMA_READ" : cmd->op[i].transfer_type ==
				  1 ? "RDMA_WRITE" : "SEND_RECV",
				  cmd->op[i].
				  server_initiated ? " (server)" : " (client)");
		DT_Tdep_PT_Printf(phead,
				  "TransCmd.op[%d].seg_size           : %d\n",
				  i, cmd->op[i].seg_size);
		DT_Tdep_PT_Printf(phead,
				  "TransCmd.op[%d].num_segs           : %d\n",
				  i, cmd->op[i].num_segs);
		DT_Tdep_PT_Printf(phead,
				  "TransCmd.op[%d].reap_send_on_recv  : %d\n",
				  i, cmd->op[i].reap_send_on_recv);
	}
}
