/*
 * Copyright (c) 2008 Lawrence Livermore National Security
 *
 * Produced at Lawrence Livermore National Laboratory.
 * Written by Ira Weiny <weiny2@llnl.gov>.
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define _GNU_SOURCE
#include <getopt.h>

#include <infiniband/mad.h>
#include <infiniband/iba/ib_types.h>

#include "ibdiag_common.h"

char *argv0 = "";

static int send_144_node_desc_update(void)
{
	ib_portid_t sm_port;
	ib_portid_t selfportid;
	int selfport;
	ib_rpc_t trap_rpc;
	ib_mad_notice_attr_t notice;

	if (ib_resolve_self(&selfportid, &selfport, NULL))
		IBERROR("can't resolve self");

	if (ib_resolve_smlid(&sm_port, 0))
		IBERROR("can't resolve SM destination port");

	memset(&trap_rpc, 0, sizeof(trap_rpc));
	trap_rpc.mgtclass = IB_SMI_CLASS;
	trap_rpc.method = IB_MAD_METHOD_TRAP;
	trap_rpc.trid = mad_trid();
	trap_rpc.attr.id = NOTICE;
	trap_rpc.datasz = IB_SMP_DATA_SIZE;
	trap_rpc.dataoffs = IB_SMP_DATA_OFFS;

	memset(&notice, 0, sizeof(notice));
	notice.generic_type = 0x80 | IB_NOTICE_TYPE_INFO;
	notice.g_or_v.generic.prod_type_lsb = cl_hton16(IB_NODE_TYPE_CA);
	notice.g_or_v.generic.trap_num = cl_hton16(144);
	notice.issuer_lid = cl_hton16(selfportid.lid);
	notice.data_details.ntc_144.lid = cl_hton16(selfportid.lid);
	notice.data_details.ntc_144.local_changes =
	    TRAP_144_MASK_OTHER_LOCAL_CHANGES;
	notice.data_details.ntc_144.change_flgs =
	    TRAP_144_MASK_NODE_DESCRIPTION_CHANGE;

	return (mad_send(&trap_rpc, &sm_port, NULL, &notice));
}

typedef struct _trap_def {
	char *trap_name;
	int (*send_func) (void);
} trap_def_t;

trap_def_t traps[2] = {
	{"node_desc_change", send_144_node_desc_update},
	{NULL, NULL}
};

static void usage(void)
{
	int i;

	fprintf(stderr, "Usage: %s [-hV]"
		" [-C <ca_name>] [-P <ca_port>] [<trap_name>]\n", argv0);
	fprintf(stderr, "   -V print version\n");
	fprintf(stderr, "   <trap_name> can be one of the following\n");
	for (i = 0; traps[i].trap_name; i++) {
		fprintf(stderr, "      %s\n", traps[i].trap_name);
	}
	fprintf(stderr, "   default behavior is to send \"%s\"\n",
		traps[0].trap_name);

	exit(-1);
}

int send_trap(char *trap_name)
{
	int i;

	for (i = 0; traps[i].trap_name; i++) {
		if (strcmp(traps[i].trap_name, trap_name) == 0) {
			return (traps[i].send_func());
		}
	}
	usage();
	exit(1);
}

int main(int argc, char **argv)
{
	int mgmt_classes[2] = { IB_SMI_CLASS, IB_SMI_DIRECT_CLASS };
	int ch = 0;
	char *trap_name = NULL;
	char *ca = NULL;
	int ca_port = 0;

	static char const str_opts[] = "hVP:C:";
	static const struct option long_opts[] = {
		{"Version", 0, 0, 'V'},
		{"P", 1, 0, 'P'},
		{"C", 1, 0, 'C'},
		{"help", 0, 0, 'h'},
		{}
	};

	argv0 = argv[0];

	while ((ch = getopt_long(argc, argv, str_opts, long_opts, NULL)) != -1) {
		switch (ch) {
		case 'V':
			fprintf(stderr, "%s %s\n", argv0, get_build_version());
			exit(-1);
		case 'C':
			ca = optarg;
			break;
		case 'P':
			ca_port = strtoul(optarg, NULL, 0);
			break;
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!argv[0]) {
		trap_name = traps[0].trap_name;
	} else {
		trap_name = argv[0];
	}

	madrpc_show_errors(1);
	madrpc_init(ca, ca_port, mgmt_classes, 2);

	return (send_trap(trap_name));
}
