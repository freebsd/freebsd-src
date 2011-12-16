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
#include <getopt.h>
#include <netinet/in.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>
#include <infiniband/complib/cl_nodenamemap.h>

#include "ibdiag_common.h"

#undef DEBUG
#define	DEBUG	if (verbose>1) IBWARN

static int dest_type = IB_DEST_LID;
static int verbose;

typedef char *(op_fn_t)(ib_portid_t *dest, char **argv, int argc);

typedef struct match_rec {
	char *name;
	op_fn_t *fn;
	unsigned opt_portnum;
} match_rec_t;

static op_fn_t	node_desc, node_info, port_info, switch_info, pkey_table,
	sl2vl_table, vlarb_table, guid_info;

static const match_rec_t match_tbl[] = {
	{ "nodeinfo", node_info },
	{ "nodedesc", node_desc },
	{ "portinfo", port_info, 1 },
	{ "switchinfo", switch_info },
	{ "pkeys", pkey_table, 1 },
	{ "sl2vl", sl2vl_table, 1 },
	{ "vlarb", vlarb_table, 1 },
	{ "guids", guid_info },
	{0}
};

char *argv0 = "smpquery";
static char *node_name_map_file = NULL;
static nn_map_t *node_name_map = NULL;

/*******************************************/
static char *
node_desc(ib_portid_t *dest, char **argv, int argc)
{
	int       node_type, l;
	uint64_t  node_guid;
	char      nd[IB_SMP_DATA_SIZE];
	uint8_t   data[IB_SMP_DATA_SIZE];
	char      dots[128];
	char     *nodename = NULL;

	if (!smp_query(data, dest, IB_ATTR_NODE_INFO, 0, 0))
		return "node info query failed";

	mad_decode_field(data, IB_NODE_TYPE_F, &node_type);
	mad_decode_field(data, IB_NODE_GUID_F, &node_guid);

	if (!smp_query(nd, dest, IB_ATTR_NODE_DESC, 0, 0))
		return "node desc query failed";

	nodename = remap_node_name(node_name_map, node_guid, nd);

	l = strlen(nodename);
	if (l < 32) {
		memset(dots, '.', 32 - l);
		dots[32 - l] = '\0';
	} else {
		dots[0] = '.';
		dots[1] = '\0';
	}

	printf("Node Description:%s%s\n", dots, nodename);
	free(nodename);
	return 0;
}

static char *
node_info(ib_portid_t *dest, char **argv, int argc)
{
	char buf[2048];
	char data[IB_SMP_DATA_SIZE];

	if (!smp_query(data, dest, IB_ATTR_NODE_INFO, 0, 0))
		return "node info query failed";

	mad_dump_nodeinfo(buf, sizeof buf, data, sizeof data);

	printf("# Node info: %s\n%s", portid2str(dest), buf);
	return 0;
}

static char *
port_info(ib_portid_t *dest, char **argv, int argc)
{
	char buf[2048];
	char data[IB_SMP_DATA_SIZE];
	int portnum = 0;

	if (argc > 0)
		portnum = strtol(argv[0], 0, 0);

	if (!smp_query(data, dest, IB_ATTR_PORT_INFO, portnum, 0))
		return "port info query failed";

	mad_dump_portinfo(buf, sizeof buf, data, sizeof data);

	printf("# Port info: %s port %d\n%s", portid2str(dest), portnum, buf);
	return 0;
}

static char *
switch_info(ib_portid_t *dest, char **argv, int argc)
{
	char buf[2048];
	char data[IB_SMP_DATA_SIZE];

	if (!smp_query(data, dest, IB_ATTR_SWITCH_INFO, 0, 0))
		return "switch info query failed";

	mad_dump_switchinfo(buf, sizeof buf, data, sizeof data);

	printf("# Switch info: %s\n%s", portid2str(dest), buf);
	return 0;
}

static char *
pkey_table(ib_portid_t *dest, char **argv, int argc)
{
	uint8_t data[IB_SMP_DATA_SIZE];
	uint32_t i, j, k;
	uint16_t *p;
	unsigned mod;
	int n, t, phy_ports;
	int portnum = 0;

	if (argc > 0)
		portnum = strtol(argv[0], 0, 0);

	/* Get the partition capacity */
	if (!smp_query(data, dest, IB_ATTR_NODE_INFO, 0, 0))
		return "node info query failed";

	mad_decode_field(data, IB_NODE_TYPE_F, &t);
	mad_decode_field(data, IB_NODE_NPORTS_F, &phy_ports);
	if (portnum > phy_ports)
		return "invalid port number";

	if ((t == IB_NODE_SWITCH) && (portnum != 0)) {
		if (!smp_query(data, dest, IB_ATTR_SWITCH_INFO, 0, 0))
			return "switch info failed";
		mad_decode_field(data, IB_SW_PARTITION_ENFORCE_CAP_F, &n);
	} else
		mad_decode_field(data, IB_NODE_PARTITION_CAP_F, &n);

	for (i = 0; i < (n + 31) / 32; i++) {
		mod =  i | (portnum << 16);
		if (!smp_query(data, dest, IB_ATTR_PKEY_TBL, mod, 0))
			return "pkey table query failed";
		if (i + 1 == (n + 31) / 32)
			k = ((n + 7 - i * 32) / 8) * 8;
		else
			k = 32;
		p = (uint16_t *) data;
		for (j = 0; j < k; j += 8, p += 8) {
			printf("%4u: 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x 0x%04x\n",
				(i * 32) + j,
				ntohs(p[0]), ntohs(p[1]),
				ntohs(p[2]), ntohs(p[3]),
				ntohs(p[4]), ntohs(p[5]),
				ntohs(p[6]), ntohs(p[7]));
		}
	}
	printf("%d pkeys capacity for this port\n", n);

	return 0;
}

static char *sl2vl_dump_table_entry(ib_portid_t *dest, int in, int out)
{
	char buf[2048];
	char data[IB_SMP_DATA_SIZE];
	int portnum = (in << 8) | out;

	if (!smp_query(data, dest, IB_ATTR_SLVL_TABLE, portnum, 0))
		return "slvl query failed";

	mad_dump_sltovl(buf, sizeof buf, data, sizeof data);
	printf("ports: in %2d, out %2d: ", in, out);
	printf("%s", buf);
	return 0;
}

static char *
sl2vl_table(ib_portid_t *dest, char **argv, int argc)
{
	uint8_t data[IB_SMP_DATA_SIZE];
	int type, num_ports, portnum = 0;
	int i;
	char *ret;

	if (argc > 0)
		portnum = strtol(argv[0], 0, 0);

	if (!smp_query(data, dest, IB_ATTR_NODE_INFO, 0, 0))
		return "node info query failed";

	mad_decode_field(data, IB_NODE_TYPE_F, &type);
	mad_decode_field(data, IB_NODE_NPORTS_F, &num_ports);
	if (portnum > num_ports)
		return "invalid port number";

	printf("# SL2VL table: %s\n", portid2str(dest));
	printf("#                 SL: |");
	for (i = 0 ; i < 16 ; i++)
		printf("%2d|", i);
	printf("\n");

	if (type != IB_NODE_SWITCH)
		return sl2vl_dump_table_entry(dest, 0, 0);

	for (i = 0 ; i <= num_ports ; i++) {
		ret = sl2vl_dump_table_entry(dest, i, portnum);
		if (ret)
			return ret;
	}
	return 0;
}

static char *vlarb_dump_table_entry(ib_portid_t *dest, int portnum, int offset, unsigned cap)
{
	char buf[2048];
	char data[IB_SMP_DATA_SIZE];

	if (!smp_query(data, dest, IB_ATTR_VL_ARBITRATION,
			(offset << 16) | portnum, 0))
		return "vl arb query failed";
	mad_dump_vlarbitration(buf, sizeof(buf), data, cap * 2);
	printf("%s", buf);
	return 0;
}

static char *vlarb_dump_table(ib_portid_t *dest, int portnum,
	char *name, int offset, int cap)
{
	char *ret;

	printf("# %s priority VL Arbitration Table:", name);
	ret = vlarb_dump_table_entry(dest, portnum, offset,
				     cap < 32 ? cap : 32);
	if (!ret && cap > 32)
		ret = vlarb_dump_table_entry(dest, portnum, offset + 1,
					     cap - 32);
	return ret;
}

static char *
vlarb_table(ib_portid_t *dest, char **argv, int argc)
{
	uint8_t data[IB_SMP_DATA_SIZE];
	int portnum = 0;
	int type, enhsp0, lowcap, highcap;
	char *ret = 0;

	if (argc > 0)
		portnum = strtol(argv[0], 0, 0);

	/* port number of 0 could mean SP0 or port MAD arrives on */
	if (portnum == 0) {
		if (!smp_query(data, dest, IB_ATTR_NODE_INFO, 0, 0))
			return "node info query failed";

		mad_decode_field(data, IB_NODE_TYPE_F, &type);
		if (type == IB_NODE_SWITCH) {
			if (!smp_query(data, dest, IB_ATTR_SWITCH_INFO, 0, 0))
				return "switch info query failed";
			mad_decode_field(data, IB_SW_ENHANCED_PORT0_F, &enhsp0);
			if (!enhsp0) {
				printf("# No VLArbitration tables (BSP0): %s port %d\n",
                        		portid2str(dest), 0);
				return 0;
			}
		}
	}

	if (!smp_query(data, dest, IB_ATTR_PORT_INFO, portnum, 0))
		return "port info query failed";

	mad_decode_field(data, IB_PORT_VL_ARBITRATION_LOW_CAP_F, &lowcap);
	mad_decode_field(data, IB_PORT_VL_ARBITRATION_HIGH_CAP_F,&highcap);

	printf("# VLArbitration tables: %s port %d LowCap %d HighCap %d\n",
			portid2str(dest), portnum, lowcap, highcap);

	if (lowcap > 0)
		ret = vlarb_dump_table(dest, portnum, "Low", 1, lowcap);

	if (!ret && highcap > 0)
		ret = vlarb_dump_table(dest, portnum, "High", 3, highcap);

	return ret;
}

static char *
guid_info(ib_portid_t *dest, char **argv, int argc)
{
	uint8_t data[IB_SMP_DATA_SIZE];
	uint32_t i, j, k;
	uint64_t *p;
	unsigned mod;
	int n;

	/* Get the guid capacity */
	if (!smp_query(data, dest, IB_ATTR_PORT_INFO, 0, 0))
		return "port info failed";
	mad_decode_field(data, IB_PORT_GUID_CAP_F, &n);

	for (i = 0; i < (n + 7) / 8; i++) {
		mod =  i;
		if (!smp_query(data, dest, IB_ATTR_GUID_INFO, mod, 0))
			return "guid info query failed";
		if (i + 1 == (n + 7) / 8)
			k = ((n + 1 - i * 8) / 2) * 2;
		else
			k = 8;
		p = (uint64_t *) data;
		for (j = 0; j < k; j += 2, p += 2) {
			printf("%4u: 0x%016"PRIx64" 0x%016"PRIx64"\n",
				(i * 8) + j,
				ntohll(p[0]), ntohll(p[1]));
		}
	}
	printf("%d guids capacity for this port\n", n);

	return 0;
}

static op_fn_t *
match_op(char *name)
{
	const match_rec_t *r;
	for (r = match_tbl; r->name; r++)
		if (!strcmp(r->name, name))
			return r->fn;
	return 0;
}

static void
usage(void)
{
	char *basename;
	const match_rec_t *r;

	if (!(basename = strrchr(argv0, '/')))
		basename = argv0;
	else
		basename++;

	fprintf(stderr, "Usage: %s [-d(ebug) -e(rr_show) -v(erbose) -D(irect) -G(uid) -s smlid -V(ersion) -C ca_name -P ca_port "
			"-t(imeout) timeout_ms --node-name-map node-name-map] <op> <dest dr_path|lid|guid> [op params]\n",
			basename);
	fprintf(stderr, "\tsupported ops:\n");
	for (r = match_tbl ; r->name ; r++) {
		fprintf(stderr, "\t\t%s <addr>%s\n", r->name,
				r->opt_portnum ? " [<portnum>]" : "");
	}
	fprintf(stderr, "\n\texamples:\n");
	fprintf(stderr, "\t\t%s portinfo 3 1\t\t\t\t# portinfo by lid, with port modifier\n", basename);
	fprintf(stderr, "\t\t%s -G switchinfo 0x2C9000100D051 1\t# switchinfo by guid\n", basename);
	fprintf(stderr, "\t\t%s -D nodeinfo 0\t\t\t\t# nodeinfo by direct route\n", basename);
	fprintf(stderr, "\t\t%s -c nodeinfo 6 0,12\t\t\t# nodeinfo by combined route\n", basename);
	exit(-1);
}

int
main(int argc, char **argv)
{
	int mgmt_classes[3] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS};
	ib_portid_t portid = {0};
	ib_portid_t *sm_id = 0, sm_portid = {0};
	extern int ibdebug;
	int timeout = 0, udebug = 0;
	char *ca = 0;
	int ca_port = 0;
	char *err;
	op_fn_t *fn;

	static char const str_opts[] = "C:P:t:s:devDcGVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "err_show", 0, 0, 'e'},
		{ "verbose", 0, 0, 'v'},
		{ "Direct", 0, 0, 'D'},
		{ "combined", 0, 0, 'c'},
		{ "Guid", 0, 0, 'G'},
		{ "smlid", 1, 0, 's'},
		{ "timeout", 1, 0, 't'},
		{ "node-name-map", 1, 0, 1},
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
		case 1:
			node_name_map_file = strdup(optarg);
			break;
		case 'd':
			ibdebug++;
			madrpc_show_errors(1);
			umad_debug(udebug);
			udebug++;
			break;
		case 'e':
			madrpc_show_errors(1);
			break;
		case 'D':
			dest_type = IB_DEST_DRPATH;
			break;
		case 'c':
			dest_type = IB_DEST_DRSLID;
			break;
		case 'G':
			dest_type = IB_DEST_GUID;
			break;
		case 'C':
			ca = optarg;
			break;
		case 'P':
			ca_port = strtoul(optarg, 0, 0);
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

	if (argc < 2)
		usage();

	if (!(fn = match_op(argv[0])))
		IBERROR("operation '%s' not supported", argv[0]);

	madrpc_init(ca, ca_port, mgmt_classes, 3);
	node_name_map = open_node_name_map(node_name_map_file);

	if (dest_type != IB_DEST_DRSLID) {
		if (ib_resolve_portid_str(&portid, argv[1], dest_type, sm_id) < 0)
			IBERROR("can't resolve destination port %s", argv[1]);
		if ((err = fn(&portid, argv+2, argc-2)))
			IBERROR("operation %s: %s", argv[0], err);
	} else {
		char concat[64];

		memset(concat, 0, 64);
		snprintf(concat, sizeof(concat), "%s %s", argv[1], argv[2]);
		if (ib_resolve_portid_str(&portid, concat, dest_type, sm_id) < 0)
			IBERROR("can't resolve destination port %s", concat);
		if ((err = fn(&portid, argv+3, argc-3)))
			IBERROR("operation %s: %s", argv[0], err);
	}
	close_node_name_map(node_name_map);
	exit(0);
}
