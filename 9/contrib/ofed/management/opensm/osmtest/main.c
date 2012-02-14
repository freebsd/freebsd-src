/*
 * Copyright (c) 2004-2006 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 * 	Command line interface for osmtest.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <complib/cl_debug.h>
#include "osmtest.h"

/********************************************************************
       D E F I N E    G L O B A L    V A R I A B L E S
*********************************************************************/

/*
	This is the global osmtest object.
	One osmtest object is required per subnet.
	Future versions could support multiple subents by
	instantiating more than one osmtest object.
*/
#define GUID_ARRAY_SIZE 64
#define OSMT_DEFAULT_RETRY_COUNT 3
#define OSMT_DEFAULT_TRANS_TIMEOUT_MILLISEC 1000
#define OSMT_DEFAULT_TRAP_WAIT_TIMEOUT_SEC 10
#define INVALID_GUID (0xFFFFFFFFFFFFFFFFULL)

/**********************************************************************
 **********************************************************************/
boolean_t osmt_is_debug(void)
{
#if defined( _DEBUG_ )
	return TRUE;
#else
	return FALSE;
#endif				/* defined( _DEBUG_ ) */
}

/**********************************************************************
 **********************************************************************/
void show_usage(void);

void show_usage()
{
	printf
	    ("\n------- osmtest - Usage and options ----------------------\n");
	printf("Usage:	  osmtest [options]\n");
	printf("Options:\n");
	printf("-f <c|a|v|s|e|f|m|q|t>\n"
	       "--flow <c|a|v|s|e|f|m|q|t>\n"
	       "          This option directs osmtest to run a specific flow:\n"
	       "          FLOW  DESCRIPTION\n"
	       "          c = create an inventory file with all nodes, ports and paths\n"
	       "          a = run all validation tests (expecting an input inventory)\n"
	       "          v = only validate the given inventory file\n"
	       "          s = run service registration, deregistration, and lease test\n"
	       "          e = run event forwarding test\n"
	       "          f = flood the SA with queries according to the stress mode\n"
	       "          m = multicast flow\n"
	       "          q = QoS info: dump VLArb and SLtoVL tables\n"
	       "          t = run trap 64/65 flow (this flow requires running of external tool)\n"
	       "          (default is all flows except QoS)\n\n");

	printf("-w <trap_wait_time>\n"
	       "--wait <trap_wait_time>\n"
	       "          This option specifies the wait time for trap 64/65 in seconds\n"
	       "          It is used only when running -f t - the trap 64/65 flow\n"
	       "          (default to 10 sec)\n\n");
	printf("-d <number>\n"
	       "--debug <number>\n"
	       "          This option specifies a debug option\n"
	       "          These options are not normally needed\n"
	       "          The number following -d selects the debug\n"
	       "          option to enable as follows:\n"
	       "          OPT   Description\n"
	       "          ---    -----------------\n"
	       "          -d0  - Unused.\n"
	       "          -d1  - Do not scan/compare path records.\n"
	       "          -d2  - Force log flushing after each log message.\n"
	       "          Without -d, no debug options are enabled\n\n");
	printf("-m <LID in hex>\n"
	       "--max_lid <LID in hex>\n"
	       "          This option specifies the maximal LID number to be searched\n"
	       "          for during inventory file build (default to 100)\n\n");
	printf("-g <GUID in hex>\n"
	       "--guid <GUID in hex>\n"
	       "          This option specifies the local port GUID value\n"
	       "          with which osmtest should bind.  osmtest may be\n"
	       "          bound to 1 port at a time\n\n");
	printf("-p \n"
	       "--port\n"
	       "          This option displays a menu of possible local port GUID values\n"
	       "          with which osmtest could bind\n\n");
	printf("-h\n"
	       "--help\n" "          Display this usage info then exit\n\n");
	printf("-i <filename>\n"
	       "--inventory <filename>\n"
	       "          This option specifies the name of the inventory file\n"
	       "          Normally, osmtest expects to find an inventory file,\n"
	       "          which osmtest uses to validate real-time information\n"
	       "          received from the SA during testing\n"
	       "          If -i is not specified, osmtest defaults to the file\n"
	       "          'osmtest.dat'\n"
	       "          See -c option for related information\n\n");
	printf("-s\n"
	       "--stress\n"
	       "          This option runs the specified stress test instead\n"
	       "          of the normal test suite\n"
	       "          Stress test options are as follows:\n"
	       "          OPT    Description\n"
	       "          ---    -----------------\n"
	       "          -s1  - Single-MAD response SA queries\n"
	       "          -s2  - Multi-MAD (RMPP) response SA queries\n"
	       "          -s3  - Multi-MAD (RMPP) Path Record SA queries\n"
	       "          Without -s, stress testing is not performed\n\n");
	printf("-M\n"
	       "--Multicast_Mode\n"
	       "          This option specify length of Multicast test:\n"
	       "          OPT    Description\n"
	       "          ---    -----------------\n"
	       "          -M1  - Short Multicast Flow (default) - single mode\n"
	       "          -M2  - Short Multicast Flow - multiple mode\n"
	       "          -M3  - Long Multicast Flow - single mode\n"
	       "          -M4  - Long Multicast Flow - multiple mode\n"
	       " Single mode - Osmtest is tested alone, with no other\n"
	       "   apps that interact with OpenSM MC\n"
	       " Multiple mode - Could be run with other apps using MC with\n"
	       "   OpenSM."
	       " Without -M, default flow testing is performed\n\n");

	printf("-t <milliseconds>\n"
	       "          This option specifies the time in milliseconds\n"
	       "          used for transaction timeouts\n"
	       "          Specifying -t 0 disables timeouts\n"
	       "          Without -t, osmtest defaults to a timeout value of\n"
	       "          1 second\n\n");
	printf("-l\n"
	       "--log_file\n"
	       "          This option defines the log to be the given file\n"
	       "          By default the log goes to stdout\n\n");
	printf("-v\n"
	       "          This option increases the log verbosity level\n"
	       "          The -v option may be specified multiple times\n"
	       "          to further increase the verbosity level\n"
	       "          See the -vf option for more information about.\n"
	       "          log verbosity\n\n");
	printf("-V\n"
	       "          This option sets the maximum verbosity level and\n"
	       "          forces log flushing\n"
	       "          The -V is equivalent to '-vf 0xFF -d 2'\n"
	       "          See the -vf option for more information about.\n"
	       "          log verbosity\n\n");
	printf("-vf <flags>\n"
	       "          This option sets the log verbosity level\n"
	       "          A flags field must follow the -vf option\n"
	       "          A bit set/clear in the flags enables/disables a\n"
	       "          specific log level as follows:\n"
	       "          BIT    LOG LEVEL ENABLED\n"
	       "          ----   -----------------\n"
	       "          0x01 - ERROR (error messages)\n"
	       "          0x02 - INFO (basic messages, low volume)\n"
	       "          0x04 - VERBOSE (interesting stuff, moderate volume)\n"
	       "          0x08 - DEBUG (diagnostic, high volume)\n"
	       "          0x10 - FUNCS (function entry/exit, very high volume)\n"
	       "          0x20 - FRAMES (dumps all SMP and GMP frames)\n"
	       "          0x40 - currently unused\n"
	       "          0x80 - currently unused\n"
	       "          Without -vf, osmtest defaults to ERROR + INFO (0x3)\n"
	       "          Specifying -vf 0 disables all messages\n"
	       "          Specifying -vf 0xFF enables all messages (see -V)\n"
	       "          High verbosity levels may require increasing\n"
	       "          the transaction timeout with the -t option\n\n");
}

/**********************************************************************
 **********************************************************************/
static void print_all_guids(IN osmtest_t * p_osmt);
static void print_all_guids(IN osmtest_t * p_osmt)
{
	ib_api_status_t status;
	uint32_t num_ports = GUID_ARRAY_SIZE;
	ib_port_attr_t attr_array[GUID_ARRAY_SIZE];
	int i;

	/*
	   Call the transport layer for a list of local port
	   GUID values.
	 */
	status =
	    osm_vendor_get_all_port_attr(p_osmt->p_vendor, attr_array,
					 &num_ports);
	if (status != IB_SUCCESS) {
		printf("\nError from osm_vendor_get_all_port_attr (%x)\n",
		       status);
		return;
	}

	printf("\nListing GUIDs:\n");
	for (i = 0; i < num_ports; i++)
		printf("Port %i: 0x%" PRIx64 "\n", i,
		       cl_hton64(attr_array[i].port_guid));
}

/**********************************************************************
 **********************************************************************/
ib_net64_t get_port_guid(IN osmtest_t * p_osmt, uint64_t port_guid)
{
	ib_api_status_t status;
	uint32_t num_ports = GUID_ARRAY_SIZE;
	ib_port_attr_t attr_array[GUID_ARRAY_SIZE];
	int i;

	/*
	   Call the transport layer for a list of local port
	   GUID values.
	 */
/* "local ports" is(?) phys, shouldn't this exclude port 0 then ? */
	status =
	    osm_vendor_get_all_port_attr(p_osmt->p_vendor, attr_array,
					 &num_ports);
	if (status != IB_SUCCESS) {
		printf("\nError from osm_vendor_get_all_port_attr (%x)\n",
		       status);
		return (0);
	}

	if (num_ports == 1) {
		printf("using default guid 0x%" PRIx64 "\n",
		       cl_hton64(attr_array[0].port_guid));
		return (attr_array[0].port_guid);
	}

	for (i = 0; i < num_ports; i++) {
		if (attr_array[i].port_guid == port_guid ||
		    (!port_guid && attr_array[i].link_state > IB_LINK_DOWN))
			return attr_array[i].port_guid;
	}

	return 0;
}

/**********************************************************************
 **********************************************************************/
int main(int argc, char *argv[])
{
	static osmtest_t osm_test;
	osmtest_opt_t opt = { 0 };
	ib_net64_t guid = 0;
	uint16_t max_lid = 100;
	ib_api_status_t status;
	uint32_t log_flags = OSM_LOG_ERROR | OSM_LOG_INFO;
	int32_t vendor_debug = 0;
	char flow_name[64];
	uint32_t next_option;
	const char *const short_option = "f:l:m:M:d:g:s:t:i:pcvVh";

	/*
	 * In the array below, the 2nd parameter specified the number
	 * of arguments as follows:
	 * 0: no arguments
	 * 1: argument
	 * 2: optional
	 */
	const struct option long_option[] = {
		{"create", 0, NULL, 'c'},
		{"debug", 1, NULL, 'd'},
		{"flow", 1, NULL, 'f'},
		{"wait", 1, NULL, 'w'},
		{"inventory", 1, NULL, 'i'},
		{"max_lid", 1, NULL, 'm'},
		{"guid", 2, NULL, 'g'},
		{"port", 0, NULL, 'p'},
		{"help", 0, NULL, 'h'},
		{"stress", 1, NULL, 's'},
		{"Multicast_Mode", 1, NULL, 'M'},
		{"timeout", 1, NULL, 't'},
		{"verbose", 0, NULL, 'v'},
		{"log_file", 1, NULL, 'l'},
		{"vf", 1, NULL, 'x'},
		{"V", 0, NULL, 'V'},

		{NULL, 0, NULL, 0}	/* Required at end of array */
	};

	/* Make sure that the opensm, complib and osmtest were compiled using
	   same modes (debug/free) */
	if (osm_is_debug() != cl_is_debug() || osm_is_debug() != osmt_is_debug()
	    || osmt_is_debug() != cl_is_debug()) {
		fprintf(stderr,
			"-E- OpenSM, Complib and OsmTest were compiled using different modes\n");
		fprintf(stderr,
			"-E- OpenSM debug:%d Complib debug:%d OsmTest debug:%d \n",
			osm_is_debug(), cl_is_debug(), osmt_is_debug());
		exit(1);
	}

	opt.transaction_timeout = OSMT_DEFAULT_TRANS_TIMEOUT_MILLISEC;
	opt.wait_time = OSMT_DEFAULT_TRAP_WAIT_TIMEOUT_SEC;
	opt.retry_count = OSMT_DEFAULT_RETRY_COUNT;
	opt.force_log_flush = FALSE;
	opt.stress = 0;
	opt.log_file = NULL;
	opt.create = FALSE;
	opt.mmode = 1;
	opt.ignore_path_records = FALSE;	/*  Do path Records too */
	opt.flow = OSMT_FLOW_ALL;	/*  run all validation tests */
	strcpy(flow_name, "All Validations");
	strcpy(opt.file_name, "osmtest.dat");

	printf("\nCommand Line Arguments\n");
	do {
		next_option = getopt_long_only(argc, argv, short_option,
					       long_option, NULL);
		switch (next_option) {
		case 'c':
			/*
			 * Create the inventory file.
			 */
			opt.create = TRUE;
			printf("\tCreating inventory file\n");
			break;

		case 'i':
			/*
			 * Specifies inventory file name.
			 */
			if (strlen(optarg) > OSMTEST_FILE_PATH_MAX)
				printf
				    ("\nError: path name too long (ignored)\n");
			else
				strcpy(opt.file_name, optarg);

			printf("\tFile = %s\n", opt.file_name);
			break;

		case 'f':
			/*
			 * Specifies Flow .
			 */
			if (strlen(optarg) > OSMTEST_FILE_PATH_MAX)
				printf
				    ("\nError: path name too long (ignored)\n");
			else
				strcpy(flow_name, optarg);

			if (!strcmp("c", optarg)) {
				strcpy(flow_name, "Create Inventory");
				opt.flow = OSMT_FLOW_CREATE_INVENTORY;
			} else if (!strcmp("v", optarg)) {
				strcpy(flow_name, "Validate Inventory");
				opt.flow = OSMT_FLOW_VALIDATE_INVENTORY;
			} else if (!strcmp("s", optarg)) {
				strcpy(flow_name, "Services Registration");
				opt.flow = OSMT_FLOW_SERVICE_REGISTRATION;
			} else if (!strcmp("e", optarg)) {
				strcpy(flow_name, "Event Forwarding");
				opt.flow = OSMT_FLOW_EVENT_FORWARDING;
			} else if (!strcmp("f", optarg)) {
				strcpy(flow_name, "Stress SA");
				opt.flow = OSMT_FLOW_STRESS_SA;
			} else if (!strcmp("m", optarg)) {
				strcpy(flow_name, "Multicast");
				opt.flow = OSMT_FLOW_MULTICAST;
			} else if (!strcmp("q", optarg)) {
				strcpy(flow_name, "QoS: VLArb and SLtoVL");
				opt.flow = OSMT_FLOW_QOS;
			} else if (!strcmp("t", optarg)) {
				strcpy(flow_name, "Trap 64/65");
				opt.flow = OSMT_FLOW_TRAP;
			} else if (!strcmp("a", optarg)) {
				strcpy(flow_name, "All Validations");
				opt.flow = OSMT_FLOW_ALL;
			} else {
				printf("\nError: unknown flow %s\n", flow_name);
				exit(2);
			}
			break;

		case 'w':
			/*
			 * Specifies trap 64/65 wait time
			 */
			CL_ASSERT(strtol(optarg, NULL, 0) < 0x100);
			opt.wait_time = (uint8_t) strtol(optarg, NULL, 0);
			printf("\tTrap 64/65 wait time = %d\n", opt.wait_time);
			break;

		case 'm':
			/*
			 * Specifies the max LID to search for during exploration.
			 */
			max_lid = atoi(optarg);
			printf("\tMAX-LID %u\n", max_lid);
			break;

		case 'g':
			/*
			 * Specifies port guid with which to bind.
			 */
			guid = cl_hton64(strtoull(optarg, NULL, 16));
			printf(" Guid <0x%" PRIx64 ">\n", cl_hton64(guid));
			break;

		case 'p':
			/*
			 * Display current port guids
			 */
			guid = INVALID_GUID;
			break;

		case 't':
			/*
			 * Specifies transaction timeout.
			 */
			opt.transaction_timeout = strtol(optarg, NULL, 0);
			printf("\tTransaction timeout = %d\n",
			       opt.transaction_timeout);
			break;

		case 'l':
			opt.log_file = optarg;
			printf("\tLog File:%s\n", opt.log_file);
			break;

		case 'v':
			/*
			 * Increases log verbosity.
			 */
			log_flags = (log_flags << 1) | 1;
			printf("\tVerbose option -v (log flags = 0x%X)\n",
			       log_flags);
			break;

		case 'V':
			/*
			 * Specifies maximum log verbosity.
			 */
			log_flags = 0xFFFFFFFF;
			opt.force_log_flush = TRUE;
			printf("\tEnabling maximum log verbosity\n");
			break;

		case 's':
			/*
			 * Perform stress test.
			 */
			opt.stress = strtol(optarg, NULL, 0);
			printf("\tStress test enabled: ");
			switch (opt.stress) {
			case 1:
				printf("Small SA queries\n");
				break;
			case 2:
				printf("Large SA queries\n");
				break;
			case 3:
				printf("Large Path Record SA queries\n");
				break;
			default:
				printf("Unknown value %u (ignored)\n",
				       opt.stress);
				opt.stress = 0;
				break;
			}
			break;

		case 'M':
			/*
			 * Perform multicast test.
			 */
			opt.mmode = strtol(optarg, NULL, 0);
			printf("\tMulticast test enabled: ");
			switch (opt.mmode) {
			case 1:
				printf
				    ("Short MC Flow - single mode (default)\n");
				break;
			case 2:
				printf("Short MC Flow - multiple mode\n");
				break;
			case 3:
				printf("Long MC Flow - single mode\n");
				break;
			case 4:
				printf("Long MC Flow - multiple mode\n");
				break;
			default:
				printf("Unknown value %u (ignored)\n",
				       opt.stress);
				opt.mmode = 0;
				break;
			}
			break;

		case 'd':
			/*
			 * Debug Options
			 */
			printf("\tDebug Option: ");
			switch (strtol(optarg, NULL, 0)) {
			case 1:
				printf("Ignore Path Records\n");
				opt.ignore_path_records = TRUE;
				break;
			case 2:
				printf("Force Log Flush\n");
				opt.force_log_flush = TRUE;
				break;
			case 3:
				/* Used to be memory tracking */
			default:
				printf("Unknown value %ld (ignored)\n",
				       strtol(optarg, NULL, 0));
				break;
			}
			break;

		case 'h':
			show_usage();
			return 0;

		case 'x':
			log_flags = strtol(optarg, NULL, 0);
			printf
			    ("\t\t\t\tVerbose option -vf (log flags = 0x%X)\n",
			     log_flags);
			break;

		case -1:
			printf("Done with args\n");
			break;

		default:	/* something wrong */
			abort();
		}

	}
	while (next_option != -1);

	printf("\tFlow = %s\n", flow_name);

	if (vendor_debug)
		osm_vendor_set_debug(osm_test.p_vendor, vendor_debug);

	complib_init();

	status = osmtest_init(&osm_test, &opt, (osm_log_level_t) log_flags);
	if (status != IB_SUCCESS) {
		printf("\nError from osmtest_init: %s\n",
		       ib_get_err_str(status));
		goto Exit;
	}
	if (cl_hton64(guid) == cl_hton64(INVALID_GUID)) {
		print_all_guids(&osm_test);
		complib_exit();
		return (status);
	}

	/*
	   If the user didn't specify a GUID on the command line,
	   then get a port GUID value with which to bind.
	 */
	if (guid == 0 && !(guid = get_port_guid(&osm_test, guid))) {
		printf("\nError: port guid 0x%" PRIx64 " not found\n", guid);
		goto Exit;
	}

	/*
	 * Guid may be zero going into this function if the user
	 * hasn't specified a binding port on the command line.
	 */
	status = osmtest_bind(&osm_test, max_lid, guid);
	if (status != IB_SUCCESS)
		exit(status);

	status = osmtest_run(&osm_test);
	if (status != IB_SUCCESS) {
		printf("OSMTEST: TEST \"%s\" FAIL\n", flow_name);
	} else {
		printf("OSMTEST: TEST \"%s\" PASS\n", flow_name);
	}
	osmtest_destroy(&osm_test);

	complib_exit();

Exit:
	return (status);
}
