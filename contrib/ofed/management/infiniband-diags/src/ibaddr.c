/*
 * Copyright (c) 2004-2008 Voltaire Inc.  All rights reserved.
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

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <arpa/inet.h>

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>

#include <sys/socket.h>

#include "ibdiag_common.h"

char *argv0 = "ibaddr";

static int
ib_resolve_addr(ib_portid_t *portid, int portnum, int show_lid, int show_gid)
{
	char   gid_str[INET6_ADDRSTRLEN];
	uint8_t portinfo[64];
	uint8_t nodeinfo[64];
	uint64_t guid, prefix;
	ibmad_gid_t gid;
	int lmc;

	if (!smp_query(nodeinfo, portid, IB_ATTR_NODE_INFO, 0, 0))
		return -1;

	if (!smp_query(portinfo, portid, IB_ATTR_PORT_INFO, portnum, 0))
		return -1;

	mad_decode_field(portinfo, IB_PORT_LID_F, &portid->lid);
	mad_decode_field(portinfo, IB_PORT_GID_PREFIX_F, &prefix);
	mad_decode_field(portinfo, IB_PORT_LMC_F, &lmc);
	mad_decode_field(nodeinfo, IB_NODE_PORT_GUID_F, &guid);

	mad_encode_field(gid, IB_GID_PREFIX_F, &prefix);
	mad_encode_field(gid, IB_GID_GUID_F, &guid);

	if (show_gid) {
		printf("GID %s ", inet_ntop(AF_INET6, gid, gid_str,
			sizeof gid_str));
	}

	if (show_lid > 0)
		printf("LID start 0x%x end 0x%x", portid->lid, portid->lid + (1 << lmc) - 1);
	else if (show_lid < 0)
		printf("LID start %d end %d", portid->lid, portid->lid + (1 << lmc) - 1);
	printf("\n");
	return 0;
}

static void
usage(void)
{
	char *basename;

	if (!(basename = strrchr(argv0, '/')))
		basename = argv0;
	else
		basename++;

	fprintf(stderr, "Usage: %s [-d(ebug) -D(irect) -G(uid) -l(id_show) -g(id_show) -s(m_port) sm_lid -C ca_name -P ca_port "
			"-t(imeout) timeout_ms -V(ersion) -h(elp)] [<lid|dr_path|guid>]\n",
			basename);
	fprintf(stderr, "\tExamples:\n");
	fprintf(stderr, "\t\t%s\t\t\t# local port's address\n", basename);
	fprintf(stderr, "\t\t%s 32\t\t# show lid range and gid of lid 32\n", basename);
	fprintf(stderr, "\t\t%s -G 0x8f1040023\t# same but using guid address\n", basename);
	fprintf(stderr, "\t\t%s -l 32\t\t# show lid range only\n", basename);
	fprintf(stderr, "\t\t%s -L 32\t\t# show decimal lid range only\n", basename);
	fprintf(stderr, "\t\t%s -g 32\t\t# show gid address only\n", basename);
	exit(-1);
}

int
main(int argc, char **argv)
{
	int mgmt_classes[3] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS};
	ib_portid_t *sm_id = 0, sm_portid = {0};
	ib_portid_t portid = {0};
	extern int ibdebug;
	int dest_type = IB_DEST_LID;
	int timeout = 0;	/* use default */
	int show_lid = 0, show_gid = 0;
	int port = 0;
	char *ca = 0;
	int ca_port = 0;

	static char const str_opts[] = "C:P:t:s:dDGglLVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "Direct", 0, 0, 'D'},
		{ "Guid", 0, 0, 'G'},
		{ "gid_show", 0, 0, 'g'},
		{ "lid_show", 0, 0, 'l'},
		{ "Lid_show", 0, 0, 'L'},
		{ "timeout", 1, 0, 't'},
		{ "sm_port", 1, 0, 's'},
		{ "Version", 0, 0, 'V'},
		{ "help", 0, 0, 'h'},
		{ "usage", 0, 0, 'u'},
		{ }
	};

	argv0 = argv[0];

	while (1) {
		int ch = getopt_long(argc, argv, str_opts, long_opts, NULL);
		if ( ch == -1 )
			break;
		switch(ch) {
		case 'C':
			ca = optarg;
			break;
		case 'P':
			ca_port = strtoul(optarg, 0, 0);
			break;
		case 'd':
			ibdebug++;
			break;
		case 'D':
			dest_type = IB_DEST_DRPATH;
			break;
		case 'g':
			show_gid++;
			break;
		case 'G':
			dest_type = IB_DEST_GUID;
			break;
		case 'l':
			show_lid++;
			break;
		case 'L':
			show_lid = -100;
			break;
		case 's':
			if (ib_resolve_portid_str(&sm_portid, optarg, IB_DEST_LID, 0) < 0)
				IBERROR("can't resolve SM destination port %s", optarg);
			sm_id = &sm_portid;
			break;
		case 't':
			timeout = strtoul(optarg, 0, 0);
			madrpc_set_timeout(timeout);
			break;
		case 'V':
			fprintf(stderr, "%s %s\n", argv0, get_build_version() );
			exit(-1);
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		port = strtoul(argv[1], 0, 0);

	if (!show_lid && !show_gid)
		show_lid = show_gid = 1;

	madrpc_init(ca, ca_port, mgmt_classes, 3);

	if (argc) {
		if (ib_resolve_portid_str(&portid, argv[0], dest_type, sm_id) < 0)
			IBERROR("can't resolve destination port %s", argv[0]);
	} else {
		if (ib_resolve_self(&portid, &port, 0) < 0)
			IBERROR("can't resolve self port %s", argv[0]);
	}

	if (ib_resolve_addr(&portid, port, show_lid, show_gid) < 0)
		IBERROR("can't resolve requested address");
	exit(0);
}
