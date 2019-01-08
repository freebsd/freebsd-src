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

//---------------------------------------------------------------------------
void DT_FFT_Cmd_Init(FFT_Cmd_t * cmd)
{
	int i;
	memset((void *)cmd, 0, sizeof(FFT_Cmd_t));
	cmd->fft_type = NONE;
	cmd->device_name[0] = '\0';
	cmd->server_name[0] = '\0';
	for (i = 0; i < MAXCASES; i++) {
		cmd->cases_flag[i] = false;
	}
	cmd->size = 0;
	cmd->num_iter = 1000;
	cmd->num_threads = 10;
	cmd->num_vis = 500;
	cmd->ReliabilityLevel = DAT_QOS_BEST_EFFORT;
}

//------------------------------------------------------------------------------
bool DT_FFT_Cmd_Parse(FFT_Cmd_t * cmd,
		      int my_argc, char **my_argv, mygetopt_t * opts)
{
	int c;
	int i, caseNum;
	unsigned int len;

	for (;;) {
		c = DT_mygetopt_r(my_argc, my_argv, "D:f:s:i:t:v:R:", opts);
		if (c == EOF) {
			break;
		}
		switch (c) {
		case 'D':	//device name
			{
				strcpy(cmd->device_name, opts->optarg);
				break;
			}
		case 's':	//server name
			{
				strcpy(cmd->server_name, opts->optarg);
				break;
			}
		case 'i':	// num iterations
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -i<iterations> option\n");
					DT_FFT_Cmd_Usage();
					return (false);
				}
				cmd->num_iter = atoi(opts->optarg);
				break;
			}
		case 't':	// num threads
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -t<num_threads> option\n");
					DT_FFT_Cmd_Usage();
					return (false);
				}
				cmd->num_threads = atoi(opts->optarg);
				break;
			}
		case 'v':	// num vis
			{
				len = strspn(opts->optarg, "0123456789");
				if (len == 0 || len != strlen(opts->optarg)) {
					DT_Mdep_printf
					    ("Syntax Error -v<num_vis> option\n");
					DT_FFT_Cmd_Usage();
					return (false);
				}
				cmd->num_vis = atoi(opts->optarg);
				break;
			}
		case 'f':	//function feature
			{
				if (strcmp(opts->optarg, "hwconn") == 0) {
					cmd->fft_type = HWCONN;
					cmd->size = 4;	//4 cases for hwconn
					break;
				}
#if 0				// not yet implemented
				else if (strcmp(opts->optarg, "cqmgt") == 0) {
					cmd->fft_type = CQMGT;
					cmd->size = 10;	//10 cases for cqmgt
					break;
				}
#endif
				else if (strcmp(opts->optarg, "endpoint") == 0) {
					cmd->fft_type = ENDPOINT;
					cmd->size = 3;	//13 cases for endpoint
					break;
				} else if (strcmp(opts->optarg, "pz") == 0) {
					cmd->fft_type = PTAGMGT;
					cmd->size = 3;	//10 cases for Ptagmgt
					break;
				} else if (strcmp(opts->optarg, "mem") == 0) {
					cmd->fft_type = MEMMGT;
					cmd->size = 5;	//12 cases for Memmgt
					break;
				} else if (strcmp(opts->optarg, "connmgt") == 0) {
					cmd->fft_type = CONNMGT;
					cmd->size = 2;	//16 cases for connmgt
					break;
				}
#if 0				// not yet implemented
				else if (strcmp(opts->optarg, "connmgt_client")
					 == 0) {
					cmd->fft_type = CONNMGT_CLIENT;
					cmd->size = 16;	//16 cases for connmgt_client
					break;
				}
#endif
				else if (strcmp(opts->optarg, "queryinfo") == 0) {
					cmd->fft_type = QUERYINFO;
#ifndef __KDAPLTEST__
					cmd->size = 18;	//18 cases for UDAPL queryinfo
#else
					cmd->size = 16;	//16 cases for KDAPL queryinfo
#endif
					break;
				}
#if 0				// not yet implemented
				else if (strcmp(opts->optarg, "ns") == 0) {
					cmd->fft_type = NS;
					cmd->size = 10;	//10 cases for ns
					break;
				} else if (strcmp(opts->optarg, "errhand") == 0) {
					cmd->fft_type = ERRHAND;
					cmd->size = 2;	//2 cases for errhand
					break;
				} else if (strcmp(opts->optarg, "unsupp") == 0) {
					cmd->fft_type = UNSUPP;
					cmd->size = 2;	//2 cases for unsupp
					break;
				} else if (strcmp(opts->optarg, "stress") == 0) {
					cmd->fft_type = STRESS;
					cmd->size = 6;	//6 cases for stress
					break;
				} else if (strcmp(opts->optarg, "stress_client")
					   == 0) {
					cmd->fft_type = STRESS_CLIENT;
					cmd->size = 6;	//6 cases for stress_client
					break;
				}
#endif
				else {
					DT_Mdep_printf
					    ("don't know this function feature: %s\n",
					     opts->optarg);
					DT_FFT_Cmd_Usage();
					return (false);
				}
			}
		case 'R':	// Service Reliability Level
			{
				cmd->ReliabilityLevel =
				    DT_ParseQoS(opts->optarg);
				if (0 == cmd->ReliabilityLevel) {
					DT_Mdep_printf
					    ("Invalid FFT Test Parameter: %c\n",
					     c);
					DT_FFT_Cmd_Usage();
					return (false);
				}
				break;
			}

		case '?':
		default:
			{
				DT_Mdep_printf
				    ("Invalid FFT Test Parameter: %c\n", c);
				DT_FFT_Cmd_Usage();
				return (false);
			}
		}
	}
	if (cmd->device_name[0] == '\0') {
		if (!DT_Mdep_GetDefaultDeviceName(cmd->device_name)) {
			DT_Mdep_printf("can't get default device name\n");
			DT_FFT_Cmd_Usage();
			return (false);
		}
	}

	if (cmd->fft_type == NONE) {
		DT_Mdep_printf
		    ("must define the function feature with -f to test\n");
		DT_FFT_Cmd_Usage();
		return (false);
	}
	if (cmd->server_name[0] == '\0' &&
	    (cmd->fft_type == CONNMGT_CLIENT || cmd->fft_type == DATAXFER_CLIENT
	     || cmd->fft_type == UNSUPP || cmd->fft_type == STRESS_CLIENT)) {
		DT_Mdep_printf("must define the server name with -s option\n");
		DT_FFT_Cmd_Usage();
		return (false);
	}

	if (cmd->server_name[0] == '\0' && cmd->fft_type == NS) {
		DT_Mdep_printf("\
	    Must specify host name or host IP address with -s option to be tested\n");
		DT_FFT_Cmd_Usage();
		return (false);
	}
	//now parse the test cases
	if (opts->optind == my_argc)	//default: test all cases
	{
		for (i = 0; i < cmd->size; i++) {
			cmd->cases_flag[i] = true;
		}
		return true;
	}
	//test specified cases
	i = opts->optind;
	while (i < my_argc) {
		if (strlen(my_argv[i]) < 5
		    || strncmp(my_argv[i], "case", 4) != 0) {
			DT_Mdep_printf("test cases format is not correct: %s\n",
				       my_argv[i]);
			DT_FFT_Cmd_Usage();
			return (false);
		}
		len = strspn(my_argv[i] + 4, "0123456789");
		if (len == 0 || len != strlen(my_argv[i] + 4)) {
			DT_Mdep_printf("must specify case number: %s\n",
				       my_argv[i]);
			DT_FFT_Cmd_Usage();
			return (false);
		}
		caseNum = atoi(my_argv[i] + 4);
		if (caseNum < 0 || caseNum >= cmd->size) {
			DT_Mdep_printf
			    ("test case number must be within range : 0 -- %d\n",
			     cmd->size - 1);
			DT_FFT_Cmd_Usage();
			return (false);
		}
		cmd->cases_flag[caseNum] = true;
		i++;
	}
	return (true);
}

//--------------------------------------------------------------
void DT_FFT_Cmd_Usage(void)
{
	char usage[] = {
		"dapltest -T F [-D <dev_name>] -f <funcfeature> [-i <iter_num>] \n"
		    "[-t <num_threads>] [-v <num_vis>] [-s <server_name>] [case0] [case1] [...]\n"
		    "USAGE:        [-D <device Name>]\n"
		    "USAGE:         -f <func_feature>\n"
		    "USAGE:             hwconn\n"
		    "USAGE:             endpoint\n"
		    "USAGE:             pz\n"
		    "USAGE:             mem\n"
		    "USAGE:             connmgt\n"
		    "USAGE:             queryinfo\n"
#if 0				// not yet implemented
		    "USAGE:             connmgt_client       (not yet implemented)\n"
		    "USAGE:             cqmgt                (not yet implemented)\n"
		    "USAGE:             ns                   (not yet implemented)\n"
		    "USAGE:             errhand              (not yet implemented)\n"
		    "USAGE:             unsupp               (not yet implemented)\n"
		    "USAGE:             stress               (not yet implemented)\n"
		    "USAGE:             stress_client        (not yet implemented)\n"
#endif
		"USAGE:         -i <iter_num>: itreration time for stress test\n"
		    "USAGE:         -t <num_threads>: number of threads for stress test\n"
		    "USAGE:         -v <num_vis>: number of vis for stress test\n"
		    "USAGE:         -s <server_name>\n"
		    "USAGE:             server host name or ip address\n"
		    "USAGE:        [-R <service reliability>]\n"
		    "USAGE:            (BE == QOS_BEST_EFFORT - Default )\n"
		    "USAGE:            (HT == QOS_HIGH_THROUGHPUT))\n"
		    "USAGE:            (LL == QOS_LOW_LATENCY)\n"
		    "USAGE:            (EC == QOS_ECONOMY)\n"
		    "USAGE:            (PM == QOS_PREMIUM)\n"
		    "NOTE: iter_num is just for stress_client test, default 100000\n"
		    "NOTE: Server_name must be specified for connmgt_client, \n"
		    "      NS and unsupp function feature.\n"
		    "NOTE: if test cases are not specified, test all cases in that function\n"
		    "      feature. else just test the specified cases\n"
	};

	DT_Mdep_printf("USAGE: -------FFT TEST------------\n");
	DT_Mdep_printf("%s\n", usage);
}
