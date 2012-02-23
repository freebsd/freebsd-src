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
#include <signal.h>
#include <getopt.h>

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>

#include "ibdiag_common.h"

#undef DEBUG
#define	DEBUG	if (verbose) IBWARN

static int dest_type = IB_DEST_LID;
static int verbose;
static char host_and_domain[IB_VENDOR_RANGE2_DATA_SIZE];
static char last_host[IB_VENDOR_RANGE2_DATA_SIZE];

char *argv0 = "ibping";

static void
get_host_and_domain(char *data, int sz)
{
	char *s = data;
	int n;

	if (gethostname(s, sz) < 0)
		snprintf(s, sz, "?hostname?");

	s[sz-1] = 0;
	if ((n = strlen(s)) >= sz)
		return;
	s[n] = '.';
	s += n + 1;
	sz -= n + 1;

	if (getdomainname(s, sz) < 0)
		snprintf(s, sz, "?domainname?");
	if (strlen(s) == 0)
		s[-1] = 0;	/* no domain */
}

static char *
ibping_serv(void)
{
	void *umad;
	void *mad;
	char *data;

	DEBUG("starting to serve...");

	while ((umad = mad_receive(0, -1))) {

		mad = umad_get_mad(umad);
		data = (char *)mad + IB_VENDOR_RANGE2_DATA_OFFS;

		memcpy(data, host_and_domain, IB_VENDOR_RANGE2_DATA_SIZE);

		DEBUG("Pong: %s", data);

		if (mad_respond(umad, 0, 0) < 0)
			DEBUG("respond failed");

		mad_free(umad);
	}

	DEBUG("server out");
	return 0;
}

static uint64_t
ibping(ib_portid_t *portid, int quiet)
{
	char data[IB_VENDOR_RANGE2_DATA_SIZE] = {0};
	ib_vendor_call_t call;
	uint64_t start, rtt;

	DEBUG("Ping..");

	start = getcurrenttime();

	call.method = IB_MAD_METHOD_GET;
	call.mgmt_class = IB_VENDOR_OPENIB_PING_CLASS;
	call.attrid = 0;
	call.mod = 0;
	call.oui = IB_OPENIB_OUI;
	call.timeout = 0;
	memset(&call.rmpp, 0, sizeof call.rmpp);

	if (!ib_vendor_call(data, portid, &call))
		return ~0llu;

	rtt = getcurrenttime() - start;

	if (!last_host[0])
		memcpy(last_host, data, sizeof last_host);

	if (!quiet)
		printf("Pong from %s (%s): time %" PRIu64 ".%03" PRIu64 " ms\n",
			data, portid2str(portid), rtt/1000, rtt%1000);

	return rtt;
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
			"-t(imeout) timeout_ms -c ping_count -f(lood) -o oui -S(erver)] <dest lid|guid>\n",
			basename);
	exit(-1);
}

static uint64_t minrtt = ~0ull, maxrtt, total_rtt;
static uint64_t start, total_time, replied, lost, ntrans;
static ib_portid_t portid = {0};

void
report(int sig)
{
	total_time = getcurrenttime() - start;

	DEBUG("out due signal %d", sig);

	printf("\n--- %s (%s) ibping statistics ---\n", last_host, portid2str(&portid));
	printf("%" PRIu64 " packets transmitted, %" PRIu64 " received, %" PRIu64 "%% packet loss, time %" PRIu64 " ms\n",
		ntrans, replied,
		(lost != 0) ?  lost * 100 / ntrans : 0, total_time / 1000);
	printf("rtt min/avg/max = %" PRIu64 ".%03" PRIu64 "/%" PRIu64 ".%03" PRIu64 "/%" PRIu64 ".%03" PRIu64 " ms\n",
		minrtt == ~0ull ? 0 : minrtt/1000,
		minrtt == ~0ull ? 0 : minrtt%1000,
		replied ? total_rtt/replied/1000 : 0,
		replied ? (total_rtt/replied)%1000 : 0,
		maxrtt/1000, maxrtt%1000);

	exit(0);
}

int
main(int argc, char **argv)
{
	int mgmt_classes[3] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS};
	int ping_class = IB_VENDOR_OPENIB_PING_CLASS;
	ib_portid_t *sm_id = 0, sm_portid = {0};
	int timeout = 0, udebug = 0, server = 0, flood = 0;
	int oui = IB_OPENIB_OUI;
	uint64_t rtt;
	unsigned count = ~0;
	extern int ibdebug;
	char *err;
	char *ca = 0;
	int ca_port = 0;

	static char const str_opts[] = "C:P:t:s:c:o:devGfSVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "err_show", 0, 0, 'e'},
		{ "verbose", 0, 0, 'v'},
		{ "Guid", 0, 0, 'G'},
		{ "s", 1, 0, 's'},
		{ "timeout", 1, 0, 't'},
		{ "c", 1, 0, 'c'},
		{ "flood", 0, 0, 'f'},
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
		case 'c':
			count = strtoul(optarg, 0, 0);
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
		case 'f':
			flood++;
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

	madrpc_init(ca, ca_port, mgmt_classes, 3);

	if (server) {
		if (mad_register_server(ping_class, 0, 0, oui) < 0)
			IBERROR("can't serve class %d on this port", ping_class);

		get_host_and_domain(host_and_domain, sizeof host_and_domain);

		if ((err = ibping_serv()))
			IBERROR("ibping to %s: %s", portid2str(&portid), err);
		exit(0);
	}

	if (mad_register_client(ping_class, 0) < 0)
		IBERROR("can't register ping class %d on this port", ping_class);

	if (ib_resolve_portid_str(&portid, argv[0], dest_type, sm_id) < 0)
		IBERROR("can't resolve destination port %s", argv[0]);

	signal(SIGINT, report);
	signal(SIGTERM, report);

	start = getcurrenttime();

	while (count-- > 0) {
		ntrans++;
		if ((rtt = ibping(&portid, flood)) == ~0ull) {
			DEBUG("ibping to %s failed", portid2str(&portid));
			lost++;
		} else {
			if (rtt < minrtt)
				minrtt = rtt;
			if (rtt > maxrtt)
				maxrtt = rtt;
			total_rtt += rtt;
			replied++;
		}

		if (!flood)
			sleep(1);
	}

	report(0);

	exit(-1);
}
