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

DAT_RETURN DT_cs_FFT(Params_t * params_ptr, FFT_Cmd_t * cmd)
{
	DT_Tdep_Print_Head *phead;
	DAT_RETURN rc = DAT_SUCCESS;

	phead = params_ptr->phead;

	switch (cmd->fft_type) {
	case HWCONN:
		{
			DT_hwconn_test(params_ptr, cmd);
			break;
		}
	case ENDPOINT:
		{
			DT_endpoint_test(params_ptr, cmd);
			break;
		}
	case PTAGMGT:
		{
			DT_pz_test(params_ptr, cmd);
			break;
		}
	case MEMMGT:
		{
			DT_mem_test(params_ptr, cmd);
			break;
		}
	case CONNMGT:
		{
			DT_connmgt_test(params_ptr, cmd);
			break;
		}
	case QUERYINFO:
		{
			DT_queryinfo_test(params_ptr, cmd);
			break;
		}
#if 0				// not yet implemented
	case CONNMGT_CLIENT:
	case NS:
	case ERRHAND:
	case UNSUPP:
	case STRESS:
	case STRESS_CLIENT:
	case CQMGT:
		{
			DT_Tdep_PT_Printf(phead, "Not Yet Implemented\n");
			break;
		}
#endif
	default:
		{
			DT_Tdep_PT_Printf(phead, "don't know this test\n");
			rc = DAT_INVALID_PARAMETER;
			break;
		}
	}
	return rc;
}
