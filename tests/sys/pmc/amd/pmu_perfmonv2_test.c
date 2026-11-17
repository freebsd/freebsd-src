/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/sysctl.h>

#include <dev/hwpmc/hwpmc_amd.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "amd_pmc_test_common.h"

#define	LOOP_ITERS	5000000UL

static bool
perfmon_v2_sysctl(bool *v2)
{
	size_t len;

	len = sizeof(*v2);
	return (sysctlbyname("kern.hwpmc.amd_perfmon_v2", v2, &len, NULL,
	    0) == 0);
}

static void
skip_unless_v2(void)
{
	struct amd_test_cpu cpu;
	bool v2;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();
	ATF_REQUIRE_MSG(perfmon_v2_sysctl(&v2),
	    "kern.hwpmc.amd_perfmon_v2 is missing: %s", strerror(errno));
	if (!v2)
		atf_tc_skip("hwpmc does not use PerfMonV2 on this CPU");
	if (!pmc_pmu_enabled())
		atf_tc_skip("pmu-events not enabled");
}

static void __noinline
loop(uint64_t iters)
{
	volatile uint64_t sink;
	uint64_t i;

	sink = 0;
	for (i = 0; i < iters; i++)
		sink += i * 3 + 7;
}

ATF_TC(pmv2_sysctl_matches_cpuid);
ATF_TC_HEAD(pmv2_sysctl_matches_cpuid, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "kern.hwpmc.amd_perfmon_v2 is set exactly when CPUID "
	    "Fn8000_0022 EAX[0] is set on family 19h or later");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(pmv2_sysctl_matches_cpuid, tc)
{
	struct amd_test_cpu cpu;
	u_int regs[4];
	bool expected, v2;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();
	ATF_REQUIRE_MSG(perfmon_v2_sysctl(&v2),
	    "kern.hwpmc.amd_perfmon_v2 is missing: %s", strerror(errno));

	do_cpuid(0x80000000, regs);
	expected = false;
	if (regs[0] >= CPUID_EXTPERFMON) {
		do_cpuid(CPUID_EXTPERFMON, regs);
		expected = EXTPERFMON_PERFMONV2(regs[0]) != 0 &&
		    cpu.family >= 0x19;
	}
	ATF_CHECK_MSG(v2 == expected,
	    "kern.hwpmc.amd_perfmon_v2 is %d, CPUID says %d", v2, expected);
}

ATF_TC(pmv2_process_counting);
ATF_TC_HEAD(pmv2_process_counting, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "With PerfMonV2, two process-mode PMCs count a known loop: at "
	    "least one retired instruction per iteration, and a plausible IPC");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(pmv2_process_counting, tc)
{
	pmc_id_t cyc, instr;
	pmc_value_t vcyc, vinstr;

	skip_unless_v2();
	ATF_REQUIRE_MSG(pmc_allocate("ex_ret_instr", PMC_MODE_TC, 0,
	    PMC_CPU_ANY, &instr, 0) == 0, "ex_ret_instr allocation failed: %s",
	    strerror(errno));
	ATF_REQUIRE_MSG(pmc_allocate("ls_not_halted_cyc", PMC_MODE_TC, 0,
	    PMC_CPU_ANY, &cyc, 0) == 0,
	    "ls_not_halted_cyc allocation failed: %s", strerror(errno));
	ATF_REQUIRE(pmc_attach(instr, 0) == 0);
	ATF_REQUIRE(pmc_attach(cyc, 0) == 0);
	ATF_REQUIRE(pmc_start(instr) == 0);
	ATF_REQUIRE(pmc_start(cyc) == 0);
	loop(LOOP_ITERS);
	/* A stopped PMC reads its last saved value. */
	ATF_REQUIRE(pmc_read(instr, &vinstr) == 0);
	ATF_REQUIRE(pmc_read(cyc, &vcyc) == 0);
	ATF_REQUIRE(pmc_stop(instr) == 0);
	ATF_REQUIRE(pmc_stop(cyc) == 0);
	amd_test_release_pmc(instr);
	amd_test_release_pmc(cyc);

	ATF_CHECK_MSG(vinstr >= LOOP_ITERS,
	    "ex_ret_instr counted %ju over a %lu-iteration loop",
	    (uintmax_t)vinstr, LOOP_ITERS);
	ATF_CHECK_MSG(vcyc > 0 && vinstr / vcyc < 20,
	    "implausible counts: %ju instructions in %ju cycles",
	    (uintmax_t)vinstr, (uintmax_t)vcyc);
}

ATF_TC(pmv2_all_core_counters);
ATF_TC_HEAD(pmv2_all_core_counters, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "With PerfMonV2, every core counter held by one process counts "
	    "the same loop, so the global enable reaches all of them");
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(pmv2_all_core_counters, tc)
{
	pmc_id_t ids[AMD_PMC_CORE_MAX];
	pmc_value_t v;
	int i, n;

	skip_unless_v2();
	amd_test_skip_unless_perfctr_extcore();
	n = amd_test_count_core_rows(0, false);
	ATF_REQUIRE_MSG(n > 0 && n <= AMD_PMC_CORE_MAX,
	    "unexpected core row count %d", n);
	if (amd_test_count_core_rows(0, true) < n)
		atf_tc_skip("not all core counters are free");

	for (i = 0; i < n; i++) {
		ATF_REQUIRE_MSG(pmc_allocate("ex_ret_instr", PMC_MODE_TC, 0,
		    PMC_CPU_ANY, &ids[i], 0) == 0,
		    "allocation %d of %d failed: %s", i, n, strerror(errno));
		ATF_REQUIRE(pmc_attach(ids[i], 0) == 0);
	}
	for (i = 0; i < n; i++)
		ATF_REQUIRE(pmc_start(ids[i]) == 0);
	loop(LOOP_ITERS);
	/* A stopped PMC reads its last saved value. */
	for (i = 0; i < n; i++) {
		ATF_REQUIRE(pmc_read(ids[i], &v) == 0);
		ATF_CHECK_MSG(v >= LOOP_ITERS,
		    "counter %d of %d counted %ju over a %lu-iteration loop",
		    i, n, (uintmax_t)v, LOOP_ITERS);
	}
	for (i = 0; i < n; i++) {
		ATF_REQUIRE(pmc_stop(ids[i]) == 0);
		amd_test_release_pmc(ids[i]);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pmv2_sysctl_matches_cpuid);
	ATF_TP_ADD_TC(tp, pmv2_process_counting);
	ATF_TP_ADD_TC(tp, pmv2_all_core_counters);

	return (atf_no_error());
}
