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

/*--------------------------------------------------------*/
int DT_hwconn_case0(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	char *dev_name;
	DAT_IA_HANDLE nic_handle;
	DAT_EVD_HANDLE evd_handle;
	DAT_RETURN rc;
	int res = 1;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	DT_Tdep_PT_Printf(phead, "\
	Description: Test if we can normally Open NIC and then close it\n");

	dev_name = cmd->device_name;
	nic_handle = 0;
	evd_handle = DAT_HANDLE_NULL;

	rc = dat_ia_open((const DAT_NAME_PTR)dev_name, 10, &evd_handle,
			 &nic_handle);
	DT_assert_dat(phead, rc == DAT_SUCCESS);

	rc = dat_ia_close(nic_handle, DAT_CLOSE_ABRUPT_FLAG);
	DT_assert_dat(phead, rc == DAT_SUCCESS);
      cleanup:

	return res;
}

/*--------------------------------------------------------*/
int DT_hwconn_case1(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DAT_IA_HANDLE nic_handle;
	DAT_RETURN rc;
	DAT_EVD_HANDLE evd_handle;
	char dev_name[100];
	int i;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	DT_Tdep_PT_Printf(phead,
			  "Description: try to open NIC with incorrect device name\n");
	DT_Tdep_PT_Printf(phead,
			  " (just num, one letter, multiple letter, num_letter\n");
	DT_Tdep_PT_Printf(phead,
			  "letter_num). You alse can do this test manually\n");
	DT_Tdep_PT_Printf(phead,
			  "dapltest -T F -D <device_name> -f hwconn <case>\n");

	for (i = 0; i < 5; i++) {
		if (i == 0) {
			sprintf(dev_name, "%s", "40");	/* just number */
		} else if (i == 1) {
			sprintf(dev_name, "%s", "x");	/* just letter */
		} else if (i == 2) {
			sprintf(dev_name, "%s", "xsdf");	/* multiple letter */
		} else if (i == 3) {
			sprintf(dev_name, "%s", "x34");	/* letter_number */
		} else if (i == 4) {
			sprintf(dev_name, "%s", "34df");	/* number_letter */
		}

		evd_handle = DAT_HANDLE_NULL;
		rc = dat_ia_open((const DAT_NAME_PTR)dev_name, 10, &evd_handle,
				 &nic_handle);
		if (DAT_GET_TYPE(rc) != DAT_PROVIDER_NOT_FOUND) {
			const char *major_msg, *minor_msg;

			DT_Tdep_PT_Printf(phead, " \
		fff not get expected result when open NIC with device name: %s\n", dev_name);
			dat_strerror(rc, &major_msg, &minor_msg);
			DT_Tdep_PT_Printf(phead, "ERROR: %s (%s)\n", major_msg,
					  minor_msg);

			if (rc == DAT_SUCCESS) {
				rc = dat_ia_close(nic_handle,
						  DAT_CLOSE_ABRUPT_FLAG);
				DT_assert_clean(phead, rc == DAT_SUCCESS);
			}
			return 0;
		}
	}
	return 1;
}

/*--------------------------------------------------------*/
int DT_hwconn_case2(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DAT_IA_HANDLE nic_handle;
	DAT_RETURN rc;
	int res = 1;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	DT_Tdep_PT_Printf(phead, "\
	Description: Try to close nic with Nic handle is null (NIC not open)\n");
	nic_handle = 0;
	rc = dat_ia_close(nic_handle, DAT_CLOSE_ABRUPT_FLAG);
	DT_assert_dat(phead, DAT_GET_TYPE(rc) == DAT_INVALID_HANDLE);

      cleanup:
	return res;
}

/*--------------------------------------------------------*/
int DT_hwconn_case3(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	FFT_Connection_t conn;
	DAT_RETURN rc;
	int res;
	DT_Tdep_Print_Head *phead;
	phead = params_ptr->phead;

	DT_Tdep_PT_Printf(phead,
			  "Description: Test if we can close NIC when the created \n");
	DT_Tdep_PT_Printf(phead, "endpoint has not been destroyed.\n");
	DT_Tdep_PT_Printf(phead,
			  "The problem for this case is that once the hca is closed, \n");
	DT_Tdep_PT_Printf(phead,
			  "there is no way to destroy the endpoint's resources\n");
	DT_Tdep_PT_Printf(phead,
			  "thus the test leaks a small amount of memory\n");

	res = 1;

	DT_fft_init_client(params_ptr, cmd, &conn);

	/* try to close nic when vi have not destroyed */
	if (conn.ia_handle) {
		rc = dat_ia_close(conn.ia_handle, DAT_CLOSE_ABRUPT_FLAG);
		if (rc != DAT_SUCCESS) {
			DT_Tdep_PT_Printf(phead,
					  "Warning: dat_ia_close fails %s, reboot for cleanup\n",
					  DT_RetToString(rc));
			return 0;
		}
	} else {
		res = 0;
	}
	/* if nic is closed, it is impossible to destory vi and ptag */
	//DT_fft_destroy_conn_struct(&conn);
	return res;

}

/*-------------------------------------------------------------*/
void DT_hwconn_test(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	int i;
	int res;
	DT_Tdep_Print_Head *phead;

	FFT_Testfunc_t cases_func[] = {
		{DT_hwconn_case0},
		{DT_hwconn_case1},
		{DT_hwconn_case2},
		{DT_hwconn_case3},
	};

	phead = params_ptr->phead;
	for (i = 0; i < cmd->size; i++) {
		if (cmd->cases_flag[i]) {
			DT_Tdep_PT_Printf(phead, "\
		*********************************************************************\n");
			DT_Tdep_PT_Printf(phead, "\
		Function feature: Hardware connection        case: %d\n", i);
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
						  "Result: next stage to develop\n");
			}

			DT_Tdep_PT_Printf(phead, "\
		*********************************************************************\n");
		}
	}
	return;
}
