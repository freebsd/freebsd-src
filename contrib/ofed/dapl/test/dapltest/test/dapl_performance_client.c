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

#define MAX_CONN_RETRY 8

/****************************************************************************/
DAT_RETURN
DT_Performance_Test_Client(Params_t * params_ptr,
			   Per_Test_Data_t * pt_ptr,
			   DAT_IA_HANDLE * ia_handle,
			   DAT_IA_ADDRESS_PTR remote_ia_addr)
{
	Performance_Test_t *test_ptr = NULL;
	int connected = 1;
	DT_Tdep_Print_Head *phead;
	DAT_RETURN rc;

	phead = pt_ptr->Params.phead;

	DT_Tdep_PT_Debug(1, (phead, "Client: Starting performance test\n"));

	if (!DT_Performance_Test_Create(pt_ptr,
					ia_handle,
					remote_ia_addr,
					false,
					pt_ptr->Server_Info.is_little_endian,
					&test_ptr)) {
		DT_Tdep_PT_Debug(1,
				 (phead, "Client: Resource Creation Failed\n"));
		connected = 0;
	} else if (!DT_Performance_Test_Client_Connect(phead, test_ptr)) {
		DT_Tdep_PT_Debug(1, (phead, "Client: Connection Failed\n"));
		connected = 0;
	}

	if (connected) {
		if (!DT_Performance_Test_Client_Exchange
		    (params_ptr, phead, test_ptr)) {
			DT_Tdep_PT_Debug(1, (phead, "Client: Test Failed\n"));
		}
	}
	/* If we never connected, then the test will hang here
	 *  because in the destroy of the test it waits for a
	 *  disconnect event which will never arrive, simply
	 *  because there was never a connection.
	 */

	DT_Performance_Test_Destroy(pt_ptr, test_ptr, false);

#ifdef CM_BUSTED
    /*****  XXX Chill out a bit to give the kludged CM a chance ...
     *****/ DT_Mdep_Sleep(5000);
#endif

	DT_Tdep_PT_Debug(1, (phead, "Client: Finished performance test\n"));

	return (connected ? DAT_SUCCESS : DAT_INSUFFICIENT_RESOURCES);
}

/****************************************************************************/
bool
DT_Performance_Test_Client_Connect(DT_Tdep_Print_Head * phead,
				   Performance_Test_t * test_ptr)
{
	DAT_RETURN ret;
	DAT_EVENT_NUMBER event_num;
	unsigned int retry_cnt = 0;

	/*
	 * Client - connect
	 */
	DT_Tdep_PT_Debug(1,
			 (phead,
			  "Client[" F64x "]: Connect on port 0x" F64x "\n",
			  test_ptr->base_port, test_ptr->ep_context.port));

      retry:
	ret = dat_ep_connect(test_ptr->ep_context.ep_handle, test_ptr->remote_ia_addr, test_ptr->ep_context.port, DAT_TIMEOUT_INFINITE, 0, (DAT_PVOID) 0,	/* no private data */
			     test_ptr->cmd->qos, DAT_CONNECT_DEFAULT_FLAG);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x "]: dat_ep_connect error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		return false;
	}

	/* wait for DAT_CONNECTION_EVENT_ESTABLISHED */
	if (!DT_conn_event_wait(phead,
				test_ptr->ep_context.ep_handle,
				test_ptr->conn_evd_hdl, &event_num)) {
		if (event_num == DAT_CONNECTION_EVENT_PEER_REJECTED) {
			DT_Mdep_Sleep(1000);
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: retrying connection...\n",
					  test_ptr->base_port);
			retry_cnt++;
			if (retry_cnt < MAX_CONN_RETRY) {
				goto retry;
			}
		}
		/* error message printed by DT_cr_event_wait */
		return false;
	}
#ifdef CM_BUSTED
    /*****  XXX Chill out a bit to give the kludged CM a chance ...
     *****/ DT_Mdep_Sleep(5000);
#endif

	DT_Tdep_PT_Debug(1,
			 (phead, "Client[" F64x "]: Got Connection\n",
			  test_ptr->base_port));

	return true;
}

/****************************************************************************/
static bool
DT_Performance_Test_Client_Phase1(DT_Tdep_Print_Head * phead,
				  Performance_Test_t * test_ptr,
				  Performance_Stats_t * stats)
{
	DT_Mdep_TimeStamp pre_ts;
	DT_Mdep_TimeStamp post_ts;
	DT_CpuStat pre_cpu_stat;
	DT_CpuStat post_cpu_stat;
	unsigned int post_cnt;
	unsigned int reap_cnt;

	/*
	 * measure bandwidth, OPS, and CPU utilization
	 */

	if (!DT_Mdep_GetCpuStat(&pre_cpu_stat)) {
		return false;
	}

	pre_ts = DT_Mdep_GetTimeStamp();

	/*
	 * Fill the pipe
	 */

	for (post_cnt = 0;
	     post_cnt < (unsigned int)test_ptr->ep_context.pipeline_len;
	     post_cnt++) {
		if (!DT_performance_post_rdma_op
		    (&test_ptr->ep_context, test_ptr->reqt_evd_hdl, stats)) {
			DT_Tdep_PT_Debug(1,
					 (phead,
					  "Test[" F64x "]: Post %i failed\n",
					  test_ptr->base_port, post_cnt));
			return false;
		}
	}

	/*
	 * Reap completions and repost
	 */

	for (reap_cnt = 0; reap_cnt < test_ptr->cmd->num_iterations;) {
		unsigned int cur_reap_cnt;
		unsigned int cur_post_cnt;
		unsigned int cur_post_i;

		cur_reap_cnt = DT_performance_reap(phead,
						   test_ptr->reqt_evd_hdl,
						   test_ptr->cmd->mode, stats);

		if (0 == cur_reap_cnt) {
			DT_Tdep_PT_Debug(1,
					 (phead,
					  "Test[" F64x "]: Poll %i failed\n",
					  test_ptr->base_port, reap_cnt));
			return false;
		}

		/* repost */
		cur_post_cnt = DT_min(test_ptr->cmd->num_iterations - post_cnt,
				      cur_reap_cnt);

		for (cur_post_i = 0; cur_post_i < cur_post_cnt; cur_post_i++) {
			if (!DT_performance_post_rdma_op(&test_ptr->ep_context,
							 test_ptr->reqt_evd_hdl,
							 stats)) {
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Test[" F64x
						  "]: Post %i failed\n",
						  test_ptr->base_port,
						  post_cnt));
				return false;
			}
		}

		reap_cnt += cur_reap_cnt;
		post_cnt += cur_post_cnt;
	}

	/* end time and update stats */
	post_ts = DT_Mdep_GetTimeStamp();
	stats->time_ts = post_ts - pre_ts;
	stats->num_ops = test_ptr->cmd->num_iterations;

	if (!DT_Mdep_GetCpuStat(&post_cpu_stat)) {
		return false;
	}

	/* calculate CPU utilization */
	{
		unsigned long int system;
		unsigned long int user;
		unsigned long int idle;
		unsigned long int total;

		system = post_cpu_stat.system - pre_cpu_stat.system;
		user = post_cpu_stat.user - pre_cpu_stat.user;
		idle = post_cpu_stat.idle - pre_cpu_stat.idle;

		total = system + user + idle;

		if (0 == total) {
			stats->cpu_utilization = 0.0;
		} else {
			stats->cpu_utilization =
			    (double)1.0 - ((double)idle / (double)total);
			stats->cpu_utilization *= 100.0;
		}
	}

	return true;
}

/****************************************************************************/
static bool
DT_Performance_Test_Client_Phase2(DT_Tdep_Print_Head * phead,
				  Performance_Test_t * test_ptr,
				  Performance_Stats_t * stats)
{
	DAT_LMR_TRIPLET *iov;
	DAT_RMR_TRIPLET rmr_triplet;
	DAT_DTO_COOKIE cookie;
	DAT_EVENT event;
	DAT_RETURN ret;
	Performance_Ep_Context_t *ep_context;
	Performance_Test_Op_t *op;
	DT_Mdep_TimeStamp pre_ts;
	DT_Mdep_TimeStamp post_ts;
	unsigned long int bytes;
	unsigned int i;

	/*
	 * measure latency
	 */

	ep_context = &test_ptr->ep_context;
	op = &ep_context->op;
	iov = DT_Bpool_GetIOV(op->bp, 0);

	bytes = op->seg_size * op->num_segs;

	/* Prep the inputs */
	for (i = 0; i < op->num_segs; i++) {
		iov[i].virtual_address = (DAT_VADDR) (uintptr_t)
		    DT_Bpool_GetBuffer(op->bp, i);
		iov[i].segment_length = op->seg_size;
		iov[i].lmr_context = DT_Bpool_GetLMR(op->bp, i);
	}

	rmr_triplet.virtual_address = op->Rdma_Address;
	rmr_triplet.segment_length = op->seg_size * op->num_segs;
	rmr_triplet.rmr_context = op->Rdma_Context;

	cookie.as_ptr = NULL;

	for (i = 0; i < test_ptr->cmd->num_iterations; i++) {
		if (RDMA_WRITE == op->transfer_type) {
			pre_ts = DT_Mdep_GetTimeStamp();

			ret = dat_ep_post_rdma_write(ep_context->ep_handle,
						     op->num_segs,
						     iov,
						     cookie,
						     &rmr_triplet,
						     DAT_COMPLETION_DEFAULT_FLAG);
		} else {
			pre_ts = DT_Mdep_GetTimeStamp();

			ret = dat_ep_post_rdma_read(ep_context->ep_handle,
						    op->num_segs,
						    iov,
						    cookie,
						    &rmr_triplet,
						    DAT_COMPLETION_DEFAULT_FLAG);
		}

		if (DAT_SUCCESS != ret) {
			return false;
		}

		for (;;) {
			DT_Mdep_Schedule();
			ret = DT_Tdep_evd_dequeue(test_ptr->reqt_evd_hdl,
						  &event);

			post_ts = DT_Mdep_GetTimeStamp();

			if (DAT_GET_TYPE(ret) == DAT_QUEUE_EMPTY) {
				continue;
			} else if (DAT_SUCCESS != ret) {
				DT_Tdep_PT_Printf(phead,
						  "Test Error: dapl_event_dequeue failed: %s\n",
						  DT_RetToString(ret));
				return false;
			} else if (event.event_number ==
				   DAT_DTO_COMPLETION_EVENT) {
				DT_performance_stats_record_latency(stats,
								    post_ts -
								    pre_ts);
				break;
			} else {	/* error */

				DT_Tdep_PT_Printf(phead,
						  "Warning: dapl_performance_wait swallowing %s event\n",
						  DT_EventToSTr(event.
								event_number));

				return false;
			}
		}
	}

	return true;
}

/****************************************************************************/
bool
DT_Performance_Test_Client_Exchange(Params_t * params_ptr,
				    DT_Tdep_Print_Head * phead,
				    Performance_Test_t * test_ptr)
{
	DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
	DAT_DTO_COOKIE dto_cookie;
	Performance_Stats_t stats;
	RemoteMemoryInfo *rmi;

	test_ptr->ep_context.op.bp =
	    DT_BpoolAlloc(test_ptr->pt_ptr,
			  phead,
			  test_ptr->ia_handle,
			  test_ptr->pz_handle,
			  test_ptr->ep_context.ep_handle,
			  test_ptr->reqt_evd_hdl,
			  test_ptr->ep_context.op.seg_size,
			  test_ptr->ep_context.op.num_segs,
			  DAT_OPTIMAL_ALIGNMENT, false, false);

	if (!test_ptr->ep_context.op.bp) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x
				  "]: no memory for buffers (RDMA/RD)\n",
				  test_ptr->base_port);
		return false;
	}

	/*
	 * Recv the other side's info
	 */
	DT_Tdep_PT_Debug(1, (phead, "Test[" F64x "]: Waiting for Sync Msg\n",
			     test_ptr->base_port));

	dto_cookie.as_64 = LZERO;
	dto_cookie.as_ptr =
	    (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->ep_context.bp,
					   DT_PERF_SYNC_RECV_BUFFER_ID);
	if (!DT_dto_event_wait(phead, test_ptr->recv_evd_hdl, &dto_stat) ||
	    !DT_dto_check(phead,
			  &dto_stat,
			  test_ptr->ep_context.ep_handle,
			  DT_PERF_SYNC_BUFF_SIZE,
			  dto_cookie, "Received Sync_Msg")) {
		return false;
	}

	/*
	 * Extract what we need
	 */
	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: Sync Msg Received\n",
			  test_ptr->base_port));
	rmi =
	    (RemoteMemoryInfo *) DT_Bpool_GetBuffer(test_ptr->ep_context.bp,
						    DT_PERF_SYNC_RECV_BUFFER_ID);

	/*
	 * If the client and server are of different endiannesses,
	 * we must correct the endianness of the handle and address
	 * we pass to the other side.  The other side cannot (and
	 * better not) interpret these values.
	 */
	if (DT_local_is_little_endian != test_ptr->is_remote_little_endian) {
		rmi->rmr_context = DT_EndianMemHandle(rmi->rmr_context);
		rmi->mem_address.as_64 =
		    DT_EndianMemAddress(rmi->mem_address.as_64);
	}

	test_ptr->ep_context.op.Rdma_Context = rmi->rmr_context;
	test_ptr->ep_context.op.Rdma_Address = rmi->mem_address.as_64;

	DT_Tdep_PT_Debug(3, (phead,
			     "Got RemoteMemInfo [ va=" F64x ", ctx=%x ]\n",
			     test_ptr->ep_context.op.Rdma_Address,
			     test_ptr->ep_context.op.Rdma_Context));

	/*
	 * Get to work ...
	 */
	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: Begin...\n",
			  test_ptr->base_port));

	DT_performance_stats_init(&stats);

	if (!DT_Performance_Test_Client_Phase1(phead, test_ptr, &stats)) {
		return false;
	}

	if (!DT_Performance_Test_Client_Phase2(phead, test_ptr, &stats)) {
		return false;
	}

	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: Sending Sync Msg\n",
			  test_ptr->base_port));

	if (!DT_post_send_buffer(phead,
				 test_ptr->ep_context.ep_handle,
				 test_ptr->ep_context.bp,
				 DT_PERF_SYNC_SEND_BUFFER_ID,
				 DT_PERF_SYNC_BUFF_SIZE)) {
		/* error message printed by DT_post_send_buffer */
		return false;
	}

	dto_cookie.as_64 = LZERO;
	dto_cookie.as_ptr =
	    (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->ep_context.bp,
					   DT_PERF_SYNC_SEND_BUFFER_ID);
	if (!DT_dto_event_wait(phead, test_ptr->reqt_evd_hdl, &dto_stat) ||
	    !DT_dto_check(phead,
			  &dto_stat,
			  test_ptr->ep_context.ep_handle,
			  DT_PERF_SYNC_BUFF_SIZE,
			  dto_cookie, "Client_Sync_Send")) {
		return false;
	}
	DT_performance_stats_print(params_ptr, phead, &stats, test_ptr->cmd,
				   test_ptr);

	return true;
}
