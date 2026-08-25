/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Netflix, Inc.
 *
 * This software was developed by Ali Mashtizadeh under the sponsorship from
 * Netflix, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/event.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <assert.h>
#include <curses.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <kvm.h>
#include <libgen.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <pmc.h>
#include <pmclog.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <libpmcstat.h>
#include "cmd_pmc.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <dev/hwpmc/hwpmc_ibs.h>
#include "display.hh"
#include "view.hh"

struct frontend {
	syminfo		func;
	int64_t		ocmiss;
	int64_t		l2miss;
	int64_t		l3miss;
	int64_t		l1tlbmiss;
	int64_t		l2tlbmiss;
	int64_t		latency;
	int64_t		samples;
};

static int sortcol = 2;

class frontend_view : public pmcview
{
public:
	frontend_view() : pmcview(), samples() { }
	~frontend_view() { }

	virtual void
	callchain(struct pmclog_ev_callchain &p,
	    ibsfetchinfo &f, __unused uintfptr_t *cc, __unused int len)
	{
		int usermode = PMC_CALLCHAIN_CPUFLAGS_TO_USERMODE(p.pl_cpuflags);

		/* Ignore zeros */
		if (IBS_FETCH_CTL_TO_LAT(f.ctl) == 0)
			return;

		auto inst = samples.find(f.linaddr);
		if (inst == samples.end()) {
			syminfo sym = addrtosymbol(usermode ? p.pl_pid : 0, f.linaddr);
			if (sym.name == "")
				return;

			samples[f.linaddr] = frontend();
			inst = samples.find(f.linaddr);
			inst->second.func = sym;
		}

		/*
		 * Save fetch cache and TLB miss counts
		 */
		if (f.ctl & IBS_FETCH_CTL_OPCACHEMISS)
			inst->second.ocmiss += 1;
		if (f.ctl & IBS_FETCH_CTL_L2MISS)
			inst->second.l2miss += 1;
		if (f.ctl & IBS_FETCH_CTL_L3MISS)
			inst->second.l3miss += 1;

		if (f.ctl & IBS_FETCH_CTL_L1TLBMISS)
			inst->second.l1tlbmiss += 1;
		if (f.ctl & IBS_FETCH_CTL_L2TLBMISS)
			inst->second.l2tlbmiss += 1;

		inst->second.latency += IBS_FETCH_CTL_TO_LAT(f.ctl);
		inst->second.samples += 1;
	}

	virtual void
	print()
	{
		title("IBS Frontend Analysis");

		// Print header
		table t = table();
		t.addcolumn("Image", true);
		t.addcolumn("Function", true);
		t.addcolumn("Latency");
		t.addcolumn("Samples");
		t.addcolumn("OC Miss");
		t.addcolumn("L2 Miss");
		t.addcolumn("L3 Miss");
		t.addcolumn("L1 TLB Miss");
		t.addcolumn("L2 TLB Miss");

		for (auto &kv : samples) {
			std::vector<field> r;

			r.emplace_back(kv.second.func.binary);
			r.emplace_back(kv.second.func.to_string());
			r.emplace_back(kv.second.latency);
			r.emplace_back(kv.second.samples);
			r.emplace_back(kv.second.ocmiss, kv.second.samples, true);
			r.emplace_back(kv.second.l2miss, kv.second.samples, true);
			r.emplace_back(kv.second.l3miss, kv.second.samples, true);
			r.emplace_back(kv.second.l1tlbmiss, kv.second.samples, true);
			r.emplace_back(kv.second.l2tlbmiss, kv.second.samples, true);

			t.addrow(r);
		}

		t.sort(sortcol);
		t.print();
	}
protected:
	std::unordered_map<uint64_t, frontend> samples;
};


static struct option longopts[] = {
	PMCFILTER_LOPTS,
	{ "sort",	required_argument,	NULL,	's' },
	{ NULL,		0,			NULL,	0 }
};

static void
usage(void)
{
	printf("Usage: pmc frontend [options] [pmclog]\n\n");
	printf("Analyze frontend bottlenecks\n\n");
	printf("Options:\n");
	printf("\t-s,--sort         Sort by event\n\n");
	PMCFILTER_PRINTOPTS();
}

int
cmd_pmc_frontend(int argc, char **argv)
{
	pmcfilter filter = pmcfilter();
	const char *logfile = "default.log";
	int option, logfd;

	while ((option = getopt_long(argc, argv, PMCFILTER_SOPTS, longopts, NULL)) != -1) {
		switch (option) {
		PMCFILTER_CASE(filter)
		case 's':
			sortcol = atoi(optarg);
			break;
		case '?':
		default:
			usage();
			exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0 && argc != 1) {
		usage();
		exit(EX_USAGE);
	}
	if (argc == 1)
		logfile = argv[0];

	setup_screen();

	if ((logfd = open(logfile, O_RDONLY)) < 0) {
		errx(EX_OSERR, "ERROR: Cannot open \"%s\" for reading: %s.", logfile,
		    strerror(errno));
	}

	frontend_view v = frontend_view();
	v.setfilter(filter);
	v.process(logfd);
	v.print();

	close(logfd);

	return (0);
}
