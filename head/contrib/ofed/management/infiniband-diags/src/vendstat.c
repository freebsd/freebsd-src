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
#include <netinet/in.h>

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>

#include "ibdiag_common.h"

#define IS3_DEVICE_ID			47396

#define IB_MLX_VENDOR_CLASS		10
/* Vendor specific Attribute IDs */
#define IB_MLX_IS3_GENERAL_INFO		0x17
#define IB_MLX_IS3_CONFIG_SPACE_ACCESS	0x50
/* Config space addresses */
#define IB_MLX_IS3_PORT_XMIT_WAIT	0x10013C

char *argv0 = "vendstat";

typedef struct {
	uint16_t hw_revision;
	uint16_t device_id;
	uint8_t  reserved[24];
	uint32_t uptime;
} is3_hw_info_t;

typedef struct {
	uint8_t resv1;
	uint8_t major;
	uint8_t minor;
	uint8_t sub_minor;
	uint32_t build_id;
	uint8_t month;
	uint8_t day;
	uint16_t year;
	uint16_t resv2;
	uint16_t hour;
	uint8_t psid[16];
	uint32_t ini_file_version;
} is3_fw_info_t;

typedef struct {
	uint8_t resv1;
	uint8_t major;
	uint8_t minor;
	uint8_t sub_minor;
	uint8_t resv2[28];
} is3_sw_info_t;

typedef struct {
	uint8_t       reserved[8];
	is3_hw_info_t hw_info;
	is3_fw_info_t fw_info;
	is3_sw_info_t sw_info;
} is3_general_info_t;

typedef struct {
	uint32_t address;
	uint32_t data;
	uint32_t mask;
} is3_record_t;

typedef struct {
	uint8_t      reserved[8];
	is3_record_t record[18];
} is3_config_space_t;

static void
usage(void)
{
	char *basename;

	if (!(basename = strrchr(argv0, '/')))
		basename = argv0;
	else
		basename++;

	fprintf(stderr, "Usage: %s [-d(ebug) -N -w -G(uid) -C ca_name -P ca_port "
			"-t(imeout) timeout_ms -V(ersion) -h(elp)] <lid|guid>\n",
			basename);
	fprintf(stderr, "\tExamples:\n");
	fprintf(stderr, "\t\t%s -N 6\t\t# read IS3 general information\n", basename);
	fprintf(stderr, "\t\t%s -w 6\t\t# read IS3 port xmit wait counters\n", basename);
	exit(-1);
}

int
main(int argc, char **argv)
{
	int mgmt_classes[4] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS, IB_MLX_VENDOR_CLASS};
	ib_portid_t *sm_id = 0, sm_portid = {0};
	ib_portid_t portid = {0};
	extern int ibdebug;
	int dest_type = IB_DEST_LID;
	int timeout = 0;	/* use default */
	int port = 0;
	char buf[1024];
	int udebug = 0;
	char *ca = 0;
	int ca_port = 0;
	ib_vendor_call_t call;
	is3_general_info_t *gi;
	is3_config_space_t *cs;
	int general_info = 0;
	int xmit_wait = 0;
	int i;

	static char const str_opts[] = "C:P:s:t:dNwGVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "N", 1, 0, 'N'},
		{ "w", 1, 0, 'w'},
		{ "debug", 0, 0, 'd'},
		{ "Guid", 0, 0, 'G'},
		{ "sm_portid", 1, 0, 's'},
		{ "timeout", 1, 0, 't'},
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
		case 'N':
			general_info = 1;
			break;
		case 'w':
			xmit_wait = 1;
			break;
		case 'd':
			ibdebug++;
			madrpc_show_errors(1);
			umad_debug(udebug);
			udebug++;
			break;
		case 'G':
			dest_type = IB_DEST_GUID;
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

	madrpc_init(ca, ca_port, mgmt_classes, 4);

	if (argc) {
		if (ib_resolve_portid_str(&portid, argv[0], dest_type, sm_id) < 0)
			IBERROR("can't resolve destination port %s", argv[0]);
	} else {
		if (ib_resolve_self(&portid, &port, 0) < 0)
			IBERROR("can't resolve self port %s", argv[0]);
	}

	/* Only General Info and Port Xmit Wait Counters */
	/* queries are currently supported */
	if (!general_info && !xmit_wait)
		IBERROR("at least one of -N and -w must be specified");

	/* These are Mellanox specific vendor MADs */
	/* but vendors change the VendorId so how know for sure ? */
	/* Would need a list of these and it might not be complete */
	/* so for right now, punt on this */

	memset(&call, 0, sizeof(call));
	call.mgmt_class = IB_MLX_VENDOR_CLASS;
	call.method = IB_MAD_METHOD_GET;
	call.timeout = timeout;

	memset(&buf, 0, sizeof(buf));
	/* vendor ClassPortInfo is required attribute if class supported */
	call.attrid = CLASS_PORT_INFO;
	if (!ib_vendor_call(&buf, &portid, &call))
		IBERROR("classportinfo query");

	memset(&buf, 0, sizeof(buf));
	call.attrid = IB_MLX_IS3_GENERAL_INFO;
	if (!ib_vendor_call(&buf, &portid, &call))
		IBERROR("vendstat");
	gi = (is3_general_info_t *)&buf;

	if (general_info) {
		/* dump IS3 general info here */
		printf("hw_dev_rev:  0x%04x\n", ntohs(gi->hw_info.hw_revision));
		printf("hw_dev_id:   0x%04x\n", ntohs(gi->hw_info.device_id));
		printf("hw_uptime:   0x%08x\n", ntohl(gi->hw_info.uptime));
		printf("fw_version:  %02d.%02d.%02d\n",
		       gi->fw_info.major, gi->fw_info.minor, gi->fw_info.sub_minor);
		printf("fw_build_id: 0x%04x\n", ntohl(gi->fw_info.build_id));
		printf("fw_date:     %02d/%02d/%04x\n",
		       gi->fw_info.month, gi->fw_info.day, ntohs(gi->fw_info.year));
		printf("fw_psid:     '%s'\n", gi->fw_info.psid);
		printf("fw_ini_ver:  %d\n", ntohl(gi->fw_info.ini_file_version));
		printf("sw_version:  %02d.%02d.%02d\n",
		       gi->sw_info.major, gi->sw_info.minor, gi->sw_info.sub_minor);
	}

	if (xmit_wait) {
		if (ntohs(gi->hw_info.device_id) != IS3_DEVICE_ID)
			IBERROR("Unsupported device ID 0x%x", ntohs(gi->hw_info.device_id));

		memset(&buf, 0, sizeof(buf));
		call.attrid = IB_MLX_IS3_CONFIG_SPACE_ACCESS;
		/* Limit of 18 accesses per MAD ? */
		call.mod = 2 << 22 | 16 << 16; /* 16 records */
		/* Set record addresses for each port */
		cs = (is3_config_space_t *)&buf;
		for (i = 0; i < 16; i++)
			cs->record[i].address = htonl(IB_MLX_IS3_PORT_XMIT_WAIT + ((i + 1) << 12));
		if (!ib_vendor_call(&buf, &portid, &call))
			IBERROR("vendstat");

		for (i = 0; i < 16; i++)
			if (cs->record[i].data)	/* PortXmitWait is 32 bit counter */
				printf("Port %d: PortXmitWait 0x%x\n", i + 4, ntohl(cs->record[i].data)); /* port 4 is first port */

		/* Last 8 ports is another query */
		memset(&buf, 0, sizeof(buf));
		call.attrid = IB_MLX_IS3_CONFIG_SPACE_ACCESS;
                call.mod = 2 << 22 | 8 << 16; /* 8 records */
		/* Set record addresses for each port */
		cs = (is3_config_space_t *)&buf;
		for (i = 0; i < 8; i++)
			cs->record[i].address = htonl(IB_MLX_IS3_PORT_XMIT_WAIT + ((i + 17) << 12));
		if (!ib_vendor_call(&buf, &portid, &call))
			IBERROR("vendstat");

		for (i = 0; i < 8; i++)
			if (cs->record[i].data) /* PortXmitWait is 32 bit counter */
				printf("Port %d: PortXmitWait 0x%x\n",
				       i < 4 ? i + 21 : i - 3,
				       ntohl(cs->record[i].data));
	}

	exit(0);
}
