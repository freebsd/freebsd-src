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

#include "dapl_transaction_cmd.h"
#include "dapl_performance_cmd.h"
#include "dapl_quit_cmd.h"
#include "dapl_limit_cmd.h"

DAT_RETURN DT_Execute_Test(Params_t * params_ptr)
{
	DAT_RETURN rc = DAT_SUCCESS;
	Transaction_Cmd_t *Transaction_Cmd;
	Quit_Cmd_t *Quit_Cmd;
	Limit_Cmd_t *Limit_Cmd;
	Performance_Cmd_t *Performance_Cmd;
	FFT_Cmd_t *FFT_Cmd;

	/* re init global data - for kdapltest, we are now in the kernel */
	DT_local_is_little_endian = params_ptr->local_is_little_endian;
	DT_dapltest_debug = params_ptr->debug;

	switch (params_ptr->test_type) {
	case SERVER_TEST:
		{
			DT_cs_Server(params_ptr);
			break;
		}
	case TRANSACTION_TEST:
		{
			Transaction_Cmd = &params_ptr->u.Transaction_Cmd;
			rc = DT_cs_Client(params_ptr,
					  Transaction_Cmd->dapl_name,
					  Transaction_Cmd->server_name,
					  Transaction_Cmd->num_threads *
					  Transaction_Cmd->eps_per_thread);
			break;
		}
	case QUIT_TEST:
		{
			Quit_Cmd = &params_ptr->u.Quit_Cmd;
			(void)DT_cs_Client(params_ptr,
					   Quit_Cmd->device_name,
					   Quit_Cmd->server_name, 0);
			break;
		}
	case LIMIT_TEST:
		{
			Limit_Cmd = &params_ptr->u.Limit_Cmd;
			rc = DT_cs_Limit(params_ptr, Limit_Cmd);
			break;
		}
	case PERFORMANCE_TEST:
		{
			Performance_Cmd = &params_ptr->u.Performance_Cmd;
			rc = DT_cs_Client(params_ptr,
					  Performance_Cmd->dapl_name,
					  Performance_Cmd->server_name, 1);
			break;
		}

	case FFT_TEST:
		{
			FFT_Cmd = &params_ptr->u.FFT_Cmd;
			rc = DT_cs_FFT(params_ptr, FFT_Cmd);
			break;
		}
	}
	return rc;
}
