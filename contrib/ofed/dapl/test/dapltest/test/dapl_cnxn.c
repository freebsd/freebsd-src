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

/****************************************************************************/
int get_ep_connection_state(DT_Tdep_Print_Head * phead, DAT_EP_HANDLE ep_handle)
{
	DAT_EP_STATE ep_state;
	DAT_BOOLEAN in_dto_idle;
	DAT_BOOLEAN out_dto_idle;
	DAT_RETURN ret;
	char *recv_status = "Idle";
	char *req_status = "Idle";

	ret = dat_ep_get_status(ep_handle, &ep_state, &in_dto_idle,
				&out_dto_idle);
	if (ret != 0) {
		DT_Tdep_PT_Printf(phead,
				  "DAT_ERROR: Can't get Connection State %s\n",
				  DT_RetToString(ret));
	} else {
		if (in_dto_idle == 0) {
			recv_status = "Active";
		}
		if (out_dto_idle == 0) {
			req_status = "Active";
		}

		DT_Tdep_PT_Printf(phead,
				  "DAT_STATE: %s\n", DT_State2Str(ep_state));
		DT_Tdep_PT_Printf(phead,
				  "DAT_STATE: Inbound DTO Status: %s \n",
				  recv_status);
		DT_Tdep_PT_Printf(phead,
				  "DAT_STATE: Outbound DTO Status: %s\n",
				  req_status);
	}

	return 0;
}
