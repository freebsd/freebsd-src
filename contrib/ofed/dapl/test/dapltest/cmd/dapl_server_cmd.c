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

void DT_Server_Cmd_Init(Server_Cmd_t * Server_Cmd)
{
	DT_dapltest_debug = 0;
	Server_Cmd->debug = false;
	Server_Cmd->dapl_name[0] = '\0';
	Server_Cmd->ReliabilityLevel = DAT_QOS_BEST_EFFORT;
}

bool
DT_Server_Cmd_Parse(Server_Cmd_t * Server_Cmd,
		    int my_argc, char **my_argv, mygetopt_t * opts)
{
	int c;
	for (;;) {
		c = DT_mygetopt_r(my_argc, my_argv, "dD:R:", opts);
		if (c == EOF) {
			break;
		}
		switch (c) {
		case 'D':
			{
				strcpy(Server_Cmd->dapl_name, opts->optarg);
				break;
			}
		case 'd':
			{
				DT_dapltest_debug++;
				Server_Cmd->debug = true;
				break;
			}
		case 'R':	/* Service Reliability Level */
			{
				Server_Cmd->ReliabilityLevel =
				    DT_ParseQoS(opts->optarg);
				if (0 == Server_Cmd->ReliabilityLevel) {
					return (false);
				}
				break;
			}
		case '?':
		default:
			{
				DT_Mdep_printf("Invalid Server option: %c\n",
					       opts->optopt);
				DT_Server_Cmd_Usage();
				return (false);
			}
		}
	}
	if (Server_Cmd->dapl_name == '\0') {
		if (!DT_Mdep_GetDefaultDeviceName(Server_Cmd->dapl_name)) {
			DT_Mdep_printf("can't get default device name\n");
			DT_Server_Cmd_Usage();
			return (false);
		}
	}
	return (true);
}

void DT_Server_Cmd_Usage(void)
{
	DT_Mdep_printf("USAGE: ---- SERVER MODE ----\n");
	DT_Mdep_printf("USAGE:     dapltest -T S\n");
	DT_Mdep_printf("USAGE:              [-D <device Name>]\n");
	DT_Mdep_printf("USAGE:              [-d] : debug (zero)\n");
	DT_Mdep_printf("USAGE:              [-R <service reliability>]\n");
	DT_Mdep_printf
	    ("USAGE:                  (BE == QOS_BEST_EFFORT - Default)\n");
	DT_Mdep_printf("USAGE:                  (HT == QOS_HIGH_THROUGHPUT)\n");
	DT_Mdep_printf("USAGE:                  (LL == QOS_LOW_LATENCY)\n");
	DT_Mdep_printf("USAGE:                  (EC == QOS_ECONOMY)\n");
	DT_Mdep_printf("USAGE:                  (PM == QOS_PREMIUM)\n");
	DT_Mdep_printf("USAGE: Run as server using default parameters\n");
	DT_Mdep_printf("USAGE:     dapltest\n");
	DT_Mdep_printf("USAGE:\n");
}

void DT_Server_Cmd_Print(Server_Cmd_t * Server_Cmd)
{
	DT_Mdep_printf("Server_Cmd.debug:       %d\n", Server_Cmd->debug);
	DT_Mdep_printf("Server_Cmd.dapl_name: %s\n", Server_Cmd->dapl_name);
}
