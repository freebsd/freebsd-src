/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/event.h>
#include <sys/pmc.h>
#include <sys/pmclog.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <dev/hwpmc/hwpmc_ibs.h>

#include <atf-c.h>

#include <errno.h>
#include <fcntl.h>
#include <pmc.h>
#include <pmclog.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "amd_pmc_test_common.h"

#define	FETCH		0x01
#define	OP		0x02
#define	WORK_SECONDS	2
#define	LOG_WAIT	5
#define	MIN_PERCENT	25

struct episode {
	int		error;
	char		what[64];
	uint64_t	samples[2];	/* FETCH, OP */
	uint64_t	dropped;
	uint64_t	processed;	/* kern.hwpmc.stats deltas */
	uint64_t	ignored;
	uint64_t	bad;		/* foreign PMC, wrong CPU or type */
};

static void
fail_step(struct episode *ep, const char *what)
{
	if (ep->error == 0) {
		ep->error = errno != 0 ? errno : EPROTO;
		strlcpy(ep->what, what, sizeof(ep->what));
	}
}

static uint64_t
stat(const char *name)
{
	uint64_t v;
	size_t len;

	len = sizeof(v);
	if (sysctlbyname(name, &v, &len, NULL, 0) != 0)
		return (0);
	return (v);
}

/* Keeps both the fetch and the op pipe busy. */
static void
workload(void)
{
	static volatile uint64_t table[4096];
	struct timespec end, now;
	uint64_t v;
	int i;

	clock_gettime(CLOCK_MONOTONIC, &end);
	end.tv_sec += WORK_SECONDS;
	v = 1;
	do {
		for (i = 0; i < 100000; i++) {
			v = v * 6364136223846793005ULL + 1442695040888963407ULL;
			if ((v >> 61) & 1)
				table[v & (nitems(table) - 1)] += v;
			else
				v ^= table[(v >> 17) & (nitems(table) - 1)];
		}
		clock_gettime(CLOCK_MONOTONIC, &now);
	} while (timespeccmp(&now, &end, <));
}

static void
drain(void *parser, int kq, const pmc_id_t *ids, int cpu, struct episode *ep)
{
	static const uint8_t type[2] = {
		PMC_CC_MULTIPART_IBS_FETCH, PMC_CC_MULTIPART_IBS_OP
	};
	struct pmclog_ev ev;
	struct kevent ke;
	struct timespec deadline, left, now;
	const struct pmclog_ev_callchain *cc;
	int k;

	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_sec += LOG_WAIT;
	for (;;) {
		memset(&ev, 0, sizeof(ev));
		if (pmclog_read(parser, &ev) == 0) {
			if (ev.pl_type == PMCLOG_TYPE_DROPNOTIFY)
				ep->dropped++;
			if (ev.pl_type != PMCLOG_TYPE_CALLCHAIN)
				continue;
			cc = &ev.pl_u.pl_cc;
			for (k = 0; k < 2; k++)
				if (ids[k] != PMC_ID_INVALID &&
				    cc->pl_pmcid == ids[k])
					break;
			if (k == 2 || cc->pl_npc == 0 ||
			    PMC_CALLCHAIN_CPUFLAGS_TO_CPU(cc->pl_cpuflags) !=
			    (unsigned int)cpu ||
			    ((const uint8_t *)cc->pl_pc)[0] != type[k])
				ep->bad++;
			else
				ep->samples[k]++;
			continue;
		}
		if (ev.pl_state != PMCLOG_EOF) {
			errno = EPROTO;
			fail_step(ep, "pmclog_read");
			return;
		}
		if (ev.pl_type == PMCLOG_TYPE_CLOSELOG)
			return;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&now, &deadline, >=)) {
			errno = ETIMEDOUT;
			fail_step(ep, "waiting for CLOSELOG");
			return;
		}
		timespecsub(&deadline, &now, &left);
		(void)kevent(kq, NULL, 0, &ke, 1, &left);
	}
}

/* A closed log leaves shutdown state on its owner: use a fresh process. */
static void
run_episode(int cpu, int engines, struct episode *ep)
{
	static const char *const event[2] = { "IBS-FETCH", "IBS-OP" };
	static const pmc_value_t rate[2] = {
		IBS_FETCH_MIN_RATE, IBS_OP_MIN_RATE
	};
	char path[] = "ibs-fo.XXXXXX";
	struct kevent ke;
	cpuset_t set;
	pmc_id_t ids[2] = { PMC_ID_INVALID, PMC_ID_INVALID };
	void *parser;
	int fd, k, kq, reader;

	memset(ep, 0, sizeof(*ep));
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (pmc_init() != 0 || cpuset_setaffinity(CPU_LEVEL_WHICH,
	    CPU_WHICH_TID, -1, sizeof(set), &set) != 0) {
		fail_step(ep, "pmc_init or pinning");
		return;
	}
	if ((fd = mkstemp(path)) < 0 ||
	    (reader = open(path, O_RDONLY)) < 0 || unlink(path) != 0 ||
	    (parser = pmclog_open(reader)) == NULL ||
	    (kq = kqueue()) < 0) {
		fail_step(ep, "log setup");
		return;
	}
	EV_SET(&ke, reader, EVFILT_VNODE, EV_ADD | EV_CLEAR,
	    NOTE_WRITE | NOTE_EXTEND, 0, NULL);
	if (kevent(kq, &ke, 1, NULL, 0, NULL) != 0 ||
	    pmc_configure_logfile(fd) != 0) {
		fail_step(ep, "pmc_configure_logfile");
		return;
	}

	ep->processed = stat("kern.hwpmc.stats.intr_processed");
	ep->ignored = stat("kern.hwpmc.stats.intr_ignored");
	for (k = 0; k < 2; k++)
		if ((engines & (1 << k)) != 0 && (pmc_allocate(event[k],
		    PMC_MODE_SS, 0, cpu, &ids[k], rate[k]) != 0 ||
		    pmc_start(ids[k]) != 0))
			fail_step(ep, event[k]);
	if (ep->error == 0)
		workload();
	for (k = 0; k < 2; k++) {
		if (ids[k] == PMC_ID_INVALID)
			continue;
		(void)pmc_stop(ids[k]);
		if (pmc_release(ids[k]) != 0)
			fail_step(ep, "pmc_release");
	}
	if (pmc_flush_logfile() != 0 || pmc_close_logfile() != 0)
		fail_step(ep, "closing the log");
	else
		drain(parser, kq, ids, cpu, ep);
	ep->processed = stat("kern.hwpmc.stats.intr_processed") - ep->processed;
	ep->ignored = stat("kern.hwpmc.stats.intr_ignored") - ep->ignored;
	(void)pmc_configure_logfile(-1);
}

static void
episode(int cpu, int engines, struct episode *ep)
{
	pid_t child;
	ssize_t n;
	int fds[2], status;

	ATF_REQUIRE(pipe(fds) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		close(fds[0]);
		run_episode(cpu, engines, ep);
		n = write(fds[1], ep, sizeof(*ep));
		_exit(n == (ssize_t)sizeof(*ep) ? 0 : 1);
	}
	close(fds[1]);
	n = read(fds[0], ep, sizeof(*ep));
	close(fds[0]);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE_MSG(n == (ssize_t)sizeof(*ep) && WIFEXITED(status) &&
	    WEXITSTATUS(status) == 0, "episode owner failed: status %#x",
	    status);
	printf("engines %#x: fetch %ju op %ju dropped %ju bad %ju, "
	    "interrupts %ju processed %ju ignored\n", engines,
	    (uintmax_t)ep->samples[0], (uintmax_t)ep->samples[1],
	    (uintmax_t)ep->dropped, (uintmax_t)ep->bad,
	    (uintmax_t)ep->processed, (uintmax_t)ep->ignored);
	ATF_REQUIRE_MSG(ep->error == 0, "%s failed: %s", ep->what,
	    strerror(ep->error));
	ATF_REQUIRE_MSG(ep->dropped == 0 && ep->bad == 0,
	    "%ju dropped buffers, %ju malformed or misrouted samples",
	    (uintmax_t)ep->dropped, (uintmax_t)ep->bad);
}

/* Skip unless IBS Fetch and Op sampling work; return the CPU to use. */
static int
setup(void)
{
	cpuset_t mask;
	u_int regs[4];
	int cpu;

	amd_test_skip_unless_amd();
	do_cpuid(0x80000000, regs);
	if (regs[0] < CPUID_IBSID)
		atf_tc_skip("CPU lacks the IBS CPUID leaf");
	do_cpuid(CPUID_IBSID, regs);
	if ((regs[0] & (CPUID_IBSID_FETCHSAM | CPUID_IBSID_OPSAM)) !=
	    (CPUID_IBSID_FETCHSAM | CPUID_IBSID_OPSAM))
		atf_tc_skip("CPU lacks IBS Fetch or Op sampling");
	amd_test_skip_unless_hwpmc();

	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0);
	cpu = CPU_FFS(&mask) - 1;
	ATF_REQUIRE(cpu >= 0);
	return (cpu);
}

/*
 * Regression test for 572cd669d6: pmc_ibs_intr() skipped a valid Op sample
 * when Fetch was valid in the same NMI, and IBS-OP starved.
 */
ATF_TC(fetch_and_op_together);
ATF_TC_HEAD(fetch_and_op_together, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "IBS-FETCH and IBS-OP system sampling on one CPU each deliver at "
	    "least a quarter of the samples they deliver alone");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "60");
}
ATF_TC_BODY(fetch_and_op_together, tc)
{
	struct episode both, fetch, op;
	int cpu;

	cpu = setup();

	episode(cpu, FETCH, &fetch);
	episode(cpu, OP, &op);
	ATF_REQUIRE_MSG(fetch.samples[0] > 0 && op.samples[1] > 0,
	    "an engine alone delivered no samples: fetch %ju, op %ju",
	    (uintmax_t)fetch.samples[0], (uintmax_t)op.samples[1]);
	episode(cpu, FETCH | OP, &both);

	ATF_CHECK_MSG(both.samples[0] * 100 >= fetch.samples[0] * MIN_PERCENT,
	    "IBS-FETCH starved beside IBS-OP: %ju samples, %ju alone",
	    (uintmax_t)both.samples[0], (uintmax_t)fetch.samples[0]);
	ATF_CHECK_MSG(both.samples[1] * 100 >= op.samples[1] * MIN_PERCENT,
	    "IBS-OP starved beside IBS-FETCH: %ju samples, %ju alone",
	    (uintmax_t)both.samples[1], (uintmax_t)op.samples[1]);
}

/* c97cea955f: an IBS interrupt was counted as processed twice. */
ATF_TC(interrupts_counted_once);
ATF_TC_HEAD(interrupts_counted_once, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Each IBS-FETCH sampling interrupt is counted once in "
	    "kern.hwpmc.stats.intr_processed and none in intr_ignored");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "30");
}
ATF_TC_BODY(interrupts_counted_once, tc)
{
	struct episode ep;

	episode(setup(), FETCH, &ep);
	ATF_CHECK_MSG(ep.ignored == 0, "%ju interrupts counted as ignored",
	    (uintmax_t)ep.ignored);
	ATF_CHECK_MSG(ep.processed >= ep.samples[0] &&
	    ep.processed <= ep.samples[0] + ep.samples[0] / 10 + 16,
	    "%ju interrupts processed for %ju samples",
	    (uintmax_t)ep.processed, (uintmax_t)ep.samples[0]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fetch_and_op_together);
	ATF_TP_ADD_TC(tp, interrupts_counted_once);

	return (atf_no_error());
}
