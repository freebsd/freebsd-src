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

/*--------------------------------------------------------- */
void DT_Quit_Cmd_Init(Quit_Cmd_t * cmd)
{
	memset((void *)cmd, 0, sizeof(Quit_Cmd_t));
	cmd->ReliabilityLevel = DAT_QOS_BEST_EFFORT;
}

/*--------------------------------------------------------- */
bool
DT_Quit_Cmd_Parse(Quit_Cmd_t * cmd,
		  int my_argc, char **my_argv, mygetopt_t * opts)
{
	int c;

	for (;;) {
		c = DT_mygetopt_r(my_argc, my_argv, "ds:D:R:", opts);
		if (c == EOF) {
			break;
		}
		switch (c) {
		case 'D':
			{
				strcpy(cmd->device_name, opts->optarg);
				break;
			}
		case 's':
			{
				strcpy(cmd->server_name, opts->optarg);
				break;
			}
		case 'd':	/* print debug messages */
			{
				DT_dapltest_debug++;
				cmd->debug = true;
				break;
			}
		case 'R':	/* Service Reliability Level */
			{
				cmd->ReliabilityLevel =
				    DT_ParseQoS(opts->optarg);
				break;
			}
		case '?':
		default:
			{
				DT_Mdep_printf("Invalid Quit option: %c\n",
					       opts->optopt);
				DT_Quit_Cmd_Usage();
				return (false);
			}
		}
	}
	if (cmd->device_name[0] == '\0') {
		if (!DT_Mdep_GetDefaultDeviceName(cmd->device_name)) {
			DT_Mdep_printf("can't get default device name\n");
			DT_Quit_Cmd_Usage();
			return (false);
		}
	}
	if (!DT_Quit_Cmd_Validate(cmd)) {
		DT_Quit_Cmd_Usage();
		return (false);
	}
	return (true);
}

/*------------------------------------------------------------------------------ */
bool DT_Quit_Cmd_Validate(Quit_Cmd_t * cmd)
{
	if (cmd->server_name[0] == '\0') {
		DT_Mdep_printf
		    ("Must specify server_name in command line or scriptfile\n");
		return (false);
	}
	return (true);
}

/*--------------------------------------------------------- */
void DT_Quit_Cmd_Usage(void)
{
	DT_Mdep_printf("USAGE: ---- QUIT TEST ----\n");
	DT_Mdep_printf("USAGE:     dapltest -T Q\n");
	DT_Mdep_printf("USAGE:              -s <server Name>\n");
	DT_Mdep_printf("USAGE:              [-D <device Name>]\n");
	DT_Mdep_printf("USAGE:              [-d] : debug (zero)\n");
	DT_Mdep_printf("USAGE:              [-R <service reliability>]\n");
	DT_Mdep_printf
	    ("USAGE:                  (BE == QOS_BEST_EFFORT - Default)\n");
	DT_Mdep_printf("USAGE:                  (HT == QOS_HIGH_THROUGHPUT)\n");
	DT_Mdep_printf("USAGE:                  (LL == QOS_LOW_LATENCY)\n");
	DT_Mdep_printf("USAGE:                  (EC == QOS_ECONOMY)\n");
	DT_Mdep_printf("USAGE:                  (PM == QOS_PREMIUM)\n");
}

/*--------------------------------------------------------- */
void DT_Quit_Cmd_Print(Quit_Cmd_t * cmd)
{
	DT_Mdep_printf("Quit_Cmd.server_name: %s\n", cmd->server_name);
	DT_Mdep_printf("Quit_Cmd.device_name: %s\n", cmd->device_name);
}
