/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/pmc.h>
#include <sys/wait.h>

#include <machine/cpufunc.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define	EVENT		"instructions"
#define	ITERATIONS	1000000

static void
setup(void)
{
	cpuset_t mask;
	int cpu;

	if (pmc_init() != 0)
		atf_tc_skip("hwpmc(4) is not loaded: %s", strerror(errno));

	/* RDPMC reads this CPU's counter; stay on one CPU. */
	ATF_REQUIRE(cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0);
	cpu = CPU_FFS(&mask) - 1;
	ATF_REQUIRE(cpu >= 0);
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	ATF_REQUIRE(cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
	    sizeof(mask), &mask) == 0);
}

static pmc_id_t
alloc_tc(uint32_t flags)
{
	pmc_id_t id;

	id = PMC_ID_INVALID;
	if (pmc_allocate(EVENT, PMC_MODE_TC, flags, PMC_CPU_ANY, &id, 0) != 0)
		atf_tc_skip("pmc_allocate(%s) failed: %s", EVENT,
		    strerror(errno));
	return (id);
}

static int
getmsr_errno(pmc_id_t id)
{
	uint32_t msr;

	errno = 0;
	return (pmc_get_msr(id, &msr) == 0 ? 0 : errno);
}

static void
work(void)
{
	volatile uint64_t x;
	int i;

	x = 1;
	for (i = 0; i < ITERATIONS; i++)
		x = x * 1664525U + 1013904223U;
}

ATF_TC(rdpmc_reads_own_counter);
ATF_TC_HEAD(rdpmc_reads_own_counter, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "RDPMC of the index pmc_get_msr() returns for a counting PMC "
	    "attached to its owner advances by at least one instruction per "
	    "loop iteration");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(rdpmc_reads_own_counter, tc)
{
	uint64_t after, before;
	uint32_t msr;
	pmc_id_t id;

	setup();
	id = alloc_tc(0);
	ATF_REQUIRE(pmc_attach(id, 0) == 0);
	ATF_REQUIRE(pmc_start(id) == 0);
	ATF_REQUIRE_MSG(pmc_get_msr(id, &msr) == 0, "pmc_get_msr() failed: %s",
	    strerror(errno));
	before = rdpmc(msr);
	work();
	after = rdpmc(msr);
	ATF_CHECK(pmc_stop(id) == 0);
	ATF_CHECK(pmc_release(id) == 0);
	ATF_CHECK_MSG(after - before >= ITERATIONS,
	    "RDPMC(%#x) advanced by %ju over %d iterations", msr,
	    (uintmax_t)(after - before), ITERATIONS);
}

ATF_TC(getmsr_refusals);
ATF_TC_HEAD(getmsr_refusals, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_get_msr() fails with EINVAL for a system-mode PMC, an "
	    "unattached PMC, a PMC attached to another process and an "
	    "inherited PMC, and with ENOSYS for the software class");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(getmsr_refusals, tc)
{
	pmc_id_t id;
	pid_t child;
	int error, status;

	setup();

	id = PMC_ID_INVALID;
	if (pmc_allocate(EVENT, PMC_MODE_SC, 0, 0, &id, 0) != 0)
		atf_tc_skip("pmc_allocate(%s) failed: %s", EVENT,
		    strerror(errno));
	error = getmsr_errno(id);
	ATF_CHECK_MSG(error == EINVAL, "system mode: errno %d", error);
	ATF_CHECK(pmc_release(id) == 0);

	id = alloc_tc(0);
	error = getmsr_errno(id);
	ATF_CHECK_MSG(error == EINVAL, "unattached: errno %d", error);

	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		pause();
		_exit(0);
	}
	ATF_REQUIRE(pmc_attach(id, child) == 0);
	error = getmsr_errno(id);
	ATF_CHECK_MSG(error == EINVAL, "attached to a child: errno %d", error);
	(void)kill(child, SIGKILL);
	(void)waitpid(child, &status, 0);
	ATF_CHECK(pmc_release(id) == 0);

	id = alloc_tc(PMC_F_DESCENDANTS);
	ATF_REQUIRE(pmc_attach(id, 0) == 0);
	error = getmsr_errno(id);
	ATF_CHECK_MSG(error == EINVAL, "inherited: errno %d", error);
	ATF_CHECK(pmc_release(id) == 0);

	id = PMC_ID_INVALID;
	ATF_REQUIRE(pmc_allocate("SOFT-CLOCK.HARD", PMC_MODE_TC, 0,
	    PMC_CPU_ANY, &id, 0) == 0);
	ATF_REQUIRE(pmc_attach(id, 0) == 0);
	error = getmsr_errno(id);
	ATF_CHECK_MSG(error == ENOSYS, "software class: errno %d", error);
	ATF_CHECK(pmc_release(id) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rdpmc_reads_own_counter);
	ATF_TP_ADD_TC(tp, getmsr_refusals);

	return (atf_no_error());
}
