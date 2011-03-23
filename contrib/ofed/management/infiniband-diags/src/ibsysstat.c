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

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>

#include "ibdiag_common.h"

#undef DEBUG
#define	DEBUG	if (verbose) IBWARN

static int dest_type = IB_DEST_LID;
static int verbose;

#define MAX_CPUS 8

enum ib_sysstat_attr_t {
	IB_PING_ATTR = 0x10,
	IB_HOSTINFO_ATTR = 0x11,
	IB_CPUINFO_ATTR = 0x12,
};

typedef struct cpu_info {
	char *model;
	char *mhz;
} cpu_info;

static cpu_info cpus[MAX_CPUS];
static int host_ncpu;

char *argv0 = "ibsysstat";

static void
mk_reply(int attr, void *data, int sz)
{
	char *s = data;
	int n, i;

	switch (attr) {
	case IB_PING_ATTR:
		break;		/* nothing to do here, just reply */
	case IB_HOSTINFO_ATTR:
		if (gethostname(s, sz) < 0)
			snprintf(s, sz, "?hostname?");
		s[sz-1] = 0;
		if ((n = strlen(s)) >= sz)
			break;
		s[n] = '.';
		s += n+1;
		sz -= n+1;
		if (getdomainname(s, sz) < 0)
			snprintf(s, sz, "?domainname?");
		if (strlen(s) == 0)
			s[-1] = 0;	/* no domain */
		break;
	case IB_CPUINFO_ATTR:
		for (i = 0; i < host_ncpu && sz > 0; i++) {
			n = snprintf(s, sz, "cpu %d: model %s MHZ %s\n",
				     i, cpus[i].model, cpus[i].mhz);
			if (n >= sz) {
				IBWARN("cpuinfo truncated");
				break;
			}
			sz -= n;
			s += n;
		}
		break;
	default:
		DEBUG("unknown attr %d", attr);
	}
}

static char *
ibsystat_serv(void)
{
	void *umad;
	void *mad;
	int attr, mod;

	DEBUG("starting to serve...");

	while ((umad = mad_receive(0, -1))) {

		mad = umad_get_mad(umad);

		attr = mad_get_field(mad, 0, IB_MAD_ATTRID_F);
		mod = mad_get_field(mad, 0, IB_MAD_ATTRMOD_F);

		DEBUG("got packet: attr 0x%x mod 0x%x", attr, mod);

		mk_reply(attr, (char *)mad + IB_VENDOR_RANGE2_DATA_OFFS, IB_VENDOR_RANGE2_DATA_SIZE);

		if (mad_respond(umad, 0, 0) < 0)
			DEBUG("respond failed");

		mad_free(umad);
	}

	DEBUG("server out");
	return 0;
}

static int
match_attr(char *str)
{
	if (!strcmp(str, "ping"))
		return IB_PING_ATTR;
	if (!strcmp(str, "host"))
		return IB_HOSTINFO_ATTR;
	if (!strcmp(str, "cpu"))
		return IB_CPUINFO_ATTR;
	return -1;
}

static char *
ibsystat(ib_portid_t *portid, int attr)
{
	char data[IB_VENDOR_RANGE2_DATA_SIZE] = {0};
	ib_vendor_call_t call;

	DEBUG("Sysstat ping..");

	call.method = IB_MAD_METHOD_GET;
	call.mgmt_class = IB_VENDOR_OPENIB_SYSSTAT_CLASS;
	call.attrid = attr;
	call.mod = 0;
	call.oui = IB_OPENIB_OUI;
	call.timeout = 0;
	memset(&call.rmpp, 0, sizeof call.rmpp);

	if (!ib_vendor_call(data, portid, &call))
		return "vendor call failed";

	DEBUG("Got sysstat pong..");
	if (attr != IB_PING_ATTR)
		puts(data);
	else
		printf("sysstat ping succeeded\n");
	return 0;
}

int
build_cpuinfo(void)
{
	char line[1024] = {0}, *s, *e;
	FILE *f;
	int ncpu = 0;

	if (!(f = fopen("/proc/cpuinfo", "r"))) {
		IBWARN("couldn't open /proc/cpuinfo");
		return 0;
	}

	while (fgets(line, sizeof(line) - 1, f)) {
		if (!strncmp(line, "processor\t", 10)) {
			ncpu++;
			if (ncpu > MAX_CPUS)
				return MAX_CPUS;
			continue;
		}

		if (!ncpu || !(s = strchr(line, ':')))
			continue;

		if ((e = strchr(s, '\n')))
			*e = 0;
		if (!strncmp(line, "model name\t", 11))
			cpus[ncpu-1].model = strdup(s+1);
		else if (!strncmp(line, "cpu MHz\t", 8))
			cpus[ncpu-1].mhz = strdup(s+1);
	}

	fclose(f);

	DEBUG("ncpu %d", ncpu);

	return ncpu;
}

static void
usage(void)
{
	char *basename;

	if (!(basename = strrchr(argv0, '/')))
		basename = argv0;
	else
		basename++;

	fprintf(stderr, "Usage: %s [-d(ebug) -e(rr_show) -v(erbose) -G(uid) -s smlid -V(ersion) -C ca_name -P ca_port "
			"-t(imeout) timeout_ms -o oui -S(erver)] <dest lid|guid> [<op>]\n",
			basename);
	exit(-1);
}

int
main(int argc, char **argv)
{
	int mgmt_classes[3] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS};
	int sysstat_class = IB_VENDOR_OPENIB_SYSSTAT_CLASS;
	ib_portid_t portid = {0};
	ib_portid_t *sm_id = 0, sm_portid = {0};
	int timeout = 0, udebug = 0, server = 0;
	int oui = IB_OPENIB_OUI, attr = IB_PING_ATTR;
	extern int ibdebug;
	char *err;
	char *ca = 0;
	int ca_port = 0;

	static char const str_opts[] = "C:P:t:s:o:devGSVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "err_show", 0, 0, 'e'},
		{ "verbose", 0, 0, 'v'},
		{ "Guid", 0, 0, 'G'},
		{ "timeout", 1, 0, 't'},
		{ "s", 1, 0, 's'},
		{ "o", 1, 0, 'o'},
		{ "Server", 0, 0, 'S'},
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
			madrpc_show_errors(1);
			umad_debug(udebug);
			udebug++;
			break;
		case 'e':
			madrpc_show_errors(1);
			break;
		case 'G':
			dest_type = IB_DEST_GUID;
			break;
		case 'o':
			oui = strtoul(optarg, 0, 0);
			break;
		case 's':
			if (ib_resolve_portid_str(&sm_portid, optarg, IB_DEST_LID, 0) < 0)
				IBERROR("can't resolve SM destination port %s", optarg);
			sm_id = &sm_portid;
			break;
		case 'S':
			server++;
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

	if (!argc && !server)
		usage();

	if (argc > 1 && (attr = match_attr(argv[1])) < 0)
		usage();

	madrpc_init(ca, ca_port, mgmt_classes, 3);

	if (server) {
		if (mad_register_server(sysstat_class, 0, 0, oui) < 0)
			IBERROR("can't serve class %d", sysstat_class);

		host_ncpu = build_cpuinfo();

		if ((err = ibsystat_serv()))
			IBERROR("ibssystat to %s: %s", portid2str(&portid), err);
		exit(0);
	}

	if (mad_register_client(sysstat_class, 0) < 0)
		IBERROR("can't register to sysstat class %d", sysstat_class);

	if (ib_resolve_portid_str(&portid, argv[0], dest_type, sm_id) < 0)
		IBERROR("can't resolve destination port %s", argv[0]);

	if ((err = ibsystat(&portid, attr)))
		IBERROR("ibsystat to %s: %s", portid2str(&portid), err);

	exit(0);
}
