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

#if defined(WIN32)
static DAT_OS_WAIT_PROXY_AGENT NULLPROXY = {
	(DAT_PVOID) NULL, (DAT_AGENT_FUNC) NULL
};
#endif

int DT_queryinfo_basic(Params_t * params_ptr,
		       FFT_Cmd_t * cmd,
		       FFT_query_enum object_to_query, DAT_RETURN result_wanted)
{
	char *dev_name;
	DAT_IA_HANDLE ia_handle;
	DAT_IA_ATTR ia_attributes;
	DAT_PROVIDER_ATTR provider_attributes;
	DAT_EVD_HANDLE evd_handle;
	DAT_EVD_HANDLE conn_evd_handle;
	DAT_EVD_HANDLE cr_evd_handle;
	DAT_EVD_HANDLE send_evd_handle;
	DAT_EVD_HANDLE recv_evd_handle;
	DAT_EP_HANDLE ep_handle;
	DAT_EP_PARAM ep_param;
	DAT_CNO_HANDLE cno_handle;
#ifndef __KDAPLTEST__
	DAT_CNO_PARAM cno_param;
#endif
	DAT_EVD_PARAM evd_param;
	DAT_PSP_HANDLE psp_handle;
	DAT_PSP_PARAM psp_param;
	DAT_RSP_HANDLE rsp_handle;
	DAT_RSP_PARAM rsp_param;
	DAT_PZ_HANDLE pz_handle;
	DAT_PZ_PARAM pz_param;
	DAT_LMR_HANDLE lmr_handle;
	DAT_LMR_PARAM lmr_param;
	DAT_LMR_CONTEXT lmr_context;
	DAT_RMR_HANDLE rmr_handle;
	DAT_RMR_PARAM rmr_param;
	DAT_REGION_DESCRIPTION region;
	DAT_VLEN reg_size;
	DAT_VADDR reg_addr;
	DAT_VLEN buffer_size;
	unsigned char *alloc_ptr;
	DT_Tdep_Print_Head *phead;

	DAT_RETURN rc;
	int res = 1;
	buffer_size = BUFFSIZE * sizeof(unsigned char);
	phead = params_ptr->phead;
	reg_addr = 0;
	alloc_ptr = 0;

	ia_handle = NULL;
	pz_handle = NULL;
	ep_handle = NULL;
	lmr_handle = NULL;
	rmr_handle = NULL;
	pz_handle = NULL;
	psp_handle = NULL;
	rsp_handle = NULL;
	cno_handle = NULL;
	evd_handle = DAT_HANDLE_NULL;
	conn_evd_handle = DAT_HANDLE_NULL;
	cr_evd_handle = DAT_HANDLE_NULL;
	recv_evd_handle = DAT_HANDLE_NULL;
	send_evd_handle = DAT_HANDLE_NULL;
	dev_name = cmd->device_name;

	/* All functions require an ia_handle to be created */
	rc = dat_ia_open((const DAT_NAME_PTR)dev_name,
			 DEFAULT_QUEUE_LEN, &evd_handle, &ia_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	/* These functions require a pz_handle to be created */
	if ((object_to_query == QUERY_EVD) ||
	    (object_to_query == QUERY_RMR) ||
	    (object_to_query == QUERY_LMR) ||
	    (object_to_query == QUERY_EP) ||
	    (object_to_query == QUERY_RSP) || (object_to_query == QUERY_PZ)) {
		rc = dat_pz_create(ia_handle, &pz_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
	}

	/* These functions require a ep_handle to be created */
	if ((object_to_query == QUERY_EP) || (object_to_query == QUERY_RSP)) {
		rc = DT_Tdep_evd_create(ia_handle,
					DEFAULT_QUEUE_LEN,
					cno_handle,
					DAT_EVD_DTO_FLAG |
					DAT_EVD_RMR_BIND_FLAG,
					&send_evd_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);

		rc = DT_Tdep_evd_create(ia_handle,
					DEFAULT_QUEUE_LEN,
					cno_handle,
					DAT_EVD_DTO_FLAG, &recv_evd_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);

		rc = DT_Tdep_evd_create(ia_handle,
					DEFAULT_QUEUE_LEN,
					cno_handle,
					DAT_EVD_CONNECTION_FLAG,
					&conn_evd_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);

		rc = dat_ep_create(ia_handle,
				   pz_handle,
				   recv_evd_handle,
				   send_evd_handle,
				   conn_evd_handle, NULL, &ep_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
	}

	/* These functions require a CR EVD to be created.  */
	if ((object_to_query == QUERY_PSP) || (object_to_query == QUERY_RSP)) {
		rc = DT_Tdep_evd_create(ia_handle,
					DEFAULT_QUEUE_LEN,
					cno_handle,
					DAT_EVD_CR_FLAG, &cr_evd_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
	}

	/* Test dat_ia_query function */
	if (object_to_query == QUERY_IA) {
		if (result_wanted == DAT_SUCCESS) {
			rc = dat_ia_query(ia_handle,
					  &evd_handle,
					  DAT_IA_ALL,
					  &ia_attributes,
					  DAT_PROVIDER_FIELD_ALL,
					  &provider_attributes);
		} else if (result_wanted == DAT_INVALID_PARAMETER) {
			/*
			 * The only way to get an invalid parameter is to
			 * NULL out ia_attr and for the DAT_IA_ATTR_MASK to
			 * have values
			 */
			rc = dat_ia_query(ia_handle,
					  &evd_handle,
					  DAT_IA_ALL,
					  NULL,
					  DAT_PROVIDER_FIELD_ALL,
					  &provider_attributes);
		} else if (result_wanted == DAT_INVALID_HANDLE) {
			rc = dat_ia_query(evd_handle,
					  &evd_handle,
					  DAT_IA_ALL,
					  &ia_attributes,
					  DAT_PROVIDER_FIELD_ALL,
					  &provider_attributes);
		}
	}

	/* Test dat_cno_query function */
	else if (object_to_query == QUERY_CNO) {

#ifndef __KDAPLTEST__
#if defined(WIN32)
		rc = dat_cno_create(ia_handle, NULLPROXY, &cno_handle);
#else
		rc = dat_cno_create(ia_handle,
				    DAT_OS_WAIT_PROXY_AGENT_NULL, &cno_handle);
#endif

		DT_assert_dat(phead, rc == DAT_SUCCESS);

		if (result_wanted == DAT_SUCCESS) {
			rc = dat_cno_query(cno_handle,
					   DAT_CNO_FIELD_ALL, &cno_param);
		} else if (result_wanted == DAT_INVALID_PARAMETER) {
			rc = dat_cno_query(cno_handle, DAT_CNO_FIELD_ALL, NULL);
		} else if (result_wanted == DAT_INVALID_HANDLE) {
			rc = dat_cno_query(ia_handle,
					   DAT_CNO_FIELD_ALL, &cno_param);
		}
#endif
	}
	/* Test dat_evd_query function */
	else if (object_to_query == QUERY_EVD) {
		if (result_wanted == DAT_SUCCESS) {
			rc = dat_evd_query(evd_handle,
					   DAT_EVD_FIELD_ALL, &evd_param);
		} else if (result_wanted == DAT_INVALID_PARAMETER) {
			rc = dat_evd_query(evd_handle, DAT_EVD_FIELD_ALL, NULL);
		} else if (result_wanted == DAT_INVALID_HANDLE) {
			rc = dat_evd_query(ia_handle,
					   DAT_EVD_FIELD_ALL, &evd_param);
		}
	}

	/* Test dat_psp_query function */
	else if (object_to_query == QUERY_PSP) {
		rc = dat_psp_create(ia_handle,
				    SERVER_PORT_NUMBER,
				    cr_evd_handle,
				    DAT_PSP_PROVIDER_FLAG, &psp_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
		if (result_wanted == DAT_SUCCESS) {
			rc = dat_psp_query(psp_handle,
					   DAT_PSP_FIELD_ALL, &psp_param);
		} else if (result_wanted == DAT_INVALID_PARAMETER) {
			rc = dat_psp_query(psp_handle, DAT_PSP_FIELD_ALL, NULL);
		} else if (result_wanted == DAT_INVALID_HANDLE) {
			rc = dat_psp_query(evd_handle,
					   DAT_PSP_FIELD_ALL, &psp_param);
		}
	}

	/* Test dat_rsp_query function */
	else if (object_to_query == QUERY_RSP) {
		rc = dat_rsp_create(ia_handle,
				    SERVER_PORT_NUMBER,
				    ep_handle, cr_evd_handle, &rsp_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
		rc = dat_rsp_query(rsp_handle, DAT_RSP_FIELD_ALL, &rsp_param);
	}

	/* Test dat_cr_query function */
	else if (object_to_query == QUERY_CR) {
		/* This query is tested in the conmgt test */
		res = -1;
	}

	/* Test dat_ep_query function */
	else if (object_to_query == QUERY_EP) {
		rc = dat_ep_query(ep_handle, DAT_EP_FIELD_ALL, &ep_param);
	}

	/* Test dat_pz_query function */
	else if (object_to_query == QUERY_PZ) {
		rc = dat_pz_query(pz_handle, DAT_PZ_FIELD_ALL, &pz_param);
	}

	/* Test dat_lmr_query function */
	else if (object_to_query == QUERY_LMR) {
		alloc_ptr =
		    (unsigned char *)DT_Mdep_Malloc((size_t) buffer_size);
		DT_assert(phead, alloc_ptr);
		memset(&region, 0, sizeof(region));
		region.for_va = alloc_ptr;
		rc = DT_Tdep_lmr_create(ia_handle, DAT_MEM_TYPE_VIRTUAL, region, buffer_size, pz_handle, DAT_MEM_PRIV_ALL_FLAG, &lmr_handle, &lmr_context, NULL,	/* FIXME */
					&reg_size, &reg_addr);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
		rc = dat_lmr_query(lmr_handle, DAT_LMR_FIELD_ALL, &lmr_param);
	}

	/* Test dat_rmr_query function */
	else if (object_to_query == QUERY_RMR) {
		rc = dat_rmr_create(pz_handle, &rmr_handle);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
		/* We don't bind the RMR to anything, so don't ask for the
		 * LMR_TRIPLET flag
		 */
		rc = dat_rmr_query(rmr_handle,
				   DAT_RMR_FIELD_ALL -
				   DAT_RMR_FIELD_LMR_TRIPLET, &rmr_param);
	}

	DT_assert_dat(phead, DAT_GET_TYPE(rc) == result_wanted);

      cleanup:
	if (rsp_handle) {
		rc = dat_rsp_free(rsp_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

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

	if (lmr_handle) {
		rc = dat_lmr_free(lmr_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

	if (rmr_handle) {
		rc = dat_rmr_free(rmr_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
#ifndef __KDAPLTEST__
	if (cno_handle) {
		rc = dat_cno_free(cno_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
#endif
	if (psp_handle) {
		rc = dat_psp_free(psp_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}

	if (cr_evd_handle) {
		rc = DT_Tdep_evd_free(cr_evd_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
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

int DT_queryinfo_case0(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify IA Querying information is successful using\nDAT_IA_QUERY.\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_IA, DAT_SUCCESS);
}

int DT_queryinfo_case1(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify CNO Querying information is successful using\nDAT_CNO_QUERY.\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_CNO, DAT_SUCCESS);
}

int DT_queryinfo_case2(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify EVD Querying information is successful using\nDAT_EVD_QUERY.\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_EVD, DAT_SUCCESS);
}

int DT_queryinfo_case3(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify PSP Querying information is successful using\nDAT_PSP_QUERY.\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_PSP, DAT_SUCCESS);
}

int DT_queryinfo_case4(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify RSP Querying information is successful using\nDAT_RSP_QUERY.\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_RSP, DAT_SUCCESS);
}

int DT_queryinfo_case5(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify CR Querying information is successful using\nDAT_CR_QUERY.\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_CR, DAT_SUCCESS);
}

int DT_queryinfo_case6(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify EP Querying information is successful using\nDAT_EP_QUERY.\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_EP, DAT_SUCCESS);
}

int DT_queryinfo_case7(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify PZ Querying information is successful using\n");
	DT_Tdep_PT_Printf(phead, "DAT_PZ_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_PZ, DAT_SUCCESS);
}

int DT_queryinfo_case8(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify LMR Querying information is successful using\n");
	DT_Tdep_PT_Printf(phead, "DAT_LMR_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_LMR, DAT_SUCCESS);
}

int DT_queryinfo_case9(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify RMR Querying information is successful using\n");
	DT_Tdep_PT_Printf(phead, "DAT_RMR_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_RMR, DAT_SUCCESS);
}

int DT_queryinfo_case10(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify IA Querying fails with DAT_INVALID_PARAMETER when\n");
	DT_Tdep_PT_Printf(phead, "passing a bad parameter to DAT_IA_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_IA,
				  DAT_INVALID_PARAMETER);
}

int DT_queryinfo_case11(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify IA Querying fails with DAT_INVALID_HANDLE when\n");
	DT_Tdep_PT_Printf(phead, "passing an invalid handle to DAT_IA_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_IA,
				  DAT_INVALID_HANDLE);
}

int DT_queryinfo_case12(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify CNO Querying fails with DAT_INVALID_PARAMETER when\n");
	DT_Tdep_PT_Printf(phead, "passing a bad parameter to DAT_CNO_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_CNO,
				  DAT_INVALID_PARAMETER);
}

int DT_queryinfo_case13(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify CNO Querying fails with DAT_INVALID_HANDLE when\n");
	DT_Tdep_PT_Printf(phead,
			  "passing an invalid handle to DAT_CNO_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_CNO,
				  DAT_INVALID_HANDLE);
}

int DT_queryinfo_case14(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify EVD Querying fails with DAT_INVALID_PARAMETER when\n");
	DT_Tdep_PT_Printf(phead, "passing a bad parameter to DAT_EVD_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_EVD,
				  DAT_INVALID_PARAMETER);
}

int DT_queryinfo_case15(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify EVD Querying fails with DAT_INVALID_HANDLE when\n");
	DT_Tdep_PT_Printf(phead,
			  "passing an invalid handle to DAT_EVD_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_EVD,
				  DAT_INVALID_HANDLE);
}

int DT_queryinfo_case16(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify PSP Querying fails with DAT_INVALID_PARAMETER when\n");
	DT_Tdep_PT_Printf(phead, "passing a bad parameter to DAT_PSP_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_PSP,
				  DAT_INVALID_PARAMETER);
}

int DT_queryinfo_case17(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead,
			  "Description: Verify PSP Querying fails with DAT_INVALID_HANDLE when\n");
	DT_Tdep_PT_Printf(phead,
			  "passing an invalid handle to DAT_PSP_QUERY\n");
	return DT_queryinfo_basic(params_ptr, cmd, QUERY_PSP,
				  DAT_INVALID_HANDLE);
}

/*-------------------------------------------------------------*/
void DT_queryinfo_test(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	int i;
	int res;
	DT_Tdep_Print_Head *phead;
	FFT_Testfunc_t cases_func[] = {
		{DT_queryinfo_case0},
		{DT_queryinfo_case1},
		{DT_queryinfo_case2},
		{DT_queryinfo_case3},
		{DT_queryinfo_case4},
		{DT_queryinfo_case5},
		{DT_queryinfo_case6},
		{DT_queryinfo_case7},
		{DT_queryinfo_case8},
		{DT_queryinfo_case9},
		{DT_queryinfo_case10},
		{DT_queryinfo_case11},
#ifndef __KDAPLTEST__
		{DT_queryinfo_case12},
		{DT_queryinfo_case13},
#endif
		{DT_queryinfo_case14},
		{DT_queryinfo_case15},
		{DT_queryinfo_case16},
		{DT_queryinfo_case17},
	};

	phead = params_ptr->phead;
	for (i = 0; i < cmd->size; i++) {
		if (cmd->cases_flag[i]) {
			DT_Tdep_PT_Printf(phead,
					  "*********************************************************************\n");
			DT_Tdep_PT_Printf(phead,
					  "Function feature: Queryinfo                                  case: %d\n",
					  i);
			res = cases_func[i].fun(params_ptr, cmd);
			if (res == 1) {
				DT_Tdep_PT_Printf(phead, "Result: PASS\n");
			} else if (res == 0) {
				DT_Tdep_PT_Printf(phead, "Result: FAIL\n");
			} else if (res == -1) {
				DT_Tdep_PT_Printf(phead, "Result: UNSUPP\n");
			}

			DT_Tdep_PT_Printf(phead,
					  "*********************************************************************\n");
		}
	}
	return;
}
