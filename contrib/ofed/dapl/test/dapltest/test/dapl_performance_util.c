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

#define DT_Mdep_GetContextSwitchNum() 0	/* FIXME */

/****************************************************************************/
bool
DT_Performance_Test_Create(Per_Test_Data_t * pt_ptr,
			   DAT_IA_HANDLE * ia_handle,
			   DAT_IA_ADDRESS_PTR remote_ia_addr,
			   DAT_BOOLEAN is_server,
			   DAT_BOOLEAN is_remote_little_endian,
			   Performance_Test_t ** perf_test)
{
	Performance_Test_t *test_ptr;
	DAT_COUNT pipeline_len;
	DAT_RETURN ret;
	DT_Tdep_Print_Head *phead;

	phead = pt_ptr->Params.phead;

	test_ptr = DT_MemListAlloc(pt_ptr,
				   "transaction_test_t",
				   TRANSACTIONTEST, sizeof(Performance_Test_t));
	if (NULL == test_ptr) {
		return false;
	}

	*perf_test = test_ptr;

	test_ptr->pt_ptr = pt_ptr;
	test_ptr->remote_ia_addr = remote_ia_addr;
	test_ptr->is_remote_little_endian = is_remote_little_endian;
	test_ptr->base_port =
	    (DAT_CONN_QUAL) pt_ptr->Server_Info.first_port_number;
	test_ptr->ia_handle = ia_handle;
	test_ptr->cmd = &pt_ptr->Params.u.Performance_Cmd;

	ret = dat_ia_query(test_ptr->ia_handle,
			   NULL, DAT_IA_ALL, &test_ptr->ia_attr, 0, NULL);
	if (DAT_SUCCESS != ret) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x "]: dat_ia_query error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		return false;
	}

	pipeline_len = DT_min(DT_min(test_ptr->cmd->num_iterations,
				     test_ptr->cmd->pipeline_len),
			      DT_min(test_ptr->ia_attr.max_dto_per_ep,
				     test_ptr->ia_attr.max_evd_qlen));

	if (RDMA_READ == test_ptr->cmd->op.transfer_type) {
		pipeline_len = DT_min(pipeline_len,
				      test_ptr->ia_attr.max_rdma_read_per_ep);
	}

	test_ptr->reqt_evd_length = pipeline_len;
	test_ptr->recv_evd_length = DT_PERF_DFLT_EVD_LENGTH;
	test_ptr->conn_evd_length = DT_PERF_DFLT_EVD_LENGTH;
	test_ptr->creq_evd_length = DT_PERF_DFLT_EVD_LENGTH;

	/* create a protection zone */
	ret = dat_pz_create(test_ptr->ia_handle, &test_ptr->pz_handle);
	if (DAT_SUCCESS != ret) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x "]: dat_pz_create error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		test_ptr->pz_handle = DAT_HANDLE_NULL;
		return false;
	}

	/* create 4 EVDs - recv, request+RMR, conn-request, connect */
	ret = DT_Tdep_evd_create(test_ptr->ia_handle, test_ptr->recv_evd_length, test_ptr->cno_handle, DAT_EVD_DTO_FLAG, &test_ptr->recv_evd_hdl);	/* recv */
	if (DAT_SUCCESS != ret) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x
				  "]: dat_evd_create (recv) error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		test_ptr->recv_evd_hdl = DAT_HANDLE_NULL;
		return false;
	}

	ret = DT_Tdep_evd_create(test_ptr->ia_handle, test_ptr->reqt_evd_length, test_ptr->cno_handle, DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG, &test_ptr->reqt_evd_hdl);	/* request + rmr bind */
	if (DAT_SUCCESS != ret) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x
				  "]: dat_evd_create (request) error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		test_ptr->reqt_evd_hdl = DAT_HANDLE_NULL;
		return false;
	}

	if (is_server) {
		/* Client-side doesn't need CR events */
		ret = DT_Tdep_evd_create(test_ptr->ia_handle, test_ptr->creq_evd_length, DAT_HANDLE_NULL, DAT_EVD_CR_FLAG, &test_ptr->creq_evd_hdl);	/* cr */
		if (DAT_SUCCESS != ret) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_evd_create (cr) error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			test_ptr->creq_evd_hdl = DAT_HANDLE_NULL;
			return false;
		}
	}

	ret = DT_Tdep_evd_create(test_ptr->ia_handle, test_ptr->conn_evd_length, DAT_HANDLE_NULL, DAT_EVD_CONNECTION_FLAG, &test_ptr->conn_evd_hdl);	/* conn */
	if (DAT_SUCCESS != ret) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x
				  "]: dat_evd_create (conn) error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		test_ptr->conn_evd_hdl = DAT_HANDLE_NULL;
		return false;
	}

	/*
	 * Set up the EP context:
	 *          create the EP
	 *          allocate buffers for remote memory info and sync message
	 *          post the receive buffers
	 *          connect
	 *          set up buffers and remote memory info
	 *          send across our info
	 *          recv the other side's info and extract what we need
	 */
	test_ptr->ep_context.ep_attr = test_ptr->pt_ptr->ep_attr;
	test_ptr->ep_context.ep_attr.max_request_dtos = pipeline_len;

	/* Create EP */
	ret = dat_ep_create(test_ptr->ia_handle,	/* IA       */
			    test_ptr->pz_handle,	/* PZ       */
			    test_ptr->recv_evd_hdl,	/* recv     */
			    test_ptr->reqt_evd_hdl,	/* request  */
			    test_ptr->conn_evd_hdl,	/* connect  */
			    &test_ptr->ep_context.ep_attr,	/* EP attrs */
			    &test_ptr->ep_context.ep_handle);
	if (DAT_SUCCESS != ret) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x "]: dat_ep_create error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		test_ptr->ep_context.ep_handle = DAT_HANDLE_NULL;
		return false;
	}

	/*
	 * Allocate a buffer pool so we can exchange the
	 * remote memory info and initialize.
	 */
	test_ptr->ep_context.bp = DT_BpoolAlloc(test_ptr->pt_ptr, phead, test_ptr->ia_handle, test_ptr->pz_handle, test_ptr->ep_context.ep_handle, DAT_HANDLE_NULL,	/* rmr */
						DT_PERF_SYNC_BUFF_SIZE, 2,	/* 2 RMIs */
						DAT_OPTIMAL_ALIGNMENT,
						false, false);
	if (!test_ptr->ep_context.bp) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x
				  "]: no memory for remote memory buffers\n",
				  test_ptr->base_port);
		return false;
	}

	DT_Tdep_PT_Debug(3, (phead,
			     "0: SYNC_SEND  %p\n",
			     (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->
							    ep_context.bp,
							    DT_PERF_SYNC_SEND_BUFFER_ID)));
	DT_Tdep_PT_Debug(3,
			 (phead, "1: SYNC_RECV  %p\n",
			  (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->ep_context.
							 bp,
							 DT_PERF_SYNC_RECV_BUFFER_ID)));

	/*
	 * Post recv and sync buffers
	 */
	if (!DT_post_recv_buffer(phead,
				 test_ptr->ep_context.ep_handle,
				 test_ptr->ep_context.bp,
				 DT_PERF_SYNC_RECV_BUFFER_ID,
				 DT_PERF_SYNC_BUFF_SIZE)) {
		/* error message printed by DT_post_recv_buffer */
		return false;
	}

	/*
	 * Fill in the test_ptr with relevant command info
	 */
	test_ptr->ep_context.op.transfer_type = test_ptr->cmd->op.transfer_type;
	test_ptr->ep_context.op.num_segs = test_ptr->cmd->op.num_segs;
	test_ptr->ep_context.op.seg_size = test_ptr->cmd->op.seg_size;

	/*
	 * Exchange remote memory info:  If we're going to participate
	 * in an RDMA, we need to allocate memory buffers and advertise
	 * them to the other side.
	 */
	test_ptr->ep_context.op.Rdma_Context = (DAT_RMR_CONTEXT) 0;
	test_ptr->ep_context.op.Rdma_Address = 0;
	test_ptr->ep_context.port = test_ptr->base_port;
	test_ptr->ep_context.pipeline_len = pipeline_len;

	return true;
}

/****************************************************************************/
void
DT_Performance_Test_Destroy(Per_Test_Data_t * pt_ptr,
			    Performance_Test_t * test_ptr,
			    DAT_BOOLEAN is_server)
{
	DAT_RETURN ret;
	DAT_EP_HANDLE ep_handle;
	DT_Tdep_Print_Head *phead;

	phead = pt_ptr->Params.phead;

	ep_handle = DAT_HANDLE_NULL;

	/* Free the per-op buffers */
	if (test_ptr->ep_context.op.bp) {
		if (!DT_Bpool_Destroy(test_ptr->pt_ptr,
				      phead, test_ptr->ep_context.op.bp)) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: Warning: Bpool destroy fails\n",
					  test_ptr->base_port);
			/* carry on trying, regardless */
		}
	}

	/* Free the remote memory info exchange buffers */
	if (test_ptr->ep_context.bp) {
		if (!DT_Bpool_Destroy(test_ptr->pt_ptr,
				      phead, test_ptr->ep_context.bp)) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: Warning: Bpool destroy fails\n",
					  test_ptr->base_port);
			/* carry on trying, regardless */
		}
	}

	/*
	 * Disconnect -- we may have left recv buffers posted, if we
	 *               bailed out mid-setup, or ran to completion
	 *               normally, so we use abrupt closure.
	 */
	if (test_ptr->ep_context.ep_handle) {
		ret = dat_ep_disconnect(test_ptr->ep_context.ep_handle,
					DAT_CLOSE_ABRUPT_FLAG);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: Warning: dat_ep_disconnect error %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			/* carry on trying, regardless */
		} else if (!DT_disco_event_wait(phead, test_ptr->conn_evd_hdl,
						&ep_handle)) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: bad disconnect event\n",
					  test_ptr->base_port);
		}
	}

	if (DAT_HANDLE_NULL != ep_handle) {
		/* Destroy the EP */
		ret = dat_ep_free(ep_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_ep_free error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			/* carry on trying, regardless */
		}
	}

	/* clean up the EVDs */
	if (test_ptr->conn_evd_hdl) {
		ret = DT_Tdep_evd_free(test_ptr->conn_evd_hdl);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_evd_free (conn) error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			/* fall through, keep trying */
		}
	}
	if (is_server) {
		if (test_ptr->creq_evd_hdl) {
			ret = DT_Tdep_evd_free(test_ptr->creq_evd_hdl);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "Test[" F64x
						  "]: dat_evd_free (creq) error: %s\n",
						  test_ptr->base_port,
						  DT_RetToString(ret));
				/* fall through, keep trying */
			}
		}
	}
	if (test_ptr->reqt_evd_hdl) {
		ret = DT_Tdep_evd_free(test_ptr->reqt_evd_hdl);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_evd_free (reqt) error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			/* fall through, keep trying */
		}
	}
	if (test_ptr->recv_evd_hdl) {
		ret = DT_Tdep_evd_free(test_ptr->recv_evd_hdl);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_evd_free (recv) error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			/* fall through, keep trying */
		}
	}

	/* clean up the PZ */
	if (test_ptr->pz_handle) {
		ret = dat_pz_free(test_ptr->pz_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_pz_free error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			/* fall through, keep trying */
		}
	}

	DT_MemListFree(test_ptr->pt_ptr, test_ptr);
	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: cleanup is done\n",
			  test_ptr->base_port));

}

/****************************************************************************/
bool
DT_performance_post_rdma_op(Performance_Ep_Context_t * ep_context,
			    DAT_EVD_HANDLE reqt_evd_hdl,
			    Performance_Stats_t * stats)
{
	unsigned int j;
	unsigned long int bytes;
	unsigned long pre_ctxt_num;
	unsigned long post_ctxt_num;
	DT_Mdep_TimeStamp pre_ts;
	DT_Mdep_TimeStamp post_ts;
	DAT_DTO_COOKIE cookie;
	DAT_RETURN ret;
	Performance_Test_Op_t *op = &ep_context->op;
	DAT_LMR_TRIPLET *iov = DT_Bpool_GetIOV(op->bp, 0);
	DAT_RMR_TRIPLET rmr_triplet;

	bytes = op->seg_size * op->num_segs;

	/* Prep the inputs */
	for (j = 0; j < op->num_segs; j++) {
		iov[j].virtual_address = (DAT_VADDR) (uintptr_t)
		    DT_Bpool_GetBuffer(op->bp, j);
		iov[j].segment_length = op->seg_size;
		iov[j].lmr_context = DT_Bpool_GetLMR(op->bp, j);
	}

	rmr_triplet.virtual_address = op->Rdma_Address;
	rmr_triplet.segment_length = op->seg_size * op->num_segs;
	rmr_triplet.rmr_context = op->Rdma_Context;

	cookie.as_ptr = NULL;

	if (RDMA_WRITE == op->transfer_type) {
		pre_ctxt_num = DT_Mdep_GetContextSwitchNum();
		pre_ts = DT_Mdep_GetTimeStamp();

		ret = dat_ep_post_rdma_write(ep_context->ep_handle,
					     op->num_segs,
					     iov,
					     cookie,
					     &rmr_triplet,
					     DAT_COMPLETION_DEFAULT_FLAG);

		post_ts = DT_Mdep_GetTimeStamp();
		post_ctxt_num = DT_Mdep_GetContextSwitchNum();

		stats->bytes += bytes;
	} else {
		pre_ctxt_num = DT_Mdep_GetContextSwitchNum();
		pre_ts = DT_Mdep_GetTimeStamp();

		ret = dat_ep_post_rdma_read(ep_context->ep_handle,
					    op->num_segs,
					    iov,
					    cookie,
					    &rmr_triplet,
					    DAT_COMPLETION_DEFAULT_FLAG);

		post_ts = DT_Mdep_GetTimeStamp();
		post_ctxt_num = DT_Mdep_GetContextSwitchNum();

		stats->bytes += bytes;
	}

	if (DAT_SUCCESS != ret) {
		return false;
	}

	DT_performance_stats_record_post(stats,
					 post_ctxt_num - pre_ctxt_num,
					 post_ts - pre_ts);

	return true;
}

/****************************************************************************/
unsigned int
DT_performance_reap(DT_Tdep_Print_Head * phead,
		    DAT_EVD_HANDLE evd_handle,
		    Performance_Mode_Type mode, Performance_Stats_t * stats)
{
	if (BLOCKING_MODE == mode) {
		return DT_performance_wait(phead, evd_handle, stats);
	} else {
		return DT_performance_poll(phead, evd_handle, stats);
	}
}

/****************************************************************************/
unsigned int
DT_performance_wait(DT_Tdep_Print_Head * phead,
		    DAT_EVD_HANDLE evd_handle, Performance_Stats_t * stats)
{
	DAT_COUNT i;
	DAT_COUNT queue_size;
	DAT_RETURN ret;
	DAT_EVENT event;
	unsigned long pre_ctxt_num;
	unsigned long post_ctxt_num;
	DT_Mdep_TimeStamp pre_ts;
	DT_Mdep_TimeStamp post_ts;

	queue_size = 0;

	pre_ctxt_num = DT_Mdep_GetContextSwitchNum();
	pre_ts = DT_Mdep_GetTimeStamp();

	ret = DT_Tdep_evd_wait(evd_handle, DAT_TIMEOUT_INFINITE, &event);

	post_ts = DT_Mdep_GetTimeStamp();
	post_ctxt_num = DT_Mdep_GetContextSwitchNum();

	if (DAT_SUCCESS != ret) {
		DT_Tdep_PT_Printf(phead,
				  "Test Error: dapl_event_dequeue failed: %s\n",
				  DT_RetToString(ret));
		return 0;
	} else if (event.event_number == DAT_DTO_COMPLETION_EVENT) {
		DT_performance_stats_record_reap(stats,
						 post_ctxt_num - pre_ctxt_num,
						 post_ts - pre_ts);
	} else {
		/* This should not happen. There has been an error if it does. */

		DT_Tdep_PT_Printf(phead,
				  "Warning: dapl_performance_wait swallowing %s event\n",
				  DT_EventToSTr(event.event_number));

		return 0;
	}

	for (i = 0; i < queue_size; i++) {
		pre_ctxt_num = DT_Mdep_GetContextSwitchNum();
		pre_ts = DT_Mdep_GetTimeStamp();

		ret = DT_Tdep_evd_dequeue(evd_handle, &event);

		post_ts = DT_Mdep_GetTimeStamp();
		post_ctxt_num = DT_Mdep_GetContextSwitchNum();

		if (DAT_GET_TYPE(ret) == DAT_QUEUE_EMPTY) {
			continue;
		} else if (DAT_SUCCESS != ret) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_dequeue failed: %s\n",
					  DT_RetToString(ret));
			return 0;
		} else if (event.event_number == DAT_DTO_COMPLETION_EVENT) {
			DT_performance_stats_record_reap(stats,
							 post_ctxt_num -
							 pre_ctxt_num,
							 post_ts - pre_ts);
		} else {
			/* This should not happen. There has been an error if it does. */

			DT_Tdep_PT_Printf(phead,
					  "Warning: dapl_performance_wait swallowing %s event\n",
					  DT_EventToSTr(event.event_number));

			return 0;
		}
	}

	return ++queue_size;
}

/****************************************************************************/
unsigned int
DT_performance_poll(DT_Tdep_Print_Head * phead,
		    DAT_EVD_HANDLE evd_handle, Performance_Stats_t * stats)
{
	DAT_RETURN ret;
	DAT_EVENT event;
	unsigned long pre_ctxt_num;
	unsigned long post_ctxt_num;
	DT_Mdep_TimeStamp pre_ts;
	DT_Mdep_TimeStamp post_ts;

	for (;;) {
		pre_ctxt_num = DT_Mdep_GetContextSwitchNum();
		pre_ts = DT_Mdep_GetTimeStamp();

		ret = DT_Tdep_evd_dequeue(evd_handle, &event);

		post_ts = DT_Mdep_GetTimeStamp();
		post_ctxt_num = DT_Mdep_GetContextSwitchNum();

		if (DAT_GET_TYPE(ret) == DAT_QUEUE_EMPTY) {
			continue;
		} else if (DAT_SUCCESS != ret) {
			DT_Tdep_PT_Printf(phead,
					  "Test Error: dapl_event_dequeue failed: %s\n",
					  DT_RetToString(ret));
			return 0;
		} else if (event.event_number == DAT_DTO_COMPLETION_EVENT) {
			DT_performance_stats_record_reap(stats,
							 post_ctxt_num -
							 pre_ctxt_num,
							 post_ts - pre_ts);
			return 1;
		} else {
			/* This should not happen. There has been an error if it does. */

			DT_Tdep_PT_Printf(phead,
					  "Warning: dapl_performance_wait swallowing %s event\n",
					  DT_EventToSTr(event.event_number));

			return 0;
		}
	}

	/*never reached */
	return 0;
}

void
DT_Performance_Cmd_PT_Print(DT_Tdep_Print_Head * phead, Performance_Cmd_t * cmd)
{
	DT_Tdep_PT_Printf(phead, "-------------------------------------\n");
	DT_Tdep_PT_Printf(phead, "PerfCmd.server_name              : %s\n",
			  cmd->server_name);
	DT_Tdep_PT_Printf(phead, "PerfCmd.dapl_name                : %s\n",
			  cmd->dapl_name);
	DT_Tdep_PT_Printf(phead, "PerfCmd.mode                     : %s\n",
			  (cmd->mode ==
			   BLOCKING_MODE) ? "BLOCKING" : "POLLING");
	DT_Tdep_PT_Printf(phead, "PerfCmd.num_iterations           : %d\n",
			  cmd->num_iterations);
	DT_Tdep_PT_Printf(phead, "PerfCmd.pipeline_len             : %d\n",
			  cmd->pipeline_len);
	DT_Tdep_PT_Printf(phead, "PerfCmd.op.transfer_type         : %s\n",
			  cmd->op.transfer_type ==
			  RDMA_READ ? "RDMA_READ" : cmd->op.transfer_type ==
			  RDMA_WRITE ? "RDMA_WRITE" : "SEND_RECV");
	DT_Tdep_PT_Printf(phead, "PerfCmd.op.num_segs              : %d\n",
			  cmd->op.num_segs);
	DT_Tdep_PT_Printf(phead, "PerfCmd.op.seg_size              : %d\n",
			  cmd->op.seg_size);
}
