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

#define MAX_ARGC    500
#define MAX_ARG_LEN 100

/* Parse command line arguments */
bool DT_Params_Parse(int argc, char *argv[], Params_t * params_ptr)
{
	Server_Cmd_t *Server_Cmd;
	Transaction_Cmd_t *Transaction_Cmd;
	Quit_Cmd_t *Quit_Cmd;
	Limit_Cmd_t *Limit_Cmd;
	Performance_Cmd_t *Performance_Cmd;
	FFT_Cmd_t *FFT_Cmd;

	char *filename;
	FILE *fd;
	mygetopt_t opts;
	int c;
	char *cp;
	char *sp;
	char line[256];
	char *my_argv[MAX_ARGC];
	int my_argc;
	int i;
	DT_mygetopt_init(&opts);
	opts.opterr = 0;	/* turn off automatical error handler */

	fd = 0;
	my_argc = 0;
	for (i = 0; i < MAX_ARGC; i++) {
		my_argv[i] = NULL;
	}

	/* dapltest with no arguments means run as a server with default values */
	if (argc == 1) {
		params_ptr->test_type = SERVER_TEST;
		params_ptr->ReliabilityLevel = DAT_QOS_BEST_EFFORT;
		Server_Cmd = &params_ptr->u.Server_Cmd;
		DT_Server_Cmd_Init(Server_Cmd);
		if (!DT_Mdep_GetDefaultDeviceName(Server_Cmd->dapl_name)) {
			DT_Mdep_printf("can't get default device name\n");
			return false;
		}
		return true;
	}
	/* check for a script file */
	if (strncmp(argv[1], "-f", 2) == 0) {
		if (argc == 2) {	/* dapltest -fdata  */
			filename = argv[1] + 2;
		} else {
			if (argc == 3 && strcmp(argv[1], "-f") == 0) {	/* dapltest -f data */
				filename = argv[2];
			} else {
				DT_Mdep_printf
				    ("-f <script_file> allows no additional options\n");
				goto main_usage;
			}
		}

		if (!filename || strlen(filename) == 0) {
			DT_Mdep_printf
			    ("Missing <script_file> with -f option\n");
			goto main_usage;
		}
		/* read the script file and create a fake argc, argv */
		fd = fopen(filename, "r");
		if (fd == 0) {
			DT_Mdep_printf("Cannot open script file: %s\n",
				       filename);
			goto main_usage;
		}
		my_argc = 1;
		my_argv[0] = DT_Mdep_Malloc(MAX_ARG_LEN);
		if (!my_argv[0]) {
			DT_Mdep_printf("No Memory\n");
			goto error_return;
		}
		strcpy(my_argv[0], argv[0]);
		while (fgets(&line[0], 256, fd)) {
			sp = &line[0];
			for (;;) {
				cp = strtok(sp, " \t\n");
				sp = 0;	/* so can continue to parse this string */
				if (!cp) {	/* no more token found */
					break;
				}
				if (*cp == '#') {	/* Comment; go to next line.  */
					break;
				}
				my_argv[my_argc] = DT_Mdep_Malloc(MAX_ARG_LEN);
				if (!my_argv[my_argc]) {
					DT_Mdep_printf("No Memory\n");
					goto error_return;
				}
				strcpy(my_argv[my_argc], cp);
				my_argc++;
			}
		}
	} else {
		my_argc = argc;
		for (i = 0; i < argc; i++) {
			my_argv[i] = argv[i];
		}
	}

#if 0
	for (i = 0; i < my_argc; i++) {
		DT_Mdep_printf("ARG %s\n", my_argv[i]);
	}
	exit(1);
#endif

	/* get test type - which must be the first arg */
	c = DT_mygetopt_r(my_argc, my_argv, "T:", &opts);
	if (c != 'T') {
		DT_Mdep_printf("Must Specify Test (-T) option first\n");
		goto main_usage;
	}
	if ((opts.optarg == 0) || strlen(opts.optarg) == 0
	    || *opts.optarg == '-') {
		DT_Mdep_printf("Must specify test type\n");
		goto main_usage;
	}
	switch (*opts.optarg) {
	case 'S':		/* Server Test */
		{
			params_ptr->test_type = SERVER_TEST;
			Server_Cmd = &params_ptr->u.Server_Cmd;
			DT_Server_Cmd_Init(Server_Cmd);
			if (!DT_Server_Cmd_Parse(Server_Cmd,
						 my_argc, my_argv, &opts)) {
				goto error_return;
			}
			params_ptr->ReliabilityLevel =
			    Server_Cmd->ReliabilityLevel;
			params_ptr->debug = Server_Cmd->debug;
			break;
		}
	case 'T':		/* Transaction Test */
		{
			params_ptr->test_type = TRANSACTION_TEST;
			Transaction_Cmd = &params_ptr->u.Transaction_Cmd;
			DT_Transaction_Cmd_Init(Transaction_Cmd);
			if (!DT_Transaction_Cmd_Parse(Transaction_Cmd,
						      my_argc, my_argv, &opts))
			{
				goto error_return;
			}
			params_ptr->ReliabilityLevel =
			    Transaction_Cmd->ReliabilityLevel;
			params_ptr->debug = Transaction_Cmd->debug;
			DT_NetAddrLookupHostAddress(&params_ptr->server_netaddr,
						    Transaction_Cmd->
						    server_name);
			break;
		}
	case 'Q':		/* Quit server Test */
		{
			params_ptr->test_type = QUIT_TEST;
			Quit_Cmd = &params_ptr->u.Quit_Cmd;
			DT_Quit_Cmd_Init(Quit_Cmd);
			if (!DT_Quit_Cmd_Parse(Quit_Cmd,
					       my_argc, my_argv, &opts)) {
				goto error_return;
			}
			params_ptr->ReliabilityLevel =
			    Quit_Cmd->ReliabilityLevel;
			params_ptr->debug = Quit_Cmd->debug;
			DT_NetAddrLookupHostAddress(&params_ptr->server_netaddr,
						    Quit_Cmd->server_name);
			break;
		}
	case 'L':		/* Limit Test */
		{
			params_ptr->test_type = LIMIT_TEST;
			Limit_Cmd = &params_ptr->u.Limit_Cmd;
			DT_Limit_Cmd_Init(Limit_Cmd);
			if (!DT_Limit_Cmd_Parse
			    (Limit_Cmd, my_argc, my_argv, &opts)) {
				goto error_return;
			}
			params_ptr->ReliabilityLevel =
			    Limit_Cmd->ReliabilityLevel;
			params_ptr->debug = Limit_Cmd->debug;
			break;
		}
	case 'P':		/* Performance Test */
		{
			params_ptr->test_type = PERFORMANCE_TEST;
			Performance_Cmd = &params_ptr->u.Performance_Cmd;

			if (!DT_Performance_Cmd_Init(Performance_Cmd)) {
				goto error_return;
			}

			if (!DT_Performance_Cmd_Parse(Performance_Cmd,
						      my_argc, my_argv, &opts))
			{
				goto error_return;
			}

			params_ptr->ReliabilityLevel = Performance_Cmd->qos;
			params_ptr->debug = Performance_Cmd->debug;
			DT_NetAddrLookupHostAddress(&params_ptr->server_netaddr,
						    Performance_Cmd->
						    server_name);
			break;
		}
	case 'F':
		{
			params_ptr->test_type = FFT_TEST;
			FFT_Cmd = &params_ptr->u.FFT_Cmd;
			DT_FFT_Cmd_Init(FFT_Cmd);
			if (!DT_FFT_Cmd_Parse(FFT_Cmd, my_argc, my_argv, &opts)) {
				goto error_return;
			}
			params_ptr->ReliabilityLevel =
			    FFT_Cmd->ReliabilityLevel;
			DT_NetAddrLookupHostAddress(&params_ptr->server_netaddr,
						    FFT_Cmd->server_name);
			params_ptr->debug = false;
			break;
		}

	default:
		{
			DT_Mdep_printf("Invalid Test Type\n");
			goto main_usage;
		}
	}

	if (fd) {
		for (i = 0; i < my_argc; i++) {
			DT_Mdep_Free(my_argv[i]);
		}
		fclose(fd);
	}
	return true;

      main_usage:
	Dapltest_Main_Usage();
      error_return:
	if (fd) {
		for (i = 0; i < my_argc; i++) {
			DT_Mdep_Free(my_argv[i]);
		}
		fclose(fd);
	}
	return false;
}
