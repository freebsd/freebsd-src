/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018, Matthew Macy
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <string>
#if _LIBCPP_STD_VER >= 11
#include <unordered_map>
using	std::unordered_map;
#else
#include <tr1/unordered_map>
using	std::tr1::unordered_map;
#endif
#define LIST_MAX 64
static struct option longopts[] = {
	{"lwps", required_argument, NULL, 't'},
	{"pids", required_argument, NULL, 'p'},
	{"threads", required_argument, NULL, 'T'},
	{"processes", required_argument, NULL, 'P'},
	{"events", required_argument, NULL, 'e'},
	{NULL, 0, NULL, 0}
};

static void
usage(void)
{
	errx(EX_USAGE,
	    "\t filter log file\n"
	    "\t -e <events>, --events <events> -- comma-delimited list of events to filter on\n"
	    "\t -p <pids>, --pids <pids> -- comma-delimited list of pids to filter on\n"
	    "\t -P <processes>, --processes <processes> -- comma-delimited list of process names to filter on\n"
	    "\t -t <lwps>, --lwps <lwps> -- comma-delimited list of lwps to filter on\n"
	    "\t -T <threads>, --threads <threads> -- comma-delimited list of thread names to filter on\n"
	    "\t -x -- toggle inclusive filtering\n"
	    );
}


static void
parse_intlist(char *strlist, uint32_t *intlist, int *pcount, int (*fn) (const char *))
{
	char *token;
	int count, tokenval;

	count = 0;
	while ((token = strsep(&strlist, ",")) != NULL &&
	    count < LIST_MAX) {
		if ((tokenval = fn(token)) < 0)
			errx(EX_USAGE, "ERROR: %s not usable value", token);
		intlist[count++] = tokenval;
	}
	*pcount = count;
}

static void
parse_events(char *strlist, uint32_t intlist[LIST_MAX], int *pcount, char *cpuid)
{
	char *token;
	int count, tokenval;

	count = 0;
	while ((token = strsep(&strlist, ",")) != NULL &&
	    count < LIST_MAX) {
		if ((tokenval = pmc_pmu_idx_get_by_event(cpuid, token)) < 0)
			errx(EX_USAGE, "ERROR: %s not usable value", token);
		intlist[count++] = tokenval;
	}
	*pcount = count;
}

static void
parse_names(char *strlist, char *namelist[LIST_MAX], int *pcount)
{
	char *token;
	int count;

	count = 0;
	while ((token = strsep(&strlist, ",")) != NULL &&
	    count < LIST_MAX) {
		namelist[count++] = token;
	}
	*pcount = count;
}


struct pmcid_ent {
	uint32_t pe_pmcid;
	uint32_t pe_idx;
};
#define	_PMCLOG_TO_HEADER(T,L)						\
	((PMCLOG_HEADER_MAGIC << 24) |					\
	 (PMCLOG_TYPE_ ## T << 16)   |					\
	 ((L) & 0xFFFF))


typedef unordered_map < int ,std::string > idmap;
typedef std::pair < int ,std::string > identry;

static bool
pmc_find_name(idmap & map, uint32_t id, char *list[LIST_MAX], int count)
{
	int i;

	auto kvpair = map.find(id);
	if (kvpair == map.end()) {
		printf("unknown id: %d\n", id);
		return (false);
	}
	auto p = list;
	for (i = 0; i < count; i++, p++) {
		if (strstr(kvpair->second.c_str(), *p) != NULL)
			return (true);
	}
	return (false);
}

static void
pmc_filter_handler(uint32_t *lwplist, int lwpcount, uint32_t *pidlist, int pidcount,
    char *events, char *processes, char *threads, bool exclusive, int infd,
    int outfd)
{
	struct pmclog_ev ev;
	struct pmclog_parse_state *ps;
	struct pmcid_ent *pe;
	uint32_t eventlist[LIST_MAX];
	char cpuid[PMC_CPUID_LEN];
	char *proclist[LIST_MAX];
	char *threadlist[LIST_MAX];
	int i, pmccount, copies, eventcount, proccount, threadcount;
	uint32_t idx;
	idmap pidmap, tidmap;

	if ((ps = static_cast < struct pmclog_parse_state *>(pmclog_open(infd)))== NULL)
		errx(EX_OSERR, "ERROR: Cannot allocate pmclog parse state: %s\n", strerror(errno));

	proccount = eventcount = pmccount = 0;
	if (processes)
		parse_names(processes, proclist, &proccount);
	if (threads)
		parse_names(threads, threadlist, &threadcount);
	while (pmclog_read(ps, &ev) == 0) {
		if (ev.pl_type == PMCLOG_TYPE_INITIALIZE)
			memcpy(cpuid, ev.pl_u.pl_i.pl_cpuid, PMC_CPUID_LEN);
		if (ev.pl_type == PMCLOG_TYPE_PMCALLOCATE)
			pmccount++;
	}
	if (events)
		parse_events(events, eventlist, &eventcount, cpuid);

	lseek(infd, 0, SEEK_SET);
	pmclog_close(ps);
	if ((ps = static_cast < struct pmclog_parse_state *>(pmclog_open(infd)))== NULL)
		errx(EX_OSERR, "ERROR: Cannot allocate pmclog parse state: %s\n", strerror(errno));
	if ((pe = (typeof(pe)) malloc(sizeof(*pe) * pmccount)) == NULL)
		errx(EX_OSERR, "ERROR: failed to allocate pmcid map");
	i = 0;
	while (pmclog_read(ps, &ev) == 0 && i < pmccount) {
		if (ev.pl_type == PMCLOG_TYPE_PMCALLOCATE) {
			pe[i].pe_pmcid = ev.pl_u.pl_a.pl_pmcid;
			pe[i].pe_idx = ev.pl_u.pl_a.pl_event;
			i++;
		}
	}
	lseek(infd, 0, SEEK_SET);
	pmclog_close(ps);
	if ((ps = static_cast < struct pmclog_parse_state *>(pmclog_open(infd)))== NULL)
		errx(EX_OSERR, "ERROR: Cannot allocate pmclog parse state: %s\n", strerror(errno));
	copies = 0;
	while (pmclog_read(ps, &ev) == 0) {
		if (ev.pl_type == PMCLOG_TYPE_THR_CREATE)
			tidmap.insert(identry(ev.pl_u.pl_tc.pl_tid, ev.pl_u.pl_tc.pl_tdname));
		if (ev.pl_type == PMCLOG_TYPE_PROC_CREATE)
			pidmap.insert(identry(ev.pl_u.pl_pc.pl_pid, ev.pl_u.pl_pc.pl_pcomm));
		if (ev.pl_type != PMCLOG_TYPE_CALLCHAIN) {
			if (write(outfd, ev.pl_data, ev.pl_len) != (ssize_t)ev.pl_len)
				errx(EX_OSERR, "ERROR: failed output write");
			continue;
		}
		if (pidcount) {
			for (i = 0; i < pidcount; i++)
				if (pidlist[i] == ev.pl_u.pl_cc.pl_pid)
					break;
			if ((i == pidcount) == exclusive)
				continue;
		}
		if (lwpcount) {
			for (i = 0; i < lwpcount; i++)
				if (lwplist[i] == ev.pl_u.pl_cc.pl_tid)
					break;
			if ((i == lwpcount) == exclusive)
				continue;
		}
		if (eventcount) {
			for (i = 0; i < pmccount; i++) {
				if (pe[i].pe_pmcid == ev.pl_u.pl_cc.pl_pmcid)
					break;
			}
			if (i == pmccount)
				errx(EX_USAGE, "ERROR: unallocated pmcid: %d\n",
				    ev.pl_u.pl_cc.pl_pmcid);

			idx = pe[i].pe_idx;
			for (i = 0; i < eventcount; i++) {
				if (idx == eventlist[i])
					break;
			}
			if ((i == eventcount) == exclusive)
				continue;
		}
		if (proccount &&
		    pmc_find_name(pidmap, ev.pl_u.pl_cc.pl_pid, proclist, proccount) == exclusive)
			continue;
		if (threadcount &&
		    pmc_find_name(tidmap, ev.pl_u.pl_cc.pl_tid, threadlist, threadcount) == exclusive)
			continue;
		if (write(outfd, ev.pl_data, ev.pl_len) != (ssize_t)ev.pl_len)
			errx(EX_OSERR, "ERROR: failed output write");
	}
}

int
cmd_pmc_filter(int argc, char **argv)
{
	char *lwps, *pids, *events, *processes, *threads;
	uint32_t lwplist[LIST_MAX];
	uint32_t pidlist[LIST_MAX];
	int option, lwpcount, pidcount;
	int prelogfd, postlogfd;
	bool exclusive;

	threads = processes = lwps = pids = events = NULL;
	lwpcount = pidcount = 0;
	exclusive = false;
	while ((option = getopt_long(argc, argv, "e:p:t:xP:T:", longopts, NULL)) != -1) {
		switch (option) {
		case 'e':
			events = strdup(optarg);
			break;
		case 'p':
			pids = strdup(optarg);
			break;
		case 'P':
			processes = strdup(optarg);
			break;
		case 't':
			lwps = strdup(optarg);
			break;
		case 'T':
			threads = strdup(optarg);
			break;
		case 'x':
			exclusive = !exclusive;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2)
		usage();

	if (lwps)
		parse_intlist(lwps, lwplist, &lwpcount, atoi);
	if (pids)
		parse_intlist(pids, pidlist, &pidcount, atoi);
	if ((prelogfd = open(argv[0], O_RDONLY,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
		errx(EX_OSERR, "ERROR: Cannot open \"%s\" for reading: %s.", argv[0],
		    strerror(errno));
	if ((postlogfd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0)
		errx(EX_OSERR, "ERROR: Cannot open \"%s\" for writing: %s.", argv[1],
		    strerror(errno));

	pmc_filter_handler(lwplist, lwpcount, pidlist, pidcount, events,
	    processes, threads, exclusive, prelogfd, postlogfd);
	return (0);
}
