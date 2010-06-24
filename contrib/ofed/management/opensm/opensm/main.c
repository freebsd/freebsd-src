/*
 * Copyright (c) 2004-2008 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2008 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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
 *    Command line interface for opensm.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <complib/cl_types.h>
#include <complib/cl_debug.h>
#include <vendor/osm_vendor_api.h>
#include <opensm/osm_version.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_console.h>
#include <opensm/osm_console_io.h>
#include <opensm/osm_perfmgr.h>

volatile unsigned int osm_exit_flag = 0;

static volatile unsigned int osm_hup_flag = 0;
static volatile unsigned int osm_usr1_flag = 0;

#define GUID_ARRAY_SIZE 64
#define INVALID_GUID (0xFFFFFFFFFFFFFFFFULL)

static void mark_exit_flag(int signum)
{
	if (!osm_exit_flag)
		printf("OpenSM: Got signal %d - exiting...\n", signum);
	osm_exit_flag = 1;
}

static void mark_hup_flag(int signum)
{
	osm_hup_flag = 1;
}

static void mark_usr1_flag(int signum)
{
	osm_usr1_flag = 1;
}

static sigset_t saved_sigset;

static void block_signals()
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
#ifndef HAVE_OLD_LINUX_THREADS
	sigaddset(&set, SIGUSR1);
#endif
	pthread_sigmask(SIG_SETMASK, &set, &saved_sigset);
}

static void setup_signals()
{
	struct sigaction act;

	sigemptyset(&act.sa_mask);
	act.sa_handler = mark_exit_flag;
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	act.sa_handler = mark_hup_flag;
	sigaction(SIGHUP, &act, NULL);
	sigaction(SIGCONT, &act, NULL);
#ifndef HAVE_OLD_LINUX_THREADS
	act.sa_handler = mark_usr1_flag;
	sigaction(SIGUSR1, &act, NULL);
#endif
	pthread_sigmask(SIG_SETMASK, &saved_sigset, NULL);
}

/**********************************************************************
 **********************************************************************/

static void show_usage(void)
{
	printf("\n------- OpenSM - Usage and options ----------------------\n");
	printf("Usage:   opensm [options]\n");
	printf("Options:\n");
	printf("--version\n          Prints OpenSM version and exits.\n\n");
	printf("--config, -F <file-name>\n"
	       "          The name of the OpenSM config file. When not specified\n"
	       "          " OSM_DEFAULT_CONFIG_FILE " will be used (if exists).\n\n");
	printf("--create-config, -c <file-name>\n"
	       "          OpenSM will dump its configuration to the specified file and exit.\n"
	       "          This is a way to generate OpenSM configuration file template.\n\n");
	printf("--guid, -g <GUID in hex>\n"
	       "          This option specifies the local port GUID value\n"
	       "          with which OpenSM should bind.  OpenSM may be\n"
	       "          bound to 1 port at a time.\n"
	       "          If GUID given is 0, OpenSM displays a list\n"
	       "          of possible port GUIDs and waits for user input.\n"
	       "          Without -g, OpenSM tries to use the default port.\n\n");
	printf("--lmc, -l <LMC>\n"
	       "          This option specifies the subnet's LMC value.\n"
	       "          The number of LIDs assigned to each port is 2^LMC.\n"
	       "          The LMC value must be in the range 0-7.\n"
	       "          LMC values > 0 allow multiple paths between ports.\n"
	       "          LMC values > 0 should only be used if the subnet\n"
	       "          topology actually provides multiple paths between\n"
	       "          ports, i.e. multiple interconnects between switches.\n"
	       "          Without -l, OpenSM defaults to LMC = 0, which allows\n"
	       "          one path between any two ports.\n\n");
	printf("--priority, -p <PRIORITY>\n"
	       "          This option specifies the SM's PRIORITY.\n"
	       "          This will effect the handover cases, where master\n"
	       "          is chosen by priority and GUID.  Range goes\n"
	       "          from 0 (lowest priority) to 15 (highest).\n\n");
	printf("--smkey, -k <SM_Key>\n"
	       "          This option specifies the SM's SM_Key (64 bits).\n"
	       "          This will effect SM authentication.\n"
	       "          Note that OpenSM version 3.2.1 and below used the\n"
	       "          default value '1' in a host byte order, it is fixed\n"
	       "          now but you may need this option to interoperate\n"
	       "          with old OpenSM running on a little endian machine.\n\n");
	printf("--reassign_lids, -r\n"
	       "          This option causes OpenSM to reassign LIDs to all\n"
	       "          end nodes. Specifying -r on a running subnet\n"
	       "          may disrupt subnet traffic.\n"
	       "          Without -r, OpenSM attempts to preserve existing\n"
	       "          LID assignments resolving multiple use of same LID.\n\n");
	printf("--routing_engine, -R <engine name>\n"
	       "          This option chooses routing engine(s) to use instead of default\n"
	       "          Min Hop algorithm.  Multiple routing engines can be specified\n"
	       "          separated by commas so that specific ordering of routing\n"
	       "          algorithms will be tried if earlier routing engines fail.\n"
	       "          Supported engines: updn, file, ftree, lash, dor\n\n");
	printf("--connect_roots, -z\n"
	       "          This option enforces a routing engine (currently\n"
	       "          up/down only) to make connectivity between root switches\n"
	       "          and in this way be IBA compliant. In many cases,\n"
	       "          this can violate \"pure\" deadlock free algorithm, so\n"
	       "          use it carefully.\n\n");
	printf("--ucast_cache, -A\n"
	       "          This option enables unicast routing cache to prevent\n"
	       "          routing recalculation (which is a heavy task in a\n"
	       "          large cluster) when there was no topology change\n"
	       "          detected during the heavy sweep, or when the topology\n"
	       "          change does not require new routing calculation,\n"
	       "          e.g. in case of host reboot.\n"
	       "          This option becomes very handy when the cluster size\n"
	       "          is thousands of nodes.\n\n");
	printf("--lid_matrix_file, -M <file name>\n"
	       "          This option specifies the name of the lid matrix dump file\n"
	       "          from where switch lid matrices (min hops tables will be\n"
	       "          loaded.\n\n");
	printf("--lfts_file, -U <file name>\n"
	       "          This option specifies the name of the LFTs file\n"
	       "          from where switch forwarding tables will be loaded.\n\n");
	printf("--sadb_file, -S <file name>\n"
	       "          This option specifies the name of the SA DB dump file\n"
	       "          from where SA database will be loaded.\n\n");
	printf("--root_guid_file, -a <path to file>\n"
	       "          Set the root nodes for the Up/Down or Fat-Tree routing\n"
	       "          algorithm to the guids provided in the given file (one\n"
	       "          to a line)\n" "\n");
	printf("--cn_guid_file, -u <path to file>\n"
	       "          Set the compute nodes for the Fat-Tree routing algorithm\n"
	       "          to the guids provided in the given file (one to a line)\n\n");
	printf("--ids_guid_file, -m <path to file>\n"
	       "          Name of the map file with set of the IDs which will be used\n"
	       "          by Up/Down routing algorithm instead of node GUIDs\n"
	       "          (format: <guid> <id> per line)\n\n");
	printf("--guid_routing_order_file, -X <path to file>\n"
	       "          Set the order port guids will be routed for the MinHop\n"
	       "          and Up/Down routing algorithms to the guids provided in the\n"
	       "          given file (one to a line)\n\n");
	printf("--once, -o\n"
	       "          This option causes OpenSM to configure the subnet\n"
	       "          once, then exit.  Ports remain in the ACTIVE state.\n\n");
	printf("--sweep, -s <interval>\n"
	       "          This option specifies the number of seconds between\n"
	       "          subnet sweeps.  Specifying -s 0 disables sweeping.\n"
	       "          Without -s, OpenSM defaults to a sweep interval of\n"
	       "          10 seconds.\n\n");
	printf("--timeout, -t <milliseconds>\n"
	       "          This option specifies the time in milliseconds\n"
	       "          used for transaction timeouts.\n"
	       "          Specifying -t 0 disables timeouts.\n"
	       "          Without -t, OpenSM defaults to a timeout value of\n"
	       "          200 milliseconds.\n\n");
	printf("--maxsmps, -n <number>\n"
	       "          This option specifies the number of VL15 SMP MADs\n"
	       "          allowed on the wire at any one time.\n"
	       "          Specifying --maxsmps 0 allows unlimited outstanding\n"
	       "          SMPs.\n"
	       "          Without --maxsmps, OpenSM defaults to a maximum of\n"
	       "          4 outstanding SMPs.\n\n");
	printf("--console, -q [off|local"
#ifdef ENABLE_OSM_CONSOLE_SOCKET
	       "|socket|loopback"
#endif
	       "]\n          This option activates the OpenSM console (default off).\n\n");
#ifdef ENABLE_OSM_CONSOLE_SOCKET
	printf("--console-port, -C <port>\n"
	       "          Specify an alternate telnet port for the console (default %d).\n\n",
	       OSM_DEFAULT_CONSOLE_PORT);
#endif
	printf("--ignore-guids, -i <equalize-ignore-guids-file>\n"
	       "          This option provides the means to define a set of ports\n"
	       "          (by guid) that will be ignored by the link load\n"
	       "          equalization algorithm.\n\n");
	printf("--honor_guid2lid, -x\n"
	       "          This option forces OpenSM to honor the guid2lid file,\n"
	       "          when it comes out of Standby state, if such file exists\n"
	       "          under OSM_CACHE_DIR, and is valid. By default, this is FALSE.\n\n");
	printf("--log_file, -f <log-file-name>\n"
	       "          This option defines the log to be the given file.\n"
	       "          By default, the log goes to /var/log/opensm.log.\n"
	       "          For the log to go to standard output use -f stdout.\n\n");
	printf("--log_limit, -L <size in MB>\n"
	       "          This option defines maximal log file size in MB. When\n"
	       "          specified the log file will be truncated upon reaching\n"
	       "          this limit.\n\n");
	printf("--erase_log_file, -e\n"
	       "          This option will cause deletion of the log file\n"
	       "          (if it previously exists). By default, the log file\n"
	       "          is accumulative.\n\n");
	printf("--Pconfig, -P <partition-config-file>\n"
	       "          This option defines the optional partition configuration file.\n"
	       "          The default name is \'"
	       OSM_DEFAULT_PARTITION_CONFIG_FILE "\'.\n\n");
	printf("--no_part_enforce, -N\n"
	       "          This option disables partition enforcement on switch external ports.\n\n");
	printf("--qos, -Q\n" "          This option enables QoS setup.\n\n");
	printf("--qos_policy_file, -Y <QoS-policy-file>\n"
	       "          This option defines the optional QoS policy file.\n"
	       "          The default name is \'" OSM_DEFAULT_QOS_POLICY_FILE
	       "\'.\n\n");
	printf("--stay_on_fatal, -y\n"
	       "          This option will cause SM not to exit on fatal initialization\n"
	       "          issues: if SM discovers duplicated guids or 12x link with\n"
	       "          lane reversal badly configured.\n"
	       "          By default, the SM will exit on these errors.\n\n");
	printf("--daemon, -B\n"
	       "          Run in daemon mode - OpenSM will run in the background.\n\n");
	printf("--inactive, -I\n"
	       "           Start SM in inactive rather than normal init SM state.\n\n");
#ifdef ENABLE_OSM_PERF_MGR
	printf("--perfmgr\n" "           Start with PerfMgr enabled.\n\n");
	printf("--perfmgr_sweep_time_s <sec.>\n"
	       "           PerfMgr sweep interval in seconds.\n\n");
#endif
	printf("--prefix_routes_file <path to file>\n"
	       "          This option specifies the prefix routes file.\n"
	       "          Prefix routes control how the SA responds to path record\n"
	       "          queries for off-subnet DGIDs.  Default file is:\n"
	       "              " OSM_DEFAULT_PREFIX_ROUTES_FILE "\n\n");
	printf("--consolidate_ipv6_snm_req\n"
	       "          Consolidate IPv6 Solicited Node Multicast group joins\n"
	       "          into 1 IB multicast group.\n\n");
	printf("--verbose, -v\n"
	       "          This option increases the log verbosity level.\n"
	       "          The -v option may be specified multiple times\n"
	       "          to further increase the verbosity level.\n"
	       "          See the -D option for more information about\n"
	       "          log verbosity.\n\n");
	printf("--V, -V\n"
	       "          This option sets the maximum verbosity level and\n"
	       "          forces log flushing.\n"
	       "          The -V is equivalent to '-D 0xFF -d 2'.\n"
	       "          See the -D option for more information about\n"
	       "          log verbosity.\n\n");
	printf("--D, -D <flags>\n"
	       "          This option sets the log verbosity level.\n"
	       "          A flags field must follow the -D option.\n"
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
	       "          0x40 - ROUTING (dump FDB routing information)\n"
	       "          0x80 - currently unused.\n"
	       "          Without -D, OpenSM defaults to ERROR + INFO (0x3).\n"
	       "          Specifying -D 0 disables all messages.\n"
	       "          Specifying -D 0xFF enables all messages (see -V).\n"
	       "          High verbosity levels may require increasing\n"
	       "          the transaction timeout with the -t option.\n\n");
	printf("--debug, -d <number>\n"
	       "          This option specifies a debug option.\n"
	       "          These options are not normally needed.\n"
	       "          The number following -d selects the debug\n"
	       "          option to enable as follows:\n"
	       "          OPT   Description\n"
	       "          ---    -----------------\n"
	       "          -d0  - Ignore other SM nodes\n"
	       "          -d1  - Force single threaded dispatching\n"
	       "          -d2  - Force log flushing after each log message\n"
	       "          -d3  - Disable multicast support\n"
	       "          -d10 - Put OpenSM in testability mode\n"
	       "          Without -d, no debug options are enabled\n\n");
	printf("--help, -h, -?\n"
	       "          Display this usage info then exit.\n\n");
	fflush(stdout);
	exit(2);
}

/**********************************************************************
 **********************************************************************/
static ib_net64_t get_port_guid(IN osm_opensm_t * p_osm, uint64_t port_guid)
{
	ib_port_attr_t attr_array[GUID_ARRAY_SIZE];
	uint32_t num_ports = GUID_ARRAY_SIZE;
	char junk[128];
	uint32_t i, choice = 0;
	boolean_t done_flag = FALSE;
	ib_api_status_t status;

	/*
	   Call the transport layer for a list of local port
	   GUID values.
	 */
	status =
	    osm_vendor_get_all_port_attr(p_osm->p_vendor, attr_array,
					 &num_ports);
	if (status != IB_SUCCESS) {
		printf("\nError from osm_vendor_get_all_port_attr (%x)\n",
		       status);
		return (0);
	}

	/* if num_ports is 0 - return 0 */
	if (num_ports == 0) {
		printf("\nNo local ports detected!\n");
		return (0);
	}
	/* If num_ports is 1, then there is only one possible port to use.
	 * Use it. */
	if (num_ports == 1) {
		printf("Using default GUID 0x%" PRIx64 "\n",
		       cl_hton64(attr_array[0].port_guid));
		return (attr_array[0].port_guid);
	}
	/* If port_guid is 0 - use the first connected port */
	if (port_guid == 0) {
		for (i = 0; i < num_ports; i++)
			if (attr_array[i].link_state > IB_LINK_DOWN)
				break;
		if (i == num_ports)
			i = 0;
		printf("Using default GUID 0x%" PRIx64 "\n",
		       cl_hton64(attr_array[i].port_guid));
		return (attr_array[i].port_guid);
	}

	if (p_osm->subn.opt.daemon)
		return 0;

	/* More than one possible port - list all ports and let the user
	 * to choose. */
	while (done_flag == FALSE) {
		printf("\nChoose a local port number with which to bind:\n\n");
		for (i = 0; i < num_ports; i++)
			/* Print the index + 1 since by convention, port
			 * numbers start with 1 on host channel adapters. */
			printf("\t%u: GUID 0x%" PRIx64
			       ", lid %u, state %s\n", i + 1,
			       cl_ntoh64(attr_array[i].port_guid),
			       attr_array[i].lid,
			       ib_get_port_state_str(attr_array[i].link_state));
		printf("\nEnter choice (1-%u): ", i);
		fflush(stdout);
		if (scanf("%u", &choice)) {
			if (choice > num_ports || choice < 1) {
				printf("\nError: Lame choice!\n");
				fflush(stdin);
			} else {
				choice--;
				done_flag = TRUE;
			}
		} else {
			/* get rid of the junk in the selection line */
			scanf("%s", junk);
			printf("\nError: Lame choice!\n");
			fflush(stdin);
		}
	}
	printf("Choice guid=0x%" PRIx64 "\n",
	       cl_ntoh64(attr_array[choice].port_guid));
	return (attr_array[choice].port_guid);
}

/**********************************************************************
 **********************************************************************/

static int daemonize(osm_opensm_t * osm)
{
	pid_t pid;
	int fd;

	fd = open("/dev/null", O_WRONLY);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		exit(-1);
	} else if (pid > 0)
		exit(0);

	setsid();

	if ((pid = fork()) < 0) {
		perror("fork");
		exit(-1);
	} else if (pid > 0)
		exit(0);

	close(0);
	close(1);
	close(2);

	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);

	return 0;
}

/**********************************************************************
 **********************************************************************/
int osm_manager_loop(osm_subn_opt_t * p_opt, osm_opensm_t * p_osm)
{
	int console_init_flag = 0;

	if (is_console_enabled(p_opt)) {
		if (!osm_console_init(p_opt, &p_osm->console, &p_osm->log))
			console_init_flag = 1;
	}

	/*
	   Sit here forever - dwell or do console i/o & cmds
	 */
	while (!osm_exit_flag) {
		if (console_init_flag)
			osm_console(p_osm);
		else
			cl_thread_suspend(10000);

		if (osm_usr1_flag) {
			osm_usr1_flag = 0;
			osm_log_reopen_file(&(p_osm->log));
		}
		if (osm_hup_flag) {
			osm_hup_flag = 0;
			/* a HUP signal should only start a new heavy sweep */
			p_osm->subn.force_heavy_sweep = TRUE;
			osm_opensm_sweep(p_osm);
		}
	}
	if (is_console_enabled(p_opt))
		osm_console_exit(&p_osm->console, &p_osm->log);
	return 0;
}

/**********************************************************************
 **********************************************************************/
int main(int argc, char *argv[])
{
	osm_opensm_t osm;
	osm_subn_opt_t opt;
	ib_net64_t sm_key = 0;
	ib_api_status_t status;
	uint32_t temp, dbg_lvl;
	boolean_t run_once_flag = FALSE;
	int32_t vendor_debug = 0;
	uint32_t next_option;
	char *conf_template = NULL;
	uint32_t val;
	unsigned config_file_done = 0;
	const char *const short_option =
	    "F:c:i:f:ed:D:g:l:L:s:t:a:u:m:X:R:zM:U:S:P:Y:ANBIQvVhoryxp:n:q:k:C:";

	/*
	   In the array below, the 2nd parameter specifies the number
	   of arguments as follows:
	   0: no arguments
	   1: argument
	   2: optional
	 */
	const struct option long_option[] = {
		{"version", 0, NULL, 12},
		{"config", 1, NULL, 'F'},
		{"create-config", 1, NULL, 'c'},
		{"debug", 1, NULL, 'd'},
		{"guid", 1, NULL, 'g'},
		{"ignore_guids", 1, NULL, 'i'},
		{"lmc", 1, NULL, 'l'},
		{"sweep", 1, NULL, 's'},
		{"timeout", 1, NULL, 't'},
		{"verbose", 0, NULL, 'v'},
		{"D", 1, NULL, 'D'},
		{"log_file", 1, NULL, 'f'},
		{"log_limit", 1, NULL, 'L'},
		{"erase_log_file", 0, NULL, 'e'},
		{"Pconfig", 1, NULL, 'P'},
		{"no_part_enforce", 0, NULL, 'N'},
		{"qos", 0, NULL, 'Q'},
		{"qos_policy_file", 1, NULL, 'Y'},
		{"maxsmps", 1, NULL, 'n'},
		{"console", 1, NULL, 'q'},
		{"V", 0, NULL, 'V'},
		{"help", 0, NULL, 'h'},
		{"once", 0, NULL, 'o'},
		{"reassign_lids", 0, NULL, 'r'},
		{"priority", 1, NULL, 'p'},
		{"smkey", 1, NULL, 'k'},
		{"routing_engine", 1, NULL, 'R'},
		{"ucast_cache", 0, NULL, 'A'},
		{"connect_roots", 0, NULL, 'z'},
		{"lid_matrix_file", 1, NULL, 'M'},
		{"lfts_file", 1, NULL, 'U'},
		{"sadb_file", 1, NULL, 'S'},
		{"root_guid_file", 1, NULL, 'a'},
		{"cn_guid_file", 1, NULL, 'u'},
		{"ids_guid_file", 1, NULL, 'm'},
		{"guid_routing_order_file", 1, NULL, 'X'},
		{"stay_on_fatal", 0, NULL, 'y'},
		{"honor_guid2lid", 0, NULL, 'x'},
#ifdef ENABLE_OSM_CONSOLE_SOCKET
		{"console-port", 1, NULL, 'C'},
#endif
		{"daemon", 0, NULL, 'B'},
		{"inactive", 0, NULL, 'I'},
#ifdef ENABLE_OSM_PERF_MGR
		{"perfmgr", 0, NULL, 1},
		{"perfmgr_sweep_time_s", 1, NULL, 2},
#endif
		{"prefix_routes_file", 1, NULL, 3},
		{"consolidate_ipv6_snm_req", 0, NULL, 4},
		{NULL, 0, NULL, 0}	/* Required at the end of the array */
	};

	/* Make sure that the opensm and complib were compiled using
	   same modes (debug/free) */
	if (osm_is_debug() != cl_is_debug()) {
		fprintf(stderr,
			"ERROR: OpenSM and Complib were compiled using different modes\n");
		fprintf(stderr, "ERROR: OpenSM debug:%d Complib debug:%d \n",
			osm_is_debug(), cl_is_debug());
		exit(1);
	}
#if defined (_DEBUG_) && defined (OSM_VENDOR_INTF_OPENIB)
	enable_stack_dump(1);
#endif

	printf("-------------------------------------------------\n");
	printf("%s\n", OSM_VERSION);

	osm_subn_set_default_opt(&opt);

	if (osm_subn_parse_conf_file(OSM_DEFAULT_CONFIG_FILE, &opt) < 0)
		printf("\nosm_subn_parse_conf_file failed!\n");

	printf("Command Line Arguments:\n");
	do {
		next_option = getopt_long_only(argc, argv, short_option,
					       long_option, NULL);
		switch (next_option) {
		case 12: /* --version - already printed above */
			exit(0);
			break;
		case 'F':
			if (config_file_done)
				break;
			printf("Reloading config from `%s`:\n", optarg);
			if (osm_subn_parse_conf_file(optarg, &opt)) {
				printf("cannot parse config file.\n");
				exit(1);
			}
			printf("Rescaning command line:\n");
			config_file_done = 1;
			optind = 0;
			break;
		case 'c':
			conf_template = optarg;
			printf(" Creating config file template \'%s\'.\n",
			       conf_template);
			break;
		case 'o':
			/*
			   Run once option.
			 */
			run_once_flag = TRUE;
			printf(" Run Once\n");
			break;

		case 'r':
			/*
			   Reassign LIDs subnet option.
			 */
			opt.reassign_lids = TRUE;
			printf(" Reassign LIDs\n");
			break;

		case 'i':
			/*
			   Specifies ignore guids file.
			 */
			opt.port_prof_ignore_file = optarg;
			printf(" Ignore Guids File = %s\n",
			       opt.port_prof_ignore_file);
			break;

		case 'g':
			/*
			   Specifies port guid with which to bind.
			 */
			opt.guid = cl_hton64(strtoull(optarg, NULL, 16));
			if (!opt.guid)
				/* If guid is 0 - need to display the
				 * guid list */
				opt.guid = INVALID_GUID;
			else
				printf(" Guid <0x%" PRIx64 ">\n",
				       cl_hton64(opt.guid));
			break;

		case 's':
			val = strtol(optarg, NULL, 0);
			/* Check that the number is not too large */
			if (((uint32_t) (val * 1000000)) / 1000000 != val)
				fprintf(stderr,
					"ERROR: sweep interval given is too large. Ignoring it.\n");
			else {
				opt.sweep_interval = val;
				printf(" sweep interval = %d\n",
				       opt.sweep_interval);
			}
			break;

		case 't':
			opt.transaction_timeout = strtol(optarg, NULL, 0);
			printf(" Transaction timeout = %d\n",
			       opt.transaction_timeout);
			break;

		case 'n':
			opt.max_wire_smps = strtol(optarg, NULL, 0);
			if (opt.max_wire_smps <= 0)
				opt.max_wire_smps = 0x7FFFFFFF;
			printf(" Max wire smp's = %d\n", opt.max_wire_smps);
			break;

		case 'q':
			/*
			 * OpenSM interactive console
			 */
			if (strcmp(optarg, OSM_DISABLE_CONSOLE) == 0
			    || strcmp(optarg, OSM_LOCAL_CONSOLE) == 0
#ifdef ENABLE_OSM_CONSOLE_SOCKET
			    || strcmp(optarg, OSM_REMOTE_CONSOLE) == 0
			    || strcmp(optarg, OSM_LOOPBACK_CONSOLE) == 0
#endif
			    )
				opt.console = optarg;
			else
				printf("-console %s option not understood\n",
				       optarg);
			break;

#ifdef ENABLE_OSM_CONSOLE_SOCKET
		case 'C':
			opt.console_port = strtol(optarg, NULL, 0);
			break;
#endif

		case 'd':
			dbg_lvl = strtol(optarg, NULL, 0);
			printf(" d level = 0x%x\n", dbg_lvl);
			if (dbg_lvl == 0) {
				printf(" Debug mode: Ignore Other SMs\n");
				opt.ignore_other_sm = TRUE;
			} else if (dbg_lvl == 1) {
				printf(" Debug mode: Forcing Single Thread\n");
				opt.single_thread = TRUE;
			} else if (dbg_lvl == 2) {
				printf(" Debug mode: Force Log Flush\n");
				opt.force_log_flush = TRUE;
			} else if (dbg_lvl == 3) {
				printf
				    (" Debug mode: Disable multicast support\n");
				opt.disable_multicast = TRUE;
			}
			/*
			 * NOTE: Debug level 4 used to be used for memory
			 * tracking but this is now deprecated
			 */
			else if (dbg_lvl == 5)
				vendor_debug++;
			else
				printf(" OpenSM: Unknown debug option %d"
				       " ignored\n", dbg_lvl);
			break;

		case 'l':
			temp = strtol(optarg, NULL, 0);
			if (temp > 7) {
				fprintf(stderr,
					"ERROR: LMC must be 7 or less.\n");
				return (-1);
			}
			opt.lmc = (uint8_t) temp;
			printf(" LMC = %d\n", temp);
			break;

		case 'D':
			opt.log_flags = strtol(optarg, NULL, 0);
			printf(" verbose option -D = 0x%x\n", opt.log_flags);
			break;

		case 'f':
			opt.log_file = optarg;
			break;

		case 'L':
			opt.log_max_size =
			    strtoul(optarg, NULL, 0) * (1024 * 1024);
			printf(" Log file max size is %lu bytes\n",
			       opt.log_max_size);
			break;

		case 'e':
			opt.accum_log_file = FALSE;
			printf(" Creating new log file\n");
			break;

		case 'P':
			opt.partition_config_file = optarg;
			break;

		case 'N':
			opt.no_partition_enforcement = TRUE;
			break;

		case 'Q':
			opt.qos = TRUE;
			break;

		case 'Y':
			opt.qos_policy_file = optarg;
			printf(" QoS policy file \'%s\'\n", optarg);
			break;

		case 'y':
			opt.exit_on_fatal = FALSE;
			printf(" Staying on fatal initialization errors\n");
			break;

		case 'v':
			opt.log_flags = (opt.log_flags << 1) | 1;
			printf(" Verbose option -v (log flags = 0x%X)\n",
			       opt.log_flags);
			break;

		case 'V':
			opt.log_flags = 0xFF;
			opt.force_log_flush = TRUE;
			printf(" Big V selected\n");
			break;

		case 'p':
			temp = strtol(optarg, NULL, 0);
			if (0 > temp || 15 < temp) {
				fprintf(stderr,
					"ERROR: priority must be between 0 and 15\n");
				return (-1);
			}
			opt.sm_priority = (uint8_t) temp;
			printf(" Priority = %d\n", temp);
			break;

		case 'k':
			sm_key = cl_hton64(strtoull(optarg, NULL, 16));
			printf(" SM Key <0x%" PRIx64 ">\n", cl_hton64(sm_key));
			opt.sm_key = sm_key;
			break;

		case 'R':
			opt.routing_engine_names = optarg;
			printf(" Activate \'%s\' routing engine(s)\n", optarg);
			break;

		case 'z':
			opt.connect_roots = TRUE;
			printf(" Connect roots option is on\n");
			break;

		case 'A':
			opt.use_ucast_cache = TRUE;
			printf(" Unicast routing cache option is on\n");
			break;

		case 'M':
			opt.lid_matrix_dump_file = optarg;
			printf(" Lid matrix dump file is \'%s\'\n", optarg);
			break;

		case 'U':
			opt.lfts_file = optarg;
			printf(" LFTs file is \'%s\'\n", optarg);
			break;

		case 'S':
			opt.sa_db_file = optarg;
			printf(" SA DB file is \'%s\'\n", optarg);
			break;

		case 'a':
			/*
			   Specifies root guids file
			 */
			opt.root_guid_file = optarg;
			printf(" Root Guid File: %s\n", opt.root_guid_file);
			break;

		case 'u':
			/*
			   Specifies compute node guids file
			 */
			opt.cn_guid_file = optarg;
			printf(" Compute Node Guid File: %s\n",
			       opt.cn_guid_file);
			break;

		case 'm':
			/* Specifies ids guid file */
			opt.ids_guid_file = optarg;
			printf(" IDs Guid File: %s\n", opt.ids_guid_file);
			break;

		case 'X':
			/* Specifies guid routing order file */
			opt.guid_routing_order_file = optarg;
			printf(" GUID Routing Order File: %s\n", opt.guid_routing_order_file);
			break;

		case 'x':
			opt.honor_guid2lid_file = TRUE;
			printf(" Honor guid2lid file, if possible\n");
			break;

		case 'B':
			opt.daemon = TRUE;
			printf(" Daemon mode\n");
			break;

		case 'I':
			opt.sm_inactive = TRUE;
			printf(" SM started in inactive state\n");
			break;

#ifdef ENABLE_OSM_PERF_MGR
		case 1:
			opt.perfmgr = TRUE;
			break;
		case 2:
			opt.perfmgr_sweep_time_s = atoi(optarg);
			break;
#endif				/* ENABLE_OSM_PERF_MGR */

		case 3:
			opt.prefix_routes_file = optarg;
			break;
		case 4:
			opt.consolidate_ipv6_snm_req = TRUE;
			break;
		case 'h':
		case '?':
		case ':':
			show_usage();
			break;

		case -1:
			break;	/* done with option */
		default:	/* something wrong */
			abort();
		}
	}
	while (next_option != -1);

	if (opt.log_file != NULL)
		printf(" Log File: %s\n", opt.log_file);
	/* Done with options description */
	printf("-------------------------------------------------\n");

	if (conf_template) {
		status = osm_subn_write_conf_file(conf_template, &opt);
		if (status)
			printf("\nosm_subn_write_conf_file failed!\n");
		exit(status);
	}

	osm_subn_verify_config(&opt);

	if (vendor_debug)
		osm_vendor_set_debug(osm.p_vendor, vendor_debug);

	block_signals();

	if (opt.daemon)
		daemonize(&osm);

	complib_init();

	status = osm_opensm_init(&osm, &opt);
	if (status != IB_SUCCESS) {
		const char *err_str = ib_get_err_str(status);
		if (err_str == NULL)
			err_str = "Unknown Error Type";
		printf("\nError from osm_opensm_init: %s.\n", err_str);
		/* We will just exit, and not go to Exit, since we don't
		   want the destroy to be called. */
		complib_exit();
		return (status);
	}

	/*
	   If the user didn't specify a GUID on the command line,
	   then get a port GUID value with which to bind.
	 */
	if (opt.guid == 0 || cl_hton64(opt.guid) == CL_HTON64(INVALID_GUID))
		opt.guid = get_port_guid(&osm, opt.guid);

	status = osm_opensm_bind(&osm, opt.guid);
	if (status != IB_SUCCESS) {
		printf("\nError from osm_opensm_bind (0x%X)\n", status);
		printf
		    ("Perhaps another instance of OpenSM is already running\n");
		goto Exit;
	}

	setup_signals();

	osm_opensm_sweep(&osm);

	if (run_once_flag == TRUE) {
		while (!osm_exit_flag) {
			status =
			    osm_opensm_wait_for_subnet_up(&osm,
							  osm.subn.opt.
							  sweep_interval *
							  1000000, TRUE);
			if (!status)
				osm_exit_flag = 1;
		}
	} else {
		/*
		 *         Sit here until signaled to exit
		 */
		osm_manager_loop(&opt, &osm);
	}

	if (osm.mad_pool.mads_out) {
		fprintf(stdout,
			"There are still %u MADs out. Forcing the exit of the OpenSM application...\n",
			osm.mad_pool.mads_out);
#ifdef HAVE_LIBPTHREAD
		pthread_cond_signal(&osm.stats.cond);
#else
		cl_event_signal(&osm.stats.event);
#endif
	}

Exit:
	osm_opensm_destroy(&osm);
	complib_exit();

	exit(0);
}
