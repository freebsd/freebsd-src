/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Per-process tracking tests for process-mode (PMC_MODE_TC) counting PMCs.
 * A process-scope counter must follow a single PID:
 *   - a process attached to its own PMC counts its own work, readable live
 *     via self-read (pidtrack_self_counts);
 *   - an idle process sharing a CPU with a busy sibling does not pick up the
 *     sibling's work (pidtrack_no_leakage);
 *   - the count is preserved and keeps advancing across the target being
 *     switched out and back in (pidtrack_ctxswitch).
 *
 * pmc_read() of a process-mode PMC attached to another live process only
 * returns the value saved at the target's last switch-out, so each measured
 * process attaches its own PMC and reports its own delta.  The
 * controller-attaches-child pattern is covered by pmc_wrap_test.
 *
 * Each test skips when no process-mode counting event can be allocated
 * (hwpmc(4) not loaded, or a VM without a vPMU).
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/wait.h>

#include <errno.h>
#include <pmc.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static const char *const counting_events[] = {
	"instructions",
	"cycles",
	"branches",
	"unhalted-core-cycles",
	"inst_retired.any",
	"cpu_clk_unhalted.thread",
	"ls_not_halted_cyc",
	NULL
};

/*
 * Instruction-retired events only.  An idle process must not accumulate
 * the event used by the no-leakage test, and cycle events tick while a
 * process merely waits to run.
 */
static const char *const retired_events[] = {
	"instructions",
	"inst_retired.any",
	"ex_ret_instr",		/* AMD */
	NULL
};

/* Spin iterations: a large, unambiguous count, still well under a second. */
#define	SPIN_ITERS	((uint64_t)200 * 1000 * 1000)

/* Active work must move the counter by at least this much. */
#define	MIN_ACTIVE	((uint64_t)1 << 16)

/* Return the name of the first allocatable event in list, or NULL. */
static const char *
find_event_in(const char *const *list)
{
	pmc_id_t id;
	int i;

	for (i = 0; list[i] != NULL; i++) {
		id = PMC_ID_INVALID;
		if (pmc_allocate(list[i], PMC_MODE_TC, 0,
		    PMC_CPU_ANY, &id, 0) == 0) {
			(void)pmc_release(id);
			return (list[i]);
		}
	}
	return (NULL);
}

/* Skip unless hwpmc(4) works and an event from list can be allocated. */
static const char *
skip_unless_event(const char *const *list)
{
	const char *event;

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available: %s", strerror(errno));
	event = find_event_in(list);
	if (event == NULL)
		atf_tc_skip("no allocatable process-mode counting event");
	return (event);
}

static void __noinline
spin_workload(void)
{
	volatile uint64_t sink;
	uint64_t i;

	sink = 0;
	for (i = 0; i < SPIN_ITERS; i++)
		sink += i * 3 + 7;
}

/* Set up a process-mode PMC for event on the calling process. */
static int
self_attach(const char *event, pmc_id_t *id)
{
	*id = PMC_ID_INVALID;
	if (pmc_init() != 0 ||
	    pmc_allocate(event, PMC_MODE_TC, 0, PMC_CPU_ANY, id, 0) != 0 ||
	    pmc_attach(*id, 0) != 0 || pmc_start(*id) != 0)
		return (-1);
	return (0);
}

static void
self_detach(pmc_id_t id)
{
	(void)pmc_stop(id);
	(void)pmc_detach(id, 0);
	(void)pmc_release(id);
}

ATF_TC(pidtrack_self_counts);
ATF_TC_HEAD(pidtrack_self_counts, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A process-mode PMC attached to the calling process counts that "
	    "process' own work, observable via a live self-read");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(pidtrack_self_counts, tc)
{
	const char *event;
	pmc_id_t id;
	pmc_value_t v0, v1;

	event = skip_unless_event(counting_events);

	ATF_REQUIRE(pmc_allocate(event, PMC_MODE_TC, 0, PMC_CPU_ANY,
	    &id, 0) == 0);
	ATF_REQUIRE(pmc_attach(id, 0) == 0);
	ATF_REQUIRE(pmc_start(id) == 0);

	ATF_REQUIRE(pmc_read(id, &v0) == 0);
	spin_workload();
	ATF_REQUIRE(pmc_read(id, &v1) == 0);

	ATF_REQUIRE(pmc_stop(id) == 0);
	ATF_REQUIRE(pmc_detach(id, 0) == 0);
	ATF_REQUIRE(pmc_release(id) == 0);

	ATF_CHECK_MSG(v1 >= v0 && v1 - v0 >= MIN_ACTIVE,
	    "self-counter did not advance over the workload: %ju -> %ju "
	    "(expected a delta >= %ju)", (uintmax_t)v0, (uintmax_t)v1,
	    (uintmax_t)MIN_ACTIVE);
}

/* Exit status of a child whose PMC could not be set up. */
#define	CHILD_NO_PMC	70

/* Skip if a child could not set up its PMC; fail if it failed otherwise. */
static void
check_child(const char *name, int status)
{
	if (WIFEXITED(status) && WEXITSTATUS(status) == CHILD_NO_PMC)
		atf_tc_skip("the %s child could not set up its own PMC", name);
	ATF_REQUIRE_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "the %s child failed: status %#x", name, status);
}

/*
 * Child for pidtrack_no_leakage.  Both children are bound to cpu0 and
 * self-attach their own PMC.  The idle child takes its first reading, tells
 * the busy child to start and blocks until the busy child has finished its
 * spin, so the idle counter is live for the whole spin on the same CPU.
 * Each child writes its delta to resfd and exits 0.
 */
static void __dead2
child_selfmeasure(const char *event, bool busy, int waitfd, int notifyfd,
    int resfd)
{
	cpuset_t mask;
	pmc_id_t id;
	pmc_value_t v0, v1;
	uint64_t delta;
	char c;

	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, -1,
	    sizeof(mask), &mask) != 0 || self_attach(event, &id) != 0)
		_exit(CHILD_NO_PMC);

	if (busy)
		(void)read(waitfd, &c, 1);
	if (pmc_read(id, &v0) != 0)
		_exit(1);
	if (busy) {
		spin_workload();
		(void)write(notifyfd, "d", 1);
	} else {
		(void)write(notifyfd, "r", 1);
		(void)read(waitfd, &c, 1);
	}
	if (pmc_read(id, &v1) != 0)
		_exit(1);
	self_detach(id);

	delta = v1 >= v0 ? v1 - v0 : 0;
	(void)write(resfd, &delta, sizeof(delta));
	_exit(0);
}

ATF_TC(pidtrack_no_leakage);
ATF_TC_HEAD(pidtrack_no_leakage, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Two processes bound to one CPU, each with its own process-mode "
	    "PMC: the idle one's counter stays near zero while the busy one "
	    "runs, i.e. no cross-PID counter leakage");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(pidtrack_no_leakage, tc)
{
	const char *event;
	pid_t busy, idle;
	int busyfd[2], donefd[2], idlefd[2], readyfd[2];
	int bstatus, istatus;
	uint64_t busy_delta, idle_delta;

	event = skip_unless_event(retired_events);

	/* Notifying a child that already exited must not kill the test. */
	(void)signal(SIGPIPE, SIG_IGN);
	ATF_REQUIRE(pipe(busyfd) == 0);
	ATF_REQUIRE(pipe(donefd) == 0);
	ATF_REQUIRE(pipe(idlefd) == 0);
	ATF_REQUIRE(pipe(readyfd) == 0);

	busy = fork();
	ATF_REQUIRE(busy >= 0);
	if (busy == 0) {
		close(busyfd[0]);
		close(donefd[0]);
		close(idlefd[0]);
		close(idlefd[1]);
		close(readyfd[1]);
		child_selfmeasure(event, true, readyfd[0], donefd[1],
		    busyfd[1]);
	}
	idle = fork();
	if (idle < 0) {
		(void)kill(busy, SIGKILL);
		(void)waitpid(busy, NULL, 0);
		atf_tc_fail("fork(idle) failed: %s", strerror(errno));
	}
	if (idle == 0) {
		close(busyfd[0]);
		close(busyfd[1]);
		close(donefd[1]);
		close(idlefd[0]);
		close(readyfd[0]);
		child_selfmeasure(event, false, donefd[0], readyfd[1],
		    idlefd[1]);
	}
	close(busyfd[1]);
	close(donefd[0]);
	close(donefd[1]);
	close(idlefd[1]);
	close(readyfd[0]);
	close(readyfd[1]);

	ATF_REQUIRE(waitpid(busy, &bstatus, 0) == busy);
	ATF_REQUIRE(waitpid(idle, &istatus, 0) == idle);
	check_child("busy", bstatus);
	check_child("idle", istatus);

	ATF_REQUIRE(read(busyfd[0], &busy_delta, sizeof(busy_delta)) ==
	    (ssize_t)sizeof(busy_delta));
	ATF_REQUIRE(read(idlefd[0], &idle_delta, sizeof(idle_delta)) ==
	    (ssize_t)sizeof(idle_delta));
	close(busyfd[0]);
	close(idlefd[0]);

	ATF_CHECK_MSG(busy_delta >= MIN_ACTIVE,
	    "busy child's own counter did not advance: %ju",
	    (uintmax_t)busy_delta);
	ATF_CHECK_MSG(idle_delta < MIN_ACTIVE,
	    "idle child's counter advanced by %ju while its sibling ran on "
	    "the same CPU (expected < %ju)",
	    (uintmax_t)idle_delta, (uintmax_t)MIN_ACTIVE);
}

ATF_TC(pidtrack_ctxswitch);
ATF_TC_HEAD(pidtrack_ctxswitch, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A process-mode PMC's accumulated count is preserved and keeps "
	    "advancing across the target being switched out and in");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(pidtrack_ctxswitch, tc)
{
	const char *event;
	pmc_id_t id;
	pmc_value_t b, e, s2;
	pid_t child;
	uint64_t out[3];
	int pfd[2], rdyfd[2], resfd[2], status;
	char c;

	event = skip_unless_event(counting_events);

	(void)signal(SIGPIPE, SIG_IGN);
	ATF_REQUIRE(pipe(pfd) == 0);
	ATF_REQUIRE(pipe(rdyfd) == 0);
	ATF_REQUIRE(pipe(resfd) == 0);

	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		/*
		 * Report three absolute reads so the parent can check both
		 * that the value survived the switch (r2_start >= r1_end)
		 * and that counting resumed afterwards (r2_end > r2_start).
		 */
		close(pfd[1]);
		close(rdyfd[0]);
		close(resfd[0]);
		if (self_attach(event, &id) != 0)
			_exit(CHILD_NO_PMC);

		spin_workload();
		if (pmc_read(id, &b) != 0)
			_exit(1);

		/* Report round 1 done, then block: switched out and in. */
		(void)write(rdyfd[1], "r", 1);
		(void)read(pfd[0], &c, 1);

		if (pmc_read(id, &s2) != 0)
			_exit(1);
		spin_workload();
		if (pmc_read(id, &e) != 0)
			_exit(1);
		self_detach(id);

		out[0] = b;
		out[1] = s2;
		out[2] = e;
		(void)write(resfd[1], out, sizeof(out));
		_exit(0);
	}
	close(pfd[0]);
	close(rdyfd[1]);
	close(resfd[1]);

	/*
	 * Once round 1 is done, give the child time to block in read(2)
	 * before releasing it.  EOF means the child exited early.
	 */
	if (read(rdyfd[0], &c, 1) == 1)
		usleep(200 * 1000);
	(void)write(pfd[1], "g", 1);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	check_child("measuring", status);
	ATF_REQUIRE(read(resfd[0], out, sizeof(out)) == (ssize_t)sizeof(out));
	close(pfd[1]);
	close(rdyfd[0]);
	close(resfd[0]);

	ATF_CHECK_MSG(out[0] >= MIN_ACTIVE,
	    "round-1 count too low: %ju", (uintmax_t)out[0]);
	ATF_CHECK_MSG(out[1] >= out[0],
	    "counter went backwards across the context switch: r1_end=%ju "
	    "r2_start=%ju", (uintmax_t)out[0], (uintmax_t)out[1]);
	/* Checked separately so that a backwards step cannot wrap. */
	ATF_CHECK_MSG(out[2] >= out[1] && out[2] - out[1] >= MIN_ACTIVE,
	    "counter did not advance after switch-in: r2_start=%ju "
	    "r2_end=%ju", (uintmax_t)out[1], (uintmax_t)out[2]);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pidtrack_self_counts);
	ATF_TP_ADD_TC(tp, pidtrack_no_leakage);
	ATF_TP_ADD_TC(tp, pidtrack_ctxswitch);

	return (atf_no_error());
}
