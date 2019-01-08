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

void DT_Client_Info_Endian(Client_Info_t * client_info)
{
	client_info->dapltest_version =
	    DT_Endian32(client_info->dapltest_version);
	client_info->is_little_endian =
	    DT_Endian32(client_info->is_little_endian);
	client_info->test_type = DT_Endian32(client_info->test_type);
	client_info->total_threads = DT_Endian32(client_info->total_threads);
}

void
DT_Client_Info_Print(DT_Tdep_Print_Head * phead, Client_Info_t * client_info)
{
	DT_Tdep_PT_Printf(phead, "-------------------------------------\n");
	DT_Tdep_PT_Printf(phead,
			  "Client_Info.dapltest_version   : %d\n",
			  client_info->dapltest_version);
	DT_Tdep_PT_Printf(phead,
			  "Client_Info.is_little_endian   : %d\n",
			  client_info->is_little_endian);
	DT_Tdep_PT_Printf(phead,
			  "Client_Info.test_type          : %d\n",
			  client_info->test_type);
}
