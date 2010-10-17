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

static bool DT_Transaction_Cmd_Parse_Op(Transaction_Cmd_t * cmd, char *arg)
{
	char *c_ptr;
	int op;
	if (cmd->num_ops >= MAX_OPS) {
		DT_Mdep_printf("Client: Too Many Ops - Max %d\n", MAX_OPS);
		goto error_return;
	}
	op = cmd->num_ops;
	cmd->num_ops++;

	/* set some defaults */
	cmd->op[op].seg_size = 4096;
	cmd->op[op].num_segs = 1;
	cmd->op[op].reap_send_on_recv = false;

	/*
	 * packet format: <server/client> <RR/RW/SR> [seg_size] [num_segs]
	 */
	c_ptr = strtok(arg, " \t");
	if (!c_ptr) {
		DT_Mdep_printf("OP first arg must <server/client>\n");
		goto error_return;
	}
	/* first token is <who>: */
	if (strcmp(c_ptr, "server") == 0) {
		cmd->op[op].server_initiated = true;
	} else {
		if (strcmp(c_ptr, "client") == 0) {
			cmd->op[op].server_initiated = false;
		} else {
			DT_Mdep_printf("OP first arg must <server/client>\n");
			goto error_return;
		}
	}

	c_ptr = strtok(0, " \t");
	if (!c_ptr) {
		DT_Mdep_printf("OP Second arg must be <RR/RW/SR>\n");
		goto error_return;
	}
	/* second token is <transfer_type>: */
	if (strcmp(c_ptr, "RR") == 0) {
		cmd->op[op].transfer_type = RDMA_READ;
	} else {
		if (strcmp(c_ptr, "RW") == 0) {
			cmd->op[op].transfer_type = RDMA_WRITE;
		} else {
			if (strcmp(c_ptr, "SR") == 0) {
				cmd->op[op].transfer_type = SEND_RECV;
			} else {
				DT_Mdep_printf
				    ("OP Second arg must be <RR/RW/SR>\n");
				goto error_return;
			}
		}
	}
	/*
	 * there may or may not be additional parameters... [seg_size] [num_segs]
	 * [-f]
	 */
	c_ptr = strtok(0, " \t");
	if (c_ptr && strspn(c_ptr, "0123456789") != 0) {
		cmd->op[op].seg_size = atoi(c_ptr);
		c_ptr = strtok(0, " \t");
	}
	if (c_ptr && strspn(c_ptr, "0123456789") != 0) {
		cmd->op[op].num_segs = atoi(c_ptr);
		c_ptr = strtok(0, " \t");
	}
	if (c_ptr && strcmp(c_ptr, "-f") == 0) {
		cmd->op[op].reap_send_on_recv = true;
		if (cmd->op[op].transfer_type != SEND_RECV) {
			DT_Mdep_printf("OP: -f only valid on SEND_RECV\n");
			goto error_return;
		}
		c_ptr = strtok(0, " \t");
	}
	if (c_ptr) {
		DT_Mdep_printf("OP too many args \n");
		goto error_return;
	}
	return true;

      error_return:
	return false;
}

static bool DT_Transaction_Cmd_Validate(Transaction_Cmd_t * cmd)
{
	unsigned int i;
	bool has_server_send;
	bool has_client_send;
	unsigned int reap_count;
	has_server_send = false;
	has_client_send = false;
	reap_count = 0;

	if (cmd->server_name[0] == '\0') {
		DT_Mdep_printf
		    ("Must specify server_name in command line or scriptfile\n");
		return (false);
	}
	for (i = 0; i < cmd->num_ops; i++) {
		switch (cmd->op[i].transfer_type) {
		case SEND_RECV:
			{
				if (cmd->op[i].server_initiated) {
					has_server_send = true;
				} else {
					has_client_send = true;
				}
				if (cmd->op[i].reap_send_on_recv) {
					if (!cmd->op[i].server_initiated) {
						/* client */
						reap_count++;
					} else {
						/* server */
						if (reap_count > 0) {
							reap_count--;
						} else {
							DT_Mdep_printf
							    ("OP: Unbalanced -f options\n");
							return false;
						}
					}
				}
				break;
			}
		case RDMA_READ:
			{
				break;
			}
		case RDMA_WRITE:
			{
				break;
			}
		}
	}

	if (!has_server_send || !has_client_send) {
		DT_Mdep_printf("Error: Transaction test requires \n");
		DT_Mdep_printf
		    ("Error: At least one server SR and one client SR Operation\n");
		return false;
	}
	if (reap_count != 0) {
		DT_Mdep_printf("OP: Unbalanced -f options\n");
		return false;
	}
	if (cmd->debug) {
		DT_Transaction_Cmd_Print(cmd);
	}
	return true;
}

static void DT_Transaction_Cmd_Usage(void)
{
	DT_Mdep_printf("USAGE: ---- TRANSACTION TEST ----\n");
	DT_Mdep_printf("USAGE:     dapltest -T T\n");
	DT_Mdep_printf("USAGE:              -s <server Name>\n");
	DT_Mdep_printf("USAGE:              [-D <device Name>]\n");
	DT_Mdep_printf("USAGE:              [-d] : debug (zero)\n");
	DT_Mdep_printf
	    ("USAGE:              [-i <num iterations>]     : (1, 000)\n");
	DT_Mdep_printf("USAGE:              [-t <num threads>]        : (1)\n");
	DT_Mdep_printf("USAGE:              [-w <num EPs per thread>] : (1)\n");
	DT_Mdep_printf("USAGE:              [-V ] : Validate data: (false)\n");
	DT_Mdep_printf
	    ("USAGE:              [-P ] : DTO Completion Polling: (false)\n");
	DT_Mdep_printf("USAGE:              [-r ] : Use RSPs: (false)\n");
	DT_Mdep_printf("USAGE:              [-R <service reliability>]\n");
	DT_Mdep_printf
	    ("USAGE:                  (BE == QOS_BEST_EFFORT - Default)\n");
	DT_Mdep_printf("USAGE:                  (HT == QOS_HIGH_THROUGHPUT)\n");
	DT_Mdep_printf("USAGE:                  (LL == QOS_LOW_LATENCY)\n");
	DT_Mdep_printf("USAGE:                  (EC == QOS_ECONOMY)\n");
	DT_Mdep_printf("USAGE:                  (PM == QOS_PREMIUM)\n");
	DT_Mdep_printf("USAGE:           <OP [OP...]\n");
	DT_Mdep_printf("USAGE:\n");
	DT_Mdep_printf("USAGE: Each OP consists of:\n");
	DT_Mdep_printf("USAGE:     <who>       : \"server\"/\"client\"\n");
	DT_Mdep_printf("USAGE:     <transfer_type> : \"SR\" (SEND/RECV)\n");
	DT_Mdep_printf("USAGE:                     : \"RR\" (RDMA READ)\n");
	DT_Mdep_printf("USAGE:                     : \"RW\" (RDMA WRITE)\n");
	DT_Mdep_printf("USAGE:     [seg_size [num_segs] ]      : (4096, 1)\n");
	DT_Mdep_printf
	    ("USAGE:     [-f]                 : Reap sends on recv\n");
	DT_Mdep_printf("USAGE:\n");
	DT_Mdep_printf("NOTE: -f is only allowed on \"SR\" OPs\n");
	DT_Mdep_printf
	    ("NOTE: -f must appear in pairs (one client, one server)\n");
	DT_Mdep_printf
	    ("NOTE: At least one server SR and one client SR OP are required\n");
	DT_Mdep_printf
	    ("NOTE: and use of -V results in the use of three extra OPs\n");
}

void DT_Transaction_Cmd_Init(Transaction_Cmd_t * cmd)
{
	memset((void *)cmd, 0, sizeof(Transaction_Cmd_t));
	cmd->dapltest_version = DAPLTEST_VERSION;
	cmd->client_is_little_endian = DT_local_is_little_endian;
	cmd->num_iterations = 1000;
	cmd->num_threads = 1;
	cmd->eps_per_thread = 1;
	cmd->debug = false;
	cmd->validate = false;
	cmd->ReliabilityLevel = DAT_QOS_BEST_EFFORT;
}

bool
DT_Transaction_Cmd_Parse(Transaction_Cmd_t * cmd,
			 int my_argc, char **my_argv, mygetopt_t * opts)
{
	int c;
	unsigned int len;
	int i;
	char op[100];
	for (;;) {
		c = DT_mygetopt_r(my_argc, my_argv, "rQVPdw:s:D:i:t:v:R:",
				  opts);
		if (c == EOF) {
			break;
		}
		switch (c) {
		case 's':	/* server name */
			{
				if ((opts->optarg == 0)
				    || strlen(opts->optarg) == 0
				    || *opts->optarg == '-') {
					DT_Mdep_printf
					    ("must specify server name\n");
					DT_Transaction_Cmd_Usage();
					return (false);
				}
				strcpy(cmd->server_name, opts->optarg);
				break;
			}
		case 'D':	/* device name */
			{
				strcpy(cmd->dapl_name, opts->optarg);
				break;
			}

		case 'i':	/* num iterations */
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -i<iterations> option\n");
					DT_Transaction_Cmd_Usage();
					return (false);
				}
				cmd->num_iterations = atol(opts->optarg);

				break;
			}
		case 't':	/* num threads */
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -t<num threads> option\n");
					DT_Transaction_Cmd_Usage();
					return (false);
				}
				cmd->num_threads = atol(opts->optarg);
				break;
			}
		case 'w':	/* num EPs per thread */
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -w<EPs/thread> option\n");
					DT_Transaction_Cmd_Usage();
					return (false);
				}
				cmd->eps_per_thread = atol(opts->optarg);
				break;
			}
		case 'd':	/* print debug messages */
			{
				DT_dapltest_debug++;
				cmd->debug = true;
				break;
			}
		case 'r':	/* use RSP instead of PSP */
			{
				cmd->use_rsp = true;
				break;
			}
		case 'V':	/* validate data being sent/received */
			{
				cmd->validate = true;
				break;
			}
		case 'P':	/* use completion polling */
			{
				cmd->poll = true;
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
				DT_Mdep_printf
				    ("Invalid Transaction Test Parameter: %c\n",
				     c);
				DT_Transaction_Cmd_Usage();
				return (false);
			}
		}
	}
	if (cmd->dapl_name[0] == '\0') {
		if (!DT_Mdep_GetDefaultDeviceName(cmd->dapl_name)) {
			DT_Mdep_printf("can't get default device name\n");
			DT_Transaction_Cmd_Usage();
			return (false);
		}
	}
	/*
	 * now parse the transaction ops this is ugly, but it's easier to gather
	 * each transaction into a single string
	 */
	for (i = opts->optind; i < my_argc; i++) {
		strcpy(&op[0], my_argv[i]);
		while (i < my_argc - 1) {
			i++;
			if ((strncmp(my_argv[i], "client", 6) == 0) ||
			    strncmp(my_argv[i], "server", 6) == 0) {
				i--;
				break;
			}
			strcat(op, " ");
			strcat(op, my_argv[i]);
		}
		if (!DT_Transaction_Cmd_Parse_Op(cmd, op)) {
			DT_Transaction_Cmd_Usage();
			return (false);
		}
	}

	/*
	 * If we're going to validate the data, we append 3 OPs that
	 * serve as barriers so that both the client and server can
	 * validate their entire set of recv transactions without
	 * interference.
	 *
	 * The first op appended serves to notify the client that the
	 * server is at the rendezvous and will transfer nothing else,
	 * so the client can validate all recv buffers.  The second op
	 * notifies the server that the client is quiescent, so the
	 * server can safely validate its recv buffers.  The final op
	 * tells the client that the server is done, and both can
	 * proceed with the next iteration.
	 */
	if (cmd->validate) {
		DT_Mdep_printf
		    ("NOTE: Adding OP \"server SR\" - for validation\n");
		memcpy(op, "server SR", strlen("server SR") + 1);
		DT_Transaction_Cmd_Parse_Op(cmd, op);

		DT_Mdep_printf
		    ("NOTE: Adding OP \"client SR\" - for validation\n");
		memcpy(op, "client SR", strlen("client SR") + 1);
		DT_Transaction_Cmd_Parse_Op(cmd, op);

		DT_Mdep_printf
		    ("NOTE: Adding OP \"server SR\" - for validation\n");
		memcpy(op, "server SR", strlen("server SR") + 1);
		DT_Transaction_Cmd_Parse_Op(cmd, op);
	}
	if (!DT_Transaction_Cmd_Validate(cmd)) {
		DT_Transaction_Cmd_Usage();
		return (false);
	}
	return (true);
}

void DT_Transaction_Cmd_Print(Transaction_Cmd_t * cmd)
{
	unsigned int i;
	DT_Mdep_printf("-------------------------------------\n");
	DT_Mdep_printf("TransCmd.server_name              : %s\n",
		       cmd->server_name);
	DT_Mdep_printf("TransCmd.num_iterations           : %d\n",
		       cmd->num_iterations);
	DT_Mdep_printf("TransCmd.num_threads              : %d\n",
		       cmd->num_threads);
	DT_Mdep_printf("TransCmd.eps_per_thread           : %d\n",
		       cmd->eps_per_thread);
	DT_Mdep_printf("TransCmd.validate                 : %d\n",
		       cmd->validate);
	DT_Mdep_printf("TransCmd.dapl_name                : %s\n",
		       cmd->dapl_name);
	DT_Mdep_printf("TransCmd.num_ops                  : %d\n",
		       cmd->num_ops);

	for (i = 0; i < cmd->num_ops; i++) {
		DT_Mdep_printf("TransCmd.op[%d].transfer_type      : %s %s\n",
			       i,
			       cmd->op[i].transfer_type == 0 ? "RDMA_READ" :
			       cmd->op[i].transfer_type == 1 ? "RDMA_WRITE" :
			       "SEND_RECV",
			       cmd->op[i].
			       server_initiated ? " (server)" : " (client)");
		DT_Mdep_printf("TransCmd.op[%d].seg_size           : %d\n", i,
			       cmd->op[i].seg_size);
		DT_Mdep_printf("TransCmd.op[%d].num_segs           : %d\n", i,
			       cmd->op[i].num_segs);
		DT_Mdep_printf("TransCmd.op[%d].reap_send_on_recv  : %d\n", i,
			       cmd->op[i].reap_send_on_recv);
	}
}
