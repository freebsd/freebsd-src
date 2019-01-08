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

#if defined(DFLT_QLEN)
#undef DFLT_QLEN
#endif

#define DFLT_QLEN      8	/* default event queue length */
#define MAX_CONN_RETRY 8

/*
 * Client control routine Connect to the server, send the command across.
 * Then start the client-side of the test - creating threads as needed
 */
DAT_RETURN
DT_cs_Client(Params_t * params_ptr,
	     char *dapl_name, char *server_name, DAT_UINT32 total_threads)
{
	Per_Test_Data_t *pt_ptr = NULL;
	DAT_IA_HANDLE ia_handle = DAT_HANDLE_NULL;
	DAT_PZ_HANDLE pz_handle = DAT_HANDLE_NULL;
	DAT_EVD_HANDLE recv_evd_hdl = DAT_HANDLE_NULL;
	DAT_EVD_HANDLE reqt_evd_hdl = DAT_HANDLE_NULL;
	DAT_EVD_HANDLE conn_evd_hdl = DAT_HANDLE_NULL;
	DAT_EVD_HANDLE async_evd_hdl = DAT_HANDLE_NULL;
	DAT_EP_HANDLE ep_handle = DAT_HANDLE_NULL;
	Server_Info_t *sinfo = NULL;
	Transaction_Cmd_t *Transaction_Cmd = NULL;
	Quit_Cmd_t *Quit_Cmd = NULL;
	Performance_Cmd_t *Performance_Cmd = NULL;
	Bpool *bpool = NULL;
	DAT_IA_ADDRESS_PTR server_netaddr = NULL;
	char *module = "DT_cs_Client";
	unsigned int did_connect = 0;
	unsigned int retry_cnt = 0;
	DAT_DTO_COOKIE dto_cookie;

	DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
	DAT_EVENT_NUMBER event_num;
	unsigned char *buffp;
	DAT_RETURN ret, rc;
	DT_Tdep_Print_Head *phead;

	phead = params_ptr->phead;

	dto_cookie.as_64 = LZERO;

	DT_Tdep_PT_Printf(phead, "%s: Starting Test ... \n", module);
	DT_Mdep_Schedule();
	/* Set up the  Per_Test_Data */
	pt_ptr = DT_Alloc_Per_Test_Data(phead);
	if (!pt_ptr) {
		DT_Tdep_PT_Printf(phead, "%s: no memory for Per_Test_Data\n",
				  module);
		return DAT_INSUFFICIENT_RESOURCES;
	}
	DT_MemListInit(pt_ptr);	/* init MemlistLock and memListHead */
	DT_Thread_Init(pt_ptr);	/* init ThreadLock and threadcount */
	pt_ptr->local_is_server = false;
	pt_ptr->Client_Info.dapltest_version = DAPLTEST_VERSION;
	pt_ptr->Client_Info.is_little_endian = DT_local_is_little_endian;
	pt_ptr->Client_Info.test_type = params_ptr->test_type;
	pt_ptr->Client_Info.total_threads = total_threads;
	memcpy((void *)(uintptr_t) & pt_ptr->Params,
	       (const void *)params_ptr, sizeof(Params_t));

	/* Allocate and fill in the Server's address */
	server_netaddr = &params_ptr->server_netaddr;

	/* Open the IA */
	ret = dat_ia_open(dapl_name, DFLT_QLEN, &async_evd_hdl, &ia_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: Could not open %s (%s)\n",
				  module, dapl_name, DT_RetToString(ret));
		ia_handle = DAT_HANDLE_NULL;
		goto client_exit;
	}
	DT_Tdep_PT_Debug(1, (phead, "%s: IA %s opened\n", module, dapl_name));

	/* Create a PZ */
	ret = dat_pz_create(ia_handle, &pz_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_pz_create error: %s\n",
				  module, DT_RetToString(ret));
		pz_handle = DAT_HANDLE_NULL;
		goto client_exit;
	}

	/* Create 3 events - recv, request, connect */

	ret = DT_Tdep_evd_create(ia_handle,
				 DFLT_QLEN,
				 NULL, DAT_EVD_DTO_FLAG, &recv_evd_hdl);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_evd_create (recv) failed %s\n",
				  module, DT_RetToString(ret));
		recv_evd_hdl = DAT_HANDLE_NULL;
		goto client_exit;
	}
	ret = DT_Tdep_evd_create(ia_handle,
				 DFLT_QLEN,
				 NULL,
				 DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG,
				 &reqt_evd_hdl);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_evd_create (send) failed %s\n",
				  module, DT_RetToString(ret));
		reqt_evd_hdl = DAT_HANDLE_NULL;
		goto client_exit;
	}
	ret = DT_Tdep_evd_create(ia_handle,
				 DFLT_QLEN,
				 NULL, DAT_EVD_CONNECTION_FLAG, &conn_evd_hdl);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_evd_create (conn) failed %s\n",
				  module, DT_RetToString(ret));
		conn_evd_hdl = DAT_HANDLE_NULL;
		goto client_exit;
	}

	/* Create an EP */
	ret = dat_ep_create(ia_handle,	/* IA       */
			    pz_handle,	/* PZ       */
			    recv_evd_hdl,	/* recv     */
			    reqt_evd_hdl,	/* request  */
			    conn_evd_hdl,	/* connect  */
			    (DAT_EP_ATTR *) NULL, &ep_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_ep_create error: %s\n",
				  module, DT_RetToString(ret));
		ep_handle = DAT_HANDLE_NULL;
		goto client_exit;
	}
	DT_Tdep_PT_Debug(1, (phead, "%s: EP created\n", module));

	/*
	 * Gather whatever info we want about defaults,
	 * and check that we can handle the requested parameters.
	 */
	if (!DT_query(pt_ptr, ia_handle, ep_handle) ||
	    !DT_check_params(pt_ptr, module)) {
		ret = DAT_INSUFFICIENT_RESOURCES;
		goto client_exit;
	}

	bpool = DT_BpoolAlloc(pt_ptr, phead, ia_handle, pz_handle, ep_handle, DAT_HANDLE_NULL,	/* no RMR */
			      DT_RoundSize(sizeof(Transaction_Cmd_t), 8192), 3,	/* num_buffers */
			      DAT_OPTIMAL_ALIGNMENT, false, false);
	if (bpool == 0) {
		DT_Tdep_PT_Printf(phead,
				  "%s: no memory for command buffer pool.\n",
				  module);
		ret =
		    DAT_ERROR(DAT_INSUFFICIENT_RESOURCES, DAT_RESOURCE_MEMORY);
		goto client_exit;
	}

	DT_Tdep_PT_Debug(3, (phead,
			     "RecvSrvInfo 0  %p\n",
			     (DAT_PVOID) DT_Bpool_GetBuffer(bpool, 0)));
	DT_Tdep_PT_Debug(3, (phead,
			     "SndCliInfo 1  %p\n",
			     (DAT_PVOID) DT_Bpool_GetBuffer(bpool, 1)));
	DT_Tdep_PT_Debug(3, (phead,
			     "SndCommand 2  %p\n",
			     (DAT_PVOID) DT_Bpool_GetBuffer(bpool, 2)));

	/* Post recv buffer for Server_Info (1st buffer in pool) */
	DT_Tdep_PT_Debug(1, (phead, "%s: Posting 1 recv buffer\n", module));
      retry_repost:
	if (!DT_post_recv_buffer(phead,
				 ep_handle,
				 bpool, 0, DT_Bpool_GetBuffSize(bpool, 0))) {
		DT_Tdep_PT_Printf(phead,
				  "%s: cannot post Server_Info recv buffer.\n",
				  module);
		ret = DAT_INSUFFICIENT_RESOURCES;
		goto client_exit;
	}

	DT_Tdep_PT_Debug(1, (phead, "%s: Connect Endpoint\n", module));
      retry:
	ret = dat_ep_connect(ep_handle, server_netaddr, SERVER_PORT_NUMBER, DAT_TIMEOUT_INFINITE, 0, (DAT_PVOID) 0,	/* no private data */
			     params_ptr->ReliabilityLevel,
			     DAT_CONNECT_DEFAULT_FLAG);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: Cannot connect Endpoint %s\n",
				  module, DT_RetToString(ret));
		goto client_exit;
	}

	DT_Tdep_PT_Debug(1, (phead, "%s: Await connection ...\n", module));
	if (!DT_conn_event_wait(phead, ep_handle, conn_evd_hdl, &event_num)) {
		if (event_num == DAT_CONNECTION_EVENT_PEER_REJECTED) {
			DAT_EVENT event;
			DAT_COUNT drained = 0;

			DT_Mdep_Sleep(1000);
			DT_Tdep_PT_Printf(phead,
					  "%s: retrying connection...\n",
					  module);
			retry_cnt++;
			/*
			 * See if any buffers were flushed as a result of
			 * the REJECT; clean them up and repost if so
			 */
			dat_ep_reset(ep_handle);
			do {
				rc = DT_Tdep_evd_dequeue(recv_evd_hdl, &event);
				drained++;
			} while (DAT_GET_TYPE(rc) != DAT_QUEUE_EMPTY);

			if (drained > 1 && retry_cnt < MAX_CONN_RETRY) {
				DT_Tdep_PT_Printf(phead, "Reposting!!! %d\n",
						  drained);
				goto retry_repost;
			} else if (retry_cnt < MAX_CONN_RETRY) {
				goto retry;
			}
		}
		ret = DAT_INSUFFICIENT_RESOURCES;
		DT_Tdep_PT_Printf(phead, "%s: bad connection event\n", module);
		goto client_exit;
	}

	did_connect++;
	if (DT_dapltest_debug) {
		DT_Tdep_PT_Debug(1, (phead, "%s: Connected!\n", module));
		get_ep_connection_state(phead, ep_handle);
	}
#ifdef CM_BUSTED
    /*****  XXX Chill out a bit to give the kludged CM a chance ...
     *****/ DT_Mdep_Sleep(1000);
#endif

	/* Send Client_Info (using 2nd buffer in the pool) */
	DT_Tdep_PT_Debug(1, (phead, "%s: Sending Client_Info\n", module));
	buffp = DT_Bpool_GetBuffer(bpool, 1);
	memcpy((void *)buffp,
	       (const void *)&pt_ptr->Client_Info, sizeof(Client_Info_t));
	DT_Client_Info_Endian((Client_Info_t *) buffp);
	if (!DT_post_send_buffer(phead,
				 ep_handle,
				 bpool, 1, DT_Bpool_GetBuffSize(bpool, 1))) {
		DT_Tdep_PT_Printf(phead, "%s: cannot send Client_Info\n",
				  module);
		ret = DAT_INSUFFICIENT_RESOURCES;
		goto client_exit;
	}
	/* reap the send and verify it */
	dto_cookie.as_ptr =
	    (DAT_PVOID) (uintptr_t) DT_Bpool_GetBuffer(bpool, 1);
	DT_Tdep_PT_Debug(1,
			 (phead, "%s: Sent Client_Info - awaiting completion\n",
			  module));
	if (!DT_dto_event_wait(phead, reqt_evd_hdl, &dto_stat)
	    || !DT_dto_check(phead, &dto_stat, ep_handle,
			     DT_Bpool_GetBuffSize(bpool, 1), dto_cookie,
			     "Client_Info_Send")) {
		ret = DAT_INSUFFICIENT_RESOURCES;
		goto client_exit;
	}

	/* Set up the Command (using 3rd buffer in pool) */
	DT_Tdep_PT_Debug(1, (phead, "%s: Sending Command\n", module));
	buffp = DT_Bpool_GetBuffer(bpool, 2);
	switch (pt_ptr->Client_Info.test_type) {
	case TRANSACTION_TEST:
		{
			Transaction_Cmd = &pt_ptr->Params.u.Transaction_Cmd;
			memcpy((void *)buffp,
			       (const void *)Transaction_Cmd,
			       sizeof(Transaction_Cmd_t));
			DT_Transaction_Cmd_Endian((Transaction_Cmd_t *) buffp,
						  true);
			break;
		}

	case QUIT_TEST:
		{
			Quit_Cmd = &pt_ptr->Params.u.Quit_Cmd;
			memcpy((void *)buffp,
			       (const void *)Quit_Cmd, sizeof(Quit_Cmd_t));
			DT_Quit_Cmd_Endian((Quit_Cmd_t *) buffp, true);
			break;
		}

	case PERFORMANCE_TEST:
		{
			Performance_Cmd = &pt_ptr->Params.u.Performance_Cmd;
			memcpy((void *)buffp,
			       (const void *)Performance_Cmd,
			       sizeof(Performance_Cmd_t));
			DT_Performance_Cmd_Endian((Performance_Cmd_t *) buffp);
			break;
		}
	default:
		{
			DT_Tdep_PT_Printf(phead, "Unknown Test Type\n");
			ret = DAT_INVALID_PARAMETER;
			goto client_exit;
		}
	}

	/* Send the Command buffer */
	if (!DT_post_send_buffer(phead,
				 ep_handle,
				 bpool, 2, DT_Bpool_GetBuffSize(bpool, 2))) {
		DT_Tdep_PT_Printf(phead, "%s: cannot send Command\n", module);
		ret = DAT_INSUFFICIENT_RESOURCES;
		goto client_exit;
	}
	/* reap the send and verify it */
	dto_cookie.as_64 = LZERO;
	dto_cookie.as_ptr =
	    (DAT_PVOID) (uintptr_t) DT_Bpool_GetBuffer(bpool, 2);
	DT_Tdep_PT_Debug(1,
			 (phead, "%s: Sent Command - awaiting completion\n",
			  module));
	if (!DT_dto_event_wait(phead, reqt_evd_hdl, &dto_stat)
	    || !DT_dto_check(phead, &dto_stat, ep_handle,
			     DT_Bpool_GetBuffSize(bpool, 2), dto_cookie,
			     "Client_Cmd_Send")) {
		ret = DAT_INSUFFICIENT_RESOURCES;
		goto client_exit;
	}

    /************************************************************************/
	dto_cookie.as_64 = LZERO;
	dto_cookie.as_ptr =
	    (DAT_PVOID) (uintptr_t) DT_Bpool_GetBuffer(bpool, 0);
	DT_Tdep_PT_Debug(1, (phead, "%s: Waiting for Server_Info\n", module));
	if (!DT_dto_event_wait(phead, recv_evd_hdl, &dto_stat) ||
	    !DT_dto_check(phead,
			  &dto_stat,
			  ep_handle,
			  DT_Bpool_GetBuffSize(bpool, 0),
			  dto_cookie, "Server_Info_Recv")) {
		ret = DAT_INSUFFICIENT_RESOURCES;
		goto client_exit;
	}

	DT_Tdep_PT_Debug(1, (phead, "%s: Server_Info Received\n", module));
	sinfo = (Server_Info_t *) DT_Bpool_GetBuffer(bpool, 0);
	DT_Server_Info_Endian(sinfo);
	memcpy((void *)(uintptr_t) & pt_ptr->Server_Info,
	       (const void *)sinfo, sizeof(Server_Info_t));

	/* Perform obligatory version check */
	if (pt_ptr->Server_Info.dapltest_version != DAPLTEST_VERSION) {
		DT_Tdep_PT_Printf(phead,
				  "%s: DAPLTEST VERSION MISMATCH: Server %d, Client %d\n",
				  module,
				  pt_ptr->Server_Info.dapltest_version,
				  DAPLTEST_VERSION);
		ret = DAT_MODEL_NOT_SUPPORTED;
		goto client_exit;
	}
	DT_Tdep_PT_Debug(1, (phead, "%s: Version OK!\n", module));

	/* Dump out what we know, if requested */
	if (DT_dapltest_debug) {
		DT_Server_Info_Print(phead, &pt_ptr->Server_Info);
		DT_Client_Info_Print(phead, &pt_ptr->Client_Info);
	}

	/* Onward to running the actual test requested */
	switch (pt_ptr->Client_Info.test_type) {
	case TRANSACTION_TEST:
		{
			if (Transaction_Cmd->debug) {
				DT_Transaction_Cmd_PT_Print(phead,
							    Transaction_Cmd);
			}
			ret = DT_Transaction_Test_Client(pt_ptr,
							 ia_handle,
							 server_netaddr);
			break;
		}

	case QUIT_TEST:
		{
			DT_Quit_Cmd_PT_Print(phead, Quit_Cmd);
			ret = DAT_SUCCESS;
			break;
		}

	case PERFORMANCE_TEST:
		{
			if (Performance_Cmd->debug) {
				DT_Performance_Cmd_PT_Print(phead,
							    Performance_Cmd);
			}

			ret = DT_Performance_Test_Client(params_ptr,
							 pt_ptr,
							 ia_handle,
							 server_netaddr);
			break;
		}
	}

    /*********************************************************************
     * Done - clean up and go home
     * ret == function DAT_RETURN return code
     */
      client_exit:
	DT_Tdep_PT_Debug(1, (phead, "%s: Cleaning Up ...\n", module));

	/* Disconnect the EP */
	if (ep_handle) {
		/*
		 * graceful attempt might fail because we got here due to
		 * some error above, so we may as well try harder.
		 */
		rc = dat_ep_disconnect(ep_handle, DAT_CLOSE_ABRUPT_FLAG);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_ep_disconnect (abrupt) error: %s\n",
					  module, DT_RetToString(rc));
		} else if (did_connect &&
			   !DT_disco_event_wait(phead, conn_evd_hdl, NULL)) {
			DT_Tdep_PT_Printf(phead, "%s: bad disconnect event\n",
					  module);
		}
	}

	/* Free the bpool (if any) */
	DT_Bpool_Destroy(pt_ptr, phead, bpool);

	/* Free the EP */
	if (ep_handle) {
		DAT_EVENT event;
		/*
		 * Drain off outstanding DTOs that may have been
		 * generated by racing disconnects
		 */
		do {
			rc = DT_Tdep_evd_dequeue(recv_evd_hdl, &event);
		} while (rc == DAT_SUCCESS);

		rc = dat_ep_free(ep_handle);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_ep_free error: %s\n",
					  module, DT_RetToString(rc));
			/* keep going */
		}
	}

	/* Free the 3 EVDs */
	if (conn_evd_hdl) {
		rc = DT_Tdep_evd_free(conn_evd_hdl);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_evd_free (conn) error: %s\n",
					  module, DT_RetToString(rc));
			/* keep going */
		}
	}
	if (reqt_evd_hdl) {
		rc = DT_Tdep_evd_free(reqt_evd_hdl);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_evd_free (reqt) error: %s\n",
					  module, DT_RetToString(rc));
			/* keep going */
		}
	}
	if (recv_evd_hdl) {
		rc = DT_Tdep_evd_free(recv_evd_hdl);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_evd_free (recv) error: %s\n",
					  module, DT_RetToString(rc));
			/* keep going */
		}
	}

	/* Free the PZ */
	if (pz_handle) {
		rc = dat_pz_free(pz_handle);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_pz_free error: %s\n",
					  module, DT_RetToString(rc));
			/* keep going */
		}
	}

	/* Close the IA */
	if (ia_handle) {
		/* dat_ia_close cleans up async evd handle, too */
		rc = dat_ia_close(ia_handle, DAT_CLOSE_GRACEFUL_FLAG);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_ia_close (graceful) error: %s\n",
					  module, DT_RetToString(rc));
			rc = dat_ia_close(ia_handle, DAT_CLOSE_ABRUPT_FLAG);
			if (rc != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_ia_close (abrupt) error: %s\n",
						  module, DT_RetToString(rc));
			}
			/* keep going */
		} else {
			DT_Tdep_PT_Debug(1,
					 (phead, "%s: IA %s closed\n", module,
					  dapl_name));
		}
	}

	/* Free the Per_Test_Data */
	DT_Mdep_LockDestroy(&pt_ptr->Thread_counter_lock);
	DT_PrintMemList(pt_ptr);	/* check if we return all space allocated  */
	DT_Mdep_LockDestroy(&pt_ptr->MemListLock);
	DT_Free_Per_Test_Data(pt_ptr);

	DT_Tdep_PT_Printf(phead,
			  "%s: ========== End of Work -- Client Exiting\n",
			  module);
	return ret;
}
