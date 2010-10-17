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

#define CQENTRYCOUNT 100
#define BUFFSIZE 1024
#define DEFAULT_QUEUE_LEN 10

int DT_endpoint_generic(Params_t * params_ptr,
			FFT_Cmd_t * cmd, bool destroy_pz_early)
{
	char *dev_name;
	DAT_IA_HANDLE ia_handle;
	DAT_PZ_HANDLE pz_handle;
	DAT_EP_HANDLE ep_handle;
	DAT_EVD_HANDLE evd_handle;
	DAT_EVD_HANDLE conn_evd_handle;
	DAT_EVD_HANDLE send_evd_handle;
	DAT_EVD_HANDLE recv_evd_handle;
	DAT_RETURN rc, wanted;
	int res;
	DT_Tdep_Print_Head *phead;

	res = 1;
	ia_handle = NULL;
	pz_handle = NULL;
	ep_handle = NULL;
	evd_handle = NULL;
	conn_evd_handle = NULL;
	send_evd_handle = NULL;
	recv_evd_handle = NULL;
	dev_name = cmd->device_name;
	evd_handle = DAT_HANDLE_NULL;
	phead = params_ptr->phead;

	rc = dat_ia_open((const DAT_NAME_PTR)dev_name,
			 DEFAULT_QUEUE_LEN, &evd_handle, &ia_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	rc = dat_pz_create(ia_handle, &pz_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	if (destroy_pz_early) {
		if (pz_handle) {
			rc = dat_pz_free(pz_handle);
			DT_assert_dat(phead, rc == DAT_SUCCESS);
		}
	}

	rc = DT_Tdep_evd_create(ia_handle, DEFAULT_QUEUE_LEN, NULL,
				DAT_EVD_DTO_FLAG | DAT_EVD_RMR_BIND_FLAG,
				&send_evd_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	rc = DT_Tdep_evd_create(ia_handle, DEFAULT_QUEUE_LEN, NULL,
				DAT_EVD_DTO_FLAG, &recv_evd_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	rc = DT_Tdep_evd_create(ia_handle, DEFAULT_QUEUE_LEN, NULL,
				DAT_EVD_CONNECTION_FLAG, &conn_evd_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	rc = dat_ep_create(ia_handle, pz_handle, recv_evd_handle,
			   send_evd_handle, conn_evd_handle, NULL, &ep_handle);
	if (destroy_pz_early) {
		wanted = DAT_INVALID_HANDLE;
	} else {
		wanted = DAT_SUCCESS;
	}
	DT_assert_dat(phead, DAT_GET_TYPE(rc) == wanted);

      cleanup:
	if (ep_handle) {
		rc = dat_ep_free(ep_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

	if (send_evd_handle) {
		rc = DT_Tdep_evd_free(send_evd_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

	if (recv_evd_handle) {
		rc = DT_Tdep_evd_free(recv_evd_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

	if (conn_evd_handle) {
		rc = DT_Tdep_evd_free(conn_evd_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

	if (!destroy_pz_early && pz_handle) {
		rc = dat_pz_free(pz_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

	if (ia_handle) {
		rc = dat_ia_close(ia_handle, DAT_CLOSE_ABRUPT_FLAG);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	return res;
}

int DT_endpoint_case0(Params_t * params_ptr, FFT_Cmd_t * cmd)
{

	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: Test if we can normally create endpoint and destory it.\n");
	DT_Tdep_PT_Printf(phead, "\
	The endpoint is not associated with a CQ\n");
	return DT_endpoint_generic(params_ptr, cmd, false);	/* destroy pz early */
}

int DT_endpoint_case1(Params_t * params_ptr, FFT_Cmd_t * cmd)
{

	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: try to create endpoint with pz already destroyed\n");
	return DT_endpoint_generic(params_ptr, cmd, true);	/* destroy pz early */
}

int DT_endpoint_case2(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	char *dev_name;
	DAT_IA_HANDLE ia_handle;
	DAT_EP_HANDLE ep_handle;
	DAT_EVD_HANDLE send_evd, conn_evd, recv_evd, cr_evd;
	DAT_PZ_HANDLE pz_handle;
	DAT_EVENT event;
	Bpool *bpool;
	int res;
	DAT_RETURN rc;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: try to destroy ep with descriptor still in working queue\n");
	res = 1;
	bpool = 0;
	pz_handle = 0;
	ia_handle = 0;
	ep_handle = 0;
	send_evd = 0;
	conn_evd = 0;
	recv_evd = 0;
	cr_evd = 0;
	dev_name = cmd->device_name;

	rc = DT_ia_open(dev_name, &ia_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);
	rc = dat_pz_create(ia_handle, &pz_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);
	rc = DT_ep_create(params_ptr,
			  ia_handle,
			  pz_handle,
			  &cr_evd, &conn_evd, &send_evd, &recv_evd, &ep_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);
	bpool =
	    DT_BpoolAlloc(0, phead, ia_handle, pz_handle, NULL, NULL, 4096, 1,
			  DAT_OPTIMAL_ALIGNMENT, false, false);
	DT_assert(phead, bpool != 0);
	DT_assert(phead, DT_post_recv_buffer(phead,
					     ep_handle,
					     bpool, 0, 4096) == true);
	if (ep_handle) {
		rc = dat_ep_free(ep_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
	}

	/*
	 * Remove all DTOs. The disconnect above may have
	 * flushed all posted operations, so this is just a
	 * clean up.
	 */
	do {
		rc = DT_Tdep_evd_dequeue(recv_evd, &event);
	} while (rc == DAT_SUCCESS);
      cleanup:
	if (bpool) {
		rc = DT_Bpool_Destroy(0, phead, bpool);
		DT_assert_clean(phead, rc != false);
	}
	if (pz_handle) {
		rc = dat_pz_free(pz_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (ia_handle) {
		rc = dat_ia_close(ia_handle, DAT_CLOSE_ABRUPT_FLAG);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	return res;

}

/*-------------------------------------------------------------*/
void DT_endpoint_test(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	int i;
	int res;
	DT_Tdep_Print_Head *phead;
	FFT_Testfunc_t cases_func[] = {
		{DT_endpoint_case0},
		{DT_endpoint_case1},
		{DT_endpoint_case2},
	};

	phead = params_ptr->phead;
	for (i = 0; i < cmd->size; i++) {
		if (cmd->cases_flag[i]) {
			DT_Tdep_PT_Printf(phead, "\
		*********************************************************************\n");
			DT_Tdep_PT_Printf(phead, "\
		Function feature: EndPoint management           case: %d\n", i);
			res = cases_func[i].fun(params_ptr, cmd);
			if (res == 1) {
				DT_Tdep_PT_Printf(phead, "Result: PASS\n");
			} else if (res == 0) {
				DT_Tdep_PT_Printf(phead, "Result: FAIL\n");
			} else if (res == -1) {
				DT_Tdep_PT_Printf(phead,
						  "Result: use other test tool\n");
			} else if (res == -2) {
				DT_Tdep_PT_Printf(phead,
						  "Result: not support or next stage to develop\n");
			}

			DT_Tdep_PT_Printf(phead, "\
		*********************************************************************\n");
		}
	}
	return;
}
