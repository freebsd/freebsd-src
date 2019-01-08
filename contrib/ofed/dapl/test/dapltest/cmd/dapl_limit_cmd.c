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

/* --------------------------------------------------- */
void DT_Limit_Cmd_Init(Limit_Cmd_t * cmd)
{
	memset((void *)cmd, 0, sizeof(Limit_Cmd_t));
	cmd->ReliabilityLevel = DAT_QOS_BEST_EFFORT;
	cmd->width = 1;
	cmd->maximum = ~0U;
}

/* --------------------------------------------------- */
bool
DT_Limit_Cmd_Parse(Limit_Cmd_t * cmd,
		   int my_argc, char **my_argv, mygetopt_t * opts)
{
	int c;
	int i;

	for (;;) {
		c = DT_mygetopt_r(my_argc, my_argv, "dm:w:D:R:", opts);
		if (c == EOF) {
			break;
		}
		switch (c) {
		case 'D':	/* device name */
			{
				strcpy(cmd->device_name, opts->optarg);
				break;
			}
		case 'R':	/* Service Reliability Level */
			{
				cmd->ReliabilityLevel =
				    DT_ParseQoS(opts->optarg);
				break;
			}
		case 'd':	/* print debug messages */
			{
				DT_dapltest_debug++;
				cmd->debug = true;
				break;
			}
		case 'm':	/* maximum for exhaustion testing */
			{
				unsigned int len =
				    strspn(opts->optarg, "0123456789");

				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -m<maximum> option\n");
					DT_Limit_Cmd_Usage();
					return (false);
				}
				cmd->maximum = atol(opts->optarg);
				break;
			}
		case 'w':	/* width (number of {ia,evd,ep,...} sets) */
			{
				unsigned int len =
				    strspn(opts->optarg, "0123456789");

				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -w<width> option\n");
					DT_Limit_Cmd_Usage();
					return (false);
				}
				cmd->width = atol(opts->optarg);
				break;
			}
		case '?':
		default:
			{
				DT_Mdep_printf
				    ("Invalid Limit Test Parameter: %c\n", c);
				DT_Limit_Cmd_Usage();
				return (false);
			}
		}
	}
	if (cmd->device_name[0] == '\0') {
		if (!DT_Mdep_GetDefaultDeviceName(cmd->device_name)) {
			DT_Mdep_printf("can't get default device name\n");
			DT_Limit_Cmd_Usage();
			return (false);
		}
	}

	/*
	 * by default: test all limit tests
	 * otherwise:  parse the remaining limit test arguments
	 */
	if (opts->optind == my_argc) {
		for (i = 0; i < LIM_NUM_TESTS; i++) {
			cmd->Test_List[i] = 1;
		}
	} else {
		for (i = opts->optind; i < my_argc; i++) {

			if (strcmp(my_argv[i], "limit_ia") == 0) {
				cmd->Test_List[LIM_IA] = 1;
				continue;
			}
			if (strcmp(my_argv[i], "limit_pz") == 0) {
				cmd->Test_List[LIM_PZ] = 1;
				continue;
			}
#ifndef __KDAPLTEST__
			if (strcmp(my_argv[i], "limit_cno") == 0) {
				cmd->Test_List[LIM_CNO] = 1;
				continue;
			}
#endif
			if (strcmp(my_argv[i], "limit_evd") == 0) {
				cmd->Test_List[LIM_EVD] = 1;
				continue;
			}
			if (strcmp(my_argv[i], "limit_ep") == 0) {
				cmd->Test_List[LIM_EP] = 1;
				continue;
			}
			if (strcmp(my_argv[i], "limit_rsp") == 0) {
				cmd->Test_List[LIM_RSP] = 1;
				continue;
			}
			if (strcmp(my_argv[i], "limit_psp") == 0) {
				cmd->Test_List[LIM_PSP] = 1;
				continue;
			}
			if (strcmp(my_argv[i], "limit_lmr") == 0) {
				cmd->Test_List[LIM_LMR] = 1;
				continue;
			}
			if (strcmp(my_argv[i], "limit_rpost") == 0) {
				cmd->Test_List[LIM_RPOST] = 1;
				continue;
			}
			if (strcmp(my_argv[i], "limit_size_lmr") == 0) {
				cmd->Test_List[LIM_SIZE_LMR] = 1;
				continue;
			}

			DT_Mdep_printf("Cannot find this limit test: %s\n",
				       my_argv[i]);
			DT_Limit_Cmd_Usage();
			return (false);

		}		/* end foreach remaining argv */
	}

	return (true);
}

/* --------------------------------------------------- */
void DT_Limit_Cmd_Usage(void)
{
	DT_Mdep_printf("USAGE: ---- LIMIT TEST ----\n");
	DT_Mdep_printf("USAGE:     dapltest -T L\n");
	DT_Mdep_printf("USAGE:              [-D <device Name>]\n");
	DT_Mdep_printf("USAGE:              [-d] : debug (zero)\n");
	DT_Mdep_printf("USAGE:              [-w <width_of_resource_sets>]\n");
	DT_Mdep_printf
	    ("USAGE:              [-m <maximum_for_exhaustion_tests>]\n");
	DT_Mdep_printf("USAGE:              [-R <service reliability>]\n");
	DT_Mdep_printf
	    ("USAGE:                  (BE == QOS_BEST_EFFORT - Default)\n");
	DT_Mdep_printf("USAGE:                  (HT == QOS_HIGH_THROUGHPUT)\n");
	DT_Mdep_printf("USAGE:                  (LL == QOS_LOW_LATENCY)\n");
	DT_Mdep_printf("USAGE:                  (EC == QOS_ECONOMY)\n");
	DT_Mdep_printf("USAGE:                  (PM == QOS_PREMIUM)\n");
	DT_Mdep_printf
	    ("USAGE:              [limit_ia [limit_pz] [limit_evd] ... ]\n");
	DT_Mdep_printf
	    ("NOTE: If test is not specified, do all the limit tests\n");
	DT_Mdep_printf("NOTE: Else, just do the specified tests\n");
	DT_Mdep_printf
	    ("NOTE: Each test is separated by space, the test can be:\n");

	DT_Mdep_printf("NOTE: [limit_ia]         test max num  of  open IAs\n");
	DT_Mdep_printf("NOTE: [limit_pz]         test max num  of  PZs\n");
#ifndef __KDAPLTEST__
	DT_Mdep_printf("NOTE: [limit_cno]        test max num  of  CNOs\n");
#endif
	DT_Mdep_printf("NOTE: [limit_evd]        test max num  of  EVDs\n");
	DT_Mdep_printf("NOTE: [limit_rsp]        test max num  of  RSPs\n");
	DT_Mdep_printf("NOTE: [limit_psp]        test max num  of  PSPs\n");
	DT_Mdep_printf("NOTE: [limit_ep]         test max num  of  EPs\n");
	DT_Mdep_printf("NOTE: [limit_lmr]        test max num  of  LMRs\n");
	DT_Mdep_printf
	    ("NOTE: [limit_rpost]      test max num  of  recvs posted\n");
	DT_Mdep_printf("NOTE: [limit_size_lmr]   test max size of  LMR\n");
}
