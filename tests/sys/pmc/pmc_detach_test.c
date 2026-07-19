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
 * Regression tests for detaching a process-mode PMC from a target that
 * still has it loaded on hardware.  The runcount reference for a live
 * PMC is dropped only by the context-switch-out reclaim, which the
 * scheduler gates on P_HWPMC; detaching used to clear P_HWPMC without
 * draining the reference, so the following release spun forever in
 * pmc_wait_for_pmc_idle() - a panic ("waiting too long for pmc to be
 * free") on an INVARIANTS kernel, an unkillable loop holding the hwpmc
 * lock otherwise.
 *
 * The failure is deterministic: attaching a counting PMC to the current
 * (running) thread and detaching it before release always left the
 * reference live.  On a kernel that carries the fix these tests complete
 * immediately; run against a kernel that lacks it they would wedge, so
 * (like any hwpmc regression test) they ship alongside the fix.
 *
 * They need an allocatable process-mode counting event, and skip where
 * none is available (hwpmc(4) not loaded, or a VM without a vPMU).
 */

#include <sys/types.h>

#include <errno.h>
#include <pmc.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

static const char *counting_events[] = {
	"instructions",
	"cycles",
	"branches",
	"unhalted-core-cycles",
	"inst_retired.any",
	"cpu_clk_unhalted.thread",
	"ls_not_halted_cyc",
	NULL
};

static volatile int spin_stop;

static void *
spinner(void *arg __unused)
{
	volatile unsigned long s = 0;

	while (spin_stop == 0)
		s += 1;
	return (NULL);
}

static void
burn_cpu(void)
{
	volatile unsigned long s = 0;
	int i;

	for (i = 0; i < 1000000; i++)
		s += i;
}

/* Allocate the first available process-mode counting PMC, or skip. */
static pmc_id_t
alloc_counting_pmc(void)
{
	pmc_id_t id;
	int i;

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not available: %s", strerror(errno));

	for (i = 0; counting_events[i] != NULL; i++) {
		id = PMC_ID_INVALID;
		if (pmc_allocate(counting_events[i], PMC_MODE_TC, 0,
		    PMC_CPU_ANY, &id, 0) == 0)
			return (id);
	}
	atf_tc_skip("no allocatable process-mode counting event "
	    "(no vPMU?)");
	return (PMC_ID_INVALID); /* not reached */
}

ATF_TC(detach_live_self);
ATF_TC_HEAD(detach_live_self, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Detaching a running counting PMC from the current thread and "
	    "then releasing it does not leak its runcount reference");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(detach_live_self, tc)
{
	pmc_id_t id;

	id = alloc_counting_pmc();

	ATF_REQUIRE(pmc_attach(id, 0) == 0);	/* 0 == current process */
	ATF_REQUIRE(pmc_start(id) == 0);
	burn_cpu();				/* make the counter live */

	/* Detach while still loaded on this CPU, then release. */
	ATF_REQUIRE(pmc_detach(id, 0) == 0);
	ATF_REQUIRE(pmc_release(id) == 0);	/* wedged before the fix */
}

ATF_TC(detach_live_multithread);
ATF_TC_HEAD(detach_live_multithread, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Detaching a running process-scope counting PMC drains the "
	    "references held by the process' other threads");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(detach_live_multithread, tc)
{
	pthread_t th[3];
	pmc_id_t id;
	int i, n;

	id = alloc_counting_pmc();

	/* Sibling threads keep the process-scope PMC live on other CPUs. */
	spin_stop = 0;
	for (n = 0; n < 3; n++)
		ATF_REQUIRE(pthread_create(&th[n], NULL, spinner, NULL) == 0);

	ATF_REQUIRE(pmc_attach(id, 0) == 0);
	ATF_REQUIRE(pmc_start(id) == 0);
	burn_cpu();
	/* Let the siblings be scheduled and pick up the PMC. */
	usleep(200 * 1000);

	ATF_REQUIRE(pmc_detach(id, 0) == 0);
	ATF_REQUIRE(pmc_release(id) == 0);	/* must drain the siblings */

	spin_stop = 1;
	for (i = 0; i < n; i++)
		pthread_join(th[i], NULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, detach_live_self);
	ATF_TP_ADD_TC(tp, detach_live_multithread);

	return (atf_no_error());
}
