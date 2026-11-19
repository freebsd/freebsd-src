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

#define	EVENT		"instructions"
#define	PERIOD		100000000
#define	NPMC		2
#define	WAIT_SECONDS	5

struct result {
	int		error;
	char		what[64];
	uint64_t	own[NPMC];	/* user-mode samples of the workload */
	uint64_t	dropped;
	uint64_t	total;		/* samples of either PMC */
	uint64_t	processed;	/* kern.hwpmc.stats deltas */
	uint64_t	ignored;
	uint64_t	bad;		/* foreign PMC id or wrong CPU */
	bool		closed;
};

static void
fail_step(struct result *r, const char *what)
{
	if (r->error == 0) {
		r->error = errno != 0 ? errno : EPROTO;
		strlcpy(r->what, what, sizeof(r->what));
	}
}

/* Returns true at CLOSELOG. */
static bool
consume(void *parser, const pmc_id_t *ids, int cpu, pid_t pid,
    struct result *r)
{
	struct pmclog_ev ev;
	const struct pmclog_ev_callchain *cc;
	int k;

	for (;;) {
		memset(&ev, 0, sizeof(ev));
		if (pmclog_read(parser, &ev) != 0) {
			if (ev.pl_state != PMCLOG_EOF) {
				errno = EPROTO;
				fail_step(r, "pmclog_read");
				return (true);
			}
			return (ev.pl_type == PMCLOG_TYPE_CLOSELOG);
		}
		if (ev.pl_type == PMCLOG_TYPE_DROPNOTIFY)
			r->dropped++;
		if (ev.pl_type != PMCLOG_TYPE_CALLCHAIN)
			continue;
		cc = &ev.pl_u.pl_cc;
		for (k = 0; k < NPMC; k++)
			if (cc->pl_pmcid == ids[k])
				break;
		if (k == NPMC ||
		    PMC_CALLCHAIN_CPUFLAGS_TO_CPU(cc->pl_cpuflags) !=
		    (unsigned int)cpu) {
			r->bad++;
			continue;
		}
		r->total++;
		/* Other processes on the CPU are legitimate samples too. */
		if (cc->pl_pid == (uint32_t)pid &&
		    PMC_CALLCHAIN_CPUFLAGS_TO_USERMODE(cc->pl_cpuflags) != 0)
			r->own[k]++;
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

static bool
all_sampled(const struct result *r)
{
	int k;

	for (k = 0; k < NPMC; k++)
		if (r->own[k] == 0)
			return (false);
	return (true);
}

/* A closed log leaves shutdown state on its owner: use a fresh process. */
static void
run_owner(int cpu, struct result *r)
{
	char path[] = "pmc-ss.XXXXXX";
	struct kevent ke;
	struct timespec deadline, left, now;
	volatile uint64_t x;
	cpuset_t set;
	pmc_id_t ids[NPMC];
	void *parser;
	int fd, i, k, kq, reader;

	memset(r, 0, sizeof(*r));
	for (k = 0; k < NPMC; k++)
		ids[k] = PMC_ID_INVALID;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	if (pmc_init() != 0 || cpuset_setaffinity(CPU_LEVEL_WHICH,
	    CPU_WHICH_TID, -1, sizeof(set), &set) != 0) {
		fail_step(r, "pmc_init or pinning");
		return;
	}
	if ((fd = mkstemp(path)) < 0 ||
	    (reader = open(path, O_RDONLY)) < 0 || unlink(path) != 0 ||
	    (parser = pmclog_open(reader)) == NULL ||
	    (kq = kqueue()) < 0) {
		fail_step(r, "log setup");
		return;
	}
	EV_SET(&ke, reader, EVFILT_VNODE, EV_ADD | EV_CLEAR,
	    NOTE_WRITE | NOTE_EXTEND, 0, NULL);
	if (kevent(kq, &ke, 1, NULL, 0, NULL) != 0 ||
	    pmc_configure_logfile(fd) != 0) {
		fail_step(r, "pmc_configure_logfile");
		return;
	}
	r->processed = stat("kern.hwpmc.stats.intr_processed");
	r->ignored = stat("kern.hwpmc.stats.intr_ignored");
	for (k = 0; k < NPMC && r->error == 0; k++)
		if (pmc_allocate(EVENT, PMC_MODE_SS, 0, cpu, &ids[k],
		    PERIOD) != 0 || pmc_start(ids[k]) != 0)
			fail_step(r, "allocating or starting a PMC");
	if (r->error == 0 && ids[0] == ids[1]) {
		errno = EPROTO;
		fail_step(r, "two allocations returned one id");
	}

	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_sec += WAIT_SECONDS;
	x = 1;
	while (r->error == 0 && !all_sampled(r)) {
		for (i = 0; i < 1000000; i++)
			x = x * 1664525U + 1013904223U;
		if (pmc_flush_logfile() != 0)
			fail_step(r, "pmc_flush_logfile");
		else if (consume(parser, ids, cpu, getpid(), r)) {
			errno = EPROTO;
			fail_step(r, "CLOSELOG while the PMCs ran");
		}
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&now, &deadline, >=))
			break;
	}

	for (k = 0; k < NPMC; k++) {
		if (ids[k] == PMC_ID_INVALID)
			continue;
		(void)pmc_stop(ids[k]);
		if (pmc_release(ids[k]) != 0)
			fail_step(r, "pmc_release");
	}
	if (pmc_flush_logfile() != 0 || pmc_close_logfile() != 0) {
		fail_step(r, "closing the log");
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_sec += WAIT_SECONDS;
	while (!(r->closed = consume(parser, ids, cpu, getpid(), r))) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&now, &deadline, >=))
			break;
		timespecsub(&deadline, &now, &left);
		(void)kevent(kq, NULL, 0, &ke, 1, &left);
	}
	r->processed = stat("kern.hwpmc.stats.intr_processed") - r->processed;
	r->ignored = stat("kern.hwpmc.stats.intr_ignored") - r->ignored;
	(void)pmc_configure_logfile(-1);
}

static int
sample(struct result *r)
{
	cpuset_t mask;
	pmc_id_t id;
	pid_t child;
	ssize_t n;
	int cpu, fds[2], status;

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not loaded: %s", strerror(errno));

	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0);
	cpu = CPU_FFS(&mask) - 1;
	ATF_REQUIRE(cpu >= 0);

	/* A guest without a PMU has the event name but no counter for it. */
	id = PMC_ID_INVALID;
	if (pmc_allocate(EVENT, PMC_MODE_SS, 0, cpu, &id, PERIOD) != 0)
		atf_tc_skip("pmc_allocate(%s) failed: %s", EVENT,
		    strerror(errno));
	ATF_REQUIRE(pmc_release(id) == 0);

	ATF_REQUIRE(pipe(fds) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		close(fds[0]);
		run_owner(cpu, r);
		n = write(fds[1], r, sizeof(*r));
		_exit(n == (ssize_t)sizeof(*r) ? 0 : 1);
	}
	close(fds[1]);
	n = read(fds[0], r, sizeof(*r));
	close(fds[0]);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_REQUIRE_MSG(n == (ssize_t)sizeof(*r) && WIFEXITED(status) &&
	    WEXITSTATUS(status) == 0, "sampling owner failed: status %#x",
	    status);
	printf("cpu%d: samples %ju %ju of %ju, dropped %ju, bad %ju, "
	    "interrupts %ju processed %ju ignored\n", cpu,
	    (uintmax_t)r->own[0], (uintmax_t)r->own[1], (uintmax_t)r->total,
	    (uintmax_t)r->dropped, (uintmax_t)r->bad,
	    (uintmax_t)r->processed, (uintmax_t)r->ignored);
	ATF_REQUIRE_MSG(r->error == 0, "%s failed: %s", r->what,
	    strerror(r->error));
	return (cpu);
}

ATF_TC(two_pmcs_deliver_samples);
ATF_TC_HEAD(two_pmcs_deliver_samples, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Two system-sampling instructions PMCs on one CPU each log "
	    "user-mode samples of a workload pinned there; the log reaches "
	    "CLOSELOG with no dropped buffers");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "60");
}
ATF_TC_BODY(two_pmcs_deliver_samples, tc)
{
	struct result r;

	(void)sample(&r);
	ATF_CHECK_MSG(r.closed, "the log did not reach CLOSELOG");
	ATF_CHECK_MSG(r.dropped == 0 && r.bad == 0,
	    "%ju dropped buffers, %ju misrouted samples", (uintmax_t)r.dropped,
	    (uintmax_t)r.bad);
	ATF_CHECK_MSG(all_sampled(&r), "a PMC logged no workload sample "
	    "within %d s: %ju and %ju", WAIT_SECONDS, (uintmax_t)r.own[0],
	    (uintmax_t)r.own[1]);
}

ATF_TC(interrupts_counted_once);
ATF_TC_HEAD(interrupts_counted_once, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Each sampling interrupt is counted once in "
	    "kern.hwpmc.stats.intr_processed and none in intr_ignored");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "timeout", "60");
}
ATF_TC_BODY(interrupts_counted_once, tc)
{
	struct result r;

	(void)sample(&r);
	ATF_CHECK_MSG(r.ignored == 0, "%ju interrupts counted as ignored",
	    (uintmax_t)r.ignored);
	/* A sample needs an interrupt; a full buffer can drop one. */
	ATF_CHECK_MSG(r.processed >= r.total && r.processed <= r.total +
	    r.total / 10 + 16, "%ju interrupts processed for %ju samples",
	    (uintmax_t)r.processed, (uintmax_t)r.total);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, two_pmcs_deliver_samples);
	ATF_TP_ADD_TC(tp, interrupts_counted_once);

	return (atf_no_error());
}
