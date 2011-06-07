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

#define _GNU_SOURCE

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <getopt.h>
#include <endian.h>
#include <byteswap.h>
#include <sys/poll.h>
#include <syslog.h>
#include <netinet/in.h>

#include <infiniband/common.h>
#include <infiniband/mad.h>
#include <infiniband/umad.h>

#include <ibdiag_common.h>

static const uint8_t  CLASS_SUBN_DIRECTED_ROUTE = 0x81;
static const uint8_t  CLASS_SUBN_LID_ROUTE = 0x1;

#define  ATTR_NODE_DESC ((uint16_t)(htons(0x10)))
#define  ATTR_NODE_INFO ((uint16_t)(htons(0x11)))
#define  ATTR_PORT_INFO ((uint16_t)(htons(0x15)))

static int mad_agent;
static int drmad_tid = 0x123;

static int debug, verbose;

char *argv0 = "smpdump";

typedef struct {
	char path[64];
	int hop_cnt;
} DRPath;

struct drsmp {
	uint8_t		base_version;
	uint8_t		mgmt_class;
	uint8_t		class_version;
	uint8_t		method;
	uint16_t	status;
	uint8_t		hop_ptr;
	uint8_t		hop_cnt;
	uint64_t	tid;
	uint16_t	attr_id;
	uint16_t	resv;
	uint32_t	attr_mod;
	uint64_t	mkey;
	uint16_t	dr_slid;
	uint16_t	dr_dlid;
	uint8_t		reserved[28];
	uint8_t		data[64];
	uint8_t		initial_path[64];
	uint8_t		return_path[64];
};

void
drsmp_get_init(void *umad, DRPath *path, int attr, int mod)
{
	struct drsmp *smp = (struct drsmp *)(umad_get_mad(umad));

	memset(smp, 0, sizeof (*smp));

	smp->base_version  = 1;
	smp->mgmt_class    = CLASS_SUBN_DIRECTED_ROUTE;
	smp->class_version = 1;

	smp->method        = 1;
	smp->attr_id	   = (uint16_t)htons((uint16_t)attr);
	smp->attr_mod	   = htonl(mod);
	smp->tid           = htonll(drmad_tid++);
	smp->dr_slid       = 0xffff;
	smp->dr_dlid       = 0xffff;

	umad_set_addr(umad, 0xffff, 0, 0, 0);

	if (path)
		memcpy(smp->initial_path, path->path, path->hop_cnt+1);

	smp->hop_cnt = path->hop_cnt;
}

void
smp_get_init(void *umad, int lid, int attr, int mod)
{
	struct drsmp *smp = (struct drsmp *)(umad_get_mad(umad));

	memset(smp, 0, sizeof (*smp));

	smp->base_version  = 1;
	smp->mgmt_class    = CLASS_SUBN_LID_ROUTE;
	smp->class_version = 1;

	smp->method        = 1;
	smp->attr_id	   = (uint16_t)htons((uint16_t)attr);
	smp->attr_mod	   = htonl(mod);
	smp->tid           = htonll(drmad_tid++);

	umad_set_addr(umad, lid, 0, 0xffff, 0);
}

void
drsmp_set_init(void *umad, DRPath *path, int attr, int mod, void *data)
{
	struct drsmp *smp = (struct drsmp *)(umad_get_mad(umad));

	memset(smp, 0, sizeof (*smp));

	smp->method        = 2;		/* SET */
	smp->attr_id	   = (uint16_t)htons((uint16_t)attr);
	smp->attr_mod	   = htonl(mod);
	smp->tid           = htonll(drmad_tid++);
	smp->dr_slid       = 0xffff;
	smp->dr_dlid       = 0xffff;

	umad_set_addr(umad, 0xffff, 0, 0, 0);

	if (path)
		memcpy(smp->initial_path, path->path, path->hop_cnt+1);

	if (data)
		memcpy(smp->data, data, sizeof smp->data);

	smp->hop_cnt = path->hop_cnt;
}

char *
drmad_status_str(struct drsmp *drsmp)
{
	switch (drsmp->status) {
	case 0:
		return "success";
	case ETIMEDOUT:
		return "timeout";
	}
	return "unknown error";
}

int
str2DRPath(char *str, DRPath *path)
{
	char *s;

	path->hop_cnt = -1;

	DEBUG("DR str: %s", str);
	while (str && *str) {
		if ((s = strchr(str, ',')))
			*s = 0;
		path->path[++path->hop_cnt] = atoi(str);
		if (!s)
			break;
		str = s+1;
	}

#if 0
	if (path->path[0] != 0 ||
	   (path->hop_cnt > 0 && dev_port && path->path[1] != dev_port)) {
		DEBUG("hop 0 != 0 or hop 1 != dev_port");
		return -1;
	}
#endif

	return path->hop_cnt;
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s [-s(ring) -D(irect) -V(ersion) -C ca_name -P ca_port -t(imeout) timeout_ms] <dlid|dr_path> <attr> [mod]\n", argv0);
	fprintf(stderr, "\tDR examples:\n");
	fprintf(stderr, "\t\t%s -D 0,1,2,3,5 16	# NODE DESC\n", argv0);
	fprintf(stderr, "\t\t%s -D 0,1,2 0x15 2	# PORT INFO, port 2\n", argv0);
	fprintf(stderr, "\n\tLID routed examples:\n");
	fprintf(stderr, "\t\t%s 3 0x15 2	# PORT INFO, lid 3 port 2\n", argv0);
	fprintf(stderr, "\t\t%s 0xa0 0x11	# NODE INFO, lid 0xa0\n", argv0);
	fprintf(stderr, "\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	int dump_char = 0, timeout_ms = 1000;
	int dev_port = 0, mgmt_class = CLASS_SUBN_LID_ROUTE, dlid = 0;
	char *dev_name = 0;
	void *umad;
	struct drsmp *smp;
	int i, portid, mod = 0, attr;
	DRPath path;
	uint8_t *desc;
	int length;

	static char const str_opts[] = "C:P:t:dsDVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "sring", 0, 0, 's'},
		{ "Direct", 0, 0, 'D'},
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
		case 's':
			dump_char++;
			break;
		case 'd':
			debug++;
			if (debug > 1)
				umad_debug(debug-1);
			break;
		case 'D':
			mgmt_class = CLASS_SUBN_DIRECTED_ROUTE;
			break;
		case 'C':
			dev_name = optarg;
			break;
		case 'P':
			dev_port = atoi(optarg);
			break;
		case 't':
			timeout_ms = strtoul(optarg, 0, 0);
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

	if (mgmt_class == CLASS_SUBN_DIRECTED_ROUTE &&
	    str2DRPath(strdup(argv[0]), &path) < 0)
		IBPANIC("bad path str '%s'", argv[0]);

	if (mgmt_class == CLASS_SUBN_LID_ROUTE)
		dlid = strtoul(argv[0], 0, 0);

	attr = strtoul(argv[1], 0, 0);
	if (argc > 2)
		mod = strtoul(argv[2], 0, 0);

	if (umad_init() < 0)
		IBPANIC("can't init UMAD library");

	if ((portid = umad_open_port(dev_name, dev_port)) < 0)
		IBPANIC("can't open UMAD port (%s:%d)", dev_name, dev_port);

	if ((mad_agent = umad_register(portid, mgmt_class, 1, 0, 0)) < 0)
		IBPANIC("Couldn't register agent for SMPs");

	if (!(umad = umad_alloc(1, umad_size() + IB_MAD_SIZE)))
		IBPANIC("can't alloc MAD");

	smp = umad_get_mad(umad);

	if (mgmt_class == CLASS_SUBN_DIRECTED_ROUTE)
		drsmp_get_init(umad, &path, attr, mod);
	else
		smp_get_init(umad, dlid, attr, mod);

	if (debug > 1)
		xdump(stderr, "before send:\n", smp, 256);

	length = IB_MAD_SIZE;
	if (umad_send(portid, mad_agent, umad, length, timeout_ms, 0) < 0)
		IBPANIC("send failed");

	if (umad_recv(portid, umad, &length, -1) != mad_agent)
		IBPANIC("recv error: %s", drmad_status_str(smp));

	if (!dump_char) {
		xdump(stdout, 0, smp->data, 64);
		if (smp->status)
			fprintf(stdout, "SMP status: 0x%x\n", ntohs(smp->status));
		return 0;
	}

	desc = smp->data;
	for (i = 0; i < 64; ++i) {
		if (!desc[i])
			break;
		putchar(desc[i]);
	}
	putchar('\n');
	if (smp->status)
		fprintf(stdout, "SMP status: 0x%x\n", ntohs(smp->status));
	return 0;
}
