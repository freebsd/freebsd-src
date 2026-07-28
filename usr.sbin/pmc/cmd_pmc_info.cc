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

#include "cmd_pmc.h"

#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "display.hh"
#include "view.hh"

class info_view : public pmcview
{
public:
	virtual void
	callchain(struct pmclog_ev_callchain &p)
	{
		uint64_t eventid = pmcidtoeventid(p.pl_pmcid);

		pmcinfo[eventid].count++;
	}

	virtual void
	print()
	{
		title("PMC Info");

		header("System Info");
		printf("CPU Model: %s\n", cpumodel.c_str());
		printf("OS Release: %s\n", osrelease.c_str());
		printf("Build ID: %s\n", buildid.c_str());
		printf("\n");

		if (arch  == PMC_ARCH_AMD64) {
			header("CPUID");
			for (auto &c : cpuid)
				printf("0x%08x: 0x%08x 0x%08x 0x%08x 0x%08x\n",
				    c.first, c.second.eax, c.second.ebx,
				    c.second.ecx, c.second.edx);
			printf("\n");
		}
    
		header("PMC Info");
		for (auto &p : extpmcinfo)
			printf("%s\n", p.event.c_str());
		printf("\n");

		table t = table();
		t.addcolumn("Counter", true);
		t.addcolumn("Counts", true);

		for (auto &p : pmcinfo) {
			std::vector<field> r;

			r.emplace_back(p.second.name);
			r.emplace_back((int64_t)p.second.count);

			t.addrow(r);
		}

		t.print();
	}
};

static struct option longopts[] = {
	PMCFILTER_LOPTS,
	{ NULL,		0,			NULL,	0 }
};

static void
usage(void)
{
	printf("Usage: pmc info [options] [pmclog]\n\n");
	printf("Display a log summary\n\n");
	printf("Options:\n");
	PMCFILTER_PRINTOPTS();
}

int
cmd_pmc_info(int argc, char **argv)
{
	struct pmcfilter filter = pmcfilter();
	const char *logfile = "default.log";
	int option, logfd;

	while ((option = getopt_long(argc, argv, PMCFILTER_SOPTS "s:", longopts, NULL)) != -1) {
		switch (option) {
		PMCFILTER_CASE(filter);
		case '?':
		default:
			usage();
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
		return (EX_NOINPUT);
	}

	info_view s = info_view();
	s.setfilter(filter);
	s.process(logfd);
	s.print();

	return (EX_OK);
}
