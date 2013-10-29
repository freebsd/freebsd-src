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
#include <inttypes.h>
#include <getopt.h>

#include <infiniband/common.h>
#include <infiniband/umad.h>
#include <infiniband/mad.h>

#include "ibdiag_common.h"

static uint8_t sminfo[1024];

char *argv0 = "sminfo";

static void
usage(void)
{
	fprintf(stderr, "Usage: %s [-d(ebug) -e(rr_show) -s state -p prio -a activity -D(irect) -G(uid) -V(ersion) -C ca_name -P ca_port "
			"-t(imeout) timeout_ms] <sm_lid|sm_dr_path> [modifier]\n",
			argv0);
	exit(-1);
}

int strdata, xdata=1, bindata;
enum {
	SMINFO_NOTACT,
	SMINFO_DISCOVER,
	SMINFO_STANDBY,
	SMINFO_MASTER,

	SMINFO_STATE_LAST,
};

char *statestr[] = {
	[SMINFO_NOTACT] = "SMINFO_NOTACT",
	[SMINFO_DISCOVER] = "SMINFO_DISCOVER",
	[SMINFO_STANDBY] = "SMINFO_STANDBY",
	[SMINFO_MASTER] = "SMINFO_MASTER",
};

#define STATESTR(s)	(((unsigned)(s)) < SMINFO_STATE_LAST ? statestr[s] : "???")

int
main(int argc, char **argv)
{
	int mgmt_classes[3] = {IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS};
	int mod = 0;
	ib_portid_t portid = {0};
	int timeout = 0;	/* use default */
	uint8_t *p;
	unsigned act = 0;
	int prio = 0, state = SMINFO_STANDBY;
	uint64_t guid = 0, key = 0;
	extern int ibdebug;
	int dest_type = IB_DEST_LID;
	int udebug = 0;
	char *ca = 0;
	int ca_port = 0;

	static char const str_opts[] = "C:P:t:s:p:a:deDGVhu";
	static const struct option long_opts[] = {
		{ "C", 1, 0, 'C'},
		{ "P", 1, 0, 'P'},
		{ "debug", 0, 0, 'd'},
		{ "err_show", 0, 0, 'e'},
		{ "s", 1, 0, 's'},
		{ "p", 1, 0, 'p'},
		{ "a", 1, 0, 'a'},
		{ "Direct", 0, 0, 'D'},
		{ "Guid", 0, 0, 'G'},
		{ "Version", 0, 0, 'V'},
		{ "timeout", 1, 0, 't'},
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
		case 'D':
			dest_type = IB_DEST_DRPATH;
			break;
		case 'G':
			dest_type = IB_DEST_GUID;
			break;
		case 't':
			timeout = strtoul(optarg, 0, 0);
			madrpc_set_timeout(timeout);
			break;
		case 'a':
			act = strtoul(optarg, 0, 0);
			break;
		case 's':
			state = strtoul(optarg, 0, 0);
			break;
		case 'p':
			prio = strtoul(optarg, 0, 0);
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
		mod = atoi(argv[1]);

	madrpc_init(ca, ca_port, mgmt_classes, 3);

	if (argc) {
		if (ib_resolve_portid_str(&portid, argv[0], dest_type, 0) < 0)
			IBERROR("can't resolve destination port %s", argv[0]);
	} else {
		if (ib_resolve_smlid(&portid, timeout) < 0)
			IBERROR("can't resolve sm port %s", argv[0]);
	}

	mad_encode_field(sminfo, IB_SMINFO_GUID_F, &guid);
	mad_encode_field(sminfo, IB_SMINFO_ACT_F, &act);
	mad_encode_field(sminfo, IB_SMINFO_KEY_F, &key);
	mad_encode_field(sminfo, IB_SMINFO_PRIO_F, &prio);
	mad_encode_field(sminfo, IB_SMINFO_STATE_F, &state);

	if (mod) {
		if (!(p = smp_set(sminfo, &portid, IB_ATTR_SMINFO, mod, timeout)))
			IBERROR("query");
	} else
		if (!(p = smp_query(sminfo, &portid, IB_ATTR_SMINFO, 0, timeout)))
			IBERROR("query");

	mad_decode_field(sminfo, IB_SMINFO_GUID_F, &guid);
	mad_decode_field(sminfo, IB_SMINFO_ACT_F, &act);
	mad_decode_field(sminfo, IB_SMINFO_KEY_F, &key);
	mad_decode_field(sminfo, IB_SMINFO_PRIO_F, &prio);
	mad_decode_field(sminfo, IB_SMINFO_STATE_F, &state);

	printf("sminfo: sm lid %d sm guid 0x%" PRIx64 ", activity count %u priority %d state %d %s\n",
		portid.lid, guid, act, prio, state, STATESTR(state));

	exit(0);
}
