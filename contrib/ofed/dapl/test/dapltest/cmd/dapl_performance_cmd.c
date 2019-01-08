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

static void DT_Performance_Cmd_Usage(void)
{
	DT_Mdep_printf("USAGE: ---- PERFORMANCE TEST ----\n");
	DT_Mdep_printf("USAGE:     dapltest -T P\n");
	DT_Mdep_printf("USAGE:              -s <server Name>\n");
	DT_Mdep_printf("USAGE:              [-m b|p]\n");
	DT_Mdep_printf("USAGE:              [-D <device Name>]\n");
	DT_Mdep_printf("USAGE:              [-d] : debug (zero)\n");
	DT_Mdep_printf
	    ("USAGE:              [-i <num iterations>]     : (1, 000)\n");
	DT_Mdep_printf("USAGE:              [-p <pipline>]\n");
	DT_Mdep_printf("USAGE:              [-R <service reliability>]\n");
	DT_Mdep_printf
	    ("USAGE:                  (BE == QOS_BEST_EFFORT - Default)\n");
	DT_Mdep_printf("USAGE:                  (HT == QOS_HIGH_THROUGHPUT)\n");
	DT_Mdep_printf("USAGE:                  (LL == QOS_LOW_LATENCY)\n");
	DT_Mdep_printf("USAGE:                  (EC == QOS_ECONOMY)\n");
	DT_Mdep_printf("USAGE:                  (PM == QOS_PREMIUM)\n");
	DT_Mdep_printf("USAGE:           <OP>\n");
	DT_Mdep_printf("USAGE:\n");
	DT_Mdep_printf("USAGE: Each OP consists of:\n");
	DT_Mdep_printf("USAGE:     <transfer_type> : \"RR\" (RDMA READ)\n");
	DT_Mdep_printf("USAGE:                     : \"RW\" (RDMA WRITE)\n");
	DT_Mdep_printf("USAGE:     [seg_size [num_segs] ]      : (4096, 1)\n");
}

static bool
DT_Performance_Cmd_Parse_Op(Performance_Cmd_t * cmd,
			    int index, int my_argc, char **my_argv)
{
	int i;

	/*
	 * Op Format: <RR/RW> [seg_size] [num_segs]
	 */

	if (index == my_argc) {
		DT_Mdep_printf("Operation Missing Transfer Type\n");
		return (false);
	}

	for (i = 0; index < my_argc; i++, index++) {
		switch (i) {
		case 0:
			{
				if (0 ==
				    strncmp(my_argv[index], "RR",
					    strlen("RR"))) {
					cmd->op.transfer_type = RDMA_READ;
				} else if (0 ==
					   strncmp(my_argv[index], "RW",
						   strlen("RW"))) {
					cmd->op.transfer_type = RDMA_WRITE;
				} else {
					DT_Mdep_printf
					    ("OP type must be <RR/RW/SR>\n");
					return (false);
				}
				break;
			}
		case 1:
			{
				cmd->op.seg_size = atoi(my_argv[index]);
				break;
			}
		case 2:
			{
				cmd->op.num_segs = atoi(my_argv[index]);
				break;
			}
		default:
			{
				DT_Mdep_printf("Too many OP args\n");
				return (false);
			}
		}
	}

	return (true);
}

static bool DT_Performance_Cmd_Validate(Performance_Cmd_t * cmd)
{
	if ('\0' == cmd->server_name[0]) {
		DT_Mdep_printf
		    ("Must specify server_name in command line or scriptfile\n");
		return (false);
	}

	if ('\0' == cmd->dapl_name[0]) {
		DT_Mdep_printf
		    ("Must specify device_name in command line or scriptfile\n");
		return (false);
	}

	if (0 == cmd->pipeline_len) {
		DT_Mdep_printf("Pipeline size must not be 0\n");
		return (false);
	}

	if (cmd->debug) {
		DT_Performance_Cmd_Print(cmd);
	}

	return true;
}

bool
DT_Performance_Cmd_Parse(Performance_Cmd_t * cmd,
			 int my_argc, char **my_argv, mygetopt_t * opts)
{
	int c;
	unsigned int len;

	for (;;) {
		c = DT_mygetopt_r(my_argc, my_argv, "D:dm:i:p:R:s:", opts);

		if (EOF == c) {
			break;
		}

		switch (c) {
		case 'D':	/* device name */
			{
				strncpy(cmd->dapl_name, opts->optarg, NAME_SZ);
				break;
			}
		case 'd':	/* print debug messages */
			{
				DT_dapltest_debug++;
				cmd->debug = true;
				break;
			}
		case 'm':	/* mode */
			{
				if (!strncmp(opts->optarg, "b", strlen("b"))) {
					cmd->mode = BLOCKING_MODE;
				} else
				    if (!strncmp
					(opts->optarg, "p", strlen("p"))) {
					cmd->mode = POLLING_MODE;
				} else {
					DT_Mdep_printf
					    ("Syntax Error -m <mode> option\n");
					DT_Performance_Cmd_Usage();
					return (false);
				}

				break;
			}
		case 'i':	/* num iterations */
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -i <iterations> option\n");
					DT_Performance_Cmd_Usage();
					return (false);
				}
				cmd->num_iterations = atol(opts->optarg);
				break;
			}
		case 'p':	/* pipline size */
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -p <piplein> option\n");
					DT_Performance_Cmd_Usage();
					return (false);
				}
				cmd->pipeline_len = atol(opts->optarg);
				break;
			}
		case 'R':	/* Service Reliability Level */
			{
				cmd->qos = DT_ParseQoS(opts->optarg);
				break;
			}
		case 's':	/* server name */
			{
				if ((opts->optarg == 0) ||
				    strlen(opts->optarg) == 0 ||
				    *opts->optarg == '-') {
					DT_Mdep_printf
					    ("must specify server name\n");
					DT_Performance_Cmd_Usage();
					return (false);
				}

				strncpy(cmd->server_name, opts->optarg,
					NAME_SZ);
				break;
			}
		default:
			{
				DT_Mdep_printf
				    ("Invalid Performance Test Parameter: %c\n",
				     c);
				DT_Performance_Cmd_Usage();
				return (false);
			}
		}
	}

	/*
	 * now parse the op
	 */
	if (!DT_Performance_Cmd_Parse_Op(cmd, opts->optind, my_argc, my_argv)) {
		DT_Performance_Cmd_Usage();
		return (false);
	}

	if (!DT_Performance_Cmd_Validate(cmd)) {
		DT_Performance_Cmd_Usage();
		return (false);
	}

	return (true);
}

bool DT_Performance_Cmd_Init(Performance_Cmd_t * cmd)
{
	memset(cmd, 0, sizeof(Performance_Cmd_t));
	cmd->dapltest_version = DAPLTEST_VERSION;
	cmd->client_is_little_endian = DT_local_is_little_endian;
	cmd->qos = DAT_QOS_BEST_EFFORT;
	cmd->debug = false;
	cmd->num_iterations = 1000;
	cmd->pipeline_len = ~0;

	cmd->op.transfer_type = RDMA_WRITE;
	cmd->op.seg_size = 4096;
	cmd->op.num_segs = 1;

	if (!DT_Mdep_GetDefaultDeviceName(cmd->dapl_name)) {
		DT_Mdep_printf("can't get default device name\n");
		return (false);
	}

	return (true);
}

void DT_Performance_Cmd_Print(Performance_Cmd_t * cmd)
{
	DT_Mdep_printf("-------------------------------------\n");
	DT_Mdep_printf("PerfCmd.server_name              : %s\n",
		       cmd->server_name);
	DT_Mdep_printf("PerfCmd.dapl_name                : %s\n",
		       cmd->dapl_name);
	DT_Mdep_printf("PerfCmd.mode                     : %s\n",
		       (cmd->mode == BLOCKING_MODE) ? "BLOCKING" : "POLLING");
	DT_Mdep_printf("PerfCmd.num_iterations           : %d\n",
		       cmd->num_iterations);
	DT_Mdep_printf("PerfCmd.pipeline_len             : %d\n",
		       cmd->pipeline_len);
	DT_Mdep_printf("PerfCmd.op.transfer_type         : %s\n",
		       cmd->op.transfer_type == RDMA_READ ? "RDMA_READ" :
		       cmd->op.transfer_type == RDMA_WRITE ? "RDMA_WRITE" :
		       "SEND_RECV");
	DT_Mdep_printf("PerfCmd.op.num_segs              : %d\n",
		       cmd->op.num_segs);
	DT_Mdep_printf("PerfCmd.op.seg_size              : %d\n",
		       cmd->op.seg_size);
}
