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

Started_server_t *DT_started_server_list = 0;

void DT_Server_Info_Endian(Server_Info_t * server_info)
{
	server_info->dapltest_version =
	    DT_Endian32(server_info->dapltest_version);
	server_info->is_little_endian =
	    DT_Endian32(server_info->is_little_endian);
	server_info->first_port_number =
	    DT_Endian32(server_info->first_port_number);
}

void
DT_Server_Info_Print(DT_Tdep_Print_Head * phead, Server_Info_t * server_info)
{
	DT_Tdep_PT_Printf(phead, "-------------------------------------\n");
	DT_Tdep_PT_Printf(phead, "Server_Info.dapltest_version   : %d\n",
			  server_info->dapltest_version);
	DT_Tdep_PT_Printf(phead, "Server_Info.is_little_endian   : %d\n",
			  server_info->is_little_endian);
}
