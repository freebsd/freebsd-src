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
#include <time.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <netinet/in.h>
#include <ctype.h>

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>
#include <infiniband/complib/cl_nodenamemap.h>

#include "ibdiag_common.h"

static int dest_type = IB_DEST_LID;
static int brief;
static int verbose;
static int dump_all;

char *argv0 = "ibroute";

/*******************************************/

char *
check_switch(ib_portid_t *portid, int *nports, uint64_t *guid,
	     uint8_t *sw, char *nd)
{
	uint8_t ni[IB_SMP_DATA_SIZE] = {0};
	int type;

	DEBUG("checking node type");
	if (!smp_query(ni, portid, IB_ATTR_NODE_INFO, 0, 0)) {
		xdump(stderr, "nodeinfo\n", ni, sizeof ni);
		return "node info failed: valid addr?";
	}

	if (!smp_query(nd, portid, IB_ATTR_NODE_DESC, 0, 0))
		return "node desc failed";

	mad_decode_field(ni, IB_NODE_TYPE_F, &type);
	if (type != IB_NODE_SWITCH)
		return "not a switch";

	DEBUG("Gathering information about switch");
	mad_decode_field(ni, IB_NODE_NPORTS_F, nports);
	mad_decode_field(ni, IB_NODE_GUID_F, guid);

	if (!smp_query(sw, portid, IB_ATTR_SWITCH_INFO, 0, 0))
		return "switch info failed: is a switch node?";

	return 0;
}

#define IB_MLIDS_IN_BLOCK	(IB_SMP_DATA_SIZE/2)

int
dump_mlid(char *str, int strlen, int mlid, int nports,
	  uint16_t mft[16][IB_MLIDS_IN_BLOCK])
{
	uint16_t mask;
	int i, chunk, bit;
	int nonzero = 0;

	if (brief) {
		int n = 0, chunks = ALIGN(nports + 1, 16) / 16;
		for (i = 0; i < chunks; i++) {
			mask = ntohs(mft[i][mlid%IB_MLIDS_IN_BLOCK]);
			if (mask)
				nonzero++;
			n += snprintf(str + n, strlen - n, "%04hx", mask);
			if (n >= strlen) {
				n = strlen;
				break;
			}
		}
		if (!nonzero && !dump_all) {
			str[0] = 0;
			return 0;
		}
		return n;
	}
	for (i = 0; i <= nports; i++) {
		chunk = i / 16;
		bit = i % 16;

		mask = ntohs(mft[chunk][mlid%IB_MLIDS_IN_BLOCK]);
		if (mask)
			nonzero++;
		str[i*2] = (mask & (1 << bit)) ? 'x' : ' ';
		str[i*2+1] = ' ';
	}
	if (!nonzero && !dump_all) {
		str[0] = 0;
		return 0;
	}
	str[i*2] = 0;
	return i * 2;
}

uint16_t mft[16][IB_MLIDS_IN_BLOCK];

char *
dump_multicast_tables(ib_portid_t *portid, int startlid, int endlid)
{
	char nd[IB_SMP_DATA_SIZE] = {0};
	uint8_t sw[IB_SMP_DATA_SIZE] = {0};
	char str[512];
	char *s;
	uint64_t nodeguid;
	uint32_t mod;
	int block, i, j, e, nports, cap, chunks;
	int n = 0, startblock, lastblock;

	if ((s = check_switch(portid, &nports, &nodeguid, sw, nd)))
		return s;

	mad_decode_field(sw, IB_SW_MCAST_FDB_CAP_F, &cap);

	if (!endlid || endlid > IB_MIN_MCAST_LID + cap - 1)
		endlid = IB_MIN_MCAST_LID + cap - 1;

	if (!startlid)
		startlid = IB_MIN_MCAST_LID;

	if (startlid < IB_MIN_MCAST_LID) {
		IBWARN("illegal start mlid %x, set to %x", startlid, IB_MIN_MCAST_LID);
		startlid = IB_MIN_MCAST_LID;
	}

	if (endlid > IB_MAX_MCAST_LID) {
		IBWARN("illegal end mlid %x, truncate to %x", endlid, IB_MAX_MCAST_LID);
		endlid = IB_MAX_MCAST_LID;
	}

	printf("Multicast mlids [0x%x-0x%x] of switch %s guid 0x%016" PRIx64 " (%s):\n",
		startlid, endlid, portid2str(portid), nodeguid, clean_nodedesc(nd));

	if (brief)
		printf(" MLid       Port Mask\n");
	else {
		if (nports > 9) {
			for (i = 0, s = str; i <= nports; i++) {
				*s++ = (i%10) ? ' ' : '0' + i/10;
				*s++ = ' ';
			}
			*s = 0;
			printf("            %s\n", str);
		}
		for (i = 0, s = str; i <= nports; i++)
			s += sprintf(s, "%d ", i%10);
		printf("     Ports: %s\n", str);
		printf(" MLid\n");
	}
	if (verbose)
		printf("Switch muticast mlids capability is 0x%d\n", cap);

	chunks = ALIGN(nports + 1, 16) / 16;

	startblock = startlid / IB_MLIDS_IN_BLOCK;
	lastblock = endlid / IB_MLIDS_IN_BLOCK;
	for (block = startblock; block <= lastblock; block++) {
		for (j = 0; j < chunks; j++) {
			mod = (block - IB_MIN_MCAST_LID/IB_MLIDS_IN_BLOCK) | (j << 28);

			DEBUG("reading block %x chunk %d mod %x", block, j, mod);
			if (!smp_query(mft + j, portid, IB_ATTR_MULTICASTFORWTBL, mod, 0))
				return "multicast forwarding table get failed";
		}

		i = block * IB_MLIDS_IN_BLOCK;
		e = i + IB_MLIDS_IN_BLOCK;
		if (i < startlid)
			i = startlid;
		if (e > endlid + 1)
			e = endlid + 1;

		for (; i < e; i++) {
			if (dump_mlid(str, sizeof str, i, nports, mft) == 0)
				continue;
			printf("0x%04x      %s\n", i, str);
			n++;
		}
	}

	printf("%d %smlids dumped \n", n, dump_all ? "" : "valid ");
	return 0;
}

int
dump_lid(char *str, int strlen, int lid, int valid)
{
	char nd[IB_SMP_DATA_SIZE] = {0};
	uint8_t ni[IB_SMP_DATA_SIZE] = {0};
	uint8_t pi[IB_SMP_DATA_SIZE] = {0};
	ib_portid_t lidport = {0};
	static int last_port_lid, base_port_lid;
	char ntype[50], sguid[30], desc[64];
	static uint64_t portguid;
	int baselid, lmc, type;

	if (brief) {
		str[0] = 0;
		return 0;
	}

	if (lid <= last_port_lid) {
		if (!valid)
			return snprintf(str, strlen, ": (path #%d - illegal port)",
					lid - base_port_lid);
		else if (!portguid)
			return snprintf(str, strlen,
					": (path #%d out of %d)",
					lid - base_port_lid + 1,
					last_port_lid - base_port_lid + 1);
		else {
			return snprintf(str, strlen,
					": (path #%d out of %d: portguid %s)",
					lid - base_port_lid + 1,
					last_port_lid - base_port_lid + 1,
					mad_dump_val(IB_NODE_PORT_GUID_F, sguid, sizeof sguid, &portguid));
		}
	}

	if (!valid)
		return snprintf(str, strlen, ": (illegal port)");

	portguid = 0;
	lidport.lid = lid;

	if (!smp_query(nd, &lidport, IB_ATTR_NODE_DESC, 0, 100) ||
	    !smp_query(pi, &lidport, IB_ATTR_PORT_INFO, 0, 100) ||
	    !smp_query(ni, &lidport, IB_ATTR_NODE_INFO, 0, 100))
		return snprintf(str, strlen, ": (unknown node and type)");

	mad_decode_field(ni, IB_NODE_PORT_GUID_F, &portguid);
	mad_decode_field(ni, IB_NODE_TYPE_F, &type);

	mad_decode_field(pi, IB_PORT_LID_F, &baselid);
	mad_decode_field(pi, IB_PORT_LMC_F, &lmc);

	if (lmc > 0) {
		base_port_lid = baselid;
		last_port_lid = baselid + (1 << lmc) - 1;
	}

	return snprintf(str, strlen, ": (%s portguid %s: %s)",
		mad_dump_val(IB_NODE_TYPE_F, ntype, sizeof ntype, &type),
		mad_dump_val(IB_NODE_PORT_GUID_F, sguid, sizeof sguid, &portguid),
		mad_dump_val(IB_NODE_DESC_F, desc, sizeof desc, clean_nodedesc(nd)));
}

char *
dump_unicast_tables(ib_portid_t *portid, int startlid, int endlid)
{
	char lft[IB_SMP_DATA_SIZE];
	char nd[IB_SMP_DATA_SIZE];
	uint8_t sw[IB_SMP_DATA_SIZE];
	char str[200], *s;
	uint64_t nodeguid;
	int block, i, e, nports, top;
	int n = 0, startblock, endblock;

	if ((s = check_switch(portid, &nports, &nodeguid, sw, nd)))
		return s;

	mad_decode_field(sw, IB_SW_LINEAR_FDB_TOP_F, &top);

	if (!endlid || endlid > top)
		endlid = top;

	if (endlid > IB_MAX_UCAST_LID) {
		IBWARN("ilegal lft top %d, truncate to %d", endlid, IB_MAX_UCAST_LID);
		endlid = IB_MAX_UCAST_LID;
	}

	printf("Unicast lids [0x%x-0x%x] of switch %s guid 0x%016" PRIx64 " (%s):\n",
		startlid, endlid, portid2str(portid), nodeguid, clean_nodedesc(nd));

	DEBUG("Switch top is 0x%x\n", top);

	printf("  Lid  Out   Destination\n");
	printf("       Port     Info \n");
	startblock = startlid / IB_SMP_DATA_SIZE;
	endblock = ALIGN(endlid, IB_SMP_DATA_SIZE) / IB_SMP_DATA_SIZE;
	for (block = startblock; block <= endblock; block++) {
		DEBUG("reading block %d", block);
		if (!smp_query(lft, portid, IB_ATTR_LINEARFORWTBL, block, 0))
			return "linear forwarding table get failed";
		i = block * IB_SMP_DATA_SIZE;
		e = i + IB_SMP_DATA_SIZE;
		if (i < startlid)
			i = startlid;
		if (e > endlid + 1)
			e = endlid + 1;

		for (;i < e; i++) {
			unsigned outport = lft[i % IB_SMP_DATA_SIZE];
			unsigned valid = (outport <= nports);

			if (!valid && !dump_all)
				continue;
			dump_lid(str, sizeof str, i, valid);
			printf("0x%04x %03u %s\n", i, outport & 0xff, str);
			n++;
		}
	}

	printf("%d %slids dumped \n", n, dump_all ? "" : "valid ");
	return 0;
}

void
usage(void)
{
	char *basename;

	if (!(basename = strrchr(argv0, '/')))
		basename = argv0;
	else
		basename++;

	fprintf(stderr, "Usage: %s [-d(ebug)] -a(ll) -n(o_dests) -v(erbose) -D(irect) -G(uid) -M(ulticast) -s smlid -V(ersion) -C ca_name -P ca_port "
			"-t(imeout) timeout_ms] [<dest dr_path|lid|guid> [<startlid> [<endlid>]]]\n",
			basename);
	fprintf(stderr, "\n\tUnicast examples:\n");
	fprintf(stderr, "\t\t%s 4\t# dump all lids with valid out ports of switch with lid 4\n", basename);
	fprintf(stderr, "\t\t%s -a 4\t# same, but dump all lids, even with invalid out ports\n", basename);
	fprintf(stderr, "\t\t%s -n 4\t# simple dump format - no destination resolving\n", basename);
	fprintf(stderr, "\t\t%s 4 10\t# dump lids starting from 10\n", basename);
	fprintf(stderr, "\t\t%s 4 0x10 0x20\t# dump lid range\n", basename);
	fprintf(stderr, "\t\t%s -G 0x08f1040023\t# resolve switch by GUID\n", basename);
	fprintf(stderr, "\t\t%s -D 0,1\t# resolve switch by direct path\n", basename);

	fprintf(stderr, "\n\tMulticast examples:\n");
	fprintf(stderr, "\t\t%s -M 4\t# dump all non empty mlids of switch with lid 4\n", basename);
	fprintf(stderr, "\t\t%s -M 4 0xc010 0xc020\t# same, but with range\n", basename);
	fprintf(stderr, "\t\t%s -M -n 4\t# simple dump format\n", basename);
	exit(-1);
}

int
main(int argc, char **argv)
{
	int mgmt_classes[3] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS};
	ib_portid_t portid = {0};
	ib_portid_t *sm_id = 0, sm_portid = {0};
	int timeout;
	int multicast = 0, startlid = 0, endlid = 0;
	char *err;
	char *ca = 0;
	int ca_port = 0;

	static char const str_opts[] = "C:P:t:s:danvDGMVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "all", 0, 0, 'a'},
		{ "no_dests", 0, 0, 'n'},
		{ "verbose", 0, 0, 'v'},
		{ "Direct", 0, 0, 'D'},
		{ "Guid", 0, 0, 'G'},
		{ "Multicast", 0, 0, 'M'},
		{ "timeout", 1, 0, 't'},
		{ "s", 1, 0, 's'},
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
		case 'a':
			dump_all++;
			break;
		case 'd':
			ibdebug++;
			break;
		case 'D':
			dest_type = IB_DEST_DRPATH;
			break;
		case 'G':
			dest_type = IB_DEST_GUID;
			break;
		case 'M':
			multicast++;
			break;
		case 'n':
			brief++;
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
		case 'v':
			madrpc_show_errors(1);
			verbose++;
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

	if (!argc)
		usage();

	if (argc > 1)
		startlid = strtoul(argv[1], 0, 0);
	if (argc > 2)
		endlid = strtoul(argv[2], 0, 0);

	madrpc_init(ca, ca_port, mgmt_classes, 3);

	if (!argc) {
		if (ib_resolve_self(&portid, 0, 0) < 0)
			IBERROR("can't resolve self addr");
	} else {
		if (ib_resolve_portid_str(&portid, argv[0], dest_type, sm_id) < 0)
			IBERROR("can't resolve destination port %s", argv[1]);
	}

	if (multicast)
		err = dump_multicast_tables(&portid, startlid, endlid);
	else
		err = dump_unicast_tables(&portid, startlid, endlid);

	if (err)
		IBERROR("dump tables: %s", err);

	exit(0);
}
