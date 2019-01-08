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

/****************************************************************************/
void DT_Performance_Test_Server(void *var)
{
	Per_Test_Data_t *pt_ptr = var;
	Performance_Test_t *test_ptr = NULL;
	DT_Tdep_Print_Head *phead;
	int success = 1;

	phead = pt_ptr->Params.phead;
	DT_Tdep_PT_Debug(1, (phead, "Server: Starting performance test\n"));

	if (!DT_Performance_Test_Create(pt_ptr,
					pt_ptr->ps_ptr->ia_handle,
					(DAT_IA_ADDRESS_PTR) 0,
					true,
					pt_ptr->Client_Info.is_little_endian,
					&test_ptr)) {
		DT_Tdep_PT_Printf(phead, "Server: Resource Creation Failed\n");
		success = 0;
	}
	if (1 == success) {
		if (!DT_Performance_Test_Server_Connect(phead, test_ptr)) {
			success = 0;
			DT_Tdep_PT_Printf(phead, "Server: Connection Failed\n");
		}
	}

	if (1 == success) {
		if (!DT_Performance_Test_Server_Exchange(phead, test_ptr)) {
			success = 0;
			DT_Tdep_PT_Printf(phead, "Server: Test Failed\n");
		}
	}
#ifdef CM_BUSTED
    /*****  XXX Chill out a bit to give the kludged CM a chance ...
     *****/ DT_Mdep_Sleep(5000);
#endif

	DT_Performance_Test_Destroy(pt_ptr, test_ptr, true);

	DT_Tdep_PT_Printf(phead,
			  "Server: Finished performance test.  Detaching.\n");

	DT_Mdep_Thread_Detach(DT_Mdep_Thread_SELF());	/* AMM */
	DT_Thread_Destroy(pt_ptr->thread, pt_ptr);	/* destroy Master thread */

	DT_Mdep_Lock(&pt_ptr->ps_ptr->num_clients_lock);
	pt_ptr->ps_ptr->num_clients--;
	DT_Mdep_Unlock(&pt_ptr->ps_ptr->num_clients_lock);

	DT_PrintMemList(pt_ptr);	/* check if we return all space allocated */
	DT_Mdep_LockDestroy(&pt_ptr->Thread_counter_lock);
	DT_Mdep_LockDestroy(&pt_ptr->MemListLock);
	DT_Free_Per_Test_Data(pt_ptr);

	DT_Mdep_Unlock(&g_PerfTestLock);
	DT_Tdep_PT_Printf(phead,
			  "Server: Finished performance test.  Exiting.\n");

	DT_Mdep_Thread_EXIT(NULL);
}

/****************************************************************************/
bool
DT_Performance_Test_Server_Connect(DT_Tdep_Print_Head * phead,
				   Performance_Test_t * test_ptr)
{
	DAT_RETURN ret;
	bool status;
	DAT_RSP_HANDLE rsp_handle;
	DAT_PSP_HANDLE psp_handle;

	DAT_CR_ARRIVAL_EVENT_DATA cr_stat;
	DAT_CR_HANDLE cr_handle;
	DAT_EVENT_NUMBER event_num;

	rsp_handle = DAT_HANDLE_NULL;
	psp_handle = DAT_HANDLE_NULL;
#if 0				/* FIXME */
	if (test_ptr->cmd->use_rsp) {
		/*
		 * Server - create a single-use RSP and
		 *          await a connection for this EP
		 */
		ret = dat_rsp_create(test_ptr->ia_handle,
				     test_ptr->ep_context.port,
				     test_ptr->ep_context.ep_handle,
				     test_ptr->creq_evd_hdl, &rsp_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_rsp_create error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			status = false;
			goto psp_free;
		}

		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Server[" F64x "]: Listen on RSP port 0x" F64x
				  "\n", test_ptr->base_port,
				  test_ptr->ep_context.port));

		/* wait for the connection request */
		if (!DT_cr_event_wait(test_ptr->conn_evd_hdl, &cr_stat) ||
		    !DT_cr_check(&cr_stat,
				 DAT_HANDLE_NULL,
				 test_ptr->ep_context.port,
				 &cr_handle, "Server")) {
			status = false;
			goto psp_free;
		}

		/* what, me query?  just try to accept the connection */
		ret = dat_cr_accept(cr_handle,
				    test_ptr->ep_context.ep_handle,
				    0, (DAT_PVOID) 0 /* no private data */ );
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_cr_accept error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			/* cr_handle consumed on failure */
			status = false;
			goto psp_free;
		}

		/* wait for DAT_CONNECTION_EVENT_ESTABLISHED */
		if (!DT_conn_event_wait(test_ptr->ep_context.ep_handle,
					test_ptr->conn_evd_hdl, &event_num)) {
			/* error message printed by DT_conn_event_wait */
			status = false;
			goto psp_free;
		}

	} else
#endif				/* FIXME */
	{
		/*
		 * Server - use a short-lived PSP instead of an RSP
		 */
		status = true;

		ret = dat_psp_create(test_ptr->ia_handle,
				     test_ptr->ep_context.port,
				     test_ptr->creq_evd_hdl,
				     DAT_PSP_CONSUMER_FLAG, &psp_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_psp_create error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			status = false;
			psp_handle = DAT_HANDLE_NULL;
			return (status);
		}

	}

	/*
	 * Here's where we tell the main server process that
	 * this thread is ready to wait for a connection request
	 * from the remote end.
	 */
	DT_Mdep_wait_object_wakeup(&test_ptr->pt_ptr->synch_wait_object);

	DT_Tdep_PT_Debug(1,
			 (phead,
			  "Server[" F64x "]: Listen on PSP port 0x" F64x "\n",
			  test_ptr->base_port, test_ptr->ep_context.port));

	/* wait for a connection request */
	if (!DT_cr_event_wait(phead, test_ptr->creq_evd_hdl, &cr_stat) ||
	    !DT_cr_check(phead,
			 &cr_stat,
			 psp_handle,
			 test_ptr->ep_context.port, &cr_handle, "Server")) {
		status = false;
		goto psp_free;
	}

	/* what, me query?  just try to accept the connection */
	ret = dat_cr_accept(cr_handle,
			    test_ptr->ep_context.ep_handle,
			    0, (DAT_PVOID) 0 /* no private data */ );
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x "]: dat_cr_accept error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		/* cr_handle consumed on failure */
		status = false;
		goto psp_free;
	}

	/* wait for DAT_CONNECTION_EVENT_ESTABLISHED */
	if (!DT_conn_event_wait(phead,
				test_ptr->ep_context.ep_handle,
				test_ptr->conn_evd_hdl, &event_num)) {
		/* error message printed by DT_cr_event_wait */
		status = false;
		goto psp_free;
	}

	DT_Tdep_PT_Debug(1,
			 (phead,
			  "Server[" F64x "]: Accept on port 0x" F64x "\n",
			  test_ptr->base_port, test_ptr->ep_context.port));
      psp_free:
	if (DAT_HANDLE_NULL != psp_handle) {
		/* throw away single-use PSP */
		ret = dat_psp_free(psp_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_psp_free error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			status = false;
		}
	}
	if (DAT_HANDLE_NULL != rsp_handle) {
		/* throw away single-use PSP */
		ret = dat_rsp_free(rsp_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_rsp_free error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			status = false;
		}
	}
	/* end short-lived PSP */
#ifdef CM_BUSTED
    /*****  XXX Chill out a bit to give the kludged CM a chance ...
     *****/ DT_Mdep_Sleep(5000);
#endif

	return status;
}

/****************************************************************************/
bool
DT_Performance_Test_Server_Exchange(DT_Tdep_Print_Head * phead,
				    Performance_Test_t * test_ptr)
{
	DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
	RemoteMemoryInfo *rmi;
	DAT_DTO_COOKIE dto_cookie;

	test_ptr->ep_context.op.bp =
	    DT_BpoolAlloc(test_ptr->pt_ptr,
			  phead,
			  test_ptr->ia_handle,
			  test_ptr->pz_handle,
			  test_ptr->ep_context.ep_handle,
			  test_ptr->reqt_evd_hdl,
			  test_ptr->ep_context.op.seg_size,
			  test_ptr->ep_context.op.num_segs,
			  DAT_OPTIMAL_ALIGNMENT, true, true);

	if (!test_ptr->ep_context.op.bp) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x
				  "]: no memory for buffers (RDMA/RD)\n",
				  test_ptr->base_port);
		return false;
	}

	test_ptr->ep_context.op.Rdma_Context =
	    DT_Bpool_GetRMR(test_ptr->ep_context.op.bp, 0);
	test_ptr->ep_context.op.Rdma_Address = (DAT_VADDR) (uintptr_t)
	    DT_Bpool_GetBuffer(test_ptr->ep_context.op.bp, 0);

	/*
	 * Prep send buffer with memory information
	 */
	rmi = (RemoteMemoryInfo *) DT_Bpool_GetBuffer(test_ptr->ep_context.bp,
						      DT_PERF_SYNC_SEND_BUFFER_ID);

	rmi->rmr_context = test_ptr->ep_context.op.Rdma_Context;
	rmi->mem_address.as_64 = test_ptr->ep_context.op.Rdma_Address;

	if (rmi->mem_address.as_ptr) {
		DT_Tdep_PT_Debug(3, (phead,
				     "RemoteMemInfo va=" F64x ", ctx=%x\n",
				     rmi->mem_address.as_64, rmi->rmr_context));
	}

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

	/*
	 * Send our memory info
	 */
	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: Sending Sync Msg\n",
			  test_ptr->base_port));

	/* post the send buffer */
	if (!DT_post_send_buffer(phead,
				 test_ptr->ep_context.ep_handle,
				 test_ptr->ep_context.bp,
				 DT_PERF_SYNC_SEND_BUFFER_ID,
				 DT_PERF_SYNC_BUFF_SIZE)) {
		/* error message printed by DT_post_send_buffer */
		return false;
	}

	/* reap the send and verify it */
	dto_cookie.as_64 = LZERO;
	dto_cookie.as_ptr =
	    (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->ep_context.bp,
					   DT_PERF_SYNC_SEND_BUFFER_ID);
	if (!DT_dto_event_wait(phead, test_ptr->reqt_evd_hdl, &dto_stat) ||
	    !DT_dto_check(phead,
			  &dto_stat,
			  test_ptr->ep_context.ep_handle,
			  DT_PERF_SYNC_BUFF_SIZE,
			  dto_cookie, "Send Sync_Msg")) {
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

	DT_Tdep_PT_Debug(1, (phead, "Test[" F64x "]: Received Sync Msg\n",
			     test_ptr->base_port));

	return true;
}
