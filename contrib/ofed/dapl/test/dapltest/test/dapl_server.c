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

#ifdef DFLT_QLEN
#undef DFLT_QLEN
#endif

#define DFLT_QLEN 8		/* default event queue length */

int send_control_data(DT_Tdep_Print_Head * phead,
		      unsigned char *buffp,
		      Per_Server_Data_t * ps_ptr, Per_Test_Data_t * pt_ptr);

void DT_cs_Server(Params_t * params_ptr)
{
	Server_Cmd_t *Server_Cmd = &params_ptr->u.Server_Cmd;
	Client_Info_t *Client_Info = NULL;
	Transaction_Cmd_t *Transaction_Cmd = NULL;
	Quit_Cmd_t *Quit_Cmd = NULL;
	Performance_Cmd_t *Performance_Cmd = NULL;
	Per_Server_Data_t *ps_ptr = NULL;
	Per_Test_Data_t *pt_ptr = NULL;
	Started_server_t *temp_list = NULL;
	Started_server_t *pre_list = NULL;
	unsigned char *buffp = NULL;
	char *module = "DT_cs_Server";

	DAT_DTO_COOKIE dto_cookie;
	DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
	DAT_RETURN ret;
	DT_Tdep_Print_Head *phead;

	phead = params_ptr->phead;

	/* Check if device from command line already in use */
	temp_list = DT_started_server_list;
	while (temp_list) {
		if (strcmp(temp_list->devicename, Server_Cmd->dapl_name) == 0) {
			DT_Tdep_PT_Printf(phead,
					  "NOTICE: server already started for this NIC: %s\n",
					  Server_Cmd->dapl_name);
			return;
		}
		temp_list = temp_list->next;
	}

	/* Alloc memory for server list */
	temp_list =
	    (Started_server_t *) DT_Mdep_Malloc(sizeof(Started_server_t));
	if (temp_list == 0) {
		DT_Tdep_PT_Printf(phead, "no memory for server_list\n");
		return;
	}
	strcpy(temp_list->devicename, Server_Cmd->dapl_name);
	temp_list->next = DT_started_server_list;
	DT_started_server_list = temp_list;

	if (Server_Cmd->debug) {
		/* Echo our inputs if debugging */
		DT_Server_Cmd_PT_Print(phead, Server_Cmd);
	}

	/* Allocate memory for Per_Server_Data */
	ps_ptr =
	    (Per_Server_Data_t *) DT_Mdep_Malloc(sizeof(Per_Server_Data_t));
	if (ps_ptr == 0) {
		DT_Tdep_PT_Printf(phead, "no memory for ps_data\n");
		goto server_exit;
	}
	DT_Mdep_LockInit(&ps_ptr->num_clients_lock);
	ps_ptr->NextPortNumber = SERVER_PORT_NUMBER + 1;
	ps_ptr->num_clients = 0;

	/* Open the IA */
	ret = dat_ia_open(Server_Cmd->dapl_name,
			  DFLT_QLEN,
			  &ps_ptr->async_evd_hdl, &ps_ptr->ia_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: Could not open %s (%s)\n",
				  module,
				  Server_Cmd->dapl_name, DT_RetToString(ret));
		ps_ptr->ia_handle = DAT_HANDLE_NULL;
		goto server_exit;
	}
	DT_Tdep_PT_Debug(1,
			 (phead, "%s: IA %s opened\n", module,
			  Server_Cmd->dapl_name));

	/* Create a PZ */
	ret = dat_pz_create(ps_ptr->ia_handle, &ps_ptr->pz_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_pz_create error: %s\n",
				  module, DT_RetToString(ret));
		ps_ptr->pz_handle = DAT_HANDLE_NULL;
		goto server_exit;
	}
	DT_Tdep_PT_Debug(1, (phead, "%s: PZ created\n", module));

	/* Create 4 events - recv, request, connection-request, connect */
	ret = DT_Tdep_evd_create(ps_ptr->ia_handle,
				 DFLT_QLEN,
				 NULL, DAT_EVD_DTO_FLAG, &ps_ptr->recv_evd_hdl);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_evd_create (recv) failed %s\n",
				  module, DT_RetToString(ret));
		ps_ptr->recv_evd_hdl = DAT_HANDLE_NULL;
		goto server_exit;
	}
	ret = DT_Tdep_evd_create(ps_ptr->ia_handle,
				 DFLT_QLEN,
				 NULL,
				 DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG,
				 &ps_ptr->reqt_evd_hdl);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_evd_create (send) failed %s\n",
				  module, DT_RetToString(ret));
		ps_ptr->reqt_evd_hdl = DAT_HANDLE_NULL;
		goto server_exit;
	}
	ret = DT_Tdep_evd_create(ps_ptr->ia_handle,
				 DFLT_QLEN,
				 NULL, DAT_EVD_CR_FLAG, &ps_ptr->creq_evd_hdl);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_evd_create (cr) failed %s\n",
				  module, DT_RetToString(ret));
		ps_ptr->creq_evd_hdl = DAT_HANDLE_NULL;
		goto server_exit;
	}
	ret = DT_Tdep_evd_create(ps_ptr->ia_handle,
				 DFLT_QLEN,
				 NULL,
				 DAT_EVD_CONNECTION_FLAG,
				 &ps_ptr->conn_evd_hdl);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_evd_create (conn) failed %s\n",
				  module, DT_RetToString(ret));
		ps_ptr->conn_evd_hdl = DAT_HANDLE_NULL;
		goto server_exit;
	}

	/* Create the EP */
	ret = dat_ep_create(ps_ptr->ia_handle,	/* IA       */
			    ps_ptr->pz_handle,	/* PZ       */
			    ps_ptr->recv_evd_hdl,	/* recv     */
			    ps_ptr->reqt_evd_hdl,	/* request  */
			    ps_ptr->conn_evd_hdl,	/* connect  */
			    (DAT_EP_ATTR *) NULL, &ps_ptr->ep_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_ep_create error: %s\n",
				  module, DT_RetToString(ret));
		ps_ptr->ep_handle = DAT_HANDLE_NULL;
		goto server_exit;
	}
	DT_Tdep_PT_Debug(1, (phead, "%s: EP created\n", module));

	/* Create PSP */
	ret = dat_psp_create(ps_ptr->ia_handle,
			     SERVER_PORT_NUMBER,
			     ps_ptr->creq_evd_hdl,
			     DAT_PSP_CONSUMER_FLAG, &ps_ptr->psp_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "%s: dat_psp_create error: %s\n",
				  module, DT_RetToString(ret));
		ps_ptr->psp_handle = DAT_HANDLE_NULL;
		goto server_exit;
	}
	DT_Tdep_PT_Debug(1, (phead, "%s: PSP created\n", module));

	/*
	 * Create two buffers, large enough to hold ClientInfo and the largest
	 * command we'll use.
	 */
	ps_ptr->bpool = DT_BpoolAlloc(NULL, phead, ps_ptr->ia_handle, ps_ptr->pz_handle, ps_ptr->ep_handle, DAT_HANDLE_NULL,	/* no RMR */
				      DT_RoundSize(sizeof(Transaction_Cmd_t), 8192), 3,	/* num_buffers */
				      DAT_OPTIMAL_ALIGNMENT, false, false);
	if (ps_ptr->bpool == 0) {
		DT_Tdep_PT_Printf(phead,
				  "%s: no memory for command buffer pool.\n",
				  module);
		goto server_exit;
	}

	DT_Tdep_PT_Debug(3, (phead,
			     "Recv 0  %p\n",
			     (DAT_PVOID) DT_Bpool_GetBuffer(ps_ptr->bpool, 0)));
	DT_Tdep_PT_Debug(3, (phead,
			     "Recv 1  %p\n",
			     (DAT_PVOID) DT_Bpool_GetBuffer(ps_ptr->bpool, 1)));
	DT_Tdep_PT_Debug(3, (phead,
			     "SrvInfo 2  %p\n",
			     (DAT_PVOID) DT_Bpool_GetBuffer(ps_ptr->bpool, 2)));

	/* Initialize the performance test lock in case an incoming test
	 * is a performance test, so we can allow only one at a time.
	 * Otherwise, multiple performance tests cause a race condition
	 * between the server creating a new thread trying to allocate a
	 * PSP with the same ID as another thread that is either running
	 * a test on that same ID or hasn't yet destroyed it.  Only one
	 * PSP with a particular ID can exist at a time.  It's a
	 * de-facto shared resource that must be protected.
	 */
    /************************************************************************
     * Loop accepting connections and acting on them
     */
	for (; /* EVER */ ;) {
		DAT_CR_HANDLE cr_handle;
		DAT_CR_ARRIVAL_EVENT_DATA cr_stat;
		DAT_EVENT_NUMBER event_num;

		/* Set up the Per_Test_Data */
		pt_ptr = DT_Alloc_Per_Test_Data(phead);
		if (!pt_ptr) {
			DT_Tdep_PT_Printf(phead,
					  "%s: no memory for Per_Test_Data\n",
					  module);
			goto server_exit;
		}
		DT_MemListInit(pt_ptr);
		DT_Thread_Init(pt_ptr);
		pt_ptr->local_is_server = true;
		pt_ptr->ps_ptr = ps_ptr;
		memcpy((void *)(uintptr_t) & pt_ptr->Params,
		       (const void *)params_ptr, sizeof(Params_t));

		/* Server_Info, Client_Info, Params set up below */

		/* Gather whatever info we want about defaults */
		if (!DT_query(pt_ptr, ps_ptr->ia_handle, ps_ptr->ep_handle)) {
			goto server_exit;
		}

		/* Post recv buffers for ClientInfo and Transaction_Cmd_t */
		DT_Tdep_PT_Debug(1, (phead, "%s: Posting 2 recvs\n", module));
		if (!DT_post_recv_buffer(phead,
					 ps_ptr->ep_handle,
					 ps_ptr->bpool,
					 0,
					 DT_Bpool_GetBuffSize(ps_ptr->bpool,
							      0))) {
			DT_Tdep_PT_Printf(phead,
					  "%s: cannot post ClientInfo recv buffer\n",
					  module);
			goto server_exit;
		}
		if (!DT_post_recv_buffer(phead,
					 ps_ptr->ep_handle,
					 ps_ptr->bpool,
					 1,
					 DT_Bpool_GetBuffSize(ps_ptr->bpool,
							      1))) {
			DT_Tdep_PT_Printf(phead,
					  "%s: cannot post Transaction_Cmd_t recv buffer\n",
					  module);
			goto server_exit;
		}

		/* message to help automated test scripts know when to start the client */
		DT_Tdep_PT_Printf(phead,
				  "Dapltest: Service Point Ready - %s\n",
				  Server_Cmd->dapl_name);

		DT_Mdep_flush();

		DT_Tdep_PT_Debug(1,
				 (phead, "%s: Waiting for Connection Request\n",
				  module));
		if (!DT_cr_event_wait(phead, ps_ptr->creq_evd_hdl, &cr_stat)
		    || !DT_cr_check(phead, &cr_stat, ps_ptr->psp_handle,
				    SERVER_PORT_NUMBER, &cr_handle, module)) {

			DT_Tdep_PT_Printf(phead,
					  "CR Check failed, file %s line %d\n",
					  __FILE__, __LINE__);
			goto server_exit;
		}

		DT_Tdep_PT_Debug(1,
				 (phead, "%s: Accepting Connection Request\n",
				  module));
		ret =
		    dat_cr_accept(cr_handle, ps_ptr->ep_handle, 0,
				  (DAT_PVOID) 0);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_cr_accept error: %s\n",
					  module, DT_RetToString(ret));
			goto server_exit;
		}

		DT_Tdep_PT_Debug(1,
				 (phead, "%s: Awaiting connection ...\n",
				  module));
		if (!DT_conn_event_wait
		    (phead, ps_ptr->ep_handle, ps_ptr->conn_evd_hdl,
		     &event_num)) {
			DT_Tdep_PT_Printf(phead,
					  "%s: error awaiting conn-established event\n",
					  module);
			goto server_exit;
		}

		if (DT_dapltest_debug) {
			DT_Tdep_PT_Debug(1,
					 (phead, "%s: Connected!\n", module));
			get_ep_connection_state(phead, ps_ptr->ep_handle);
		}

		/* Wait for Client_Info */
		dto_cookie.as_64 = LZERO;
		dto_cookie.as_ptr =
		    (DAT_PVOID) (uintptr_t) DT_Bpool_GetBuffer(ps_ptr->bpool,
							       0);
		DT_Tdep_PT_Debug(1,
				 (phead, "%s: Waiting for Client_Info\n",
				  module));
		if (!DT_dto_event_wait(phead, ps_ptr->recv_evd_hdl, &dto_stat)
		    || !DT_dto_check(phead, &dto_stat, ps_ptr->ep_handle,
				     DT_Bpool_GetBuffSize(ps_ptr->bpool, 0),
				     dto_cookie, "Client_Info_Recv")) {
			goto server_exit;
		}
		DT_Tdep_PT_Debug(1, (phead, "%s: Got Client_Info\n", module));

		/* Extract the Client_Info */
		Client_Info =
		    (Client_Info_t *) DT_Bpool_GetBuffer(ps_ptr->bpool, 0);
		DT_Client_Info_Endian(Client_Info);
		memcpy((void *)(uintptr_t) & pt_ptr->Client_Info,
		       (const void *)Client_Info, sizeof(Client_Info_t));

		/* Wait for client's command info */
		dto_cookie.as_64 = LZERO;
		dto_cookie.as_ptr =
		    (DAT_PVOID) (uintptr_t) DT_Bpool_GetBuffer(ps_ptr->bpool,
							       1);
		DT_Tdep_PT_Debug(1,
				 (phead, "%s: Waiting for Client_Cmd_Info\n",
				  module));
		if (!DT_dto_event_wait(phead, ps_ptr->recv_evd_hdl, &dto_stat)
		    || !DT_dto_check(phead, &dto_stat, ps_ptr->ep_handle,
				     DT_Bpool_GetBuffSize(ps_ptr->bpool, 1),
				     dto_cookie, "Client_Cmd_Recv")) {
			goto server_exit;
		}

		/* Extract the client's command info */
		switch (Client_Info->test_type) {
		case TRANSACTION_TEST:
			{
				Transaction_Cmd = (Transaction_Cmd_t *)
				    DT_Bpool_GetBuffer(ps_ptr->bpool, 1);
				DT_Transaction_Cmd_Endian(Transaction_Cmd,
							  false);
				memcpy((void *)(uintptr_t) & pt_ptr->Params.u.
				       Transaction_Cmd,
				       (const void *)Transaction_Cmd,
				       sizeof(Transaction_Cmd_t));
				break;
			}

		case QUIT_TEST:
			{
				Quit_Cmd =
				    (Quit_Cmd_t *) DT_Bpool_GetBuffer(ps_ptr->
								      bpool, 1);
				DT_Quit_Cmd_Endian(Quit_Cmd, false);
				memcpy((void *)(uintptr_t) & pt_ptr->Params.u.
				       Quit_Cmd, (const void *)Quit_Cmd,
				       sizeof(Quit_Cmd_t));
				break;
			}

		case PERFORMANCE_TEST:
			{
				Performance_Cmd = (Performance_Cmd_t *)
				    DT_Bpool_GetBuffer(ps_ptr->bpool, 1);
				DT_Performance_Cmd_Endian(Performance_Cmd);
				memcpy((void *)(uintptr_t) & pt_ptr->Params.u.
				       Performance_Cmd,
				       (const void *)Performance_Cmd,
				       sizeof(Performance_Cmd_t));
				break;
			}

		default:
			{
				DT_Tdep_PT_Printf(phead,
						  "Unknown TestType received\n");
				goto server_exit;
				break;
			}
		}

		/* Setup Server Info */
		DT_Tdep_PT_Debug(1, (phead, "%s: Send Server_Info\n", module));
		pt_ptr->Server_Info.dapltest_version = DAPLTEST_VERSION;
		pt_ptr->Server_Info.is_little_endian =
		    DT_local_is_little_endian;
		/* reset port, don't eat up port space on long runs */
		if (ps_ptr->NextPortNumber >= SERVER_PORT_NUMBER + 1000)
			ps_ptr->NextPortNumber = SERVER_PORT_NUMBER + 1;
		pt_ptr->Server_Info.first_port_number = ps_ptr->NextPortNumber;
		ps_ptr->NextPortNumber += pt_ptr->Client_Info.total_threads;

		/* This had to be done here because the pt_ptr is being fed to
		 * the thread as its context, and if it isn't properly
		 * initialized before the thread spawns then the thread may
		 * incorrectly set up its PSP and the server will be listening
		 * on the WRONG PORT!
		 */

		switch (Client_Info->test_type) {
		case TRANSACTION_TEST:
			{
				/* create a thread to handle this pt_ptr; */
				ps_ptr->NextPortNumber +=
				    (pt_ptr->Params.u.Transaction_Cmd.
				     eps_per_thread -
				     1) * pt_ptr->Client_Info.total_threads;
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "%s: Creating Transaction Test Thread\n",
						  module));
				pt_ptr->thread =
				    DT_Thread_Create(pt_ptr,
						     DT_Transaction_Test_Server,
						     pt_ptr,
						     DT_MDEP_DEFAULT_STACK_SIZE);
				if (pt_ptr->thread == 0) {
					DT_Tdep_PT_Printf(phead,
							  "no memory to create thread\n");
					goto server_exit;
				}
				break;
			}

		case QUIT_TEST:
			{
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Client Requests Server to Quit\n"));
				(void)send_control_data(phead, buffp, ps_ptr,
							pt_ptr);
				goto server_exit;
				break;
			}

		case LIMIT_TEST:
			{
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Limit Test is Client-side Only!\n"));
				(void)send_control_data(phead, buffp, ps_ptr,
							pt_ptr);
				goto server_exit;
				break;
			}

		case PERFORMANCE_TEST:
			{
				/* create a thread to handle this pt_ptr; */
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "%s: Creating Performance Test Thread\n",
						  module));
				pt_ptr->thread =
				    DT_Thread_Create(pt_ptr,
						     DT_Performance_Test_Server,
						     pt_ptr,
						     DT_MDEP_DEFAULT_STACK_SIZE);
				if (pt_ptr->thread == 0) {
					DT_Tdep_PT_Printf(phead,
							  "no memory to create thread\n");
					goto server_exit;
				}
				/* take the performance test lock to serialize */
				DT_Mdep_Lock(&g_PerfTestLock);

				break;
			}

		case FFT_TEST:

		default:
			{
				DT_Tdep_PT_Printf(phead,
						  "Unknown TestType received\n");
				(void)send_control_data(phead, buffp, ps_ptr,
							pt_ptr);
				goto server_exit;
				break;
			}
		}

		/* Start the new test thread */
		DT_Tdep_PT_Debug(1,
				 (phead, "%s: Starting Test Thread\n", module));
		if (DT_Thread_Start(pt_ptr->thread) == false) {
			DT_Tdep_PT_Debug(1,
					 (phead,
					  "failed to start test thread\n"));
			goto server_exit;
		}

		buffp = DT_Bpool_GetBuffer(ps_ptr->bpool, 2);	/* 3rd buffer */
		memcpy((void *)buffp,
		       (const void *)&pt_ptr->Server_Info,
		       sizeof(Server_Info_t));
		DT_Server_Info_Endian((Server_Info_t *) buffp);

		/* Perform obligatory version check */
		if (pt_ptr->Client_Info.dapltest_version != DAPLTEST_VERSION) {
			DT_Tdep_PT_Printf(phead,
					  "%s: %s: Server %d, Client %d\n",
					  module,
					  "DAPLTEST VERSION MISMATCH",
					  DAPLTEST_VERSION,
					  pt_ptr->Client_Info.dapltest_version);
			goto server_exit;
		}
		DT_Tdep_PT_Debug(1, (phead, "%s: Version OK!\n", module));

		DT_Mdep_wait_object_wait(&pt_ptr->synch_wait_object,
					 DAT_TIMEOUT_INFINITE);

		/* Send the Server_Info */
		DT_Tdep_PT_Debug(1, (phead, "%s: Send Server_Info\n", module));

		if (!DT_post_send_buffer(phead, ps_ptr->ep_handle,
					 ps_ptr->bpool,
					 2,
					 DT_Bpool_GetBuffSize(ps_ptr->bpool,
							      2))) {
			DT_Tdep_PT_Printf(phead,
					  "%s: cannot send Server_Info\n",
					  module);
			goto server_exit;
		}
		/* reap the send and verify it */
		dto_cookie.as_64 = LZERO;
		dto_cookie.as_ptr =
		    (DAT_PVOID) (uintptr_t) DT_Bpool_GetBuffer(ps_ptr->bpool,
							       2);
		if (!DT_dto_event_wait(phead, ps_ptr->reqt_evd_hdl, &dto_stat)
		    || !DT_dto_check(phead, &dto_stat, ps_ptr->ep_handle,
				     DT_Bpool_GetBuffSize(ps_ptr->bpool, 2),
				     dto_cookie, "Server_Info_Send")) {
			goto server_exit;
		}

		/* Count this new client and get ready for the next */
		DT_Mdep_Lock(&ps_ptr->num_clients_lock);
		ps_ptr->num_clients++;
		DT_Mdep_Unlock(&ps_ptr->num_clients_lock);

		/* we passed the pt_ptr to the thread and must now 'forget' it */
		pt_ptr = 0;

#ifdef CM_BUSTED
		DT_Tdep_PT_Debug(1,
				 (phead,
				  "%s: Server exiting because provider does not support\n"
				  " multiple connections to the same service point\n",
				  module));
		/* Until connections are healthier we run just one test */
		break;
#else
		ret =
		    dat_ep_disconnect(ps_ptr->ep_handle,
				      DAT_CLOSE_GRACEFUL_FLAG);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "%s: dat_ep_disconnect fails: %s\n",
					  module, DT_RetToString(ret));
			goto server_exit;
		}
		if (!DT_disco_event_wait(phead, ps_ptr->conn_evd_hdl, NULL)) {
			DT_Tdep_PT_Printf(phead, "%s: bad disconnect event\n",
					  module);
			goto server_exit;
		}

		/* reset the EP to get back into the game */
		dat_ep_reset(ps_ptr->ep_handle);
		DT_Tdep_PT_Debug(1,
				 (phead, "%s: Waiting for another client...\n",
				  module));
#endif
	}			/* end loop accepting connections */

    /************************************************************************
     * Finished (or had an error) so clean up and go home
     */
      server_exit:

	/* Wait until all of our clients are gone */
	DT_Tdep_PT_Debug(1,
			 (phead, "%s: Waiting for clients to all go away...\n",
			  module));
	while (ps_ptr && ps_ptr->num_clients > 0) {
		DT_Mdep_Sleep(100);
	}

	/* Clean up the Per_Test_Data (if any) */
	DT_Tdep_PT_Debug(1, (phead, "%s: Cleaning up ...\n", module));
	if (pt_ptr) {
		DT_Mdep_LockDestroy(&pt_ptr->Thread_counter_lock);
		DT_Mdep_LockDestroy(&pt_ptr->MemListLock);
		DT_Free_Per_Test_Data(pt_ptr);
	}

	/* Clean up the Per_Server_Data */
	if (ps_ptr) {
		/*
		 * disconnect the most recent EP
		 *
		 * we also get here on error, hence abrupt closure to
		 * flush any lingering buffers posted.
		 */
		if (ps_ptr->ep_handle) {
			ret = dat_ep_disconnect(ps_ptr->ep_handle,
						DAT_CLOSE_ABRUPT_FLAG);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_ep_disconnect fails: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			} else if (!DT_disco_event_wait(phead,
							ps_ptr->conn_evd_hdl,
							NULL)) {
				DT_Tdep_PT_Printf(phead,
						  "%s: bad disconnect event\n",
						  module);
			}
		}

		/* Destroy the Bpool */
		if (ps_ptr->bpool) {
			if (!DT_Bpool_Destroy(NULL, phead, ps_ptr->bpool)) {
				DT_Tdep_PT_Printf(phead,
						  "%s: error destroying buffer pool\n",
						  module);
				/* keep trying */
			}
		}

		/* Free the PSP */
		if (ps_ptr->psp_handle) {
			ret = dat_psp_free(ps_ptr->psp_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_psp_free error: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			}
		}

		/* Free the EP */
		if (ps_ptr->ep_handle) {
			ret = dat_ep_free(ps_ptr->ep_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_ep_free error: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			}
		}

		/* Free the 4 EVDs */
		if (ps_ptr->conn_evd_hdl) {
			ret = DT_Tdep_evd_free(ps_ptr->conn_evd_hdl);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_evd_free (conn) error: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			}
		}
		if (ps_ptr->creq_evd_hdl) {
			ret = DT_Tdep_evd_free(ps_ptr->creq_evd_hdl);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_evd_free (creq) error: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			}
		}
		if (ps_ptr->reqt_evd_hdl) {
			ret = DT_Tdep_evd_free(ps_ptr->reqt_evd_hdl);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_evd_free (reqt) error: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			}
		}
		if (ps_ptr->recv_evd_hdl) {
			ret = DT_Tdep_evd_free(ps_ptr->recv_evd_hdl);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_evd_free (recv) error: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			}
		}

		/* Free the PZ */
		if (ps_ptr->pz_handle) {
			ret = dat_pz_free(ps_ptr->pz_handle);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_pz_free error: %s\n",
						  module, DT_RetToString(ret));
				/* keep trying */
			}
		}

		/* Close the IA */
		if (ps_ptr->ia_handle) {
			/* dat_ia_close cleans up async evd handle, too */
			ret =
			    dat_ia_close(ps_ptr->ia_handle,
					 DAT_CLOSE_GRACEFUL_FLAG);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "%s: dat_ia_close (graceful) error: %s\n",
						  module, DT_RetToString(ret));
				ret =
				    dat_ia_close(ps_ptr->ia_handle,
						 DAT_CLOSE_ABRUPT_FLAG);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "%s: dat_ia_close (abrupt) error: %s\n",
							  module,
							  DT_RetToString(ret));
				}
				/* keep trying */
			} else {
				DT_Tdep_PT_Debug(1,
						 (phead, "%s: IA %s closed\n",
						  module,
						  Server_Cmd->dapl_name));
			}
		}

		/* Destroy the ps_ptr */
		DT_Mdep_LockDestroy(&ps_ptr->num_clients_lock);
		DT_Mdep_Free(ps_ptr);
	}

	/* end if ps_ptr */
	/* Clean up the server list */
	pre_list = 0;
	temp_list = DT_started_server_list;
	while (temp_list) {
		if (strcmp(temp_list->devicename, Server_Cmd->dapl_name) == 0) {
			if (pre_list == 0) {	/* first one */
				DT_started_server_list = temp_list->next;
			} else {
				pre_list->next = temp_list->next;
			}
			DT_Mdep_Free(temp_list);
			break;
		}
		pre_list = temp_list;
		temp_list = temp_list->next;
	}

	DT_Tdep_PT_Printf(phead,
			  "%s (%s):  Exiting.\n", module,
			  Server_Cmd->dapl_name);
}

int
send_control_data(DT_Tdep_Print_Head * phead,
		  unsigned char *buffp,
		  Per_Server_Data_t * ps_ptr, Per_Test_Data_t * pt_ptr)
{
	char *module = "send_control_data";
	DAT_DTO_COOKIE dto_cookie;
	DAT_DTO_COMPLETION_EVENT_DATA dto_stat;

	buffp = DT_Bpool_GetBuffer(ps_ptr->bpool, 2);	/* 3rd buffer */
	memcpy((void *)buffp,
	       (const void *)&pt_ptr->Server_Info, sizeof(Server_Info_t));
	DT_Server_Info_Endian((Server_Info_t *) buffp);

	if (!DT_post_send_buffer(phead, ps_ptr->ep_handle,
				 ps_ptr->bpool,
				 2, DT_Bpool_GetBuffSize(ps_ptr->bpool, 2))) {
		DT_Tdep_PT_Printf(phead, "%s: cannot send Server_Info\n",
				  module);
		return 1;
	}
	/* reap the send and verify it */
	dto_cookie.as_64 = LZERO;
	dto_cookie.as_ptr =
	    (DAT_PVOID) (uintptr_t) DT_Bpool_GetBuffer(ps_ptr->bpool, 2);
	if (!DT_dto_event_wait(phead, ps_ptr->reqt_evd_hdl, &dto_stat) ||
	    !DT_dto_check(phead,
			  &dto_stat,
			  ps_ptr->ep_handle,
			  DT_Bpool_GetBuffSize(ps_ptr->bpool, 2),
			  dto_cookie, "Server_Info_Send")) {
		return 1;
	}

	return 0;
}

void
DT_Server_Cmd_PT_Print(DT_Tdep_Print_Head * phead, Server_Cmd_t * Server_Cmd)
{
	DT_Tdep_PT_Printf(phead, "Server_Cmd.debug:       %d\n",
			  Server_Cmd->debug);
	DT_Tdep_PT_Printf(phead, "Server_Cmd.dapl_name: %s\n",
			  Server_Cmd->dapl_name);
}
