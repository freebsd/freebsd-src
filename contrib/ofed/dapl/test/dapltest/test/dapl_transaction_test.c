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

#define RMI_SEND_BUFFER_ID      0
#define RMI_RECV_BUFFER_ID      1
#define SYNC_SEND_BUFFER_ID     2
#define SYNC_RECV_BUFFER_ID     3

/*
 * The sync buffers are sent to say "Go!" to the other side.
 * This is a handy place to test whether a zero-sized send into
 * a zero-sized buffer actually works.  If the client side hangs
 * in 'Wait for Sync Message' when this is zero, it's a DAPL bug.
 */
#define SYNC_BUFF_SIZE		64

#define DFLT_QLEN      8	/* default event queue length */
#define DFLT_TMO      10	/* default timeout (seconds)  */
#define MAX_CONN_RETRY 8

/****************************************************************************/
DAT_RETURN
DT_Transaction_Test_Client(Per_Test_Data_t * pt_ptr,
			   DAT_IA_HANDLE ia_handle,
			   DAT_IA_ADDRESS_PTR remote_ia_addr)
{
	Transaction_Cmd_t *cmd = &pt_ptr->Params.u.Transaction_Cmd;
	unsigned int i;
	DT_Tdep_Print_Head *phead;
	DAT_RETURN rc = DAT_SUCCESS;

	phead = pt_ptr->Params.phead;

	DT_init_transaction_stats(&pt_ptr->Client_Stats,
				  cmd->num_threads * cmd->eps_per_thread);

	/* Now go set up the client test threads */
	for (i = 0; i < cmd->num_threads; i++) {
		unsigned int port_num = pt_ptr->Server_Info.first_port_number
		    + i * cmd->eps_per_thread;

		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Client: Starting Client side of test\n"));
		if (!DT_Transaction_Create_Test
		    (pt_ptr, ia_handle, false, port_num,
		     pt_ptr->Server_Info.is_little_endian, remote_ia_addr)) {
			DT_Tdep_PT_Printf(phead,
					  "Client: Cannot Create Test!\n");
			rc = DAT_INSUFFICIENT_RESOURCES;
			break;
		}
#ifdef CM_BUSTED
	/*****  XXX Chill out a bit to give the kludged CM a chance ...
	 *****/ DT_Mdep_Sleep(5000);
#endif

	}

	/* Wait until end of all threads */
	while (pt_ptr->Thread_counter > 0) {
		DT_Mdep_Sleep(100);
	}

	DT_print_transaction_stats(phead,
				   &pt_ptr->Client_Stats,
				   cmd->num_threads, cmd->eps_per_thread);
	return rc;
}

/****************************************************************************/
void DT_Transaction_Test_Server(void *params)
{
	Per_Test_Data_t *pt_ptr = (Per_Test_Data_t *) params;
	Transaction_Cmd_t *cmd = &pt_ptr->Params.u.Transaction_Cmd;
	unsigned int i;
	DT_Tdep_Print_Head *phead;

	phead = pt_ptr->Params.phead;

	pt_ptr->Countdown_Counter = cmd->num_threads;

	for (i = 0; i < cmd->num_threads; i++) {
		unsigned int port_num = pt_ptr->Server_Info.first_port_number
		    + i * cmd->eps_per_thread;

		if (!DT_Transaction_Create_Test(pt_ptr,
						pt_ptr->ps_ptr->ia_handle,
						true,
						port_num,
						pt_ptr->Client_Info.
						is_little_endian,
						(DAT_IA_ADDRESS_PTR) 0)) {
			DT_Tdep_PT_Printf(phead,
					  "Server: Cannot Create Test!\n");
			break;
		}
#ifdef CM_BUSTED
	/*****  XXX Chill out a bit to give the kludged CM a chance ...
	 *****/ DT_Mdep_Sleep(5000);
#endif

	}

	/* Wait until end of all sub-threads */
	while (pt_ptr->Thread_counter > 1) {
		DT_Mdep_Sleep(100);
	}
	DT_Thread_Destroy(pt_ptr->thread, pt_ptr);	/* destroy Master thread */

	DT_Mdep_Lock(&pt_ptr->ps_ptr->num_clients_lock);
	pt_ptr->ps_ptr->num_clients--;
	DT_Mdep_Unlock(&pt_ptr->ps_ptr->num_clients_lock);

	/* NB: Server has no pt_ptr->remote_netaddr */
	DT_PrintMemList(pt_ptr);	/* check if we return all space allocated */
	DT_Mdep_LockDestroy(&pt_ptr->Thread_counter_lock);
	DT_Mdep_LockDestroy(&pt_ptr->MemListLock);
	DT_Free_Per_Test_Data(pt_ptr);
	DT_Tdep_PT_Printf(phead,
			  "Server: Transaction Test Finished for this client\n");
	/*
	 * check memory leaking DT_Tdep_PT_Printf(phead, "Server: App allocated Memory Left:
	 * %d\n", alloc_count);
	 */
}

/****************************************************************************/
/*
 * DT_Transaction_Create_Test()
 *
 * Initialize what we can in the test structure.  Then fork a thread to do the
 * work.
 */

bool
DT_Transaction_Create_Test(Per_Test_Data_t * pt_ptr,
			   DAT_IA_HANDLE * ia_handle,
			   DAT_BOOLEAN is_server,
			   unsigned int port_num,
			   DAT_BOOLEAN remote_is_little_endian,
			   DAT_IA_ADDRESS_PTR remote_ia_addr)
{
	Transaction_Test_t *test_ptr;
	DT_Tdep_Print_Head *phead;

	phead = pt_ptr->Params.phead;

	test_ptr = (Transaction_Test_t *) DT_MemListAlloc(pt_ptr,
							  "transaction_test_t",
							  TRANSACTIONTEST,
							  sizeof
							  (Transaction_Test_t));
	if (!test_ptr) {
		DT_Tdep_PT_Printf(phead,
				  "No Memory to create transaction test structure!\n");
		return false;
	}

	/* Unused fields zeroed by allocator */
	test_ptr->remote_is_little_endian = remote_is_little_endian;
	test_ptr->is_server = is_server;
	test_ptr->pt_ptr = pt_ptr;
	test_ptr->ia_handle = ia_handle;
	test_ptr->base_port = (DAT_CONN_QUAL) port_num;
	test_ptr->cmd = &pt_ptr->Params.u.Transaction_Cmd;
	test_ptr->time_out = DFLT_TMO * 1000;	/* DFLT_TMO seconds  */

	/* FIXME more analysis needs to go into determining the minimum  */
	/* possible value for DFLT_QLEN. This evd_length value will be   */
	/* used for all EVDs. There are a number of dependencies imposed */
	/* by this design (ex. min(cr_evd_len) != min(recv_evd_len) ).   */
	/* In the future it may be best to use individual values.        */
	test_ptr->evd_length = DT_max(DFLT_QLEN,
				      test_ptr->cmd->eps_per_thread *
				      test_ptr->cmd->num_ops);

	test_ptr->remote_ia_addr = remote_ia_addr;

	test_ptr->thread = DT_Thread_Create(pt_ptr,
					    DT_Transaction_Main,
					    test_ptr,
					    DT_MDEP_DEFAULT_STACK_SIZE);
	if (test_ptr->thread == 0) {
		DT_Tdep_PT_Printf(phead, "No memory!\n");
		DT_MemListFree(test_ptr->pt_ptr, test_ptr);
		return false;
	}
	DT_Thread_Start(test_ptr->thread);

	return true;
}

/****************************************************************************/
/*
 * Main Transaction Test Execution Routine
 *
 * Both client and server threads start here, with IA already open.
 * Each test thread establishes a connection with its counterpart.
 * They swap remote memory information (if necessary), then set up
 * buffers and local data structures.  When ready, the two sides
 * synchronize, then testing begins.
 */
void DT_Transaction_Main(void *param)
{
	Transaction_Test_t *test_ptr = (Transaction_Test_t *) param;
	DAT_RETURN ret;
	DAT_UINT32 i, j;
	bool success = false;
	Per_Test_Data_t *pt_ptr;
	Thread *thread;
	DAT_DTO_COOKIE dto_cookie;
	char *private_data_str;
	DAT_EVENT_NUMBER event_num;
	DT_Tdep_Print_Head *phead;

	pt_ptr = test_ptr->pt_ptr;
	thread = test_ptr->thread;
	phead = pt_ptr->Params.phead;
#ifdef CM_BUSTED
	private_data_str = "";
#else
	private_data_str = "DAPL and RDMA rule! Test 4321.";
#endif

	/* create a protection zone */
	ret = dat_pz_create(test_ptr->ia_handle, &test_ptr->pz_handle);
	if (ret != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x "]: dat_pz_create error: %s\n",
				  test_ptr->base_port, DT_RetToString(ret));
		test_ptr->pz_handle = DAT_HANDLE_NULL;
		goto test_failure;
	}

	/* Allocate per-EP data */
	test_ptr->ep_context = (Ep_Context_t *)
	    DT_MemListAlloc(pt_ptr,
			    "transaction_test",
			    EPCONTEXT,
			    test_ptr->cmd->eps_per_thread
			    * sizeof(Ep_Context_t));
	if (!test_ptr->ep_context) {
		DT_Tdep_PT_Printf(phead,
				  "Test[" F64x "]: no memory for EP context\n",
				  test_ptr->base_port);
		goto test_failure;
	}

	/*
	 * Set up the per-EP contexts:
	 *          create the EP
	 *          allocate buffers for remote memory info exchange
	 *          post the receive buffers
	 *          connect
	 *          set up buffers and remote memory info
	 *          send across our info
	 *          recv the other side's info and extract what we need
	 */
	for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
		DAT_EP_ATTR ep_attr;
		DAT_UINT32 buff_size = MAX_OPS * sizeof(RemoteMemoryInfo);

		/* create 4 EVDs - recv, request+RMR, conn-request, connect */
		ret =
		    DT_Tdep_evd_create(test_ptr->ia_handle,
				       test_ptr->evd_length, NULL,
				       DAT_EVD_DTO_FLAG,
				       &test_ptr->ep_context[i].recv_evd_hdl);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_evd_create (recv) error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			test_ptr->ep_context[i].recv_evd_hdl = DAT_HANDLE_NULL;
			goto test_failure;
		}

		ret =
		    DT_Tdep_evd_create(test_ptr->ia_handle,
				       test_ptr->evd_length, NULL,
				       DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG,
				       &test_ptr->ep_context[i].reqt_evd_hdl);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_evd_create (request) error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			test_ptr->ep_context[i].reqt_evd_hdl = DAT_HANDLE_NULL;
			goto test_failure;
		}

		if (pt_ptr->local_is_server) {
			/* Client-side doesn't need CR events */
			ret =
			    DT_Tdep_evd_create(test_ptr->ia_handle,
					       test_ptr->evd_length, NULL,
					       DAT_EVD_CR_FLAG,
					       &test_ptr->ep_context[i].
					       creq_evd_hdl);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "Test[" F64x
						  "]: dat_evd_create (cr) error: %s\n",
						  test_ptr->base_port,
						  DT_RetToString(ret));
				test_ptr->ep_context[i].creq_evd_hdl =
				    DAT_HANDLE_NULL;
				goto test_failure;
			}
		}

		ret =
		    DT_Tdep_evd_create(test_ptr->ia_handle,
				       test_ptr->evd_length, NULL,
				       DAT_EVD_CONNECTION_FLAG,
				       &test_ptr->ep_context[i].conn_evd_hdl);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_evd_create (conn) error: %s\n",
					  test_ptr->base_port,
					  DT_RetToString(ret));
			test_ptr->ep_context[i].conn_evd_hdl = DAT_HANDLE_NULL;
			goto test_failure;
		}

		/*
		 * Adjust default EP attributes to fit the requested test.
		 * This is simplistic; in that we don't count ops of each
		 * type and direction, checking EP limits.  We just try to
		 * be sure the EP's WQs are large enough.  The "+2" is for
		 * the RemoteMemInfo and Sync receive buffers.
		 */
		ep_attr = pt_ptr->ep_attr;
		if (ep_attr.max_recv_dtos < test_ptr->cmd->num_ops + 2) {
			ep_attr.max_recv_dtos = test_ptr->cmd->num_ops + 2;
		}
		if (ep_attr.max_request_dtos < test_ptr->cmd->num_ops + 2) {
			ep_attr.max_request_dtos = test_ptr->cmd->num_ops + 2;
		}

		/* Create EP */
		ret = dat_ep_create(test_ptr->ia_handle,	/* IA       */
				    test_ptr->pz_handle,	/* PZ       */
				    test_ptr->ep_context[i].recv_evd_hdl,	/* recv     */
				    test_ptr->ep_context[i].reqt_evd_hdl,	/* request  */
				    test_ptr->ep_context[i].conn_evd_hdl,	/* connect  */
				    &ep_attr,	/* EP attrs */
				    &test_ptr->ep_context[i].ep_handle);
		if (ret != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: dat_ep_create #%d error: %s\n",
					  test_ptr->base_port, i,
					  DT_RetToString(ret));
			test_ptr->ep_context[i].ep_handle = DAT_HANDLE_NULL;
			goto test_failure;
		}

		/*
		 * Allocate a buffer pool so we can exchange the
		 * remote memory info and initialize.
		 */
		test_ptr->ep_context[i].bp = DT_BpoolAlloc(pt_ptr, phead, test_ptr->ia_handle, test_ptr->pz_handle, test_ptr->ep_context[i].ep_handle, DAT_HANDLE_NULL,	/* rmr */
							   buff_size,
							   4,
							   DAT_OPTIMAL_ALIGNMENT,
							   false, false);
		if (!test_ptr->ep_context[i].bp) {
			DT_Tdep_PT_Printf(phead,
					  "Test[" F64x
					  "]: no memory for remote memory buffers\n",
					  test_ptr->base_port);
			goto test_failure;
		}

		DT_Tdep_PT_Debug(3, (phead,
				     "0: RMI_SEND  %p\n",
				     (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->
								    ep_context
								    [i].bp,
								    0)));
		DT_Tdep_PT_Debug(3,
				 (phead, "1: RMI_RECV  %p\n",
				  (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->
								 ep_context[i].
								 bp, 1)));
		DT_Tdep_PT_Debug(3,
				 (phead, "2: SYNC_SEND %p\n",
				  (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->
								 ep_context[i].
								 bp, 2)));
		DT_Tdep_PT_Debug(3,
				 (phead, "3: SYNC_RECV %p\n",
				  (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->
								 ep_context[i].
								 bp, 3)));

		/*
		 * Post recv and sync buffers
		 */
		if (!DT_post_recv_buffer(phead,
					 test_ptr->ep_context[i].ep_handle,
					 test_ptr->ep_context[i].bp,
					 RMI_RECV_BUFFER_ID, buff_size)) {
			/* error message printed by DT_post_recv_buffer */
			goto test_failure;
		}
		if (!DT_post_recv_buffer(phead,
					 test_ptr->ep_context[i].ep_handle,
					 test_ptr->ep_context[i].bp,
					 SYNC_RECV_BUFFER_ID, SYNC_BUFF_SIZE)) {
			/* error message printed by DT_post_recv_buffer */
			goto test_failure;
		}

		/*
		 * Establish the connection
		 */
		test_ptr->ep_context[i].ia_port = test_ptr->base_port + i;

		if (pt_ptr->local_is_server) {
			if (test_ptr->cmd->use_rsp) {
				/*
				 * Server - create a single-use RSP and
				 *          await a connection for this EP
				 */

				ret = dat_rsp_create(test_ptr->ia_handle,
						     test_ptr->ep_context[i].
						     ia_port,
						     test_ptr->ep_context[i].
						     ep_handle,
						     test_ptr->ep_context[i].
						     creq_evd_hdl,
						     &test_ptr->ep_context[i].
						     rsp_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_rsp_create #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					goto test_failure;
				}
			} else {
				ret = dat_psp_create(test_ptr->ia_handle,
						     test_ptr->ep_context[i].
						     ia_port,
						     test_ptr->ep_context[i].
						     creq_evd_hdl,
						     DAT_PSP_CONSUMER_FLAG,
						     &test_ptr->ep_context[i].
						     psp_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_psp_create #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					goto test_failure;
				}

				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Server[" F64x
						  "]: Listen #%d on PSP port 0x"
						  F64x "\n",
						  test_ptr->base_port, i,
						  test_ptr->ep_context[i].
						  ia_port));
			}
		}
	}

	/* Here's where we tell the server process that this thread is
	 * ready to wait for connection requests from the remote end.
	 * Modify the synch wait semantics at your own risk - if these
	 * signals and waits aren't here, there will be chronic
	 * connection rejection timing problems.
	 */
	if (pt_ptr->local_is_server) {
		DT_Mdep_Lock(&pt_ptr->Thread_counter_lock);
		pt_ptr->Countdown_Counter--;
		/* Deliberate pre-decrement.  Post decrement won't
		 * work here, so don't do it.
		 */
		if (pt_ptr->Countdown_Counter <= 0) {
			DT_Mdep_wait_object_wakeup(&pt_ptr->synch_wait_object);
		}

		DT_Mdep_Unlock(&pt_ptr->Thread_counter_lock);
	}

	for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
		DAT_UINT32 buff_size = MAX_OPS * sizeof(RemoteMemoryInfo);
		RemoteMemoryInfo *RemoteMemInfo;
		DAT_DTO_COMPLETION_EVENT_DATA dto_stat;
		DAT_CR_ARRIVAL_EVENT_DATA cr_stat;
		DAT_CR_HANDLE cr_handle;

		/*
		 * Establish the connection
		 */

		if (pt_ptr->local_is_server) {
			DAT_CR_PARAM cr_param;

			if (test_ptr->cmd->use_rsp) {

				/* wait for the connection request */
				if (!DT_cr_event_wait(phead,
						      test_ptr->ep_context[i].
						      creq_evd_hdl, &cr_stat)
				    || !DT_cr_check(phead, &cr_stat,
						    test_ptr->ep_context[i].
						    rsp_handle,
						    test_ptr->ep_context[i].
						    ia_port, &cr_handle,
						    "Server")) {
					goto test_failure;
				}

				ret = dat_cr_query(cr_handle,
						   DAT_CR_FIELD_ALL, &cr_param);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_cr_query #%d error:(%x) %s\n",
							  test_ptr->base_port,
							  i, ret,
							  DT_RetToString(ret));
				} else {
					if (strncmp
					    ((char *)cr_param.private_data,
					     private_data_str,
					     strlen(private_data_str)) != 0) {
						DT_Tdep_PT_Printf(phead,
								  "--Private Data mismatch!\n");
					} else {
						DT_Tdep_PT_Debug(1,
								 (phead,
								  "--Private Data: %d: <%s>\n",
								  cr_param.
								  private_data_size,
								  (char *)
								  cr_param.
								  private_data));
					}
				}

				/* what, me query?  just try to accept the connection */
				ret = dat_cr_accept(cr_handle, 0,	/* NULL for RSP */
						    0,
						    (DAT_PVOID) 0
						    /* no private data */ );
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_cr_accept #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					/* cr_handle consumed on failure */
					goto test_failure;
				}

				/* wait for DAT_CONNECTION_EVENT_ESTABLISHED */
				if (!DT_conn_event_wait(phead,
							test_ptr->ep_context[i].
							ep_handle,
							test_ptr->ep_context[i].
							conn_evd_hdl,
							&event_num)) {
					/* error message printed by DT_conn_event_wait */
					goto test_failure;
				}
				/* throw away single-use PSP */
				ret =
				    dat_rsp_free(test_ptr->ep_context[i].
						 rsp_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_rsp_free #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					goto test_failure;
				}

			} else {
				/*
				 * Server - use a short-lived PSP instead of an RSP
				 */
				/* wait for a connection request */
				if (!DT_cr_event_wait(phead,
						      test_ptr->ep_context[i].
						      creq_evd_hdl, &cr_stat)) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_psp_create #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					goto test_failure;
				}
				if (!DT_cr_check(phead,
						 &cr_stat,
						 test_ptr->ep_context[i].
						 psp_handle,
						 test_ptr->ep_context[i].
						 ia_port, &cr_handle,
						 "Server")) {
					goto test_failure;
				}

				ret = dat_cr_query(cr_handle,
						   DAT_CR_FIELD_ALL, &cr_param);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_cr_query #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
				} else {
					if (strncmp
					    ((char *)cr_param.private_data,
					     private_data_str,
					     strlen(private_data_str)) != 0) {
						DT_Tdep_PT_Printf(phead,
								  "--Private Data mismatch!\n");
					} else {
						DT_Tdep_PT_Debug(1,
								 (phead,
								  "--Private Data: %d: <%s>\n",
								  cr_param.
								  private_data_size,
								  (char *)
								  cr_param.
								  private_data));
					}
				}

				/* what, me query?  just try to accept the connection */
				ret = dat_cr_accept(cr_handle,
						    test_ptr->ep_context[i].
						    ep_handle, 0,
						    (DAT_PVOID) 0
						    /* no private data */ );
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_cr_accept #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					/* cr_handle consumed on failure */
					(void)dat_psp_free(test_ptr->
							   ep_context[i].
							   psp_handle);
					goto test_failure;
				}

				/* wait for DAT_CONNECTION_EVENT_ESTABLISHED */
				if (!DT_conn_event_wait(phead,
							test_ptr->ep_context[i].
							ep_handle,
							test_ptr->ep_context[i].
							conn_evd_hdl,
							&event_num)) {
					/* error message printed by DT_cr_event_wait */
					(void)dat_psp_free(&test_ptr->
							   ep_context[i].
							   psp_handle);
					goto test_failure;
				}

				/* throw away single-use PSP */
				ret =
				    dat_psp_free(test_ptr->ep_context[i].
						 psp_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_psp_free #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					goto test_failure;
				}
			}	/* end short-lived PSP */

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "Server[" F64x
					  "]: Accept #%d on port 0x" F64x "\n",
					  test_ptr->base_port, i,
					  test_ptr->ep_context[i].ia_port));
		} else {
			/*
			 * Client - connect
			 */
			unsigned int retry_cnt = 0;
			DAT_UINT32 buff_size =
			    MAX_OPS * sizeof(RemoteMemoryInfo);

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "Client[" F64x
					  "]: Connect #%d on port 0x" F64x "\n",
					  test_ptr->base_port, i,
					  test_ptr->ep_context[i].ia_port));

#ifdef CM_BUSTED
	    /*****  XXX Chill out a bit to give the kludged CM a chance ...
	     *****/ DT_Mdep_Sleep(5000);
#endif

		      retry:
			ret = dat_ep_connect(test_ptr->ep_context[i].ep_handle,
					     test_ptr->remote_ia_addr,
					     test_ptr->ep_context[i].ia_port,
					     DAT_TIMEOUT_INFINITE,
					     strlen(private_data_str),
					     private_data_str,
					     pt_ptr->Params.ReliabilityLevel,
					     DAT_CONNECT_DEFAULT_FLAG);
			if (ret != DAT_SUCCESS) {
				DT_Tdep_PT_Printf(phead,
						  "Test[" F64x
						  "]: dat_ep_connect #%d error: %s (0x%x)\n",
						  test_ptr->base_port, i,
						  DT_RetToString(ret), ret);
				goto test_failure;
			}

			/* wait for DAT_CONNECTION_EVENT_ESTABLISHED */
			if (!DT_conn_event_wait(phead,
						test_ptr->ep_context[i].
						ep_handle,
						test_ptr->ep_context[i].
						conn_evd_hdl, &event_num)) {
				/* error message printed by DT_cr_event_wait */
				if (event_num ==
				    DAT_CONNECTION_EVENT_PEER_REJECTED) {
					DT_Mdep_Sleep(1000);
					/*
					 * See if any buffers were flushed as a result of
					 * the REJECT; clean them up and repost if so
					 */
					{
						DAT_EVENT event;
						DAT_COUNT drained = 0;

						dat_ep_reset(test_ptr->
							     ep_context[i].
							     ep_handle);
						do {
							ret =
							    DT_Tdep_evd_dequeue
							    (test_ptr->
							     ep_context[i].
							     recv_evd_hdl,
							     &event);
							drained++;
						} while (DAT_GET_TYPE(ret) !=
							 DAT_QUEUE_EMPTY);

						if (drained > 1) {
							/*
							 * Post recv and sync buffers
							 */
							if (!DT_post_recv_buffer
							    (phead,
							     test_ptr->
							     ep_context[i].
							     ep_handle,
							     test_ptr->
							     ep_context[i].bp,
							     RMI_RECV_BUFFER_ID,
							     buff_size)) {
								/* error message printed by DT_post_recv_buffer */
								goto test_failure;
							}
							if (!DT_post_recv_buffer
							    (phead,
							     test_ptr->
							     ep_context[i].
							     ep_handle,
							     test_ptr->
							     ep_context[i].bp,
							     SYNC_RECV_BUFFER_ID,
							     SYNC_BUFF_SIZE)) {
								/* error message printed by DT_post_recv_buffer */
								goto test_failure;
							}
						}
					}
					DT_Tdep_PT_Printf(phead,
							  "Client[" F64x
							  "]: retrying connection...\n",
							  test_ptr->base_port);
					retry_cnt++;
					if (retry_cnt < MAX_CONN_RETRY) {
						goto retry;
					}
				}
				/* error message printed by DT_cr_event_wait */
				goto test_failure;
			}

			DT_Tdep_PT_Debug(1,
					 (phead,
					  "Client[" F64x
					  "]: Got Connection #%d\n",
					  test_ptr->base_port, i));
		}

#ifdef CM_BUSTED
	/*****  XXX Chill out a bit to give the kludged CM a chance ...
	 *****/ DT_Mdep_Sleep(5000);
#endif

		/*
		 * Fill in the test_ptr with relevant command info
		 */
		for (j = 0; j < test_ptr->cmd->num_ops; j++) {
			test_ptr->ep_context[i].op[j].server_initiated
			    = test_ptr->cmd->op[j].server_initiated;
			test_ptr->ep_context[i].op[j].transfer_type
			    = test_ptr->cmd->op[j].transfer_type;
			test_ptr->ep_context[i].op[j].num_segs
			    = test_ptr->cmd->op[j].num_segs;
			test_ptr->ep_context[i].op[j].seg_size
			    = test_ptr->cmd->op[j].seg_size;
			test_ptr->ep_context[i].op[j].reap_send_on_recv
			    = test_ptr->cmd->op[j].reap_send_on_recv;
		}

		/*
		 * Exchange remote memory info:  If we're going to participate
		 * in an RDMA, we need to allocate memory buffers and advertise
		 * them to the other side.
		 */
		for (j = 0; j < test_ptr->cmd->num_ops; j++) {
			DAT_BOOLEAN us;

			us = (pt_ptr->local_is_server &&
			      test_ptr->ep_context[i].op[j].server_initiated) ||
			    (!pt_ptr->local_is_server &&
			     !test_ptr->ep_context[i].op[j].server_initiated);

			test_ptr->ep_context[i].op[j].Rdma_Context =
			    (DAT_RMR_CONTEXT) 0;
			test_ptr->ep_context[i].op[j].Rdma_Address = 0;

			switch (test_ptr->ep_context[i].op[j].transfer_type) {
			case RDMA_READ:
				{
					test_ptr->ep_context[i].op[j].bp =
					    DT_BpoolAlloc(pt_ptr,
							  phead,
							  test_ptr->ia_handle,
							  test_ptr->pz_handle,
							  test_ptr->
							  ep_context[i].
							  ep_handle,
							  test_ptr->
							  ep_context[i].
							  reqt_evd_hdl,
							  test_ptr->
							  ep_context[i].op[j].
							  seg_size,
							  test_ptr->
							  ep_context[i].op[j].
							  num_segs,
							  DAT_OPTIMAL_ALIGNMENT,
							  false,
							  !us ? true : false);
					if (!test_ptr->ep_context[i].op[j].bp) {
						DT_Tdep_PT_Printf(phead,
								  "Test[" F64x
								  "]: no memory for buffers (RDMA/RD)\n",
								  test_ptr->
								  base_port);
						goto test_failure;
					}
					if (!us) {
						test_ptr->ep_context[i].op[j].
						    Rdma_Context =
						    DT_Bpool_GetRMR(test_ptr->
								    ep_context
								    [i].op[j].
								    bp, 0);
						test_ptr->ep_context[i].op[j].
						    Rdma_Address =
						    (DAT_VADDR) (uintptr_t)
						    DT_Bpool_GetBuffer
						    (test_ptr->ep_context[i].
						     op[j].bp, 0);
						DT_Tdep_PT_Debug(3,
								 (phead,
								  "not-us: RDMA/RD [ va="
								  F64x
								  ", ctxt=%x ]\n",
								  test_ptr->
								  ep_context[i].
								  op[j].
								  Rdma_Address,
								  test_ptr->
								  ep_context[i].
								  op[j].
								  Rdma_Context));
					}
					break;
				}

			case RDMA_WRITE:
				{
					test_ptr->ep_context[i].op[j].bp =
					    DT_BpoolAlloc(pt_ptr,
							  phead,
							  test_ptr->ia_handle,
							  test_ptr->pz_handle,
							  test_ptr->
							  ep_context[i].
							  ep_handle,
							  test_ptr->
							  ep_context[i].
							  reqt_evd_hdl,
							  test_ptr->
							  ep_context[i].op[j].
							  seg_size,
							  test_ptr->
							  ep_context[i].op[j].
							  num_segs,
							  DAT_OPTIMAL_ALIGNMENT,
							  !us ? true : false,
							  false);
					if (!test_ptr->ep_context[i].op[j].bp) {
						DT_Tdep_PT_Printf(phead,
								  "Test[" F64x
								  "]: no memory for buffers (RDMA/WR)\n",
								  test_ptr->
								  base_port);
						goto test_failure;
					}
					if (!us) {
						test_ptr->ep_context[i].op[j].
						    Rdma_Context =
						    DT_Bpool_GetRMR(test_ptr->
								    ep_context
								    [i].op[j].
								    bp, 0);
						test_ptr->ep_context[i].op[j].
						    Rdma_Address =
						    (DAT_VADDR) (uintptr_t)
						    DT_Bpool_GetBuffer
						    (test_ptr->ep_context[i].
						     op[j].bp, 0);
						DT_Tdep_PT_Debug(3,
								 (phead,
								  "not-us: RDMA/WR [ va="
								  F64x
								  ", ctxt=%x ]\n",
								  test_ptr->
								  ep_context[i].
								  op[j].
								  Rdma_Address,
								  test_ptr->
								  ep_context[i].
								  op[j].
								  Rdma_Context));
					}
					break;
				}

			case SEND_RECV:
				{
					test_ptr->ep_context[i].op[j].bp = DT_BpoolAlloc(pt_ptr, phead, test_ptr->ia_handle, test_ptr->pz_handle, test_ptr->ep_context[i].ep_handle, DAT_HANDLE_NULL,	/* rmr */
											 test_ptr->
											 ep_context
											 [i].
											 op
											 [j].
											 seg_size,
											 test_ptr->
											 ep_context
											 [i].
											 op
											 [j].
											 num_segs,
											 DAT_OPTIMAL_ALIGNMENT,
											 false,
											 false);
					if (!test_ptr->ep_context[i].op[j].bp) {
						DT_Tdep_PT_Printf(phead,
								  "Test[" F64x
								  "]: no memory for buffers (S/R)\n",
								  test_ptr->
								  base_port);
						goto test_failure;
					}

					DT_Tdep_PT_Debug(3, (phead,
							     "%d: S/R [ va=%p ]\n",
							     j, (DAT_PVOID)
							     DT_Bpool_GetBuffer
							     (test_ptr->
							      ep_context[i].
							      op[j].bp, 0)));
					break;
				}
			}
		}		/* end foreach op */

		/*
		 * Prep send buffer with memory information
		 */
		RemoteMemInfo = (RemoteMemoryInfo *)
		    DT_Bpool_GetBuffer(test_ptr->ep_context[i].bp,
				       RMI_SEND_BUFFER_ID);

		for (j = 0; j < test_ptr->cmd->num_ops; j++) {
			RemoteMemInfo[j].rmr_context =
			    test_ptr->ep_context[i].op[j].Rdma_Context;
			RemoteMemInfo[j].mem_address.as_64 =
			    test_ptr->ep_context[i].op[j].Rdma_Address;
			if (RemoteMemInfo[j].mem_address.as_64) {
				DT_Tdep_PT_Debug(3, (phead,
						     "RemoteMemInfo[%d] va="
						     F64x ", ctx=%x\n", j,
						     RemoteMemInfo[j].
						     mem_address.as_64,
						     RemoteMemInfo[j].
						     rmr_context));
			}
			/*
			 * If the client and server are of different endiannesses,
			 * we must correct the endianness of the handle and address
			 * we pass to the other side.  The other side cannot (and
			 * better not) interpret these values.
			 */
			if (DT_local_is_little_endian !=
			    test_ptr->remote_is_little_endian) {
				RemoteMemInfo[j].rmr_context =
				    DT_EndianMemHandle(RemoteMemInfo[j].
						       rmr_context);
				RemoteMemInfo[j].mem_address.as_64 =
				    DT_EndianMemAddress(RemoteMemInfo[j].
							mem_address.as_64);
			}
		}		/* end foreach op */

		/*
		 * Send our memory info. The client performs the first send to comply
		 * with the iWARP MPA protocol's "Connection Startup Rules".
		 */
		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Test[" F64x "]: Sending %s Memory Info\n",
				  test_ptr->base_port,
				  test_ptr->is_server ? "Server" : "Client"));

		if (!test_ptr->is_server) {

			/* post the send buffer */
			if (!DT_post_send_buffer(phead,
						 test_ptr->ep_context[i].
						 ep_handle,
						 test_ptr->ep_context[i].bp,
						 RMI_SEND_BUFFER_ID,
						 buff_size)) {
				/* error message printed by DT_post_send_buffer */
				goto test_failure;
			}
			/* reap the send and verify it */
			dto_cookie.as_64 = LZERO;
			dto_cookie.as_ptr =
			    (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->
							   ep_context[i].bp,
							   RMI_SEND_BUFFER_ID);
			if (!DT_dto_event_wait
			    (phead, test_ptr->ep_context[i].reqt_evd_hdl,
			     &dto_stat)
			    || !DT_dto_check(phead, &dto_stat,
					     test_ptr->ep_context[i].ep_handle,
					     buff_size, dto_cookie,
					     test_ptr->
					     is_server ? "Client_Mem_Info_Send"
					     : "Server_Mem_Info_Send")) {
				goto test_failure;
			}
		}

		/*
		 * Recv the other side's info
		 */
		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Test[" F64x
				  "]: Waiting for %s Memory Info\n",
				  test_ptr->base_port,
				  test_ptr->is_server ? "Client" : "Server"));
		dto_cookie.as_64 = LZERO;
		dto_cookie.as_ptr =
		    (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->ep_context[i].bp,
						   RMI_RECV_BUFFER_ID);
		if (!DT_dto_event_wait
		    (phead, test_ptr->ep_context[i].recv_evd_hdl, &dto_stat)
		    || !DT_dto_check(phead, &dto_stat,
				     test_ptr->ep_context[i].ep_handle,
				     buff_size, dto_cookie,
				     test_ptr->
				     is_server ? "Client_Mem_Info_Recv" :
				     "Server_Mem_Info_Recv")) {
			goto test_failure;
		}

		if (test_ptr->is_server) {
			/* post the send buffer */
			if (!DT_post_send_buffer(phead,
						 test_ptr->ep_context[i].
						 ep_handle,
						 test_ptr->ep_context[i].bp,
						 RMI_SEND_BUFFER_ID,
						 buff_size)) {
				/* error message printed by DT_post_send_buffer */
				goto test_failure;
			}
			/* reap the send and verify it */
			dto_cookie.as_64 = LZERO;
			dto_cookie.as_ptr =
			    (DAT_PVOID) DT_Bpool_GetBuffer(test_ptr->
							   ep_context[i].bp,
							   RMI_SEND_BUFFER_ID);
			if (!DT_dto_event_wait
			    (phead, test_ptr->ep_context[i].reqt_evd_hdl,
			     &dto_stat)
			    || !DT_dto_check(phead, &dto_stat,
					     test_ptr->ep_context[i].ep_handle,
					     buff_size, dto_cookie,
					     test_ptr->
					     is_server ? "Client_Mem_Info_Send"
					     : "Server_Mem_Info_Send")) {
				goto test_failure;
			}
		}

		/*
		 * Extract what we need
		 */
		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Test[" F64x "]: Memory Info received \n",
				  test_ptr->base_port));
		RemoteMemInfo = (RemoteMemoryInfo *)
		    DT_Bpool_GetBuffer(test_ptr->ep_context[i].bp,
				       RMI_RECV_BUFFER_ID);
		for (j = 0; j < test_ptr->cmd->num_ops; j++) {
			DAT_BOOLEAN us;

			us = (pt_ptr->local_is_server &&
			      test_ptr->ep_context[i].op[j].server_initiated) ||
			    (!pt_ptr->local_is_server &&
			     !test_ptr->ep_context[i].op[j].server_initiated);
			if (us &&
			    (test_ptr->ep_context[i].op[j].transfer_type ==
			     RDMA_READ
			     || test_ptr->ep_context[i].op[j].transfer_type ==
			     RDMA_WRITE)) {
				test_ptr->ep_context[i].op[j].Rdma_Context =
				    RemoteMemInfo[j].rmr_context;
				test_ptr->ep_context[i].op[j].Rdma_Address =
				    RemoteMemInfo[j].mem_address.as_64;
				DT_Tdep_PT_Debug(3,
						 (phead,
						  "Got RemoteMemInfo [ va=" F64x
						  ", ctx=%x ]\n",
						  test_ptr->ep_context[i].op[j].
						  Rdma_Address,
						  test_ptr->ep_context[i].op[j].
						  Rdma_Context));
			}
		}
	}			/* end foreach EP context */

	/*
	 * Dump out the state of the world if we're debugging
	 */
	if (test_ptr->cmd->debug) {
		DT_Print_Transaction_Test(phead, test_ptr);
	}

	/*
	 * Finally!  Run the test.
	 */
	success = DT_Transaction_Run(phead, test_ptr);

	/*
	 * Now clean up and go home
	 */
      test_failure:
	if (test_ptr->ep_context) {

		/* Foreach EP */
		for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
			DAT_EP_HANDLE ep_handle;

			ep_handle = DAT_HANDLE_NULL;

			/* Free the per-op buffers */
			for (j = 0; j < test_ptr->cmd->num_ops; j++) {
				if (test_ptr->ep_context[i].op[j].bp) {
					if (!DT_Bpool_Destroy(pt_ptr,
							      phead,
							      test_ptr->
							      ep_context[i].
							      op[j].bp)) {
						DT_Tdep_PT_Printf(phead,
								  "test[" F64x
								  "]: Warning: Bpool destroy fails\n",
								  test_ptr->
								  base_port);
						/* carry on trying, regardless */
					}
				}
			}

			/* Free the remote memory info exchange buffers */
			if (test_ptr->ep_context[i].bp) {
				if (!DT_Bpool_Destroy(pt_ptr,
						      phead,
						      test_ptr->ep_context[i].
						      bp)) {
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
			if (test_ptr->ep_context[i].ep_handle) {
				ret =
				    dat_ep_disconnect(test_ptr->ep_context[i].
						      ep_handle,
						      DAT_CLOSE_ABRUPT_FLAG);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: Warning: dat_ep_disconnect (%s) "
							  "#%d error %s\n",
							  test_ptr->base_port,
							  success ? "graceful" :
							  "abrupt", i,
							  DT_RetToString(ret));
					/* carry on trying, regardless */
				}
			}

			/*
			 * Wait on each of the outstanding EP handles. Some of them
			 * may be disconnected by the remote side, we are racing
			 * here.
			 */

			if (success) {	/* Ensure DT_Transaction_Run did not return error otherwise may get stuck waiting for disconnect event */
				if (!DT_disco_event_wait(phead,
							 test_ptr->
							 ep_context[i].
							 conn_evd_hdl,
							 &ep_handle)) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: bad disconnect event\n",
							  test_ptr->base_port);
				}
			}
			ep_handle = test_ptr->ep_context[i].ep_handle;

			/*
			 * Free the handle returned by the disconnect event.
			 * With multiple EPs, it may not be the EP we just
			 * disconnected as we are racing with the remote side
			 * disconnects.
			 */
			if (DAT_HANDLE_NULL != ep_handle) {
				DAT_EVENT event;
				/*
				 * Drain off outstanding DTOs that may have been
				 * generated by racing disconnects
				 */
				do {
					ret =
					    DT_Tdep_evd_dequeue(test_ptr->
								ep_context[i].
								recv_evd_hdl,
								&event);
				} while (ret == DAT_SUCCESS);
				/* Destroy the EP */
				ret = dat_ep_free(ep_handle);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_ep_free #%d error: %s\n",
							  test_ptr->base_port,
							  i,
							  DT_RetToString(ret));
					/* carry on trying, regardless */
				}
			}
			/* clean up the EVDs */
			if (test_ptr->ep_context[i].conn_evd_hdl) {
				ret =
				    DT_Tdep_evd_free(test_ptr->ep_context[i].
						     conn_evd_hdl);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_evd_free (conn) error: %s\n",
							  test_ptr->base_port,
							  DT_RetToString(ret));
				}
			}
			if (pt_ptr->local_is_server) {
				if (test_ptr->ep_context[i].creq_evd_hdl) {
					ret =
					    DT_Tdep_evd_free(test_ptr->
							     ep_context[i].
							     creq_evd_hdl);
					if (ret != DAT_SUCCESS) {
						DT_Tdep_PT_Printf(phead,
								  "Test[" F64x
								  "]: dat_evd_free (creq) error: %s\n",
								  test_ptr->
								  base_port,
								  DT_RetToString
								  (ret));
					}
				}
			}
			if (test_ptr->ep_context[i].reqt_evd_hdl) {
				ret =
				    DT_Tdep_evd_free(test_ptr->ep_context[i].
						     reqt_evd_hdl);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_evd_free (reqt) error: %s\n",
							  test_ptr->base_port,
							  DT_RetToString(ret));
				}
			}
			if (test_ptr->ep_context[i].recv_evd_hdl) {
				ret =
				    DT_Tdep_evd_free(test_ptr->ep_context[i].
						     recv_evd_hdl);
				if (ret != DAT_SUCCESS) {
					DT_Tdep_PT_Printf(phead,
							  "Test[" F64x
							  "]: dat_evd_free (recv) error: %s\n",
							  test_ptr->base_port,
							  DT_RetToString(ret));
				}
			}
		}		/* end foreach per-EP context */

		DT_MemListFree(pt_ptr, test_ptr->ep_context);
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

	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: cleanup is done\n",
			  test_ptr->base_port));
	DT_MemListFree(pt_ptr, test_ptr);
	DT_Thread_Destroy(thread, pt_ptr);
	DT_Mdep_Thread_Detach(DT_Mdep_Thread_SELF());	/* AMM */
	DT_Mdep_Thread_EXIT(NULL);	/* AMM */
}

/* -----------------------------------------------------------------------
 * The actual performance test
 */
bool
DT_Transaction_Run(DT_Tdep_Print_Head * phead, Transaction_Test_t * test_ptr)
{
	unsigned int op;
	unsigned int iteration;
	int bytes;
	bool ours;
	bool success = false;
	bool repost_recv;
	unsigned int i;

	/* pre-post all receive buffers */
	for (op = 0; op < test_ptr->cmd->num_ops; op++) {
		/* if it is a SEND/RECV, we must post receive buffers */
		if (test_ptr->ep_context[0].op[op].transfer_type == SEND_RECV) {
			ours = (test_ptr->is_server ==
				test_ptr->ep_context[0].op[op].
				server_initiated);
			if (!ours) {
				if (!DT_handle_post_recv_buf(phead,
							     test_ptr->
							     ep_context,
							     test_ptr->cmd->
							     eps_per_thread,
							     op)) {
					goto bail;
				}
			}
		}
	}

	/* initialize data if we are validating it */
	if (test_ptr->cmd->validate) {
		DT_Transaction_Validation_Fill(phead, test_ptr, 0);
	}

	/*
	 * Now that we've posted our receive buffers...
	 * synchronize with the other side.
	 */
	DT_Tdep_PT_Debug(1,
			 (phead,
			  "Test[" F64x "]: Synchronize with the other side\n",
			  test_ptr->base_port));

	/*
	 * Each server thread sends a sync message to the corresponding
	 * client thread.  All clients wait until all server threads
	 * have sent their sync messages. Then all clients send
	 * sync message.
	 *
	 * Since all of the events are directed to the same EVD,
	 * we do not use DT_dto_check(.) to verify the attributes
	 * of the sync message event. DT_dto_check(.) requires the
	 * comsumer to pass the expected EP, but we do not know
	 * what to expect. DAPL does not guarantee the order of
	 * completions across EPs. Therfore we only know that
	 * test_ptr->cmd->eps_per_thread number of completion events
	 * will be generated but not the order in which they will
	 * complete.
	 */

	if (test_ptr->is_server) {
		/*
		 * Server
		 */
		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Test[" F64x "]: Send Sync to Client\n",
				  test_ptr->base_port));
		for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
			if (!DT_post_send_buffer(phead,
						 test_ptr->ep_context[i].
						 ep_handle,
						 test_ptr->ep_context[i].bp,
						 SYNC_SEND_BUFFER_ID,
						 SYNC_BUFF_SIZE)) {
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Test[" F64x
						  "]: Server sync send error\n",
						  test_ptr->base_port));
				goto bail;
			}
		}
		for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
			DAT_DTO_COMPLETION_EVENT_DATA dto_stat;

			if (!DT_dto_event_wait(phead,
					       test_ptr->ep_context[i].
					       reqt_evd_hdl, &dto_stat)) {
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Test[" F64x
						  "]: Server sync send error\n",
						  test_ptr->base_port));

				goto bail;
			}
		}

		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Test[" F64x "]: Wait for Sync Message\n",
				  test_ptr->base_port));
		for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
			DAT_DTO_COMPLETION_EVENT_DATA dto_stat;

			if (!DT_dto_event_wait(phead,
					       test_ptr->ep_context[i].
					       recv_evd_hdl, &dto_stat)) {
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Test[" F64x
						  "]: Server sync recv error\n",
						  test_ptr->base_port));
				goto bail;
			}
		}
	} else {
		/*
		 * Client
		 */
		DT_Tdep_PT_Debug(1,
				 (phead,
				  "Test[" F64x "]: Wait for Sync Message\n",
				  test_ptr->base_port));
		for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
			DAT_DTO_COMPLETION_EVENT_DATA dto_stat;

			if (!DT_dto_event_wait(phead,
					       test_ptr->ep_context[i].
					       recv_evd_hdl, &dto_stat)) {
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Test[" F64x
						  "]: Client sync recv error\n",
						  test_ptr->base_port));
				goto bail;
			}
			DT_transaction_stats_set_ready(phead,
						       &test_ptr->pt_ptr->
						       Client_Stats);
		}

		/* check if it is time for client to send sync */
		if (!DT_transaction_stats_wait_for_all(phead,
						       &test_ptr->pt_ptr->
						       Client_Stats)) {
			goto bail;
		}

		DT_Tdep_PT_Debug(1,
				 (phead, "Test[" F64x "]: Send Sync Msg\n",
				  test_ptr->base_port));
		for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
			if (!DT_post_send_buffer(phead,
						 test_ptr->ep_context[i].
						 ep_handle,
						 test_ptr->ep_context[i].bp,
						 SYNC_SEND_BUFFER_ID,
						 SYNC_BUFF_SIZE)) {
				DT_Tdep_PT_Debug(1,
						 (phead,
						  "Test[" F64x
						  "]: Client sync send error\n",
						  test_ptr->base_port));
				goto bail;
			}
		}
		for (i = 0; i < test_ptr->cmd->eps_per_thread; i++) {
			DAT_DTO_COMPLETION_EVENT_DATA dto_stat;

			if (!DT_dto_event_wait(phead,
					       test_ptr->ep_context[i].
					       reqt_evd_hdl, &dto_stat)) {
				goto bail;
			}
		}
	}

	/*
	 * Get to work ...
	 */
	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: Begin...\n",
			  test_ptr->base_port));
	test_ptr->stats.start_time = DT_Mdep_GetTime();

	for (iteration = 0;
	     iteration < test_ptr->cmd->num_iterations; iteration++) {

		DT_Tdep_PT_Debug(1, (phead, "iteration: %d\n", iteration));

		/* repost unless this is the last iteration */
		repost_recv = (iteration + 1 != test_ptr->cmd->num_iterations);

		for (op = 0; op < test_ptr->cmd->num_ops; op++) {
			ours = (test_ptr->is_server ==
				test_ptr->ep_context[0].op[op].
				server_initiated);
			bytes =
			    (test_ptr->ep_context[0].op[op].seg_size *
			     test_ptr->ep_context[0].op[op].num_segs *
			     test_ptr->cmd->eps_per_thread);

			switch (test_ptr->ep_context[0].op[op].transfer_type) {
			case RDMA_READ:
				{
					test_ptr->stats.stat_bytes_rdma_read +=
					    bytes;
					if (ours) {
						DT_Tdep_PT_Debug(1,
								 (phead,
								  "Test[" F64x
								  "]: RdmaRead [%d]\n",
								  test_ptr->
								  base_port,
								  op));
						if (!DT_handle_rdma_op
						    (phead,
						     test_ptr->ep_context,
						     test_ptr->cmd->
						     eps_per_thread, RDMA_READ,
						     op, test_ptr->cmd->poll)) {
							DT_Tdep_PT_Printf(phead,
									  "Test["
									  F64x
									  "]: RdmaRead error[%d]\n",
									  test_ptr->
									  base_port,
									  op);
							goto bail;
						}
					}
					break;
				}

			case RDMA_WRITE:
				{
					test_ptr->stats.stat_bytes_rdma_write +=
					    bytes;
					if (ours) {
						DT_Tdep_PT_Debug(1,
								 (phead,
								  "Test[" F64x
								  "]: RdmaWrite [%d]\n",
								  test_ptr->
								  base_port,
								  op));
						if (!DT_handle_rdma_op
						    (phead,
						     test_ptr->ep_context,
						     test_ptr->cmd->
						     eps_per_thread, RDMA_WRITE,
						     op, test_ptr->cmd->poll)) {
							DT_Tdep_PT_Printf(phead,
									  "Test["
									  F64x
									  "]: RdmaWrite error[%d]\n",
									  test_ptr->
									  base_port,
									  op);
							goto bail;
						}
					}
					break;
				}

			case SEND_RECV:
				{
					if (ours) {
						test_ptr->stats.
						    stat_bytes_send += bytes;
						DT_Tdep_PT_Debug(1,
								 (phead,
								  "Test[" F64x
								  "]: postsend [%d] \n",
								  test_ptr->
								  base_port,
								  op));
						/* send data */
						if (!DT_handle_send_op(phead,
								       test_ptr->
								       ep_context,
								       test_ptr->
								       cmd->
								       eps_per_thread,
								       op,
								       test_ptr->
								       cmd->
								       poll)) {
							goto bail;
						}
					} else {
						test_ptr->stats.
						    stat_bytes_recv += bytes;
						DT_Tdep_PT_Debug(1,
								 (phead,
								  "Test[" F64x
								  "]: RecvWait and Re-Post [%d] \n",
								  test_ptr->
								  base_port,
								  op));

						if (!DT_handle_recv_op(phead,
								       test_ptr->
								       ep_context,
								       test_ptr->
								       cmd->
								       eps_per_thread,
								       op,
								       test_ptr->
								       cmd->
								       poll,
								       repost_recv))
						{
							goto bail;
						}
					}

					/* now before going on, is it time to validate? */
					if (test_ptr->cmd->validate) {
						if (!test_ptr->pt_ptr->local_is_server) {	/* CLIENT */
							/* the client validates on the third to last op */
							if (op ==
							    test_ptr->cmd->
							    num_ops - 3) {
								if (!DT_Transaction_Validation_Check(phead, test_ptr, iteration)) {
									goto bail;
								}
								DT_Transaction_Validation_Fill
								    (phead,
								     test_ptr,
								     iteration +
								     1);
							}
						} else {	/* SERVER */

							/* the server validates on the second to last op */
							if (op ==
							    test_ptr->cmd->
							    num_ops - 2) {
								if (!DT_Transaction_Validation_Check(phead, test_ptr, iteration)) {
									goto bail;
								}
								DT_Transaction_Validation_Fill
								    (phead,
								     test_ptr,
								     iteration +
								     1);
							}
						}
					}	/* end validate */
					break;
				}
			}	/* end switch for transfer type */
		}		/* end loop for each op */
	}			/* end loop for iteration */

	/* end time and print stats */
	test_ptr->stats.end_time = DT_Mdep_GetTime();
	if (!test_ptr->pt_ptr->local_is_server) {
		DT_update_transaction_stats(&test_ptr->pt_ptr->Client_Stats,
					    test_ptr->cmd->eps_per_thread *
					    test_ptr->cmd->num_ops *
					    test_ptr->cmd->num_iterations,
					    test_ptr->stats.end_time -
					    test_ptr->stats.start_time,
					    test_ptr->stats.stat_bytes_send,
					    test_ptr->stats.stat_bytes_recv,
					    test_ptr->stats.
					    stat_bytes_rdma_read,
					    test_ptr->stats.
					    stat_bytes_rdma_write);
	}
	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: End Successfully\n",
			  test_ptr->base_port));
	success = true;

      bail:
	return (success);
}

/*------------------------------------------------------------------------------ */
void
DT_Transaction_Validation_Fill(DT_Tdep_Print_Head * phead,
			       Transaction_Test_t * test_ptr,
			       unsigned int iteration)
{
	bool ours;
	unsigned int op;
	unsigned int i;
	unsigned int j;
	unsigned int ind;
	unsigned char *buff;

	if (iteration >= test_ptr->cmd->num_iterations) {
		return;
	}
	DT_Tdep_PT_Debug(1,
			 (phead, "Test[" F64x "]: FILL Buffers Iteration %d\n",
			  test_ptr->base_port, iteration));

	/*
	 * fill all but the last three ops, which
	 * were added to create barriers for data validation
	 */
	for (ind = 0; ind < test_ptr->cmd->eps_per_thread; ind++) {
		for (op = 0; op < test_ptr->cmd->num_ops - 3; op++) {
			ours = (test_ptr->is_server ==
				test_ptr->ep_context[ind].op[op].
				server_initiated);

			switch (test_ptr->ep_context[ind].op[op].transfer_type)
			{
			case RDMA_READ:
				{
					if (!ours) {
						for (i = 0;
						     i <
						     test_ptr->ep_context[ind].
						     op[op].num_segs; i++) {

							buff =
							    DT_Bpool_GetBuffer
							    (test_ptr->
							     ep_context[ind].
							     op[op].bp, i);
							for (j = 0;
							     j <
							     test_ptr->
							     ep_context[ind].
							     op[op].seg_size;
							     j++) {
								/* Avoid using all zero bits the 1st time */
								buff[j] =
								    (iteration +
								     1) & 0xFF;
							}
						}
					}
					break;
				}

			case RDMA_WRITE:
				{
					if (ours) {
						for (i = 0;
						     i <
						     test_ptr->ep_context[ind].
						     op[op].num_segs; i++) {

							buff =
							    DT_Bpool_GetBuffer
							    (test_ptr->
							     ep_context[ind].
							     op[op].bp, i);
							for (j = 0;
							     j <
							     test_ptr->
							     ep_context[ind].
							     op[op].seg_size;
							     j++) {
								/* Avoid using all zero bits the 1st time */
								buff[j] =
								    (iteration +
								     1) & 0xFF;
							}
						}
					}
					break;
				}

			case SEND_RECV:
				{
					if (ours) {
						for (i = 0;
						     i <
						     test_ptr->ep_context[ind].
						     op[op].num_segs; i++) {

							buff =
							    DT_Bpool_GetBuffer
							    (test_ptr->
							     ep_context[ind].
							     op[op].bp, i);
			    /*****
			       DT_Tdep_PT_Printf(phead, 
			       "\tFill: wq=%d op=%d seg=%d ptr=[%p, %d]\n",
			       ind, op, i, buff, j);
			     *****/
							for (j = 0;
							     j <
							     test_ptr->
							     ep_context[ind].
							     op[op].seg_size;
							     j++) {
								/* Avoid using all zero bits the 1st time */
								buff[j] =
								    (iteration +
								     1) & 0xFF;
							}
						}
					}
					break;
				}
			}	/* end switch transfer_type */
		}		/* end for each op */
	}			/* end for each ep per thread */
}

/*------------------------------------------------------------------------------ */
bool
DT_Transaction_Validation_Check(DT_Tdep_Print_Head * phead,
				Transaction_Test_t * test_ptr, int iteration)
{
	bool ours;
	bool success = true;
	unsigned int op;
	unsigned int i;
	unsigned int j;
	unsigned int ind;
	unsigned char *buff;
	unsigned char expect;
	unsigned char got;

	DT_Tdep_PT_Debug(1,
			 (phead,
			  "Test[" F64x "]: VALIDATE Buffers Iteration %d\n",
			  test_ptr->base_port, iteration));

	/*
	 * fill all but the last three ops, which
	 * were added to create barriers for data validation
	 */
	for (ind = 0; ind < test_ptr->cmd->eps_per_thread; ind++) {
		for (op = 0; op < test_ptr->cmd->num_ops - 3; op++) {
			ours = (test_ptr->is_server ==
				test_ptr->ep_context[ind].op[op].
				server_initiated);

			switch (test_ptr->ep_context[ind].op[op].transfer_type) {
			case RDMA_READ:
				{
					if (ours) {
						for (i = 0;
						     i <
						     test_ptr->ep_context[ind].
						     op[op].num_segs; i++) {

							buff =
							    DT_Bpool_GetBuffer
							    (test_ptr->
							     ep_context[ind].
							     op[op].bp, i);

							for (j = 0;
							     j <
							     test_ptr->
							     ep_context[ind].
							     op[op].seg_size;
							     j++) {

								expect =
								    (iteration +
								     1) & 0xFF;
								got = buff[j];
								if (expect !=
								    got) {
									DT_Tdep_PT_Printf
									    (phead,
									     "Test["
									     F64x
									     "]: Validation Error :: %d\n",
									     test_ptr->
									     base_port,
									     op);
									DT_Tdep_PT_Printf
									    (phead,
									     "Test["
									     F64x
									     "]: Expected %x Got %x\n",
									     test_ptr->
									     base_port,
									     expect,
									     got);
									DT_Tdep_PT_Debug
									    (3,
									     (phead,
									      "\twq=%d op=%d seg=%d byte=%d ptr=%p\n",
									      ind,
									      op,
									      i,
									      j,
									      buff));
									success
									    =
									    false;
									break;
								}
							}
						}
					}
					break;
				}

			case RDMA_WRITE:
				{
					if (!ours) {
						for (i = 0;
						     i <
						     test_ptr->ep_context[ind].
						     op[op].num_segs; i++) {

							buff =
							    DT_Bpool_GetBuffer
							    (test_ptr->
							     ep_context[ind].
							     op[op].bp, i);
							for (j = 0;
							     j <
							     test_ptr->
							     ep_context[ind].
							     op[op].seg_size;
							     j++) {

								expect =
								    (iteration +
								     1) & 0xFF;
								got = buff[j];
								if (expect !=
								    got) {
									DT_Tdep_PT_Printf
									    (phead,
									     "Test["
									     F64x
									     "]: Validation Error :: %d\n",
									     test_ptr->
									     base_port,
									     op);
									DT_Tdep_PT_Printf
									    (phead,
									     "Test["
									     F64x
									     "]: Expected %x Got %x\n",
									     test_ptr->
									     base_port,
									     expect,
									     got);
									DT_Tdep_PT_Debug
									    (3,
									     (phead,
									      "\twq=%d op=%d seg=%d byte=%d ptr=%p\n",
									      ind,
									      op,
									      i,
									      j,
									      buff));
									success
									    =
									    false;
									break;
								}
							}
						}
					}
					break;
				}

			case SEND_RECV:
				{
					if (!ours) {
						for (i = 0;
						     i <
						     test_ptr->ep_context[ind].
						     op[op].num_segs; i++) {

							buff =
							    DT_Bpool_GetBuffer
							    (test_ptr->
							     ep_context[ind].
							     op[op].bp, i);
							DT_Tdep_PT_Debug(3,
									 (phead,
									  "\tCheck:wq=%d op=%d seg=%d ptr=[%p, %d]\n",
									  ind,
									  op, i,
									  buff,
									  test_ptr->
									  ep_context
									  [ind].
									  op
									  [op].
									  seg_size));

							for (j = 0;
							     j <
							     test_ptr->
							     ep_context[ind].
							     op[op].seg_size;
							     j++) {

								expect =
								    (iteration +
								     1) & 0xFF;
								got = buff[j];
								if (expect !=
								    got) {
									DT_Tdep_PT_Printf
									    (phead,
									     "Test["
									     F64x
									     "]: Validation Error :: %d\n",
									     test_ptr->
									     base_port,
									     op);
									DT_Tdep_PT_Printf
									    (phead,
									     "Test["
									     F64x
									     "]: Expected %x Got %x\n",
									     test_ptr->
									     base_port,
									     expect,
									     got);
									DT_Tdep_PT_Debug
									    (3,
									     (phead,
									      "\twq=%d op=%d seg=%d byte=%d ptr=%p\n",
									      ind,
									      op,
									      i,
									      j,
									      buff));
									success
									    =
									    false;
									break;
								}
							}
						}
					}
					break;
				}
			}	/* end switch transfer_type */
		}		/* end for each op */
	}			/* end for each ep per thread */

	return (success);
}

/*------------------------------------------------------------------------------ */
void
DT_Print_Transaction_Test(DT_Tdep_Print_Head * phead,
			  Transaction_Test_t * test_ptr)
{
	DT_Tdep_PT_Printf(phead, "-------------------------------------\n");
	DT_Tdep_PT_Printf(phead, "TransTest.is_server              : %d\n",
			  test_ptr->is_server);
	DT_Tdep_PT_Printf(phead, "TransTest.remote_little_endian   : %d\n",
			  test_ptr->remote_is_little_endian);
	DT_Tdep_PT_Printf(phead,
			  "TransTest.base_port              : " F64x "\n",
			  test_ptr->base_port);
	DT_Tdep_PT_Printf(phead, "TransTest.pz_handle              : %p\n",
			  test_ptr->pz_handle);
	/* statistics */
	DT_Tdep_PT_Printf(phead, "TransTest.bytes_send             : %d\n",
			  test_ptr->stats.stat_bytes_send);
	DT_Tdep_PT_Printf(phead, "TransTest.bytes_recv             : %d\n",
			  test_ptr->stats.stat_bytes_recv);
	DT_Tdep_PT_Printf(phead, "TransTest.bytes_rdma_read        : %d\n",
			  test_ptr->stats.stat_bytes_rdma_read);
	DT_Tdep_PT_Printf(phead, "TransTest.bytes_rdma_write       : %d\n",
			  test_ptr->stats.stat_bytes_rdma_write);
}

/*------------------------------------------------------------------------------ */
void
DT_Print_Transaction_Stats(DT_Tdep_Print_Head * phead,
			   Transaction_Test_t * test_ptr)
{
	double time;
	double mbytes_send;
	double mbytes_recv;
	double mbytes_rdma_read;
	double mbytes_rdma_write;
	int total_ops;
	time =
	    (double)(test_ptr->stats.end_time -
		     test_ptr->stats.start_time) / 1000;
	mbytes_send = (double)test_ptr->stats.stat_bytes_send / 1024 / 1024;
	mbytes_recv = (double)test_ptr->stats.stat_bytes_recv / 1024 / 1024;
	mbytes_rdma_read =
	    (double)test_ptr->stats.stat_bytes_rdma_read / 1024 / 1024;
	mbytes_rdma_write =
	    (double)test_ptr->stats.stat_bytes_rdma_write / 1024 / 1024;
	total_ops = test_ptr->cmd->num_ops * test_ptr->cmd->num_iterations;

	DT_Tdep_PT_Printf(phead, "Test[: " F64x "] ---- Stats ----\n",
			  test_ptr->base_port);
	DT_Tdep_PT_Printf(phead, "Iterations : %u\n",
			  test_ptr->cmd->num_iterations);
	DT_Tdep_PT_Printf(phead, "Ops     : %7d.%02d Ops/Sec\n",
			  whole(total_ops / time),
			  hundredths(total_ops / time));
	DT_Tdep_PT_Printf(phead, "Time       : %7d.%02d sec\n", whole(time),
			  hundredths(time));
	DT_Tdep_PT_Printf(phead, "Sent       : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_send), hundredths(mbytes_send),
			  whole(mbytes_send / time),
			  hundredths(mbytes_send / time));
	DT_Tdep_PT_Printf(phead, "Recv       : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_recv), hundredths(mbytes_recv),
			  whole(mbytes_recv / time),
			  hundredths(mbytes_recv / time));
	DT_Tdep_PT_Printf(phead, "RDMA Read  : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_rdma_read), hundredths(mbytes_rdma_read),
			  whole(mbytes_rdma_read / time),
			  hundredths(mbytes_rdma_read / time));
	DT_Tdep_PT_Printf(phead, "RDMA Write : %7d.%02d MB - %7d.%02d MB/Sec\n",
			  whole(mbytes_rdma_write),
			  hundredths(mbytes_rdma_write),
			  whole(mbytes_rdma_write / time),
			  hundredths(mbytes_rdma_write / time));
}
