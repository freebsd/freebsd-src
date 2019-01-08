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

int DT_connmgt_case0(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	FFT_Connection_t conn;
	int res = 1;
	DAT_RETURN rc = 0;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	DT_Tdep_PT_Printf(phead, "\
	Description: Ensure time in dat_evd_wait works correctly\n");

	DT_fft_init_server(params_ptr, cmd, &conn);
	DT_assert(phead, NULL != conn.ia_handle);
	rc = DT_Tdep_evd_wait(conn.cr_evd, 10000, &conn.event);
	DT_assert_dat(phead, DAT_GET_TYPE(rc) == DAT_TIMEOUT_EXPIRED);

      cleanup:
	rc = DT_fft_destroy_conn_struct(params_ptr, &conn);
	DT_assert_clean(phead, rc == DAT_SUCCESS);

	return res;
}

int DT_connmgt_case1(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	FFT_Connection_t conn;
	int res = 1;
	DAT_RETURN rc;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	DT_Tdep_PT_Printf(phead, "\
	Description: Attempt to use timeout of 0 in dat_evd_wait\n");

	DT_fft_init_server(params_ptr, cmd, &conn);
	DT_assert(phead, NULL != conn.ia_handle);

	rc = DT_Tdep_evd_wait(conn.cr_evd, 0, &conn.event);
	DT_assert_dat(phead, DAT_GET_TYPE(rc) == DAT_TIMEOUT_EXPIRED);

      cleanup:
	rc = DT_fft_destroy_conn_struct(params_ptr, &conn);
	DT_assert_clean(phead, rc == DAT_SUCCESS);
	return res;

}

void DT_connmgt_test(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	int i;
	int res;
	DT_Tdep_Print_Head *phead;
	FFT_Testfunc_t cases_func[] = {
		{DT_connmgt_case0},
		{DT_connmgt_case1},
	};
	phead = params_ptr->phead;
	for (i = 0; i < cmd->size; i++) {
		if (cmd->cases_flag[i]) {
			if (cmd->cases_flag[i]) {

				DT_Tdep_PT_Printf(phead, "\
		    *********************************************************************\n");
				DT_Tdep_PT_Printf(phead, "\
		    Function feature: Connect Management (Server side)     case: %d\n", i);
				res = cases_func[i].fun(params_ptr, cmd);
				if (res == 1) {
					DT_Tdep_PT_Printf(phead,
							  "Result: PASS\n");
				} else if (res == 0) {
					DT_Tdep_PT_Printf(phead,
							  "Result: FAIL\n");
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
	}
}
