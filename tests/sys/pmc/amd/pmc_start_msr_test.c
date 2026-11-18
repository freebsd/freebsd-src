/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Davi C.A <davi.chavesazevedo@amd.com>
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/cpuctl.h>
#include <sys/cpuset.h>
#include <sys/ioctl.h>
#include <sys/pmc.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <dev/hwpmc/hwpmc_amd.h>

#include <atf-c.h>

#include <errno.h>
#include <fcntl.h>
#include <pmc.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "amd_pmc_test_common.h"

#define	CORE_EVENT		"unhalted-cycles"
/* PerfEvtSel of core counter n when PerfCtrExtCore is present. */
#define	PERF_CTL_MSR(n)		(AMD_PMC_CORE_BASE + 2 * (n))

static void
skip_unless_ready(void)
{
	struct amd_test_cpu cpu;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_perfctr_extcore();
	amd_test_skip_unless_hwpmc();
	if (!pmc_pmu_enabled())
		atf_tc_skip("pmu-events not enabled");
}

static uint64_t
rdmsr_on(int cpu, uint32_t msr)
{
	cpuctl_msr_args_t args;
	char path[32];
	int fd;

	(void)snprintf(path, sizeof(path), "/dev/cpuctl%d", cpu);
	fd = open(path, O_RDONLY);
	ATF_REQUIRE_MSG(fd >= 0, "open(%s) failed: %s", path, strerror(errno));
	args.msr = msr;
	args.data = 0;
	ATF_REQUIRE_MSG(ioctl(fd, CPUCTL_RDMSR, &args) == 0,
	    "CPUCTL_RDMSR(%#x) on cpu%d failed: %s", msr, cpu,
	    strerror(errno));
	close(fd);
	return (args.data);
}

static unsigned int
core_counter(pmc_id_t id)
{
	struct pmc_pmcinfo *pmcinfo;
	unsigned int row;
	int n;

	row = PMC_ID_TO_ROWINDEX(id);
	ATF_REQUIRE(pmc_pmcinfo(0, &pmcinfo) == 0);
	n = amd_test_core_index(pmcinfo->pm_pmcs[row].pm_name);
	ATF_REQUIRE_MSG(n >= 0, "row %u is %s, not a core counter", row,
	    pmcinfo->pm_pmcs[row].pm_name);
	free(pmcinfo);
	return ((unsigned int)n);
}

static void
wait_until_sleeping(pid_t pid)
{
	struct kinfo_proc kp;
	size_t len;
	int i, mib[4];

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;
	for (i = 0; i < 1000; i++) {
		len = sizeof(kp);
		ATF_REQUIRE_MSG(sysctl(mib, nitems(mib), &kp, &len, NULL,
		    0) == 0, "kern.proc.pid.%d: %s", pid, strerror(errno));
		if (kp.ki_stat == SSLEEP)
			return;
		usleep(1000);
	}
	atf_tc_fail("process %d did not go to sleep", pid);
}

static bool
evsel_enabled(int cpu, unsigned int counter)
{
	return ((rdmsr_on(cpu, PERF_CTL_MSR(counter)) & AMD_PMC_ENABLE) != 0);
}

ATF_TC(system_start_enables_now);
ATF_TC_HEAD(system_start_enables_now, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_start() of a system-mode core PMC on cpu0 sets PerfEvtSel.En "
	    "on cpu0 right away, and pmc_stop() clears it");
	atf_tc_set_md_var(tc, "require.kmods", "cpuctl");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(system_start_enables_now, tc)
{
	unsigned int ctr;
	pmc_id_t id;

	skip_unless_ready();
	ATF_REQUIRE_MSG(pmc_allocate(CORE_EVENT, PMC_MODE_SC, 0, 0, &id,
	    0) == 0, "pmc_allocate(%s, SC) failed: %s", CORE_EVENT,
	    strerror(errno));
	ctr = core_counter(id);
	ATF_CHECK_MSG(!evsel_enabled(0, ctr),
	    "counter %u was enabled before pmc_start()", ctr);

	ATF_REQUIRE(pmc_start(id) == 0);
	ATF_CHECK_MSG(evsel_enabled(0, ctr),
	    "pmc_start() did not set PerfEvtSel.En of counter %u", ctr);
	ATF_REQUIRE(pmc_stop(id) == 0);
	ATF_CHECK_MSG(!evsel_enabled(0, ctr),
	    "pmc_stop() did not clear PerfEvtSel.En of counter %u", ctr);
	amd_test_release_pmc(id);
}

ATF_TC(system_start_sets_global_ctl);
ATF_TC_HEAD(system_start_sets_global_ctl, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "With PerfMonV2, pmc_start() of a system-mode core PMC on cpu0 "
	    "sets its PerfCntrGlobalCtl bit on cpu0, and pmc_stop() clears it");
	atf_tc_set_md_var(tc, "require.kmods", "cpuctl");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(system_start_sets_global_ctl, tc)
{
	unsigned int ctr;
	pmc_id_t id;
	size_t len;
	bool v2;

	skip_unless_ready();
	len = sizeof(v2);
	if (sysctlbyname("kern.hwpmc.amd_perfmon_v2", &v2, &len, NULL,
	    0) != 0 || !v2)
		atf_tc_skip("hwpmc does not use PerfMonV2 on this CPU");
	ATF_REQUIRE_MSG(pmc_allocate(CORE_EVENT, PMC_MODE_SC, 0, 0, &id,
	    0) == 0, "pmc_allocate(%s, SC) failed: %s", CORE_EVENT,
	    strerror(errno));
	ctr = core_counter(id);
	ATF_CHECK_MSG((rdmsr_on(0, AMD_PMC_GLOBAL_CTL) & (1ULL << ctr)) == 0,
	    "counter %u was enabled before pmc_start()", ctr);
	ATF_REQUIRE(pmc_start(id) == 0);
	ATF_CHECK_MSG((rdmsr_on(0, AMD_PMC_GLOBAL_CTL) & (1ULL << ctr)) != 0,
	    "pmc_start() did not set the GlobalCtl bit of counter %u", ctr);
	ATF_REQUIRE(pmc_stop(id) == 0);
	ATF_CHECK_MSG((rdmsr_on(0, AMD_PMC_GLOBAL_CTL) & (1ULL << ctr)) == 0,
	    "pmc_stop() did not clear the GlobalCtl bit of counter %u", ctr);
	amd_test_release_pmc(id);
}

ATF_TC(process_start_waits_for_target);
ATF_TC_HEAD(process_start_waits_for_target, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "pmc_start() of a process-mode core PMC attached to a child that "
	    "is bound to cpu1 and blocked in read(2) leaves PerfEvtSel.En "
	    "clear there; the PMC counts once the child runs");
	atf_tc_set_md_var(tc, "require.kmods", "cpuctl");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(process_start_waits_for_target, tc)
{
	cpuset_t mask;
	volatile uint64_t sink;
	uint64_t i;
	pid_t child;
	pmc_id_t id;
	pmc_value_t v;
	unsigned int ctr;
	int go[2], status;
	char c;

	skip_unless_ready();
	if (pmc_ncpu() < 2)
		atf_tc_skip("needs at least two CPUs");

	ATF_REQUIRE(pipe(go) == 0);
	child = fork();
	ATF_REQUIRE(child >= 0);
	if (child == 0) {
		close(go[1]);
		if (read(go[0], &c, 1) != 1)
			_exit(1);
		sink = 0;
		for (i = 0; i < 50000000; i++)
			sink += i;
		_exit(0);
	}
	close(go[0]);

	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	if (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, child,
	    sizeof(mask), &mask) != 0) {
		(void)kill(child, SIGKILL);
		(void)waitpid(child, NULL, 0);
		atf_tc_skip("cannot bind the child to cpu1: %s",
		    strerror(errno));
	}
	wait_until_sleeping(child);

	ATF_REQUIRE_MSG(pmc_allocate(CORE_EVENT, PMC_MODE_TC, 0, PMC_CPU_ANY,
	    &id, 0) == 0, "pmc_allocate(%s, TC) failed: %s", CORE_EVENT,
	    strerror(errno));
	ctr = core_counter(id);
	ATF_REQUIRE(pmc_attach(id, child) == 0);
	ATF_REQUIRE(pmc_start(id) == 0);

	/* The child is blocked in read(2), so nothing has loaded the PMC. */
	ATF_CHECK_MSG(!evsel_enabled(1, ctr),
	    "counter %u was enabled on cpu1 before the target ran", ctr);

	ATF_REQUIRE(write(go[1], "g", 1) == 1);
	close(go[1]);
	ATF_REQUIRE(waitpid(child, &status, 0) == child);
	ATF_CHECK_MSG(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	    "child failed: status %#x", status);

	/* hwpmc saved the child's count when it exited. */
	ATF_REQUIRE(pmc_read(id, &v) == 0);
	ATF_CHECK_MSG(v > 0, "the PMC did not count while the target ran");
	amd_test_release_pmc(id);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, system_start_enables_now);
	ATF_TP_ADD_TC(tp, system_start_sets_global_ctl);
	ATF_TP_ADD_TC(tp, process_start_waits_for_target);

	return (atf_no_error());
}
