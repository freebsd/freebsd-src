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

#define DEFAULT_QUEUE_LEN 10

/* function that is called when an assertion fails, printing out the line
 * that failed vi DT_Tdep_PT_Printf
 */
void DT_assert_fail(DT_Tdep_Print_Head * phead, char *exp, char *file,
		    char *baseFile, int line)
{
	if (!strcmp(file, baseFile)) {
		DT_Tdep_PT_Printf(phead,
				  "%s failed in file %s, line %d\n",
				  exp, file, line);
	} else {
		DT_Tdep_PT_Printf(phead,
				  "%s failed in file %s (included from %s), line %d\n",
				  exp, file, baseFile, line);
	}
}

/* helper function to open an IA */
int DT_ia_open(DAT_NAME_PTR dev_name, DAT_IA_HANDLE * ia_handle)
{
	DAT_EVD_HANDLE evd_handle;
	evd_handle = DAT_HANDLE_NULL;
	return dat_ia_open(dev_name, DEFAULT_QUEUE_LEN, &evd_handle, ia_handle);
}

/* helper function to create an endpoint and its associated EVDs */
int DT_ep_create(Params_t * params_ptr,
		 DAT_IA_HANDLE ia_handle,
		 DAT_PZ_HANDLE pz_handle,
		 DAT_EVD_HANDLE * cr_evd,
		 DAT_EVD_HANDLE * conn_evd,
		 DAT_EVD_HANDLE * send_evd,
		 DAT_EVD_HANDLE * recv_evd, DAT_EP_HANDLE * ep_handle)
{
	DAT_RETURN status;
	DT_Tdep_Print_Head *phead;
	*conn_evd = 0;
	*send_evd = 0;
	*recv_evd = 0;
	*cr_evd = 0;
	phead = params_ptr->phead;

	status =
	    DT_Tdep_evd_create(ia_handle, DEFAULT_QUEUE_LEN, DAT_HANDLE_NULL,
			       DAT_EVD_CR_FLAG, cr_evd);
	if (status != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "dat_evd_create failed %s\n",
				  DT_RetToString(status));
		return status;
	}

	status =
	    DT_Tdep_evd_create(ia_handle, DEFAULT_QUEUE_LEN, DAT_HANDLE_NULL,
			       DAT_EVD_CONNECTION_FLAG, conn_evd);
	if (status != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "dat_evd_create failed %s\n",
				  DT_RetToString(status));
		return status;
	}

	status =
	    DT_Tdep_evd_create(ia_handle, DEFAULT_QUEUE_LEN, DAT_HANDLE_NULL,
			       DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG,
			       send_evd);
	if (status != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "dat_evd_create failed %s\n",
				  DT_RetToString(status));
		return status;
	}

	status =
	    DT_Tdep_evd_create(ia_handle, DEFAULT_QUEUE_LEN, DAT_HANDLE_NULL,
			       DAT_EVD_DTO_FLAG, recv_evd);
	if (status != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "dat_evd_create failed %s\n",
				  DT_RetToString(status));
		return status;
	}

	status = dat_ep_create(ia_handle, pz_handle, *recv_evd,
			       *send_evd, *conn_evd, NULL, ep_handle);
	if (status != DAT_SUCCESS) {
		DT_Tdep_PT_Printf(phead, "dat_ep_create failed %s\n",
				  DT_RetToString(status));
	}
	return status;
}

/* function that initializes the connection struct */
void DT_fft_init_conn_struct(FFT_Connection_t * conn)
{
	conn->ia_handle = 0;
	conn->pz_handle = 0;
	conn->psp_handle = 0;
	conn->ep_handle = 0;
	conn->cr_evd = 0;
	conn->send_evd = 0;
	conn->conn_evd = 0;
	conn->recv_evd = 0;
	conn->cr_handle = 0;
	conn->remote_netaddr = 0;
	conn->bpool = 0;
	conn->pt_ptr = 0;
	conn->connected = false;
}

/* helper function that simplifies many dat calls for the initiialization of a
 * dat "client"
 */
void DT_fft_init_client(Params_t * params_ptr, FFT_Cmd_t * cmd,
			FFT_Connection_t * conn)
{
	int res;
	DAT_RETURN rc = 0;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	/* initialize the struct's members */
	DT_fft_init_conn_struct(conn);

	/* open the IA */
	rc = DT_ia_open(cmd->device_name, &conn->ia_handle);
	if (rc != DAT_SUCCESS) {
		/* make sure the handle has an invalid value */
		conn->ia_handle = NULL;
	}
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* create a PZ */
	rc = dat_pz_create(conn->ia_handle, &conn->pz_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* create an EP and its EVDs */
	rc = DT_ep_create(params_ptr,
			  conn->ia_handle,
			  conn->pz_handle,
			  &conn->cr_evd,
			  &conn->conn_evd,
			  &conn->send_evd, &conn->recv_evd, &conn->ep_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* if a server name is given, allocate memory for a net address and set it
	 * up appropriately
	 */
	if (cmd->server_name && strlen(cmd->server_name)) {
		conn->remote_netaddr = &params_ptr->server_netaddr;
	}
      cleanup:
	return;
}

/* helper function to break down a client or server created with one of the
 * init helper functions
 */
int DT_fft_destroy_conn_struct(Params_t * params_ptr, FFT_Connection_t * conn)
{
	DAT_RETURN rc = DAT_SUCCESS;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	if (conn->ep_handle) {
		if (conn->connected) {
			rc = dat_ep_disconnect(conn->ep_handle,
					       DAT_CLOSE_DEFAULT);
			DT_assert_clean(phead, rc == DAT_SUCCESS);

			if (!DT_disco_event_wait(phead, conn->cr_evd, NULL))
			{
				DT_Tdep_PT_Printf(phead,
						  "DT_fft_destroy_conn_struct: bad disconnect event\n");
			}
		}
		rc = dat_ep_free(conn->ep_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (conn->bpool) {
		DT_Bpool_Destroy(0, phead, conn->bpool);
	}
	if (conn->psp_handle) {
		rc = dat_psp_free(conn->psp_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (conn->cr_evd) {
		rc = DT_Tdep_evd_free(conn->cr_evd);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (conn->conn_evd) {
		rc = DT_Tdep_evd_free(conn->conn_evd);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (conn->send_evd) {
		rc = DT_Tdep_evd_free(conn->send_evd);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (conn->recv_evd) {
		rc = DT_Tdep_evd_free(conn->recv_evd);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (conn->pt_ptr) {
		DT_Free_Per_Test_Data(conn->pt_ptr);
	}
	if (conn->pz_handle) {
		rc = dat_pz_free(conn->pz_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (conn->ia_handle) {
		rc = dat_ia_close(conn->ia_handle, DAT_CLOSE_ABRUPT_FLAG);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	return rc;
}

/* helper function to init a dat "server" */
void DT_fft_init_server(Params_t * params_ptr, FFT_Cmd_t * cmd,
			FFT_Connection_t * conn)
{
	int res;
	DAT_RETURN rc = 0;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	/* init the connection struct's members */
	DT_fft_init_conn_struct(conn);

	/* open the IA */
	rc = DT_ia_open(cmd->device_name, &conn->ia_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* create a PZ */
	rc = dat_pz_create(conn->ia_handle, &conn->pz_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* create an EP and its EVDs */
	rc = DT_ep_create(params_ptr,
			  conn->ia_handle,
			  conn->pz_handle,
			  &conn->cr_evd,
			  &conn->conn_evd,
			  &conn->send_evd, &conn->recv_evd, &conn->ep_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* create a PSP */
	rc = dat_psp_create(conn->ia_handle, SERVER_PORT_NUMBER, conn->cr_evd,
			    DAT_PSP_CONSUMER_FLAG, &conn->psp_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* allocate memory for buffers */
	conn->bpool =
	    DT_BpoolAlloc(0, phead, conn->ia_handle, conn->pz_handle, NULL,
			  NULL, 8192, 2, DAT_OPTIMAL_ALIGNMENT, false, false);
	DT_assert(phead, conn->bpool);
      cleanup:
	return;
}

/* helper function that allows a server to listen for a connection */
void DT_fft_listen(Params_t * params_ptr, FFT_Connection_t * conn)
{
	int res;
	DAT_RETURN rc = 0;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	/* wait on a CR event via the CR EVD */
	DT_assert_dat(phead,
		      DT_cr_event_wait(phead, conn->cr_evd, &conn->cr_stat)
		      && DT_cr_check(phead, &conn->cr_stat, conn->psp_handle,
				     SERVER_PORT_NUMBER, &conn->cr_handle,
				     "DT_fft_listen"));

	/* accept the connection */
	rc = dat_cr_accept(conn->cr_handle, conn->ep_handle, 0, (DAT_PVOID) 0);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* wait on a conn event via the conn EVD */
	DT_assert(phead,
		  DT_conn_event_wait(phead,
				     conn->ep_handle,
				     conn->conn_evd, &conn->event_num) == true);
	conn->connected = true;
      cleanup:
	return;
}

/* helper function that allows a client to connect to a server */
int DT_fft_connect(Params_t * params_ptr, FFT_Connection_t * conn)
{
	int wait_count;
	int res;
	DAT_RETURN rc = 0;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	/* try 10 times to connect */
	for (wait_count = 0; wait_count < 10; wait_count++) {
		DT_Tdep_PT_Printf(phead, "Connection to server, attempt #%d\n",
				  wait_count + 1);

		/* attempt to connect, timeout = 10 secs */
		rc = dat_ep_connect(conn->ep_handle, conn->remote_netaddr,
				    SERVER_PORT_NUMBER, 10 * 1000000, 0,
				    (DAT_PVOID) 0, DAT_QOS_BEST_EFFORT,
				    DAT_CONNECT_DEFAULT_FLAG);
		DT_assert_dat(phead, rc == DAT_SUCCESS);

		/* wait on conn event */
		DT_assert(phead,
			  DT_conn_event_wait(phead,
					     conn->ep_handle,
					     conn->conn_evd,
					     &conn->event_num) == true);

		/* make sure we weren't rejected by the peer */
		if (conn->event_num == DAT_CONNECTION_EVENT_PEER_REJECTED) {
			DT_Mdep_Sleep(1000);
			DT_Tdep_PT_Printf(phead,
					  "Connection rejected by peer; retrying\n");
		}
	}
      cleanup:
	if (conn->event_num == DAT_CONNECTION_EVENT_ESTABLISHED) {
		conn->connected = true;
	}
	/* returns true if connected, false otherwise */
	return (conn->connected);
}
