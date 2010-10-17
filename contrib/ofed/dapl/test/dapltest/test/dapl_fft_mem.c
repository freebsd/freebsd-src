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

/*--------------------------------------------------------*/
int DT_mem_generic(Params_t * params_ptr, FFT_Cmd_t * cmd, int flag)
{
	DAT_RETURN rc, expect;
	FFT_Connection_t conn;
	DAT_REGION_DESCRIPTION region;
	DAT_VLEN reg_size;
	DAT_LMR_HANDLE lmr_handle;
	DAT_LMR_CONTEXT lmr_context;
	DAT_VADDR reg_addr;
	unsigned char *alloc_ptr;
	int res;
	DAT_VLEN buffer_size;
	DAT_IA_HANDLE ia_handle;
	DAT_PZ_HANDLE pz_handle;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	rc = 0;
	expect = 0;
	res = 1;
	lmr_handle = 0;
	lmr_context = 0;
	reg_addr = 0;
	alloc_ptr = 0;
	ia_handle = 0;
	pz_handle = 0;

	DT_fft_init_client(params_ptr, cmd, &conn);
	DT_assert(phead, NULL != conn.ia_handle);

	if (flag == 2) {
		buffer_size = 0;
		alloc_ptr = 0;
	} else {
		buffer_size = BUFFSIZE * sizeof(unsigned char);
		alloc_ptr =
		    (unsigned char *)DT_Mdep_Malloc((size_t) buffer_size);
		DT_assert(phead, alloc_ptr);
	}

	memset(&region, 0, sizeof(region));
	region.for_va = alloc_ptr;

	ia_handle = conn.ia_handle;

	if (flag != 3) {
		pz_handle = conn.pz_handle;
	}

	if (flag != 4) {
		DT_Tdep_PT_Printf(phead, "Registering memory\n");
		rc = DT_Tdep_lmr_create(ia_handle, DAT_MEM_TYPE_VIRTUAL, region, buffer_size, conn.pz_handle, DAT_MEM_PRIV_ALL_FLAG, &lmr_handle, &lmr_context, NULL,	/* FIXME */
					&reg_size, &reg_addr);
		if (flag == 2) {
			expect = DAT_LENGTH_ERROR;
		} else {
			expect = DAT_SUCCESS;
		}
		DT_assert_dat(phead, DAT_GET_TYPE(rc) == expect);
	}
	if (flag == 1) {
		if (lmr_handle) {
			rc = dat_lmr_free(lmr_handle);
			DT_assert_dat(phead, rc == DAT_SUCCESS);
		}
		lmr_handle = 0;

		rc = DT_Tdep_lmr_create(conn.ia_handle, DAT_MEM_TYPE_VIRTUAL, region, buffer_size, conn.pz_handle, DAT_MEM_PRIV_ALL_FLAG, &lmr_handle, &lmr_context, NULL,	/* FIXME */
					&reg_size, &reg_addr);
		DT_assert_dat(phead, rc == DAT_SUCCESS);
	}

      cleanup:
	if (lmr_handle) {
		rc = dat_lmr_free(lmr_handle);
		DT_assert_clean(phead, rc == DAT_SUCCESS);
	}
	if (alloc_ptr) {
		DT_Mdep_Free(alloc_ptr);
	}
	rc = DT_fft_destroy_conn_struct(params_ptr, &conn);
	DT_assert_clean(phead, rc == DAT_SUCCESS);

	return res;

}

int DT_mem_case0(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: Test if we can register typical size of memory\n");
	DT_Tdep_PT_Printf(phead, "\
	then deregister it.\n");
	return DT_mem_generic(params_ptr, cmd, 0);
}

/*--------------------------------------------------------*/
int DT_mem_case1(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: Test if we can register typical size of memory\n");
	DT_Tdep_PT_Printf(phead, "\
	deregister, then register it again.\n");
	return DT_mem_generic(params_ptr, cmd, 1);
}

/*--------------------------------------------------------*/
int DT_mem_case2(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: Try to register memory with memory size 0\n");
	return DT_mem_generic(params_ptr, cmd, 2);
}

/*--------------------------------------------------------*/
int DT_mem_case3(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: Try to register memory with null pz\n");
	return DT_mem_generic(params_ptr, cmd, 3);
}

/*--------------------------------------------------------*/
int DT_mem_case4(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;
	DT_Tdep_PT_Printf(phead, "\
	Description: Try to deregister memory with null lmr_handle\n");
	return DT_mem_generic(params_ptr, cmd, 4);
}

/*-------------------------------------------------------------*/
void DT_mem_test(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	int i;
	int res;
	DT_Tdep_Print_Head *phead;

	FFT_Testfunc_t cases_func[] = {
		{DT_mem_case0},
		{DT_mem_case1},
		{DT_mem_case2},
		{DT_mem_case3},
		{DT_mem_case4},
	};

	phead = params_ptr->phead;
	for (i = 0; i < cmd->size; i++) {
		if (cmd->cases_flag[i]) {
			DT_Tdep_PT_Printf(phead, "\
		*********************************************************************\n");
			DT_Tdep_PT_Printf(phead, "\
		Function feature: Memory register/deregister       case: %d\n", i);
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
