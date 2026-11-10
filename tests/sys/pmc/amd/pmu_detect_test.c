/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Osvaldo Janeri Filho <ojanerif@amd.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Checks that hwpmc registers the AMD core PMU on Zen CPUs: the K8 class is
 * present with 48-bit counters and at least six core rows, alongside the
 * TSC class.
 */

#include <sys/param.h>
#include <sys/pmc.h>

#include <atf-c.h>

#include <errno.h>
#include <pmc.h>
#include <string.h>

#include "amd_pmc_test_common.h"

/* Every Zen generation has at least six core counters. */
#define	AMD_CORE_PMC_MIN	6

ATF_TC(pmu_detect_k8_class_present);
ATF_TC_HEAD(pmu_detect_k8_class_present, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify that PMC_CLASS_K8 is listed in pmc_cpuinfo() on Zen.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_detect_k8_class_present, tc)
{
	const struct pmc_cpuinfo *cpuinfo;
	struct amd_test_cpu cpu;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();

	ATF_REQUIRE_MSG(pmc_cpuinfo(&cpuinfo) == 0,
	    "pmc_cpuinfo() failed: %s", strerror(errno));
	ATF_CHECK_MSG(amd_test_find_classinfo(cpuinfo, PMC_CLASS_K8) != NULL,
	    "PMC_CLASS_K8 not registered on %s", amd_test_zen_name(cpu.zen));
}

ATF_TC(pmu_detect_counter_width);
ATF_TC_HEAD(pmu_detect_counter_width, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify that the AMD core counters are reported as 48 bits wide.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_detect_counter_width, tc)
{
	const struct pmc_cpuinfo *cpuinfo;
	const struct pmc_classinfo *ci;
	struct amd_test_cpu cpu;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();

	ATF_REQUIRE_MSG(pmc_cpuinfo(&cpuinfo) == 0,
	    "pmc_cpuinfo() failed: %s", strerror(errno));
	ci = amd_test_find_classinfo(cpuinfo, PMC_CLASS_K8);
	ATF_REQUIRE_MSG(ci != NULL, "PMC_CLASS_K8 not registered");

	ATF_CHECK_MSG(ci->pm_width == 48,
	    "AMD core counter width is %u bits, expected 48", ci->pm_width);
}

ATF_TC(pmu_detect_core_pmc_count);
ATF_TC_HEAD(pmu_detect_core_pmc_count, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify that cpu0 exposes at least "
	    __XSTRING(AMD_CORE_PMC_MIN) " AMD core counter rows.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_detect_core_pmc_count, tc)
{
	struct amd_test_cpu cpu;
	int n;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_perfctr_extcore();
	amd_test_skip_unless_hwpmc();

	n = amd_test_count_core_rows(0, false);
	ATF_CHECK_MSG(n >= AMD_CORE_PMC_MIN,
	    "expected at least %d core counter rows on cpu0 (%s), got %d",
	    AMD_CORE_PMC_MIN, amd_test_zen_name(cpu.zen), n);
}

ATF_TC(pmu_detect_tsc_class_coexists);
ATF_TC_HEAD(pmu_detect_tsc_class_coexists, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify that PMC_CLASS_TSC and PMC_CLASS_K8 are both registered.");
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(pmu_detect_tsc_class_coexists, tc)
{
	const struct pmc_cpuinfo *cpuinfo;
	struct amd_test_cpu cpu;

	amd_test_skip_unless_zen(&cpu);
	amd_test_skip_unless_hwpmc();

	ATF_REQUIRE_MSG(pmc_cpuinfo(&cpuinfo) == 0,
	    "pmc_cpuinfo() failed: %s", strerror(errno));
	ATF_CHECK_MSG(amd_test_find_classinfo(cpuinfo, PMC_CLASS_K8) != NULL,
	    "PMC_CLASS_K8 not registered");
	ATF_CHECK_MSG(amd_test_find_classinfo(cpuinfo, PMC_CLASS_TSC) != NULL,
	    "PMC_CLASS_TSC not registered");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, pmu_detect_k8_class_present);
	ATF_TP_ADD_TC(tp, pmu_detect_counter_width);
	ATF_TP_ADD_TC(tp, pmu_detect_core_pmc_count);
	ATF_TP_ADD_TC(tp, pmu_detect_tsc_class_coexists);

	return (atf_no_error());
}
