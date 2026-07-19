/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Alexander Leidinger <netchild@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other advertising materials provided with the
 *    distribution.
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
 */

/*
 * Regression tests for process-mode counting PMCs whose accumulated
 * count is close to, or beyond, the range of the underlying hardware
 * counter.  Hardware counters are narrower than the 64-bit software
 * counter (48 bits on modern x86), and the increment collected when a
 * counter is read out (at context switch and at process exit) must be
 * computed modulo the hardware width.
 *
 * Before the fix these tests panicked INVARIANTS kernels with
 * "[pmc,...] negative increment" once the hardware counter wrapped,
 * and silently corrupted the accumulated count on other kernels.
 *
 * The counter is attached to a short-lived child process which spins
 * to advance it; the child exits before the PMC is released, so the
 * accounting is torn down cleanly (the accumulation is collected on
 * the exit path, which is one of the two sites the fix touches).
 *
 * The tests need a hardware counting event whose counter is narrower
 * than 64 bits; they skip on systems without one (e.g. VMs without a
 * vPMU, or with hwpmc(4) not loaded).
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <pmc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * High-rate events; at least one of these should be allocatable on
 * any x86 or arm64 system with a vPMU.  All of them count while the
 * child's spin loop runs.
 */
static const char *wrap_test_events[] = {
	"instructions",
	"cycles",
	"branches",
	"unhalted-core-cycles",
	"inst_retired.any",
	"cpu_clk_unhalted.thread",
	"ls_not_halted_cyc",
	NULL
};

/* The child generates well more than this many events past the seed. */
#define	WRAP_MARGIN	((uint64_t)1 << 20)

/* Spin iterations: comfortably more than WRAP_MARGIN events, still < 1s. */
#define	SPIN_ITERS	((uint64_t)200 * 1000 * 1000)

/* Upper bound on events a single test run can plausibly generate. */
#define	SANITY_BOUND	((uint64_t)1 << 40)

/*
 * Child: wait for the parent to start the PMC (one byte on the pipe),
 * spin to advance the counter across the end of its range, then exit.
 */
static void __attribute__((noinline))
child_spin(int startfd)
{
	volatile uint64_t sink = 0;
	uint64_t i;
	char c;

	(void)read(startfd, &c, 1);
	for (i = 0; i < SPIN_ITERS; i++)
		sink += i;
	_exit((int)(sink & 0x7f));
}

/*
 * Allocate a process-mode counting PMC for the first available event
 * backed by a counter narrower than 64 bits, attach it to a child,
 * seed it with the given value, let the child spin past the end of
 * the counter range, and check that the accumulated count stayed sane.
 */
static void
wrap_test(bool seed_beyond_width)
{
	pmc_id_t pmcid;
	pmc_value_t final;
	uint64_t seed;
	uint32_t width;
	pid_t child;
	int i, pfd[2], status;

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available: %s", strerror(errno));

	width = 0;
	pmcid = PMC_ID_INVALID;
	for (i = 0; wrap_test_events[i] != NULL; i++) {
		if (pmc_allocate(wrap_test_events[i], PMC_MODE_TC, 0,
		    PMC_CPU_ANY, &pmcid, 0) != 0)
			continue;
		ATF_REQUIRE(pmc_width(pmcid, &width) == 0);
		if (width >= 32 && width < 64)
			break;
		ATF_REQUIRE(pmc_release(pmcid) == 0);
		pmcid = PMC_ID_INVALID;
	}
	if (pmcid == PMC_ID_INVALID)
		atf_tc_skip("no allocatable counting event with a hardware "
		    "counter narrower than 64 bits");

	if (seed_beyond_width) {
		/* Accumulated count that no longer fits the counter at all. */
		seed = ((uint64_t)1 << width) + 12345;
	} else {
		/* Just below the end of the counter range; the child crosses it. */
		seed = ((uint64_t)1 << width) - WRAP_MARGIN;
	}

	ATF_REQUIRE(pipe(pfd) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		close(pfd[1]);
		child_spin(pfd[0]);
		/* NOTREACHED */
	}
	close(pfd[0]);

	ATF_REQUIRE(pmc_attach(pmcid, child) == 0);
	ATF_REQUIRE(pmc_write(pmcid, seed) == 0);
	ATF_REQUIRE(pmc_start(pmcid) == 0);

	/* Release the child, which spins and exits. */
	ATF_REQUIRE(write(pfd[1], "g", 1) == 1);
	close(pfd[1]);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);

	/*
	 * The child is gone: its final increment was collected on the
	 * exit path.  Read the accumulated 64-bit count and stop.
	 */
	ATF_REQUIRE(pmc_read(pmcid, &final) == 0);
	(void)pmc_stop(pmcid);

	ATF_CHECK_MSG(final >= seed,
	    "accumulated count went backwards: seed 0x%jx, final 0x%jx "
	    "(width %u)", (uintmax_t)seed, (uintmax_t)final, width);
	ATF_CHECK_MSG(final - seed < SANITY_BOUND,
	    "accumulated count jumped implausibly: seed 0x%jx, final 0x%jx "
	    "(width %u)", (uintmax_t)seed, (uintmax_t)final, width);

	ATF_REQUIRE(pmc_release(pmcid) == 0);
}

ATF_TC(counting_pmc_wraps_hardware_counter);
ATF_TC_HEAD(counting_pmc_wraps_hardware_counter, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A process-mode counting PMC survives its hardware counter "
	    "wrapping around while the counted process runs");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(counting_pmc_wraps_hardware_counter, tc)
{
	wrap_test(false);
}

ATF_TC(counting_pmc_beyond_hardware_width);
ATF_TC_HEAD(counting_pmc_beyond_hardware_width, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A process-mode counting PMC keeps counting correctly once "
	    "its accumulated count exceeds the hardware counter range");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(counting_pmc_beyond_hardware_width, tc)
{
	wrap_test(true);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, counting_pmc_wraps_hardware_counter);
	ATF_TP_ADD_TC(tp, counting_pmc_beyond_hardware_width);

	return (atf_no_error());
}
